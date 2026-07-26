#include <tgd/gameplay/workshop_session.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset_basis = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;

template <typename Value>
void hash_integer(std::uint64_t& hash, Value value) noexcept {
    using Unsigned = std::make_unsigned_t<Value>;
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Value); ++index) {
        hash ^= static_cast<std::uint8_t>(bits & 0xffU);
        hash *= fnv_prime;
        bits >>= 8U;
    }
}

[[nodiscard]] WorkshopActionDisposition translate_disposition(
    CraftActionDisposition disposition
) noexcept {
    switch (disposition) {
        case CraftActionDisposition::applied:
            return WorkshopActionDisposition::applied;
        case CraftActionDisposition::wrong_stage:
            return WorkshopActionDisposition::wrong_stage;
        case CraftActionDisposition::unknown_target:
            return WorkshopActionDisposition::unknown_target;
        case CraftActionDisposition::wrong_order:
            return WorkshopActionDisposition::wrong_order;
        case CraftActionDisposition::invalid_state:
        case CraftActionDisposition::invalid:
            return WorkshopActionDisposition::invalid_state;
    }
    return WorkshopActionDisposition::invalid_state;
}

}  // namespace

WorkshopSessionBuildError WorkshopSession::initialize(
    const contracts::CraftDefinition& craft,
    const contracts::WorkshopDefinition& workshop,
    contracts::StableContentKey workshop_id,
    contracts::StableContentKey order_id
) noexcept {
    if (initialized_ || workshop_id == 0 || order_id == 0) {
        return WorkshopSessionBuildError::invalid_definition;
    }
    if (workshop.material_stocks.size() > stock_capacity) {
        return WorkshopSessionBuildError::capacity_exceeded;
    }
    const auto workshop_found = std::find_if(
        workshop.workshops.begin(),
        workshop.workshops.end(),
        [workshop_id](const auto& value) { return value.id.key == workshop_id; }
    );
    if (workshop_found == workshop.workshops.end()) {
        return WorkshopSessionBuildError::workshop_not_found;
    }
    const auto order_found = std::find_if(
        workshop.orders.begin(),
        workshop.orders.end(),
        [order_id](const auto& value) { return value.id.key == order_id; }
    );
    if (order_found == workshop.orders.end() ||
        order_found->workshop_id.key != workshop_id) {
        return WorkshopSessionBuildError::order_not_found;
    }
    WorkshopSession candidate;
    candidate.craft_definition_ = &craft;
    candidate.workshop_definition_ = &workshop;
    candidate.workshop_ = &*workshop_found;
    candidate.order_ = &*order_found;
    const auto result = candidate.build_runtime();
    if (result == WorkshopSessionBuildError::none) {
        *this = std::move(candidate);
    }
    return result;
}

WorkshopSessionBuildError WorkshopSession::build_runtime() noexcept {
    if (craft_definition_ == nullptr || workshop_definition_ == nullptr ||
        workshop_ == nullptr || order_ == nullptr ||
        workshop_->initial_funds < 0 || order_->required_quantity == 0 ||
        order_->minimum_quality == 0) {
        return WorkshopSessionBuildError::invalid_definition;
    }

    stocks_ = {};
    stock_count_ = 0;
    for (const auto& value : workshop_definition_->material_stocks) {
        if (value.workshop_id.key != workshop_->id.key) {
            continue;
        }
        if (stock_count_ >= stocks_.size() || value.initial_quantity == 0 ||
            value.unit_cost <= 0 || value.base_quality == 0 ||
            value.base_quality > 10'000U ||
            value.rework_quality_gain > 10'000U - value.base_quality) {
            return WorkshopSessionBuildError::invalid_definition;
        }
        stocks_[stock_count_++] = {&value, value.initial_quantity};
    }
    if (stock_count_ == 0) {
        return WorkshopSessionBuildError::invalid_definition;
    }
    craft_session_ = {};
    const auto craft_result =
        craft_session_.initialize(*craft_definition_, order_->process_id.key);
    if (craft_result != CraftSessionBuildError::none) {
        return WorkshopSessionBuildError::craft_initialize_failed;
    }

    selected_material_ = 0;
    funds_ = workshop_->initial_funds;
    spent_funds_ = 0;
    item_quality_ = 0;
    delivered_quantity_ = 0;
    delivery_count_ = 0;
    workstation_occupied_ = false;
    order_fulfilled_ = false;
    initialized_ = true;
    refresh_snapshot();
    return WorkshopSessionBuildError::none;
}

