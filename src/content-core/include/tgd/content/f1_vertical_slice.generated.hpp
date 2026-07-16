// Generated from content/design/f1-vertical-slice.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/combat_types.hpp>
#include <tgd/contracts/content_definition.hpp>

#include <array>
#include <span>

namespace tgd::content::generated {

inline constexpr std::array<contracts::ContentId, 11> beat_0_objectives{{
    contracts::content_id("f1_objective_inspect_travel_writ"),
    contracts::content_id("f1_objective_choose_arrival_clue"),
    contracts::content_id("f1_objective_read_ferry_condition"),
    contracts::content_id("f1_objective_choose_mooring_method"),
    contracts::content_id("f1_objective_secure_ferry_mooring"),
    contracts::content_id("f1_objective_inspect_bilge_counterweight"),
    contracts::content_id("f1_objective_release_ferry_bilge"),
    contracts::content_id("f1_objective_raise_wayfinding_lantern"),
    contracts::content_id("f1_objective_read_workshop_bell_code"),
    contracts::content_id("f1_objective_sound_workshop_bell"),
    contracts::content_id("f1_objective_reach_ferry_gate"),
}};

inline constexpr std::array<contracts::ContentId, 14> beat_1_objectives{{
    contracts::content_id("f1_objective_meet_shen_yan"),
    contracts::content_id("f1_objective_choose_training_lane"),
    contracts::content_id("f1_objective_take_eavesguard_mark"),
    contracts::content_id("f1_objective_eavesguard_counter"),
    contracts::content_id("f1_objective_commit_eavesguard_heavy"),
    contracts::content_id("f1_objective_break_eavesguard_target"),
    contracts::content_id("f1_objective_review_eavesguard_with_shen_yan"),
    contracts::content_id("f1_objective_enter_flower_turn"),
    contracts::content_id("f1_objective_cross_flower_turn_line"),
    contracts::content_id("f1_objective_flower_turn_counter"),
    contracts::content_id("f1_objective_commit_flower_turn_light"),
    contracts::content_id("f1_objective_commit_flower_turn_heavy"),
    contracts::content_id("f1_objective_break_flower_turn_target"),
    contracts::content_id("f1_objective_finish_shen_yan_training"),
}};

inline constexpr std::array<contracts::ContentId, 6> beat_2_objectives{{
    contracts::content_id("f1_objective_defeat_leaking_dolls"),
    contracts::content_id("f1_objective_inspect_torn_canopy_seam"),
    contracts::content_id("f1_objective_release_flooded_gutter"),
    contracts::content_id("f1_objective_raise_paper_egret_lure"),
    contracts::content_id("f1_objective_answer_paper_egret"),
    contracts::content_id("f1_objective_choose_lane_route"),
}};

inline constexpr std::array<contracts::ContentId, 4> beat_3_objectives{{
    contracts::content_id("f1_objective_reveal_spring_trace"),
    contracts::content_id("f1_objective_reveal_winter_trace"),
    contracts::content_id("f1_objective_review_shared_ledger"),
    contracts::content_id("f1_objective_choose_rib_calibration"),
}};

inline constexpr std::array<contracts::ContentId, 4> beat_4_objectives{{
    contracts::content_id("f1_objective_prime_return_calibration"),
    contracts::content_id("f1_objective_demonstrate_rib_calibration"),
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

inline constexpr std::array<contracts::ContentId, 1> interaction_1_prerequisites{{
    contracts::content_id("f1_objective_inspect_travel_writ"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_2_prerequisites{{
    contracts::content_id("f1_objective_inspect_travel_writ"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_3_prerequisites{{
    contracts::content_id("f1_objective_inspect_travel_writ"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_4_prerequisites{{
    contracts::content_id("f1_objective_choose_arrival_clue"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_5_prerequisites{{
    contracts::content_id("f1_objective_choose_arrival_clue"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_6_prerequisites{{
    contracts::content_id("f1_objective_choose_arrival_clue"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_7_prerequisites{{
    contracts::content_id("f1_objective_read_ferry_condition"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_8_prerequisites{{
    contracts::content_id("f1_objective_read_ferry_condition"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_9_prerequisites{{
    contracts::content_id("f1_objective_choose_mooring_method"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_10_prerequisites{{
    contracts::content_id("f1_objective_choose_mooring_method"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_11_prerequisites{{
    contracts::content_id("f1_objective_secure_ferry_mooring"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_12_prerequisites{{
    contracts::content_id("f1_objective_inspect_bilge_counterweight"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_13_prerequisites{{
    contracts::content_id("f1_objective_release_ferry_bilge"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_14_prerequisites{{
    contracts::content_id("f1_objective_raise_wayfinding_lantern"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_15_prerequisites{{
    contracts::content_id("f1_objective_read_workshop_bell_code"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_16_prerequisites{{
    contracts::content_id("f1_objective_sound_workshop_bell"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_18_prerequisites{{
    contracts::content_id("f1_objective_meet_shen_yan"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_19_prerequisites{{
    contracts::content_id("f1_objective_meet_shen_yan"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_20_prerequisites{{
    contracts::content_id("f1_objective_choose_training_lane"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_21_prerequisites{{
    contracts::content_id("f1_objective_choose_training_lane"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_22_prerequisites{{
    contracts::content_id("f1_objective_break_eavesguard_target"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_23_prerequisites{{
    contracts::content_id("f1_objective_enter_flower_turn"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_24_prerequisites{{
    contracts::content_id("f1_objective_break_flower_turn_target"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_25_prerequisites{{
    contracts::content_id("f1_objective_defeat_leaking_dolls"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_26_prerequisites{{
    contracts::content_id("f1_objective_inspect_torn_canopy_seam"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_27_prerequisites{{
    contracts::content_id("f1_objective_release_flooded_gutter"),
}};

inline constexpr std::array<contracts::ContentId, 5> interaction_28_prerequisites{{
    contracts::content_id("f1_objective_defeat_leaking_dolls"),
    contracts::content_id("f1_objective_inspect_torn_canopy_seam"),
    contracts::content_id("f1_objective_release_flooded_gutter"),
    contracts::content_id("f1_objective_raise_paper_egret_lure"),
    contracts::content_id("f1_objective_answer_paper_egret"),
}};

inline constexpr std::array<contracts::ContentId, 5> interaction_29_prerequisites{{
    contracts::content_id("f1_objective_defeat_leaking_dolls"),
    contracts::content_id("f1_objective_inspect_torn_canopy_seam"),
    contracts::content_id("f1_objective_release_flooded_gutter"),
    contracts::content_id("f1_objective_raise_paper_egret_lure"),
    contracts::content_id("f1_objective_answer_paper_egret"),
}};

inline constexpr std::array<contracts::ContentId, 3> interaction_34_prerequisites{{
    contracts::content_id("f1_objective_reveal_spring_trace"),
    contracts::content_id("f1_objective_reveal_winter_trace"),
    contracts::content_id("f1_objective_review_shared_ledger"),
}};

inline constexpr std::array<contracts::ContentId, 3> interaction_35_prerequisites{{
    contracts::content_id("f1_objective_reveal_spring_trace"),
    contracts::content_id("f1_objective_reveal_winter_trace"),
    contracts::content_id("f1_objective_review_shared_ledger"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_37_prerequisites{{
    contracts::content_id("f1_objective_validate_calibration"),
}};

inline constexpr std::array<contracts::ContentId, 1> interaction_40_prerequisites{{
    contracts::content_id("f1_objective_choose_resolution"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_0_prerequisites{{
    contracts::content_id("f1_objective_take_eavesguard_mark"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_1_prerequisites{{
    contracts::content_id("f1_objective_eavesguard_counter"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_2_prerequisites{{
    contracts::content_id("f1_objective_review_eavesguard_with_shen_yan"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_3_prerequisites{{
    contracts::content_id("f1_objective_cross_flower_turn_line"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_4_prerequisites{{
    contracts::content_id("f1_objective_flower_turn_counter"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_5_prerequisites{{
    contracts::content_id("f1_objective_commit_flower_turn_light"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_6_prerequisites{{
    contracts::content_id("f1_objective_prime_return_calibration"),
}};

inline constexpr std::array<contracts::ContentId, 1> combat_trigger_7_prerequisites{{
    contracts::content_id("f1_objective_prime_return_calibration"),
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_0_actors{{
    104ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_1_actors{{
    104ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_2_actors{{
    107ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_3_actors{{
    107ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_4_actors{{
    108ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_5_actors{{
    109ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 2> encounter_activation_6_actors{{
    101ULL,
    102ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_7_actors{{
    103ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 3> encounter_activation_8_actors{{
    101ULL,
    102ULL,
    103ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_9_actors{{
    106ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_10_actors{{
    105ULL,
}};

inline constexpr std::array<contracts::StableActorKey, 1> encounter_activation_11_actors{{
    201ULL,
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_0_placements{{
    {104ULL, {-4100, 2300, 0, 0}, 0U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_1_placements{{
    {104ULL, {-3900, -2500, 0, 0}, 0U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_2_placements{{
    {107ULL, {-4000, 2000, 0, 0}, 0U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_3_placements{{
    {107ULL, {-3800, -2100, 0, 0}, 0U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_4_placements{{
    {108ULL, {-3300, 1200, 700, 0}, 2U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_5_placements{{
    {109ULL, {-3000, -800, 700, 0}, 2U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 2> encounter_activation_6_placements{{
    {101ULL, {-4000, -2600, 0, 0}, 1U},
    {102ULL, {-3000, -400, 0, 0}, 5U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_7_placements{{
    {103ULL, {-1500, 900, 700, 0}, 2U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 3> encounter_activation_8_placements{{
    {101ULL, {-2500, -1800, 0, 0}, 0U},
    {102ULL, {-900, -300, 0, 0}, 3U},
    {103ULL, {-500, 1700, 700, 0}, 6U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_9_placements{{
    {106ULL, {500, 1400, 0, 0}, 5U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_10_placements{{
    {105ULL, {500, 1400, 700, 0}, 5U},
}};

inline constexpr std::array<contracts::EncounterActorPlacementDefinition, 1> encounter_activation_11_placements{{
    {201ULL, {4000, 1900, 0, 0}, 4U},
}};

inline constexpr std::array<contracts::ContentId, 1> quest_ui_cue_0_objectives{{
    contracts::content_id("f1_objective_choose_arrival_clue"),
}};

inline constexpr std::array<contracts::ContentId, 1> quest_ui_cue_1_objectives{{
    contracts::content_id("f1_objective_choose_mooring_method"),
}};

inline constexpr std::array<contracts::ContentId, 1> quest_ui_cue_2_objectives{{
    contracts::content_id("f1_objective_secure_ferry_mooring"),
}};

inline constexpr std::array<contracts::ContentId, 1> quest_ui_cue_3_objectives{{
    contracts::content_id("f1_objective_sound_workshop_bell"),
}};

inline constexpr std::array<contracts::ContentId, 1> quest_ui_cue_4_objectives{{
    contracts::content_id("f1_objective_choose_training_lane"),
}};

inline constexpr std::array<contracts::ContentId, 2> quest_ui_cue_5_objectives{{
    contracts::content_id("f1_objective_eavesguard_counter"),
    contracts::content_id("f1_objective_flower_turn_counter"),
}};

inline constexpr std::array<contracts::ContentId, 8> quest_ui_cue_6_objectives{{
    contracts::content_id("f1_objective_eavesguard_counter"),
    contracts::content_id("f1_objective_commit_eavesguard_heavy"),
    contracts::content_id("f1_objective_break_eavesguard_target"),
    contracts::content_id("f1_objective_enter_flower_turn"),
    contracts::content_id("f1_objective_flower_turn_counter"),
    contracts::content_id("f1_objective_commit_flower_turn_light"),
    contracts::content_id("f1_objective_commit_flower_turn_heavy"),
    contracts::content_id("f1_objective_break_flower_turn_target"),
}};

inline constexpr std::array<contracts::ContentId, 2> quest_ui_cue_7_objectives{{
    contracts::content_id("f1_objective_eavesguard_counter"),
    contracts::content_id("f1_objective_flower_turn_counter"),
}};

inline constexpr std::array<contracts::QuestUiResultSelectorDefinition, 1> quest_ui_cue_2_selectors{{
    {contracts::QuestUiProjectionSource::interaction_feedback, contracts::content_id("f1_objective_secure_ferry_mooring"), contracts::content_id("f1_interaction_choose_quick_hitch"), contracts::ContentId{}, contracts::QuestUiPolarityOverride::negative},
}};

inline constexpr std::array<contracts::QuestUiResultSelectorDefinition, 2> quest_ui_cue_6_selectors{{
    {contracts::QuestUiProjectionSource::combat_feedback, contracts::content_id("f1_objective_break_eavesguard_target"), contracts::content_id("f1_trigger_eavesguard_heavy"), contracts::content_id("f1_outcome_break_eavesguard_target"), contracts::QuestUiPolarityOverride::none},
    {contracts::QuestUiProjectionSource::combat_feedback, contracts::content_id("f1_objective_break_flower_turn_target"), contracts::content_id("f1_trigger_flower_turn_heavy"), contracts::content_id("f1_outcome_break_flower_turn_target"), contracts::QuestUiPolarityOverride::none},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> quest_ui_cue_0_attempt_evidence{{
    {contracts::QuestUiProjectionSource::choice_available, contracts::content_id("f1_objective_choose_arrival_clue"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_first_visit},
    {contracts::QuestUiProjectionSource::interaction_feedback, contracts::content_id("f1_objective_choose_arrival_clue"), {contracts::content_id("f1_interaction_arrival_clue_drowned_manifest"), contracts::QuestUiResultStatus::ignored_repeat, contracts::QuestUiRejectionReason::selection_already_committed}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::repeat_no_progress},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 1> quest_ui_cue_1_attempt_evidence{{
    {contracts::QuestUiProjectionSource::choice_available, contracts::content_id("f1_objective_choose_mooring_method"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_craft_decision},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> quest_ui_cue_2_attempt_evidence{{
    {contracts::QuestUiProjectionSource::interaction_feedback, contracts::content_id("f1_objective_secure_ferry_mooring"), {contracts::content_id("f1_interaction_lock_cross_belay"), contracts::QuestUiResultStatus::accepted, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_craft_decision},
    {contracts::QuestUiProjectionSource::interaction_feedback, contracts::content_id("f1_objective_secure_ferry_mooring"), {contracts::content_id("f1_interaction_choose_quick_hitch"), contracts::QuestUiResultStatus::accepted, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_error_feedback},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> quest_ui_cue_3_attempt_evidence{{
    {contracts::QuestUiProjectionSource::interaction_feedback, contracts::content_id("f1_objective_sound_workshop_bell"), {contracts::content_id("f1_interaction_sound_workshop_bell"), contracts::QuestUiResultStatus::rejected, contracts::QuestUiRejectionReason::prerequisite_incomplete}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_wrong_order_feedback},
    {contracts::QuestUiProjectionSource::interaction_feedback, contracts::content_id("f1_objective_sound_workshop_bell"), {contracts::content_id("f1_interaction_sound_workshop_bell"), contracts::QuestUiResultStatus::accepted, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_craft_confirmation},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 1> quest_ui_cue_4_attempt_evidence{{
    {contracts::QuestUiProjectionSource::choice_available, contracts::content_id("f1_objective_choose_training_lane"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_dialogue_decision},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> quest_ui_cue_5_attempt_evidence{{
    {contracts::QuestUiProjectionSource::objective_state, contracts::content_id("f1_objective_eavesguard_counter"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_training_risk},
    {contracts::QuestUiProjectionSource::objective_state, contracts::content_id("f1_objective_flower_turn_counter"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_training_risk},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> quest_ui_cue_6_attempt_evidence{{
    {contracts::QuestUiProjectionSource::combat_feedback, contracts::content_id("f1_objective_eavesguard_counter"), {contracts::content_id("f1_trigger_eavesguard_counter"), contracts::QuestUiResultStatus::accepted, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::qualifying_combat_proof},
    {contracts::QuestUiProjectionSource::combat_feedback, contracts::content_id("f1_objective_break_flower_turn_target"), {contracts::content_id("f1_trigger_flower_turn_heavy"), contracts::QuestUiResultStatus::accepted, contracts::QuestUiRejectionReason::none}, {contracts::content_id("f1_outcome_break_flower_turn_target"), contracts::QuestUiResultStatus::rejected, contracts::QuestUiRejectionReason::wrong_target}, contracts::QuestUiAttemptTimeClassification::qualifying_combat_feedback},
}};

inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 4> quest_ui_cue_7_attempt_evidence{{
    {contracts::QuestUiProjectionSource::recovery_offer, contracts::content_id("f1_objective_eavesguard_counter"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::failure_retry_excluded},
    {contracts::QuestUiProjectionSource::recovery_resume, contracts::content_id("f1_objective_eavesguard_counter"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::resume_no_duplicate_progress},
    {contracts::QuestUiProjectionSource::recovery_offer, contracts::content_id("f1_objective_flower_turn_counter"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::failure_retry_excluded},
    {contracts::QuestUiProjectionSource::recovery_resume, contracts::content_id("f1_objective_flower_turn_counter"), {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, {contracts::ContentId{}, contracts::QuestUiResultStatus::not_applicable, contracts::QuestUiRejectionReason::none}, contracts::QuestUiAttemptTimeClassification::resume_no_duplicate_progress},
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

inline constexpr std::array<contracts::VerticalSliceSafePointDefinition, 7> f1_safe_points{{
    {contracts::content_id("f1_safe_point_rain_ferry_arrival"), contracts::content_id("f1_beat_rain_ferry_arrival"), {-12000, -1600, 0, 0}},
    {contracts::content_id("f1_safe_point_shen_yan_training"), contracts::content_id("f1_beat_shen_yan_training"), {-6500, -500, 0, 0}},
    {contracts::content_id("f1_safe_point_umbrella_lane"), contracts::content_id("f1_beat_umbrella_lane_first_encounter"), {-5600, -1200, 0, 0}},
    {contracts::content_id("f1_safe_point_shared_workbench"), contracts::content_id("f1_beat_shared_workbench_investigation"), {-4300, -100, 0, 0}},
    {contracts::content_id("f1_safe_point_canopy_return"), contracts::content_id("f1_beat_canopy_return_encounter"), {-4300, -100, 0, 0}},
    {contracts::content_id("f1_safe_point_four_seasons_court"), contracts::content_id("f1_beat_four_seasons_wraith"), {2200, 800, 0, 0}},
    {contracts::content_id("f1_safe_point_resolution_return"), contracts::content_id("f1_beat_resolution_and_return"), {3000, 800, 0, 0}},
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

inline constexpr std::array<contracts::QuestInteractionDefinition, 41> f1_quest_interactions{{
    {contracts::content_id("f1_interaction_travel_writ"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_inspect_travel_writ"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-12000, -1600, 0, 0}, 800, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_arrival_clue_high_water_tags"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_arrival_clue"), contracts::content_id("f1_choice_arrival_high_water_tags"), contracts::ContentId{}, contracts::ContentId{}, {-12200, 900, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_1_prerequisites}},
    {contracts::content_id("f1_interaction_arrival_clue_drowned_manifest"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_arrival_clue"), contracts::content_id("f1_choice_arrival_drowned_manifest"), contracts::ContentId{}, contracts::ContentId{}, {-11400, -3100, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_2_prerequisites}},
    {contracts::content_id("f1_interaction_arrival_clue_follow_bell"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_arrival_clue"), contracts::content_id("f1_choice_arrival_follow_bell"), contracts::ContentId{}, contracts::ContentId{}, {-10900, -1000, 0, 0}, 700, std::span<const contracts::ContentId>{interaction_3_prerequisites}},
    {contracts::content_id("f1_interaction_read_high_water_repairs"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_read_ferry_condition"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_arrival_clue"), contracts::content_id("f1_choice_arrival_high_water_tags"), {-11100, 1700, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_4_prerequisites}},
    {contracts::content_id("f1_interaction_read_manifest_waterline"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_read_ferry_condition"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_arrival_clue"), contracts::content_id("f1_choice_arrival_drowned_manifest"), {-10600, -2800, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_5_prerequisites}},
    {contracts::content_id("f1_interaction_read_main_flood_gauge"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_read_ferry_condition"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_arrival_clue"), contracts::content_id("f1_choice_arrival_follow_bell"), {-10700, -900, 0, 0}, 650, std::span<const contracts::ContentId>{interaction_6_prerequisites}},
    {contracts::content_id("f1_interaction_choose_cross_belay"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_mooring_method"), contracts::content_id("f1_choice_mooring_cross_belay"), contracts::ContentId{}, contracts::ContentId{}, {-10100, 1200, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_7_prerequisites}},
    {contracts::content_id("f1_interaction_choose_quick_hitch"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_mooring_method"), contracts::content_id("f1_choice_mooring_quick_hitch"), contracts::ContentId{}, contracts::ContentId{}, {-10000, -1700, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_8_prerequisites}},
    {contracts::content_id("f1_interaction_lock_cross_belay"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_secure_ferry_mooring"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_mooring_method"), contracts::content_id("f1_choice_mooring_cross_belay"), {-9400, 1900, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_9_prerequisites}},
    {contracts::content_id("f1_interaction_correct_overloaded_quick_hitch"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_secure_ferry_mooring"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_mooring_method"), contracts::content_id("f1_choice_mooring_quick_hitch"), {-9200, -2300, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_10_prerequisites}},
    {contracts::content_id("f1_interaction_inspect_bilge_counterweight"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_inspect_bilge_counterweight"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-8700, -800, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_11_prerequisites}},
    {contracts::content_id("f1_interaction_release_ferry_bilge"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_release_ferry_bilge"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-8400, 1100, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_12_prerequisites}},
    {contracts::content_id("f1_interaction_raise_wayfinding_lantern"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_raise_wayfinding_lantern"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-7900, -300, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_13_prerequisites}},
    {contracts::content_id("f1_interaction_read_workshop_bell_code"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_read_workshop_bell_code"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-7500, 1300, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_14_prerequisites}},
    {contracts::content_id("f1_interaction_sound_workshop_bell"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_sound_workshop_bell"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-7100, 300, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_15_prerequisites}},
    {contracts::content_id("f1_interaction_ferry_gate"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_reach_ferry_gate"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-6700, -600, 0, 0}, 800, std::span<const contracts::ContentId>{interaction_16_prerequisites}},
    {contracts::content_id("f1_interaction_meet_shen_yan"), contracts::QuestInteractionKind::talk, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_meet_shen_yan"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-6500, -500, 0, 0}, 800, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_choose_training_windward_lane"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_windward_lane"), contracts::ContentId{}, contracts::ContentId{}, {-6100, 1600, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_18_prerequisites}},
    {contracts::content_id("f1_interaction_choose_training_leeward_lane"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_leeward_lane"), contracts::ContentId{}, contracts::ContentId{}, {-6000, -2200, 0, 0}, 600, std::span<const contracts::ContentId>{interaction_19_prerequisites}},
    {contracts::content_id("f1_interaction_take_windward_eavesguard_mark"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_take_eavesguard_mark"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_windward_lane"), {-5400, 1900, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_20_prerequisites}},
    {contracts::content_id("f1_interaction_take_leeward_eavesguard_mark"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_take_eavesguard_mark"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_leeward_lane"), {-5200, -2400, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_21_prerequisites}},
    {contracts::content_id("f1_interaction_review_eavesguard_with_shen_yan"), contracts::QuestInteractionKind::talk, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_review_eavesguard_with_shen_yan"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-5000, -200, 0, 0}, 800, std::span<const contracts::ContentId>{interaction_22_prerequisites}},
    {contracts::content_id("f1_interaction_cross_flower_turn_line"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_cross_flower_turn_line"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-3400, -1700, 0, 0}, 550, std::span<const contracts::ContentId>{interaction_23_prerequisites}},
    {contracts::content_id("f1_interaction_finish_shen_yan_training"), contracts::QuestInteractionKind::talk, contracts::content_id("f1_cell_rain_ferry"), contracts::content_id("f1_objective_finish_shen_yan_training"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-5000, -200, 0, 0}, 800, std::span<const contracts::ContentId>{interaction_24_prerequisites}},
    {contracts::content_id("f1_interaction_inspect_torn_canopy_seam"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_umbrella_lane_a"), contracts::content_id("f1_objective_inspect_torn_canopy_seam"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-3600, -1700, 0, 0}, 650, std::span<const contracts::ContentId>{interaction_25_prerequisites}},
    {contracts::content_id("f1_interaction_release_flooded_gutter"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_umbrella_lane_a"), contracts::content_id("f1_objective_release_flooded_gutter"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-2700, -700, 0, 0}, 650, std::span<const contracts::ContentId>{interaction_26_prerequisites}},
    {contracts::content_id("f1_interaction_raise_paper_egret_lure"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_umbrella_lane_a"), contracts::content_id("f1_objective_raise_paper_egret_lure"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-1800, 500, 0, 0}, 700, std::span<const contracts::ContentId>{interaction_27_prerequisites}},
    {contracts::content_id("f1_interaction_choose_lane_route"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_umbrella_lane_a"), contracts::content_id("f1_objective_choose_lane_route"), contracts::content_id("f1_choice_lane_canopy"), contracts::ContentId{}, contracts::ContentId{}, {-3900, -100, 0, 0}, 1200, std::span<const contracts::ContentId>{interaction_28_prerequisites}},
    {contracts::content_id("f1_interaction_choose_lane_drain_route"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_umbrella_lane_a"), contracts::content_id("f1_objective_choose_lane_route"), contracts::content_id("f1_choice_lane_drain"), contracts::ContentId{}, contracts::ContentId{}, {-800, 900, 0, 0}, 900, std::span<const contracts::ContentId>{interaction_29_prerequisites}},
    {contracts::content_id("f1_interaction_reveal_spring_trace"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_reveal_spring_trace"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_lane_route"), contracts::content_id("f1_choice_lane_canopy"), {-3900, -100, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_reveal_spring_trace_from_drain"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_reveal_spring_trace"), contracts::ContentId{}, contracts::content_id("f1_objective_choose_lane_route"), contracts::content_id("f1_choice_lane_drain"), {-2700, -1700, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_reveal_winter_trace"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_reveal_winter_trace"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-3100, -100, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_review_shared_ledger"), contracts::QuestInteractionKind::inspect, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_review_shared_ledger"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-2300, -100, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_calibrate_rib_spring"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_spring_calibration"), contracts::ContentId{}, contracts::ContentId{}, {-1500, 400, 0, 0}, 500, std::span<const contracts::ContentId>{interaction_34_prerequisites}},
    {contracts::content_id("f1_interaction_calibrate_rib_winter"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_winter_calibration"), contracts::ContentId{}, contracts::ContentId{}, {-1500, -600, 0, 0}, 500, std::span<const contracts::ContentId>{interaction_35_prerequisites}},
    {contracts::content_id("f1_interaction_prime_return_calibration"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_prime_return_calibration"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-3500, -900, 0, 0}, 650, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_open_return_shortcut"), contracts::QuestInteractionKind::operate, contracts::content_id("f1_cell_canopy_workstation"), contracts::content_id("f1_objective_open_return_shortcut"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-800, 400, 0, 0}, 1400, std::span<const contracts::ContentId>{interaction_37_prerequisites}},
    {contracts::content_id("f1_interaction_resolution_subdue"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_return_safe_point"), contracts::content_id("f1_objective_choose_resolution"), contracts::content_id("f1_choice_resolution_subdue"), contracts::ContentId{}, contracts::ContentId{}, {3300, 1200, 0, 0}, 500, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_resolution_restore_shared_mark"), contracts::QuestInteractionKind::choose, contracts::content_id("f1_cell_return_safe_point"), contracts::content_id("f1_objective_choose_resolution"), contracts::content_id("f1_choice_resolution_restore_shared_mark"), contracts::ContentId{}, contracts::ContentId{}, {4200, 2300, 0, 0}, 500, std::span<const contracts::ContentId>{}},
    {contracts::content_id("f1_interaction_return_to_shen_yan"), contracts::QuestInteractionKind::talk, contracts::content_id("f1_cell_return_safe_point"), contracts::content_id("f1_objective_return_to_shen_yan"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, {-10500, -600, 0, 0}, 1000, std::span<const contracts::ContentId>{interaction_40_prerequisites}},
}};

inline constexpr std::array<contracts::QuestCombatTriggerDefinition, 8> f1_quest_combat_triggers{{
    {contracts::content_id("f1_trigger_eavesguard_counter"), contracts::QuestCombatTriggerKind::player_hit_guarded, contracts::content_id("f1_objective_eavesguard_counter"), contracts::stable_content_key("stance_eavesguard"), 0, contracts::ContentId{}, contracts::ContentId{}, std::span<const contracts::ContentId>{combat_trigger_0_prerequisites}},
    {contracts::content_id("f1_trigger_eavesguard_heavy"), contracts::QuestCombatTriggerKind::player_ability_started, contracts::content_id("f1_objective_commit_eavesguard_heavy"), contracts::stable_content_key("stance_eavesguard"), contracts::stable_content_key("ability_eavesguard_heavy"), contracts::ContentId{}, contracts::ContentId{}, std::span<const contracts::ContentId>{combat_trigger_1_prerequisites}},
    {contracts::content_id("f1_trigger_enter_flower_turn"), contracts::QuestCombatTriggerKind::player_stance_changed, contracts::content_id("f1_objective_enter_flower_turn"), contracts::stable_content_key("stance_flower_turn"), 0, contracts::ContentId{}, contracts::ContentId{}, std::span<const contracts::ContentId>{combat_trigger_2_prerequisites}},
    {contracts::content_id("f1_trigger_flower_turn_counter"), contracts::QuestCombatTriggerKind::player_hit_evaded, contracts::content_id("f1_objective_flower_turn_counter"), contracts::stable_content_key("stance_flower_turn"), 0, contracts::ContentId{}, contracts::ContentId{}, std::span<const contracts::ContentId>{combat_trigger_3_prerequisites}},
    {contracts::content_id("f1_trigger_flower_turn_light"), contracts::QuestCombatTriggerKind::player_ability_started, contracts::content_id("f1_objective_commit_flower_turn_light"), contracts::stable_content_key("stance_flower_turn"), contracts::stable_content_key("ability_flower_light"), contracts::ContentId{}, contracts::ContentId{}, std::span<const contracts::ContentId>{combat_trigger_4_prerequisites}},
    {contracts::content_id("f1_trigger_flower_turn_heavy"), contracts::QuestCombatTriggerKind::player_ability_started, contracts::content_id("f1_objective_commit_flower_turn_heavy"), contracts::stable_content_key("stance_flower_turn"), contracts::stable_content_key("ability_flower_heavy"), contracts::ContentId{}, contracts::ContentId{}, std::span<const contracts::ContentId>{combat_trigger_5_prerequisites}},
    {contracts::content_id("f1_trigger_return_spring_calibration_heavy"), contracts::QuestCombatTriggerKind::player_ability_started, contracts::content_id("f1_objective_demonstrate_rib_calibration"), contracts::stable_content_key("stance_eavesguard"), contracts::stable_content_key("ability_eavesguard_heavy"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_spring_calibration"), std::span<const contracts::ContentId>{combat_trigger_6_prerequisites}},
    {contracts::content_id("f1_trigger_return_winter_calibration_light"), contracts::QuestCombatTriggerKind::player_ability_started, contracts::content_id("f1_objective_demonstrate_rib_calibration"), contracts::stable_content_key("stance_flower_turn"), contracts::stable_content_key("ability_flower_light"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_winter_calibration"), std::span<const contracts::ContentId>{combat_trigger_7_prerequisites}},
}};

inline constexpr std::array<contracts::QuestCombatOutcomeDefinition, 5> f1_quest_combat_outcomes{{
    {contracts::content_id("f1_outcome_break_eavesguard_target"), contracts::QuestCombatOutcomeKind::hostile_archetype_defeated, contracts::content_id("f1_objective_break_eavesguard_target"), contracts::content_id("f1_training_eavesguard_target"), 1U},
    {contracts::content_id("f1_outcome_break_flower_turn_target"), contracts::QuestCombatOutcomeKind::hostile_archetype_defeated, contracts::content_id("f1_objective_break_flower_turn_target"), contracts::content_id("f1_training_flower_turn_target"), 1U},
    {contracts::content_id("f1_outcome_defeat_leaking_dolls"), contracts::QuestCombatOutcomeKind::hostile_archetype_defeated, contracts::content_id("f1_objective_defeat_leaking_dolls"), contracts::content_id("jn_enemy_leaking_umbrella_doll"), 2U},
    {contracts::content_id("f1_outcome_answer_paper_egret"), contracts::QuestCombatOutcomeKind::hostile_archetype_defeated, contracts::content_id("f1_objective_answer_paper_egret"), contracts::content_id("jn_enemy_faded_paper_egret"), 1U},
    {contracts::content_id("f1_outcome_validate_return_calibration"), contracts::QuestCombatOutcomeKind::all_hostiles_defeated, contracts::content_id("f1_objective_validate_calibration"), contracts::ContentId{}, 0U},
}};

inline constexpr std::array<contracts::QuestEncounterActivationDefinition, 12> f1_quest_encounter_activations{{
    {contracts::content_id("f1_activation_training_windward_guard_rig"), contracts::content_id("f1_beat_shen_yan_training"), contracts::content_id("f1_objective_take_eavesguard_mark"), contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_windward_lane"), contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_0_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_0_placements}},
    {contracts::content_id("f1_activation_training_leeward_guard_rig"), contracts::content_id("f1_beat_shen_yan_training"), contracts::content_id("f1_objective_take_eavesguard_mark"), contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_leeward_lane"), contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_1_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_1_placements}},
    {contracts::content_id("f1_activation_training_windward_eavesguard_target"), contracts::content_id("f1_beat_shen_yan_training"), contracts::content_id("f1_objective_commit_eavesguard_heavy"), contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_windward_lane"), contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_2_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_2_placements}},
    {contracts::content_id("f1_activation_training_leeward_eavesguard_target"), contracts::content_id("f1_beat_shen_yan_training"), contracts::content_id("f1_objective_commit_eavesguard_heavy"), contracts::content_id("f1_objective_choose_training_lane"), contracts::content_id("f1_choice_training_leeward_lane"), contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_3_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_3_placements}},
    {contracts::content_id("f1_activation_training_flower_turn_rig"), contracts::content_id("f1_beat_shen_yan_training"), contracts::content_id("f1_objective_review_eavesguard_with_shen_yan"), contracts::ContentId{}, contracts::ContentId{}, contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_4_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_4_placements}},
    {contracts::content_id("f1_activation_training_flower_turn_target"), contracts::content_id("f1_beat_shen_yan_training"), contracts::content_id("f1_objective_commit_flower_turn_heavy"), contracts::ContentId{}, contracts::ContentId{}, contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_5_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_5_placements}},
    {contracts::content_id("f1_activation_umbrella_lane_first_encounter"), contracts::content_id("f1_beat_umbrella_lane_first_encounter"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_6_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_6_placements}},
    {contracts::content_id("f1_activation_umbrella_lane_paper_egret"), contracts::content_id("f1_beat_umbrella_lane_first_encounter"), contracts::content_id("f1_objective_raise_paper_egret_lure"), contracts::ContentId{}, contracts::ContentId{}, contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_7_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_7_placements}},
    {contracts::content_id("f1_activation_canopy_return_encounter"), contracts::content_id("f1_beat_canopy_return_encounter"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_8_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_8_placements}},
    {contracts::content_id("f1_activation_canopy_return_spring_reinforcement"), contracts::content_id("f1_beat_canopy_return_encounter"), contracts::content_id("f1_objective_prime_return_calibration"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_spring_calibration"), contracts::EncounterActivationMode::reinforce, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_9_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_9_placements}},
    {contracts::content_id("f1_activation_canopy_return_winter_reinforcement"), contracts::content_id("f1_beat_canopy_return_encounter"), contracts::content_id("f1_objective_prime_return_calibration"), contracts::content_id("f1_objective_choose_rib_calibration"), contracts::content_id("f1_choice_rib_winter_calibration"), contracts::EncounterActivationMode::reinforce, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_10_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_10_placements}},
    {contracts::content_id("f1_activation_four_seasons_wraith"), contracts::content_id("f1_beat_four_seasons_wraith"), contracts::ContentId{}, contracts::ContentId{}, contracts::ContentId{}, contracts::EncounterActivationMode::replace, contracts::content_id("f1_encounter_umbrella_lane_bootstrap"), std::span<const contracts::StableActorKey>{encounter_activation_11_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_11_placements}},
}};

inline constexpr std::array<contracts::QuestBossPhaseDefinition, 4> f1_quest_boss_phases{{
    {contracts::content_id("f1_boss_phase_spring"), contracts::content_id("f1_objective_survive_spring_phase"), 201ULL, 75U, contracts::stable_content_key("stance_wraith_summer")},
    {contracts::content_id("f1_boss_phase_summer"), contracts::content_id("f1_objective_survive_summer_phase"), 201ULL, 50U, contracts::stable_content_key("stance_wraith_autumn")},
    {contracts::content_id("f1_boss_phase_autumn"), contracts::content_id("f1_objective_survive_autumn_phase"), 201ULL, 25U, contracts::stable_content_key("stance_wraith_winter")},
    {contracts::content_id("f1_boss_phase_winter"), contracts::content_id("f1_objective_survive_winter_phase"), 201ULL, 0U, 0},
}};

inline constexpr std::array<contracts::QuestResolutionRewardDefinition, 2> f1_quest_resolution_rewards{{
    {contracts::content_id("f1_resolution_reward_subdue"), contracts::content_id("f1_objective_choose_resolution"), contracts::content_id("f1_choice_resolution_subdue"), contracts::content_id("f1_reward_sealed_mixed_umbrella"), contracts::content_id("f1_claim_resolution_subdue")},
    {contracts::content_id("f1_resolution_reward_restore_shared_mark"), contracts::content_id("f1_objective_choose_resolution"), contracts::content_id("f1_choice_resolution_restore_shared_mark"), contracts::content_id("f1_reward_joint_workshop_formula"), contracts::content_id("f1_claim_resolution_restore_shared_mark")},
}};

inline constexpr std::array<contracts::QuestUiCueDefinition, 8> f1_quest_ui_cues{{
    {contracts::content_id("ui.f1.rain.choice.arrival-clue"), contracts::content_id("f1_beat_rain_ferry_arrival"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::choice_available) | contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::interaction_feedback), std::span<const contracts::ContentId>{quest_ui_cue_0_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_0_attempt_evidence}},
    {contracts::content_id("ui.f1.rain.choice.mooring-method"), contracts::content_id("f1_beat_rain_ferry_arrival"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::choice_available), std::span<const contracts::ContentId>{quest_ui_cue_1_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_1_attempt_evidence}},
    {contracts::content_id("ui.f1.rain.mooring-load"), contracts::content_id("f1_beat_rain_ferry_arrival"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::interaction_feedback), std::span<const contracts::ContentId>{quest_ui_cue_2_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{quest_ui_cue_2_selectors}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_2_attempt_evidence}},
    {contracts::content_id("ui.f1.rain.bell-feedback"), contracts::content_id("f1_beat_rain_ferry_arrival"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::interaction_feedback), std::span<const contracts::ContentId>{quest_ui_cue_3_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_3_attempt_evidence}},
    {contracts::content_id("ui.f1.training.choice.lane"), contracts::content_id("f1_beat_shen_yan_training"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::choice_available), std::span<const contracts::ContentId>{quest_ui_cue_4_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_4_attempt_evidence}},
    {contracts::content_id("ui.f1.training.phase"), contracts::content_id("f1_beat_shen_yan_training"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::objective_state), std::span<const contracts::ContentId>{quest_ui_cue_5_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_5_attempt_evidence}},
    {contracts::content_id("ui.f1.training.action-proof"), contracts::content_id("f1_beat_shen_yan_training"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::combat_feedback), std::span<const contracts::ContentId>{quest_ui_cue_6_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{quest_ui_cue_6_selectors}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_6_attempt_evidence}},
    {contracts::content_id("ui.f1.training.recovery"), contracts::content_id("f1_beat_shen_yan_training"), contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::recovery_offer) | contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::recovery_resume), std::span<const contracts::ContentId>{quest_ui_cue_7_objectives}, std::span<const contracts::QuestUiResultSelectorDefinition>{}, std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_7_attempt_evidence}},
}};

inline constexpr std::array<contracts::CombatActorConfig, 11> f1_combat_actors{{
    {1ULL, contracts::content_id("actor_f1_player"), contracts::CombatFaction::player, {-12000, -1600, 0, 0}, {120, 120, 100, 100, 80, 80, 30, 30, 0}, {contracts::stable_content_key("stance_eavesguard"), contracts::stable_content_key("stance_flower_turn"), 0, 0}, 2U, contracts::stable_content_key("stance_eavesguard"), {30, 6, 2, 120, 12, 4}, true},
    {101ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-4000, -2600, 0, 0}, {90, 90, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, false},
    {102ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-3000, -400, 0, 0}, {90, 90, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, false},
    {103ULL, contracts::content_id("jn_enemy_faded_paper_egret"), contracts::CombatFaction::hostile, {-1500, 900, 700, 0}, {70, 70, 100, 100, 28, 28, 0, 0, 0}, {contracts::stable_content_key("stance_paper_egret"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_paper_egret"), {36, 8, 2, 120, 12, 3}, false},
    {104ULL, contracts::content_id("f1_training_umbrella_rig"), contracts::CombatFaction::hostile, {-4100, 2300, 0, 0}, {999, 999, 100, 100, 80, 80, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, false},
    {105ULL, contracts::content_id("f1_training_egret_rig"), contracts::CombatFaction::hostile, {-5200, -1600, 700, 0}, {70, 70, 120, 120, 60, 60, 0, 0, 0}, {contracts::stable_content_key("stance_paper_egret"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_paper_egret"), {36, 8, 2, 120, 12, 3}, false},
    {106ULL, contracts::content_id("jn_enemy_leaking_umbrella_doll"), contracts::CombatFaction::hostile, {-4600, 1600, 0, 0}, {70, 70, 80, 80, 40, 40, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, false},
    {107ULL, contracts::content_id("f1_training_eavesguard_target"), contracts::CombatFaction::hostile, {-4000, 2000, 0, 0}, {120, 120, 80, 80, 48, 48, 0, 0, 0}, {contracts::stable_content_key("stance_umbrella_rust"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_umbrella_rust"), {45, 10, 2, 150, 15, 3}, false},
    {108ULL, contracts::content_id("f1_training_flower_turn_rig"), contracts::CombatFaction::hostile, {-3300, 1200, 700, 0}, {999, 999, 120, 120, 80, 80, 0, 0, 0}, {contracts::stable_content_key("stance_paper_egret"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_paper_egret"), {36, 8, 2, 120, 12, 3}, false},
    {109ULL, contracts::content_id("f1_training_flower_turn_target"), contracts::CombatFaction::hostile, {-3000, -800, 700, 0}, {120, 120, 100, 100, 42, 42, 0, 0, 0}, {contracts::stable_content_key("stance_paper_egret"), 0, 0, 0}, 1U, contracts::stable_content_key("stance_paper_egret"), {36, 8, 2, 120, 12, 3}, false},
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
    180,
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
    std::span<const contracts::VerticalSliceSafePointDefinition>{f1_safe_points},
    std::span<const contracts::QuestInteractionDefinition>{f1_quest_interactions},
    std::span<const contracts::QuestCombatTriggerDefinition>{f1_quest_combat_triggers},
    std::span<const contracts::QuestCombatOutcomeDefinition>{f1_quest_combat_outcomes},
    std::span<const contracts::QuestEncounterActivationDefinition>{f1_quest_encounter_activations},
    std::span<const contracts::QuestBossPhaseDefinition>{f1_quest_boss_phases},
    std::span<const contracts::QuestResolutionRewardDefinition>{f1_quest_resolution_rewards},
    std::span<const contracts::QuestUiCueDefinition>{f1_quest_ui_cues},
};

}  // namespace tgd::content::generated
