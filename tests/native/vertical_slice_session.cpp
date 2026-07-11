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
#include <string_view>

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

[[nodiscard]] std::unique_ptr<tgd::runtime::StaticCollisionWorld> empty_world() {
    auto world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    if (world->configure({}) != tgd::runtime::CollisionWorldError::none) {
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

bool test_objective_driven_progression_and_lifecycle() {
    VerticalSliceSession session;
    bool ok = session.initialize(definition(), empty_world()) == VerticalSliceError::none;
    ok &= expect(
        session.lifecycle() == VerticalSliceLifecycle::ready_at_safe_point,
        "initialization stops at a safe point"
    );
    ok &= expect(
        session.current_snapshot().beat_id.name == "f1_beat_rain_ferry_arrival",
        "the route starts at Rain Ferry"
    );
    ok &= expect(session.start() == VerticalSliceError::none, "slice starts explicitly");

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

    for (std::size_t beat_index = 0; beat_index < definition().beats.size(); ++beat_index) {
        const auto& beat = definition().beats[beat_index];
        for (auto objective = beat.objectives.rbegin(); objective != beat.objectives.rend();
             ++objective) {
            const auto result = session.complete_objective(objective->key);
            ok &= expect(result.error == VerticalSliceError::none && result.accepted, "objective is accepted");
            const auto duplicate = session.complete_objective(objective->key);
            ok &= expect(
                duplicate.error == VerticalSliceError::none && !duplicate.accepted,
                "objective completion is idempotent"
            );
        }
    }
    ok &= expect(
        session.lifecycle() == VerticalSliceLifecycle::resolved &&
            session.current_snapshot().resolved,
        "the final return objective resolves the slice"
    );
    ok &= expect(
        session.current_snapshot().simulation_ticks == 1,
        "content objectives never use a forced timer gate"
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
    const auto completed_before = session.current_snapshot().completed_objectives;
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
        session.current_snapshot().player_pose == definition().player.initial_pose &&
            session.current_snapshot().completed_objectives == completed_before &&
            session.current_snapshot().simulation_ticks == 1,
        "retry restores pose without erasing authored progress or elapsed simulation"
    );
    ok &= expect(session.generation() == 2, "slice generation changes on retry");
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_objective_driven_progression_and_lifecycle();
    ok &= test_same_commands_produce_same_composed_checksum();
    ok &= test_retry_preserves_objective_progress();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
