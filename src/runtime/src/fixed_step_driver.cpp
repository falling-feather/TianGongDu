#include <tgd/runtime/fixed_step_driver.hpp>

#include <algorithm>

namespace tgd::runtime {

void FixedStepDriver::set_visible(bool visible) noexcept {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    accumulator_scaled_ = 0;
    discard_next_elapsed_ = visible;
}

bool FixedStepDriver::visible() const noexcept {
    return visible_;
}

FrameAdvanceResult FixedStepDriver::advance_frame(
    GameSession& session,
    std::uint64_t elapsed_nanoseconds,
    std::uint32_t tick_budget
) noexcept {
    FrameAdvanceResult result{};
    if (!visible_) {
        return result;
    }
    if (session.lifecycle() == GameSessionLifecycle::paused) {
        accumulator_scaled_ = 0;
        discard_next_elapsed_ = true;
        return result;
    }
    if (session.lifecycle() != GameSessionLifecycle::running) {
        result.error = GameSessionError::invalid_lifecycle;
        return result;
    }
    if (discard_next_elapsed_) {
        discard_next_elapsed_ = false;
        return result;
    }

    const auto clamped_elapsed = std::min(elapsed_nanoseconds, max_visible_delta_nanoseconds);
    accumulator_scaled_ += clamped_elapsed * 60ULL;
    const auto due_ticks = accumulator_scaled_ / scaled_tick;
    const auto allowed_ticks = std::min<std::uint64_t>(
        {due_ticks, max_ticks_per_frame, tick_budget}
    );
    if (allowed_ticks > 0) {
        const auto advanced = session.advance(static_cast<std::uint32_t>(allowed_ticks));
        result.error = advanced.error;
        result.executed_ticks = advanced.executed_ticks;
        accumulator_scaled_ -= static_cast<std::uint64_t>(advanced.executed_ticks) * scaled_tick;
    }

    if (due_ticks > max_ticks_per_frame) {
        result.simulation_overrun = true;
        const auto remainder = accumulator_scaled_ % scaled_tick;
        result.dropped_scaled_time = accumulator_scaled_ - remainder;
        accumulator_scaled_ = remainder;
    }
    result.interpolation_numerator = accumulator_scaled_;
    return result;
}

}  // namespace tgd::runtime
