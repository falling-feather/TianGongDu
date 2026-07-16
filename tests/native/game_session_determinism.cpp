#include <tgd/contracts/session_types.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/runtime/fixed_step_driver.hpp>
#include <tgd/runtime/game_session.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

using tgd::contracts::GroundVectorQ15;
using tgd::contracts::SessionCommand;
using tgd::contracts::SessionCommandType;
using tgd::runtime::GameSession;
using tgd::runtime::GameSessionConfig;
using tgd::runtime::GameSessionError;
using tgd::runtime::StaticCollisionWorld;

constexpr tgd::contracts::StableActorKey player_actor = 1;
bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "game session determinism failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] std::unique_ptr<StaticCollisionWorld> empty_world() {
    auto world = std::make_unique<StaticCollisionWorld>();
    const auto configured = world->configure({});
    if (configured != tgd::runtime::CollisionWorldError::none) {
        std::abort();
    }
    return world;
}

[[nodiscard]] SessionCommand move_command(
    std::uint64_t tick,
    std::uint64_t sequence,
    GroundVectorQ15 direction
) {
    return {{tick, player_actor, sequence, SessionCommandType::move_intent}, direction};
}

[[nodiscard]] SessionCommand jump_command(std::uint64_t tick, std::uint64_t sequence) {
    return {{tick, player_actor, sequence, SessionCommandType::jump_pressed}, {}};
}

