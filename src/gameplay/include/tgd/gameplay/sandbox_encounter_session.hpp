#pragma once

#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_gameplay_binding.hpp>
#include <tgd/gameplay/combat_resolver.hpp>
#include <tgd/gameplay/encounter_director.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tgd::gameplay {

enum class SandboxEncounterBuildError : std::uint8_t {
    none = 0,
    invalid_definition = 1,
    invalid_binding = 2,
    capacity_exceeded = 3,
    combat_initialize_failed = 4,
    director_initialize_failed = 5,
    invalid = 255,
};

enum class SandboxEncounterEventDisposition : std::uint8_t {
    applied = 1,
    repeated = 2,
    unknown_target = 3,
    invalid_state = 4,
    invalid = 255,
};

enum class SandboxEncounterAttack : std::uint8_t {
    light = 1,
    heavy = 2,
    invalid = 255,
};

enum class SandboxEncounterAttackDisposition : std::uint8_t {
    queued = 1,
    already_queued = 2,
    player_defeated = 3,
    no_active_target = 4,
    invalid_state = 5,
    invalid = 255,
};

enum class SandboxEncounterStepDisposition : std::uint8_t {
    advanced = 1,
    player_defeated = 2,
    terminal_completed = 3,
    invalid_state = 4,
    combat_rejected = 5,
    director_rejected = 6,
    activation_rejected = 7,
    invalid = 255,
};

enum class SandboxWaveRuntimeState : std::uint8_t {
    waiting = 1,
    active = 2,
    completed = 3,
    invalid = 255,
};

enum class SandboxObjectiveRuntimeState : std::uint8_t {
    locked = 1,
    active = 2,
    completed = 3,
    invalid = 255,
};

struct SandboxEncounterSnapshot final {
    bool initialized{};
    contracts::TickIndex tick{};
    contracts::StableActorKey player_actor{};
    std::int32_t player_health{};
    std::int32_t player_health_max{};
    bool player_defeated{};
    contracts::StableContentKey active_wave{};
    std::uint16_t active_hostile_count{};
    std::uint16_t defeated_hostile_count{};
    std::uint16_t completed_wave_count{};
    std::uint16_t completed_objective_count{};
    bool terminal_completed{};
    std::uint32_t accepted_attack_count{};
    std::uint32_t repeated_trigger_count{};
    std::uint64_t checksum{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxEncounterSnapshot&,
        const SandboxEncounterSnapshot&
    ) noexcept = default;
};

struct SandboxEncounterStepResult final {
    SandboxEncounterStepDisposition disposition{
        SandboxEncounterStepDisposition::invalid
    };
    SandboxEncounterSnapshot snapshot{};
};

class SandboxEncounterSession final {
  public:
    static constexpr std::size_t hostile_capacity =
        contracts::sandbox_actor_capacity;
    static constexpr std::size_t combat_actor_capacity = hostile_capacity + 1U;
    static constexpr std::size_t wave_capacity = contracts::sandbox_wave_capacity;
    static constexpr std::size_t spawn_capacity =
        contracts::sandbox_wave_spawn_capacity;
    static constexpr std::size_t objective_capacity =
        contracts::sandbox_objective_capacity;

    [[nodiscard]] SandboxEncounterBuildError initialize(
        const contracts::SandboxDefinition& definition,
        const contracts::SandboxGameplayBindingDefinition& binding,
        contracts::StableActorKey player_actor,
        contracts::GroundPoseMm player_pose
    ) noexcept;

    [[nodiscard]] SandboxEncounterEventDisposition notify_mechanism_activated(
        contracts::StableContentKey mechanism
    ) noexcept;
    [[nodiscard]] SandboxEncounterAttackDisposition queue_player_attack(
        SandboxEncounterAttack attack
    ) noexcept;
    [[nodiscard]] SandboxEncounterStepResult advance_one_tick(
        contracts::GroundPoseMm player_pose
    ) noexcept;
    [[nodiscard]] SandboxEncounterBuildError restart(
        contracts::GroundPoseMm player_pose
    ) noexcept;

