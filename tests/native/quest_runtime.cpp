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
using tgd::gameplay::DeterministicQuestCombatOutcomeAttemptResolver;
using tgd::gameplay::DeterministicQuestBossPhaseResolver;
using tgd::gameplay::DeterministicQuestResolutionRewardResolver;
using tgd::gameplay::QuestError;
using tgd::gameplay::QuestCombatOutcomeError;
using tgd::gameplay::QuestCombatOutcomeAttemptDisposition;
using tgd::gameplay::QuestCombatOutcomeAttemptError;
using tgd::gameplay::QuestCombatTriggerError;
using tgd::gameplay::QuestBossPhaseError;
using tgd::gameplay::QuestResolutionRewardError;
using tgd::gameplay::QuestInteractionAvailability;
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

[[nodiscard]] std::vector<tgd::contracts::CombatActorSnapshot>
combat_actor_snapshots() {
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
    return actors;
}

[[nodiscard]] bool combat_actor_snapshots_equal(
    std::span<const tgd::contracts::CombatActorSnapshot> left,
    std::span<const tgd::contracts::CombatActorSnapshot> right
) {
    return left.size() == right.size() && std::equal(
        left.begin(),
        left.end(),
        right.begin(),
        [](const tgd::contracts::CombatActorSnapshot& lhs,
           const tgd::contracts::CombatActorSnapshot& rhs) {
            return lhs.actor == rhs.actor && lhs.archetype == rhs.archetype &&
                   lhs.faction == rhs.faction && lhs.pose.x == rhs.pose.x &&
                   lhs.pose.y == rhs.pose.y && lhs.pose.height == rhs.pose.height &&
                   lhs.pose.floor_layer == rhs.pose.floor_layer &&
                   lhs.resources == rhs.resources && lhs.stance == rhs.stance &&
                   lhs.active_ability == rhs.active_ability &&
                   lhs.guarding == rhs.guarding && lhs.active == rhs.active &&
                   lhs.defeated == rhs.defeated;
        }
    );
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
    const auto skip_clue = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.id.key == tgd::contracts::stable_content_key(
                                             "f1_interaction_arrival_clue_follow_bell"
                                         );
        }
    );
    if (skip_clue == definition().quest_interactions.end()) {
        return false;
    }
    const auto consumed = interactions.resolve(
        {
            definition().player.actor,
            definition().beats.front().cell_id.key,
            skip_clue->pose,
        },
        quest
    );
    ok &= expect(
        consumed.found && consumed.objective != initial.objective &&
            consumed.objective ==
                tgd::contracts::stable_content_key("f1_objective_choose_arrival_clue") &&
            consumed.selection ==
                tgd::contracts::stable_content_key("f1_choice_arrival_follow_bell"),
        "the direct route skips optional clues without blocking the readiness path"
    );
    ok &= quest.apply(
              {
                  2,
                  definition().player.actor,
                  2,
                  {},
                  consumed.objective,
                  consumed.selection,
              },
              sink
          ).error == QuestError::none;
    const auto main_gauge = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.id.key == tgd::contracts::stable_content_key(
                                             "f1_interaction_read_main_flood_gauge"
                                         );
        }
    );
    if (main_gauge == definition().quest_interactions.end()) {
        return false;
    }
    const auto gated_condition = interactions.resolve(
        {definition().player.actor, main_gauge->cell_id.key, main_gauge->pose},
        quest
    );
    ok &= expect(
        gated_condition.found && gated_condition.objective ==
                                     tgd::contracts::stable_content_key(
                                         "f1_objective_read_ferry_condition"
                                     ),
        "the skip option converges on its authored main-gauge condition check"
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

bool test_interaction_attempt_resolution_is_read_only_and_fail_closed() {
    const auto find_interaction = [](tgd::contracts::StableContentKey id) {
        return std::find_if(
            definition().quest_interactions.begin(),
            definition().quest_interactions.end(),
            [id](const tgd::contracts::QuestInteractionDefinition& interaction) {
                return interaction.id.key == id;
            }
        );
    };
    const auto travel = find_interaction(
        tgd::contracts::stable_content_key("f1_interaction_travel_writ")
    );
    const auto drowned_manifest = find_interaction(
        tgd::contracts::stable_content_key(
            "f1_interaction_arrival_clue_drowned_manifest"
        )
    );
    const auto read_manifest = find_interaction(
        tgd::contracts::stable_content_key("f1_interaction_read_manifest_waterline")
    );
    const auto bell = find_interaction(
        tgd::contracts::stable_content_key("f1_interaction_sound_workshop_bell")
    );
    const auto future_training_choice = find_interaction(
        tgd::contracts::stable_content_key(
            "f1_interaction_choose_training_windward_lane"
        )
    );
    if (travel == definition().quest_interactions.end() ||
        drowned_manifest == definition().quest_interactions.end() ||
        read_manifest == definition().quest_interactions.end() ||
        bell == definition().quest_interactions.end() ||
        future_training_choice == definition().quest_interactions.end()) {
        return false;
    }

    DeterministicQuestRuntime quest;
    DeterministicQuestInteractionResolver interactions;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    ok &= expect(
        interactions.initialize(definition()) == QuestInteractionError::none,
        "attempt queries initialize from the complete authored quest definition"
    );

    const auto opening_before = quest.snapshot();
    const auto legacy_opening = interactions.resolve(
        {definition().player.actor, travel->cell_id.key, travel->pose},
        quest
    );
    const auto attempted_opening = interactions.resolve_attempt(
        {definition().player.actor, travel->cell_id.key, travel->pose},
        quest
    );
    ok &= expect(
        attempted_opening.error == QuestInteractionError::none &&
            attempted_opening.found &&
            attempted_opening.availability == QuestInteractionAvailability::eligible &&
            attempted_opening.interaction == legacy_opening.interaction &&
            attempted_opening.objective == legacy_opening.objective,
        "eligible attempt results preserve the existing resolver ordering and payload"
    );
    ok &= expect(
        quest.snapshot().checksum == opening_before.checksum &&
            quest.snapshot().tick == opening_before.tick,
        "eligible attempt queries do not mutate quest state"
    );

    const auto blocked_before = quest.snapshot();
    const auto blocked_bell = interactions.resolve_attempt(
        {definition().player.actor, bell->cell_id.key, bell->pose},
        quest
    );
    const auto blocked_bell_repeat = interactions.resolve_attempt(
        {definition().player.actor, bell->cell_id.key, bell->pose},
        quest
    );
    ok &= expect(
        blocked_bell.error == QuestInteractionError::none && blocked_bell.found &&
            blocked_bell.interaction == bell->id.key &&
            blocked_bell.objective == bell->objective_id.key &&
            blocked_bell.availability ==
                QuestInteractionAvailability::prerequisite_incomplete &&
            blocked_bell_repeat.interaction == blocked_bell.interaction &&
            blocked_bell_repeat.availability == blocked_bell.availability,
        "a current-Beat interaction with an incomplete prerequisite is reported deterministically"
    );
    ok &= expect(
        quest.snapshot().checksum == blocked_before.checksum &&
            quest.snapshot().tick == blocked_before.tick,
        "repeated prerequisite queries remain pure reads"
    );

    auto out_of_range_pose = bell->pose;
    out_of_range_pose.x += bell->radius_mm + 1;
    ok &= expect(
        !interactions
             .resolve_attempt(
                 {definition().player.actor, bell->cell_id.key, out_of_range_pose},
                 quest
             )
             .found,
        "a query one millimetre outside the authored radius is not an attempt candidate"
    );
    ok &= expect(
        interactions.resolve_attempt({0, bell->cell_id.key, bell->pose}, quest).error ==
                QuestInteractionError::invalid_query &&
            interactions
                    .resolve_attempt({definition().player.actor, 0, bell->pose}, quest)
                    .error == QuestInteractionError::invalid_query,
        "zero actor and cell attempt queries fail closed"
    );
    ok &= expect(
        !interactions
             .resolve_attempt(
                 {
                     definition().player.actor,
                     future_training_choice->cell_id.key,
                     future_training_choice->pose,
                 },
                 quest
             )
             .found,
        "future-Beat interactions are not surfaced as attempts"
    );
    ok &= expect(
        quest.snapshot().checksum == blocked_before.checksum &&
            quest.snapshot().completed_total == blocked_before.completed_total &&
            quest.snapshot().selection_count == blocked_before.selection_count,
        "invalid, out-of-range, and future-Beat attempt queries leave the quest unchanged"
    );

    tgd::contracts::TickIndex tick = 1;
    tgd::contracts::CommandSequence sequence = 1;
    ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      travel->objective_id.key,
                  },
                  sink
              ).error == QuestError::none;
    const auto completed_non_choice = interactions.resolve_attempt(
        {definition().player.actor, travel->cell_id.key, travel->pose},
        quest
    );
    ok &= expect(
        !completed_non_choice.found && quest.snapshot().completed_total == 1 &&
            quest.snapshot().selection_count == 0,
        "completed non-choice interactions are not misreported as selection repeats"
    );

    const auto arrival_choice =
        tgd::contracts::stable_content_key("f1_objective_choose_arrival_clue");
    const auto high_water =
        tgd::contracts::stable_content_key("f1_choice_arrival_high_water_tags");
    ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      arrival_choice,
                      high_water,
                  },
                  sink
              ).error == QuestError::none;
    const auto repeat_before = quest.snapshot();
    const auto repeated_choice = interactions.resolve_attempt(
        {
            definition().player.actor,
            drowned_manifest->cell_id.key,
            drowned_manifest->pose,
        },
        quest
    );
    ok &= expect(
        repeated_choice.error == QuestInteractionError::none && repeated_choice.found &&
            repeated_choice.interaction == drowned_manifest->id.key &&
            repeated_choice.objective == arrival_choice &&
            repeated_choice.availability ==
                QuestInteractionAvailability::selection_already_committed &&
            quest.selected_option(arrival_choice) == high_water,
        "touching another authored option reports the retained completed choice as a repeat"
    );
    ok &= expect(
        quest.snapshot().checksum == repeat_before.checksum &&
            quest.snapshot().selection_count == repeat_before.selection_count,
        "selection-repeat attempts preserve sequence, checksum, and the retained option"
    );
    ok &= expect(
        !interactions
             .resolve_attempt(
                 {definition().player.actor, read_manifest->cell_id.key, read_manifest->pose},
                 quest
             )
             .found,
        "required-selection mismatches are not surfaced as attempts"
    );
    ok &= expect(
        quest.snapshot().checksum == repeat_before.checksum &&
            quest.selected_option(arrival_choice) == high_water,
        "route-mismatch queries preserve the committed branch and checksum"
    );

    const auto read_code = tgd::contracts::stable_content_key(
        "f1_objective_read_workshop_bell_code"
    );
    ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      read_code,
                  },
                  sink
              ).error == QuestError::none;
    const auto eligible_bell = interactions.resolve_attempt(
        {definition().player.actor, bell->cell_id.key, bell->pose},
        quest
    );
    ok &= expect(
        eligible_bell.found && eligible_bell.interaction == bell->id.key &&
            eligible_bell.availability == QuestInteractionAvailability::eligible,
        "the same bell interaction becomes eligible after its authored prerequisite"
    );

    auto collocated_interactions =
        std::vector<tgd::contracts::QuestInteractionDefinition>{
            definition().quest_interactions.begin(),
            definition().quest_interactions.end(),
        };
    const auto collocated_travel = std::find_if(
        collocated_interactions.begin(),
        collocated_interactions.end(),
        [travel](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.id.key == travel->id.key;
        }
    );
    if (collocated_travel == collocated_interactions.end()) {
        return false;
    }
    collocated_travel->pose = bell->pose;
    collocated_travel->radius_mm = bell->radius_mm;
    auto collocated_definition = definition();
    collocated_definition.quest_interactions = collocated_interactions;
    DeterministicQuestRuntime collocated_quest;
    DeterministicQuestInteractionResolver collocated_resolver;
    ok &= collocated_quest.initialize(
              collocated_definition,
              collocated_definition.player.actor
          ) == QuestError::none;
    ok &= collocated_quest.start() == QuestError::none;
    ok &= collocated_resolver.initialize(collocated_definition) ==
          QuestInteractionError::none;
    const auto collocated = collocated_resolver.resolve_attempt(
        {
            collocated_definition.player.actor,
            bell->cell_id.key,
            bell->pose,
        },
        collocated_quest
    );
    ok &= expect(
        collocated.found && collocated.interaction == travel->id.key &&
            collocated.availability == QuestInteractionAvailability::eligible,
        "an eligible collocated candidate outranks an unavailable attempt candidate"
    );

    DeterministicQuestInteractionResolver legacy_only;
    ok &= legacy_only.initialize(definition().quest_interactions) ==
          QuestInteractionError::none;
    ok &= expect(
        legacy_only
                .resolve_attempt(
                    {definition().player.actor, travel->cell_id.key, travel->pose},
                    quest
                )
                .error == QuestInteractionError::invalid_lifecycle,
        "attempt resolution requires the additive full-definition initialization path"
    );

    auto invalid_interactions =
        std::vector<tgd::contracts::QuestInteractionDefinition>{
            definition().quest_interactions.begin(),
            definition().quest_interactions.end(),
        };
    invalid_interactions.front().objective_id =
        tgd::contracts::content_id("unknown_attempt_objective");
    auto invalid_definition = definition();
    invalid_definition.quest_interactions = invalid_interactions;
    DeterministicQuestInteractionResolver invalid_context;
    ok &= expect(
        invalid_context.initialize(invalid_definition) ==
                QuestInteractionError::invalid_definition &&
            invalid_context.initialize(definition()) == QuestInteractionError::none,
        "full-definition attempt initialization rejects unknown objectives without partial state"
    );

    auto mismatched_definition = definition();
    mismatched_definition.id = tgd::contracts::content_id("mismatched_quest_context");
    DeterministicQuestRuntime mismatched_quest;
    ok &= mismatched_quest.initialize(
              mismatched_definition,
              mismatched_definition.player.actor
          ) == QuestError::none;
    ok &= mismatched_quest.start() == QuestError::none;
    ok &= expect(
        interactions
                .resolve_attempt(
                    {definition().player.actor, travel->cell_id.key, travel->pose},
                    mismatched_quest
                )
                .error == QuestInteractionError::invalid_quest_context,
        "attempt queries reject a quest snapshot from another definition"
    );
    return ok;
}

