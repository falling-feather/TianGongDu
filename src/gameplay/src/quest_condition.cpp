#include <tgd/gameplay/quest_condition.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace tgd::gameplay {
namespace {

[[nodiscard]] bool valid_id(const contracts::ContentId& id) noexcept {
    return id.key != 0 && !id.name.empty() && contracts::stable_content_key(id.name) == id.key;
}

[[nodiscard]] bool empty_id(const contracts::ContentId& id) noexcept {
    return id.key == 0 && id.name.empty();
}

[[nodiscard]] bool objective_exists(const contracts::VerticalSliceDefinition& definition,
                                    contracts::StableContentKey objective) noexcept {
    return std::any_of(definition.beats.begin(), definition.beats.end(),
                       [objective](const contracts::VerticalSliceBeatDefinition& beat) {
                           return std::any_of(beat.objectives.begin(), beat.objectives.end(),
                                              [objective](const contracts::ContentId& candidate) {
                                                  return candidate.key == objective;
                                              });
                       });
}

[[nodiscard]] bool selection_exists(const contracts::VerticalSliceDefinition& definition,
                                    contracts::StableContentKey objective,
                                    contracts::StableContentKey selection) noexcept {
    return std::any_of(
        definition.quest_interactions.begin(), definition.quest_interactions.end(),
        [objective, selection](const contracts::QuestInteractionDefinition& interaction) {
            return interaction.kind == contracts::QuestInteractionKind::choose &&
                   interaction.objective_id.key == objective &&
                   interaction.selection_id.key == selection;
        });
}

[[nodiscard]] bool valid_program(
    const contracts::QuestConditionProgramDefinition& definition,
    const contracts::VerticalSliceDefinition& quest_definition) noexcept {
    if (!valid_id(definition.id) || definition.instructions.empty() ||
        definition.instructions.size() >
            DeterministicQuestConditionEvaluator::instruction_capacity) {
        return false;
    }

    std::size_t stack_depth = 0;
    for (const auto& instruction : definition.instructions) {
        switch (instruction.opcode) {
            case contracts::QuestConditionOpcode::objective_completed:
                if (!valid_id(instruction.objective_id) || !empty_id(instruction.selection_id) ||
                    instruction.operand_count != 0 ||
                    !objective_exists(quest_definition, instruction.objective_id.key)) {
                    return false;
                }
                ++stack_depth;
                break;
            case contracts::QuestConditionOpcode::selection_equals:
                if (!valid_id(instruction.objective_id) || !valid_id(instruction.selection_id) ||
                    instruction.operand_count != 0 ||
                    !objective_exists(quest_definition, instruction.objective_id.key) ||
                    !selection_exists(quest_definition, instruction.objective_id.key,
                                      instruction.selection_id.key)) {
                    return false;
                }
                ++stack_depth;
                break;
            case contracts::QuestConditionOpcode::all:
            case contracts::QuestConditionOpcode::any:
                if (!empty_id(instruction.objective_id) || !empty_id(instruction.selection_id) ||
                    instruction.operand_count < 2 ||
                    instruction.operand_count >
                        DeterministicQuestConditionEvaluator::operand_capacity ||
                    stack_depth < instruction.operand_count) {
                    return false;
                }
                stack_depth -= static_cast<std::size_t>(instruction.operand_count) - 1;
                break;
            case contracts::QuestConditionOpcode::negate:
                if (!empty_id(instruction.objective_id) || !empty_id(instruction.selection_id) ||
                    instruction.operand_count != 1 || stack_depth < 1) {
                    return false;
                }
                break;
            default:
                return false;
        }
        if (stack_depth > DeterministicQuestConditionEvaluator::stack_capacity) {
            return false;
        }
    }
    return stack_depth == 1;
}

}  // namespace

QuestConditionError DeterministicQuestConditionEvaluator::initialize(
    std::span<const contracts::QuestConditionProgramDefinition> definitions,
    const contracts::VerticalSliceDefinition& quest_definition) noexcept {
    if (initialized_) {
        return QuestConditionError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > condition_capacity) {
        return QuestConditionError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        if (!valid_program(definitions[index], quest_definition)) {
            return QuestConditionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[prior].id.key == definitions[index].id.key) {
                return QuestConditionError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestConditionError::none;
}

QuestConditionResult DeterministicQuestConditionEvaluator::evaluate(
    contracts::StableContentKey condition, const IQuestRuntime& quest) const noexcept {
    QuestConditionResult result{};
    if (!initialized_) {
        result.error = QuestConditionError::invalid_lifecycle;
        return result;
    }
    const auto definition =
        std::find_if(definitions_.begin(), definitions_.end(),
                     [condition](const contracts::QuestConditionProgramDefinition& candidate) {
                         return candidate.id.key == condition;
                     });
    if (condition == 0 || definition == definitions_.end()) {
        result.error = QuestConditionError::unknown_condition;
        return result;
    }

    std::array<bool, stack_capacity> stack{};
    std::size_t stack_depth = 0;
    for (const auto& instruction : definition->instructions) {
        switch (instruction.opcode) {
            case contracts::QuestConditionOpcode::objective_completed:
                if (stack_depth >= stack.size()) {
                    result.error = QuestConditionError::invalid_definition;
                    return result;
                }
                stack[stack_depth++] = quest.objective_state(instruction.objective_id.key) ==
                                       QuestObjectiveState::completed;
                break;
            case contracts::QuestConditionOpcode::selection_equals:
                if (stack_depth >= stack.size()) {
                    result.error = QuestConditionError::invalid_definition;
                    return result;
                }
                stack[stack_depth++] = quest.selected_option(instruction.objective_id.key) ==
                                       instruction.selection_id.key;
                break;
            case contracts::QuestConditionOpcode::all:
            case contracts::QuestConditionOpcode::any: {
                if (instruction.operand_count < 2 ||
                    instruction.operand_count > operand_capacity ||
                    stack_depth < instruction.operand_count) {
                    result.error = QuestConditionError::invalid_definition;
                    return result;
                }
                const auto first =
                    stack_depth - static_cast<std::size_t>(instruction.operand_count);
                bool value = instruction.opcode == contracts::QuestConditionOpcode::all;
                for (std::size_t index = first; index < stack_depth; ++index) {
                    if (instruction.opcode == contracts::QuestConditionOpcode::all) {
                        value = value && stack[index];
                    } else {
                        value = value || stack[index];
                    }
                }
                stack_depth = first;
                stack[stack_depth++] = value;
                break;
            }
            case contracts::QuestConditionOpcode::negate:
                if (instruction.operand_count != 1 || stack_depth == 0) {
                    result.error = QuestConditionError::invalid_definition;
                    return result;
                }
                stack[stack_depth - 1] = !stack[stack_depth - 1];
                break;
            default:
                result.error = QuestConditionError::invalid_definition;
                return result;
        }
    }
    if (stack_depth != 1) {
        result.error = QuestConditionError::invalid_definition;
        return result;
    }
    result.value = stack.front();
    return result;
}

}  // namespace tgd::gameplay
