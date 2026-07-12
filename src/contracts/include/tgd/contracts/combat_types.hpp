#pragma once

#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/session_types.hpp>

#include <array>
#include <cstdint>
#include <span>

namespace tgd::contracts {

enum class CombatFaction : std::uint8_t {
    player,
    hostile,
    neutral,
};

enum class CombatCommandType : std::uint8_t {
    light_attack,
    heavy_attack,
    guard_started,
    guard_ended,
    evade,
    switch_stance,
};

enum class CombatEventType : std::uint8_t {
    stance_changed,
    guard_changed,
    ability_started,
    attack_missed,
    hit_landed,
    hit_guarded,
    hit_evaded,
    poise_broken,
    actor_defeated,
    command_ignored,
    stamina_recovered,
    poise_recovered,
    encounter_restarted,
};

enum CombatFeedbackTag : std::uint32_t {
    feedback_none = 0,
    feedback_light = 1U << 0U,
    feedback_heavy = 1U << 1U,
    feedback_guard = 1U << 2U,
    feedback_poise_break = 1U << 3U,
    feedback_evade = 1U << 4U,
};

struct CombatResources final {
    std::int32_t health{};
    std::int32_t health_max{};
    std::int32_t stamina{};
    std::int32_t stamina_max{};
    std::int32_t poise{};
    std::int32_t poise_max{};
    std::int32_t lantern{};
    std::int32_t lantern_max{};
    std::int32_t evidence{};

    [[nodiscard]] friend constexpr bool operator==(
        const CombatResources&,
        const CombatResources&
    ) noexcept = default;
};

struct CombatRecoveryDefinition final {
    std::uint16_t stamina_delay_ticks{};
    std::uint16_t stamina_interval_ticks{};
    std::int32_t stamina_per_interval{};
    std::uint16_t poise_delay_ticks{};
    std::uint16_t poise_interval_ticks{};
    std::int32_t poise_per_interval{};
};

struct CombatCommand final {
    TickIndex tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    CombatCommandType type{CombatCommandType::light_attack};
    StableActorKey target{};
    StableContentKey stance{};
};

struct CombatPoseUpdate final {
    TickIndex tick{};
    StableActorKey actor{};
    GroundPoseMm pose{};
};

struct AbilityDefinition final {
    ContentId id{};
    CombatCommandType trigger{CombatCommandType::light_attack};
    StableContentKey required_stance{};
    std::int32_t stamina_cost{};
    std::uint16_t windup_ticks{};
    std::uint16_t active_ticks{1};
    std::uint16_t recovery_ticks{};
    std::int32_t range_mm{};
    std::int32_t height_tolerance_mm{};
    std::int32_t health_damage{};
    std::int32_t poise_damage{};
    std::uint32_t feedback_tags{};
};

struct CombatActorConfig final {
    StableActorKey actor{};
    ContentId archetype_id{};
    CombatFaction faction{CombatFaction::neutral};
    GroundPoseMm initial_pose{};
    CombatResources initial_resources{};
    std::array<StableContentKey, 4> stance_ids{};
    std::uint8_t stance_count{};
    StableContentKey initial_stance{};
    CombatRecoveryDefinition recovery{};
    bool initially_active{true};
};

struct CombatActorSnapshot final {
    StableActorKey actor{};
    StableContentKey archetype{};
    CombatFaction faction{CombatFaction::neutral};
    GroundPoseMm pose{};
    CombatResources resources{};
    StableContentKey stance{};
    StableContentKey active_ability{};
    bool guarding{};
    bool active{};
    bool defeated{};
};

struct CombatEvent final {
    TickIndex tick{};
    CombatEventType type{CombatEventType::command_ignored};
    StableActorKey source{};
    StableActorKey target{};
    StableContentKey ability{};
    std::int32_t value{};
    std::uint32_t feedback_tags{};
};

struct EncounterDirectorDefinition final {
    StableActorKey player_actor{};
    std::int32_t aggro_range_mm{};
    std::int32_t leash_range_mm{};
    std::int32_t chase_speed_mm_per_second{};
    std::int32_t formation_radius_mm{};
    std::uint16_t decision_interval_ticks{};
    std::uint16_t post_attack_cooldown_ticks{};
    std::uint8_t max_simultaneous_attackers{};
};

struct CombatEncounterDefinition final {
    ContentId id{};
    std::span<const CombatActorConfig> actors{};
    std::span<const AbilityDefinition> abilities{};
    EncounterDirectorDefinition director{};
};

}  // namespace tgd::contracts