bool test_rain_ferry_optional_clues_and_error_recovery_converge() {
    const auto clue_objective =
        tgd::contracts::stable_content_key("f1_objective_choose_arrival_clue");
    const auto condition_objective =
        tgd::contracts::stable_content_key("f1_objective_read_ferry_condition");
    const auto mooring_objective =
        tgd::contracts::stable_content_key("f1_objective_choose_mooring_method");
    const auto secured_objective =
        tgd::contracts::stable_content_key("f1_objective_secure_ferry_mooring");
    const std::array clue_routes{
        std::pair{
            tgd::contracts::stable_content_key("f1_choice_arrival_high_water_tags"),
            tgd::contracts::stable_content_key("f1_interaction_read_high_water_repairs")
        },
        std::pair{
            tgd::contracts::stable_content_key("f1_choice_arrival_drowned_manifest"),
            tgd::contracts::stable_content_key("f1_interaction_read_manifest_waterline")
        },
        std::pair{
            tgd::contracts::stable_content_key("f1_choice_arrival_follow_bell"),
            tgd::contracts::stable_content_key("f1_interaction_read_main_flood_gauge")
        },
    };
    const std::array mooring_routes{
        std::pair{
            tgd::contracts::stable_content_key("f1_choice_mooring_cross_belay"),
            tgd::contracts::stable_content_key("f1_interaction_lock_cross_belay")
        },
        std::pair{
            tgd::contracts::stable_content_key("f1_choice_mooring_quick_hitch"),
            tgd::contracts::stable_content_key(
                "f1_interaction_correct_overloaded_quick_hitch"
            )
        },
    };

    bool ok = true;
    for (const auto& [clue_selection, expected_condition] : clue_routes) {
        for (const auto& [mooring_selection, expected_mooring] : mooring_routes) {
            DeterministicQuestRuntime quest;
            DeterministicQuestInteractionResolver interactions;
            CollectingSink sink;
            ok &= quest.initialize(definition(), definition().player.actor) == QuestError::none;
            ok &= quest.start() == QuestError::none;
            ok &= interactions.initialize(definition().quest_interactions) ==
                  QuestInteractionError::none;
            tgd::contracts::TickIndex tick = 1;
            tgd::contracts::CommandSequence sequence = 1;
            ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              definition().beats[0].objectives[0].key,
                          },
                          sink
                      ).error == QuestError::none;
            ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              clue_objective,
                              clue_selection,
                          },
                          sink
                      ).error == QuestError::none;

            const tgd::contracts::QuestInteractionDefinition* condition = nullptr;
            for (const auto& candidate : definition().quest_interactions) {
                if (candidate.objective_id.key != condition_objective) {
                    continue;
                }
                const auto resolved = interactions.resolve(
                    {definition().player.actor, candidate.cell_id.key, candidate.pose},
                    quest
                );
                const bool selected = candidate.id.key == expected_condition;
                ok &= expect(
                    selected ? resolved.found && resolved.interaction == expected_condition
                             : !resolved.found,
                    "only the committed clue route exposes its condition reading"
                );
                if (selected) {
                    condition = &candidate;
                }
            }
            if (condition == nullptr) {
                return false;
            }
            ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              condition->objective_id.key,
                          },
                          sink
                      ).error == QuestError::none;
            ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              mooring_objective,
                              mooring_selection,
                          },
                          sink
                      ).error == QuestError::none;

            const tgd::contracts::QuestInteractionDefinition* mooring = nullptr;
            for (const auto& candidate : definition().quest_interactions) {
                if (candidate.objective_id.key != secured_objective) {
                    continue;
                }
                const auto resolved = interactions.resolve(
                    {definition().player.actor, candidate.cell_id.key, candidate.pose},
                    quest
                );
                const bool selected = candidate.id.key == expected_mooring;
                ok &= expect(
                    selected ? resolved.found && resolved.interaction == expected_mooring
                             : !resolved.found,
                    "cross-belay and quick-hitch correction remain mutually exclusive"
                );
                if (selected) {
                    mooring = &candidate;
                }
            }
            if (mooring == nullptr) {
                return false;
            }
            const auto secured = quest.apply(
                {
                    tick++,
                    definition().player.actor,
                    sequence++,
                    {},
                    mooring->objective_id.key,
                },
                sink
            );
            const auto repeated = quest.apply(
                {
                    tick++,
                    definition().player.actor,
                    sequence++,
                    {},
                    mooring->objective_id.key,
                },
                sink
            );
            ok &= expect(
                secured.accepted && !repeated.accepted &&
                    quest.selected_option(clue_objective) == clue_selection &&
                    quest.selected_option(mooring_objective) == mooring_selection,
                "each clue/method route converges once and repeated operation is idempotent"
            );
        }
    }
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
            {},
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
            {},
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

    auto invalid_interactions = std::vector<tgd::contracts::QuestInteractionDefinition>{
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
    };
    const auto spring_trace = std::find_if(
        invalid_interactions.begin(),
        invalid_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.id.key == tgd::contracts::stable_content_key(
                                             "f1_interaction_reveal_spring_trace"
                                         );
        }
    );
    if (spring_trace == invalid_interactions.end()) {
        return false;
    }
    spring_trace->required_selection_id = {};
    auto half_gate_definition = definition();
    half_gate_definition.quest_interactions = invalid_interactions;
    DeterministicQuestRuntime half_gate_runtime;
    ok &= expect(
        half_gate_runtime.initialize(half_gate_definition, definition().player.actor) ==
            QuestError::invalid_definition,
        "half-configured interaction selection gates fail quest definition validation"
    );

    invalid_interactions.assign(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end()
    );
    const auto spring_variants = std::array{
        tgd::contracts::stable_content_key("f1_choice_lane_canopy"),
        tgd::contracts::stable_content_key("f1_choice_lane_drain"),
    };
    for (std::size_t index = 0; index < spring_variants.size(); ++index) {
        const auto variant = std::find_if(
            invalid_interactions.begin(),
            invalid_interactions.end(),
            [selection = spring_variants[index]](
                const tgd::contracts::QuestInteractionDefinition& interaction
            ) {
                return interaction.objective_id.key == tgd::contracts::stable_content_key(
                                                          "f1_objective_reveal_spring_trace"
                                                      ) &&
                       interaction.required_selection_id.key == selection;
            }
        );
        if (variant == invalid_interactions.end()) {
            return false;
        }
        variant->required_selection_objective_id = tgd::contracts::content_id(
            "f1_objective_choose_resolution"
        );
        variant->required_selection_id = index == 0
                                             ? tgd::contracts::content_id(
                                                   "f1_choice_resolution_subdue"
                                               )
                                             : tgd::contracts::content_id(
                                                   "f1_choice_resolution_restore_shared_mark"
                                               );
    }
    auto future_gate_definition = definition();
    future_gate_definition.quest_interactions = invalid_interactions;
    DeterministicQuestRuntime future_gate_runtime;
    ok &= expect(
        future_gate_runtime.initialize(future_gate_definition, definition().player.actor) ==
            QuestError::invalid_definition,
        "interaction selection gates cannot depend on a future choice objective"
    );

    invalid_interactions.assign(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end()
    );
    const auto drain_variant = std::find_if(
        invalid_interactions.begin(),
        invalid_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.id.key == tgd::contracts::stable_content_key(
                                             "f1_interaction_reveal_spring_trace_from_drain"
                                         );
        }
    );
    if (drain_variant == invalid_interactions.end()) {
        return false;
    }
    invalid_interactions.erase(drain_variant);
    auto incomplete_gate_definition = definition();
    incomplete_gate_definition.quest_interactions = invalid_interactions;
    DeterministicQuestRuntime incomplete_gate_runtime;
    ok &= expect(
        incomplete_gate_runtime.initialize(
            incomplete_gate_definition,
            definition().player.actor
        ) == QuestError::invalid_definition,
        "interaction variants must cover every authored source choice"
    );
    return ok;
}

