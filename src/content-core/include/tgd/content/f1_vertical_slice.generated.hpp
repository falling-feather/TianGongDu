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

inline constexpr std::array<contracts::ContentId, 2> interaction_3_prerequisites{{
    contracts::content_id("f1_objective_defeat_leaking_dolls"),
    contracts::content_id("f1_objective_answer_paper_egret"),
}};

inline constexpr std::array<contracts::ContentId, 3> interaction_7_prerequisites{{
    contracts::content_id("f1_objective_reveal_spring_trace"),
    contracts::content_id("f1_objective_reveal_winter_trace"),
    contracts::content_id("f1_objective_review_shared_ledger"),
}};

inline constexpr std::array<contracts::ContentId, 3> interaction_8_prerequisites{{
    contracts::content_id("f1_objective_reveal_spring_trace"),
    contracts::content_id("f1_objective_reveal_winter_trace"),
    contracts::content_id("f1_objective_review_shared_ledger"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_9_prerequisites{{
    contracts::content_id("f1_objective_validate_calibration"),
}};

inline constexpr std::array<contracts::StableActorKey, 3> encounter_activation_0_actors{{
    101ULL,
    102ULL,
    103ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_1_actors{{
    201ULL,
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

inline constexpr std::array<contracts::QuestInteractionDefinition, 10> f1_quest_interactions{{
    {contracts::content_id("f1_interaction_travel_writ"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_inspect_travel_writ"), contracts::ContentId{}, {-12000, -1600, 0, 0}, 800, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_ferry_gate"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_reach_ferry_gate"), contracts::ContentId{}, {-10450, -100, 0, 0}, 900, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_meet_shen_yan"), contracts::QuestInteractionKind::talk, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_meet_shen_yan"), contracts::ContentId{}, {-10500, -600, 0, 0}, 800, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_choose_lane_route"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_umbrella_lane_a"), contracts::content_id("f1_objective_choose_lane_route"), contracts::content_id("f1_choice_lane_canopy"), {-3900, -100, 0, 0}, 1200, std::span<const contracts::ContentId>{interaction_3_prerequisites}},
    {contracts::content_id("f1_interaction_reveal_spring_trace"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_reveal_spring_trace"), contracts::ContentId{}, {-3900, -100, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_reveal_winter_trace"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_reveal_winter_trace"), contracts::ContentId{}, {-3100, -100, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_review_shared_ledger"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_review_shared_ledger"), contracts::ContentId{}, {-2300, -100, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_calibrate_rib_spring"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_spring_calibration"), {-1500, 400, 0, 0}, 500, std::span<const contracts::ContentId>{interaction_7_prerequisites}},
    {contracts::content_id("f1_interaction_calibrate_rib_winter"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_winter_calibration"), {-1500, -600, 0, 0}, 500, std::span<const contracts::ContentId>{interaction_8_prerequisites}},
    {contracts::content_id("f1_interaction_open_return_shortcut"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_open_return_shortcut"), contracts::ContentId{}, {-800, 400, 0, 0}, 1400, std::span<const contracts::ContentId>{interaction_9_prerequisites}},
}};

inline constexpr std::array<contracts::QuestCombatTriggerDefinition, 2> f1_quest_combat_triggers{{
    {contracts::content_id("f1_trigger_eavesguard_counter"), contracts::QuestCombatTriggerKind::player_hit_guarded, contracts::content_id("f1_objective_eavesguard_counter"), contracts::stable_content_key("stance_eavesguard")},
    {contracts::content_id("f1_trigger_flower_turn_counter"), contracts::QuestCombatTriggerKind::player_hit_evaded, contracts::content_id("f1_objective_flower_turn_counter"), contracts::stable_content_key("stance_flower_turn")},
}};

inline constexpr std::array<contracts::QuestCombatOutcomeDefinition, 3> f1_quest_combat_outcomes{{
    {contracts::content_id("f1_outcome_defeat_leaking_dolls"), contracts::QuestCombatOutcomeKind::hostile_archetype_defeated, contracts::content_id("f1_objective_defeat_leaking_dolls"), contracts::content_id("jn_enemy_leaking_umbrella_doll"), 2U},
    {contracts::content_id("f1_outcome_answer_paper_egret"), contracts::QuestCombatOutcomeKind::hostile_archetype_defeated, contracts::content_id("f1_objective_answer_paper_egret"), contracts::content_id("jn_enemy_faded_paper_egret"), 1U},
    {contracts::content_id("f1_outcome_validate_return_calibration"), contracts::QuestCombatOutcomeKind::all_hostiles_defeated, contracts::content_id("f1_objective_validate_calibration"), contracts::ContentId{}, 0U},
}};

inline constexpr std::array<contracts::QuestEncounterActivationDefinition, 2> f1_quest_encounter_activations{{
    {contracts::content_id("f1_activation_canopy_return_encounter"), contracts::content_id("f1_beat_canopy_return_encounter"), contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_0_actors}},
    {contracts::content_id("f1_activation_four_seasons_wraith"), contracts::content_id("f1_beat_four_seasons_wraith"), contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_1_actors}},
}};

inline constexpr std::array<contracts::QuestBossPhaseDefinition, 4> f1_quest_boss_phases{{
    {contracts::content_id("f1_boss_phase_spring"), contracts::content_id("f1_objective_survive_spring_phase"), 201ULL, 75U, contracts::stable_content_key("stance_wraith_summer")},
    {contracts::content_id("f1_boss_phase_summer"), contracts::content_id("f1_objective_survive_summer_phase"), 201ULL, 50U, contracts::stable_content_key("stance_wraith_autumn")},
    {contracts::content_id("f1_boss_phase_autumn"), contracts::content_id("f1_objective_survive_autumn_phase"), 201ULL, 25U, contracts::stable_content_key("stance_wraith_winter")},
    {contracts::content_id("f1_boss_phase_winter"), contracts::content_id("f1_objective_survive_winter_phase"), 201ULL, 0U, 0},
}};

inline constexpr std::array<contracts::CombatActorConfig, 5> f1_combat_actors{{
    {1ULL, contracts::content_id("actor_f1_player"), contracts::CombatFaction::player, {-12000, -1600, 0, 0}, {120, 120, 100, 100, 80, 80, 30, 30, 0}, {contracts::stable_content_key("stance_eavesguard"), contracts::stable_content_key("stance_flower_turn"), 0, 0}, 2U, contracts::stable_content_key("stance_eavesguard"), {30, 6, 2, 120, 12, 4}, true},
    {101ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-4000, -2600, 0, 0}, {90, 90, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, true},
    {102ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-3000, -400, 0, 0}, {90, 90, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, true},
    {103ULL, contracts::content_id("jn_enemy_faded_paper_egret"), contracts::CombatFaction::hostile, {-1500, 900, 700, 0}, {70, 70, 100, 100, 28, 28, 0, 0, 0}, {contracts::stable_content_key("stance_paper_egret"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_paper_egret"), {36, 8, 2, 120, 12, 3}, true},
    {201ULL, contracts::content_id("jn_boss_umbrella_wraith"), contracts::CombatFaction::hostile, {4000, 1900, 0, 0}, {420, 420, 240, 240, 120, 120, 0, 0, 0}, {contracts::stable_content_key("stance_wraith_spring"), contracts::stable_content_key("stance_wraith_summer"), contracts::stable_content_key("stance_wraith_autumn"), contracts::stable_content_key("stance_wraith_winter")}, 4U, contracts::stable_content_key("stance_wraith_spring"), {24, 6, 4, 90, 10, 5}, false},
}};

inline constexpr std::array<contracts::AbilityDefinition, 17> f1_combat_abilities{{
    {contracts::content_id("ability_eavesguard_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_eavesguard"), 8, 12, 3, 12, 1800, 700, 18, 18, contracts::feedback_light},
    {contracts::content_id("ability_eavesguard_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_eavesguard"), 18, 18, 4, 24, 2050, 800, 30, 46, contracts::feedback_heavy},
    {contracts::content_id("ability_flower_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_flower_turn"), 10, 6, 4, 8, 2100, 900, 30, 14, contracts::feedback_light},
    {contracts::content_id("ability_flower_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_flower_turn"), 22, 14, 6, 28, 2350, 1000, 38, 34, contracts::feedback_heavy},
    {contracts::content_id("ability_umbrella_rust_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_umbrella_rust"), 6, 18, 4, 24, 1650, 700, 12, 16, contracts::feedback_light},
    {contracts::content_id("ability_umbrella_rust_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_umbrella_rust"), 16, 32, 6, 40, 1900, 800, 24, 36, contracts::feedback_heavy},
    {contracts::content_id("ability_paper_egret_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_paper_egret"), 8, 22, 6, 26, 2700, 1300, 14, 12, contracts::feedback_light},
    {contracts::content_id("ability_paper_egret_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_paper_egret"), 20, 38, 8, 42, 3200, 1500, 26, 30, contracts::feedback_heavy},
    {contracts::content_id("ability_wraith_spring_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_wraith_spring"), 8, 24, 5, 24, 2500, 1100, 12, 18, contracts::feedback_light},
    {contracts::content_id("ability_wraith_spring_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_wraith_spring"), 20, 42, 8, 38, 3100, 1300, 22, 34, contracts::feedback_heavy},
    {contracts::content_id("ability_wraith_summer_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_wraith_summer"), 10, 16, 6, 20, 2700, 1200, 14, 20, contracts::feedback_light},
    {contracts::content_id("ability_wraith_summer_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_wraith_summer"), 22, 32, 9, 34, 3300, 1400, 24, 38, contracts::feedback_heavy},
    {contracts::content_id("ability_wraith_autumn_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_wraith_autumn"), 11, 20, 6, 18, 2850, 1200, 16, 24, contracts::feedback_light},
    {contracts::content_id("ability_wraith_autumn_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_wraith_autumn"), 24, 36, 10, 32, 3500, 1500, 28, 44, contracts::feedback_heavy},
    {contracts::content_id("ability_wraith_winter_light"), contracts::CombatCommandType::light_attack, contracts::stable_content_key("stance_wraith_winter"), 12, 14, 7, 17, 3000, 1300, 18, 28, contracts::feedback_light},
    {contracts::content_id("ability_wraith_winter_heavy"), contracts::CombatCommandType::heavy_attack, contracts::stable_content_key("stance_wraith_winter"), 26, 28, 11, 30, 3700, 1600, 32, 50, contracts::feedback_heavy},
    {contracts::content_id("ability_common_evade"), contracts::CombatCommandType::evade, 0, 14, 0, 12, 22, 0, 0, 0, 0, contracts::feedback_evade},
}};

inline constexpr contracts::CombatEncounterDefinition f1_combat_encounter_definition{
    contracts::content_id("f1_encounter_umbrella_lane_bootstrap"),
    std::span<const contracts::CombatActorConfig>{f1_combat_actors},
    std::span<const contracts::AbilityDefinition>{f1_combat_abilities},
    {1ULL, 5500, 9000, 1800, 1500, 6, 30, 1U},
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
    std::span<const contracts::QuestInteractionDefinition>{f1_quest_interactions},
    std::span<const contracts::QuestCombatTriggerDefinition>{f1_quest_combat_triggers},
    std::span<const contracts::QuestCombatOutcomeDefinition>{f1_quest_combat_outcomes},
    std::span<const contracts::QuestEncounterActivationDefinition>{f1_quest_encounter_activations},
    std::span<const contracts::QuestBossPhaseDefinition>{f1_quest_boss_phases},
};

}  // namespace tgd::content::generated
