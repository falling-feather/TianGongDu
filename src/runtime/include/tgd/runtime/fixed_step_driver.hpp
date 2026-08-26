#pragma once

#include <tgd/runtime/game_session.hpp>

#include <cstdint>

namespace tgd::runtime {

struct FrameAdvanceResult final {
    GameSessionError error{GameSessionError::none};
    std::uint32_t executed_ticks{};
    bool simulation_overrun{};
    std::uint64_t dropped_scaled_time{};
    std::uint64_t interpolation_numerator{};
};

class FixedStepDriver final {
  public:
    static constexpr std::uint64_t scaled_tick = 1'000'000'000ULL;
    static constexpr std::uint64_t max_visible_delta_nanoseconds = 250'000'000ULL;
    static constexpr std::uint32_t max_ticks_per_frame = 4;

    void set_visible(bool visible) noexcept;
    [[nodiscard]] bool visible() const noexcept;
    [[nodiscard]] FrameAdvanceResult advance_frame(
        GameSession& session,
        std::uint64_t elapsed_nanoseconds,
        std::uint32_t tick_budget = max_ticks_per_frame
    ) noexcept;

  private:
    bool visible_{true};
    bool discard_next_elapsed_{};
    std::uint64_t accumulator_scaled_{};
};

}  // namespace tgd::runtime
