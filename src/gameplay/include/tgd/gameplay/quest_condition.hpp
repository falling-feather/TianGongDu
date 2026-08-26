#pragma once

#include <tgd/contracts/content_definition.hpp>
#include <tgd/gameplay/quest_runtime.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class QuestConditionError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_definition,
    unknown_condition,
};

struct QuestConditionResult final {
    QuestConditionError error{QuestConditionError::none};
    bool value{};
};

class DeterministicQuestConditionEvaluator final {
  public:
    static constexpr std::size_t condition_capacity = 64;
    static constexpr std::size_t instruction_capacity = 64;
    static constexpr std::size_t stack_capacity = 16;
    static constexpr std::uint8_t operand_capacity = 8;

    [[nodiscard]] QuestConditionError initialize(
        std::span<const contracts::QuestConditionProgramDefinition> definitions,
        const contracts::VerticalSliceDefinition& quest_definition) noexcept;
    [[nodiscard]] QuestConditionResult evaluate(contracts::StableContentKey condition,
                                                const IQuestRuntime& quest) const noexcept;

  private:
    std::span<const contracts::QuestConditionProgramDefinition> definitions_{};
    bool initialized_{};
};

}  // namespace tgd::gameplay
