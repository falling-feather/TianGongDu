#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/session_types.hpp>
#include <tgd/gameplay/vertical_slice_session.hpp>
#include <tgd/runtime/collision_world.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {

using tgd::gameplay::VerticalSliceError;
using tgd::gameplay::VerticalSliceLifecycle;
using tgd::gameplay::VerticalSliceSession;

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "vertical slice session failure: " << message << '\n';
    }
    return condition;
}

class CollectingQuestSink final : public tgd::gameplay::IQuestEventSink {
  public:
    void publish(std::span<const tgd::contracts::QuestEvent> events) noexcept override {
        values.insert(values.end(), events.begin(), events.end());
    }

    [[nodiscard]] bool contains(tgd::contracts::QuestEventType type) const {
        return std::any_of(values.begin(), values.end(), [type](const auto& event) {
            return event.type == type;
        });
    }

    std::vector<tgd::contracts::QuestEvent> values;
};

[[nodiscard]] std::unique_ptr<tgd::runtime::StaticCollisionWorld> empty_world() {
    auto world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    if (world->configure({}) != tgd::runtime::CollisionWorldError::none) {
        std::abort();
    }
    return world;
}

[[nodiscard]] std::unique_ptr<tgd::runtime::StaticCollisionWorld> world_blocking(
    const tgd::contracts::GroundPoseMm& pose
) {
    auto world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    const std::array blockers{
        tgd::runtime::GroundBlocker{
            1,
            pose.x - 100,
            pose.x + 100,
            pose.y - 100,
            pose.y + 100,
            pose.height,
            pose.height + 2'000,
            pose.floor_layer,
        },
    };
    if (world->configure(blockers) != tgd::runtime::CollisionWorldError::none) {
        std::abort();
    }
    return world;
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

bool test_objective_driven_progression_and_lifecycle() {
    VerticalSliceSession session;
    bool ok = session.initialize(definition(), empty_world()) == VerticalSliceError::none;
    ok &= expect(
        session.lifecycle() == VerticalSliceLifecycle::ready_at_safe_point,
        "initialization stops at a safe point"
    );
    ok &= expect(
        session.current_snapshot().beat_id.name == "f1_beat_rain_ferry_arrival" &&
            session.current_snapshot().safe_point_id == definition().safe_points.front().id &&
            session.current_snapshot().safe_point_pose ==
                definition().safe_points.front().pose,
        "the route starts at the authored Rain Ferry safe point"
    );
    ok &= expect(session.start() == VerticalSliceError::none, "slice starts explicitly");
    ok &= expect(
        session.quest_snapshot().stage == definition().beats.front().id.key &&
            session.objective_state(definition().beats.front().objectives.front().key) ==
                tgd::gameplay::QuestObjectiveState::active,
        "slice exposes the composed quest snapshot"
    );

    const tgd::contracts::SessionCommand move{
        {1, definition().player.actor, 1, tgd::contracts::SessionCommandType::move_intent},
        {tgd::contracts::ground_axis_one, 0},
    };
    ok &= expect(
        session.submit_movement(std::span{&move, 1}) == VerticalSliceError::none,
        "movement reuses the proven GameSession command path"
    );
    const auto advanced = session.advance(1);
    ok &= expect(
        advanced.error == VerticalSliceError::none && advanced.executed_ticks == 1 &&
            session.current_snapshot().player_pose.x > definition().player.initial_pose.x,
        "one 60 Hz movement tick reaches the composed snapshot"
    );

    const auto future_objective = definition().beats[1].objectives.front().key;
    ok &= expect(
        session.complete_objective(future_objective).error ==
            VerticalSliceError::objective_not_active,
        "future content cannot bypass the active beat"
    );
    ok &= expect(
        session.complete_objective(tgd::contracts::stable_content_key("missing_objective"))
                .error == VerticalSliceError::unknown_objective,
        "unknown content fails closed"
    );

    CollectingQuestSink quest_sink;
    for (std::size_t beat_index = 0; beat_index < definition().beats.size(); ++beat_index) {
        const auto& beat = definition().beats[beat_index];
        for (auto objective = beat.objectives.rbegin(); objective != beat.objectives.rend();
             ++objective) {
            const auto selection = selection_for_objective(objective->key);
            const auto result = session.complete_objective(
                objective->key,
                selection,
                quest_sink
            );
            ok &= expect(result.error == VerticalSliceError::none && result.accepted, "objective is accepted");
            const auto duplicate = session.complete_objective(objective->key, selection);
            ok &= expect(
                duplicate.error == VerticalSliceError::none && !duplicate.accepted,
                "objective completion is idempotent"
            );
        }
        if (beat_index + 1 < definition().beats.size()) {
            ok &= expect(
                session.current_snapshot().safe_point_id ==
                        definition().safe_points[beat_index + 1].id &&
                    session.current_snapshot().safe_point_pose ==
                        definition().safe_points[beat_index + 1].pose,
                "beat advancement commits the next authored movement safe point"
            );
        }
    }
    ok &= expect(
        session.lifecycle() == VerticalSliceLifecycle::resolved &&
            session.current_snapshot().resolved &&
            session.current_snapshot().safe_point_id == definition().safe_points.back().id,
        "the final return objective resolves the slice"
    );
    ok &= expect(
        quest_sink.contains(tgd::contracts::QuestEventType::stage_advanced) &&
            quest_sink.contains(tgd::contracts::QuestEventType::quest_resolved),
        "quest events survive composition through the vertical slice"
    );
    ok &= expect(
        session.current_snapshot().simulation_ticks == 1 &&
            session.current_snapshot().playtime.total_ticks == 1 &&
            session.current_snapshot().playtime.eligible_ticks == 1 &&
            session.current_snapshot().playtime.idle_ticks == 0 &&
            session.current_snapshot().playtime.failure_retry_ticks == 0 &&
            !session.current_snapshot().playtime.playable_target_met,
        "content objectives never use a forced timer gate or fabricate the one-hour audit"
    );
    ok &= expect(
        session.advance(1).error == VerticalSliceError::invalid_lifecycle,
        "resolved simulation stays frozen at the settlement safe point"
    );
    ok &= expect(session.destroy() == VerticalSliceError::none, "resolved slice destroys cleanly");
    ok &= expect(session.generation() == 2, "destroy invalidates the session generation");
    return ok;
}

bool test_same_commands_produce_same_composed_checksum() {
    VerticalSliceSession left;
    VerticalSliceSession right;
    bool ok = left.initialize(definition(), empty_world()) == VerticalSliceError::none;
    ok &= right.initialize(definition(), empty_world()) == VerticalSliceError::none;
    ok &= left.start() == VerticalSliceError::none;
    ok &= right.start() == VerticalSliceError::none;

    std::array<tgd::contracts::SessionCommand, 3> commands{};
    for (std::size_t index = 0; index < commands.size(); ++index) {
        commands[index] = {
            {index + 1,
             definition().player.actor,
             index + 1,
             tgd::contracts::SessionCommandType::move_intent},
            {tgd::contracts::ground_axis_one, 0},
        };
    }
    ok &= left.submit_movement(commands) == VerticalSliceError::none;
    ok &= right.submit_movement(commands) == VerticalSliceError::none;
    ok &= left.advance(3).error == VerticalSliceError::none;
    ok &= right.advance(3).error == VerticalSliceError::none;

    const auto& first_beat = definition().beats.front();
    for (const auto& objective : first_beat.objectives) {
        ok &= left.complete_objective(objective.key).error == VerticalSliceError::none;
        ok &= right.complete_objective(objective.key).error == VerticalSliceError::none;
    }
    ok &= expect(
        left.current_snapshot().checksum == right.current_snapshot().checksum &&
            left.current_snapshot().player_pose == right.current_snapshot().player_pose,
        "composed movement and objective snapshots are deterministic"
    );
    ok &= expect(left.pause() == VerticalSliceError::none, "slice pauses explicitly");
    ok &= expect(left.resume() == VerticalSliceError::none, "slice resumes explicitly");
    return ok;
}

bool test_retry_preserves_objective_progress() {
    VerticalSliceSession session;
    bool ok = session.initialize(definition(), empty_world()) == VerticalSliceError::none;
    ok &= session.start() == VerticalSliceError::none;
    const tgd::contracts::SessionCommand move{
        {1, definition().player.actor, 1, tgd::contracts::SessionCommandType::move_intent},
        {tgd::contracts::ground_axis_one, 0},
    };
    ok &= session.submit_movement(std::span{&move, 1}) == VerticalSliceError::none;
    ok &= session.advance(1).error == VerticalSliceError::none;
    ok &= session.complete_objective(definition().beats.front().objectives.front().key).error ==
          VerticalSliceError::none;
    for (std::size_t index = 1; index < definition().beats.front().objectives.size(); ++index) {
        ok &= session.complete_objective(
                  definition().beats.front().objectives[index].key
              ).error == VerticalSliceError::none;
    }
    for (const auto& objective : definition().beats[1].objectives) {
        ok &= session.complete_objective(objective.key).error == VerticalSliceError::none;
    }
    ok &= expect(
        session.current_snapshot().beat_index == 2 &&
            session.current_snapshot().safe_point_id == definition().safe_points[2].id &&
            session.current_snapshot().safe_point_pose == definition().safe_points[2].pose,
        "entering the umbrella lane commits its authored retry pose"
    );
    const tgd::contracts::SessionCommand failed_attempt_move{
        {2, definition().player.actor, 2, tgd::contracts::SessionCommandType::move_intent},
        {tgd::contracts::ground_axis_one, 0},
    };
    ok &= session.submit_movement(std::span{&failed_attempt_move, 1}) ==
          VerticalSliceError::none;
    ok &= session.advance(1).error == VerticalSliceError::none;
    const auto completed_before = session.current_snapshot().completed_objectives;
    const auto quest_checksum_before = session.quest_snapshot().checksum;
    const tgd::contracts::SafePointRetryCommand retry{
        session.current_snapshot().tick,
        definition().player.actor,
        1,
    };
    ok &= expect(
        session.retry_from_safe_point(retry) == VerticalSliceError::none,
        "slice composes a movement safe-point retry"
    );
    ok &= expect(
        session.current_snapshot().player_pose == definition().safe_points[2].pose &&
            session.current_snapshot().safe_point_id == definition().safe_points[2].id &&
            session.current_snapshot().completed_objectives == completed_before &&
            session.current_snapshot().simulation_ticks == 2 &&
            session.current_snapshot().playtime.eligible_ticks == 1 &&
            session.current_snapshot().playtime.idle_ticks == 0 &&
            session.current_snapshot().playtime.failure_retry_ticks == 1 &&
            session.quest_snapshot().checksum == quest_checksum_before,
        "retry restores pose and progress while excluding the failed attempt from playtime"
    );
    ok &= expect(session.generation() == 2, "slice generation changes on retry");
    return ok;
}

bool test_every_authored_safe_point_is_collision_checked_before_start() {
    VerticalSliceSession session;
    const auto& boss_safe_point = definition().safe_points[5];
    return expect(
        session.initialize(definition(), world_blocking(boss_safe_point.pose)) ==
            VerticalSliceError::movement_session_error &&
            session.lifecycle() == VerticalSliceLifecycle::uninitialized,
        "a blocked future safe point rejects the slice before any quest progress can commit"
    );
}

bool test_idle_simulation_is_visible_but_never_eligible() {
    VerticalSliceSession session;
    bool ok = session.initialize(definition(), empty_world()) == VerticalSliceError::none;
    ok &= session.start() == VerticalSliceError::none;
    ok &= session.advance(181).error == VerticalSliceError::none;
    return expect(
        ok && session.current_snapshot().simulation_ticks == 181 &&
            session.current_snapshot().playtime.total_ticks == 181 &&
            session.current_snapshot().playtime.eligible_ticks == 0 &&
            session.current_snapshot().playtime.idle_ticks == 181 &&
            !session.current_snapshot().playtime.playable_target_met,
        "running without player activity is audited as idle rather than playable time"
    );
}

bool test_objective_driven_encounter_activations_fail_closed() {
    auto cross_beat = definition();
    std::vector<tgd::contracts::QuestEncounterActivationDefinition> cross_beat_activations(
        definition().quest_encounter_activations.begin(),
        definition().quest_encounter_activations.end()
    );
    cross_beat_activations[1].trigger_objective_id = definition().beats[2].objectives.front();
    cross_beat.quest_encounter_activations = cross_beat_activations;
    VerticalSliceSession cross_beat_session;
    bool ok = expect(
        cross_beat_session.initialize(cross_beat, empty_world()) ==
            VerticalSliceError::invalid_definition,
        "an encounter activation cannot listen to an objective from another beat"
    );

    auto duplicate_entry = definition();
    std::vector<tgd::contracts::QuestEncounterActivationDefinition> duplicate_activations(
        definition().quest_encounter_activations.begin(),
        definition().quest_encounter_activations.end()
    );
    duplicate_activations[1].trigger_objective_id = {};
    duplicate_entry.quest_encounter_activations = duplicate_activations;
    VerticalSliceSession duplicate_session;
    ok &= expect(
        duplicate_session.initialize(duplicate_entry, empty_world()) ==
            VerticalSliceError::invalid_definition,
        "one beat cannot declare two stage-entry encounter activations"
    );

    auto mismatched_placement = definition();
    std::vector<tgd::contracts::QuestEncounterActivationDefinition>
        mismatched_activations(
            definition().quest_encounter_activations.begin(),
            definition().quest_encounter_activations.end()
        );
    std::vector<tgd::contracts::EncounterActorPlacementDefinition> mismatched_placements(
        mismatched_activations[2].actor_placements.begin(),
        mismatched_activations[2].actor_placements.end()
    );
    mismatched_placements[0].actor = mismatched_activations[3].actor_keys.front();
    mismatched_activations[2].actor_placements = mismatched_placements;
    mismatched_placement.quest_encounter_activations = mismatched_activations;
    VerticalSliceSession mismatched_session;
    ok &= expect(
        mismatched_session.initialize(mismatched_placement, empty_world()) ==
            VerticalSliceError::invalid_definition,
        "an encounter placement must target its ordered actor key"
    );

    auto overlapping_slots = definition();
    std::vector<tgd::contracts::QuestEncounterActivationDefinition> overlapping_activations(
        definition().quest_encounter_activations.begin(),
        definition().quest_encounter_activations.end()
    );
    std::vector<tgd::contracts::EncounterActorPlacementDefinition> overlapping_placements(
        overlapping_activations[2].actor_placements.begin(),
        overlapping_activations[2].actor_placements.end()
    );
    overlapping_placements[1].formation_slot = overlapping_placements[0].formation_slot;
    overlapping_activations[2].actor_placements = overlapping_placements;
    overlapping_slots.quest_encounter_activations = overlapping_activations;
    VerticalSliceSession overlapping_session;
    ok &= expect(
        overlapping_session.initialize(overlapping_slots, empty_world()) ==
            VerticalSliceError::invalid_definition,
        "one encounter group cannot overlap formation slots"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_objective_driven_progression_and_lifecycle();
    ok &= test_same_commands_produce_same_composed_checksum();
    ok &= test_retry_preserves_objective_progress();
    ok &= test_every_authored_safe_point_is_collision_checked_before_start();
    ok &= test_idle_simulation_is_visible_but_never_eligible();
    ok &= test_objective_driven_encounter_activations_fail_closed();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
