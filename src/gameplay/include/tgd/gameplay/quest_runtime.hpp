#pragma once

#include <tgd/contracts/quest_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class QuestLifecycle : std::uint8_t {
    uninitialized,
    ready,
    running,
    paused,
    resolved,
    destroyed,
};

enum class QuestError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_definition,
    too_many_stages,
    too_many_objectives,
    duplicate_objective,
    invalid_command,
    tick_regressed,
    stale_command_sequence,
    unknown_objective,
    objective_not_active,
};

enum class QuestObjectiveState : std::uint8_t {
    unknown,
    locked,
    active,
    completed,
};

struct QuestApplyResult final {
    QuestError error{QuestError::none};
    bool accepted{};
    bool stage_advanced{};
    bool quest_resolved{};
};

class IQuestEventSink {
  public:
    virtual ~IQuestEventSink() = default;
    virtual void publish(std::span<const contracts::QuestEvent> events) noexcept = 0;
};

class IQuestRuntime {
  public:
    virtual ~IQuestRuntime() = default;

    [[nodiscard]] virtual QuestError initialize(
        const contracts::VerticalSliceDefinition& definition,
        contracts::StableActorKey player_actor
    ) noexcept = 0;
    [[nodiscard]] virtual QuestError start() noexcept = 0;
    [[nodiscard]] virtual QuestError pause() noexcept = 0;
    [[nodiscard]] virtual QuestError resume() noexcept = 0;
    [[nodiscard]] virtual QuestError destroy() noexcept = 0;
    [[nodiscard]] virtual QuestApplyResult apply(
        const contracts::QuestCommand& command,
        IQuestEventSink& sink
    ) noexcept = 0;
    [[nodiscard]] virtual QuestObjectiveState objective_state(
        contracts::StableContentKey objective
    ) const noexcept = 0;
    [[nodiscard]] virtual const contracts::QuestSnapshot& snapshot() const noexcept = 0;
};

class DeterministicQuestRuntime final : public IQuestRuntime {
  public:
    static constexpr std::size_t stage_capacity = 16;
    static constexpr std::size_t objective_capacity = 64;
    static constexpr std::size_t event_capacity = 2;

    [[nodiscard]] QuestError initialize(
        const contracts::VerticalSliceDefinition& definition,
        contracts::StableActorKey player_actor
    ) noexcept override;
    [[nodiscard]] QuestError start() noexcept override;
    [[nodiscard]] QuestError pause() noexcept override;
    [[nodiscard]] QuestError resume() noexcept override;
    [[nodiscard]] QuestError destroy() noexcept override;
    [[nodiscard]] QuestApplyResult apply(
        const contracts::QuestCommand& command,
        IQuestEventSink& sink
    ) noexcept override;
    [[nodiscard]] QuestObjectiveState objective_state(
        contracts::StableContentKey objective
    ) const noexcept override;
    [[nodiscard]] const contracts::QuestSnapshot& snapshot() const noexcept override;

    [[nodiscard]] QuestLifecycle lifecycle() const noexcept;

  private:
    [[nodiscard]] QuestError validate_definition(
        const contracts::VerticalSliceDefinition& definition
    ) const noexcept;
    [[nodiscard]] std::size_t objective_stage(
        contracts::StableContentKey objective
    ) const noexcept;
    [[nodiscard]] bool is_completed(contracts::StableContentKey objective) const noexcept;
    [[nodiscard]] std::size_t completed_in_stage(std::size_t stage_index) const noexcept;
    [[nodiscard]] bool active_stage_complete() const noexcept;
    void refresh_snapshot() noexcept;
    void update_checksum() noexcept;

    QuestLifecycle lifecycle_{QuestLifecycle::uninitialized};
    const contracts::VerticalSliceDefinition* definition_{};
    contracts::StableActorKey player_actor_{};
    std::size_t stage_index_{};
    std::array<contracts::StableContentKey, objective_capacity> completed_objectives_{};
    std::size_t completed_objective_count_{};
    contracts::CommandSequence last_command_sequence_{};
    contracts::QuestSnapshot snapshot_{};
    std::array<contracts::QuestEvent, event_capacity> events_{};
};

}  // namespace tgd::gameplay