bool test_scene_interaction_selection_gates_follow_lane_route() {
    const auto lane_objective =
        tgd::contracts::stable_content_key("f1_objective_choose_lane_route");
    const auto spring_objective =
        tgd::contracts::stable_content_key("f1_objective_reveal_spring_trace");
    const auto canopy_selection =
        tgd::contracts::stable_content_key("f1_choice_lane_canopy");
    const auto drain_selection =
        tgd::contracts::stable_content_key("f1_choice_lane_drain");
    const auto canopy_interaction = tgd::contracts::stable_content_key(
        "f1_interaction_reveal_spring_trace"
    );
    const auto drain_interaction = tgd::contracts::stable_content_key(
        "f1_interaction_reveal_spring_trace_from_drain"
    );
    bool ok = true;
    for (const auto selected_route : std::array{canopy_selection, drain_selection}) {
        DeterministicQuestRuntime quest;
        DeterministicQuestInteractionResolver interactions;
        CollectingSink sink;
        ok &= quest.initialize(definition(), definition().player.actor) == QuestError::none;
        ok &= quest.start() == QuestError::none;
        ok &= interactions.initialize(definition().quest_interactions) ==
              QuestInteractionError::none;
        tgd::contracts::CommandSequence sequence = 1;
        tgd::contracts::TickIndex tick = 1;
        for (std::size_t stage = 0; stage < 2; ++stage) {
            for (const auto& objective : definition().beats[stage].objectives) {
                ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              objective.key,
                              selection_for_objective(objective.key),
                          },
                          sink
                      ).error == QuestError::none;
            }
        }
        for (std::size_t index = 0; index < 5; ++index) {
            ok &= quest.apply(
                      {
                          tick++,
                          definition().player.actor,
                          sequence++,
                          {},
                          definition().beats[2].objectives[index].key,
                      },
                      sink
                  ).error == QuestError::none;
        }
        ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      lane_objective,
                      selected_route,
                  },
                  sink
              ).error == QuestError::none;
        ok &= expect(
            quest.snapshot().stage_index == 3 &&
                quest.selected_option(lane_objective) == selected_route,
            "an authored lane route advances into the shared workbench beat"
        );

        for (const auto& candidate : definition().quest_interactions) {
            if (candidate.objective_id.key != spring_objective) {
                continue;
            }
            const auto resolved = interactions.resolve(
                {definition().player.actor, candidate.cell_id.key, candidate.pose},
                quest
            );
            const bool selected_variant = candidate.required_selection_id.key == selected_route;
            ok &= expect(
                selected_variant
                    ? resolved.found && resolved.interaction == candidate.id.key &&
                          resolved.objective == spring_objective
                    : !resolved.found,
                "only the spring-trace interaction for the committed lane route resolves"
            );
        }
        const auto expected = selected_route == canopy_selection ? canopy_interaction
                                                                 : drain_interaction;
        const auto selected_definition = std::find_if(
            definition().quest_interactions.begin(),
            definition().quest_interactions.end(),
            [expected](const tgd::contracts::QuestInteractionDefinition& interaction) {
                return interaction.id.key == expected;
            }
        );
        ok &= expect(
            selected_definition != definition().quest_interactions.end(),
            "each lane route owns a stable route-specific workbench interaction"
        );
    }
    return ok;
}

