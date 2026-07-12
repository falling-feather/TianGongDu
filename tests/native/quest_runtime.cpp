#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/quest_types.hpp>
#include <tgd/gameplay/quest_runtime.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using tgd::gameplay::DeterministicQuestRuntime;
using tgd::gameplay::DeterministicQuestInteractionResolver;
using tgd::gameplay::DeterministicQuestCombatTriggerResolver;
using tgd::gameplay::DeterministicQuestCombatOutcomeResolver;
using tgd::gameplay::DeterministicQuestBossPhaseResolver;
using tgd::gameplay::DeterministicQuestResolutionRewardResolver;
using tgd::gameplay::QuestError;
using tgd::gameplay::QuestCombatOutcomeError;
using tgd::gameplay::QuestCombatTriggerError;
using tgd::gameplay::QuestBossPhaseError;
using tgd::gameplay::QuestResolutionRewardError;
using tgd::gameplay::QuestInteractionError;
using tgd::gameplay::QuestLifecycle;
using tgd::gameplay::QuestObjectiveState;

class CollectingSink final : public tgd::gameplay::IQuestEventSink {
  public:
    void publish(std::span<const tgd::contracts::QuestEvent> events) noexcept override {
        values.insert(values.end(), events.begin(), events.end());
    }

    [[nodiscard]] bool contains(tgd::contracts::QuestEventType type) const {
        for (const auto& event : values) {
            if (event.type == type) {
                return true;
            }
        }
        return false;
    }

    std::vector<tgd::contracts::QuestEvent> values;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "quest runtime failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] const tgd::contracts::VerticalSliceDefinition& definition() {
    static tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* value = provider.find_vertical_slice(
        tgd::contracts::stable_content_key("f1_rainy_umbrella_trial")
    );
    if (value == nullptr) {
        std::abort();
    }
    return *value;
}

[[nodiscard]] const tgd::contracts::CombatEncounterDefinition& combat_definition() {
    static tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* value = provider.find_combat_encounter(
        tgd::contracts::stable_content_key("f1_encounter_umbrella_lane_bootstrap")
    );
    if (value == nullptr) {
        std::abort();
    }
    return *value;
}

[[nodiscard]] tgd::contracts::StableContentKey selection_for_objective(
    tgd::contracts::StableContentKey objective
) {
    const auto match = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [objective](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key == objective &&
                   interaction.kind == tgd::contracts::QuestInteractionKind::choose;
        }
    );
    return match == definition().quest_interactions.end() ? 0 : match->selection_id.key;
}

bool test_ordering_idempotency_and_lifecycle() {
    DeterministicQuestRuntime quest;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    const auto initial_checksum = quest.snapshot().checksum;
    const auto future = definition().beats[1].objectives.front().key;
    ok &= expect(
        quest.apply({1, definition().player.actor, 1, {}, future}, sink).error ==
            QuestError::objective_not_active,
        "future objectives cannot bypass the active stage"
    );
    ok &= expect(
        quest.snapshot().checksum == initial_checksum,
        "rejected future commands do not mutate quest state"
    );
    ok &= expect(
        quest.apply({1, definition().player.actor, 1, {}, 999}, sink).error ==
            QuestError::unknown_objective,
        "unknown objectives fail closed"
    );

    const auto& first = definition().beats.front();
    const auto first_result = quest.apply(
        {10, definition().player.actor, 1, {}, first.objectives.back().key},
        sink
    );
    ok &= expect(first_result.error == QuestError::none && first_result.accepted, "active objective completes");
    const auto duplicate = quest.apply(
        {10, definition().player.actor, 2, {}, first.objectives.back().key},
        sink
    );
    ok &= expect(
        duplicate.error == QuestError::none && !duplicate.accepted &&
            sink.contains(tgd::contracts::QuestEventType::objective_already_completed),
        "a new command for a completed objective is idempotent"
    );
    auto advanced = first_result;
    tgd::contracts::CommandSequence sequence = 3;
    tgd::contracts::TickIndex tick = 11;
    for (const auto& objective : first.objectives.first(first.objectives.size() - 1U)) {
        advanced = quest.apply(
            {tick++, definition().player.actor, sequence++, {}, objective.key},
            sink
        );
        ok &= expect(
            advanced.error == QuestError::none && advanced.accepted,
            "every authored opening objective completes exactly once"
        );
    }
    ok &= expect(
        advanced.error == QuestError::none && advanced.accepted && advanced.stage_advanced &&
            !advanced.quest_resolved,
        "completing the expanded opening objective group advances one stage"
    );
    ok &= expect(
        quest.snapshot().stage == definition().beats[1].id.key &&
            quest.objective_state(first.objectives.front().key) == QuestObjectiveState::completed &&
            quest.objective_state(definition().beats[2].objectives.front().key) ==
                QuestObjectiveState::locked,
        "snapshot exposes completed, active, and locked objective states"
    );
    ok &= expect(
        quest.apply(
                 {tick, definition().player.actor, sequence - 1U, {}, future},
                 sink
             ).error ==
            QuestError::stale_command_sequence,
        "reused command sequences are rejected"
    );
    ok &= expect(
        quest.apply({9, definition().player.actor, sequence, {}, future}, sink).error ==
            QuestError::tick_regressed,
        "quest events cannot move backward in simulation time"
    );
    ok &= expect(quest.pause() == QuestError::none, "quest pauses explicitly");
    ok &= expect(quest.resume() == QuestError::none, "quest resumes explicitly");
    return ok;
}

