#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <cstdint>
#include <span>

namespace tgd::contracts {

// Append-only: serialized values are part of .tgdsbx v1.3+.
enum class CraftMaterialOutcome : std::uint8_t {
    passes_trial = 1,
    requires_rework = 2,
    invalid = 255,
};

enum class CraftStepKind : std::uint8_t {
    operation = 1,
    trial = 2,
    rework = 3,
    invalid = 255,
};

struct CraftMaterialDefinition final {
    ContentId id{};

    [[nodiscard]] friend constexpr bool operator==(
        const CraftMaterialDefinition&,
        const CraftMaterialDefinition&
    ) noexcept = default;
};

struct CraftWorkstationDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};

    [[nodiscard]] friend constexpr bool operator==(
        const CraftWorkstationDefinition&,
        const CraftWorkstationDefinition&
    ) noexcept = default;
};

struct CraftProcessDefinition final {
    ContentId id{};
    ContentId workstation_id{};
    ContentId need_id{};
    ContentId output_item_id{};
    ContentId trial_step_id{};

    [[nodiscard]] friend constexpr bool operator==(
        const CraftProcessDefinition&,
        const CraftProcessDefinition&
    ) noexcept = default;
};

struct CraftMaterialChoiceDefinition final {
    ContentId process_id{};
    ContentId material_id{};
    CraftMaterialOutcome outcome{CraftMaterialOutcome::invalid};

    [[nodiscard]] friend constexpr bool operator==(
        const CraftMaterialChoiceDefinition&,
        const CraftMaterialChoiceDefinition&
    ) noexcept = default;
};

struct CraftStepDefinition final {
    ContentId id{};
    ContentId process_id{};
    // Empty only for the first operation in a process.
    ContentId predecessor_step_id{};
    ContentId action_id{};
    CraftStepKind kind{CraftStepKind::invalid};

    [[nodiscard]] friend constexpr bool operator==(
        const CraftStepDefinition&,
        const CraftStepDefinition&
    ) noexcept = default;
};

// Immutable and non-owning. Region and asset references resolve against the
// containing SandboxDefinition; every other reference resolves within this view.
struct CraftDefinition final {
    std::span<const CraftMaterialDefinition> materials{};
    std::span<const CraftWorkstationDefinition> workstations{};
    std::span<const CraftProcessDefinition> processes{};
    std::span<const CraftMaterialChoiceDefinition> material_choices{};
    std::span<const CraftStepDefinition> steps{};
};

}  // namespace tgd::contracts
