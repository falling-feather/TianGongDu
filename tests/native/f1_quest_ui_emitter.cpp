#include "F1QuestUiProjection.hpp"

#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/combat_types.hpp>
#include <tgd/gameplay/quest_ui_projection.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using tgd::contracts::StableContentKey;
using tgd::contracts::stable_content_key;
using tgd::gameplay::DeterministicQuestRuntime;
using tgd::gameplay::QuestError;

class CollectingQuestSink final : public tgd::gameplay::IQuestEventSink {
  public:
    void publish(std::span<const tgd::contracts::QuestEvent> events) noexcept override {
        values.assign(events.begin(), events.end());
    }

    std::vector<tgd::contracts::QuestEvent> values;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "F1 Quest UI emitter failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] const tgd::contracts::VerticalSliceDefinition& definition() {
    static tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* value = provider.find_vertical_slice(
        stable_content_key("f1_rainy_umbrella_trial")
    );
    if (value == nullptr) {
        std::cerr << "F1 Quest UI emitter setup failure: vertical slice missing\n";
        std::abort();
    }
    return *value;
}

[[nodiscard]] const tgd::contracts::CombatEncounterDefinition& combat_definition() {
    static tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* value = provider.find_combat_encounter(
        stable_content_key("f1_encounter_umbrella_lane_bootstrap")
    );
    if (value == nullptr) {
        std::cerr << "F1 Quest UI emitter setup failure: combat encounter missing\n";
        std::abort();
    }
    return *value;
}

struct QuestHarness final {
    DeterministicQuestRuntime quest{};
    CollectingQuestSink sink{};
    tgd::contracts::TickIndex tick{1};
    tgd::contracts::CommandSequence sequence{1};

    [[nodiscard]] bool start() {
        return quest.initialize(definition(), definition().player.actor) == QuestError::none &&
               quest.start() == QuestError::none;
    }

    [[nodiscard]] bool complete(
        StableContentKey objective,
        StableContentKey selection = 0
    ) {
        sink.values.clear();
        const auto applied = quest.apply(
            {
                tick++,
                definition().player.actor,
                sequence++,
                tgd::contracts::QuestCommandType::complete_objective,
                objective,
                selection,
            },
            sink
        );
        return applied.error == QuestError::none && applied.accepted;
    }
};

using Selection = std::pair<StableContentKey, StableContentKey>;

[[nodiscard]] StableContentKey selection_for(
    StableContentKey objective,
    std::span<const Selection> selections
) {
    const auto selected = std::find_if(
        selections.begin(),
        selections.end(),
        [objective](const Selection& candidate) { return candidate.first == objective; }
    );
    if (selected != selections.end()) {
        return selected->second;
    }
    const auto authored = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [objective](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key == objective &&
                   interaction.kind == tgd::contracts::QuestInteractionKind::choose;
        }
    );
    return authored == definition().quest_interactions.end() ? 0
                                                              : authored->selection_id.key;
}

bool complete_prefix(
    QuestHarness& harness,
    std::size_t beat_index,
    std::size_t count,
    std::span<const Selection> selections = {}
) {
    if (beat_index >= definition().beats.size() ||
        count > definition().beats[beat_index].objectives.size()) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        const auto objective = definition().beats[beat_index].objectives[index].key;
        if (!harness.complete(objective, selection_for(objective, selections))) {
            return false;
        }
    }
    return true;
}

bool enter_training(
    QuestHarness& harness,
    std::span<const Selection> selections = {}
) {
    return complete_prefix(
        harness,
        0,
        definition().beats[0].objectives.size(),
        selections
    );
}

[[nodiscard]] const tgd::contracts::QuestInteractionDefinition* interaction_definition(
    StableContentKey id
) {
    const auto found = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [id](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.id.key == id;
        }
    );
    return found == definition().quest_interactions.end() ? nullptr : &*found;
}

[[nodiscard]] const tgd::contracts::QuestCombatTriggerDefinition* trigger_definition(
    StableContentKey id
) {
    const auto found = std::find_if(
        definition().quest_combat_triggers.begin(),
        definition().quest_combat_triggers.end(),
        [id](const tgd::contracts::QuestCombatTriggerDefinition& trigger) {
            return trigger.id.key == id;
        }
    );
    return found == definition().quest_combat_triggers.end() ? nullptr : &*found;
}