bool test_full_resolution_is_deterministic() {
    DeterministicQuestRuntime left;
    DeterministicQuestRuntime right;
    CollectingSink left_sink;
    CollectingSink right_sink;
    bool ok = left.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= right.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= left.start() == QuestError::none;
    ok &= right.start() == QuestError::none;
    tgd::contracts::CommandSequence sequence = 1;
    tgd::contracts::TickIndex tick = 1;
    tgd::contracts::StableContentKey last_objective = 0;
    tgd::contracts::StableContentKey last_selection = 0;
    for (const auto& stage : definition().beats) {
        for (auto objective = stage.objectives.rbegin(); objective != stage.objectives.rend(); ++objective) {
            const tgd::contracts::QuestCommand command{
                tick++,
                definition().player.actor,
                sequence++,
                tgd::contracts::QuestCommandType::complete_objective,
                objective->key,
                selection_for_objective(objective->key),
            };
            last_objective = objective->key;
            last_selection = command.selection;
            const auto left_result = left.apply(command, left_sink);
            const auto right_result = right.apply(command, right_sink);
            ok &= left_result.error == QuestError::none && right_result.error == QuestError::none;
            ok &= expect(
                left.snapshot().checksum == right.snapshot().checksum,
                "the same objective commands produce the same checksum"
            );
        }
    }
    ok &= expect(
        left.lifecycle() == QuestLifecycle::resolved && left.snapshot().resolved &&
            left.snapshot().completed_total > 0 &&
            left_sink.contains(tgd::contracts::QuestEventType::quest_resolved),
        "the final stage resolves the quest graph"
    );
    const auto duplicate = left.apply(
        {tick, definition().player.actor, sequence, {}, last_objective, last_selection},
        left_sink
    );
    ok &= expect(
        duplicate.error == QuestError::none && !duplicate.accepted &&
            duplicate.quest_resolved,
        "resolved graphs keep duplicate objective completion idempotent"
    );
    return ok;
}

bool test_invalid_definition_fails_closed() {
    const std::array duplicate_objectives{
        tgd::contracts::content_id("objective_duplicate"),
        tgd::contracts::content_id("objective_duplicate"),
    };
    const std::array stages{
        tgd::contracts::VerticalSliceBeatDefinition{
            tgd::contracts::content_id("stage_duplicate"),
            tgd::contracts::VerticalSliceBeatKind::exploration,
            1,
            tgd::contracts::content_id("cell_duplicate"),
            duplicate_objectives,
        },
    };
    auto invalid = definition();
    invalid.beats = stages;
    DeterministicQuestRuntime quest;
    bool ok = expect(
        quest.initialize(invalid, invalid.player.actor) == QuestError::duplicate_objective,
        "duplicate stable objective IDs fail definition validation"
    );
    DeterministicQuestRuntime wrong_actor;
    ok &= expect(
        wrong_actor.initialize(definition(), 999) == QuestError::invalid_definition,
        "quest ownership must match the definition player"
    );
    return ok;
}

