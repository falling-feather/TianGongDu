#pragma once

#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_pack.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::contracts {

inline constexpr std::int32_t sandbox_operate_range_min_mm = 500;
inline constexpr std::int32_t sandbox_operate_range_max_mm = 3'000;

enum class SandboxInteractionOperation : std::uint8_t {
    operate = 1,
    invalid = 255,
};

enum class SandboxMechanismActivation : std::uint8_t {
    one_shot_activate = 1,
    invalid = 255,
};

struct SandboxInteractionGameplayBinding final {
    ContentId interaction_id{};
    SandboxInteractionOperation operation{SandboxInteractionOperation::invalid};
    std::int32_t range_mm{};
    ContentId target_mechanism_id{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxInteractionGameplayBinding&,
        const SandboxInteractionGameplayBinding&
    ) noexcept = default;
};

struct SandboxMechanismGameplayBinding final {
    ContentId mechanism_id{};
    SandboxMechanismActivation activation{SandboxMechanismActivation::invalid};
    ContentId target_ground_blocker_id{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxMechanismGameplayBinding&,
        const SandboxMechanismGameplayBinding&
    ) noexcept = default;
};

// Immutable and non-owning. The caller retains every referenced binding record
// for the full lifetime of this view.
struct SandboxGameplayBindingDefinition final {
    std::span<const SandboxInteractionGameplayBinding> interaction_bindings{};
    std::span<const SandboxMechanismGameplayBinding> mechanism_bindings{};
};

enum class SandboxInteractionState : std::uint8_t {
    uncompleted = 1,
    completed = 2,
    invalid = 255,
};

enum class SandboxMechanismState : std::uint8_t {
    inactive = 1,
    activated = 2,
    invalid = 255,
};

enum class SandboxGroundBlockerState : std::uint8_t {
    enabled_solid = 1,
    disabled_non_solid = 2,
    invalid = 255,
};

inline constexpr SandboxInteractionState sandbox_interaction_initial_state =
    SandboxInteractionState::uncompleted;
inline constexpr SandboxMechanismState sandbox_mechanism_initial_state =
    SandboxMechanismState::inactive;
inline constexpr SandboxGroundBlockerState sandbox_ground_blocker_initial_state =
    SandboxGroundBlockerState::enabled_solid;

enum class SandboxInteractionOperateResult : std::uint8_t {
    completed = 1,
    already_completed = 2,
    not_applied = 3,
    invalid = 255,
};

enum class SandboxMechanismActivateResult : std::uint8_t {
    activated = 1,
    already_activated = 2,
    not_applied = 3,
    invalid = 255,
};

enum class SandboxGroundBlockerWriteResult : std::uint8_t {
    disabled = 1,
    ignored_repeat = 2,
    not_applied = 3,
    invalid = 255,
};

enum class SandboxGameplayEventKind : std::uint8_t {
    interaction_completed = 1,
    mechanism_activated = 2,
    invalid = 255,
};

enum class SandboxOperateDisposition : std::uint8_t {
    completed_chain = 1,
    repeated_chain = 2,
    unknown_interaction = 3,
    floor_mismatch = 4,
    out_of_range = 5,
    stale_generation = 6,
    stale_sequence = 7,
    invalid_binding = 8,
    invalid = 255,
};

// Contract shape only. A future Session owns non-zero/stale generation and
// sequence checks and resolves the authoritative actor pose from mutable state.
struct SandboxOperateCommand final {
    std::uint32_t generation{};
    TickIndex completed_tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    StableContentKey interaction{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxOperateCommand&,
        const SandboxOperateCommand&
    ) noexcept = default;
};

// The single disposition is the only stored result truth. Component results and
// event order are total derived mappings below; no mutable state lives here.
struct SandboxOperateResult final {
    SandboxOperateDisposition disposition{SandboxOperateDisposition::invalid};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxOperateResult&,
        const SandboxOperateResult&
    ) noexcept = default;
};

[[nodiscard]] constexpr SandboxInteractionOperateResult sandbox_interaction_operate_result(
    SandboxOperateResult result
) noexcept {
    switch (result.disposition) {
        case SandboxOperateDisposition::completed_chain:
            return SandboxInteractionOperateResult::completed;
        case SandboxOperateDisposition::repeated_chain:
            return SandboxInteractionOperateResult::already_completed;
        case SandboxOperateDisposition::unknown_interaction:
        case SandboxOperateDisposition::floor_mismatch:
        case SandboxOperateDisposition::out_of_range:
        case SandboxOperateDisposition::stale_generation:
        case SandboxOperateDisposition::stale_sequence:
        case SandboxOperateDisposition::invalid_binding:
            return SandboxInteractionOperateResult::not_applied;
        case SandboxOperateDisposition::invalid:
            return SandboxInteractionOperateResult::invalid;
    }
    return SandboxInteractionOperateResult::invalid;
}

[[nodiscard]] constexpr SandboxMechanismActivateResult sandbox_mechanism_activate_result(
    SandboxOperateResult result
) noexcept {
    switch (result.disposition) {
        case SandboxOperateDisposition::completed_chain:
            return SandboxMechanismActivateResult::activated;
        case SandboxOperateDisposition::repeated_chain:
            return SandboxMechanismActivateResult::already_activated;
        case SandboxOperateDisposition::unknown_interaction:
        case SandboxOperateDisposition::floor_mismatch:
        case SandboxOperateDisposition::out_of_range:
        case SandboxOperateDisposition::stale_generation:
        case SandboxOperateDisposition::stale_sequence:
        case SandboxOperateDisposition::invalid_binding:
            return SandboxMechanismActivateResult::not_applied;
        case SandboxOperateDisposition::invalid:
            return SandboxMechanismActivateResult::invalid;
    }
    return SandboxMechanismActivateResult::invalid;
}