[[nodiscard]] const tgd::contracts::QuestCombatOutcomeDefinition* outcome_definition(
    StableContentKey id
) {
    const auto found = std::find_if(
        definition().quest_combat_outcomes.begin(),
        definition().quest_combat_outcomes.end(),
        [id](const tgd::contracts::QuestCombatOutcomeDefinition& outcome) {
            return outcome.id.key == id;
        }
    );
    return found == definition().quest_combat_outcomes.end() ? nullptr : &*found;
}

[[nodiscard]] tgd::gameplay::QuestInteractionResult resolve_attempt(
    const tgd::contracts::QuestInteractionDefinition& interaction,
    const tgd::gameplay::IQuestRuntime& quest
) {
    tgd::gameplay::DeterministicQuestInteractionResolver resolver;
    if (resolver.initialize(definition()) != tgd::gameplay::QuestInteractionError::none) {
        std::cerr << "F1 Quest UI emitter setup failure: interaction resolver init\n";
        std::abort();
    }
    return resolver.resolve_attempt(
        {
            definition().player.actor,
            interaction.cell_id.key,
            interaction.pose,
        },
        quest
    );
}

[[nodiscard]] StableContentKey safe_point_for(const tgd::gameplay::IQuestRuntime& quest) {
    const auto found = std::find_if(
        definition().safe_points.begin(),
        definition().safe_points.end(),
        [&quest](const tgd::contracts::VerticalSliceSafePointDefinition& safe_point) {
            return safe_point.beat_id.key == quest.snapshot().stage;
        }
    );
    return found == definition().safe_points.end() ? 0 : found->id.key;
}

bool project(
    const F1QuestUiSignalEmission& emission,
    const tgd::gameplay::IQuestRuntime& quest,
    tgd::contracts::QuestUiAttemptTimeClassification expected_classification,
    std::span<const tgd::contracts::CombatActorSnapshot> actors = {}
) {
    if (!expect(
            emission.error == F1QuestUiSignalEmitterError::none && emission.found,
            "authoritative raw result produces one App emission"
        )) {
        return false;
    }
    tgd::gameplay::DeterministicQuestUiProjectionProducer producer;
    bool ok = expect(
        producer.initialize(definition()) == tgd::gameplay::QuestUiProjectionError::none,
        "actual F1 1.6 producer initializes"
    );
    const auto projected = producer.project(
        emission.signal,
        quest,
        safe_point_for(quest),
        actors
    );
    if (projected.error != tgd::gameplay::QuestUiProjectionError::none) {
        std::cerr << "  projection error=" << static_cast<int>(projected.error)
                  << " source=" << static_cast<int>(emission.signal.source)
                  << " focus=" << emission.signal.objective
                  << " primary=" << emission.signal.primary_result.id
                  << " secondary=" << emission.signal.secondary_result.id << '\n';
    }
    ok &= expect(
        projected.error == tgd::gameplay::QuestUiProjectionError::none,
        "actual raw signal projects through the Definition-owned cue and evidence rule"
    );
    ok &= expect(
        projected.projection.attempt_time_classification == expected_classification,
        "attempt classification is derived by the producer rather than supplied by the emitter"
    );
    ok &= expect(
        projected.projection.primary_result == emission.signal.primary_result &&
            projected.projection.secondary_result == emission.signal.secondary_result,
        "projection preserves independent raw result slots"
    );
    return ok;
}

