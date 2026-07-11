// Generated from content/design/f1-vertical-slice.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/combat_types.hpp>
#include <tgd/contracts/content_definition.hpp>

#include <array>
#include <span>

namespace tgd::content::generated {

inline constexpr std::array<contracts::ContentId, 2> beat_0_objectives{{
    contracts::content_id("f1_objective_reach_ferry_gate"),
    contracts::content_id("f1_objective_inspect_travel_writ"),
}};

inline constexpr std::array<contracts::ContentId, 3> beat_1_objectives{{
    contracts::content_id("f1_objective_meet_shen_yan"),
    contracts::content_id("f1_objective_eavesguard_counter"),
    contracts::content_id("f1_objective_flower_turn_counter"),
}};

inline constexpr std::array<contracts::ContentId, 3> beat_2_objectives{{
    contracts::content_id("f1_objective_defeat_leaking_dolls"),
    contracts::content_id("f1_objective_answer_paper_egret"),
    contracts::content_id("f1_objective_choose_lane_route"),
}};

inline constexpr std::array<contracts::ContentId, 4> beat_3_objectives{{
    contracts::content_id("f1_objective_reveal_spring_trace"),
    contracts::content_id("f1_objective_reveal_winter_trace"),
    contracts::content_id("f1_objective_review_shared_ledger"),
    contracts::content_id("f1_objective_choose_rib_calibration"),
}};

inline constexpr std::array<contracts::ContentId, 2> beat_4_objectives{{
    contracts::content_id("f1_objective_validate_calibration"),
    contracts::content_id("f1_objective_open_return_shortcut"),
}};

inline constexpr std::array<contracts::ContentId, 4> beat_5_objectives{{
    contracts::content_id("f1_objective_survive_spring_phase"),
    contracts::content_id("f1_objective_survive_summer_phase"),
    contracts::content_id("f1_objective_survive_autumn_phase"),
    contracts::content_id("f1_objective_survive_winter_phase"),
}};

inline constexpr std::array<contracts::ContentId, 2> beat_6_objectives{{
    contracts::content_id("f1_objective_choose_resolution"),
    contracts::content_id("f1_objective_return_to_shen_yan"),
}};

inline constexpr std::array<contracts::VerticalSliceBeatDefinition, 7> f1_beats{{
    {contracts::content_id("f1_beat_rain_ferry_arrival"), contracts::VerticalSliceBeatKind::exploration, 9, contracts::content_id("f1_cell_rain_ferry"), std::span<const contracts::ContentId>{beat_0_objectives}},
    {contracts::content_id("f1_beat_shen_yan_training"), contracts::VerticalSliceBeatKind::training, 8, contracts::content_id("f1_cell_rain_ferry"), std::span<const contracts::ContentId>{beat_1_objectives}},
    {contracts::content_id("f1_beat_umbrella_lane_first_encounter"), contracts::VerticalSliceBeatKind::combat, 12, contracts::content_id("f1_cell_umbrella_lane_a"), std::span<const contracts::ContentId>{beat_2_objectives}},
    {contracts::content_id("f1_beat_shared_workbench_investigation"), contracts::VerticalSliceBeatKind::investigation, 11, contracts::content_id("f1_cell_canopy_workstation"), std::span<const contracts::ContentId>{beat_3_objectives}},
    {contracts::content_id("f1_beat_canopy_return_encounter"), contracts::VerticalSliceBeatKind::combat, 7, contracts::content_id("f1_cell_canopy_workstation"), std::span<const contracts::ContentId>{beat_4_objectives}},
    {contracts::content_id("f1_beat_four_seasons_wraith"), contracts::VerticalSliceBeatKind::boss, 10, contracts::content_id("f1_cell_four_seasons_court"), std::span<const contracts::ContentId>{beat_5_objectives}},
    {contracts::content_id("f1_beat_resolution_and_return"), contracts::VerticalSliceBeatKind::resolution, 3, contracts::content_id("f1_cell_return_safe_point"), std::span<const contracts::ContentId>{beat_6_objectives}},
}};

inline constexpr std::array<contracts::ContentId, 2> f1_subregions{{
    contracts::content_id("jn_rain_ferry"),
    contracts::content_id("jn_umbrella_lane"),
}};

inline constexpr std::array<contracts::ContentId, 1> f1_npcs{{
    contracts::content_id("npc_shen_yan"),
}};

inline constexpr std::array<contracts::ContentId, 2> f1_enemy_families{{
    contracts::content_id("jn_enemy_leaking_umbrella_doll"),
    contracts::content_id("jn_enemy_faded_paper_egret"),
}};

inline constexpr std::array<contracts::ContentId, 5> f1_cells{{
    contracts::content_id("f1_cell_rain_ferry"),
    contracts::content_id("f1_cell_umbrella_lane_a"),
    contracts::content_id("f1_cell_canopy_workstation"),
    contracts::content_id("f1_cell_four_seasons_court"),
    contracts::content_id("f1_cell_return_safe_point"),
}};

inline constexpr std::array<contracts::CombatActorConfig, 4> f1_combat_actors{{
    {1ULL, contracts::content_id("actor_f1_player"), contracts::CombatFaction::player, {-12000, -1600, 0, 0}, {120, 120, 100, 100, 80, 80, 30, 30, 0}, {contracts::stable_content_key("stance_eavesguard"), contracts::stable_content_key("stance_flower_turn"), 0}, 2U, contracts::stable_content_key("stance_eavesguard")},
    {101ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-4000, -2600, 0, 0}, {90, 90, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust")},
    {102ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-3000, -400, 0, 0}, {90, 90, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust")},
    {103ULL, contracts::content_id("jn_enemy_faded_paper_egret"), contracts::CombatFaction::hostile, {-1500, 900, 700, 0}, {70, 70, 100, 100, 28, 28, 0, 0, 0}, {contracts::stable_content_key("stance_paper_egret"), 0, 0}, 1U, contracts::stable_content_key("stance_paper_egret")},
}};

inline constexpr std::array<contracts::AbilityDefinition, 9> f1_combat_abilities{{
    {contracts::content_id("ability_eavesguard_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_eavesguard"), 8, 8, 3, 12, 1800, 700, 18, 18, contracts::feedback_light},
    {contracts::content_id("ability_eavesguard_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_eavesguard"), 18, 18, 4, 24, 2050, 800, 30, 46, contracts::feedback_heavy},
    {contracts::content_id("ability_flower_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_flower_turn"), 10, 6, 4, 14, 2100, 900, 20, 14, contracts::feedback_light},
    {contracts::content_id("ability_flower_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_flower_turn"), 22, 14, 6, 28, 2350, 1000, 38, 34, contracts::feedback_heavy},
    {contracts::content_id("ability_umbrella_rust_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_umbrella_rust"), 6, 18, 4, 24, 1650, 700, 12, 16, contracts::feedback_light},
    {contracts::content_id("ability_umbrella_rust_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_umbrella_rust"), 16, 32, 6, 40, 1900, 800, 24, 36, contracts::feedback_heavy},
    {contracts::content_id("ability_paper_egret_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_paper_egret"), 8, 22, 6, 26, 2700, 1300, 14, 12, contracts::feedback_light},
    {contracts::content_id("ability_paper_egret_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_paper_egret"), 20, 38, 8, 42, 3200, 1500, 26, 30, contracts::feedback_heavy},
    {contracts::content_id("ability_common_evade"), contracts::CombatCommandType::evade, 0, 14, 0, 12, 22, 0, 0, 0, 0, contracts::feedback_evade},
}};

inline constexpr contracts::CombatEncounterDefinition f1_combat_encounter_definition{
    contracts::content_id("f1_encounter_umbrella_lane_bootstrap"),
    std::span<const contracts::CombatActorConfig>{f1_combat_actors},
    std::span<const contracts::AbilityDefinition>{f1_combat_abilities},
};

inline constexpr contracts::VerticalSliceDefinition f1_vertical_slice_definition{
    contracts::content_id("f1_rainy_umbrella_trial"),
    "2.5d-oblique-panoramic",
    "douzhanshen",
    "warm-snow-combat-readability",
    "author-controlled-oblique",
    60,
    70,
    contracts::content_id("fixture_f1_rainy_umbrella_start"),
    contracts::content_id("jn_chapter_02"),
    contracts::content_id("jn_boss_umbrella_wraith"),
    {
        1ULL,
        {-12000, -1600, 0, 0},
        3600,
        4800,
        9600,
        260,
        1800,
        {
            {23170, -23170},
            {23170, 23170},
            2U,
        },
    },
    std::span<const contracts::ContentId>{f1_subregions},
    std::span<const contracts::ContentId>{f1_npcs},
    std::span<const contracts::ContentId>{f1_enemy_families},
    std::span<const contracts::ContentId>{f1_cells},
    std::span<const contracts::VerticalSliceBeatDefinition>{f1_beats},
};

}  // namespace tgd::content::generated