bool test_collision_height_and_floor_layers() {
    StaticCollisionWorld world;
    const std::array blockers{
        tgd::runtime::GroundBlocker{1, 500, 700, -500, 500, 0, 2'000, 0},
    };
    bool ok = world.configure(blockers) == tgd::runtime::CollisionWorldError::none;

    const tgd::contracts::GroundPoseMm ground_pose{350, 0, 0, 0};
    const auto blocked = world.resolve_ground_move(ground_pose, 100, 0, 100, 1'800);
    ok &= expect(blocked.blocked_x && blocked.pose.x == ground_pose.x, "same-layer blocker stops x");

    auto bridge_pose = ground_pose;
    bridge_pose.floor_layer = 1;
    const auto bridge = world.resolve_ground_move(bridge_pose, 100, 0, 100, 1'800);
    ok &= expect(!bridge.blocked_x && bridge.pose.x == 450, "bridge layer can overlap ground x/y");

    auto high_pose = ground_pose;
    high_pose.height = 2'100;
    const auto high = world.resolve_ground_move(high_pose, 100, 0, 100, 1'800);
    ok &= expect(!high.blocked_x && high.pose.x == 450, "height interval separates occupancy");
    return ok;
}

bool test_command_order_jump_and_lifecycle() {
    GameSession session;
    GameSessionConfig config{};
    config.collision_radius = 100;
    bool ok = session.initialize(config, empty_world()) == GameSessionError::none;
    ok &= expect(session.start() == GameSessionError::none, "session starts from safe point");

    const std::array commands{
        move_command(1, 2, {tgd::contracts::ground_axis_one, 0}),
        move_command(1, 1, {-tgd::contracts::ground_axis_one, 0}),
        jump_command(1, 3),
    };
    ok &= expect(session.submit(commands) == GameSessionError::none, "commands enqueue");
    const std::array duplicate_commands{
        move_command(2, 9, {tgd::contracts::ground_axis_one, 0}),
        move_command(2, 9, {-tgd::contracts::ground_axis_one, 0}),
    };
    ok &= expect(
        session.submit(duplicate_commands) == GameSessionError::duplicate_command_key,
        "duplicate ordering keys are rejected atomically"
    );
    const auto advanced = session.advance(1);
    ok &= expect(
        advanced.error == GameSessionError::none && advanced.executed_ticks == 1,
        "one fixed tick executes"
    );
    ok &= expect(session.current_snapshot().player_pose.x == 50, "stable command order applies last intent");
    ok &= expect(
        session.current_snapshot().player_pose.height > 0 &&
            !session.current_snapshot().grounded,
        "height is independent from ground x/y"
    );
    ok &= expect(
        session.submit(std::span{commands}.first(1)) ==
            GameSessionError::command_targets_past_tick,
        "past tick commands are rejected"
    );
    ok &= expect(session.pause() == GameSessionError::none, "session pauses explicitly");
    ok &= expect(session.advance(1).error == GameSessionError::invalid_lifecycle, "paused tick is rejected");
    ok &= expect(session.resume() == GameSessionError::none, "session resumes explicitly");
    ok &= expect(session.destroy() == GameSessionError::none, "session destroys explicitly");
    ok &= expect(session.generation() == 2, "destroy invalidates the session generation");
    return ok;
}

bool test_frame_overrun_and_visibility() {
    GameSession session;
    GameSessionConfig config{};
    bool ok = session.initialize(config, empty_world()) == GameSessionError::none;
    ok &= session.start() == GameSessionError::none;
    std::array<SessionCommand, 10> commands{};
    for (std::size_t index = 0; index < commands.size(); ++index) {
        commands[index] = move_command(
            index + 1,
            index + 1,
            {tgd::contracts::ground_axis_one, 0}
        );
    }
    ok &= session.submit(commands) == GameSessionError::none;

    tgd::runtime::FixedStepDriver driver;
    const auto overrun = driver.advance_frame(session, 250'000'000ULL);
    ok &= expect(overrun.simulation_overrun, "250 ms frame reports simulation overrun");
    ok &= expect(overrun.executed_ticks == 4, "frame catch-up is capped at four ticks");
    ok &= expect(
        overrun.interpolation_numerator < tgd::runtime::FixedStepDriver::scaled_tick,
        "overrun keeps less than one tick of interpolation"
    );

    driver.set_visible(false);
    const auto hidden_tick = session.current_snapshot().tick;
    ok &= expect(
        driver.advance_frame(session, 5'000'000'000ULL).executed_ticks == 0 &&
            session.current_snapshot().tick == hidden_tick,
        "hidden time never advances gameplay"
    );
    driver.set_visible(true);
    ok &= expect(
        driver.advance_frame(session, 5'000'000'000ULL).executed_ticks == 0,
        "first restored frame discards elapsed time"
    );
    ok &= expect(
        driver.advance_frame(session, 16'666'667ULL).executed_ticks == 1,
        "visible cadence resumes from a fresh tick"
    );
    ok &= expect(session.pause() == GameSessionError::none, "runtime pause is explicit");
    ok &= expect(
        driver.advance_frame(session, 1'000'000'000ULL).executed_ticks == 0,
        "paused frame clears accumulated time"
    );
    ok &= expect(session.resume() == GameSessionError::none, "runtime resume is explicit");
    ok &= expect(
        driver.advance_frame(session, 1'000'000'000ULL).executed_ticks == 0,
        "first frame after pause discards elapsed time"
    );
    ok &= expect(
        driver.advance_frame(session, 16'666'667ULL).executed_ticks == 1,
        "post-pause cadence resumes without debt"
    );
    return ok;
}

bool test_safe_point_retry_boundary() {
    GameSession session;
    GameSessionConfig config{};
    config.initial_pose = {100, -50, 0, 0};
    bool ok = session.initialize(config, empty_world()) == GameSessionError::none;
    ok &= session.start() == GameSessionError::none;
    const std::array commands{
        move_command(1, 1, {tgd::contracts::ground_axis_one, 0}),
        move_command(2, 2, {tgd::contracts::ground_axis_one, 0}),
    };
    ok &= session.submit(commands) == GameSessionError::none;
    ok &= session.advance(1).error == GameSessionError::none;
    ok &= expect(
        session.current_snapshot().player_pose.x > config.initial_pose.x &&
            session.queued_command_count() == 1,
        "movement and a future command exist before retry"
    );

    const auto checksum_before_safe_point = session.current_snapshot().checksum;
    constexpr tgd::contracts::StableContentKey safe_point_key = 77;
    const tgd::contracts::GroundPoseMm safe_point_pose{400, 250, 0, 0};
    const tgd::contracts::SafePointCommitCommand wrong_safe_point_tick{
        0,
        player_actor,
        1,
        safe_point_key,
        safe_point_pose,
    };
    ok &= expect(
        session.commit_safe_point(wrong_safe_point_tick) ==
            GameSessionError::safe_point_targets_wrong_tick,
        "safe-point commits bind to an exact completed tick"
    );
    const tgd::contracts::SafePointCommitCommand commit{
        1,
        player_actor,
        1,
        safe_point_key,
        safe_point_pose,
    };
    ok &= expect(
        session.commit_safe_point(commit) == GameSessionError::none &&
            session.active_safe_point() == safe_point_key &&
            session.active_safe_point_pose() == safe_point_pose &&
            session.current_snapshot().checksum != checksum_before_safe_point,
        "a valid authored safe point becomes deterministic retry state"
    );
    ok &= expect(
        session.commit_safe_point(commit) == GameSessionError::stale_safe_point_sequence,
        "duplicate safe-point sequences are rejected"
    );
    auto invalid_commit = commit;
    invalid_commit.sequence = 2;
    invalid_commit.safe_point = 0;
    ok &= expect(
        session.commit_safe_point(invalid_commit) == GameSessionError::invalid_safe_point,
        "safe-point commits reject empty stable IDs"
    );

    const tgd::contracts::SafePointRetryCommand wrong_tick{0, player_actor, 1};
    ok &= expect(
        session.retry_from_safe_point(wrong_tick) == GameSessionError::retry_targets_wrong_tick,
        "retry binds to an exact completed tick"
    );
    const tgd::contracts::SafePointRetryCommand retry{1, player_actor, 1};
    ok &= expect(
        session.retry_from_safe_point(retry) == GameSessionError::none,
        "valid retry restores the configured safe point"
    );
    ok &= expect(
        session.current_snapshot().tick == 1 &&
            session.current_snapshot().player_pose == safe_point_pose &&
            session.queued_command_count() == 0,
        "retry preserves the monotonic tick and restores the latest authored safe point"
    );
    ok &= expect(session.generation() == 2, "retry invalidates the movement generation");
    ok &= expect(
        session.retry_from_safe_point(retry) == GameSessionError::stale_retry_sequence,
        "duplicate retry sequence is rejected"
    );
    ok &= session.advance(1).error == GameSessionError::none;
    ok &= expect(
        session.current_snapshot().player_pose == safe_point_pose,
        "discarded future input cannot move the restored player"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_collision_height_and_floor_layers();
    ok &= test_command_order_jump_and_lifecycle();
    ok &= test_frame_overrun_and_visibility();
    ok &= test_safe_point_retry_boundary();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