bool test_interaction_feedback_events() {
    using tgd::contracts::QuestUiAttemptTimeClassification;
    using tgd::contracts::QuestUiRejectionReason;
    using tgd::contracts::QuestUiResultStatus;
    using tgd::gameplay::QuestInteractionAvailability;

    F1QuestUiSignalEmitter emitter;
    bool ok = expect(
        emitter.initialize(definition()) == F1QuestUiSignalEmitterError::none,
        "App emitter initializes from the actual Definition"
    );

    {
        QuestHarness harness;
        const std::array selections{
            Selection{
                stable_content_key("f1_objective_choose_arrival_clue"),
                stable_content_key("f1_choice_arrival_high_water_tags"),
            },
        };
        ok &= expect(harness.start(), "arrival repeat quest starts");
        ok &= expect(complete_prefix(harness, 0, 2, selections), "arrival choice is committed");
        const auto* drowned = interaction_definition(
            stable_content_key("f1_interaction_arrival_clue_drowned_manifest")
        );
        ok &= expect(drowned != nullptr, "repeat interaction is authored");
        if (drowned != nullptr) {
            const auto attempt = resolve_attempt(*drowned, harness.quest);
            const auto before = harness.quest.snapshot();
            const auto emission = emitter.interaction_feedback(attempt, false, harness.quest);
            ok &= expect(
                attempt.availability == QuestInteractionAvailability::selection_already_committed,
                "completed choose returns selection_already_committed"
            );
            ok &= expect(
                emission.signal.primary_result.status == QuestUiResultStatus::ignored_repeat &&
                    emission.signal.primary_result.rejection_reason ==
                        QuestUiRejectionReason::selection_already_committed,
                "repeat maps to ignored_repeat without a Quest command"
            );
            ok &= project(
                emission,
                harness.quest,
                QuestUiAttemptTimeClassification::repeat_no_progress
            );
            ok &= expect(
                harness.quest.snapshot().checksum == before.checksum &&
                    harness.quest.snapshot().completed_total == before.completed_total &&
                    harness.quest.snapshot().selection_count == before.selection_count,
                "repeat emission and projection do not advance Quest"
            );
        }
    }

    {
        QuestHarness harness;
        ok &= expect(harness.start(), "bell rejection quest starts");
        ok &= expect(complete_prefix(harness, 0, 8), "bell prerequisite frontier is built");
        const auto* bell = interaction_definition(
            stable_content_key("f1_interaction_sound_workshop_bell")
        );
        ok &= expect(bell != nullptr, "bell interaction is authored");
        if (bell != nullptr) {
            const auto attempt = resolve_attempt(*bell, harness.quest);
            const auto before = harness.quest.snapshot();
            const auto emission = emitter.interaction_feedback(attempt, false, harness.quest);
            ok &= expect(
                attempt.availability == QuestInteractionAvailability::prerequisite_incomplete &&
                    emission.signal.primary_result.status == QuestUiResultStatus::rejected &&
                    emission.signal.primary_result.rejection_reason ==
                        QuestUiRejectionReason::prerequisite_incomplete,
                "unread bell code maps to rejected prerequisite feedback"
            );
            ok &= project(
                emission,
                harness.quest,
                QuestUiAttemptTimeClassification::qualifying_wrong_order_feedback
            );
            ok &= expect(
                harness.quest.snapshot().checksum == before.checksum,
                "rejected interaction emission does not submit a Quest command"
            );
        }
    }

    {
        QuestHarness harness;
        ok &= expect(harness.start(), "accepted interaction quest starts");
        ok &= expect(complete_prefix(harness, 0, 9), "bell code is read");
        const auto* bell = interaction_definition(
            stable_content_key("f1_interaction_sound_workshop_bell")
        );
        if (bell != nullptr) {
            const auto accepted = resolve_attempt(*bell, harness.quest);
            const auto before_failed_command = harness.quest.snapshot();
            const auto premature = emitter.interaction_feedback(
                accepted,
                false,
                harness.quest
            );
            ok &= expect(
                premature.error == F1QuestUiSignalEmitterError::none && !premature.found &&
                    harness.quest.snapshot().checksum == before_failed_command.checksum,
                "eligible interaction does not emit accepted feedback before command acceptance"
            );
            ok &= expect(harness.complete(accepted.objective, accepted.selection), "bell completes");
            const auto emission = emitter.interaction_feedback(accepted, true, harness.quest);
            ok &= expect(
                emission.signal.primary_result.status == QuestUiResultStatus::accepted,
                "accepted bell maps only after Quest acceptance"
            );
            ok &= project(
                emission,
                harness.quest,
                QuestUiAttemptTimeClassification::qualifying_craft_confirmation
            );
        }
    }

    {
        QuestHarness harness;
        const std::array selections{
            Selection{
                stable_content_key("f1_objective_choose_mooring_method"),
                stable_content_key("f1_choice_mooring_cross_belay"),
            },
        };
        ok &= expect(harness.start(), "cross-belay quest starts");
        ok &= expect(complete_prefix(harness, 0, 4, selections), "cross-belay is selected");
        const auto* lock = interaction_definition(
            stable_content_key("f1_interaction_lock_cross_belay")
        );
        if (lock != nullptr) {
            const auto accepted = resolve_attempt(*lock, harness.quest);
            ok &= expect(harness.complete(accepted.objective, accepted.selection), "cross-belay locks");
            ok &= project(
                emitter.interaction_feedback(accepted, true, harness.quest),
                harness.quest,
                QuestUiAttemptTimeClassification::qualifying_craft_decision
            );
        }
    }

    {
        QuestHarness harness;
        const std::array selections{
            Selection{
                stable_content_key("f1_objective_choose_mooring_method"),
                stable_content_key("f1_choice_mooring_quick_hitch"),
            },
        };
        ok &= expect(harness.start(), "quick-hitch quest starts");
        ok &= expect(complete_prefix(harness, 0, 4, selections), "quick hitch is committed");
        const tgd::contracts::QuestUiSelectionIntent intent{
            1,
            1,
            stable_content_key("f1_objective_choose_mooring_method"),
            stable_content_key("f1_interaction_choose_quick_hitch"),
            stable_content_key("f1_choice_mooring_quick_hitch"),
        };
        const auto emission = emitter.accepted_choice_feedback(intent, harness.quest);
        ok &= expect(
            emission.signal.objective == stable_content_key(
                                             "f1_objective_secure_ferry_mooring"
                                         ) &&
                emission.signal.primary_result.objective == intent.objective,
            "accepted choice keeps result owner while focusing the immediate active objective"
        );
        ok &= project(
            emission,
            harness.quest,
            QuestUiAttemptTimeClassification::qualifying_error_feedback
        );
    }
    return ok;
}

