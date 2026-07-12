#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>

#include <algorithm>
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
        definition->quest_interactions.size() == 23 &&
            definition->quest_interactions.front().objective_id.key ==
                tgd::contracts::stable_content_key("f1_objective_inspect_travel_writ"),
        "the expanded opening scene interactions are generated content, not presentation rules"
    );
    ok &= expect(
        definition->quest_combat_triggers.size() == 7 &&
            definition->quest_combat_triggers.front().required_stance ==
                tgd::contracts::stable_content_key("stance_eavesguard") &&
            definition->quest_combat_triggers.front().required_ability ==
                tgd::contracts::stable_content_key("ability_eavesguard_heavy") &&
            definition->quest_combat_triggers.front().prerequisite_objectives.size() == 1 &&
            definition->quest_combat_triggers.front().prerequisite_objectives.front().key ==
                tgd::contracts::stable_content_key("f1_objective_meet_shen_yan"),
        "training actions and counters are generated combat-to-quest bindings"
    );
    ok &= expect(
        definition->quest_combat_triggers[5].objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_demonstrate_rib_calibration"
                ) &&
            definition->quest_combat_triggers[5].required_ability ==
                tgd::contracts::stable_content_key("ability_eavesguard_heavy") &&
            definition->quest_combat_triggers[5].required_selection_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_choice_rib_spring_calibration"
                ) &&
            definition->quest_combat_triggers[6].objective_id.key ==
                definition->quest_combat_triggers[5].objective_id.key &&
            definition->quest_combat_triggers[6].required_ability ==
                tgd::contracts::stable_content_key("ability_flower_light") &&
            definition->quest_combat_triggers[6].required_selection_id.key ==
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
            definition->quest_combat_outcomes.size() == 3,
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
    ok &= expect(
        combat != nullptr && definition->quest_encounter_activations.size() == 8 &&
            definition->quest_encounter_activations.front().beat_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_beat_shen_yan_training"
                ) &&
            definition->quest_encounter_activations.front().encounter_id.key ==
                combat->id.key &&
            definition->quest_encounter_activations.front().trigger_objective_id.key == 0 &&
            definition->quest_encounter_activations.front().mode ==
                tgd::contracts::EncounterActivationMode::replace &&
            definition->quest_encounter_activations.front().actor_keys.size() == 1 &&
            definition->quest_encounter_activations.front().actor_keys.front() == 104 &&
            definition->quest_encounter_activations.front().actor_placements.size() == 1 &&
            definition->quest_encounter_activations.front()
                    .actor_placements.front()
                    .formation_slot == 0 &&
            definition->quest_encounter_activations[1].beat_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_beat_shen_yan_training"
                ) &&
            definition->quest_encounter_activations[1].trigger_objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_eavesguard_counter"
                ) &&
            definition->quest_encounter_activations[1].mode ==
                tgd::contracts::EncounterActivationMode::replace &&
            definition->quest_encounter_activations[1].actor_keys.size() == 1 &&
            definition->quest_encounter_activations[1].actor_keys.front() == 105 &&
            definition->quest_encounter_activations[1]
                    .actor_placements.front()
                    .formation_slot == 2 &&
            definition->quest_encounter_activations[2].beat_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_beat_umbrella_lane_first_encounter"
                ) &&
            definition->quest_encounter_activations[2].actor_keys.size() == 2 &&
            definition->quest_encounter_activations[2].actor_placements[0].formation_slot == 1 &&
            definition->quest_encounter_activations[2].actor_placements[1].formation_slot == 5 &&
            definition->quest_encounter_activations[3].trigger_objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_raise_paper_egret_lure"
                ) &&
            definition->quest_encounter_activations[3].mode ==
                tgd::contracts::EncounterActivationMode::replace &&
            definition->quest_encounter_activations[3].actor_keys.size() == 1 &&
            definition->quest_encounter_activations[3].actor_keys.front() == 103 &&
            definition->quest_encounter_activations[3]
                    .actor_placements.front()
                    .formation_slot == 2 &&
            definition->quest_encounter_activations[4].beat_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_beat_canopy_return_encounter"
                ) &&
            definition->quest_encounter_activations[4].actor_keys.size() == 3 &&
            definition->quest_encounter_activations[4].actor_placements[0].pose ==
                tgd::contracts::GroundPoseMm{-2500, -1800, 0, 0} &&
            definition->quest_encounter_activations[4].actor_placements[1].formation_slot == 3 &&
            definition->quest_encounter_activations[4].actor_placements[2].formation_slot == 6 &&
            definition->quest_encounter_activations[5].trigger_objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_prime_return_calibration"
                ) &&
            definition->quest_encounter_activations[5]
                    .required_selection_objective_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_objective_choose_rib_calibration"
                ) &&
            definition->quest_encounter_activations[5].required_selection_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_choice_rib_spring_calibration"
                ) &&
            definition->quest_encounter_activations[5].mode ==
                tgd::contracts::EncounterActivationMode::reinforce &&
            definition->quest_encounter_activations[5].actor_keys.size() == 1 &&
            definition->quest_encounter_activations[5].actor_keys[0] == 106 &&
            definition->quest_encounter_activations[5]
                    .actor_placements[0]
                    .formation_slot == 5 &&
            definition->quest_encounter_activations[6].required_selection_id.key ==
                tgd::contracts::stable_content_key(
                    "f1_choice_rib_winter_calibration"
                ) &&
            definition->quest_encounter_activations[6].mode ==
                tgd::contracts::EncounterActivationMode::reinforce &&
            definition->quest_encounter_activations[6].actor_keys.size() == 1 &&
            definition->quest_encounter_activations[6].actor_keys[0] == 105 &&
            definition->quest_encounter_activations[6]
                    .actor_placements[0]
                    .formation_slot == 5,
        "training, lane waves, and both selection-driven return variants are generated content"
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
            combat->actors.size() == 8 && combat->abilities.size() == 17,
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
                                           );
            found_training_egret |= actor.actor == 105 &&
                                    actor.archetype_id.key ==
                                        tgd::contracts::stable_content_key(
                                            "f1_training_egret_rig"
                                        ) &&
                                    actor.initial_stance ==
                                        tgd::contracts::stable_content_key(
                                            "stance_paper_egret"
                                        );
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
            found_training_umbrella && found_training_egret && all_hostiles_dormant,
            "training rigs and future hostile groups start dormant until their authored beat"
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
