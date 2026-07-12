#pragma once

#include <tgd/contracts/combat_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class EncounterDirectorError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_definition,
    invalid_actor_config,
    missing_player,
    missing_hostile,
    missing_ability,
    wrong_tick,
    invalid_snapshot,
    output_capacity_exceeded,
    retry_targets_wrong_tick,
    retry_not_allowed,
    stale_retry_sequence,
    activation_targets_wrong_tick,
    activation_not_allowed,
    stale_activation_sequence,
};

struct EncounterPlanBatch final {
    static constexpr std::size_t capacity = 16;

    std::array<contracts::CombatPoseUpdate, capacity> pose_updates{};
    std::size_t pose_update_count{};
    std::array<contracts::CombatCommand, capacity> commands{};
    std::size_t command_count{};

    [[nodiscard]] std::span<const contracts::CombatPoseUpdate> poses() const noexcept {
        return std::span{pose_updates}.first(pose_update_count);
    }

    [[nodiscard]] std::span<const contracts::CombatCommand> command_view() const noexcept {
        return std::span{commands}.first(command_count);
    }
};

struct EncounterPlanResult final {
    EncounterDirectorError error{EncounterDirectorError::none};
    EncounterPlanBatch batch{};
};

class IEncounterDirector {
  public:
    virtual ~IEncounterDirector() = default;

    [[nodiscard]] virtual EncounterDirectorError initialize(
        const contracts::EncounterDirectorDefinition& definition,
        std::span<const contracts::CombatActorConfig> actors,
        std::span<const contracts::AbilityDefinition> abilities
    ) noexcept = 0;
    [[nodiscard]] virtual EncounterPlanResult plan_tick(
        contracts::TickIndex tick,
        std::span<const contracts::CombatActorSnapshot> actors,
        contracts::CommandSequence first_sequence
    ) noexcept = 0;
    [[nodiscard]] virtual EncounterDirectorError retry_from_initial(
        const contracts::SafePointRetryCommand& command
    ) noexcept = 0;
    [[nodiscard]] virtual EncounterDirectorError activate_group(
        const contracts::EncounterActivationCommand& command,
        std::span<const contracts::EncounterActorPlacementDefinition> actor_placements,
        std::span<const contracts::CombatActorSnapshot> actors
    ) noexcept = 0;
    [[nodiscard]] virtual contracts::TickIndex current_tick() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t checksum() const noexcept = 0;
};

class DeterministicEncounterDirector final : public IEncounterDirector {
  public:
    static constexpr std::size_t hostile_capacity = EncounterPlanBatch::capacity - 1;
    static constexpr std::size_t ability_capacity = 32;
    static_assert(hostile_capacity == contracts::encounter_formation_slot_capacity);

    [[nodiscard]] EncounterDirectorError initialize(
        const contracts::EncounterDirectorDefinition& definition,
        std::span<const contracts::CombatActorConfig> actors,
        std::span<const contracts::AbilityDefinition> abilities
    ) noexcept override;
    [[nodiscard]] EncounterPlanResult plan_tick(
        contracts::TickIndex tick,
        std::span<const contracts::CombatActorSnapshot> actors,
        contracts::CommandSequence first_sequence
    ) noexcept override;
    [[nodiscard]] EncounterDirectorError retry_from_initial(
        const contracts::SafePointRetryCommand& command
    ) noexcept override;
    [[nodiscard]] EncounterDirectorError activate_group(
        const contracts::EncounterActivationCommand& command,
        std::span<const contracts::EncounterActorPlacementDefinition> actor_placements,
        std::span<const contracts::CombatActorSnapshot> actors
    ) noexcept override;
    [[nodiscard]] contracts::TickIndex current_tick() const noexcept override;
    [[nodiscard]] std::uint64_t checksum() const noexcept override;

  private:
    struct HostileRuntime final {
        contracts::StableActorKey actor{};
        contracts::GroundPoseMm home_pose{};
        std::uint8_t formation_slot{};
        contracts::TickIndex next_attack_tick{};
        std::uint32_t attack_count{};
    };

    struct AttackCandidate final {
        std::size_t runtime_index{};
        std::uint64_t distance_squared{};
    };

    [[nodiscard]] const contracts::CombatActorSnapshot* find_snapshot(
        std::span<const contracts::CombatActorSnapshot> actors,
        contracts::StableActorKey actor
    ) const noexcept;
    [[nodiscard]] const contracts::AbilityDefinition* find_ability(
        contracts::CombatCommandType trigger,
        contracts::StableContentKey stance
    ) const noexcept;
    [[nodiscard]] contracts::GroundPoseMm move_toward(
        const contracts::GroundPoseMm& source,
        const contracts::GroundPoseMm& target
    ) const noexcept;
    [[nodiscard]] contracts::GroundPoseMm formation_target(
        const contracts::GroundPoseMm& player,
        std::size_t hostile_index
    ) const noexcept;
    void update_checksum() noexcept;

    bool initialized_{};
    contracts::EncounterDirectorDefinition definition_{};
    std::array<HostileRuntime, hostile_capacity> hostiles_{};
    std::size_t hostile_count_{};
    std::array<contracts::AbilityDefinition, ability_capacity> abilities_{};
    std::size_t ability_count_{};
    contracts::TickIndex current_tick_{};
    contracts::CommandSequence last_boundary_sequence_{};
    std::uint64_t checksum_{};
};

}  // namespace tgd::gameplay
