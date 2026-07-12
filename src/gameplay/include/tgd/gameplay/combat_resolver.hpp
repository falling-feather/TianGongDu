#pragma once

#include <tgd/contracts/combat_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class CombatLifecycle : std::uint8_t {
    uninitialized,
    ready,
    running,
    paused,
    destroyed,
};

enum class CombatError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_config,
    too_many_actors,
    too_many_abilities,
    duplicate_actor,
    duplicate_ability,
    duplicate_trigger,
    invalid_command,
    duplicate_command_key,
    command_targets_past_tick,
    command_queue_full,
    pose_targets_wrong_tick,
    invalid_pose_update,
    duplicate_pose_update,
    pose_update_queue_full,
    event_capacity_exceeded,
    retry_targets_wrong_tick,
    retry_not_allowed,
    stale_retry_sequence,
};

class ICombatEventSink {
  public:
    virtual ~ICombatEventSink() = default;
    virtual void publish(std::span<const contracts::CombatEvent> events) noexcept = 0;
};

class ICombatResolver {
  public:
    virtual ~ICombatResolver() = default;

    [[nodiscard]] virtual CombatError initialize(
        std::span<const contracts::CombatActorConfig> actors,
        std::span<const contracts::AbilityDefinition> abilities
    ) noexcept = 0;
    [[nodiscard]] virtual CombatError start() noexcept = 0;
    [[nodiscard]] virtual CombatError pause() noexcept = 0;
    [[nodiscard]] virtual CombatError resume() noexcept = 0;
    [[nodiscard]] virtual CombatError destroy() noexcept = 0;
    [[nodiscard]] virtual CombatError submit(
        std::span<const contracts::CombatCommand> commands
    ) noexcept = 0;
    [[nodiscard]] virtual CombatError synchronize_poses(
        std::span<const contracts::CombatPoseUpdate> updates
    ) noexcept = 0;
    [[nodiscard]] virtual CombatError retry_from_initial(
        const contracts::SafePointRetryCommand& command,
        ICombatEventSink& sink
    ) noexcept = 0;
    [[nodiscard]] virtual CombatError activate_group(
        const contracts::SafePointRetryCommand& command,
        std::span<const contracts::StableActorKey> actor_keys,
        ICombatEventSink& sink
    ) noexcept = 0;
    [[nodiscard]] virtual CombatError advance_one_tick(ICombatEventSink& sink) noexcept = 0;

    [[nodiscard]] virtual contracts::TickIndex current_tick() const noexcept = 0;
    [[nodiscard]] virtual std::span<const contracts::CombatActorSnapshot> actors() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t checksum() const noexcept = 0;
};

class DeterministicCombatResolver final : public ICombatResolver {
  public:
    static constexpr std::size_t actor_capacity = 16;
    static constexpr std::size_t ability_capacity = 32;
    static constexpr std::size_t command_capacity = 256;
    static constexpr std::size_t event_capacity = 512;

    [[nodiscard]] CombatError initialize(
        std::span<const contracts::CombatActorConfig> actors,
        std::span<const contracts::AbilityDefinition> abilities
    ) noexcept override;
    [[nodiscard]] CombatError start() noexcept override;
    [[nodiscard]] CombatError pause() noexcept override;
    [[nodiscard]] CombatError resume() noexcept override;
    [[nodiscard]] CombatError destroy() noexcept override;
    [[nodiscard]] CombatError submit(
        std::span<const contracts::CombatCommand> commands
    ) noexcept override;
    [[nodiscard]] CombatError synchronize_poses(
        std::span<const contracts::CombatPoseUpdate> updates
    ) noexcept override;
    [[nodiscard]] CombatError retry_from_initial(
        const contracts::SafePointRetryCommand& command,
        ICombatEventSink& sink
    ) noexcept override;
    [[nodiscard]] CombatError activate_group(
        const contracts::SafePointRetryCommand& command,
        std::span<const contracts::StableActorKey> actor_keys,
        ICombatEventSink& sink
    ) noexcept override;
    [[nodiscard]] CombatError advance_one_tick(ICombatEventSink& sink) noexcept override;

