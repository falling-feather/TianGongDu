#include <tgd/gameplay/craft_session.hpp>

#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;

void hash_integer(std::uint64_t& hash, bool value) noexcept {
    hash ^= static_cast<std::uint8_t>(value);
    hash *= fnv_prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash ^= static_cast<std::uint8_t>(bits & 0xffU);
        hash *= fnv_prime;
        bits >>= 8U;
    }
}

[[nodiscard]] bool valid_outcome(
    contracts::CraftMaterialOutcome outcome
) noexcept {
    return outcome == contracts::CraftMaterialOutcome::passes_trial ||
           outcome == contracts::CraftMaterialOutcome::requires_rework;
}

}  // namespace

CraftSessionBuildError CraftSession::initialize(
    const contracts::CraftDefinition& definition,
    contracts::StableContentKey process
) noexcept {
    if (initialized_ || process == 0) {
        return CraftSessionBuildError::invalid_definition;
    }
    if (definition.steps.size() > contracts::sandbox_craft_step_capacity ||
        definition.materials.size() > contracts::sandbox_craft_material_capacity ||
        definition.material_choices.size() >
            contracts::sandbox_craft_material_choice_capacity ||
        definition.processes.size() > contracts::sandbox_craft_process_capacity) {
        return CraftSessionBuildError::capacity_exceeded;
    }
    const auto found = std::find_if(
        definition.processes.begin(),
        definition.processes.end(),
        [process](const auto& value) { return value.id.key == process; }
    );
    if (found == definition.processes.end()) {
        return CraftSessionBuildError::process_not_found;
    }
    definition_ = &definition;
    process_ = &*found;
    return build_runtime();
}

CraftSessionBuildError CraftSession::build_runtime() noexcept {
    if (definition_ == nullptr || process_ == nullptr) {
        return CraftSessionBuildError::invalid_definition;
    }
    operations_.fill(0);
    operation_count_ = 0;
    trial_step_ = 0;
    rework_step_ = 0;
    selected_outcome_ = contracts::CraftMaterialOutcome::invalid;
    completed_operation_count_ = 0;
    trial_count_ = 0;
    mistake_count_ = 0;
    rework_count_ = 0;
    stage_ = CraftSessionStage::invalid;
    initialized_ = false;
    snapshot_ = {};

    std::size_t process_step_count = 0;
    std::size_t trial_count = 0;
    std::size_t rework_count = 0;
    for (const auto& step : definition_->steps) {
        if (step.process_id != process_->id) continue;
        ++process_step_count;
        if (step.kind == contracts::CraftStepKind::trial) {
            ++trial_count;
            trial_step_ = step.id.key;
        } else if (step.kind == contracts::CraftStepKind::rework) {
            ++rework_count;
            rework_step_ = step.id.key;
        } else if (step.kind != contracts::CraftStepKind::operation) {
            return CraftSessionBuildError::invalid_definition;
        }
    }
    if (process_step_count < 4 || trial_count != 1 || rework_count != 1 ||
        trial_step_ != process_->trial_step_id.key) {
        return CraftSessionBuildError::invalid_definition;
    }

    contracts::StableContentKey predecessor = 0;
    while (operation_count_ < operations_.size()) {
        const auto next = std::find_if(
            definition_->steps.begin(),
            definition_->steps.end(),
            [&](const auto& step) {
                return step.process_id == process_->id &&
                       step.kind == contracts::CraftStepKind::operation &&
                       step.predecessor_step_id.key == predecessor;
            }
        );
        if (next == definition_->steps.end()) break;
        const auto duplicate_successor = std::find_if(
            next + 1,
            definition_->steps.end(),
            [&](const auto& step) {
                return step.process_id == process_->id &&
                       step.kind == contracts::CraftStepKind::operation &&
                       step.predecessor_step_id.key == predecessor;
            }
        );
        if (duplicate_successor != definition_->steps.end()) {
            return CraftSessionBuildError::invalid_definition;
        }
        operations_[operation_count_++] = next->id.key;
        predecessor = next->id.key;
    }
    if (operation_count_ < 2 || operation_count_ + 2 != process_step_count) {
        return CraftSessionBuildError::invalid_definition;
    }

    const auto trial = std::find_if(
        definition_->steps.begin(),
        definition_->steps.end(),
        [&](const auto& step) { return step.id.key == trial_step_; }
    );
    const auto rework = std::find_if(
        definition_->steps.begin(),
        definition_->steps.end(),
        [&](const auto& step) { return step.id.key == rework_step_; }
    );
    if (trial == definition_->steps.end() ||
        rework == definition_->steps.end() ||
        trial->predecessor_step_id.key != operations_[operation_count_ - 1U] ||
        rework->predecessor_step_id.key != trial_step_) {
        return CraftSessionBuildError::invalid_definition;
    }

    std::size_t material_choices = 0;
    bool has_pass = false;
    bool has_rework = false;
    for (const auto& choice : definition_->material_choices) {
        if (choice.process_id != process_->id) continue;
        const bool known_material = std::any_of(
            definition_->materials.begin(),
            definition_->materials.end(),
            [&](const auto& material) { return material.id == choice.material_id; }
        );
        if (!known_material || !valid_outcome(choice.outcome)) {
            return CraftSessionBuildError::invalid_definition;
        }
        ++material_choices;
        has_pass |= choice.outcome == contracts::CraftMaterialOutcome::passes_trial;
        has_rework |=
            choice.outcome == contracts::CraftMaterialOutcome::requires_rework;
    }
    if (material_choices < 2 || !has_pass || !has_rework) {
        return CraftSessionBuildError::invalid_definition;
    }

    initialized_ = true;
    stage_ = CraftSessionStage::awaiting_material;
    refresh_snapshot();
    return CraftSessionBuildError::none;
}