bool test_phase_events() {
    using tgd::contracts::QuestUiAttemptTimeClassification;
    F1QuestUiSignalEmitter emitter;
    bool ok = emitter.initialize(definition()) == F1QuestUiSignalEmitterError::none;

    const auto run_phase = [&](std::size_t completed_count, StableContentKey lane) {
        QuestHarness harness;
        const std::array selections{
            Selection{stable_content_key("f1_objective_choose_training_lane"), lane},
        };
        bool phase_ok = expect(harness.start(), "training phase quest starts") &&
                        expect(enter_training(harness), "training beat is entered") &&
                        expect(
                            complete_prefix(harness, 1, completed_count, selections),
                            "training phase prefix completes"
                        );
        const auto event = std::find_if(
            harness.sink.values.begin(),
            harness.sink.values.end(),
            [](const tgd::contracts::QuestEvent& candidate) {
                return candidate.type == tgd::contracts::QuestEventType::objective_completed;
            }
        );
        phase_ok &= expect(event != harness.sink.values.end(), "phase boundary emits QuestEvent");
        if (event == harness.sink.values.end()) {
            return phase_ok;
        }
        const auto emission = emitter.objective_state_after(*event, harness.quest);
        phase_ok &= expect(
            emission.signal.objective ==
                    definition().beats[1].objectives[completed_count].key &&
                harness.quest.objective_state(emission.signal.objective) ==
                    tgd::gameplay::QuestObjectiveState::active,
            "phase focus is the immediate authored Active objective"
        );
        phase_ok &= project(
            emission,
            harness.quest,
            QuestUiAttemptTimeClassification::qualifying_training_risk
        );
        return phase_ok;
    };

    ok &= run_phase(
        3,
        stable_content_key("f1_choice_training_windward_lane")
    );
    ok &= run_phase(
        9,
        stable_content_key("f1_choice_training_leeward_lane")
    );
    return ok;
}

[[nodiscard]] tgd::gameplay::QuestCombatTriggerResult accept_trigger(
    QuestHarness& harness,
    const tgd::contracts::QuestCombatTriggerDefinition& trigger
) {
    tgd::gameplay::DeterministicQuestCombatTriggerResolver resolver;
    if (resolver.initialize(definition().quest_combat_triggers) !=
        tgd::gameplay::QuestCombatTriggerError::none) {
        std::cerr << "F1 Quest UI emitter setup failure: combat trigger resolver init\n";
        std::abort();
    }
    const auto resolved = resolver.resolve(
        {
            definition().player.actor,
            trigger.kind,
            trigger.required_stance,
            trigger.required_ability,
        },
        harness.quest
    );
    if (!resolved.found || !harness.complete(resolved.objective)) {
        std::cerr << "F1 Quest UI emitter setup failure: trigger acceptance"
                  << " trigger=" << trigger.id.key
                  << " found=" << resolved.found
                  << " objective=" << resolved.objective
                  << " stage=" << harness.quest.snapshot().stage << '\n';
        std::abort();
    }
    return resolved;
}