WorkshopActionResult WorkshopSession::select_material(
    contracts::StableContentKey material
) noexcept {
    if (!initialized_ || order_fulfilled_) {
        return reject(
            order_fulfilled_ ? WorkshopActionDisposition::already_fulfilled
                             : WorkshopActionDisposition::invalid_state
        );
    }
    const auto* source_stock = find_stock(material);
    if (source_stock == nullptr) {
        return reject(WorkshopActionDisposition::unknown_target);
    }
    if (source_stock->remaining_quantity == 0) {
        return reject(WorkshopActionDisposition::insufficient_stock);
    }
    if (funds_ < source_stock->definition->unit_cost) {
        return reject(WorkshopActionDisposition::insufficient_funds);
    }

    auto candidate = *this;
    auto* stock = candidate.find_stock(material);
    const auto selected = candidate.craft_session_.select_material(material);
    if (selected.disposition != CraftActionDisposition::applied || stock == nullptr) {
        return reject(translate_disposition(selected.disposition));
    }
    --stock->remaining_quantity;
    candidate.funds_ -= stock->definition->unit_cost;
    candidate.spent_funds_ += stock->definition->unit_cost;
    candidate.selected_material_ = material;
    candidate.item_quality_ = stock->definition->base_quality;
    candidate.workstation_occupied_ = true;
    candidate.refresh_snapshot();
    *this = std::move(candidate);
    return {WorkshopActionDisposition::applied, snapshot_};
}

WorkshopActionResult WorkshopSession::perform_operation(
    contracts::StableContentKey step
) noexcept {
    if (!initialized_) {
        return reject(WorkshopActionDisposition::invalid_state);
    }
    auto candidate = *this;
    const auto result = candidate.craft_session_.perform_operation(step);
    if (result.disposition != CraftActionDisposition::applied) {
        return reject(translate_disposition(result.disposition));
    }
    candidate.refresh_snapshot();
    *this = std::move(candidate);
    return {WorkshopActionDisposition::applied, snapshot_};
}

WorkshopActionResult WorkshopSession::run_trial() noexcept {
    if (!initialized_) {
        return reject(WorkshopActionDisposition::invalid_state);
    }
    auto candidate = *this;
    const auto result = candidate.craft_session_.run_trial();
    if (result.disposition != CraftActionDisposition::applied) {
        return reject(translate_disposition(result.disposition));
    }
    candidate.refresh_snapshot();
    *this = std::move(candidate);
    return {WorkshopActionDisposition::applied, snapshot_};
}

WorkshopActionResult WorkshopSession::perform_rework() noexcept {
    if (!initialized_) {
        return reject(WorkshopActionDisposition::invalid_state);
    }
    auto candidate = *this;
    const auto result = candidate.craft_session_.perform_rework();
    if (result.disposition != CraftActionDisposition::applied) {
        return reject(translate_disposition(result.disposition));
    }
    const auto* stock = candidate.find_stock(candidate.selected_material_);
    if (stock == nullptr) {
        return reject(WorkshopActionDisposition::invalid_state);
    }
    const auto quality =
        static_cast<std::uint32_t>(candidate.item_quality_) +
        stock->definition->rework_quality_gain;
    candidate.item_quality_ = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(quality, 10'000U)
    );
    candidate.refresh_snapshot();
    *this = std::move(candidate);
    return {WorkshopActionDisposition::applied, snapshot_};
}

WorkshopActionResult WorkshopSession::deliver_order() noexcept {
    if (!initialized_) {
        return reject(WorkshopActionDisposition::invalid_state);
    }
    if (order_fulfilled_) {
        return reject(WorkshopActionDisposition::already_fulfilled);
    }
    if (!craft_session_.snapshot().completed || !workstation_occupied_) {
        return reject(WorkshopActionDisposition::wrong_stage);
    }
    if (item_quality_ < order_->minimum_quality) {
        return reject(WorkshopActionDisposition::quality_too_low);
    }
    if (delivered_quantity_ == std::numeric_limits<std::uint16_t>::max() ||
        delivery_count_ == std::numeric_limits<std::uint16_t>::max()) {
        return reject(WorkshopActionDisposition::invalid_state);
    }

    auto candidate = *this;
    ++candidate.delivered_quantity_;
    ++candidate.delivery_count_;
    candidate.workstation_occupied_ = false;
    candidate.order_fulfilled_ =
        candidate.delivered_quantity_ >= candidate.order_->required_quantity;
    if (candidate.order_fulfilled_) {
        if (candidate.order_->reward_funds >
            std::numeric_limits<std::int32_t>::max() - candidate.funds_) {
            return reject(WorkshopActionDisposition::invalid_state);
        }
        candidate.funds_ += candidate.order_->reward_funds;
    } else {
        if (candidate.craft_session_.restart() != CraftSessionBuildError::none) {
            return reject(WorkshopActionDisposition::invalid_state);
        }
        candidate.selected_material_ = 0;
        candidate.item_quality_ = 0;
    }
    candidate.refresh_snapshot();
    *this = std::move(candidate);
    return {WorkshopActionDisposition::applied, snapshot_};
}

