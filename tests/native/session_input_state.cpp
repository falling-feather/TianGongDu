#include <tgd/contracts/input_action.hpp>
#include <tgd/gameplay/session_input_state.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/runtime/game_session.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "session input state failure: " << message << '\n';
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

[[nodiscard]] tgd::contracts::ScalarActionSample axis_sample(
    std::uint64_t sequence,
    std::string_view action,
    std::int32_t value
) {
    return {
        sequence,
        tgd::contracts::action_id(action),
        value,
        tgd::contracts::ActionSampleEdge::value_changed,
        false,
    };
}

[[nodiscard]] tgd::contracts::ScalarActionSample jump_sample(
    std::uint64_t sequence,
    bool repeated
) {
    return {
        sequence,
        tgd::contracts::action_id("jump"),
        tgd::contracts::ground_axis_one,
        tgd::contracts::ActionSampleEdge::pressed,
        repeated,
    };
}

}  // namespace

int main() {
    using tgd::gameplay::SessionInputError;
    using tgd::gameplay::SessionInputState;

    bool ok = true;
    SessionInputState diagonal_input;
    const std::array diagonal_samples{
        axis_sample(1, "move_x", tgd::contracts::ground_axis_one),
        axis_sample(2, "move_y", tgd::contracts::ground_axis_one),
    };
    ok &= diagonal_input.submit(diagonal_samples) == SessionInputError::none;
    const auto diagonal = diagonal_input.commands_for_tick(1, 1, 1).commands[0].ground_direction;
    const auto diagonal_length_squared = static_cast<std::int64_t>(diagonal.x) * diagonal.x +
                                         static_cast<std::int64_t>(diagonal.y) * diagonal.y;
    const auto unit_length_squared =
        static_cast<std::int64_t>(tgd::contracts::ground_axis_one) *
        tgd::contracts::ground_axis_one;
    ok &= expect(
        diagonal.x == diagonal.y && diagonal_length_squared <= unit_length_squared,
        "full diagonal input is normalized without a speed advantage"
    );

    SessionInputState input;
    const std::array initial_samples{
        axis_sample(1, "move_y", tgd::contracts::ground_axis_one),
    };
    ok &= expect(input.submit(initial_samples) == SessionInputError::none, "move_y is accepted");
    const auto before_camera_change = input.commands_for_tick(1, 1, 10);
    ok &= expect(
        before_camera_change.commands[0].ground_direction ==
            tgd::contracts::GroundVectorQ15{0, tgd::contracts::ground_axis_one},
        "default camera maps screen forward to world y"
    );

    const tgd::contracts::CameraBasisQ15 rotated_basis{
        {0, -tgd::contracts::ground_axis_one},
        {tgd::contracts::ground_axis_one, 0},
        2,
    };
    ok &= expect(
        input.set_camera_basis(rotated_basis) == SessionInputError::none,
        "authored orthogonal camera basis is accepted"
    );
    const auto after_camera_change = input.commands_for_tick(2, 1, 20);
    ok &= expect(
        after_camera_change.commands[0].ground_direction ==
            tgd::contracts::GroundVectorQ15{tgd::contracts::ground_axis_one, 0},
        "future input follows the new camera basis"
    );

    tgd::runtime::GameSession session;
    tgd::runtime::GameSessionConfig config{};
    config.collision_radius = 100;
    ok &= session.initialize(config, empty_world()) == tgd::runtime::GameSessionError::none;
    ok &= session.start() == tgd::runtime::GameSessionError::none;
    ok &= session.submit(before_camera_change.view()) == tgd::runtime::GameSessionError::none;
    ok &= session.submit(after_camera_change.view()) == tgd::runtime::GameSessionError::none;
    ok &= session.advance(2).error == tgd::runtime::GameSessionError::none;
    ok &= expect(
        session.current_snapshot().player_pose.x == 50 &&
            session.current_snapshot().player_pose.y == 50,
        "already submitted world direction is immutable across camera changes"
    );

    ok &= expect(
        input.clear(2, tgd::contracts::InputClearReason::blur) == SessionInputError::none,
        "blur clears held gameplay actions"
    );
    const auto after_blur = input.commands_for_tick(3, 1, 30);
    ok &= expect(
        after_blur.commands[0].ground_direction == tgd::contracts::GroundVectorQ15{},
        "blur prevents sticky movement"
    );
    ok &= expect(
        input.clear(2, tgd::contracts::InputClearReason::device_disconnected) ==
            SessionInputError::out_of_order_platform_sequence,
        "out-of-order lifecycle batches are rejected"
    );

    const std::array repeated_jump{jump_sample(3, true)};
    ok &= input.submit(repeated_jump) == SessionInputError::none;
    ok &= expect(
        input.commands_for_tick(4, 1, 40).size == 1,
        "browser repeat never creates a jump press edge"
    );
    const std::array real_jump{jump_sample(4, false)};
    ok &= input.submit(real_jump) == SessionInputError::none;
    ok &= expect(input.commands_for_tick(5, 1, 50).size == 2, "new physical edge creates jump");
    ok &= expect(input.commands_for_tick(6, 1, 60).size == 1, "jump edge is consumed once");

    const std::array held_again{
        axis_sample(5, "move_x", tgd::contracts::ground_axis_one),
    };
    ok &= input.submit(held_again) == SessionInputError::none;
    ok &= input.set_gameplay_enabled(false, 6) == SessionInputError::none;
    ok &= expect(
        input.commands_for_tick(7, 1, 70).commands[0].ground_direction ==
            tgd::contracts::GroundVectorQ15{},
        "higher input context clears gameplay axes"
    );
    ok &= input.set_gameplay_enabled(true, 7) == SessionInputError::none;
    ok &= expect(
        input.clear(8, tgd::contracts::InputClearReason::device_disconnected) ==
            SessionInputError::none &&
            input.last_clear_reason() == tgd::contracts::InputClearReason::device_disconnected,
        "device disconnect is an explicit clear reason"
    );

    auto invalid_basis = rotated_basis;
    invalid_basis.revision = 3;
    invalid_basis.screen_forward_world = invalid_basis.screen_right_world;
    ok &= expect(
        input.set_camera_basis(invalid_basis) == SessionInputError::invalid_camera_basis,
        "non-orthogonal camera basis is rejected"
    );
    invalid_basis.screen_forward_world = {-tgd::contracts::ground_axis_one, 0};
    ok &= expect(
        input.set_camera_basis(invalid_basis) == SessionInputError::invalid_camera_basis,
        "mirrored camera basis is rejected"
    );
    ok &= expect(
        input.set_camera_basis(rotated_basis) == SessionInputError::stale_camera_basis,
        "camera basis revisions never move backward"
    );
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