[[nodiscard]] StableContentKey authored_hostile_with_other_archetype(
    StableContentKey archetype
) {
    const auto actor = std::find_if(
        combat_definition().actors.begin(),
        combat_definition().actors.end(),
        [archetype](const tgd::contracts::CombatActorConfig& candidate) {
            return candidate.faction == tgd::contracts::CombatFaction::hostile &&
                   candidate.archetype_id.key != archetype;
        }
    );
    return actor == combat_definition().actors.end() ? 0 : actor->actor;
}

[[nodiscard]] StableContentKey authored_hostile_with_archetype(
    StableContentKey archetype
) {
    const auto actor = std::find_if(
        combat_definition().actors.begin(),
        combat_definition().actors.end(),
        [archetype](const tgd::contracts::CombatActorConfig& candidate) {
            return candidate.faction == tgd::contracts::CombatFaction::hostile &&
                   candidate.archetype_id.key == archetype;
        }
    );
    return actor == combat_definition().actors.end() ? 0 : actor->actor;
}

[[nodiscard]] const tgd::contracts::CombatActorConfig* actor_config(
    StableContentKey actor
) {
    const auto found = std::find_if(
        combat_definition().actors.begin(),
        combat_definition().actors.end(),
        [actor](const tgd::contracts::CombatActorConfig& candidate) {
            return candidate.actor == actor;
        }
    );
    return found == combat_definition().actors.end() ? nullptr : &*found;
}

