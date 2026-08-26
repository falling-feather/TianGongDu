#pragma once

#include <tgd/contracts/craft_definition.hpp>
#include <tgd/contracts/sandbox_pack.hpp>
#include <tgd/contracts/workshop_definition.hpp>
#include <tgd/gameplay/craft_session.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace tgd::gameplay {

enum class WorkshopSessionBuildError : std::uint8_t {
    none = 0,
    invalid_definition = 1,
    workshop_not_found = 2,
    order_not_found = 3,
    capacity_exceeded = 4,
    craft_initialize_failed = 5,
    invalid = 255,
};

enum class WorkshopActionDisposition : std::uint8_t {
    applied = 1,
    wrong_stage = 2,
    unknown_target = 3,
    wrong_order = 4,
    insufficient_stock = 5,
    insufficient_funds = 6,
    quality_too_low = 7,
    already_fulfilled = 8,
    invalid_state = 9,
    invalid = 255,
};

struct WorkshopMaterialStockSnapshot final {
    contracts::StableContentKey material{};
    std::uint16_t remaining_quantity{};

    [[nodiscard]] friend constexpr bool operator==(
        const WorkshopMaterialStockSnapshot&,
        const WorkshopMaterialStockSnapshot&
    ) noexcept = default;
};

struct WorkshopSessionSnapshot final {
    bool initialized{};
    contracts::StableContentKey workshop{};
    contracts::StableContentKey workstation{};
    contracts::StableContentKey order{};
    contracts::StableContentKey process{};
    contracts::StableContentKey selected_material{};
    contracts::StableContentKey output_item{};
    contracts::StableContentKey unlocked_route{};
    std::int32_t funds{};
    std::int32_t spent_funds{};
    std::uint16_t item_quality{};
    std::uint16_t minimum_quality{};
    std::uint16_t delivered_quantity{};
    std::uint16_t required_quantity{};
    bool workstation_occupied{};
    bool output_ready{};
    bool order_fulfilled{};
    std::uint16_t delivery_count{};
    CraftSessionSnapshot craft{};
    std::uint64_t checksum{};

    [[nodiscard]] friend constexpr bool operator==(
        const WorkshopSessionSnapshot&,
        const WorkshopSessionSnapshot&
    ) noexcept = default;
};

struct WorkshopActionResult final {
    WorkshopActionDisposition disposition{WorkshopActionDisposition::invalid};
    WorkshopSessionSnapshot snapshot{};
};

class WorkshopSession final {
  public:
    static constexpr std::size_t stock_capacity =
        contracts::sandbox_workshop_material_stock_capacity;

    [[nodiscard]] WorkshopSessionBuildError initialize(
        const contracts::CraftDefinition& craft,
        const contracts::WorkshopDefinition& workshop,
        contracts::StableContentKey workshop_id,
        contracts::StableContentKey order_id
    ) noexcept;
    [[nodiscard]] WorkshopActionResult select_material(
        contracts::StableContentKey material
    ) noexcept;
    [[nodiscard]] WorkshopActionResult perform_operation(
        contracts::StableContentKey step
    ) noexcept;
    [[nodiscard]] WorkshopActionResult run_trial() noexcept;
    [[nodiscard]] WorkshopActionResult perform_rework() noexcept;
    [[nodiscard]] WorkshopActionResult deliver_order() noexcept;
    [[nodiscard]] WorkshopSessionBuildError restart() noexcept;
    [[nodiscard]] WorkshopSessionSnapshot snapshot() const noexcept;
    [[nodiscard]] std::uint16_t stock_remaining(
        contracts::StableContentKey material
    ) const noexcept;

  private:
    struct StockRuntime final {
        const contracts::WorkshopMaterialStockDefinition* definition{};
        std::uint16_t remaining_quantity{};
    };

    [[nodiscard]] WorkshopSessionBuildError build_runtime() noexcept;
    [[nodiscard]] WorkshopActionResult reject(
        WorkshopActionDisposition disposition
    ) const noexcept;
    [[nodiscard]] StockRuntime* find_stock(
        contracts::StableContentKey material
    ) noexcept;
    [[nodiscard]] const StockRuntime* find_stock(
        contracts::StableContentKey material
    ) const noexcept;
    void refresh_snapshot() noexcept;

    const contracts::CraftDefinition* craft_definition_{};
    const contracts::WorkshopDefinition* workshop_definition_{};
    const contracts::WorkshopDefinitionRecord* workshop_{};
    const contracts::WorkshopOrderDefinition* order_{};
    std::array<StockRuntime, stock_capacity> stocks_{};
    std::size_t stock_count_{};
    CraftSession craft_session_{};
    contracts::StableContentKey selected_material_{};
    std::int32_t funds_{};
    std::int32_t spent_funds_{};
    std::uint16_t item_quality_{};
    std::uint16_t delivered_quantity_{};
    std::uint16_t delivery_count_{};
    bool workstation_occupied_{};
    bool order_fulfilled_{};
    bool initialized_{};
    WorkshopSessionSnapshot snapshot_{};
};

}  // namespace tgd::gameplay
