#include <tgd/contracts/sandbox_gameplay_binding.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::contracts {
namespace {

[[nodiscard]] bool valid_binding_id(ContentId id) noexcept {
    return id.key != 0 && !id.name.empty() && stable_content_key(id.name) == id.key;
}

template <typename Definition>
[[nodiscard]] bool contains_id(
    std::span<const Definition> definitions,
    ContentId id
) noexcept {
    for (const auto& definition : definitions) {
        if (definition.id == id) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] SandboxGameplayBindingValidationResult failure(
    SandboxGameplayBindingValidationCode code,
    StableContentKey subject,
    StableContentKey related,
    std::size_t record_index,
    SandboxGameplayBindingValidationDomain domain,
    SandboxGameplayBindingValidationField field
) noexcept {
    return {
        code,
        subject,
        related,
        static_cast<std::uint32_t>(record_index),
        domain,
        field,
    };
}

}  // namespace

SandboxOperateRangeCheck sandbox_check_operate_range(
    GroundPoseMm actor_pose,
    GroundPoseMm interaction_pose,
    std::int32_t range_mm
) noexcept {
    if (range_mm < sandbox_operate_range_min_mm ||
        range_mm > sandbox_operate_range_max_mm) {
        return SandboxOperateRangeCheck::invalid_range;
    }
    if (actor_pose.floor_layer != interaction_pose.floor_layer) {
        return SandboxOperateRangeCheck::floor_mismatch;
    }

    const auto delta_x = static_cast<std::int64_t>(actor_pose.x) -
                         static_cast<std::int64_t>(interaction_pose.x);
    const auto delta_y = static_cast<std::int64_t>(actor_pose.y) -
                         static_cast<std::int64_t>(interaction_pose.y);
    const auto range = static_cast<std::int64_t>(range_mm);

    // Reject before squaring. Extreme int32 coordinates can produce a delta up
    // to 2^32-1, whose square cannot be represented by signed int64.
    if (delta_x < -range || delta_x > range || delta_y < -range || delta_y > range) {
        return SandboxOperateRangeCheck::out_of_range;
    }

    const auto distance_squared = delta_x * delta_x + delta_y * delta_y;
    const auto range_squared = range * range;
    return distance_squared <= range_squared ? SandboxOperateRangeCheck::eligible
                                             : SandboxOperateRangeCheck::out_of_range;
}

SandboxGameplayBindingValidationResult validate_sandbox_gameplay_binding(
    const SandboxDefinition& core,
    const SandboxGameplayBindingDefinition& binding
) noexcept {
    using Code = SandboxGameplayBindingValidationCode;
    using Domain = SandboxGameplayBindingValidationDomain;
    using Field = SandboxGameplayBindingValidationField;

    if (binding.interaction_bindings.size() > sandbox_interaction_capacity) {
        return failure(
            Code::too_many_interaction_bindings,
            0,
            0,
            binding.interaction_bindings.size(),
            Domain::interaction_binding,
            Field::interaction_id
        );
    }
    if (binding.mechanism_bindings.size() > sandbox_mechanism_capacity) {
        return failure(
            Code::too_many_mechanism_bindings,
            0,
            0,
            binding.mechanism_bindings.size(),
            Domain::mechanism_binding,
            Field::mechanism_id
        );
    }

    for (std::size_t index = 0; index < binding.interaction_bindings.size(); ++index) {
        const auto& current = binding.interaction_bindings[index];
        if (!valid_binding_id(current.interaction_id)) {
            return failure(
                Code::invalid_interaction_id,
                current.interaction_id.key,
                0,
                index,
                Domain::interaction_binding,
                Field::interaction_id
            );
        }
        if (!contains_id(core.interactions, current.interaction_id)) {
            return failure(
                Code::dangling_interaction_id,
                current.interaction_id.key,
                0,
                index,
                Domain::interaction_binding,
                Field::interaction_id
            );
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (binding.interaction_bindings[prior].interaction_id == current.interaction_id) {
                return failure(
                    Code::duplicate_interaction_binding,
                    current.interaction_id.key,
                    current.interaction_id.key,
                    index,
                    Domain::interaction_binding,
                    Field::interaction_id
                );
            }
        }
        if (current.operation != SandboxInteractionOperation::operate) {
            return failure(
                Code::invalid_interaction_operation,
                current.interaction_id.key,
                0,
                index,
                Domain::interaction_binding,
                Field::operation
            );
        }
        if (current.range_mm < sandbox_operate_range_min_mm ||
            current.range_mm > sandbox_operate_range_max_mm) {
            return failure(
                Code::invalid_operate_range,
                current.interaction_id.key,
                0,
                index,
                Domain::interaction_binding,
                Field::range_mm
            );
        }
        if (!valid_binding_id(current.target_mechanism_id)) {
            return failure(
                Code::invalid_target_mechanism_id,
                current.interaction_id.key,
                current.target_mechanism_id.key,
                index,
                Domain::interaction_binding,
                Field::target_mechanism_id
            );
        }
        if (!contains_id(core.mechanisms, current.target_mechanism_id)) {
            return failure(
                Code::dangling_target_mechanism_id,
                current.interaction_id.key,
                current.target_mechanism_id.key,
                index,
                Domain::interaction_binding,
                Field::target_mechanism_id
            );
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (binding.interaction_bindings[prior].target_mechanism_id ==
                current.target_mechanism_id) {
                return failure(
                    Code::duplicate_target_mechanism_writer,
                    current.interaction_id.key,
                    current.target_mechanism_id.key,
                    index,
                    Domain::interaction_binding,
                    Field::target_mechanism_id
                );
            }
        }
    }

    for (std::size_t index = 0; index < binding.mechanism_bindings.size(); ++index) {
        const auto& current = binding.mechanism_bindings[index];
        if (!valid_binding_id(current.mechanism_id)) {
            return failure(
                Code::invalid_mechanism_id,
                current.mechanism_id.key,
                0,
                index,
                Domain::mechanism_binding,
                Field::mechanism_id
            );
        }
        if (!contains_id(core.mechanisms, current.mechanism_id)) {
            return failure(
                Code::dangling_mechanism_id,
                current.mechanism_id.key,
                0,
                index,
                Domain::mechanism_binding,
                Field::mechanism_id
            );
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (binding.mechanism_bindings[prior].mechanism_id == current.mechanism_id) {
                return failure(
                    Code::duplicate_mechanism_binding,
                    current.mechanism_id.key,
                    current.mechanism_id.key,
                    index,
                    Domain::mechanism_binding,
                    Field::mechanism_id
                );
            }
        }
        if (current.activation != SandboxMechanismActivation::one_shot_activate) {
            return failure(
                Code::invalid_mechanism_activation,
                current.mechanism_id.key,
                0,
                index,
                Domain::mechanism_binding,
                Field::activation
            );
        }
        if (!valid_binding_id(current.target_ground_blocker_id)) {
            return failure(
                Code::invalid_target_ground_blocker_id,
                current.mechanism_id.key,
                current.target_ground_blocker_id.key,
                index,
                Domain::mechanism_binding,
                Field::target_ground_blocker_id
            );
        }
        if (!contains_id(core.ground_blockers, current.target_ground_blocker_id)) {
            return failure(
                Code::dangling_target_ground_blocker_id,
                current.mechanism_id.key,
                current.target_ground_blocker_id.key,
                index,
                Domain::mechanism_binding,
                Field::target_ground_blocker_id
            );
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (binding.mechanism_bindings[prior].target_ground_blocker_id ==
                current.target_ground_blocker_id) {
                return failure(
                    Code::duplicate_ground_blocker_writer,
                    current.mechanism_id.key,
                    current.target_ground_blocker_id.key,
                    index,
                    Domain::mechanism_binding,
                    Field::target_ground_blocker_id
                );
            }
        }
    }

    for (std::size_t index = 0; index < core.interactions.size(); ++index) {
        bool found = false;
        for (const auto& candidate : binding.interaction_bindings) {
            if (candidate.interaction_id == core.interactions[index].id) {
                found = true;
                break;
            }
        }
        if (!found) {
            return failure(
                Code::missing_interaction_binding,
                core.interactions[index].id.key,
                0,
                index,
                Domain::core_interaction,
                Field::interaction_id
            );
        }
    }

    for (std::size_t index = 0; index < core.mechanisms.size(); ++index) {
        bool found = false;
        for (const auto& candidate : binding.mechanism_bindings) {
            if (candidate.mechanism_id == core.mechanisms[index].id) {
                found = true;
                break;
            }
        }
        if (!found) {
            return failure(
                Code::missing_mechanism_binding,
                core.mechanisms[index].id.key,
                0,
                index,
                Domain::core_mechanism,
                Field::mechanism_id
            );
        }
    }

    for (std::size_t index = 0; index < core.mechanisms.size(); ++index) {
        bool found = false;
        for (const auto& candidate : binding.interaction_bindings) {
            if (candidate.target_mechanism_id == core.mechanisms[index].id) {
                found = true;
                break;
            }
        }
        if (!found) {
            return failure(
                Code::unreferenced_mechanism,
                core.mechanisms[index].id.key,
                0,
                index,
                Domain::core_mechanism,
                Field::mechanism_id
            );
        }
    }

    return {
        Code::valid,
        0,
        0,
        0,
        Domain::invalid,
        Field::invalid,
    };
}

}  // namespace tgd::contracts