bool test_scene_interactions_resolve_from_active_objectives() {
    DeterministicQuestRuntime quest;
    DeterministicQuestInteractionResolver interactions;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    ok &= expect(
        interactions.initialize(definition().quest_interactions) == QuestInteractionError::none,
        "generated scene interactions initialize once"
    );

    const auto initial = interactions.resolve(
        {
            definition().player.actor,
            definition().beats.front().cell_id.key,
            definition().player.initial_pose,
        },
        quest
    );
    ok &= expect(
        initial.error == QuestInteractionError::none && initial.found &&
            initial.interaction ==
                tgd::contracts::stable_content_key("f1_interaction_travel_writ") &&
            initial.objective ==
                tgd::contracts::stable_content_key("f1_objective_inspect_travel_writ"),
        "the closest active opening interaction resolves from authored content"
    );
    ok &= quest.apply(
              {1, definition().player.actor, 1, {}, initial.objective},
              sink
          ).error == QuestError::none;
    const auto consumed = interactions.resolve(
        {
            definition().player.actor,
            definition().beats.front().cell_id.key,
            definition().player.initial_pose,
        },
        quest
    );
    ok &= expect(
        consumed.found && consumed.objective != initial.objective &&
            consumed.objective ==
                tgd::contracts::stable_content_key("f1_objective_read_flood_marks"),
        "a completed interaction disappears and exposes the next readiness step"
    );

    auto wrong_floor_pose = definition().quest_interactions.back().pose;
    ++wrong_floor_pose.floor_layer;
    ok &= expect(
        !interactions
             .resolve(
                 {
                     definition().player.actor,
                     definition().quest_interactions.back().cell_id.key,
                     wrong_floor_pose,
                 },
                 quest
             )
             .found,
        "interactions do not leak across authored floor layers"
    );
    ok &= expect(
        interactions.resolve({0, definition().beats.front().cell_id.key, {}}, quest).error ==
            QuestInteractionError::invalid_query,
        "invalid actor queries fail closed"
    );
    return ok;
}

bool test_scene_interaction_ties_are_stable() {
    const auto& first = definition().beats.front();
    const auto shared_pose = definition().player.initial_pose;
    const std::array tied{
        tgd::contracts::QuestInteractionDefinition{
            tgd::contracts::content_id("interaction_tie_b"),
            tgd::contracts::QuestInteractionKind::inspect,
            first.cell_id,
            first.objectives.front(),
            {},
            shared_pose,
            1000,
        },
        tgd::contracts::QuestInteractionDefinition{
            tgd::contracts::content_id("interaction_tie_a"),
            tgd::contracts::QuestInteractionKind::operate,
            first.cell_id,
            first.objectives.back(),
            {},
            shared_pose,
            1000,
        },
    };
    DeterministicQuestRuntime quest;
    DeterministicQuestInteractionResolver interactions;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    ok &= interactions.initialize(tied) == QuestInteractionError::none;
    const auto resolved = interactions.resolve(
        {definition().player.actor, first.cell_id.key, shared_pose},
        quest
    );
    const auto expected = std::min(tied[0].id.key, tied[1].id.key);
    ok &= expect(
        resolved.found && resolved.interaction == expected,
        "equal-distance interactions use stable content ID ordering"
    );

    auto invalid = tied;
    invalid[1].objective_id = invalid[0].objective_id;
    DeterministicQuestInteractionResolver invalid_resolver;
    ok &= expect(
        invalid_resolver.initialize(invalid) == QuestInteractionError::invalid_definition,
        "duplicate interaction objectives fail definition validation"
    );
    return ok;
}