CraftActionResult CraftSession::select_material(
    contracts::StableContentKey material
) noexcept {
    if (!initialized_) return reject(CraftActionDisposition::invalid_state);
    if (stage_ != CraftSessionStage::awaiting_material) {
        return reject(CraftActionDisposition::wrong_stage);
    }
    const auto choice = std::find_if(
        definition_->material_choices.begin(),
        definition_->material_choices.end(),
        [&](const auto& value) {
            return value.process_id == process_->id &&
                   value.material_id.key == material;
        }
    );
    if (choice == definition_->material_choices.end()) {
        return reject(CraftActionDisposition::unknown_target);
    }
    selected_outcome_ = choice->outcome;
    stage_ = CraftSessionStage::performing_operations;
    snapshot_.selected_material = material;
    refresh_snapshot();
    return {CraftActionDisposition::applied, snapshot_};
}

CraftActionResult CraftSession::perform_operation(
    contracts::StableContentKey step
) noexcept {
    if (!initialized_) return reject(CraftActionDisposition::invalid_state);
    if (stage_ != CraftSessionStage::performing_operations) {
        return reject(CraftActionDisposition::wrong_stage);
    }
    if (completed_operation_count_ >= operation_count_ ||
        operations_[completed_operation_count_] != step) {
        return reject(CraftActionDisposition::wrong_order);
    }
    ++completed_operation_count_;
    if (completed_operation_count_ == operation_count_) {
        stage_ = CraftSessionStage::trial_ready;
    }
    refresh_snapshot();
    return {CraftActionDisposition::applied, snapshot_};
}

CraftActionResult CraftSession::run_trial() noexcept {
    if (!initialized_) return reject(CraftActionDisposition::invalid_state);
    if (stage_ != CraftSessionStage::trial_ready) {
        return reject(CraftActionDisposition::wrong_stage);
    }
    ++trial_count_;
    if (selected_outcome_ == contracts::CraftMaterialOutcome::requires_rework &&
        rework_count_ == 0) {
        ++mistake_count_;
        stage_ = CraftSessionStage::rework_required;
    } else {
        stage_ = CraftSessionStage::completed;
    }
    refresh_snapshot();
    return {CraftActionDisposition::applied, snapshot_};
}

CraftActionResult CraftSession::perform_rework() noexcept {
    if (!initialized_) return reject(CraftActionDisposition::invalid_state);
    if (stage_ != CraftSessionStage::rework_required) {
        return reject(CraftActionDisposition::wrong_stage);
    }
    ++rework_count_;
    stage_ = CraftSessionStage::trial_ready;
    refresh_snapshot();
    return {CraftActionDisposition::applied, snapshot_};
}

CraftSessionBuildError CraftSession::restart() noexcept {
    return initialized_ ? build_runtime() : CraftSessionBuildError::invalid_definition;
}

CraftSessionSnapshot CraftSession::snapshot() const noexcept {
    return snapshot_;
}

CraftActionResult CraftSession::reject(
    CraftActionDisposition disposition
) const noexcept {
    return {disposition, snapshot_};
}

void CraftSession::refresh_snapshot() noexcept {
    snapshot_.initialized = initialized_;
    snapshot_.process = process_ == nullptr ? 0 : process_->id.key;
    snapshot_.need = process_ == nullptr ? 0 : process_->need_id.key;
    snapshot_.expected_step =
        stage_ == CraftSessionStage::performing_operations &&
                completed_operation_count_ < operation_count_
            ? operations_[completed_operation_count_]
            : stage_ == CraftSessionStage::trial_ready
                  ? trial_step_
                  : stage_ == CraftSessionStage::rework_required
                        ? rework_step_
                        : 0;
    snapshot_.output_item =
        process_ == nullptr ? 0 : process_->output_item_id.key;
    snapshot_.stage = stage_;
    snapshot_.completed_operation_count =
        static_cast<std::uint16_t>(completed_operation_count_);
    snapshot_.operation_count = static_cast<std::uint16_t>(operation_count_);
    snapshot_.trial_count = trial_count_;
    snapshot_.mistake_count = mistake_count_;
    snapshot_.rework_count = rework_count_;
    snapshot_.completed = stage_ == CraftSessionStage::completed;

    std::uint64_t hash = fnv_offset;
    hash_integer(hash, snapshot_.initialized);
    hash_integer(hash, snapshot_.process);
    hash_integer(hash, snapshot_.need);
    hash_integer(hash, snapshot_.selected_material);
    hash_integer(hash, snapshot_.expected_step);
    hash_integer(hash, snapshot_.output_item);
    hash_integer(hash, static_cast<std::uint8_t>(snapshot_.stage));
    hash_integer(hash, snapshot_.completed_operation_count);
    hash_integer(hash, snapshot_.operation_count);
    hash_integer(hash, snapshot_.trial_count);
    hash_integer(hash, snapshot_.mistake_count);
    hash_integer(hash, snapshot_.rework_count);
    hash_integer(hash, snapshot_.completed);
    snapshot_.checksum = hash;
}

}  // namespace tgd::gameplay
