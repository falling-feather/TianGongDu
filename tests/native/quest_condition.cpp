#include <tgd/content/f1_vertical_slice.generated.hpp>
#include <tgd/gameplay/quest_condition.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>

namespace {

using tgd::contracts::QuestConditionInstructionDefinition;
using tgd::contracts::QuestConditionOpcode;
using tgd::contracts::QuestConditionProgramDefinition;
using tgd::gameplay::DeterministicQuestConditionEvaluator;
using tgd::gameplay::QuestConditionError;
using tgd::gameplay::QuestObjectiveState;

inline constexpr auto first_joined_objective =
    tgd::contracts::content_id("f1_objective_defeat_leaking_dolls");
inline constexpr auto second_joined_objective =
    tgd::contracts::content_id("f1_objective_answer_paper_egret");
inline constexpr auto choice_objective =
    tgd::contracts::content_id("f1_objective_choose_lane_route");
inline constexpr auto canopy_choice = tgd::contracts::content_id("f1_choice_lane_canopy");
inline constexpr auto drain_choice = tgd::contracts::content_id("f1_choice_lane_drain");

[[nodiscard]] constexpr QuestConditionInstructionDefinition completed(
    tgd::contracts::ContentId objective) noexcept {
    return {QuestConditionOpcode::objective_completed, objective, {}, 0};
}

[[nodiscard]] constexpr QuestConditionInstructionDefinition selected(
    tgd::contracts::ContentId objective, tgd::contracts::ContentId option) noexcept {
    return {QuestConditionOpcode::selection_equals, objective, option, 0};
}

[[nodiscard]] constexpr QuestConditionInstructionDefinition combine(
    QuestConditionOpcode opcode, std::uint8_t operands) noexcept {
    return {opcode, {}, {}, operands};
}

inline constexpr std::array<QuestConditionInstructionDefinition, 4> all_instructions{{
    completed(first_joined_objective),
    completed(second_joined_objective),
    selected(choice_objective, canopy_choice),
    combine(QuestConditionOpcode::all, 3),
}};

inline constexpr std::array<QuestConditionInstructionDefinition, 3> any_instructions{{
    selected(choice_objective, canopy_choice),
    selected(choice_objective, drain_choice),
    combine(QuestConditionOpcode::any, 2),
}};

inline constexpr std::array<QuestConditionInstructionDefinition, 2> negate_instructions{{
    completed(first_joined_objective),
    combine(QuestConditionOpcode::negate, 1),
}};

inline constexpr std::array<QuestConditionInstructionDefinition, 5> nested_instructions{{
    completed(first_joined_objective),
    selected(choice_objective, canopy_choice),
    combine(QuestConditionOpcode::all, 2),
    selected(choice_objective, drain_choice),
    combine(QuestConditionOpcode::any, 2),
}};

inline constexpr std::array<QuestConditionProgramDefinition, 4> programs{{
    {
        tgd::contracts::content_id("test_condition_joined_canopy"),
        std::span<const QuestConditionInstructionDefinition>{all_instructions},
    },
    {
        tgd::contracts::content_id("test_condition_either_route"),
        std::span<const QuestConditionInstructionDefinition>{any_instructions},
    },
    {
        tgd::contracts::content_id("test_condition_not_joined"),
        std::span<const QuestConditionInstructionDefinition>{negate_instructions},
    },
    {
        tgd::contracts::content_id("test_condition_joined_canopy_or_drain"),
        std::span<const QuestConditionInstructionDefinition>{nested_instructions},
    },
}};

class StubQuestRuntime final : public tgd::gameplay::IQuestRuntime {
  public:
    [[nodiscard]] tgd::gameplay::QuestError initialize(
        const tgd::contracts::VerticalSliceDefinition&,
        tgd::contracts::StableActorKey) noexcept override {
        return tgd::gameplay::QuestError::none;
    }

    [[nodiscard]] tgd::gameplay::QuestError start() noexcept override {
        return tgd::gameplay::QuestError::none;
    }

    [[nodiscard]] tgd::gameplay::QuestError pause() noexcept override {
        return tgd::gameplay::QuestError::none;
    }

    [[nodiscard]] tgd::gameplay::QuestError resume() noexcept override {
        return tgd::gameplay::QuestError::none;
    }