bool test_combat_signals_resolve_training_objectives() {
    DeterministicQuestRuntime quest;
    DeterministicQuestCombatTriggerResolver triggers;
    DeterministicQuestCombatOutcomeResolver outcomes;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    tgd::contracts::CommandSequence sequence = 1;
    for (const auto& objective : definition().beats.front().objectives) {
        ok &= quest.apply(
                  {
                      sequence,
                      definition().player.actor,
                      sequence,
                      {},
                      objective.key,
                      selection_for_objective(objective.key),
                  },
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
    ok &= expect(
        outcomes.initialize(definition().quest_combat_outcomes) ==
            QuestCombatOutcomeError::none,
        "generated target-defeat bindings initialize once"
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
        "the guard drill stays locked until one practice lane and mark are committed"
    );

    const auto training_lane = tgd::contracts::stable_content_key(
        "f1_choice_training_windward_lane"
    );
    ok &= quest.apply(
              {
                  sequence,
                  definition().player.actor,
                  sequence,
                  {},
                  definition().beats[1].objectives[1].key,
                  training_lane,
              },
              sink
          ).error == QuestError::none;
    ++sequence;
    ok &= quest.apply(
              {
                  sequence,
                  definition().player.actor,
                  sequence,
                  {},
                  definition().beats[1].objectives[2].key,
              },
              sink
          ).error == QuestError::none;
    ++sequence;

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
    const auto premature_heavy = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_eavesguard"),
            tgd::contracts::stable_content_key("ability_eavesguard_heavy"),
        },
        quest
    );
    ok &= expect(
        !premature_heavy.found,
        "the eavesguard heavy stays locked until a real hostile impact is guarded"
    );

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
        "guarding then committing the eavesguard heavy resolves the action chain"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, heavy.objective},
              sink
          ).error == QuestError::none;
    ++sequence;

    std::vector<tgd::contracts::CombatActorSnapshot> training_actors(
        combat_definition().actors.size()
    );
    for (std::size_t index = 0; index < combat_definition().actors.size(); ++index) {
        const auto& config = combat_definition().actors[index];
        training_actors[index] = {
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
    ok &= expect(
        !outcomes.resolve(training_actors, quest).found,
        "starting the proof target does not complete the eavesguard lesson"
    );
    const auto eavesguard_target = std::find_if(
        training_actors.begin(),
        training_actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) { return actor.actor == 107; }
    );
    if (eavesguard_target == training_actors.end()) {
        return false;
    }
    eavesguard_target->resources.health = 0;
    eavesguard_target->active = false;
    eavesguard_target->defeated = true;
    const auto eavesguard_clear = outcomes.resolve(training_actors, quest);
    ok &= expect(
        eavesguard_clear.found && eavesguard_clear.objective ==
                                       tgd::contracts::stable_content_key(
                                           "f1_objective_break_eavesguard_target"
                                       ),
        "the dedicated target defeat proves the eavesguard lesson"
    );
    ok &= quest.apply(
              {
                  sequence,
                  definition().player.actor,
                  sequence,
                  {},
                  eavesguard_clear.objective,
              },
              sink
          ).error == QuestError::none;
    ++sequence;
    ok &= quest.apply(
              {
                  sequence,
                  definition().player.actor,
                  sequence,
                  {},
                  definition().beats[1].objectives[6].key,
              },
              sink
          ).error == QuestError::none;
    ++sequence;

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

    ok &= quest.apply(
              {
                  sequence,
                  definition().player.actor,
                  sequence,
                  {},
                  definition().beats[1].objectives[8].key,
              },
              sink
          ).error == QuestError::none;
    ++sequence;

    const auto premature_flower_light = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_flower_turn"),
            tgd::contracts::stable_content_key("ability_flower_light"),
        },
        quest
    );
    ok &= expect(
        !premature_flower_light.found,
        "flower attacks stay locked until the active rig is crossed and evaded"
    );

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
    ok &= quest.apply(
        {sequence, definition().player.actor, sequence, {}, evaded.objective},
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
        "the evasion opens the authored flower-light response"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, flower_light.objective},
              sink
          ).error == QuestError::none;
    ++sequence;

    const auto flower_heavy = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_flower_turn"),
            tgd::contracts::stable_content_key("ability_flower_heavy"),
        },
        quest
    );
    ok &= expect(
        flower_heavy.found && flower_heavy.objective ==
                                  tgd::contracts::stable_content_key(
                                      "f1_objective_commit_flower_turn_heavy"
                                  ),
        "the flower-light response chains into the committed heavy finish"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, flower_heavy.objective},
              sink
          ).error == QuestError::none;
    ++sequence;

    const auto flower_target = std::find_if(
        training_actors.begin(),
        training_actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) { return actor.actor == 109; }
    );
    if (flower_target == training_actors.end()) {
        return false;
    }
    flower_target->resources.health = 0;
    flower_target->active = false;
    flower_target->defeated = true;
    const auto flower_clear = outcomes.resolve(training_actors, quest);
    ok &= expect(
        flower_clear.found && flower_clear.objective ==
                                  tgd::contracts::stable_content_key(
                                      "f1_objective_break_flower_turn_target"
                                  ),
        "the dedicated flower target must actually be defeated"
    );
    ok &= quest.apply(
              {sequence, definition().player.actor, sequence, {}, flower_clear.objective},
              sink
          ).error == QuestError::none;
    ++sequence;
    const auto training_completed = quest.apply(
        {
            sequence,
            definition().player.actor,
            sequence,
            {},
            definition().beats[1].objectives[13].key,
        },
        sink
    );
    ++sequence;
    ok &= expect(
        training_completed.error == QuestError::none &&
            training_completed.stage_advanced && quest.snapshot().stage_index == 2,
        "dialogue, lane choice, risk signals, action chains, and two clears finish training"
    );
    ok &= expect(
        triggers.resolve({0, {}, 0}, quest).error ==
            QuestCombatTriggerError::invalid_signal,
        "invalid combat signals fail closed"
    );

    std::vector<tgd::contracts::QuestCombatTriggerDefinition> invalid(
        definition().quest_combat_triggers.begin(),
        definition().quest_combat_triggers.end()
    );
    invalid[1] = invalid[0];
    invalid[1].id = tgd::contracts::content_id(
        "f1_trigger_duplicate_eavesguard_heavy"
    );
    DeterministicQuestCombatTriggerResolver invalid_triggers;
    ok &= expect(
        invalid_triggers.initialize(invalid) == QuestCombatTriggerError::invalid_definition,
        "duplicate combat trigger objectives fail definition validation"
    );
    std::vector<tgd::contracts::QuestCombatTriggerDefinition> half_selection(
        definition().quest_combat_triggers.begin(),
        definition().quest_combat_triggers.end()
    );
    half_selection[6].required_selection_id = {};
    DeterministicQuestCombatTriggerResolver half_selection_triggers;
    ok &= expect(
        half_selection_triggers.initialize(half_selection) ==
            QuestCombatTriggerError::invalid_definition,
        "combat trigger selection gates require both objective and option"
    );
    std::vector<tgd::contracts::QuestCombatTriggerDefinition> duplicate_selection(
        definition().quest_combat_triggers.begin(),
        definition().quest_combat_triggers.end()
    );
    duplicate_selection[7].required_selection_id =
        duplicate_selection[6].required_selection_id;
    DeterministicQuestCombatTriggerResolver duplicate_selection_triggers;
    ok &= expect(
        duplicate_selection_triggers.initialize(duplicate_selection) ==
            QuestCombatTriggerError::invalid_definition,
        "combat trigger variants cannot duplicate one selection option"
    );
    return ok;
}

