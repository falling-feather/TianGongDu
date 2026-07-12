#pragma once

#include <tgd/contracts/session_types.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace tgd::contracts {

[[nodiscard]] constexpr StableContentKey stable_content_key(std::string_view name) noexcept {
    StableContentKey hash = 14'695'981'039'346'656'037ULL;
    for (const auto character : name) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

struct ContentId final {
    StableContentKey key{};
    std::string_view name{};

    [[nodiscard]] friend constexpr bool operator==(const ContentId&, const ContentId&) noexcept =
        default;
};

[[nodiscard]] constexpr ContentId content_id(std::string_view name) noexcept {
    return {stable_content_key(name), name};
}

enum class VerticalSliceBeatKind : std::uint8_t {
    exploration,
    training,
    combat,
    investigation,
    boss,
    resolution,
};

struct VerticalSlicePlayerSeed final {
    StableActorKey actor{};
    GroundPoseMm initial_pose{};
    std::int32_t move_speed_mm_per_second{};
    std::int32_t jump_speed_mm_per_second{};
    std::int32_t gravity_mm_per_second_squared{};
    std::int32_t collision_radius_mm{};
    std::int32_t collision_height_mm{};
    CameraBasisQ15 camera_basis{};
};

struct VerticalSliceBeatDefinition final {
    ContentId id{};
    VerticalSliceBeatKind kind{VerticalSliceBeatKind::exploration};
    std::uint16_t target_minutes{};
    ContentId cell_id{};
    std::span<const ContentId> objectives{};
};

struct VerticalSliceSafePointDefinition final {
    ContentId id{};
    ContentId beat_id{};
    GroundPoseMm pose{};
};

enum class QuestInteractionKind : std::uint8_t {
    inspect,
    operate,
    talk,
    choose,
};

struct QuestInteractionDefinition final {
    ContentId id{};
    QuestInteractionKind kind{QuestInteractionKind::inspect};
    ContentId cell_id{};
    ContentId objective_id{};
    ContentId selection_id{};
    GroundPoseMm pose{};
    std::int32_t radius_mm{};
    std::span<const ContentId> prerequisite_objectives{};
};

enum class QuestCombatTriggerKind : std::uint8_t {
    player_ability_started,
    player_stance_changed,
    player_hit_guarded,
    player_hit_evaded,
};

struct QuestCombatTriggerDefinition final {
    ContentId id{};
    QuestCombatTriggerKind kind{QuestCombatTriggerKind::player_hit_guarded};
    ContentId objective_id{};
    StableContentKey required_stance{};
    StableContentKey required_ability{};
    std::span<const ContentId> prerequisite_objectives{};
};

enum class QuestCombatOutcomeKind : std::uint8_t {
    hostile_archetype_defeated,
    all_hostiles_defeated,
};

struct QuestCombatOutcomeDefinition final {
    ContentId id{};
    QuestCombatOutcomeKind kind{QuestCombatOutcomeKind::hostile_archetype_defeated};
    ContentId objective_id{};
    ContentId archetype_id{};
    std::uint16_t required_count{};
};

struct QuestEncounterActivationDefinition final {
    ContentId id{};
    ContentId beat_id{};
    ContentId encounter_id{};
    std::span<const StableActorKey> actor_keys{};
};

struct QuestBossPhaseDefinition final {
    ContentId id{};
    ContentId objective_id{};
    StableActorKey actor{};
    std::uint8_t health_percent{};
    StableContentKey next_stance{};
};

struct QuestResolutionRewardDefinition final {
    ContentId id{};
    ContentId objective_id{};
    ContentId selection_id{};
    ContentId reward_id{};
    ContentId reward_dedup_key{};
};

struct VerticalSliceDefinition final {
    ContentId id{};
    std::string_view view_model{};
    std::string_view primary_guidance{};
    std::string_view secondary_reference{};
    std::string_view camera_mode{};
    std::uint16_t playable_target_minutes{};
    std::uint16_t end_to_end_test_budget_minutes{};
    std::uint16_t playable_activity_grace_ticks{};
    ContentId start_fixture_id{};
    ContentId chapter_id{};
    ContentId boss_id{};
    VerticalSlicePlayerSeed player{};
    std::span<const ContentId> subregion_ids{};
    std::span<const ContentId> npc_ids{};
    std::span<const ContentId> enemy_family_ids{};
    std::span<const ContentId> cell_ids{};
    std::span<const VerticalSliceBeatDefinition> beats{};
    std::span<const VerticalSliceSafePointDefinition> safe_points{};
    std::span<const QuestInteractionDefinition> quest_interactions{};
    std::span<const QuestCombatTriggerDefinition> quest_combat_triggers{};
    std::span<const QuestCombatOutcomeDefinition> quest_combat_outcomes{};
    std::span<const QuestEncounterActivationDefinition> quest_encounter_activations{};
    std::span<const QuestBossPhaseDefinition> quest_boss_phases{};
    std::span<const QuestResolutionRewardDefinition> quest_resolution_rewards{};
};

}  // namespace tgd::contracts