bool test_combat_signals_resolve_training_objectives() {
    DeterministicQuestRuntime quest;
    DeterministicQuestCombatTriggerResolver triggers;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    tgd::contracts::CommandSequence sequence = 1;
    for (const auto& objective : definition().beats.front().objectives) {
        ok &= quest.apply(
                  {sequence, definition().player.actor, sequence, {}, objective.key},
                  sink
              ).error == QuestError::none;
        ++sequence;
    }
    ok &= expect(
        quest.snapshot().stage_index == 1,
        "combat trigger tests enter the authored training beat"
    );
    ok &= quest.apply(
              {
                  sequence,
                  definition().player.actor,
                  sequence,
                  {},
                  definition().beats[1].objectives.front().key,
              },
              sink
          ).error == QuestError::none;
    ++sequence;
    ok &= expect(
        triggers.initialize(definition().quest_combat_triggers) ==
            QuestCombatTriggerError::none,
        "generated combat-to-quest bindings initialize once"
    );

    const auto premature_guard = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_hit_guarded,
            tgd::contracts::stable_content_key("stance_eavesguard"),
        },
        quest
    );
    ok &= expect(
        !premature_guard.found,
        "the guard drill stays locked until the eavesguard heavy is committed"
    );

    const auto wrong_heavy = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_eavesguard"),
            tgd::contracts::stable_content_key("ability_flower_light"),
        },
        quest
    );
    ok &= expect(!wrong_heavy.found, "training abilities require the authored stable ID");
    const auto heavy = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_eavesguard"),
            tgd::contracts::stable_content_key("ability_eavesguard_heavy"),
        },
        quest
    );
    ok &= expect(
        heavy.found && heavy.objective ==
                           tgd::contracts::stable_content_key(
                               "f1_objective_commit_eavesguard_heavy"
                           ),
        "an accepted eavesguard heavy resolves its authored drill"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, heavy.objective},
              sink
          ).error == QuestError::none;
    ++sequence;

    const auto wrong_stance = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_hit_guarded,
            tgd::contracts::stable_content_key("stance_flower_turn"),
        },
        quest
    );
    ok &= expect(!wrong_stance.found, "guard counters require the authored stance");
    const auto guarded = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_hit_guarded,
            tgd::contracts::stable_content_key("stance_eavesguard"),
        },
        quest
    );
    ok &= expect(
        guarded.found && guarded.objective ==
                             tgd::contracts::stable_content_key(
                                 "f1_objective_eavesguard_counter"
                             ),
        "a guarded player hit resolves the eavesguard training objective"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, guarded.objective},
              sink
          ).error == QuestError::none;
    ++sequence;
    ok &= expect(
        !triggers
             .resolve(
                 {
                     definition().player.actor,
                     tgd::contracts::QuestCombatTriggerKind::player_hit_guarded,
                     tgd::contracts::stable_content_key("stance_eavesguard"),
                 },
                 quest
             )
             .found,
        "completed counter objectives no longer consume combat signals"
    );

    const auto flower_stance = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_stance_changed,
            tgd::contracts::stable_content_key("stance_flower_turn"),
        },
        quest
    );
    ok &= expect(
        flower_stance.found && flower_stance.objective ==
                                   tgd::contracts::stable_content_key(
                                       "f1_objective_enter_flower_turn"
                                   ),
        "entering flower turn resolves the authored stance drill"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, flower_stance.objective},
              sink
          ).error == QuestError::none;
    ++sequence;

    const auto flower_light = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_flower_turn"),
            tgd::contracts::stable_content_key("ability_flower_light"),
        },
        quest
    );
    ok &= expect(
        flower_light.found && flower_light.objective ==
                                  tgd::contracts::stable_content_key(
                                      "f1_objective_commit_flower_turn_light"
                                  ),
        "an accepted flower light resolves its authored tempo drill"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, flower_light.objective},
              sink
          ).error == QuestError::none;
    ++sequence;

    const auto evaded = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_hit_evaded,
            tgd::contracts::stable_content_key("stance_flower_turn"),
        },
        quest
    );
    ok &= expect(
        evaded.found && evaded.objective ==
                            tgd::contracts::stable_content_key(
                                "f1_objective_flower_turn_counter"
                            ),
        "an evaded player hit resolves the flower-turn training objective"
    );
    const auto training_completed = quest.apply(
        {sequence, definition().player.actor, sequence, {}, evaded.objective},
        sink
    );
    ++sequence;
    ok &= expect(
        training_completed.error == QuestError::none &&
            training_completed.stage_advanced && quest.snapshot().stage_index == 2,
        "all five authored combat drills advance into the umbrella lane"
    );
    ok &= expect(
        triggers.resolve({0, {}, 0}, quest).error ==
            QuestCombatTriggerError::invalid_signal,
        "invalid combat signals fail closed"
    );

    std::array<tgd::contracts::QuestCombatTriggerDefinition, 5> invalid{};
    std::copy(
        definition().quest_combat_triggers.begin(),
        definition().quest_combat_triggers.end(),
        invalid.begin()
    );
    invalid[1].objective_id = invalid[0].objective_id;
    DeterministicQuestCombatTriggerResolver invalid_triggers;
    ok &= expect(
        invalid_triggers.initialize(invalid) == QuestCombatTriggerError::invalid_definition,
        "duplicate combat trigger objectives fail definition validation"
    );
    return ok;
}

