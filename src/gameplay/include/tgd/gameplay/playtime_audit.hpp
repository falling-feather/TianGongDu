#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class PlaytimeAuditLifecycle : std::uint8_t {
    uninitialized,
    ready,
    running,
    paused,
    resolved,
    destroyed,
};

enum class PlaytimeAuditError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_definition,
    invalid_activity,
    beat_order_violation,
    counter_overflow,
};

enum class PlaytimeActivityKind : std::uint8_t {
    movement,
    jump,
    combat,
    interaction,
    authored_progress,
};

struct PlaytimeAuditSnapshot final {
    std::uint64_t total_ticks{};
    std::uint64_t eligible_ticks{};
    std::uint64_t idle_ticks{};
    std::uint64_t failure_retry_ticks{};
    std::uint64_t current_attempt_ticks{};
    std::uint64_t current_beat_eligible_ticks{};
    std::uint64_t current_beat_target_ticks{};
    std::uint64_t playable_target_ticks{};
    std::uint16_t beat_index{};
    std::uint16_t beat_count{};
    std::uint16_t beat_targets_met{};
    std::uint16_t activity_grace_remaining_ticks{};
    bool current_beat_target_met{};
    bool playable_target_met{};
    bool resolved{};
    std::uint64_t checksum{};
};

class DeterministicPlaytimeAudit final {
  public:
    static constexpr std::size_t max_beats = 16;
    static constexpr std::uint64_t ticks_per_second = 60;
    static constexpr std::uint64_t ticks_per_minute = 60 * ticks_per_second;

    [[nodiscard]] PlaytimeAuditError initialize(
        std::span<const contracts::VerticalSliceBeatDefinition> beats,
        std::uint16_t playable_target_minutes,
        std::uint16_t activity_grace_ticks
    ) noexcept;
    [[nodiscard]] PlaytimeAuditError start() noexcept;
    [[nodiscard]] PlaytimeAuditError pause() noexcept;
    [[nodiscard]] PlaytimeAuditError resume() noexcept;
    [[nodiscard]] PlaytimeAuditError destroy() noexcept;

    [[nodiscard]] PlaytimeAuditError note_activity(PlaytimeActivityKind kind) noexcept;
    [[nodiscard]] PlaytimeAuditError advance(std::uint32_t executed_ticks) noexcept;
    [[nodiscard]] PlaytimeAuditError discard_current_attempt_for_retry() noexcept;
    [[nodiscard]] PlaytimeAuditError commit_current_beat(
        std::uint16_t next_beat_index
    ) noexcept;
    [[nodiscard]] PlaytimeAuditError resolve() noexcept;

    [[nodiscard]] PlaytimeAuditLifecycle lifecycle() const noexcept;
    [[nodiscard]] const PlaytimeAuditSnapshot& snapshot() const noexcept;

  private:
    void commit_attempt() noexcept;
    void refresh_snapshot() noexcept;
    void update_checksum() noexcept;

    PlaytimeAuditLifecycle lifecycle_{PlaytimeAuditLifecycle::uninitialized};
    std::array<std::uint64_t, max_beats> beat_target_ticks_{};
    std::array<std::uint64_t, max_beats> committed_beat_eligible_ticks_{};
    std::uint16_t beat_count_{};
    std::uint16_t beat_index_{};
    std::uint16_t committed_beat_targets_met_{};
    std::uint16_t activity_grace_ticks_{};
    std::uint16_t activity_grace_remaining_ticks_{};
    std::uint64_t playable_target_ticks_{};
    std::uint64_t committed_eligible_ticks_{};
    std::uint64_t committed_idle_ticks_{};
    std::uint64_t attempt_eligible_ticks_{};
    std::uint64_t attempt_idle_ticks_{};
    std::uint64_t failure_retry_ticks_{};
    PlaytimeAuditSnapshot snapshot_{};
};

}  // namespace tgd::gameplay
