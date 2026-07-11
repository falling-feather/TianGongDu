#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/quest_types.hpp>
#include <tgd/gameplay/quest_runtime.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using tgd::gameplay::DeterministicQuestRuntime;
using tgd::gameplay::QuestError;
using tgd::gameplay::QuestLifecycle;
using tgd::gameplay::QuestObjectiveState;

class CollectingSink final : public tgd::gameplay::IQuestEventSink {
  public:
    void publish(std::span<const tgd::contracts::QuestEvent> events) noexcept override {
        values.insert(values.end(), events.begin(), events.end());
    }

    [[nodiscard]] bool contains(tgd::contracts::QuestEventType type) const {
        for (const auto& event : values) {
            if (event.type == type) {
                return true;
            }
        }
        return false;
    }

    std::vector<tgd::contracts::QuestEvent> values;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "quest runtime failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] const tgd::contracts::VerticalSliceDefinition& definition() {
    static tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* value = provider.find_vertical_slice(
        tgd::contracts::stable_content_key("f1_rainy_umbrella_trial")
    );
    if (value == nullptr) {
        std::abort();
    }
    return *value;
}

bool test_ordering_idempotency_and_lifecycle() {
    DeterministicQuestRuntime quest;
    CollectingSink sink;
    bool ok = quest.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= quest.start() == QuestError::none;
    const auto initial_checksum = quest.snapshot().checksum;
    const auto future = definition().beats[1].objectives.front().key;
    ok &= expect(
        quest.apply({1, definition().player.actor, 1, {}, future}, sink).error ==
            QuestError::objective_not_active,
        "future objectives cannot bypass the active stage"
    );
    ok &= expect(
        quest.snapshot().checksum == initial_checksum,
        "rejected future commands do not mutate quest state"
    );
    ok &= expect(
        quest.apply({1, definition().player.actor, 1, {}, 999}, sink).error ==
            QuestError::unknown_objective,
        "unknown objectives fail closed"
    );

    const auto& first = definition().beats.front();
    const auto first_result = quest.apply(
        {10, definition().player.actor, 1, {}, first.objectives.back().key},
        sink
    );
    ok &= expect(first_result.error == QuestError::none && first_result.accepted, "active objective completes");
    const auto duplicate = quest.apply(
        {10, definition().player.actor, 2, {}, first.objectives.back().key},
        sink
    );
    ok &= expect(
        duplicate.error == QuestError::none && !duplicate.accepted &&
            sink.contains(tgd::contracts::QuestEventType::objective_already_completed),
        "a new command for a completed objective is idempotent"
    );
    const auto advanced = quest.apply(
        {11, definition().player.actor, 3, {}, first.objectives.front().key},
        sink
    );
    ok &= expect(
        advanced.error == QuestError::none && advanced.accepted && advanced.stage_advanced &&
            !advanced.quest_resolved,
        "completing the parallel objective group advances one stage"
    );
    ok &= expect(
        quest.snapshot().stage == definition().beats[1].id.key &&
            quest.objective_state(first.objectives.front().key) == QuestObjectiveState::completed &&
            quest.objective_state(definition().beats[2].objectives.front().key) ==
                QuestObjectiveState::locked,
        "snapshot exposes completed, active, and locked objective states"
    );
    ok &= expect(
        quest.apply({11, definition().player.actor, 3, {}, future}, sink).error ==
            QuestError::stale_command_sequence,
        "reused command sequences are rejected"
    );
    ok &= expect(
        quest.apply({9, definition().player.actor, 4, {}, future}, sink).error ==
            QuestError::tick_regressed,
        "quest events cannot move backward in simulation time"
    );
    ok &= expect(quest.pause() == QuestError::none, "quest pauses explicitly");
    ok &= expect(quest.resume() == QuestError::none, "quest resumes explicitly");
    return ok;
}

bool test_full_resolution_is_deterministic() {
    DeterministicQuestRuntime left;
    DeterministicQuestRuntime right;
    CollectingSink left_sink;
    CollectingSink right_sink;
    bool ok = left.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= right.initialize(definition(), definition().player.actor) == QuestError::none;
    ok &= left.start() == QuestError::none;
    ok &= right.start() == QuestError::none;
    tgd::contracts::CommandSequence sequence = 1;
    tgd::contracts::TickIndex tick = 1;
    tgd::contracts::StableContentKey last_objective = 0;
    for (const auto& stage : definition().beats) {
        for (auto objective = stage.objectives.rbegin(); objective != stage.objectives.rend(); ++objective) {
            const tgd::contracts::QuestCommand command{
                tick++,
                definition().player.actor,
                sequence++,
                tgd::contracts::QuestCommandType::complete_objective,
                objective->key,
            };
            last_objective = objective->key;
            const auto left_result = left.apply(command, left_sink);
            const auto right_result = right.apply(command, right_sink);
            ok &= left_result.error == QuestError::none && right_result.error == QuestError::none;
            ok &= expect(
                left.snapshot().checksum == right.snapshot().checksum,
                "the same objective commands produce the same checksum"
            );
        }
    }
    ok &= expect(
        left.lifecycle() == QuestLifecycle::resolved && left.snapshot().resolved &&
            left.snapshot().completed_total > 0 &&
            left_sink.contains(tgd::contracts::QuestEventType::quest_resolved),
        "the final stage resolves the quest graph"
    );
    const auto duplicate = left.apply(
        {tick, definition().player.actor, sequence, {}, last_objective},
        left_sink
    );
    ok &= expect(
        duplicate.error == QuestError::none && !duplicate.accepted &&
            duplicate.quest_resolved,
        "resolved graphs keep duplicate objective completion idempotent"
    );
    return ok;
}

bool test_invalid_definition_fails_closed() {
    const std::array duplicate_objectives{
        tgd::contracts::content_id("objective_duplicate"),
        tgd::contracts::content_id("objective_duplicate"),
    };
    const std::array stages{
        tgd::contracts::VerticalSliceBeatDefinition{
            tgd::contracts::content_id("stage_duplicate"),
            tgd::contracts::VerticalSliceBeatKind::exploration,
            1,
            tgd::contracts::content_id("cell_duplicate"),
            duplicate_objectives,
        },
    };
    auto invalid = definition();
    invalid.beats = stages;
    DeterministicQuestRuntime quest;
    bool ok = expect(
        quest.initialize(invalid, invalid.player.actor) == QuestError::duplicate_objective,
        "duplicate stable objective IDs fail definition validation"
    );
    DeterministicQuestRuntime wrong_actor;
    ok &= expect(
        wrong_actor.initialize(definition(), 999) == QuestError::invalid_definition,
        "quest ownership must match the definition player"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_ordering_idempotency_and_lifecycle();
    ok &= test_full_resolution_is_deterministic();
    ok &= test_invalid_definition_fails_closed();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