bool test_hostile_group_outcomes_unlock_lane_choice() {
    DeterministicQuestRuntime quest;
    DeterministicQuestCombatOutcomeResolver outcomes;
    DeterministicQuestInteractionResolver interactions;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    tgd::contracts::CommandSequence sequence = 1;
    tgd::contracts::TickIndex tick = 1;
    for (std::size_t stage = 0; stage < 2; ++stage) {
        for (const auto& objective : definition().beats[stage].objectives) {
            ok &= quest.apply(
                      {tick++, definition().player.actor, sequence++, {}, objective.key},
                      sink
                  ).error == QuestError::none;
        }
    }
    ok &= expect(
        quest.snapshot().stage_index == 2,
        "hostile outcome tests enter the umbrella-lane beat"
    );
    ok &= outcomes.initialize(definition().quest_combat_outcomes) ==
          QuestCombatOutcomeError::none;
    ok &= interactions.initialize(definition().quest_interactions) ==
          QuestInteractionError::none;

    std::vector<tgd::contracts::CombatActorSnapshot> actors(
        combat_definition().actors.size()
    );
    for (std::size_t index = 0; index < combat_definition().actors.size(); ++index) {
        const auto& config = combat_definition().actors[index];
        actors[index] = {
            config.actor,
            config.archetype_id.key,
            config.faction,
            config.initial_pose,
            config.initial_resources,
            config.initial_stance,
            0,
            false,
            config.initially_active,
            false,
        };
    }
    for (const auto& placement :
         definition().quest_encounter_activations[2].actor_placements) {
        const auto actor = std::find_if(
            actors.begin(),
            actors.end(),
            [&placement](const tgd::contracts::CombatActorSnapshot& candidate) {
                return candidate.actor == placement.actor;
            }
        );
        if (actor != actors.end()) {
            actor->pose = placement.pose;
            actor->active = true;
            actor->defeated = false;
        }
    }
    ok &= expect(
        !outcomes.resolve(actors, quest).found,
        "active hostile groups do not complete combat objectives"
    );

    const auto route = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                   tgd::contracts::stable_content_key("f1_objective_choose_lane_route");
        }
    );
    ok &= expect(
        route != definition().quest_interactions.end(),
        "the authored lane choice exists"
    );
    if (route == definition().quest_interactions.end()) {
        return false;
    }
    ok &= expect(
        !interactions
             .resolve(
                 {definition().player.actor, route->cell_id.key, route->pose},
                 quest
             )
             .found,
        "the lane choice stays hidden while combat prerequisites are incomplete"
    );

    std::size_t defeated_dolls = 0;
    for (auto& actor : actors) {
        if (actor.archetype ==
                tgd::contracts::stable_content_key("jn_enemy_leaking_umbrella_doll") &&
            defeated_dolls < 2) {
            actor.resources.health = 0;
            actor.active = false;
            actor.defeated = true;
            ++defeated_dolls;
            const auto resolved = outcomes.resolve(actors, quest);
            if (defeated_dolls == 1) {
                ok &= expect(!resolved.found, "one of two dolls is not a completed group");
            } else {
                ok &= expect(
                    resolved.found && resolved.objective ==
                                          tgd::contracts::stable_content_key(
                                              "f1_objective_defeat_leaking_dolls"
                                          ),
                    "both defeated dolls resolve their group objective"
                );
                ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              resolved.objective,
                          },
                          sink
                      ).error == QuestError::none;
            }
        }
    }
    ok &= expect(
        !outcomes.resolve(actors, quest).found,
        "a dormant paper egret is not misclassified as defeated"
    );
    for (const auto& placement :
         definition().quest_encounter_activations[3].actor_placements) {
        const auto actor = std::find_if(
            actors.begin(),
            actors.end(),
            [&placement](const tgd::contracts::CombatActorSnapshot& candidate) {
                return candidate.actor == placement.actor;
            }
        );
        const auto config = std::find_if(
            combat_definition().actors.begin(),
            combat_definition().actors.end(),
            [&placement](const tgd::contracts::CombatActorConfig& candidate) {
                return candidate.actor == placement.actor;
            }
        );
        if (actor != actors.end() && config != combat_definition().actors.end()) {
            actor->resources = config->initial_resources;
            actor->pose = placement.pose;
            actor->active = true;
            actor->defeated = false;
        }
    }
    for (auto& actor : actors) {
        if (actor.archetype ==
            tgd::contracts::stable_content_key("jn_enemy_faded_paper_egret")) {
            actor.resources.health = 0;
            actor.active = false;
            actor.defeated = true;
        }
    }
    const auto egret = outcomes.resolve(actors, quest);
    ok &= expect(
        egret.found && egret.objective ==
                           tgd::contracts::stable_content_key(
                               "f1_objective_answer_paper_egret"
                           ),
        "the defeated paper egret resolves its authored answer objective"
    );
    ok &= quest.apply(
              {tick++, definition().player.actor, sequence++, {}, egret.objective},
              sink
          ).error == QuestError::none;
    const auto unlocked_route = interactions.resolve(
        {definition().player.actor, route->cell_id.key, route->pose},
        quest
    );
    ok &= expect(
        unlocked_route.found && unlocked_route.objective == route->objective_id.key &&
            unlocked_route.selection == route->selection_id.key,
        "completed hostile groups unlock the authored lane choice and stable option"
    );
    const auto checksum_before_missing_selection = quest.snapshot().checksum;
    ok &= expect(
        quest.apply(
                 {
                     tick++,
                     definition().player.actor,
                     sequence++,
                     {},
                     unlocked_route.objective,
                 },
                 sink
             )
                .error == QuestError::invalid_selection &&
            quest.snapshot().checksum == checksum_before_missing_selection,
        "choice objectives reject missing selections without mutating state"
    );
    const auto selected = quest.apply(
        {
            tick++,
            definition().player.actor,
            sequence++,
            {},
            unlocked_route.objective,
            unlocked_route.selection,
        },
        sink
    );
    ok &= expect(
        selected.error == QuestError::none && selected.accepted && selected.stage_advanced &&
            quest.snapshot().selection_count == 1 &&
            quest.selected_option(unlocked_route.objective) == unlocked_route.selection,
        "accepted choices persist one stable option and advance the beat"
    );
    const auto duplicate_choice = quest.apply(
        {
            tick++,
            definition().player.actor,
            sequence++,
            {},
            unlocked_route.objective,
            unlocked_route.selection,
        },
        sink
    );
    ok &= expect(
        duplicate_choice.error == QuestError::none && !duplicate_choice.accepted &&
            quest.snapshot().selection_count == 1,
        "repeating the same stable choice is idempotent"
    );

    const auto& workbench = definition().beats[3];
    for (std::size_t index = 0; index < 3; ++index) {
        const auto evidence = quest.apply(
            {
                tick++,
                definition().player.actor,
                sequence++,
                {},
                workbench.objectives[index].key,
            },
            sink
        );
        ok &= expect(
            evidence.error == QuestError::none && evidence.accepted &&
                !evidence.stage_advanced,
            "each workbench evidence trace commits without bypassing calibration"
        );
    }
    const auto spring_choice = tgd::contracts::stable_content_key(
        "f1_choice_rib_spring_calibration"
    );
    const auto winter_choice = tgd::contracts::stable_content_key(
        "f1_choice_rib_winter_calibration"
    );
    const auto calibration_objective = workbench.objectives[3].key;
    const auto calibrated = quest.apply(
        {
            tick++,
            definition().player.actor,
            sequence++,
            {},
            calibration_objective,
            spring_choice,
        },
        sink
    );
    ok &= expect(
        calibrated.error == QuestError::none && calibrated.accepted &&
            calibrated.stage_advanced && quest.snapshot().selection_count == 2 &&
            quest.selected_option(calibration_objective) == spring_choice,
        "workbench calibration persists a second stable choice"
    );
    const auto checksum_before_conflict = quest.snapshot().checksum;
    const auto conflict = quest.apply(
        {
            tick++,
            definition().player.actor,
            sequence++,
            {},
            calibration_objective,
            winter_choice,
        },
        sink
    );
    ok &= expect(
        conflict.error == QuestError::selection_conflict &&
            quest.snapshot().checksum == checksum_before_conflict &&
            quest.selected_option(calibration_objective) == spring_choice,
        "a later conflicting choice fails closed without rewriting history"
    );

    auto return_actors = actors;
    for (const auto& placement :
         definition().quest_encounter_activations[4].actor_placements) {
        for (auto& actor : return_actors) {
            if (actor.actor == placement.actor) {
                const auto config = std::find_if(
                    combat_definition().actors.begin(),
                    combat_definition().actors.end(),
                    [&placement](const tgd::contracts::CombatActorConfig& candidate) {
                        return candidate.actor == placement.actor;
                    }
                );
                if (config != combat_definition().actors.end()) {
                    actor.resources = config->initial_resources;
                }
                actor.pose = placement.pose;
                actor.active = true;
                actor.defeated = false;
            }
        }
    }
    ok &= expect(
        !outcomes.resolve(return_actors, quest).found,
        "reactivated return hostiles block calibration validation"
    );
    for (const auto& placement :
         definition().quest_encounter_activations[4].actor_placements) {
        for (auto& actor : return_actors) {
            if (actor.actor == placement.actor) {
                actor.resources.health = 0;
                actor.active = false;
                actor.defeated = true;
            }
        }
    }
    const auto return_clear = outcomes.resolve(return_actors, quest);
    ok &= expect(
        return_clear.found && return_clear.objective ==
                                  tgd::contracts::stable_content_key(
                                      "f1_objective_validate_calibration"
                                  ),
        "defeating every reactivated hostile validates the selected calibration"
    );
    ok &= quest.apply(
              {
                  tick++,
                  definition().player.actor,
                  sequence++,
                  {},
                  return_clear.objective,
              },
              sink
          ).error == QuestError::none;
    const auto shortcut = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                   tgd::contracts::stable_content_key(
                       "f1_objective_open_return_shortcut"
                   );
        }
    );
    ok &= expect(
        shortcut != definition().quest_interactions.end(),
        "the return shortcut interaction exists"
    );
    if (shortcut != definition().quest_interactions.end()) {
        const auto unlocked_shortcut = interactions.resolve(
            {definition().player.actor, shortcut->cell_id.key, shortcut->pose},
            quest
        );
        ok &= expect(
            unlocked_shortcut.found &&
                unlocked_shortcut.objective == shortcut->objective_id.key,
            "validated calibration unlocks the authored return shortcut"
        );
        ok &= expect(
            quest.apply(
                     {
                         tick++,
                         definition().player.actor,
                         sequence++,
                         {},
                         unlocked_shortcut.objective,
                     },
                     sink
                 )
                    .stage_advanced &&
                quest.snapshot().stage_index == 5,
            "opening the return shortcut advances into the boss beat"
        );
    }

    auto invalid_actors = actors;
    invalid_actors[1].actor = invalid_actors[0].actor;
    ok &= expect(
        outcomes.resolve(invalid_actors, quest).error ==
            QuestCombatOutcomeError::invalid_actor_snapshot,
        "duplicate actor snapshots fail closed"
    );
    auto invalid_lifecycle = actors;
    invalid_lifecycle[1].active = true;
    invalid_lifecycle[1].defeated = true;
    ok &= expect(
        outcomes.resolve(invalid_lifecycle, quest).error ==
            QuestCombatOutcomeError::invalid_actor_snapshot,
        "an actor cannot be active and defeated at the same time"
    );
    return ok;
}

