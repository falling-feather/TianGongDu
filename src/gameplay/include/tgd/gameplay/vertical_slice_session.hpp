#pragma once

#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/session_types.hpp>
#include <tgd/gameplay/quest_runtime.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/runtime/game_session.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace tgd::gameplay {

enum class VerticalSliceLifecycle : std::uint8_t {
    uninitialized,
    ready_at_safe_point,
    running,
    paused,
    resolved,
    destroyed,
};

enum class VerticalSliceError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_definition,
    missing_collision_world,
    movement_session_error,
    quest_runtime_error,
    unknown_objective,
    objective_not_active,
    invalid_selection,
    selection_conflict,
};

struct VerticalSliceSnapshot final {
    contracts::TickIndex tick{};
    contracts::ContentId slice_id{};
    contracts::ContentId beat_id{};
    contracts::ContentId cell_id{};
    contracts::ContentId safe_point_id{};
    contracts::GroundPoseMm safe_point_pose{};
    contracts::GroundPoseMm player_pose{};
    std::uint16_t beat_index{};
    std::uint16_t beat_count{};
    std::uint16_t completed_objectives{};
    std::uint16_t required_objectives{};
    std::uint16_t selected_choices{};
    std::uint64_t simulation_ticks{};
    bool resolved{};
    std::uint64_t checksum{};
};

struct CompleteObjectiveResult final {
    VerticalSliceError error{VerticalSliceError::none};
    bool accepted{};
    bool beat_advanced{};
    bool slice_resolved{};
};

struct VerticalSliceAdvanceResult final {
    VerticalSliceError error{VerticalSliceError::none};
    std::uint32_t executed_ticks{};
};

class VerticalSliceSession final {
  public:
    static constexpr std::size_t max_beats = 16;
    static constexpr std::size_t max_objectives = 64;

    [[nodiscard]] VerticalSliceError initialize(
        const contracts::VerticalSliceDefinition& definition,
        std::unique_ptr<runtime::ICollisionWorld> collision_world
    ) noexcept;
    [[nodiscard]] VerticalSliceError start() noexcept;
    [[nodiscard]] VerticalSliceError pause() noexcept;
    [[nodiscard]] VerticalSliceError resume() noexcept;
    [[nodiscard]] VerticalSliceError destroy() noexcept;

    [[nodiscard]] VerticalSliceError submit_movement(
        std::span<const contracts::SessionCommand> commands
    ) noexcept;
    [[nodiscard]] VerticalSliceError retry_from_safe_point(
        const contracts::SafePointRetryCommand& command
    ) noexcept;
    [[nodiscard]] VerticalSliceAdvanceResult advance(std::uint32_t tick_budget) noexcept;
    [[nodiscard]] CompleteObjectiveResult complete_objective(
        contracts::StableContentKey objective
    ) noexcept;
    [[nodiscard]] CompleteObjectiveResult complete_objective(
        contracts::StableContentKey objective,
        IQuestEventSink& sink
    ) noexcept;
    [[nodiscard]] CompleteObjectiveResult complete_objective(
        contracts::StableContentKey objective,
        contracts::StableContentKey selection
    ) noexcept;
    [[nodiscard]] CompleteObjectiveResult complete_objective(
        contracts::StableContentKey objective,
        contracts::StableContentKey selection,
        IQuestEventSink& sink
    ) noexcept;

    [[nodiscard]] VerticalSliceLifecycle lifecycle() const noexcept;
    [[nodiscard]] std::uint32_t generation() const noexcept;
    [[nodiscard]] runtime::GameSessionError last_movement_error() const noexcept;
    [[nodiscard]] const contracts::VerticalSliceDefinition* definition() const noexcept;
    [[nodiscard]] const VerticalSliceSnapshot& previous_snapshot() const noexcept;
    [[nodiscard]] const VerticalSliceSnapshot& current_snapshot() const noexcept;
    [[nodiscard]] const contracts::QuestSnapshot& quest_snapshot() const noexcept;
    [[nodiscard]] const IQuestRuntime& quest_runtime() const noexcept;
    [[nodiscard]] QuestObjectiveState objective_state(
        contracts::StableContentKey objective
    ) const noexcept;
    [[nodiscard]] contracts::StableContentKey selected_option(
        contracts::StableContentKey objective
    ) const noexcept;

  private:
    [[nodiscard]] bool valid_definition(
        const contracts::VerticalSliceDefinition& definition
    ) const noexcept;
    [[nodiscard]] bool commit_safe_point_for_beat(
        contracts::StableContentKey beat
    ) noexcept;
    void refresh_snapshot() noexcept;
    void update_checksum() noexcept;

    VerticalSliceLifecycle lifecycle_{VerticalSliceLifecycle::uninitialized};
    std::uint32_t generation_{};
    const contracts::VerticalSliceDefinition* definition_{};
    runtime::GameSession movement_{};
    runtime::GameSessionError last_movement_error_{runtime::GameSessionError::none};
    DeterministicQuestRuntime quest_{};
    contracts::CommandSequence quest_command_sequence_{1};
    contracts::CommandSequence safe_point_command_sequence_{1};
    std::uint64_t simulation_ticks_{};
    VerticalSliceSnapshot previous_snapshot_{};
    VerticalSliceSnapshot current_snapshot_{};
};

}  // namespace tgd::gameplay