WorkshopSessionBuildError WorkshopSession::restart() noexcept {
    if (!initialized_) {
        return WorkshopSessionBuildError::invalid_definition;
    }
    auto candidate = *this;
    const auto result = candidate.build_runtime();
    if (result == WorkshopSessionBuildError::none) {
        *this = std::move(candidate);
    }
    return result;
}

WorkshopSessionSnapshot WorkshopSession::snapshot() const noexcept {
    return snapshot_;
}

std::uint16_t WorkshopSession::stock_remaining(
    contracts::StableContentKey material
) const noexcept {
    const auto* stock = find_stock(material);
    return stock == nullptr ? 0 : stock->remaining_quantity;
}

WorkshopActionResult WorkshopSession::reject(
    WorkshopActionDisposition disposition
) const noexcept {
    return {disposition, snapshot_};
}

WorkshopSession::StockRuntime* WorkshopSession::find_stock(
    contracts::StableContentKey material
) noexcept {
    const auto end =
        stocks_.begin() + static_cast<std::ptrdiff_t>(stock_count_);
    const auto found = std::find_if(
        stocks_.begin(),
        end,
        [material](const auto& value) {
            return value.definition != nullptr &&
                   value.definition->material_id.key == material;
        }
    );
    return found == end ? nullptr : &*found;
}

const WorkshopSession::StockRuntime* WorkshopSession::find_stock(
    contracts::StableContentKey material
) const noexcept {
    const auto end =
        stocks_.begin() + static_cast<std::ptrdiff_t>(stock_count_);
    const auto found = std::find_if(
        stocks_.begin(),
        end,
        [material](const auto& value) {
            return value.definition != nullptr &&
                   value.definition->material_id.key == material;
        }
    );
    return found == end ? nullptr : &*found;
}

void WorkshopSession::refresh_snapshot() noexcept {
    snapshot_ = {};
    snapshot_.initialized = initialized_;
    if (!initialized_ || workshop_ == nullptr || order_ == nullptr) {
        return;
    }
    snapshot_.workshop = workshop_->id.key;
    snapshot_.workstation = workshop_->workstation_id.key;
    snapshot_.order = order_->id.key;
    snapshot_.process = order_->process_id.key;
    snapshot_.selected_material = selected_material_;
    snapshot_.funds = funds_;
    snapshot_.spent_funds = spent_funds_;
    snapshot_.item_quality = item_quality_;
    snapshot_.minimum_quality = order_->minimum_quality;
    snapshot_.delivered_quantity = delivered_quantity_;
    snapshot_.required_quantity = order_->required_quantity;
    snapshot_.workstation_occupied = workstation_occupied_;
    snapshot_.order_fulfilled = order_fulfilled_;
    snapshot_.delivery_count = delivery_count_;
    snapshot_.craft = craft_session_.snapshot();
    snapshot_.output_item = snapshot_.craft.output_item;
    snapshot_.output_ready = snapshot_.craft.completed && workstation_occupied_;
    snapshot_.unlocked_route =
        order_fulfilled_ &&
                order_->consequence_kind ==
                    contracts::WorkshopOrderConsequenceKind::
                        operate_route_interaction
            ? order_->consequence_target_id.key
            : 0;

    auto hash = fnv_offset_basis;
    hash_integer(hash, static_cast<std::uint8_t>(snapshot_.initialized));
    hash_integer(hash, snapshot_.workshop);
    hash_integer(hash, snapshot_.workstation);
    hash_integer(hash, snapshot_.order);
    hash_integer(hash, snapshot_.process);
    hash_integer(hash, snapshot_.selected_material);
    hash_integer(hash, snapshot_.output_item);
    hash_integer(hash, snapshot_.unlocked_route);
    hash_integer(hash, snapshot_.funds);
    hash_integer(hash, snapshot_.spent_funds);
    hash_integer(hash, snapshot_.item_quality);
    hash_integer(hash, snapshot_.minimum_quality);
    hash_integer(hash, snapshot_.delivered_quantity);
    hash_integer(hash, snapshot_.required_quantity);
    hash_integer(
        hash, static_cast<std::uint8_t>(snapshot_.workstation_occupied)
    );
    hash_integer(hash, static_cast<std::uint8_t>(snapshot_.output_ready));
    hash_integer(hash, static_cast<std::uint8_t>(snapshot_.order_fulfilled));
    hash_integer(hash, snapshot_.delivery_count);
    hash_integer(hash, snapshot_.craft.checksum);
    for (std::size_t index = 0; index < stock_count_; ++index) {
        hash_integer(hash, stocks_[index].definition->material_id.key);
        hash_integer(hash, stocks_[index].remaining_quantity);
    }
    snapshot_.checksum = hash;
}

}  // namespace tgd::gameplay
