#include <tgd/gameplay/combat_action_intent.hpp>

namespace tgd::gameplay {
namespace {

inline constexpr auto gameplay_context = contracts::input_context_id("gameplay");
inline constexpr auto weapon_skill_action = contracts::action_id("weapon_skill");

}  // namespace

CombatActionIntentResult DeterministicCombatActionIntentMapper::resolve(
    const CombatActionIntent& intent
) noexcept {
    CombatActionIntentResult result{};
    if (intent.sample.platform_sequence <= last_platform_sequence_) {
        result.error = CombatActionIntentError::out_of_order_platform_sequence;
        return result;
    }
    const auto* descriptor = contracts::find_action_descriptor(intent.sample.action);
    if (descriptor == nullptr ||
        !contracts::action_supports_context(*descriptor, gameplay_context) ||
        intent.sample.action != weapon_skill_action) {
        result.error = CombatActionIntentError::unsupported_action;
        return result;
    }
    if (descriptor->value_type != contracts::ActionValueType::digital ||
        intent.sample.edge != contracts::ActionSampleEdge::pressed ||
        intent.sample.value_q15 != contracts::ground_axis_one || intent.sample.repeated) {
        result.error = CombatActionIntentError::invalid_sample;
        return result;
    }
    if (intent.tick == 0 || intent.actor == 0 || intent.sequence == 0) {
        result.error = CombatActionIntentError::invalid_identity;
        return result;
    }

    result.command = {
        intent.tick,
        intent.actor,
        intent.sequence,
        contracts::CombatCommandType::weapon_skill,
        intent.requested_target,
        0,
        contracts::CombatSkillSlot::primary,
    };
    result.has_command = true;
    last_platform_sequence_ = intent.sample.platform_sequence;
    return result;
}

contracts::PlatformSequence DeterministicCombatActionIntentMapper::last_platform_sequence(
) const noexcept {
    return last_platform_sequence_;
}

}  // namespace tgd::gameplay