    [[nodiscard]] tgd::gameplay::QuestError destroy() noexcept override {
        return tgd::gameplay::QuestError::none;
    }

    [[nodiscard]] tgd::gameplay::QuestApplyResult apply(
        const tgd::contracts::QuestCommand&, tgd::gameplay::IQuestEventSink&) noexcept override {
        return {};
    }

    [[nodiscard]] QuestObjectiveState objective_state(
        tgd::contracts::StableContentKey objective) const noexcept override {
        if (objective == first_joined_objective.key) {
            return first_joined_state_;
        }
        if (objective == second_joined_objective.key) {
            return second_joined_state_;
        }
        return QuestObjectiveState::unknown;
    }

    [[nodiscard]] tgd::contracts::StableContentKey selected_option(
        tgd::contracts::StableContentKey objective) const noexcept override {
        return objective == choice_objective.key ? selected_option_ : 0;
    }

    [[nodiscard]] const tgd::contracts::QuestSnapshot& snapshot() const noexcept override {
        return snapshot_;
    }

    void set_first_joined_state(QuestObjectiveState state) noexcept { first_joined_state_ = state; }

    void set_second_joined_state(QuestObjectiveState state) noexcept {
        second_joined_state_ = state;
    }

    void set_selected_option(tgd::contracts::StableContentKey option) noexcept {
        selected_option_ = option;
    }

  private:
    QuestObjectiveState first_joined_state_{QuestObjectiveState::locked};
    QuestObjectiveState second_joined_state_{QuestObjectiveState::locked};
    tgd::contracts::StableContentKey selected_option_{};
    tgd::contracts::QuestSnapshot snapshot_{};
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "quest condition failure: " << message << '\n';
    }
    return condition;
}

bool test_postfix_boolean_evaluation() {
    DeterministicQuestConditionEvaluator evaluator;
    StubQuestRuntime quest;
    bool ok = expect(evaluator.evaluate(programs[0].id.key, quest).error ==
                         QuestConditionError::invalid_lifecycle,
                     "conditions cannot be evaluated before validated initialization");
    ok &= expect(
        evaluator.initialize(programs, tgd::content::generated::f1_vertical_slice_definition) ==
            QuestConditionError::none,
        "a bounded postfix condition bank initializes against authored quest "
        "identifiers");
    ok &= expect(
        evaluator.initialize(programs, tgd::content::generated::f1_vertical_slice_definition) ==
            QuestConditionError::invalid_lifecycle,
        "condition ownership initializes exactly once");

    ok &= expect(!evaluator.evaluate(programs[0].id.key, quest).value, "AND starts false");
    ok &= expect(!evaluator.evaluate(programs[1].id.key, quest).value, "OR starts false");
    ok &= expect(evaluator.evaluate(programs[2].id.key, quest).value, "NOT inverts false");
    ok &= expect(!evaluator.evaluate(programs[3].id.key, quest).value, "nested gate starts false");

    quest.set_first_joined_state(QuestObjectiveState::completed);
    quest.set_selected_option(canopy_choice.key);
    ok &= expect(!evaluator.evaluate(programs[0].id.key, quest).value,
                 "a partial Objective join cannot pass");
    quest.set_second_joined_state(QuestObjectiveState::completed);
    ok &= expect(evaluator.evaluate(programs[0].id.key, quest).value,
                 "multiple Objective and selection facts join through AND");
    ok &= expect(evaluator.evaluate(programs[1].id.key, quest).value, "OR accepts canopy");
    ok &= expect(!evaluator.evaluate(programs[2].id.key, quest).value, "NOT inverts true");
    ok &= expect(evaluator.evaluate(programs[3].id.key, quest).value, "nested AND/OR is true");

    quest.set_first_joined_state(QuestObjectiveState::locked);
    quest.set_second_joined_state(QuestObjectiveState::locked);
    quest.set_selected_option(drain_choice.key);
    const auto nested = evaluator.evaluate(programs[3].id.key, quest);
    ok &= expect(nested.error == QuestConditionError::none && nested.value,
                 "an alternate selection can satisfy the other nested branch");
    ok &= expect(
        evaluator.evaluate(tgd::contracts::stable_content_key("missing_condition"), quest).error ==
            QuestConditionError::unknown_condition,
        "unknown condition identifiers fail closed");
    return ok;
}