    [[nodiscard]] SandboxEncounterSnapshot snapshot() const noexcept;
    [[nodiscard]] std::span<const contracts::CombatActorSnapshot>
    combat_actors() const noexcept;
    [[nodiscard]] const contracts::SandboxActorGameplayBinding*
    actor_binding(contracts::StableActorKey actor) const noexcept;
    [[nodiscard]] SandboxWaveRuntimeState wave_state(
        contracts::StableContentKey wave
    ) const noexcept;
    [[nodiscard]] SandboxObjectiveRuntimeState objective_state(
        contracts::StableContentKey objective
    ) const noexcept;

  private:
    struct WaveRuntime final {
        contracts::StableContentKey id{};
        SandboxWaveRuntimeState state{SandboxWaveRuntimeState::waiting};
        contracts::TickIndex activated_tick{};
    };

    struct SpawnRuntime final {
        contracts::StableContentKey wave{};
        contracts::StableActorKey actor{};
        std::uint32_t delay_ticks{};
        std::uint16_t spawn_order{};
        bool activated{};
    };

    struct ObjectiveRuntime final {
        contracts::StableContentKey id{};
        SandboxObjectiveRuntimeState state{SandboxObjectiveRuntimeState::locked};
    };

    class EventCollector final : public ICombatEventSink {
      public:
        void clear() noexcept;
        void publish(std::span<const contracts::CombatEvent> events) noexcept override;
        [[nodiscard]] std::span<const contracts::CombatEvent> events() const noexcept;
        [[nodiscard]] bool overflowed() const noexcept;

      private:
        std::array<contracts::CombatEvent, DeterministicCombatResolver::event_capacity>
            values_{};
        std::size_t count_{};
        bool overflowed_{};
    };

    [[nodiscard]] SandboxEncounterBuildError build_runtime(
        contracts::GroundPoseMm player_pose
    ) noexcept;
    [[nodiscard]] SandboxEncounterStepResult advance_one_tick_candidate(
        contracts::GroundPoseMm player_pose
    ) noexcept;
    [[nodiscard]] bool evaluate_graph() noexcept;
    [[nodiscard]] bool trigger_satisfied(
        const contracts::SandboxTriggerDefinition& trigger
    ) const noexcept;
    [[nodiscard]] bool completion_satisfied(
        const contracts::SandboxObjectiveCompletionDefinition& completion
    ) const noexcept;
    [[nodiscard]] bool activate_due_spawns() noexcept;
    [[nodiscard]] bool activate_spawn_group(
        std::span<const contracts::EncounterActorPlacementDefinition> placements,
        contracts::EncounterActivationMode mode
    ) noexcept;
    [[nodiscard]] bool update_wave_completion() noexcept;
    [[nodiscard]] const contracts::CombatActorSnapshot* find_combat_actor(
        contracts::StableActorKey actor
    ) const noexcept;
    [[nodiscard]] contracts::StableActorKey nearest_active_hostile() const noexcept;
    void refresh_snapshot() noexcept;

    const contracts::SandboxDefinition* definition_{};
    const contracts::SandboxGameplayBindingDefinition* binding_{};
    contracts::StableActorKey player_actor_{};

    std::array<contracts::CombatActorConfig, combat_actor_capacity> actor_configs_{};
    std::size_t actor_config_count_{};
    std::array<contracts::AbilityDefinition, 4> abilities_{};
    std::array<contracts::EncounterActorDutyDefinition, hostile_capacity> duties_{};
    std::size_t duty_count_{};

    std::array<WaveRuntime, wave_capacity> waves_{};
    std::size_t wave_count_{};
    std::array<SpawnRuntime, spawn_capacity> spawns_{};
    std::size_t spawn_count_{};
    std::array<ObjectiveRuntime, objective_capacity> objectives_{};
    std::size_t objective_count_{};
    std::array<contracts::StableContentKey, contracts::sandbox_mechanism_capacity>
        activated_mechanisms_{};
    std::size_t activated_mechanism_count_{};

    DeterministicCombatResolver combat_{};
    DeterministicEncounterDirector director_{};
    EventCollector events_{};
    SandboxEncounterAttack pending_attack_{SandboxEncounterAttack::invalid};
    contracts::CommandSequence sequence_cursor_{};
    std::uint32_t accepted_attack_count_{};
    std::uint32_t repeated_trigger_count_{};
    bool has_activated_group_{};
    bool initialized_{};
    SandboxEncounterSnapshot snapshot_{};
};

}  // namespace tgd::gameplay
