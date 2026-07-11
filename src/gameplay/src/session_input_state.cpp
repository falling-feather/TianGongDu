#include <tgd/gameplay/session_input_state.hpp>

#include <algorithm>
#include <cstdint>

namespace tgd::gameplay {
namespace {

inline constexpr auto gameplay_context = contracts::input_context_id("gameplay");
inline constexpr auto move_x_action = contracts::action_id("move_x");
inline constexpr auto move_y_action = contracts::action_id("move_y");
inline constexpr auto jump_action = contracts::action_id("jump");

[[nodiscard]] std::uint64_t integer_sqrt_ceil(std::uint64_t value) noexcept {
    std::uint64_t low = 0;
    std::uint64_t high = 65'536;
    while (low + 1 < high) {
        const auto middle = (low + high) / 2;
        if (middle * middle < value) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low * low == value ? low : high;
}

[[nodiscard]] contracts::GroundVectorQ15 clamp_to_unit(
    contracts::GroundVectorQ15 vector
) noexcept {
    const auto x = static_cast<std::int64_t>(vector.x);
    const auto y = static_cast<std::int64_t>(vector.y);
    const auto magnitude_squared = static_cast<std::uint64_t>(x * x + y * y);
    const auto limit = static_cast<std::uint64_t>(contracts::ground_axis_one) *
                       contracts::ground_axis_one;
    if (magnitude_squared <= limit || magnitude_squared == 0) {
        return vector;
    }
    const auto magnitude = static_cast<std::int64_t>(integer_sqrt_ceil(magnitude_squared));
    return {
        static_cast<std::int32_t>(x * contracts::ground_axis_one / magnitude),
        static_cast<std::int32_t>(y * contracts::ground_axis_one / magnitude),
    };
}

[[nodiscard]] contracts::GroundVectorQ15 transform_to_world(
    std::int32_t move_x,
    std::int32_t move_y,
    const contracts::CameraBasisQ15& basis
) noexcept {
    const auto input = clamp_to_unit({move_x, move_y});
    const auto world_x =
        static_cast<std::int64_t>(input.x) * basis.screen_right_world.x +
        static_cast<std::int64_t>(input.y) * basis.screen_forward_world.x;
    const auto world_y =
        static_cast<std::int64_t>(input.x) * basis.screen_right_world.y +
        static_cast<std::int64_t>(input.y) * basis.screen_forward_world.y;
    return clamp_to_unit({
        static_cast<std::int32_t>(world_x / contracts::ground_axis_one),
        static_cast<std::int32_t>(world_y / contracts::ground_axis_one),
    });
}

[[nodiscard]] bool valid_basis_vector(contracts::GroundVectorQ15 vector) noexcept {
    const auto x = static_cast<std::int64_t>(vector.x);
    const auto y = static_cast<std::int64_t>(vector.y);
    const auto length_squared = x * x + y * y;
    const auto target = static_cast<std::int64_t>(contracts::ground_axis_one) *
                        contracts::ground_axis_one;
    const auto tolerance = target / 100;
    return length_squared >= target - tolerance && length_squared <= target + tolerance;
}

[[nodiscard]] bool valid_basis(const contracts::CameraBasisQ15& basis) noexcept {
    if (basis.revision == 0 || !valid_basis_vector(basis.screen_right_world) ||
        !valid_basis_vector(basis.screen_forward_world)) {
        return false;
    }
    const auto dot =
        static_cast<std::int64_t>(basis.screen_right_world.x) * basis.screen_forward_world.x +
        static_cast<std::int64_t>(basis.screen_right_world.y) * basis.screen_forward_world.y;
    const auto target = static_cast<std::int64_t>(contracts::ground_axis_one) *
                        contracts::ground_axis_one;
    const auto determinant =
        static_cast<std::int64_t>(basis.screen_right_world.x) *
            basis.screen_forward_world.y -
        static_cast<std::int64_t>(basis.screen_right_world.y) *
            basis.screen_forward_world.x;
    const auto tolerance = target / 100;
    return dot >= -tolerance && dot <= tolerance && determinant >= target - tolerance &&
           determinant <= target + tolerance;
}

}  // namespace

SessionInputError SessionInputState::submit(
    std::span<const contracts::ScalarActionSample> samples
) noexcept {
    auto sequence = last_platform_sequence_;
    for (const auto& sample : samples) {
        if (sample.platform_sequence <= sequence) {
            return SessionInputError::out_of_order_platform_sequence;
        }
        const auto error = validate_sample(sample);
        if (error != SessionInputError::none) {
            return error;
        }
        sequence = sample.platform_sequence;
    }
    for (const auto& sample : samples) {
        apply_sample(sample);
        last_platform_sequence_ = sample.platform_sequence;
    }
    return SessionInputError::none;
}

SessionInputError SessionInputState::clear(
    contracts::PlatformSequence sequence,
    contracts::InputClearReason reason
) noexcept {
    if (sequence <= last_platform_sequence_) {
        return SessionInputError::out_of_order_platform_sequence;
    }
    last_platform_sequence_ = sequence;
    clear_values(reason);
    return SessionInputError::none;
}

SessionInputError SessionInputState::set_gameplay_enabled(
    bool enabled,
    contracts::PlatformSequence sequence
) noexcept {
    if (sequence <= last_platform_sequence_) {
        return SessionInputError::out_of_order_platform_sequence;
    }
    last_platform_sequence_ = sequence;
    if (gameplay_enabled_ != enabled) {
        gameplay_enabled_ = enabled;
        clear_values(contracts::InputClearReason::context_changed);
    }
    return SessionInputError::none;
}

SessionInputError SessionInputState::set_camera_basis(
    const contracts::CameraBasisQ15& basis
) noexcept {
    if (!valid_basis(basis)) {
        return SessionInputError::invalid_camera_basis;
    }
    if (basis.revision <= camera_basis_.revision) {
        return SessionInputError::stale_camera_basis;
    }
    camera_basis_ = basis;
    return SessionInputError::none;
}

SessionCommandBatch SessionInputState::commands_for_tick(
    contracts::TickIndex tick,
    contracts::StableActorKey actor,
    contracts::CommandSequence first_sequence
) noexcept {
    SessionCommandBatch batch{};
    const auto direction = gameplay_enabled_
                               ? transform_to_world(move_x_, move_y_, camera_basis_)
                               : contracts::GroundVectorQ15{};
    batch.commands[0] = {
        {tick, actor, first_sequence, contracts::SessionCommandType::move_intent},
        direction,
    };
    batch.size = 1;
    if (gameplay_enabled_ && jump_pressed_) {
        batch.commands[1] = {
            {tick, actor, first_sequence + 1, contracts::SessionCommandType::jump_pressed},
            {},
        };
        batch.size = 2;
    }
    jump_pressed_ = false;
    return batch;
}

contracts::PlatformSequence SessionInputState::last_platform_sequence() const noexcept {
    return last_platform_sequence_;
}

contracts::InputClearReason SessionInputState::last_clear_reason() const noexcept {
    return last_clear_reason_;
}

const contracts::CameraBasisQ15& SessionInputState::camera_basis() const noexcept {
    return camera_basis_;
}

bool SessionInputState::gameplay_enabled() const noexcept {
    return gameplay_enabled_;
}

SessionInputError SessionInputState::validate_sample(
    const contracts::ScalarActionSample& sample
) const noexcept {
    const auto* descriptor = contracts::find_action_descriptor(sample.action);
    if (descriptor == nullptr) {
        return SessionInputError::unknown_action;
    }
    if (!contracts::action_supports_context(*descriptor, gameplay_context)) {
        return SessionInputError::none;
    }
    if (sample.action != move_x_action && sample.action != move_y_action &&
        sample.action != jump_action) {
        return SessionInputError::unsupported_gameplay_action;
    }
    if (sample.action == move_x_action || sample.action == move_y_action) {
        if (descriptor->value_type != contracts::ActionValueType::axis1d ||
            sample.edge != contracts::ActionSampleEdge::value_changed || sample.repeated ||
            sample.value_q15 < -contracts::ground_axis_one ||
            sample.value_q15 > contracts::ground_axis_one) {
            return SessionInputError::invalid_sample;
        }
        return SessionInputError::none;
    }
    if (descriptor->value_type != contracts::ActionValueType::digital ||
        (sample.edge != contracts::ActionSampleEdge::pressed &&
         sample.edge != contracts::ActionSampleEdge::released) ||
        (sample.edge == contracts::ActionSampleEdge::pressed &&
         sample.value_q15 != contracts::ground_axis_one) ||
        (sample.edge == contracts::ActionSampleEdge::released && sample.value_q15 != 0) ||
        (sample.repeated && sample.edge != contracts::ActionSampleEdge::pressed)) {
        return SessionInputError::invalid_sample;
    }
    return SessionInputError::none;
}

void SessionInputState::apply_sample(const contracts::ScalarActionSample& sample) noexcept {
    const auto* descriptor = contracts::find_action_descriptor(sample.action);
    if (descriptor == nullptr ||
        !contracts::action_supports_context(*descriptor, gameplay_context) ||
        !gameplay_enabled_) {
        return;
    }
    if (sample.action == move_x_action) {
        move_x_ = sample.value_q15;
    } else if (sample.action == move_y_action) {
        move_y_ = sample.value_q15;
    } else if (sample.action == jump_action &&
               sample.edge == contracts::ActionSampleEdge::pressed && !sample.repeated) {
        jump_pressed_ = true;
    }
}

void SessionInputState::clear_values(contracts::InputClearReason reason) noexcept {
    move_x_ = 0;
    move_y_ = 0;
    jump_pressed_ = false;
    last_clear_reason_ = reason;
}

}  // namespace tgd::gameplay