bool test_combat_outcome_attempts_are_target_specific_and_read_only() {
    DeterministicQuestRuntime quest;
    DeterministicQuestCombatTriggerResolver triggers;
    DeterministicQuestCombatOutcomeResolver outcomes;
    DeterministicQuestCombatOutcomeAttemptResolver attempts;
    DeterministicQuestResolutionRewardResolver rewards;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    ok &= triggers.initialize(definition().quest_combat_triggers) ==
          QuestCombatTriggerError::none;
    ok &= outcomes.initialize(definition().quest_combat_outcomes) ==
          QuestCombatOutcomeError::none;
    ok &= rewards.initialize(definition().quest_resolution_rewards) ==
          QuestResolutionRewardError::none;

    const tgd::contracts::CombatEvent empty_event{};
    ok &= expect(
        attempts.evaluate_attempt({}, empty_event, {}, quest).error ==
            QuestCombatOutcomeAttemptError::invalid_lifecycle,
        "combat outcome attempts require full-definition initialization"
    );
    ok &= expect(
        attempts.initialize(definition()) == QuestCombatOutcomeAttemptError::none &&
            attempts.initialize(definition()) ==
                QuestCombatOutcomeAttemptError::invalid_lifecycle,
        "combat outcome attempt definitions initialize exactly once"
    );

    tgd::contracts::TickIndex tick = 1;
    tgd::contracts::CommandSequence sequence = 1;
    for (const auto& objective : definition().beats.front().objectives) {
        ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      objective.key,
                      selection_for_objective(objective.key),
                  },
                  sink
              ).error == QuestError::none;
    }
    const auto& training = definition().beats[1];
    for (std::size_t index = 0; index < 3; ++index) {
        ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      training.objectives[index].key,
                      selection_for_objective(training.objectives[index].key),
                  },
                  sink
              ).error == QuestError::none;
    }

    const auto guarded = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_hit_guarded,
            tgd::contracts::stable_content_key("stance_eavesguard"),
        },
        quest
    );
    ok &= expect(
        guarded.error == QuestCombatTriggerError::none && guarded.found,
        "the eavesguard counter produces an accepted authored trigger result"
    );
    ok &= quest.apply(
              {
                  tick++,
                  definition().player.actor,
                  sequence++,
                  {},
                  guarded.objective,
              },
              sink
          ).error == QuestError::none;
    const auto guard_before = quest.snapshot();
    const auto no_candidate = attempts.evaluate_attempt(
        guarded,
        {
            tick,
            tgd::contracts::CombatEventType::hit_guarded,
            104,
            definition().player.actor,
        },
        {},
        quest
    );
    ok &= expect(
        no_candidate.error == QuestCombatOutcomeAttemptError::none &&
            !no_candidate.found &&
            no_candidate.disposition ==
                QuestCombatOutcomeAttemptDisposition::no_candidate,
        "a counter whose immediate next objective is not an outcome reports no candidate"
    );
    ok &= expect(
        quest.snapshot().checksum == guard_before.checksum &&
            quest.snapshot().completed_total == guard_before.completed_total,
        "no-candidate evaluation does not require a hostile snapshot or mutate the quest"
    );
    auto forged_guard_event = tgd::contracts::CombatEvent{
        tick,
        tgd::contracts::CombatEventType::hit_guarded,
        999'999,
        definition().player.actor,
    };
    ok &= expect(
        attempts.evaluate_attempt(guarded, forged_guard_event, {}, quest).error ==
            QuestCombatOutcomeAttemptError::invalid_signal,
        "defensive evidence rejects a source not authored by the complete Definition"
    );

    for (std::size_t index = 4; index <= 10; ++index) {
        ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      training.objectives[index].key,
                  },
                  sink
              ).error == QuestError::none;
    }
    const auto flower_heavy = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_flower_turn"),
            tgd::contracts::stable_content_key("ability_flower_heavy"),
        },
        quest
    );
    ok &= expect(
        flower_heavy.error == QuestCombatTriggerError::none && flower_heavy.found,
        "the flower heavy produces an accepted authored trigger result"
    );
    ok &= quest.apply(
              {
                  tick++,
                  definition().player.actor,
                  sequence++,
                  {},
                  flower_heavy.objective,
              },
              sink
          ).error == QuestError::none;

    auto actors = combat_actor_snapshots();
    for (auto& actor : actors) {
        if (actor.actor == 108 || actor.actor == 109) {
            actor.active = true;
            actor.defeated = false;
        }
    }
    const auto wrong_target_event = tgd::contracts::CombatEvent{
        tick,
        tgd::contracts::CombatEventType::ability_started,
        definition().player.actor,
        108,
        tgd::contracts::stable_content_key("ability_flower_heavy"),
    };
    const auto attempt_before = quest.snapshot();
    const auto actors_before = actors;
    const auto reward_before = rewards.resolve(quest);
    const auto wrong_target = attempts.evaluate_attempt(
        flower_heavy,
        wrong_target_event,
        actors,
        quest
    );
    const auto repeated_wrong_target = attempts.evaluate_attempt(
        flower_heavy,
        wrong_target_event,
        actors,
        quest
    );
    ok &= expect(
        wrong_target.error == QuestCombatOutcomeAttemptError::none &&
            wrong_target.found &&
            wrong_target.outcome == tgd::contracts::stable_content_key(
                                        "f1_outcome_break_flower_turn_target"
                                    ) &&
            wrong_target.objective == training.objectives[12].key &&
            wrong_target.disposition ==
                QuestCombatOutcomeAttemptDisposition::wrong_target,
        "an accepted attack on another authored hostile reports the adjacent proof target mismatch"
    );
    ok &= expect(
        repeated_wrong_target.error == wrong_target.error &&
            repeated_wrong_target.found == wrong_target.found &&
            repeated_wrong_target.outcome == wrong_target.outcome &&
            repeated_wrong_target.objective == wrong_target.objective &&
            repeated_wrong_target.disposition == wrong_target.disposition,
        "repeating the same wrong-target attempt returns the same result"
    );
    const auto reward_after_wrong_target = rewards.resolve(quest);
    ok &= expect(
        quest.snapshot().tick == attempt_before.tick &&
            quest.snapshot().checksum == attempt_before.checksum &&
            quest.snapshot().completed_total == attempt_before.completed_total &&
            quest.snapshot().selection_count == attempt_before.selection_count &&
            combat_actor_snapshots_equal(actors, actors_before) &&
            reward_before.found == reward_after_wrong_target.found &&
            reward_before.reward == reward_after_wrong_target.reward &&
            reward_before.reward_dedup_key == reward_after_wrong_target.reward_dedup_key,
        "repeated wrong-target evidence changes no Quest, Combat, or reward state"
    );

    auto correct_target_event = wrong_target_event;
    correct_target_event.target = 109;
    const auto correct_target = attempts.evaluate_attempt(
        flower_heavy,
        correct_target_event,
        actors,
        quest
    );
    ok &= expect(
        correct_target.error == QuestCombatOutcomeAttemptError::none &&
            correct_target.found && correct_target.outcome == wrong_target.outcome &&
            correct_target.objective == wrong_target.objective &&
            correct_target.disposition ==
                QuestCombatOutcomeAttemptDisposition::target_matches_pending &&
            quest.objective_state(correct_target.objective) == QuestObjectiveState::active &&
            !outcomes.resolve(actors, quest).found,
        "a matching active target remains pending until the existing outcome resolver sees defeat"
    );
    ok &= expect(
        quest.snapshot().checksum == attempt_before.checksum &&
            combat_actor_snapshots_equal(actors, actors_before),
        "correct-target attempt evidence is also a pure read"
    );

    auto defeated_target = actors;
    const auto flower_target = std::find_if(
        defeated_target.begin(),
        defeated_target.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == 109;
        }
    );
    if (flower_target == defeated_target.end()) {
        return false;
    }
    flower_target->resources.health = 0;
    flower_target->active = false;
    flower_target->defeated = true;
    const auto completed_outcome = outcomes.resolve(defeated_target, quest);
    ok &= expect(
        completed_outcome.error == QuestCombatOutcomeError::none &&
            completed_outcome.found && completed_outcome.outcome == correct_target.outcome,
        "only the existing combat outcome resolver reports the defeated proof target"
    );
    ok &= expect(
        quest.objective_state(correct_target.objective) == QuestObjectiveState::active,
        "reporting a satisfied outcome still requires an explicit Quest command to progress"
    );

    ok &= expect(
        attempts.evaluate_attempt({}, wrong_target_event, actors, quest).error ==
            QuestCombatOutcomeAttemptError::invalid_signal,
        "missing accepted trigger results fail closed"
    );
    auto mismatched_result = flower_heavy;
    mismatched_result.objective = training.objectives[10].key;
    ok &= expect(
        attempts.evaluate_attempt(mismatched_result, wrong_target_event, actors, quest).error ==
            QuestCombatOutcomeAttemptError::invalid_signal,
        "trigger IDs cannot be paired with a different objective"
    );
    auto rejected_result = flower_heavy;
    rejected_result.error = QuestCombatTriggerError::invalid_signal;
    ok &= expect(
        attempts.evaluate_attempt(rejected_result, wrong_target_event, actors, quest).error ==
            QuestCombatOutcomeAttemptError::invalid_signal,
        "a rejected trigger result cannot be presented as accepted history"
    );

    auto wrong_kind_event = wrong_target_event;
    wrong_kind_event.type = tgd::contracts::CombatEventType::hit_guarded;
    auto wrong_source_event = wrong_target_event;
    wrong_source_event.source = 104;
    auto wrong_ability_event = wrong_target_event;
    wrong_ability_event.ability =
        tgd::contracts::stable_content_key("ability_flower_light");
    auto zero_target_event = wrong_target_event;
    zero_target_event.target = 0;
    ok &= expect(
        attempts.evaluate_attempt(flower_heavy, wrong_kind_event, actors, quest).error ==
                QuestCombatOutcomeAttemptError::invalid_signal &&
            attempts.evaluate_attempt(flower_heavy, wrong_source_event, actors, quest).error ==
                QuestCombatOutcomeAttemptError::invalid_signal &&
            attempts.evaluate_attempt(flower_heavy, wrong_ability_event, actors, quest).error ==
                QuestCombatOutcomeAttemptError::invalid_signal &&
            attempts.evaluate_attempt(flower_heavy, zero_target_event, actors, quest).error ==
                QuestCombatOutcomeAttemptError::invalid_signal,
        "event kind, source, ability, and candidate target must match the accepted trigger"
    );

    auto unknown_target_event = wrong_target_event;
    unknown_target_event.target = 999'999;
    auto unauthorized_actors = actors;
    auto unauthorized_actor = actors.front();
    unauthorized_actor.actor = unknown_target_event.target;
    unauthorized_actor.archetype = tgd::contracts::stable_content_key(
        "f1_training_flower_turn_rig"
    );
    unauthorized_actor.faction = tgd::contracts::CombatFaction::hostile;
    unauthorized_actor.active = true;
    unauthorized_actor.defeated = false;
    unauthorized_actors.push_back(unauthorized_actor);
    ok &= expect(
        attempts.evaluate_attempt(flower_heavy, unknown_target_event, actors, quest).error ==
                QuestCombatOutcomeAttemptError::invalid_actor_snapshot &&
            attempts
                    .evaluate_attempt(
                        flower_heavy,
                        unknown_target_event,
                        unauthorized_actors,
                        quest
                    )
                    .error == QuestCombatOutcomeAttemptError::invalid_actor_snapshot,
        "missing and unauthorised target actors fail closed"
    );

    auto invalid_target_actors = actors;
    const auto invalid_target = std::find_if(
        invalid_target_actors.begin(),
        invalid_target_actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == 108;
        }
    );
    if (invalid_target == invalid_target_actors.end()) {
        return false;
    }
    invalid_target->resources.health = 0;
    invalid_target->active = false;
    invalid_target->defeated = true;
    auto wrong_faction_actors = actors;
    const auto wrong_faction_target = std::find_if(
        wrong_faction_actors.begin(),
        wrong_faction_actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == 108;
        }
    );
    if (wrong_faction_target == wrong_faction_actors.end()) {
        return false;
    }
    wrong_faction_target->faction = tgd::contracts::CombatFaction::player;
    auto duplicate_target_actors = actors;
    duplicate_target_actors.push_back(*std::find_if(
        actors.begin(),
        actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == 108;
        }
    ));
    ok &= expect(
        attempts
                .evaluate_attempt(
                    flower_heavy,
                    wrong_target_event,
                    invalid_target_actors,
                    quest
                )
                .error == QuestCombatOutcomeAttemptError::invalid_actor_snapshot &&
            attempts
                    .evaluate_attempt(
                        flower_heavy,
                        wrong_target_event,
                        wrong_faction_actors,
                        quest
                    )
                    .error == QuestCombatOutcomeAttemptError::invalid_actor_snapshot &&
            attempts
                    .evaluate_attempt(
                        flower_heavy,
                        wrong_target_event,
                        duplicate_target_actors,
                        quest
                    )
                    .error == QuestCombatOutcomeAttemptError::invalid_actor_snapshot,
        "inactive, defeated, wrong-faction, and duplicate target snapshots fail closed"
    );

    auto other_definition = definition();
    other_definition.id = tgd::contracts::content_id("other_quest_definition");
    DeterministicQuestRuntime other_quest;
    ok &= other_quest.initialize(other_definition, other_definition.player.actor) ==
          QuestError::none;
    ok &= other_quest.start() == QuestError::none;
    ok &= expect(
        attempts.evaluate_attempt(flower_heavy, wrong_target_event, actors, other_quest).error ==
            QuestCombatOutcomeAttemptError::invalid_quest_context,
        "accepted trigger evidence cannot be replayed against another quest definition"
    );

    auto invalid_outcomes = std::vector<tgd::contracts::QuestCombatOutcomeDefinition>{
        definition().quest_combat_outcomes.begin(),
        definition().quest_combat_outcomes.end(),
    };
    invalid_outcomes.front().objective_id =
        tgd::contracts::content_id("unknown_outcome_attempt_objective");
    auto invalid_definition = definition();
    invalid_definition.quest_combat_outcomes = invalid_outcomes;
    DeterministicQuestCombatOutcomeAttemptResolver retryable;
    ok &= expect(
        retryable.initialize(invalid_definition) ==
                QuestCombatOutcomeAttemptError::invalid_definition &&
            retryable.initialize(definition()) == QuestCombatOutcomeAttemptError::none,
        "invalid cross-references fail initialization without leaving partial state"
    );
    ok &= expect(
        quest.snapshot().checksum == attempt_before.checksum &&
            combat_actor_snapshots_equal(actors, actors_before),
        "all negative attempt queries leave the last valid Quest and Combat snapshots intact"
    );
    return ok;
}