bool rejects(std::span<const QuestConditionProgramDefinition> definitions,
             std::string_view message) {
    DeterministicQuestConditionEvaluator evaluator;
    return expect(
        evaluator.initialize(definitions, tgd::content::generated::f1_vertical_slice_definition) ==
            QuestConditionError::invalid_definition,
        message);
}

bool test_invalid_programs_fail_closed() {
    bool ok = rejects({}, "an empty condition bank is invalid");

    const std::array<QuestConditionProgramDefinition, 1> empty_program{{
        {tgd::contracts::content_id("test_condition_empty"), {}},
    }};
    ok &= rejects(empty_program, "an empty postfix program is invalid");

    const std::array<QuestConditionInstructionDefinition, 1> unknown_objective{{
        completed(tgd::contracts::content_id("missing_objective")),
    }};
    const std::array<QuestConditionProgramDefinition, 1> unknown_objective_program{{
        {
            tgd::contracts::content_id("test_condition_unknown_objective"),
            std::span<const QuestConditionInstructionDefinition>{unknown_objective},
        },
    }};
    ok &= rejects(unknown_objective_program, "unknown Objective facts are invalid");

    const std::array<QuestConditionInstructionDefinition, 1> unknown_selection{{
        selected(choice_objective, tgd::contracts::content_id("missing_selection")),
    }};
    const std::array<QuestConditionProgramDefinition, 1> unknown_selection_program{{
        {
            tgd::contracts::content_id("test_condition_unknown_selection"),
            std::span<const QuestConditionInstructionDefinition>{unknown_selection},
        },
    }};
    ok &= rejects(unknown_selection_program, "unauthored selection facts are invalid");

    const std::array<QuestConditionInstructionDefinition, 2> stack_underflow{{
        completed(first_joined_objective),
        combine(QuestConditionOpcode::all, 2),
    }};
    const std::array<QuestConditionProgramDefinition, 1> stack_underflow_program{{
        {
            tgd::contracts::content_id("test_condition_stack_underflow"),
            std::span<const QuestConditionInstructionDefinition>{stack_underflow},
        },
    }};
    ok &= rejects(stack_underflow_program, "operators cannot consume missing operands");

    const std::array<QuestConditionInstructionDefinition, 2> dangling_roots{{
        completed(first_joined_objective),
        selected(choice_objective, canopy_choice),
    }};
    const std::array<QuestConditionProgramDefinition, 1> dangling_roots_program{{
        {
            tgd::contracts::content_id("test_condition_dangling_roots"),
            std::span<const QuestConditionInstructionDefinition>{dangling_roots},
        },
    }};
    ok &= rejects(dangling_roots_program, "a program must reduce to exactly one result");

    std::array<QuestConditionInstructionDefinition, 10> too_many_operands{};
    too_many_operands.fill(completed(first_joined_objective));
    too_many_operands.back() = combine(QuestConditionOpcode::all, 9);
    const std::array<QuestConditionProgramDefinition, 1> too_many_operands_program{{
        {
            tgd::contracts::content_id("test_condition_too_many_operands"),
            std::span<const QuestConditionInstructionDefinition>{too_many_operands},
        },
    }};
    ok &= rejects(too_many_operands_program,
                  "compound operators have an explicit fan-in bound");

    const std::array<QuestConditionProgramDefinition, 2> duplicate_ids{{
        programs[0],
        {
            programs[0].id,
            std::span<const QuestConditionInstructionDefinition>{any_instructions},
        },
    }};
    ok &= rejects(duplicate_ids, "condition identifiers are unique within the bank");

    std::array<QuestConditionInstructionDefinition, 17> overflowing_stack{};
    overflowing_stack.fill(completed(first_joined_objective));
    const std::array<QuestConditionProgramDefinition, 1> overflowing_stack_program{{
        {
            tgd::contracts::content_id("test_condition_stack_overflow"),
            std::span<const QuestConditionInstructionDefinition>{overflowing_stack},
        },
    }};
    ok &= rejects(overflowing_stack_program, "condition evaluation stack is explicitly bounded");
    return ok;
}

}  // namespace

int main() {
    const bool ok = test_postfix_boolean_evaluation() && test_invalid_programs_fail_closed();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
