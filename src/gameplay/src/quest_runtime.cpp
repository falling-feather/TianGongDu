#include <tgd/gameplay/quest_runtime.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnv_prime;
}

[[nodiscard]] std::uint64_t squared_component(
    std::int32_t left,
    std::int32_t right
) noexcept {
    const auto delta = static_cast<std::int64_t>(left) - static_cast<std::int64_t>(right);
    const auto magnitude = static_cast<std::uint64_t>(delta < 0 ? -delta : delta);
    return magnitude * magnitude;
}

[[nodiscard]] std::uint64_t saturating_add(
    std::uint64_t left,
    std::uint64_t right
) noexcept {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return right > maximum - left ? maximum : left + right;
}

[[nodiscard]] std::uint64_t ground_distance_squared(
    const contracts::GroundPoseMm& left,
    const contracts::GroundPoseMm& right
) noexcept {
    return saturating_add(
        squared_component(left.x, right.x),
        squared_component(left.y, right.y)
    );
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & 0xffU));
        bits >>= 8U;
    }
}

}  // namespace

QuestInteractionError DeterministicQuestInteractionResolver::initialize(
    std::span<const contracts::QuestInteractionDefinition> definitions
) noexcept {
    if (initialized_) {
        return QuestInteractionError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > interaction_capacity) {
        return QuestInteractionError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& definition = definitions[index];
        if (definition.id.key == 0 || definition.cell_id.key == 0 ||
            definition.objective_id.key == 0 || definition.radius_mm <= 0) {
            return QuestInteractionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[prior].id.key == definition.id.key ||
                definitions[prior].objective_id.key == definition.objective_id.key) {
                return QuestInteractionError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestInteractionError::none;
}

QuestInteractionResult DeterministicQuestInteractionResolver::resolve(
    const QuestInteractionQuery& query,
    const IQuestRuntime& quest
) const noexcept {
    QuestInteractionResult result{};
    if (!initialized_) {
        result.error = QuestInteractionError::invalid_lifecycle;
        return result;
    }
    if (query.actor == 0 || query.cell == 0) {
        result.error = QuestInteractionError::invalid_query;
        return result;
    }

    auto best_distance = std::numeric_limits<std::uint64_t>::max();
    for (const auto& definition : definitions_) {
        if (definition.cell_id.key != query.cell ||
            definition.pose.floor_layer != query.pose.floor_layer ||
            quest.objective_state(definition.objective_id.key) != QuestObjectiveState::active) {
            continue;
        }
        const auto distance = ground_distance_squared(query.pose, definition.pose);
        const auto radius = static_cast<std::uint64_t>(definition.radius_mm);
        if (distance > radius * radius) {
            continue;
        }
        if (!result.found || distance < best_distance ||
            (distance == best_distance && definition.id.key < result.interaction)) {
            result.found = true;
            result.interaction = definition.id.key;
            result.objective = definition.objective_id.key;
            result.kind = definition.kind;
            best_distance = distance;
        }
    }
    return result;
}

QuestCombatTriggerError DeterministicQuestCombatTriggerResolver::initialize(
    std::span<const contracts::QuestCombatTriggerDefinition> definitions
) noexcept {
    if (initialized_) {
        return QuestCombatTriggerError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > trigger_capacity) {
        return QuestCombatTriggerError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& definition = definitions[index];
        if (definition.id.key == 0 || definition.objective_id.key == 0 ||
            definition.required_stance == 0) {
            return QuestCombatTriggerError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[prior].id.key == definition.id.key ||
                definitions[prior].objective_id.key == definition.objective_id.key) {
                return QuestCombatTriggerError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestCombatTriggerError::none;
}

QuestCombatTriggerResult DeterministicQuestCombatTriggerResolver::resolve(
    const QuestCombatSignal& signal,
    const IQuestRuntime& quest
) const noexcept {
    QuestCombatTriggerResult result{};
    if (!initialized_) {
        result.error = QuestCombatTriggerError::invalid_lifecycle;
        return result;
    }
    if (signal.actor == 0 || signal.stance == 0) {
        result.error = QuestCombatTriggerError::invalid_signal;
        return result;
    }
    for (const auto& definition : definitions_) {
        if (definition.kind != signal.kind || definition.required_stance != signal.stance ||
            quest.objective_state(definition.objective_id.key) != QuestObjectiveState::active) {
            continue;
        }
        if (!result.found || definition.id.key < result.trigger) {
            result.found = true;
            result.trigger = definition.id.key;
            result.objective = definition.objective_id.key;
        }
    }
    return result;
}

QuestError DeterministicQuestRuntime::initialize(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableActorKey player_actor
) noexcept {
    if (lifecycle_ != QuestLifecycle::uninitialized) {
        return QuestError::invalid_lifecycle;
    }
    const auto validation = validate_definition(definition);
    if (validation != QuestError::none) {
        return validation;
    }
    if (player_actor == 0 || player_actor != definition.player.actor) {
        return QuestError::invalid_definition;
    }
    definition_ = &definition;
    player_actor_ = player_actor;
    lifecycle_ = QuestLifecycle::ready;
    refresh_snapshot();
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::start() noexcept {
    if (lifecycle_ != QuestLifecycle::ready) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::running;
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::pause() noexcept {
    if (lifecycle_ != QuestLifecycle::running) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::paused;
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::resume() noexcept {
    if (lifecycle_ != QuestLifecycle::paused) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::running;
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::destroy() noexcept {
    if (lifecycle_ == QuestLifecycle::uninitialized ||
        lifecycle_ == QuestLifecycle::destroyed) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::destroyed;
    definition_ = nullptr;
    completed_objective_count_ = 0;
    return QuestError::none;
}

QuestApplyResult DeterministicQuestRuntime::apply(
    const contracts::QuestCommand& command,
    IQuestEventSink& sink
) noexcept {
    QuestApplyResult result{};
    if (lifecycle_ != QuestLifecycle::running && lifecycle_ != QuestLifecycle::resolved) {
        result.error = QuestError::invalid_lifecycle;
        return result;
    }
    if (command.actor != player_actor_ || command.sequence == 0 || command.objective == 0 ||
        command.type != contracts::QuestCommandType::complete_objective) {
        result.error = QuestError::invalid_command;
        return result;
    }
    if (command.completed_tick < snapshot_.tick) {
        result.error = QuestError::tick_regressed;
        return result;
    }
    if (command.sequence <= last_command_sequence_) {
        result.error = QuestError::stale_command_sequence;
        return result;
    }
    const auto state = objective_state(command.objective);
    if (state == QuestObjectiveState::unknown) {
        result.error = QuestError::unknown_objective;
        return result;
    }
    if (state == QuestObjectiveState::locked) {
        result.error = QuestError::objective_not_active;
        return result;
    }

    const auto objective_stage_index = objective_stage(command.objective);
    std::size_t event_count = 0;
    last_command_sequence_ = command.sequence;
    snapshot_.tick = command.completed_tick;
    if (state == QuestObjectiveState::completed) {
        events_[event_count++] = {
            command.completed_tick,
            contracts::QuestEventType::objective_already_completed,
            command.actor,
            definition_->id.key,
            definition_->beats[objective_stage_index].id.key,
            command.objective,
        };
        refresh_snapshot();
        sink.publish(std::span{events_}.first(event_count));
        result.quest_resolved = lifecycle_ == QuestLifecycle::resolved;
        return result;
    }

    completed_objectives_[completed_objective_count_++] = command.objective;
    result.accepted = true;
    events_[event_count++] = {
        command.completed_tick,
        contracts::QuestEventType::objective_completed,
        command.actor,
        definition_->id.key,
        definition_->beats[stage_index_].id.key,
        command.objective,
    };
    if (active_stage_complete()) {
        result.stage_advanced = true;
        if (stage_index_ + 1 == definition_->beats.size()) {
            lifecycle_ = QuestLifecycle::resolved;
            result.quest_resolved = true;
            events_[event_count++] = {
                command.completed_tick,
                contracts::QuestEventType::quest_resolved,
                command.actor,
                definition_->id.key,
                definition_->beats[stage_index_].id.key,
                command.objective,
            };
        } else {
            ++stage_index_;
            events_[event_count++] = {
                command.completed_tick,
                contracts::QuestEventType::stage_advanced,
                command.actor,
                definition_->id.key,
                definition_->beats[stage_index_].id.key,
                command.objective,
            };
        }
    }
    refresh_snapshot();
    sink.publish(std::span{events_}.first(event_count));
    return result;
}

QuestObjectiveState DeterministicQuestRuntime::objective_state(
    contracts::StableContentKey objective
) const noexcept {
    const auto stage = objective_stage(objective);
    if (stage == stage_capacity) {
        return QuestObjectiveState::unknown;
    }
    if (is_completed(objective)) {
        return QuestObjectiveState::completed;
    }
    return stage == stage_index_ ? QuestObjectiveState::active : QuestObjectiveState::locked;
}

const contracts::QuestSnapshot& DeterministicQuestRuntime::snapshot() const noexcept {
    return snapshot_;
}

QuestLifecycle DeterministicQuestRuntime::lifecycle() const noexcept {
    return lifecycle_;
}

QuestError DeterministicQuestRuntime::validate_definition(
    const contracts::VerticalSliceDefinition& definition
) const noexcept {
    if (definition.id.key == 0 || definition.beats.empty()) {
        return QuestError::invalid_definition;
    }
    if (definition.beats.size() > stage_capacity) {
        return QuestError::too_many_stages;
    }
    std::array<contracts::StableContentKey, objective_capacity> objectives{};
    std::size_t objective_count = 0;
    for (const auto& stage : definition.beats) {
        if (stage.id.key == 0 || stage.objectives.empty()) {
            return QuestError::invalid_definition;
        }
        if (stage.objectives.size() > objective_capacity - objective_count) {
            return QuestError::too_many_objectives;
        }
        for (const auto& objective : stage.objectives) {
            if (objective.key == 0) {
                return QuestError::invalid_definition;
            }
            if (std::find(objectives.begin(), objectives.begin() + objective_count, objective.key) !=
                objectives.begin() + objective_count) {
                return QuestError::duplicate_objective;
            }
            objectives[objective_count++] = objective.key;
        }
    }
    return QuestError::none;
}

std::size_t DeterministicQuestRuntime::objective_stage(
    contracts::StableContentKey objective
) const noexcept {
    if (definition_ == nullptr) {
        return stage_capacity;
    }
    for (std::size_t stage = 0; stage < definition_->beats.size(); ++stage) {
        const auto& objectives = definition_->beats[stage].objectives;
        if (std::find_if(
                objectives.begin(),
                objectives.end(),
                [objective](const contracts::ContentId& value) {
                    return value.key == objective;
                }
            ) != objectives.end()) {
            return stage;
        }
    }
    return stage_capacity;
}

bool DeterministicQuestRuntime::is_completed(
    contracts::StableContentKey objective
) const noexcept {
    return std::find(
               completed_objectives_.begin(),
               completed_objectives_.begin() + completed_objective_count_,
               objective
           ) != completed_objectives_.begin() + completed_objective_count_;
}

std::size_t DeterministicQuestRuntime::completed_in_stage(std::size_t stage_index) const noexcept {
    const auto& objectives = definition_->beats[stage_index].objectives;
    return static_cast<std::size_t>(std::count_if(
        objectives.begin(),
        objectives.end(),
        [this](const contracts::ContentId& objective) {
            return is_completed(objective.key);
        }
    ));
}

bool DeterministicQuestRuntime::active_stage_complete() const noexcept {
    return completed_in_stage(stage_index_) == definition_->beats[stage_index_].objectives.size();
}

void DeterministicQuestRuntime::refresh_snapshot() noexcept {
    snapshot_.quest = definition_->id.key;
    snapshot_.stage = definition_->beats[stage_index_].id.key;
    snapshot_.stage_index = static_cast<std::uint16_t>(stage_index_);
    snapshot_.stage_count = static_cast<std::uint16_t>(definition_->beats.size());
    snapshot_.completed_in_stage = static_cast<std::uint16_t>(completed_in_stage(stage_index_));
    snapshot_.required_in_stage =
        static_cast<std::uint16_t>(definition_->beats[stage_index_].objectives.size());
    snapshot_.completed_total = static_cast<std::uint16_t>(completed_objective_count_);
    snapshot_.resolved = lifecycle_ == QuestLifecycle::resolved;
    update_checksum();
}

void DeterministicQuestRuntime::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, snapshot_.tick);
    hash_integer(hash, snapshot_.quest);
    hash_integer(hash, snapshot_.stage);
    hash_integer(hash, snapshot_.stage_index);
    hash_integer(hash, snapshot_.stage_count);
    hash_integer(hash, snapshot_.completed_in_stage);
    hash_integer(hash, snapshot_.required_in_stage);
    hash_integer(hash, snapshot_.completed_total);
    hash_byte(hash, snapshot_.resolved ? 1U : 0U);
    hash_integer(hash, last_command_sequence_);
    for (std::size_t index = 0; index < completed_objective_count_; ++index) {
        hash_integer(hash, completed_objectives_[index]);
    }
    snapshot_.checksum = hash;
}

}  // namespace tgd::gameplay
