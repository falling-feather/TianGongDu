#pragma once

#include <tgd/contracts/session_types.hpp>
#include <tgd/runtime/collision_world.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace tgd::runtime {

enum class GameSessionLifecycle : std::uint8_t {
    uninitialized,
    ready_at_safe_point,
    running,
    paused,
    destroyed,
};

enum class GameSessionError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_config,
    missing_collision_world,
    invalid_command,
    duplicate_command_key,
    command_targets_past_tick,
    command_queue_full,
    safe_point_targets_wrong_tick,
    stale_safe_point_sequence,
    invalid_safe_point,
    retry_targets_wrong_tick,
    stale_retry_sequence,
};

struct GameSessionConfig final {
    contracts::StableActorKey player_actor{1};
    contracts::GroundPoseMm initial_pose{};
    std::int32_t ground_height{};
    std::int32_t move_speed_mm_per_second{3'000};
    std::int32_t jump_speed_mm_per_second{4'800};
    std::int32_t gravity_mm_per_second_squared{9'600};
    std::int32_t collision_radius{250};
    std::int32_t collision_height{1'800};
};

struct PresentationSnapshot final {
    contracts::TickIndex tick{};
    contracts::StableActorKey player_actor{};
    contracts::GroundPoseMm player_pose{};
    std::int32_t vertical_velocity_mm_per_second{};
    bool grounded{true};
    std::uint64_t checksum{};
};

struct AdvanceTicksResult final {
    GameSessionError error{GameSessionError::none};
    std::uint32_t executed_ticks{};
};

class GameSession final {
  public:
    static constexpr std::size_t command_capacity = 256;

    [[nodiscard]] GameSessionError initialize(
        const GameSessionConfig& config,
        std::unique_ptr<ICollisionWorld> collision_world
    ) noexcept;
    [[nodiscard]] GameSessionError start() noexcept;
    [[nodiscard]] GameSessionError pause() noexcept;
    [[nodiscard]] GameSessionError resume() noexcept;
    [[nodiscard]] GameSessionError destroy() noexcept;

    [[nodiscard]] GameSessionError submit(
        std::span<const contracts::SessionCommand> commands
    ) noexcept;
    [[nodiscard]] GameSessionError commit_safe_point(
        const contracts::SafePointCommitCommand& command
    ) noexcept;
    [[nodiscard]] GameSessionError validate_safe_point_pose(
        const contracts::GroundPoseMm& pose
    ) const noexcept;
    [[nodiscard]] GameSessionError retry_from_safe_point(
        const contracts::SafePointRetryCommand& command
    ) noexcept;
    [[nodiscard]] AdvanceTicksResult advance(std::uint32_t tick_budget) noexcept;

    [[nodiscard]] GameSessionLifecycle lifecycle() const noexcept;
    [[nodiscard]] std::uint32_t generation() const noexcept;
    [[nodiscard]] std::size_t queued_command_count() const noexcept;
    [[nodiscard]] contracts::StableContentKey active_safe_point() const noexcept;
    [[nodiscard]] const contracts::GroundPoseMm& active_safe_point_pose() const noexcept;
    [[nodiscard]] const PresentationSnapshot& previous_snapshot() const noexcept;
    [[nodiscard]] const PresentationSnapshot& current_snapshot() const noexcept;

  private:
    [[nodiscard]] bool valid_config(const GameSessionConfig& config) const noexcept;
    [[nodiscard]] bool valid_command(const contracts::SessionCommand& command) const noexcept;
    [[nodiscard]] bool duplicate_command_key(
        const contracts::SessionCommand& command,
        std::span<const contracts::SessionCommand> pending,
        std::size_t pending_index
    ) const noexcept;
    void sort_commands() noexcept;
    void compact_commands(std::size_t consumed) noexcept;
    void simulate_tick(
        contracts::GroundVectorQ15 ground_direction,
        bool jump_pressed
    ) noexcept;
    void update_checksum() noexcept;

    GameSessionLifecycle lifecycle_{GameSessionLifecycle::uninitialized};
    std::uint32_t generation_{};
    GameSessionConfig config_{};
    std::unique_ptr<ICollisionWorld> collision_world_{};
    std::array<contracts::SessionCommand, command_capacity> commands_{};
    std::size_t command_count_{};
    contracts::StableContentKey active_safe_point_{};
    contracts::GroundPoseMm safe_point_pose_{};
    contracts::CommandSequence last_safe_point_sequence_{};
    contracts::CommandSequence last_retry_sequence_{};
    PresentationSnapshot previous_snapshot_{};
    PresentationSnapshot current_snapshot_{};
    std::int64_t movement_remainder_x_{};
    std::int64_t movement_remainder_y_{};
    std::int64_t vertical_remainder_{};
    std::int64_t gravity_remainder_{};
};

}  // namespace tgd::runtime
