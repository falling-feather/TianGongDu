#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <cstdint>
#include <span>

namespace tgd::contracts {

// Append-only: serialized values are part of .tgdsbx v1.4+.
enum class WorkshopOrderConsequenceKind : std::uint8_t {
    operate_route_interaction = 1,
    invalid = 255,
};

struct WorkshopDefinitionRecord final {
    ContentId id{};
    ContentId workstation_id{};
    std::int32_t initial_funds{};

    [[nodiscard]] friend constexpr bool operator==(
        const WorkshopDefinitionRecord&,
        const WorkshopDefinitionRecord&
    ) noexcept = default;
};

struct WorkshopMaterialStockDefinition final {
    ContentId workshop_id{};
    ContentId material_id{};
    std::int32_t unit_cost{};
    std::uint16_t initial_quantity{};
    std::uint16_t base_quality{};
    std::uint16_t rework_quality_gain{};

    [[nodiscard]] friend constexpr bool operator==(
        const WorkshopMaterialStockDefinition&,
        const WorkshopMaterialStockDefinition&
    ) noexcept = default;
};

struct WorkshopOrderDefinition final {
    ContentId id{};
    ContentId workshop_id{};
    ContentId process_id{};
    ContentId consequence_target_id{};
    std::uint16_t required_quantity{};
    std::uint16_t minimum_quality{};
    std::int32_t reward_funds{};
    WorkshopOrderConsequenceKind consequence_kind{
        WorkshopOrderConsequenceKind::invalid
    };

    [[nodiscard]] friend constexpr bool operator==(
        const WorkshopOrderDefinition&,
        const WorkshopOrderDefinition&
    ) noexcept = default;
};

// Immutable and non-owning. Workstation/material/process references resolve
// against CraftDefinition; route interactions resolve against SandboxDefinition.
struct WorkshopDefinition final {
    std::span<const WorkshopDefinitionRecord> workshops{};
    std::span<const WorkshopMaterialStockDefinition> material_stocks{};
    std::span<const WorkshopOrderDefinition> orders{};
};

}  // namespace tgd::contracts