bool test_combat_feedback_events() {
    using tgd::contracts::QuestUiAttemptTimeClassification;
    using tgd::gameplay::QuestCombatOutcomeAttemptDisposition;
    F1QuestUiSignalEmitter emitter;
    bool ok = emitter.initialize(definition()) == F1QuestUiSignalEmitterError::none;

    {
        QuestHarness harness;
        const std::array selections{
            Selection{
                stable_content_key("f1_objective_choose_training_lane"),
                stable_content_key("f1_choice_training_windward_lane"),
            },
        };
        ok &= expect(harness.start(), "eavesguard combat quest starts");
        ok &= expect(enter_training(harness), "eavesguard enters training");
        ok &= expect(complete_prefix(harness, 1, 3, selections), "eavesguard phase is active");
        const auto* trigger = trigger_definition(
            stable_content_key("f1_trigger_eavesguard_counter")
        );
        ok &= expect(trigger != nullptr, "eavesguard trigger is authored");
        if (trigger != nullptr) {
            const auto accepted = accept_trigger(harness, *trigger);
            tgd::gameplay::DeterministicQuestCombatOutcomeAttemptResolver attempts;
            ok &= expect(
                attempts.initialize(definition()) ==
                    tgd::gameplay::QuestCombatOutcomeAttemptError::none,
                "combat attempt resolver initializes"
            );
            const tgd::contracts::CombatEvent event{
                harness.tick,
                tgd::contracts::CombatEventType::hit_guarded,
                104,
                definition().player.actor,
                0,
                0,
                tgd::contracts::feedback_guard,
            };
            const auto attempt = attempts.evaluate_attempt(
                accepted,
                event,
                {},
                harness.quest
            );
            ok &= expect(
                attempt.disposition == QuestCombatOutcomeAttemptDisposition::no_candidate,
                "eavesguard accepted trigger has no adjacent outcome candidate"
            );
            const auto emission = emitter.combat_feedback(accepted, attempt, harness.quest);
            ok &= expect(
                emission.signal.secondary_result.status ==
                    tgd::contracts::QuestUiResultStatus::not_applicable,
                "trigger-only feedback keeps the secondary slot canonical N/A"
            );
            ok &= project(
                emission,
                harness.quest,
                QuestUiAttemptTimeClassification::qualifying_combat_proof
            );
        }
    }

    {
        QuestHarness harness;
        const std::array selections{
            Selection{
                stable_content_key("f1_objective_choose_training_lane"),
                stable_content_key("f1_choice_training_leeward_lane"),
            },
        };
        ok &= expect(harness.start(), "flower combat quest starts");
        ok &= expect(enter_training(harness), "flower enters training");
        ok &= expect(complete_prefix(harness, 1, 11, selections), "flower heavy is pending");
        const auto* trigger = trigger_definition(
            stable_content_key("f1_trigger_flower_turn_heavy")
        );
        const auto* outcome = outcome_definition(
            stable_content_key("f1_outcome_break_flower_turn_target")
        );
        ok &= expect(trigger != nullptr && outcome != nullptr, "flower proof definitions exist");
        if (trigger != nullptr && outcome != nullptr) {
            const auto accepted = accept_trigger(harness, *trigger);
            const auto wrong_actor = authored_hostile_with_other_archetype(
                outcome->archetype_id.key
            );
            const auto* config = actor_config(wrong_actor);
            ok &= expect(config != nullptr, "wrong target is a Definition-authored hostile");
            if (config != nullptr) {
                tgd::contracts::CombatActorSnapshot actor;
                actor.actor = config->actor;
                actor.archetype = config->archetype_id.key;
                actor.faction = tgd::contracts::CombatFaction::hostile;
                actor.pose = config->initial_pose;
                actor.resources = config->initial_resources;
                actor.resources.health = std::max(1, actor.resources.health);
                actor.resources.health_max = std::max(actor.resources.health, 1);
                actor.stance = config->initial_stance;
                actor.active = true;
                actor.defeated = false;
                const tgd::contracts::CombatEvent event{
                    harness.tick,
                    tgd::contracts::CombatEventType::ability_started,
                    definition().player.actor,
                    actor.actor,
                    trigger->required_ability,
                    0,
                    tgd::contracts::feedback_heavy,
                };
                tgd::gameplay::DeterministicQuestCombatOutcomeAttemptResolver attempts;
                ok &= expect(
                    attempts.initialize(definition()) ==
                        tgd::gameplay::QuestCombatOutcomeAttemptError::none,
                    "flower attempt resolver initializes"
                );
                const auto before = harness.quest.snapshot();
                const auto attempt = attempts.evaluate_attempt(
                    accepted,
                    event,
                    std::span{&actor, 1U},
                    harness.quest
                );
                const auto repeated = attempts.evaluate_attempt(
                    accepted,
                    event,
                    std::span{&actor, 1U},
                    harness.quest
                );
                ok &= expect(
                    attempt.disposition == QuestCombatOutcomeAttemptDisposition::wrong_target &&
                        repeated.disposition == attempt.disposition &&
                        repeated.outcome == attempt.outcome,
                    "wrong target remains a stable pure-read result"
                );
                const auto emission = emitter.combat_feedback(
                    accepted,
                    attempt,
                    harness.quest
                );
                ok &= expect(
                    emission.signal.primary_result.status ==
                            tgd::contracts::QuestUiResultStatus::accepted &&
                        emission.signal.secondary_result.status ==
                            tgd::contracts::QuestUiResultStatus::rejected &&
                        emission.signal.secondary_result.rejection_reason ==
                            tgd::contracts::QuestUiRejectionReason::wrong_target,
                    "accepted action and rejected outcome remain separate result slots"
                );
                ok &= project(
                    emission,
                    harness.quest,
                    QuestUiAttemptTimeClassification::qualifying_combat_feedback,
                    std::span{&actor, 1U}
                );
                ok &= expect(
                    harness.quest.snapshot().checksum == before.checksum &&
                        harness.quest.objective_state(outcome->objective_id.key) ==
                            tgd::gameplay::QuestObjectiveState::active,
                    "wrong-target evaluation and emission never complete the pending outcome"
                );

                const auto matching_actor_key = authored_hostile_with_archetype(
                    outcome->archetype_id.key
                );
                const auto* matching_config = actor_config(matching_actor_key);
                ok &= expect(matching_config != nullptr, "pending proof target is authored");
                if (matching_config == nullptr) {
                    return false;
                }
                tgd::contracts::CombatActorSnapshot matching_actor;
                matching_actor.actor = matching_config->actor;
                matching_actor.archetype = matching_config->archetype_id.key;
                matching_actor.faction = tgd::contracts::CombatFaction::hostile;
                matching_actor.pose = matching_config->initial_pose;
                matching_actor.resources = matching_config->initial_resources;
                matching_actor.resources.health = std::max(1, matching_actor.resources.health);
                matching_actor.resources.health_max = std::max(
                    matching_actor.resources.health,
                    1
                );
                matching_actor.stance = matching_config->initial_stance;
                matching_actor.active = true;
                matching_actor.defeated = false;
                const auto matching_attempt = attempts.evaluate_attempt(
                    accepted,
                    tgd::contracts::CombatEvent{
                        event.tick,
                        event.type,
                        event.source,
                        matching_actor.actor,
                        event.ability,
                        event.value,
                        event.feedback_tags,
                    },
                    std::span{&matching_actor, 1U},
                    harness.quest
                );
                const auto matching_emission = emitter.combat_feedback(
                    accepted,
                    matching_attempt,
                    harness.quest
                );
                ok &= expect(
                    matching_attempt.disposition ==
                            QuestCombatOutcomeAttemptDisposition::target_matches_pending &&
                        matching_emission.error == F1QuestUiSignalEmitterError::none &&
                        !matching_emission.found,
                    "correct target produces no false failure projection and still does not complete"
                );
            }
        }
    }
    return ok;
}