    [[nodiscard]] CombatLifecycle lifecycle() const noexcept;
    [[nodiscard]] contracts::TickIndex current_tick() const noexcept override;
    [[nodiscard]] std::span<const contracts::CombatActorSnapshot> actors() const noexcept override;
    [[nodiscard]] std::uint64_t checksum() const noexcept override;

  private:
    struct ActorRuntime final {
        std::uint16_t active_ability{ability_capacity};
        contracts::StableActorKey target{};
        contracts::TickIndex ability_started_tick{};
        contracts::TickIndex evade_until_tick{};
        contracts::TickIndex next_stamina_recovery_tick{};
        contracts::TickIndex next_poise_recovery_tick{};
        bool hit_applied{};
    };

    [[nodiscard]] CombatError validate_config(
        std::span<const contracts::CombatActorConfig> actors,
        std::span<const contracts::AbilityDefinition> abilities
    ) const noexcept;
    [[nodiscard]] bool valid_command(const contracts::CombatCommand& command) const noexcept;
    [[nodiscard]] bool duplicate_command_key(
        const contracts::CombatCommand& command,
        std::span<const contracts::CombatCommand> pending,
        std::size_t pending_index
    ) const noexcept;
    [[nodiscard]] bool duplicate_pose_update(
        const contracts::CombatPoseUpdate& update,
        std::span<const contracts::CombatPoseUpdate> pending,
        std::size_t pending_index
    ) const noexcept;
    [[nodiscard]] std::size_t actor_index(contracts::StableActorKey actor) const noexcept;
    [[nodiscard]] std::size_t ability_index(
        contracts::CombatCommandType trigger,
        contracts::StableContentKey stance
    ) const noexcept;
    [[nodiscard]] bool actor_allows_stance(
        std::size_t actor_index,
        contracts::StableContentKey stance
    ) const noexcept;
    [[nodiscard]] bool target_in_range(
        const contracts::CombatActorSnapshot& source,
        const contracts::CombatActorSnapshot& target,
        const contracts::AbilityDefinition& ability
    ) const noexcept;
    void restore_actor(std::size_t index) noexcept;
    void sort_commands() noexcept;
    void compact_commands(std::size_t consumed) noexcept;
    [[nodiscard]] bool emit(contracts::CombatEvent event) noexcept;
    [[nodiscard]] bool process_command(const contracts::CombatCommand& command) noexcept;
    [[nodiscard]] bool resolve_actor(std::size_t index, contracts::TickIndex tick) noexcept;
    [[nodiscard]] bool recover_actor(std::size_t index, contracts::TickIndex tick) noexcept;
    [[nodiscard]] bool resolve_hit(
        std::size_t source_index,
        std::size_t target_index,
        const contracts::AbilityDefinition& ability,
        contracts::TickIndex tick
    ) noexcept;
    void update_checksum() noexcept;

    CombatLifecycle lifecycle_{CombatLifecycle::uninitialized};
    contracts::TickIndex current_tick_{};
    std::array<contracts::CombatActorConfig, actor_capacity> actor_configs_{};
    std::array<contracts::CombatActorSnapshot, actor_capacity> actor_snapshots_{};
    std::array<ActorRuntime, actor_capacity> actor_runtime_{};
    std::size_t actor_count_{};
    std::array<contracts::AbilityDefinition, ability_capacity> abilities_{};
    std::size_t ability_count_{};
    std::array<contracts::CombatCommand, command_capacity> commands_{};
    std::size_t command_count_{};
    std::array<contracts::CombatPoseUpdate, actor_capacity> pose_updates_{};
    std::size_t pose_update_count_{};
    std::array<contracts::CombatEvent, event_capacity> events_{};
    std::size_t event_count_{};
    contracts::CommandSequence last_retry_sequence_{};
    std::uint64_t checksum_{};
};

}  // namespace tgd::gameplay