bool test_four_seasons_boss_phases_are_ordered() {
    DeterministicQuestRuntime quest;
    DeterministicQuestBossPhaseResolver phases;
    DeterministicQuestResolutionRewardResolver rewards;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    tgd::contracts::TickIndex tick = 1;
    tgd::contracts::CommandSequence sequence = 1;
    for (std::size_t stage = 0; stage < 5; ++stage) {
        for (const auto& objective : definition().beats[stage].objectives) {
            const auto applied = quest.apply(
                {
                    tick++,
                    definition().player.actor,
                    sequence++,
                    {},
                    objective.key,
                    selection_for_objective(objective.key),
                },
                sink
            );
            ok &= applied.error == QuestError::none;
        }
    }
    ok &= expect(
        quest.snapshot().stage_index == 5,
        "boss phase tests enter the authored four-seasons beat"
    );
    ok &= expect(
        phases.initialize(definition().quest_boss_phases) == QuestBossPhaseError::none,
        "generated boss phase bindings initialize once"
    );

    std::vector<tgd::contracts::CombatActorSnapshot> actors(
        combat_definition().actors.size()
    );
    for (std::size_t index = 0; index < combat_definition().actors.size(); ++index) {
        const auto& config = combat_definition().actors[index];
        actors[index] = {
            config.actor,
            config.archetype_id.key,
            config.faction,
            config.initial_pose,
            config.initial_resources,
            config.initial_stance,
            0,
            false,
            config.initially_active,
            false,
        };
    }
    const auto boss = std::find_if(
        actors.begin(),
        actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) { return actor.actor == 201; }
    );
    ok &= expect(boss != actors.end(), "the boss actor exists in the combat snapshot");
    if (boss == actors.end()) {
        return false;
    }
    boss->active = true;
    ok &= expect(
        !phases.resolve(actors, quest).found,
        "full boss health does not complete the spring threshold"
    );
    ok &= expect(
        phases.resolve(std::span{actors}.first(4), quest).error ==
            QuestBossPhaseError::invalid_actor_snapshot,
        "a missing authored boss snapshot fails closed"
    );

    for (std::size_t index = 0; index < definition().quest_boss_phases.size(); ++index) {
        const auto& phase = definition().quest_boss_phases[index];
        boss->resources.health =
            boss->resources.health_max * static_cast<std::int32_t>(phase.health_percent) / 100;
        boss->active = boss->resources.health > 0;
        boss->defeated = boss->resources.health == 0;
        const auto resolved = phases.resolve(actors, quest);
        ok &= expect(
            resolved.error == QuestBossPhaseError::none && resolved.found &&
                resolved.phase == phase.id.key && resolved.objective == phase.objective_id.key &&
                resolved.actor == 201 && resolved.next_stance == phase.next_stance,
            "each boss threshold resolves exactly its next authored phase"
        );
        if (resolved.next_stance != 0) {
            boss->stance = resolved.next_stance;
        }
        const auto applied = quest.apply(
            {
                tick++,
                definition().player.actor,
                sequence++,
                {},
                resolved.objective,
            },
            sink
        );
        ok &= expect(
            applied.error == QuestError::none && applied.accepted &&
                applied.stage_advanced == (index + 1 == definition().quest_boss_phases.size()),
            "boss objectives advance only after all four ordered phases"
        );
    }
    ok &= expect(
        quest.snapshot().stage_index == 6,
        "defeating the winter phase advances into resolution"
    );
    ok &= expect(
        rewards.resolve(quest).error == QuestResolutionRewardError::invalid_lifecycle,
        "resolution rewards fail closed before initialization"
    );
    ok &= expect(
        rewards.initialize(definition().quest_resolution_rewards) ==
            QuestResolutionRewardError::none,
        "generated resolution reward receipts initialize once"
    );
    ok &= expect(
        !rewards.resolve(quest).found,
        "a reward receipt cannot resolve before the quest is fully returned"
    );

    const auto& resolution = definition().beats[6];
    const auto restore_shared_mark = tgd::contracts::stable_content_key(
        "f1_choice_resolution_restore_shared_mark"
    );
    const auto selected_resolution = quest.apply(
        {
            tick++,
            definition().player.actor,
            sequence++,
            {},
            resolution.objectives[0].key,
            restore_shared_mark,
        },
        sink
    );
    ok &= expect(
        selected_resolution.error == QuestError::none &&
            selected_resolution.accepted && !selected_resolution.quest_resolved &&
            quest.selected_option(resolution.objectives[0].key) == restore_shared_mark &&
            !rewards.resolve(quest).found,
        "the chosen resolution is stable but does not grant before returning to Shen Yan"
    );
    const auto returned = quest.apply(
        {
            tick++,
            definition().player.actor,
            sequence++,
            {},
            resolution.objectives[1].key,
        },
        sink
    );
    ok &= expect(
        returned.error == QuestError::none && returned.accepted &&
            returned.stage_advanced && returned.quest_resolved && quest.snapshot().resolved,
        "returning to Shen Yan resolves the full seven-beat quest"
    );
    const auto receipt = rewards.resolve(quest);
    const auto repeated_receipt = rewards.resolve(quest);
    ok &= expect(
        receipt.error == QuestResolutionRewardError::none && receipt.found &&
            receipt.resolution == tgd::contracts::stable_content_key(
                                      "f1_resolution_reward_restore_shared_mark"
                                  ) &&
            receipt.objective == resolution.objectives[0].key &&
            receipt.selection == restore_shared_mark &&
            receipt.reward == tgd::contracts::stable_content_key(
                                  "f1_reward_joint_workshop_formula"
                              ) &&
            receipt.reward_dedup_key == tgd::contracts::stable_content_key(
                                             "f1_claim_resolution_restore_shared_mark"
                                         ) &&
            repeated_receipt.resolution == receipt.resolution &&
            repeated_receipt.reward == receipt.reward &&
            repeated_receipt.reward_dedup_key == receipt.reward_dedup_key,
        "resolved choices produce the same idempotent reward receipt on every read"
    );

    auto invalid = std::array<tgd::contracts::QuestBossPhaseDefinition, 4>{};
    std::copy(
        definition().quest_boss_phases.begin(),
        definition().quest_boss_phases.end(),
        invalid.begin()
    );
    invalid[1].health_percent = 80;
    DeterministicQuestBossPhaseResolver invalid_phases;
    ok &= expect(
        invalid_phases.initialize(invalid) == QuestBossPhaseError::invalid_definition,
        "out-of-order boss thresholds fail definition validation"
    );
    auto invalid_rewards = std::array<tgd::contracts::QuestResolutionRewardDefinition, 2>{};
    std::copy(
        definition().quest_resolution_rewards.begin(),
        definition().quest_resolution_rewards.end(),
        invalid_rewards.begin()
    );
    invalid_rewards[1].reward_dedup_key = invalid_rewards[0].reward_dedup_key;
    DeterministicQuestResolutionRewardResolver invalid_reward_resolver;
    ok &= expect(
        invalid_reward_resolver.initialize(invalid_rewards) ==
            QuestResolutionRewardError::invalid_definition,
        "duplicate reward deduplication keys fail definition validation"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_ordering_idempotency_and_lifecycle();
    ok &= test_full_resolution_is_deterministic();
    ok &= test_invalid_definition_fails_closed();
    ok &= test_scene_interactions_resolve_from_active_objectives();
    ok &= test_scene_interaction_ties_are_stable();
    ok &= test_combat_signals_resolve_training_objectives();
    ok &= test_hostile_group_outcomes_unlock_lane_choice();
    ok &= test_four_seasons_boss_phases_are_ordered();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