bool test_recovery_events() {
    using tgd::contracts::QuestUiAttemptTimeClassification;
    F1QuestUiSignalEmitter emitter;
    bool ok = emitter.initialize(definition()) == F1QuestUiSignalEmitterError::none;

    const auto run_recovery = [&emitter](
                                  std::size_t completed_count,
                                  StableContentKey lane,
                                  tgd::contracts::QuestUiProjectionSource source,
                                  QuestUiAttemptTimeClassification classification
                              ) {
        QuestHarness harness;
        const std::array selections{
            Selection{stable_content_key("f1_objective_choose_training_lane"), lane},
        };
        bool recovery_ok = expect(harness.start(), "recovery quest starts") &&
                           expect(enter_training(harness), "recovery enters training") &&
                           expect(
                               complete_prefix(harness, 1, completed_count, selections),
                               "recovery completed prefix is built"
                           );
        const auto before = harness.quest.snapshot();
        const auto emission = emitter.recovery(source, harness.quest);
        recovery_ok &= expect(
            emission.signal.objective ==
                    definition().beats[1].objectives[completed_count].key &&
                harness.quest.objective_state(emission.signal.objective) ==
                    tgd::gameplay::QuestObjectiveState::active,
            "recovery focus is the first incomplete authored frontier"
        );
        recovery_ok &= project(emission, harness.quest, classification);
        recovery_ok &= expect(
            harness.quest.snapshot().completed_total == before.completed_total &&
                harness.quest.snapshot().selection_count == before.selection_count &&
                harness.quest.snapshot().checksum == before.checksum &&
                harness.quest.selected_option(
                    stable_content_key("f1_objective_choose_training_lane")
                ) == lane,
            "recovery emission preserves completed count, selection and Quest checksum"
        );
        return recovery_ok;
    };

    ok &= run_recovery(
        3,
        stable_content_key("f1_choice_training_windward_lane"),
        tgd::contracts::QuestUiProjectionSource::recovery_offer,
        QuestUiAttemptTimeClassification::failure_retry_excluded
    );
    ok &= run_recovery(
        9,
        stable_content_key("f1_choice_training_leeward_lane"),
        tgd::contracts::QuestUiProjectionSource::recovery_resume,
        QuestUiAttemptTimeClassification::resume_no_duplicate_progress
    );
    return ok;
}

bool test_fail_closed_shapes() {
    F1QuestUiSignalEmitter emitter;
    bool ok = emitter.initialize(definition()) == F1QuestUiSignalEmitterError::none;
    ok &= expect(
        emitter.initialize(definition()) == F1QuestUiSignalEmitterError::invalid_lifecycle,
        "emitter rejects reinitialization"
    );
    QuestHarness harness;
    ok &= expect(harness.start(), "negative quest starts");
    const auto before = harness.quest.snapshot();
    tgd::gameplay::QuestInteractionResult forged;
    forged.found = true;
    forged.interaction = stable_content_key("f1_interaction_sound_workshop_bell");
    forged.objective = stable_content_key("f1_objective_inspect_travel_writ");
    forged.kind = tgd::contracts::QuestInteractionKind::inspect;
    const auto rejected = emitter.interaction_feedback(forged, true, harness.quest);
    ok &= expect(
        rejected.error == F1QuestUiSignalEmitterError::invalid_result && !rejected.found,
        "mismatched raw result identity fails closed"
    );
    ok &= expect(
        harness.quest.snapshot().checksum == before.checksum,
        "failed emission cannot mutate Quest"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_interaction_feedback_events();
    ok &= test_phase_events();
    ok &= test_combat_feedback_events();
    ok &= test_recovery_events();
    ok &= test_fail_closed_shapes();
    return ok ? 0 : 1;
}