[[nodiscard]] constexpr SandboxGroundBlockerWriteResult sandbox_ground_blocker_write_result(
    SandboxOperateResult result
) noexcept {
    switch (result.disposition) {
        case SandboxOperateDisposition::completed_chain:
            return SandboxGroundBlockerWriteResult::disabled;
        case SandboxOperateDisposition::repeated_chain:
            return SandboxGroundBlockerWriteResult::ignored_repeat;
        case SandboxOperateDisposition::unknown_interaction:
        case SandboxOperateDisposition::floor_mismatch:
        case SandboxOperateDisposition::out_of_range:
        case SandboxOperateDisposition::stale_generation:
        case SandboxOperateDisposition::stale_sequence:
        case SandboxOperateDisposition::invalid_binding:
            return SandboxGroundBlockerWriteResult::not_applied;
        case SandboxOperateDisposition::invalid:
            return SandboxGroundBlockerWriteResult::invalid;
    }
    return SandboxGroundBlockerWriteResult::invalid;
}

[[nodiscard]] constexpr std::uint8_t sandbox_operate_event_count(
    SandboxOperateResult result
) noexcept {
    return result.disposition == SandboxOperateDisposition::completed_chain ? 2 : 0;
}

[[nodiscard]] constexpr SandboxGameplayEventKind sandbox_operate_event_at(
    SandboxOperateResult result,
    std::size_t index
) noexcept {
    if (result.disposition != SandboxOperateDisposition::completed_chain) {
        return SandboxGameplayEventKind::invalid;
    }
    if (index == 0) {
        return SandboxGameplayEventKind::interaction_completed;
    }
    if (index == 1) {
        return SandboxGameplayEventKind::mechanism_activated;
    }
    return SandboxGameplayEventKind::invalid;
}

enum class SandboxOperateRangeCheck : std::uint8_t {
    eligible = 1,
    invalid_range = 2,
    floor_mismatch = 3,
    out_of_range = 4,
    invalid = 255,
};

enum class SandboxGameplayBindingValidationCode : std::uint8_t {
    valid = 1,
    too_many_interaction_bindings = 2,
    too_many_mechanism_bindings = 3,
    invalid_interaction_id = 4,
    dangling_interaction_id = 5,
    duplicate_interaction_binding = 6,
    invalid_interaction_operation = 7,
    invalid_operate_range = 8,
    invalid_target_mechanism_id = 9,
    dangling_target_mechanism_id = 10,
    missing_interaction_binding = 11,
    invalid_mechanism_id = 12,
    dangling_mechanism_id = 13,
    duplicate_mechanism_binding = 14,
    invalid_mechanism_activation = 15,
    invalid_target_ground_blocker_id = 16,
    dangling_target_ground_blocker_id = 17,
    duplicate_ground_blocker_writer = 18,
    missing_mechanism_binding = 19,
    duplicate_target_mechanism_writer = 20,
    unreferenced_mechanism = 21,
    invalid = 255,
};

enum class SandboxGameplayBindingValidationDomain : std::uint8_t {
    interaction_binding = 1,
    mechanism_binding = 2,
    core_interaction = 3,
    core_mechanism = 4,
    invalid = 255,
};

enum class SandboxGameplayBindingValidationField : std::uint16_t {
    interaction_id = 1,
    operation = 2,
    range_mm = 3,
    target_mechanism_id = 4,
    mechanism_id = 5,
    activation = 6,
    target_ground_blocker_id = 7,
    invalid = 65'535,
};

struct SandboxGameplayBindingValidationResult final {
    SandboxGameplayBindingValidationCode code{
        SandboxGameplayBindingValidationCode::invalid
    };
    StableContentKey subject{};
    StableContentKey related{};
    std::uint32_t record_index{};
    SandboxGameplayBindingValidationDomain domain{
        SandboxGameplayBindingValidationDomain::invalid
    };
    SandboxGameplayBindingValidationField field{
        SandboxGameplayBindingValidationField::invalid
    };

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxGameplayBindingValidationResult&,
        const SandboxGameplayBindingValidationResult&
    ) noexcept = default;
};

[[nodiscard]] constexpr bool sandbox_gameplay_binding_is_valid(
    SandboxGameplayBindingValidationResult result
) noexcept {
    return result.code == SandboxGameplayBindingValidationCode::valid;
}

[[nodiscard]] SandboxOperateRangeCheck sandbox_check_operate_range(
    GroundPoseMm actor_pose,
    GroundPoseMm interaction_pose,
    std::int32_t range_mm
) noexcept;

// core must already have passed package-core validation. This function validates
// only the separate gameplay binding and never mutates either input view.
[[nodiscard]] SandboxGameplayBindingValidationResult validate_sandbox_gameplay_binding(
    const SandboxDefinition& core,
    const SandboxGameplayBindingDefinition& binding
) noexcept;

}  // namespace tgd::contracts
