#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unordered_set>

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "F1 content contract failure: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* definition = provider.find_vertical_slice(
        tgd::contracts::stable_content_key("f1_rainy_umbrella_trial")
    );
    bool ok = expect(definition != nullptr, "built-in provider resolves the F1 slice");
    if (definition == nullptr) {
        return EXIT_FAILURE;
    }

    ok &= expect(
        provider.find_vertical_slice(tgd::contracts::stable_content_key("missing_slice")) ==
            nullptr,
        "unknown content fails closed"
    );
    const auto* combat = provider.find_combat_encounter(
        tgd::contracts::stable_content_key("f1_encounter_umbrella_lane_bootstrap")
    );
    ok &= expect(combat != nullptr, "built-in provider resolves the F1 combat bootstrap");
    ok &= expect(
        provider.find_combat_encounter(tgd::contracts::stable_content_key("missing_encounter")) ==
            nullptr,
        "unknown combat content fails closed"
    );
    ok &= expect(
        definition->view_model == "2.5d-oblique-panoramic" &&
            definition->primary_guidance == "douzhanshen" &&
            definition->secondary_reference == "warm-snow-combat-readability",
        "Douzhanshen remains the first view and staging guide"
    );
    ok &= expect(
        definition->playable_target_minutes == 60 &&
            definition->end_to_end_test_budget_minutes == 70 &&
            definition->playable_activity_grace_ticks == 180,
        "playable, E2E, and bounded activity-audit budgets stay explicit"
    );
    ok &= expect(
        definition->beats.size() == 7 && definition->cell_ids.size() == 5 &&
            definition->enemy_family_ids.size() == 2,
        "the first route, cells, and enemy families are explicit"
    );
    ok &= expect(
        definition->safe_points.size() == definition->beats.size() &&
            definition->safe_points.front().beat_id.key ==
                definition->beats.front().id.key &&
            definition->safe_points.front().pose == definition->player.initial_pose &&
            definition->safe_points[5].id.key ==
                tgd::contracts::stable_content_key(
                    "f1_safe_point_four_seasons_court"
                ) &&
            definition->safe_points[5].pose ==
                tgd::contracts::GroundPoseMm{2200, 800, 0, 0},
        "every beat owns an ordered content-driven movement safe point"
    );
    ok &= expect(
        definition->quest_interactions.size() == 41 &&
            definition->quest_interactions.front().objective_id.key ==
                tgd::contracts::stable_content_key("f1_objective_inspect_travel_writ"),
        "the expanded opening scene interactions are generated content, not presentation rules"
    );
    const auto find_ui_cue = [definition](std::string_view id) {
        return std::find_if(
            definition->quest_ui_cues.begin(),
            definition->quest_ui_cues.end(),
            [id](const tgd::contracts::QuestUiCueDefinition& cue) {
                return cue.cue_id.key == tgd::contracts::stable_content_key(id);
            }
        );
    };
    const auto mooring_load_cue = find_ui_cue("ui.f1.rain.mooring-load");
    const auto action_proof_cue = find_ui_cue("ui.f1.training.action-proof");
    std::size_t attempt_evidence_rule_count = 0;
    for (const auto& cue : definition->quest_ui_cues) {
        attempt_evidence_rule_count += cue.attempt_evidence_rules.size();
    }
    ok &= expect(
        definition->quest_ui_cues.size() == 8 &&
            attempt_evidence_rule_count == 14 &&
            mooring_load_cue != definition->quest_ui_cues.end() &&
            mooring_load_cue->objective_ids.size() == 1 &&
            mooring_load_cue->result_selectors.size() == 1 &&
            mooring_load_cue->result_selectors.front().primary_result_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_interaction_choose_quick_hitch"
                ) &&
            mooring_load_cue->result_selectors.front().polarity_override ==
                tgd::contracts::QuestUiPolarityOverride::negative,
        "the generated rain-ferry cue preserves accepted-but-negative craft feedback"
    );
    ok &= expect(
        mooring_load_cue != definition->quest_ui_cues.end() &&
            std::any_of(
                mooring_load_cue->attempt_evidence_rules.begin(),
                mooring_load_cue->attempt_evidence_rules.end(),
                [](const tgd::contracts::QuestUiAttemptEvidenceRuleDefinition& rule) {
                    return rule.source ==
                               tgd::contracts::QuestUiProjectionSource::interaction_feedback &&
                           rule.objective_id.key == tgd::contracts::stable_content_key(
                                                        "f1_objective_secure_ferry_mooring"
                                                    ) &&
                           rule.primary_result.result_id.key ==
                               tgd::contracts::stable_content_key(
                                   "f1_interaction_choose_quick_hitch"
                               ) &&
                           rule.primary_result.status ==
                               tgd::contracts::QuestUiResultStatus::accepted &&
                           rule.primary_result.rejection_reason ==
                               tgd::contracts::QuestUiRejectionReason::none &&
                           rule.secondary_result.status ==
                               tgd::contracts::QuestUiResultStatus::not_applicable &&
                           rule.classification ==
                               tgd::contracts::QuestUiAttemptTimeClassification::
                                   qualifying_error_feedback;
                }
            ),
        "quick-hitch overload feedback is exact Definition-owned attempt evidence"
    );
    ok &= expect(
        action_proof_cue != definition->quest_ui_cues.end() &&
            action_proof_cue->objective_ids.size() == 8 &&
            action_proof_cue->result_selectors.size() == 2 &&
            action_proof_cue->result_selectors[0].primary_result_id.key ==
                tgd::contracts::stable_content_key("f1_trigger_eavesguard_heavy") &&
            action_proof_cue->result_selectors[0].secondary_result_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_outcome_break_eavesguard_target"
                ) &&
            action_proof_cue->result_selectors[1].primary_result_id.key ==
                tgd::contracts::stable_content_key("f1_trigger_flower_turn_heavy") &&
            action_proof_cue->result_selectors[1].secondary_result_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_outcome_break_flower_turn_target"
                ),
        "the generated training cue keeps action and world outcomes in separate result slots"
    );
    ok &= expect(
        action_proof_cue != definition->quest_ui_cues.end() &&
            std::any_of(
                action_proof_cue->attempt_evidence_rules.begin(),
                action_proof_cue->attempt_evidence_rules.end(),
                [](const tgd::contracts::QuestUiAttemptEvidenceRuleDefinition& rule) {
                    return rule.source ==
                               tgd::contracts::QuestUiProjectionSource::combat_feedback &&
                           rule.objective_id.key == tgd::contracts::stable_content_key(
                                                        "f1_objective_break_flower_turn_target"
                                                    ) &&
                           rule.primary_result.result_id.key ==
                               tgd::contracts::stable_content_key(
                                   "f1_trigger_flower_turn_heavy"
                               ) &&
                           rule.primary_result.status ==
                               tgd::contracts::QuestUiResultStatus::accepted &&
                           rule.secondary_result.result_id.key ==
                               tgd::contracts::stable_content_key(
                                   "f1_outcome_break_flower_turn_target"
                               ) &&
                           rule.secondary_result.status ==
                               tgd::contracts::QuestUiResultStatus::rejected &&
                           rule.secondary_result.rejection_reason ==
                               tgd::contracts::QuestUiRejectionReason::wrong_target &&
                           rule.classification ==
                               tgd::contracts::QuestUiAttemptTimeClassification::
                                   qualifying_combat_feedback;
                }
            ),
        "wrong-target flower proof remains exact two-slot Definition-owned evidence"
    );
    for (const auto& cue : definition->quest_ui_cues) {
        const auto beat = std::find_if(
            definition->beats.begin(),
            definition->beats.end(),
            [&cue](const tgd::contracts::VerticalSliceBeatDefinition& candidate) {
                return candidate.id.key == cue.beat_id.key;
            }
        );
        ok &= expect(
            beat != definition->beats.end() &&
                static_cast<std::size_t>(beat - definition->beats.begin()) < 2,
            "F1 1.6 task UI cues remain scoped to the first two authored beats"
        );
        if (beat == definition->beats.end()) {
            continue;
        }
        for (const auto& objective : cue.objective_ids) {
            ok &= expect(
                std::find_if(
                    beat->objectives.begin(),
                    beat->objectives.end(),
                    [&objective](const tgd::contracts::ContentId& candidate) {
                        return candidate.key == objective.key;
                    }
                ) != beat->objectives.end(),
                "every task UI objective is authored by its declared beat"
            );
        }
        ok &= expect(
            !cue.attempt_evidence_rules.empty() &&
                cue.attempt_evidence_rules.size() <=
                    tgd::contracts::quest_ui_attempt_evidence_rule_capacity,
            "every task UI cue owns a bounded attempt-evidence rule set"
        );
        for (const auto& rule : cue.attempt_evidence_rules) {
            ok &= expect(
                (cue.source_mask &
                 tgd::contracts::quest_ui_projection_source_bit(rule.source)) != 0 &&
                    std::find_if(
                        cue.objective_ids.begin(),
                        cue.objective_ids.end(),
                        [&rule](const tgd::contracts::ContentId& objective) {
                            return objective.key == rule.objective_id.key;
                        }
                    ) != cue.objective_ids.end(),
                "every attempt-evidence rule stays inside its cue source/objective domain"
            );
        }
    }
    ok &= expect(
        definition->quest_combat_triggers.size() == 8 &&
            definition->quest_combat_triggers.front().required_stance ==
                tgd::contracts::stable_content_key("stance_eavesguard") &&
            definition->quest_combat_triggers.front().required_ability == 0 &&
            definition->quest_combat_triggers[1].required_ability ==
                tgd::contracts::stable_content_key("ability_eavesguard_heavy") &&
            definition->quest_combat_triggers.front().prerequisite_objectives.size() == 1 &&
            definition->quest_combat_triggers.front().prerequisite_objectives.front().key ==
                tgd::contracts::stable_content_key("f1_objective_take_eavesguard_mark"),
        "training guard, evade, and chained abilities are generated combat-to-quest bindings"
    );
    ok &= expect(
        definition->quest_combat_triggers[6].objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_demonstrate_rib_calibration"
                ) &&
            definition->quest_combat_triggers[6].required_ability ==
                tgd::contracts::stable_content_key("ability_eavesguard_heavy") &&
            definition->quest_combat_triggers[6].required_selection_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_choice_rib_spring_calibration"
                ) &&
            definition->quest_combat_triggers[7].objective_id.key ==
                definition->quest_combat_triggers[6].objective_id.key &&
            definition->quest_combat_triggers[7].required_ability ==
                tgd::contracts::stable_content_key("ability_flower_light") &&
            definition->quest_combat_triggers[7].required_selection_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_choice_rib_winter_calibration"
                ),
        "return calibration actions are generated as mutually exclusive choice variants"
    );
    const auto route_interaction_count = std::count_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                   tgd::contracts::stable_content_key("f1_objective_choose_lane_route");
        }
    );
    const auto canopy_route_interaction = std::find_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.selection_id.key ==
                   tgd::contracts::stable_content_key("f1_choice_lane_canopy");
        }
    );
    const auto drain_route_interaction = std::find_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.selection_id.key ==
                   tgd::contracts::stable_content_key("f1_choice_lane_drain");
        }
    );
    ok &= expect(
        route_interaction_count == 2 &&
            canopy_route_interaction != definition->quest_interactions.end() &&
            drain_route_interaction != definition->quest_interactions.end() &&
            canopy_route_interaction->prerequisite_objectives.size() == 5 &&
            drain_route_interaction->prerequisite_objectives.size() == 5 &&
            definition->quest_combat_outcomes.size() == 5,
        "two lane choices wait for combat and the ordered rainworks chain"
    );
    const auto route_evidence_count = std::count_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key == tgd::contracts::stable_content_key(
                                                      "f1_objective_reveal_spring_trace"
                                                  ) &&
                   interaction.required_selection_objective_id.key ==
                       tgd::contracts::stable_content_key(
                           "f1_objective_choose_lane_route"
                       );
        }
    );
    ok &= expect(
        route_evidence_count == 2,
        "each authored lane route owns one selection-gated spring-trace entry"
    );
    const auto calibration_count = std::count_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                   tgd::contracts::stable_content_key(
                       "f1_objective_choose_rib_calibration"
                   );
        }
    );
    ok &= expect(
        calibration_count == 2,
        "the shared workbench exposes two generated calibration choices"
    );
    const auto resolution_choice_count = std::count_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                       tgd::contracts::stable_content_key(
                           "f1_objective_choose_resolution"
                       ) &&
                   interaction.kind == tgd::contracts::QuestInteractionKind::choose;
        }
    );
    const auto return_to_shen_yan = std::find_if(
        definition->quest_interactions.begin(),
        definition->quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                   tgd::contracts::stable_content_key(
                       "f1_objective_return_to_shen_yan"
                   );
        }
    );
    ok &= expect(
        resolution_choice_count == 2 &&
            return_to_shen_yan != definition->quest_interactions.end() &&
            return_to_shen_yan->kind == tgd::contracts::QuestInteractionKind::talk &&
            return_to_shen_yan->prerequisite_objectives.size() == 1 &&
            definition->quest_resolution_rewards.size() == 2 &&
            definition->quest_resolution_rewards.front().reward_dedup_key.key ==
                tgd::contracts::stable_content_key("f1_claim_resolution_subdue") &&
            definition->quest_resolution_rewards.back().reward_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_reward_joint_workshop_formula"
                ),
        "two resolutions gate the authored return and map to stable reward receipts"
    );
    const auto training_beat =
        tgd::contracts::stable_content_key("f1_beat_shen_yan_training");
    const auto training_activation_count = std::count_if(
        definition->quest_encounter_activations.begin(),
        definition->quest_encounter_activations.end(),
        [](const tgd::contracts::QuestEncounterActivationDefinition& activation) {
            return activation.beat_id.key == training_beat;
        }
    );
    ok &= expect(
        combat != nullptr && definition->quest_encounter_activations.size() == 12 &&
            training_activation_count == 6 &&
            definition->quest_encounter_activations[0].trigger_objective_id.key ==
                tgd::contracts::stable_content_key("f1_objective_take_eavesguard_mark") &&
            definition->quest_encounter_activations[0].required_selection_id.key ==
                tgd::contracts::stable_content_key("f1_choice_training_windward_lane") &&
            definition->quest_encounter_activations[1].required_selection_id.key ==
                tgd::contracts::stable_content_key("f1_choice_training_leeward_lane") &&
            definition->quest_encounter_activations[0].actor_keys.front() == 104 &&
            definition->quest_encounter_activations[1].actor_keys.front() == 104 &&
            definition->quest_encounter_activations[2].trigger_objective_id.key ==
                tgd::contracts::stable_content_key("f1_objective_commit_eavesguard_heavy") &&
            definition->quest_encounter_activations[2].actor_keys.front() == 107 &&
            definition->quest_encounter_activations[3].actor_keys.front() == 107 &&
            definition->quest_encounter_activations[4].trigger_objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_review_eavesguard_with_shen_yan"
                ) &&
            definition->quest_encounter_activations[4].actor_keys.front() == 108 &&
            definition->quest_encounter_activations[5].trigger_objective_id.key ==
                tgd::contracts::stable_content_key("f1_objective_commit_flower_turn_heavy") &&
            definition->quest_encounter_activations[5].actor_keys.front() == 109 &&
            definition->quest_encounter_activations[6].actor_keys.size() == 2 &&
            definition->quest_encounter_activations[7].actor_keys.front() == 103 &&
            definition->quest_encounter_activations[8].actor_keys.size() == 3 &&
            definition->quest_encounter_activations[9].required_selection_id.key ==
                tgd::contracts::stable_content_key("f1_choice_rib_spring_calibration") &&
            definition->quest_encounter_activations[9].actor_keys.front() == 106 &&
            definition->quest_encounter_activations[10].required_selection_id.key ==
                tgd::contracts::stable_content_key("f1_choice_rib_winter_calibration") &&
            definition->quest_encounter_activations[10].actor_keys.front() == 105,
        "training lanes, proof targets, lane waves, and return variants are generated content"
    );
    ok &= expect(
        definition->quest_encounter_activations.back().beat_id.key ==
                tgd::contracts::stable_content_key("f1_beat_four_seasons_wraith") &&
            definition->quest_encounter_activations.back().actor_keys.size() == 1 &&
            definition->quest_encounter_activations.back().actor_keys.front() == 201 &&
            definition->quest_encounter_activations.back()
                    .actor_placements.front()
                    .formation_slot == 4 &&
            definition->quest_boss_phases.size() == 4 &&
            definition->quest_boss_phases.front().health_percent == 75 &&
            definition->quest_boss_phases.back().health_percent == 0 &&
            definition->quest_boss_phases.back().next_stance == 0,
        "the four-season boss activation and ordered thresholds are generated content"
    );

    std::uint32_t minutes = 0;
    std::unordered_set<tgd::contracts::StableContentKey> ids;
    for (const auto& beat : definition->beats) {
        minutes += beat.target_minutes;
        ok &= expect(beat.id.key != 0 && ids.insert(beat.id.key).second, "beat key is unique");
        ok &= expect(beat.cell_id.key != 0, "beat has an explicit cell");
        ok &= expect(!beat.objectives.empty(), "beat advances from objectives, not a timer");
        for (const auto& objective : beat.objectives) {
            ok &= expect(
                objective.key != 0 && ids.insert(objective.key).second,
                "objective key is unique"
            );
        }
    }
    ok &= expect(minutes == 60, "seven playable beat budgets total exactly 60 minutes");

    if (combat != nullptr) {
        ok &= expect(
            combat->actors.size() == 11 && combat->abilities.size() == 17,
            "the player, enemy groups, and four-season boss combat set is explicit"
        );
        ok &= expect(
            combat->director.player_actor == definition->player.actor &&
                combat->director.max_simultaneous_attackers == 1 &&
                combat->director.formation_radius_mm > 0 &&
                combat->director.chase_speed_mm_per_second % 60 == 0,
            "the encounter director has deterministic player and attack-token rules"
        );
        bool found_player = false;
        bool found_training_umbrella = false;
        bool found_training_egret = false;
        bool found_eavesguard_target = false;
        bool found_flower_signal_rig = false;
        bool found_flower_target = false;
        bool found_inactive_boss = false;
        bool all_hostiles_dormant = true;
        std::unordered_set<tgd::contracts::StableContentKey> ability_ids;
        for (const auto& actor : combat->actors) {
            found_player |= actor.actor == definition->player.actor &&
                            actor.faction == tgd::contracts::CombatFaction::player &&
                            actor.initial_pose == definition->player.initial_pose;
            found_training_umbrella |= actor.actor == 104 &&
                                       actor.archetype_id.key ==
                                           tgd::contracts::stable_content_key(
                                               "f1_training_umbrella_rig"
                                           ) &&
                                       actor.initial_stance ==
                                           tgd::contracts::stable_content_key(
                                               "stance_umbrella_rust"
                                           ) &&
                                       actor.initial_resources.health == 999;
            found_training_egret |= actor.actor == 105 &&
                                    actor.archetype_id.key ==
                                        tgd::contracts::stable_content_key(
                                            "f1_training_egret_rig"
                                        ) &&
                                    actor.initial_stance ==
                                        tgd::contracts::stable_content_key(
                                            "stance_paper_egret"
                                        );
            found_eavesguard_target |= actor.actor == 107 &&
                                        actor.archetype_id.key ==
                                            tgd::contracts::stable_content_key(
                                                "f1_training_eavesguard_target"
                                            ) &&
                                        actor.initial_resources.health == 120;
            found_flower_signal_rig |= actor.actor == 108 &&
                                       actor.archetype_id.key ==
                                           tgd::contracts::stable_content_key(
                                               "f1_training_flower_turn_rig"
                                           ) &&
                                       actor.initial_resources.health == 999;
            found_flower_target |= actor.actor == 109 &&
                                   actor.archetype_id.key ==
                                       tgd::contracts::stable_content_key(
                                           "f1_training_flower_turn_target"
                                       ) &&
                                   actor.initial_resources.health == 120;
            found_inactive_boss |= actor.actor == 201 &&
                                   actor.archetype_id.key == definition->boss_id.key &&
                                   actor.faction == tgd::contracts::CombatFaction::hostile &&
                                   actor.stance_count == 4 && !actor.initially_active;
            if (actor.faction == tgd::contracts::CombatFaction::hostile) {
                all_hostiles_dormant &= !actor.initially_active;
            }
            ok &= expect(
                actor.stance_count > 0 && actor.initial_stance != 0,
                "each combat actor declares a stance"
            );
            ok &= expect(
                actor.recovery.stamina_delay_ticks > 0 &&
                    actor.recovery.stamina_interval_ticks > 0 &&
                    actor.recovery.stamina_per_interval > 0 &&
                    actor.recovery.poise_delay_ticks > 0 &&
                    actor.recovery.poise_interval_ticks > 0 &&
                    actor.recovery.poise_per_interval > 0,
                "each combat actor declares deterministic resource recovery"
            );
        }
        for (const auto& ability : combat->abilities) {
            ok &= expect(
                ability.id.key != 0 && ability_ids.insert(ability.id.key).second,
                "combat ability keys are unique"
            );
            ok &= expect(ability.active_ticks > 0, "ability has an authoritative active window");
        }
        ok &= expect(found_player, "combat and movement share the same player seed");
        ok &= expect(
            found_training_umbrella && found_training_egret &&
                found_eavesguard_target && found_flower_signal_rig &&
                found_flower_target && all_hostiles_dormant,
            "training safety rigs, proof targets, and future groups start dormant"
        );
        ok &= expect(found_inactive_boss, "the four-season boss starts inactive with four stances");
    }

    const auto& basis = definition->player.camera_basis;
    const auto dot = static_cast<std::int64_t>(basis.screen_right_world.x) *
                         basis.screen_forward_world.x +
                     static_cast<std::int64_t>(basis.screen_right_world.y) *
                         basis.screen_forward_world.y;
    const auto determinant = static_cast<std::int64_t>(basis.screen_right_world.x) *
                                 basis.screen_forward_world.y -
                             static_cast<std::int64_t>(basis.screen_right_world.y) *
                                 basis.screen_forward_world.x;
    ok &= expect(dot == 0 && determinant > 0, "camera-relative movement uses a right-handed oblique basis");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