bool test_hostile_group_outcomes_unlock_lane_choice() {
    DeterministicQuestRuntime quest;
    DeterministicQuestCombatTriggerResolver triggers;
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
                      {
                          tick++,
                          definition().player.actor,
                          sequence++,
                          {},
                          objective.key,
                          selection_for_objective(objective.key),
                      },
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
    ok &= triggers.initialize(definition().quest_combat_triggers) ==
          QuestCombatTriggerError::none;
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
         definition().quest_encounter_activations[6].actor_placements) {
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
    for (std::size_t objective_index = 1; objective_index <= 3; ++objective_index) {
        const auto objective = definition().beats[2].objectives[objective_index].key;
        const auto rainworks_interaction = std::find_if(
            definition().quest_interactions.begin(),
            definition().quest_interactions.end(),
            [objective](const tgd::contracts::QuestInteractionDefinition& interaction) {
                return interaction.objective_id.key == objective;
            }
        );
        ok &= expect(
            rainworks_interaction != definition().quest_interactions.end(),
            "each umbrella-lane rainworks objective owns an authored interaction"
        );
        if (rainworks_interaction == definition().quest_interactions.end()) {
            return false;
        }
        ok &= expect(
            !interactions
                 .resolve(
                     {definition().player.actor, route->cell_id.key, route->pose},
                     quest
                 )
                 .found,
            "the lane choice stays hidden until every rainworks step and combat outcome completes"
        );
        const auto resolved_rainworks = interactions.resolve(
            {
                definition().player.actor,
                rainworks_interaction->cell_id.key,
                rainworks_interaction->pose,
            },
            quest
        );
        ok &= expect(
            resolved_rainworks.found && resolved_rainworks.objective == objective,
            "umbrella-lane rainworks interactions resolve in authored prerequisite order"
        );
        ok &= quest.apply(
                      {
                          tick++,
                          definition().player.actor,
                          sequence++,
                          {},
                          resolved_rainworks.objective,
                      },
                      sink
                  ).error == QuestError::none;
    }
    ok &= expect(
        !interactions
             .resolve(
                 {definition().player.actor, route->cell_id.key, route->pose},
                 quest
             )
             .found,
        "the completed rainworks chain still waits for the paper-egret answer"
    );
    for (const auto& placement :
         definition().quest_encounter_activations[7].actor_placements) {
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
        "completed hostile groups and rainworks unlock the authored lane choice and stable option"
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
    const auto selections_before_lane = quest.snapshot().selection_count;
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
            quest.snapshot().selection_count == selections_before_lane + 1 &&
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
            quest.snapshot().selection_count == selections_before_lane + 1,
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
    const auto selections_before_calibration = quest.snapshot().selection_count;
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
            calibrated.stage_advanced &&
            quest.snapshot().selection_count == selections_before_calibration + 1 &&
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
         definition().quest_encounter_activations[8].actor_placements) {
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
        "return hostiles do not bypass the authored calibration primer"
    );
    const auto primer = std::find_if(
        definition().quest_interactions.begin(),
        definition().quest_interactions.end(),
        [](const tgd::contracts::QuestInteractionDefinition& interaction) {
            return interaction.objective_id.key ==
                   tgd::contracts::stable_content_key(
                       "f1_objective_prime_return_calibration"
                   );
        }
    );
    ok &= expect(
        primer != definition().quest_interactions.end(),
        "the return calibration primer interaction exists"
    );
    if (primer == definition().quest_interactions.end()) {
        return false;
    }
    const auto resolved_primer = interactions.resolve(
        {definition().player.actor, primer->cell_id.key, primer->pose},
        quest
    );
    ok &= expect(
        resolved_primer.found && resolved_primer.objective == primer->objective_id.key,
        "the calibration primer resolves while the entry group remains active"
    );
    ok &= quest.apply(
                  {
                      tick++,
                      definition().player.actor,
                      sequence++,
                      {},
                      resolved_primer.objective,
                  },
                  sink
              ).error == QuestError::none;
    for (const auto& placement :
         definition().quest_encounter_activations[9].actor_placements) {
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
        std::count_if(
            return_actors.begin(),
            return_actors.end(),
            [](const tgd::contracts::CombatActorSnapshot& actor) {
                return actor.faction == tgd::contracts::CombatFaction::hostile &&
                       actor.active;
            }
        ) == 4,
        "the calibration primer reinforces the three-actor return group to four"
    );
    ok &= expect(
        !outcomes.resolve(return_actors, quest).found,
        "the return clear outcome remains locked until the selected calibration action"
    );
    const auto wrong_calibration_action = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_flower_turn"),
            tgd::contracts::stable_content_key("ability_flower_light"),
        },
        quest
    );
    ok &= expect(
        !wrong_calibration_action.found,
        "a winter calibration action cannot satisfy the selected spring-rib branch"
    );
    const auto calibration_action = triggers.resolve(
        {
            definition().player.actor,
            tgd::contracts::QuestCombatTriggerKind::player_ability_started,
            tgd::contracts::stable_content_key("stance_eavesguard"),
            tgd::contracts::stable_content_key("ability_eavesguard_heavy"),
        },
        quest
    );
    ok &= expect(
        calibration_action.found && calibration_action.objective ==
            tgd::contracts::stable_content_key(
                "f1_objective_demonstrate_rib_calibration"
            ),
        "the selected spring-rib branch requires an eavesguard heavy demonstration"
    );
    ok &= quest.apply(
              {
                  tick++,
                  definition().player.actor,
                  sequence++,
                  {},
                  calibration_action.objective,
              },
              sink
          ).error == QuestError::none;
    for (const auto activation_index : std::array<std::size_t, 2>{8, 9}) {
        for (const auto& placement :
             definition().quest_encounter_activations[activation_index].actor_placements) {
        for (auto& actor : return_actors) {
            if (actor.actor == placement.actor) {
                actor.resources.health = 0;
                actor.active = false;
                actor.defeated = true;
            }
        }
        }
    }
    const auto return_clear = outcomes.resolve(return_actors, quest);
    ok &= expect(
        return_clear.found && return_clear.objective ==
                                  tgd::contracts::stable_content_key(
                                      "f1_objective_validate_calibration"
                                  ),
        "defeating the entry group and additive reinforcement validates calibration"
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

bool test_return_calibration_combat_trigger_follows_choice() {
    const auto exercise_branch = [](
                                     tgd::contracts::StableContentKey choice,
                                     tgd::contracts::StableContentKey expected_stance,
                                     tgd::contracts::StableContentKey expected_ability,
                                     tgd::contracts::StableContentKey rejected_stance,
                                     tgd::contracts::StableContentKey rejected_ability
                                 ) {
        DeterministicQuestRuntime quest;
        DeterministicQuestCombatTriggerResolver triggers;
        CollectingSink sink;
        bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
        ok &= quest.start() == QuestError::none;
        ok &= triggers.initialize(definition().quest_combat_triggers) ==
              QuestCombatTriggerError::none;
        tgd::contracts::TickIndex tick = 1;
        tgd::contracts::CommandSequence sequence = 1;
        for (std::size_t stage = 0; stage < 3; ++stage) {
            for (const auto& objective : definition().beats[stage].objectives) {
                ok &= quest.apply(
                              {
                                  tick++,
                                  definition().player.actor,
                                  sequence++,
                                  {},
                                  objective.key,
                                  selection_for_objective(objective.key),
                              },
                              sink
                          ).error == QuestError::none;
            }
        }
        const auto& workbench = definition().beats[3];
        for (std::size_t index = 0; index < 3; ++index) {
            ok &= quest.apply(
                          {
                              tick++,
                              definition().player.actor,
                              sequence++,
                              {},
                              workbench.objectives[index].key,
                          },
                          sink
                      ).error == QuestError::none;
        }
        ok &= quest.apply(
                      {
                          tick++,
                          definition().player.actor,
                          sequence++,
                          {},
                          workbench.objectives[3].key,
                          choice,
                      },
                      sink
                  ).error == QuestError::none;
        const auto& return_beat = definition().beats[4];
        ok &= quest.apply(
                      {
                          tick++,
                          definition().player.actor,
                          sequence++,
                          {},
                          return_beat.objectives[0].key,
                      },
                      sink
                  ).error == QuestError::none;
        const auto rejected = triggers.resolve(
            {
                definition().player.actor,
                tgd::contracts::QuestCombatTriggerKind::player_ability_started,
                rejected_stance,
                rejected_ability,
            },
            quest
        );
        const auto accepted = triggers.resolve(
            {
                definition().player.actor,
                tgd::contracts::QuestCombatTriggerKind::player_ability_started,
                expected_stance,
                expected_ability,
            },
            quest
        );
        ok &= expect(
            !rejected.found && accepted.found && accepted.objective ==
                tgd::contracts::stable_content_key(
                    "f1_objective_demonstrate_rib_calibration"
                ),
            "only the combat action mapped to the committed rib choice can progress"
        );
        return ok;
    };

    return exercise_branch(
               tgd::contracts::stable_content_key(
                   "f1_choice_rib_spring_calibration"
               ),
               tgd::contracts::stable_content_key("stance_eavesguard"),
               tgd::contracts::stable_content_key("ability_eavesguard_heavy"),
               tgd::contracts::stable_content_key("stance_flower_turn"),
               tgd::contracts::stable_content_key("ability_flower_light")
           ) &&
           exercise_branch(
               tgd::contracts::stable_content_key(
                   "f1_choice_rib_winter_calibration"
               ),
               tgd::contracts::stable_content_key("stance_flower_turn"),
               tgd::contracts::stable_content_key("ability_flower_light"),
               tgd::contracts::stable_content_key("stance_eavesguard"),
               tgd::contracts::stable_content_key("ability_eavesguard_heavy")
           );
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
    ok &= test_interaction_attempt_resolution_is_read_only_and_fail_closed();
    ok &= test_rain_ferry_optional_clues_and_error_recovery_converge();
    ok &= test_scene_interaction_ties_are_stable();
    ok &= test_scene_interaction_selection_gates_follow_lane_route();
    ok &= test_combat_signals_resolve_training_objectives();
    ok &= test_combat_outcome_attempts_are_target_specific_and_read_only();
    ok &= test_hostile_group_outcomes_unlock_lane_choice();
    ok &= test_return_calibration_combat_trigger_follows_choice();
    ok &= test_four_seasons_boss_phases_are_ordered();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
