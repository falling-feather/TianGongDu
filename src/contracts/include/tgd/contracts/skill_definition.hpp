#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <cstddef>
#include <cstdint>

namespace tgd::contracts {

// Ability targeting stays authored data. Platform input can request an ability,
// but only Gameplay may decide whether the current actor/target satisfies this policy.
enum class AbilityTargetPolicy : std::uint8_t {
    trigger_default,
    opposing_actor,
    self_actor,
    no_target,
};

enum class CombatSkillSlot : std::uint8_t {
    none,
    primary,
    secondary,
    utility,
    special,
};

inline constexpr std::size_t combat_skill_slot_capacity = 4;
inline constexpr std::size_t combat_skill_binding_capacity = 16;

struct CombatSkillBindingDefinition final {
    StableContentKey stance{};
    CombatSkillSlot slot{CombatSkillSlot::none};
    StableContentKey ability{};

    [[nodiscard]] friend constexpr bool operator==(
        const CombatSkillBindingDefinition&,
        const CombatSkillBindingDefinition&
    ) noexcept = default;
};

inline constexpr std::uint16_t max_ability_cooldown_ticks = 3'600;
inline constexpr std::uint16_t max_ability_phase_ticks = 3'600;
inline constexpr std::int32_t max_ability_stamina_cost = 1'000'000;
inline constexpr std::int32_t max_ability_range_mm = 1'000'000;
inline constexpr std::int32_t max_ability_damage = 1'000'000;

}  // namespace tgd::contracts
