#pragma once

#include <tgd/contracts/sandbox_gameplay_binding.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace tgd::gameplay {

struct SandboxPlayerRuntimeBinding final {
    contracts::ContentId player_content_id{};
    contracts::StableActorKey actor_key{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxPlayerRuntimeBinding&,
        const SandboxPlayerRuntimeBinding&
    ) noexcept = default;
};

enum class SandboxSessionBuildError : std::uint8_t {
    none = 0,
    already_initialized = 1,
    capacity_exceeded = 2,
    invalid_player_definition = 3,
    missing_player_runtime_binding = 4,
    invalid_player_runtime_id = 5,
    player_runtime_id_mismatch = 6,
    invalid_player_actor = 7,
    invalid_initial_safe_point = 8,
    invalid_gameplay_binding = 9,
    invalid_owned_state = 10,
    invalid = 255,
};

struct SandboxSessionBuildResult final {
    SandboxSessionBuildError error{SandboxSessionBuildError::invalid};
    contracts::SandboxGameplayBindingValidationResult binding_validation{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxSessionBuildResult&,
        const SandboxSessionBuildResult&
    ) noexcept = default;
};

struct SandboxGameplayEvent final {
    std::uint64_t sequence{};
    std::uint32_t generation{};
    contracts::TickIndex completed_tick{};
    contracts::SandboxGameplayEventKind kind{
        contracts::SandboxGameplayEventKind::invalid
    };
    contracts::StableActorKey actor{};
    contracts::StableContentKey interaction{};
    contracts::StableContentKey mechanism{};
    contracts::StableContentKey ground_blocker{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxGameplayEvent&,
        const SandboxGameplayEvent&
    ) noexcept = default;
};

struct SandboxOperateDispatch final {
    contracts::SandboxOperateResult result{};
    std::array<SandboxGameplayEvent, 2> events{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxOperateDispatch&,
        const SandboxOperateDispatch&
    ) noexcept = default;
};

struct SandboxSessionRetryCommand final {
    std::uint32_t generation{};
    contracts::CommandSequence sequence{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxSessionRetryCommand&,
        const SandboxSessionRetryCommand&
    ) noexcept = default;
};

enum class SandboxSessionRetryDisposition : std::uint8_t {
    restored = 1,
    stale_generation = 2,
    stale_sequence = 3,
    generation_exhausted = 4,
    invalid_state = 5,
    invalid = 255,
};

struct SandboxGenerationAdvance final {
    bool valid{};
    std::uint32_t generation{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxGenerationAdvance&,
        const SandboxGenerationAdvance&
    ) noexcept = default;
};

[[nodiscard]] constexpr SandboxGenerationAdvance sandbox_next_generation(
    std::uint32_t current
) noexcept {
    if (current == 0 || current == std::numeric_limits<std::uint32_t>::max()) {
        return {};
    }
    return {true, current + 1U};
}

struct SandboxSessionSnapshot final {
    std::uint32_t generation{};
    contracts::StableContentKey player_content{};
    contracts::StableActorKey player_actor{};
    contracts::StableContentKey retry_safe_point{};
    contracts::GroundPoseMm player_pose{};
    std::uint32_t player_facing_millidegrees{};
    contracts::CommandSequence last_command_sequence{};
    contracts::TickIndex last_completed_tick{};
    std::uint64_t last_event_sequence{};
    std::uint16_t interaction_count{};
    std::uint16_t mechanism_count{};
    std::uint16_t ground_blocker_count{};
    std::uint64_t checksum{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxSessionSnapshot&,
        const SandboxSessionSnapshot&
    ) noexcept = default;
};

class SandboxSession final {
  public:
    static constexpr std::size_t interaction_capacity =
        contracts::sandbox_interaction_capacity;
    static constexpr std::size_t mechanism_capacity =
        contracts::sandbox_mechanism_capacity;
    static constexpr std::size_t ground_blocker_capacity =
        contracts::sandbox_ground_blocker_capacity;

    [[nodiscard]] SandboxSessionBuildResult initialize(
        const contracts::SandboxDefinition& validated_core,
        const contracts::SandboxGameplayBindingDefinition& gameplay_binding,
        const SandboxPlayerRuntimeBinding& player_binding
    ) noexcept;

    [[nodiscard]] SandboxOperateDispatch submit_operate(
        const contracts::SandboxOperateCommand& command
    ) noexcept;

    [[nodiscard]] SandboxSessionRetryDisposition retry(
        const SandboxSessionRetryCommand& command
    ) noexcept;

    [[nodiscard]] const SandboxSessionSnapshot& snapshot() const noexcept;
    [[nodiscard]] contracts::SandboxInteractionState interaction_state(
        contracts::StableContentKey interaction
    ) const noexcept;
    [[nodiscard]] contracts::SandboxMechanismState mechanism_state(
        contracts::StableContentKey mechanism
    ) const noexcept;
    [[nodiscard]] contracts::SandboxGroundBlockerState ground_blocker_state(
        contracts::StableContentKey ground_blocker
    ) const noexcept;

  private:
    static constexpr std::uint16_t invalid_index =
        std::numeric_limits<std::uint16_t>::max();

    struct InteractionRecord final {
        contracts::StableContentKey key{};
        contracts::GroundPoseMm pose{};
        std::int32_t range_mm{};
        std::uint16_t mechanism_index{invalid_index};
        contracts::SandboxInteractionState state{
            contracts::SandboxInteractionState::invalid
        };
    };

    struct MechanismRecord final {
        contracts::StableContentKey key{};
        std::uint16_t ground_blocker_index{invalid_index};
        contracts::SandboxMechanismState state{contracts::SandboxMechanismState::invalid};
    };

    struct GroundBlockerRecord final {
        contracts::StableContentKey key{};
        contracts::SandboxGroundBlockerState state{
            contracts::SandboxGroundBlockerState::invalid
        };
    };

    [[nodiscard]] bool valid_owned_state() const noexcept;
    [[nodiscard]] std::uint64_t compute_checksum() const noexcept;
    void update_checksum() noexcept;

    bool initialized_{};
    contracts::GroundPoseMm player_spawn_pose_{};
    std::uint32_t player_spawn_facing_millidegrees_{};
    contracts::GroundPoseMm player_retry_pose_{};
    std::uint32_t player_retry_facing_millidegrees_{};
    std::array<InteractionRecord, interaction_capacity> interactions_{};
    std::array<MechanismRecord, mechanism_capacity> mechanisms_{};
    std::array<GroundBlockerRecord, ground_blocker_capacity> ground_blockers_{};
    std::size_t interaction_count_{};
    std::size_t mechanism_count_{};
    std::size_t ground_blocker_count_{};
    SandboxSessionSnapshot snapshot_{};
};

}  // namespace tgd::gameplay
