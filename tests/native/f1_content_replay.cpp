#include "F1QuestUiProjection.hpp"

#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/gameplay/quest_ui_projection.hpp>
#include <tgd/gameplay/vertical_slice_session.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/test/f1_content_replay.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <string_view>

namespace {

using tgd::gameplay::VerticalSliceError;
using tgd::gameplay::VerticalSliceSession;

constexpr std::uint64_t replay_ui_checksum_offset = 14695981039346656037ULL;
constexpr std::uint64_t replay_ui_checksum_prime = 1099511628211ULL;

void hash_projection(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        hash ^= static_cast<std::uint8_t>(value & 0xffU);
        hash *= replay_ui_checksum_prime;
        value >>= 8U;
    }
}

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "F1 content replay failure: " << message << '\n';
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

[[nodiscard]] std::unique_ptr<tgd::runtime::StaticCollisionWorld> empty_world() {
    auto world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    if (world->configure({}) != tgd::runtime::CollisionWorldError::none) {
        std::abort();
    }
    return world;
}

struct ReplayRun final {
    bool ok{};
    tgd::gameplay::VerticalSliceSnapshot snapshot{};
    std::uint64_t quest_checksum{};
    std::uint64_t quest_ui_checksum{};
    tgd::contracts::StableContentKey arrival_clue{};
    tgd::contracts::StableContentKey mooring_method{};
    tgd::contracts::StableContentKey training_lane{};
};

[[nodiscard]] ReplayRun run_replay(const tgd::test::F1ContentReplayFixture& replay) {
    VerticalSliceSession session;
    bool ok = expect(
        session.initialize(definition(), empty_world()) == VerticalSliceError::none,
        "fixture initializes"
    );
    ok &= expect(session.start() == VerticalSliceError::none, "fixture starts");
    tgd::gameplay::DeterministicQuestUiProjectionProducer quest_ui;
    ok &= expect(
        quest_ui.initialize(definition()) ==
            tgd::gameplay::QuestUiProjectionError::none,
        "the F1 1.6 generated cue set initializes the generic projection producer"
    );

    std::uint64_t completed_steps = 0;
    std::uint64_t quest_ui_checksum = replay_ui_checksum_offset;
    std::uint8_t quest_ui_projection_count = 0;
    std::uint32_t retry_sequence = 1;
    for (const auto& step : replay.steps) {
        switch (step.type) {
            case tgd::test::F1ContentReplayStepType::complete_objective: {
                tgd::contracts::StableContentKey expected_cue{};
                auto expected_classification =
                    tgd::contracts::QuestUiAttemptTimeClassification::unspecified;
                std::uint8_t expected_option_count = 0;
                if (step.objective == tgd::contracts::stable_content_key(
                                          "f1_objective_choose_arrival_clue"
                                      )) {
                    expected_cue = tgd::contracts::stable_content_key(
                        "ui.f1.rain.choice.arrival-clue"
                    );
                    expected_classification = tgd::contracts::QuestUiAttemptTimeClassification::
                        qualifying_first_visit;
                    expected_option_count = 3;
                } else if (step.objective == tgd::contracts::stable_content_key(
                                                 "f1_objective_choose_mooring_method"
                                             )) {
                    expected_cue = tgd::contracts::stable_content_key(
                        "ui.f1.rain.choice.mooring-method"
                    );
                    expected_classification = tgd::contracts::QuestUiAttemptTimeClassification::
                        qualifying_craft_decision;
                    expected_option_count = 2;
                } else if (step.objective == tgd::contracts::stable_content_key(
                                                 "f1_objective_choose_training_lane"
                                             )) {
                    expected_cue = tgd::contracts::stable_content_key(
                        "ui.f1.training.choice.lane"
                    );
                    expected_classification = tgd::contracts::QuestUiAttemptTimeClassification::
                        qualifying_dialogue_decision;
                    expected_option_count = 2;
                }
                if (expected_cue != 0) {
                    const auto quest_checksum_before = session.quest_snapshot().checksum;
                    const auto safe_point_before =
                        session.current_snapshot().safe_point_id.key;
                    const tgd::contracts::QuestUiProjectionSignal signal{
                        .source = tgd::contracts::QuestUiProjectionSource::choice_available,
                        .objective = step.objective,
                    };
                    const auto projected = quest_ui.project(
                        signal,
                        session.quest_runtime(),
                        safe_point_before,
                        std::span<const tgd::contracts::CombatActorSnapshot>{}
                    );
                    ok &= expect(
                        projected.error == tgd::gameplay::QuestUiProjectionError::none &&
                            projected.projection.cue == expected_cue &&
                            projected.projection.objective == step.objective &&
                            projected.projection.surface ==
                                tgd::contracts::QuestUiSurface::choice &&
                            projected.projection.choice_option_count ==
                                expected_option_count &&
                            projected.projection.attempt_time_classification ==
                                expected_classification &&
                            projected.projection.safe_point == safe_point_before &&
                            projected.projection.quest_checksum == quest_checksum_before &&
                            session.quest_snapshot().checksum == quest_checksum_before,
                        "choice projection derives F1 1.6 attempt evidence without mutating quest truth"
                    );
                    const auto option_end =
                        projected.projection.choice_options.begin() +
                        projected.projection.choice_option_count;
                    const auto selected_option = std::find_if(
                        projected.projection.choice_options.begin(),
                        option_end,
                        [&step](const tgd::contracts::QuestUiChoiceOption& option) {
                            return option.selection == step.selection;
                        }
                    );
                    const auto selected_index = static_cast<std::size_t>(
                        std::distance(
                            projected.projection.choice_options.begin(),
                            selected_option
                        )
                    );
                    F1QuestUiChoiceState native_choice;
                    const auto native_started = native_choice.begin(
                        projected.projection,
                        false
                    );
                    const auto native_intent = native_choice.native_intent(selected_index);
                    ok &= expect(
                        selected_option != option_end &&
                            native_started == F1QuestUiChoiceError::none &&
                            native_choice.native_pending() &&
                            native_choice.option_count() == expected_option_count &&
                            native_intent.error == F1QuestUiChoiceError::none &&
                            native_intent.intent.interaction == selected_option->interaction &&
                            native_intent.intent.selection == step.selection &&
                            quest_ui.validate_choice_intent(
                                native_intent.intent,
                                session.quest_runtime()
                            ) == tgd::gameplay::QuestUiSelectionIntentError::none,
                        "each real choice activates the ordered Native fallback and exact intent"
                    );
                    F1QuestUiChoiceState external_choice;
                    ok &= expect(
                        external_choice.begin(projected.projection, true) ==
                                F1QuestUiChoiceError::none &&
                            external_choice.mode() == F1QuestUiChoiceMode::external &&
                            external_choice.matches(native_intent.intent),
                        "each real choice exposes the same identity to the external Action path"
                    );
                    native_choice.finish();
                    external_choice.finish();
                    if (projected.error == tgd::gameplay::QuestUiProjectionError::none) {
                        hash_projection(quest_ui_checksum, projected.projection.sequence);
                        hash_projection(quest_ui_checksum, projected.projection.checksum);
                        ++quest_ui_projection_count;
                    }
                }
                const auto result = session.complete_objective(step.objective, step.selection);
                ok &= expect(
                    result.error == VerticalSliceError::none && result.accepted,
                    "authored completion is accepted"
                );
                completed_steps += result.accepted ? 1U : 0U;
                break;
            }
            case tgd::test::F1ContentReplayStepType::repeat_objective: {
                const auto completed_before = session.current_snapshot().completed_objectives;
                const auto selected_before = session.current_snapshot().selected_choices;
                const auto result = session.complete_objective(step.objective, step.selection);
                ok &= expect(
                    result.error == VerticalSliceError::none && !result.accepted &&
                        session.current_snapshot().completed_objectives == completed_before &&
                        session.current_snapshot().selected_choices == selected_before,
                    "repeated completion is idempotent"
                );
                break;
            }
            case tgd::test::F1ContentReplayStepType::advance_interaction:
            case tgd::test::F1ContentReplayStepType::advance_combat: {
                const auto activity =
                    step.type == tgd::test::F1ContentReplayStepType::advance_interaction
                        ? tgd::gameplay::PlaytimeActivityKind::interaction
                        : tgd::gameplay::PlaytimeActivityKind::combat;
                ok &= expect(
                    session.report_playtime_activity(activity) == VerticalSliceError::none,
                    "replay activity is classified"
                );
                const auto advanced = session.advance(step.ticks);
                ok &= expect(
                    advanced.error == VerticalSliceError::none &&
                        advanced.executed_ticks == step.ticks,
                    "replay advances the exact authored tick count"
                );
                break;
            }
            case tgd::test::F1ContentReplayStepType::retry_safe_point: {
                const auto completed_before = session.current_snapshot().completed_objectives;
                const auto quest_checksum_before = session.quest_snapshot().checksum;
                const auto training_lane_before = session.selected_option(
                    tgd::contracts::stable_content_key(
                        "f1_objective_choose_training_lane"
                    )
                );
                const tgd::contracts::SafePointRetryCommand command{
                    session.current_snapshot().tick,
                    definition().player.actor,
                    retry_sequence++,
                };
                ok &= expect(
                    session.retry_from_safe_point(command) == VerticalSliceError::none,
                    "replay retries through the generic safe-point command"
                );
                ok &= expect(
                    session.current_snapshot().completed_objectives == completed_before &&
                        session.quest_snapshot().checksum == quest_checksum_before &&
                        session.selected_option(
                            tgd::contracts::stable_content_key(
                                "f1_objective_choose_training_lane"
                            )
                        ) == training_lane_before &&
                        session.current_snapshot().safe_point_id ==
                            definition().safe_points[1].id &&
                        session.current_snapshot().player_pose ==
                            definition().safe_points[1].pose,
                    "retry preserves accepted training progress and restores Shen Yan"
                );
                break;
            }
        }
    }

    const auto arrival_objective = tgd::contracts::stable_content_key(
        "f1_objective_choose_arrival_clue"
    );
    const auto mooring_objective = tgd::contracts::stable_content_key(
        "f1_objective_choose_mooring_method"
    );
    const auto training_objective = tgd::contracts::stable_content_key(
        "f1_objective_choose_training_lane"
    );
    const auto arrival_clue = session.selected_option(arrival_objective);
    const auto mooring_method = session.selected_option(mooring_objective);
    const auto training_lane = session.selected_option(training_objective);

    ok &= expect(
        completed_steps == replay.expected_completed_objectives,
        "all 25 objectives replay"
    );
    ok &= expect(
        quest_ui_projection_count == 3 && quest_ui.snapshot().sequence == 3,
        "each fixed route consumes the three authored first-two-beat choice projections"
    );
    ok &= expect(
        session.quest_snapshot().completed_total == replay.expected_completed_objectives &&
            session.current_snapshot().completed_objectives == 0 &&
            session.current_snapshot().beat_index == replay.expected_beat_index &&
            session.current_snapshot().safe_point_id == definition().safe_points[2].id,
        "both authored beats are committed before the next safe point"
    );
    ok &= expect(
        arrival_clue == replay.expected_arrival_clue &&
            mooring_method == replay.expected_mooring_method &&
            training_lane == replay.expected_training_lane,
        "replay preserves all three branch selections"
    );
    ok &= expect(
        session.current_snapshot().simulation_ticks == replay.expected_simulation_ticks &&
            session.current_snapshot().playtime.eligible_ticks == replay.expected_eligible_ticks &&
            session.current_snapshot().playtime.failure_retry_ticks ==
                replay.expected_failure_retry_ticks &&
            !session.current_snapshot().playtime.current_beat_target_met &&
            !session.current_snapshot().playtime.playable_target_met,
        "replay audits ticks without pretending to prove the human time gate"
    );

    const auto guard_activation = session.encounter_activation(
        tgd::contracts::stable_content_key("f1_beat_shen_yan_training"),
        tgd::contracts::stable_content_key("f1_objective_take_eavesguard_mark")
    );
    const auto target_activation = session.encounter_activation(
        tgd::contracts::stable_content_key("f1_beat_shen_yan_training"),
        tgd::contracts::stable_content_key("f1_objective_commit_eavesguard_heavy")
    );
    ok &= expect(
        guard_activation.boundary_defined && guard_activation.activation != nullptr &&
            !guard_activation.ambiguous &&
            guard_activation.activation->required_selection_id.key == training_lane &&
            guard_activation.activation->actor_keys.size() == 1 &&
            guard_activation.activation->actor_keys.front() == 104,
        "selected training lane resolves one guard rig"
    );
    ok &= expect(
        target_activation.boundary_defined && target_activation.activation != nullptr &&
            !target_activation.ambiguous &&
            target_activation.activation->required_selection_id.key == training_lane &&
            target_activation.activation->actor_keys.size() == 1 &&
            target_activation.activation->actor_keys.front() == 107,
        "selected training lane resolves one proof target"
    );

    if (replay.expected_quest_checksum != 0) {
        ok &= expect(
            session.quest_snapshot().checksum == replay.expected_quest_checksum,
            "quest checksum matches the frozen fixture"
        );
    }
    if (replay.expected_quest_ui_checksum != 0) {
        ok &= expect(
            quest_ui_checksum == replay.expected_quest_ui_checksum,
            "quest UI projection checksum matches the frozen fixture"
        );
    }
    if (replay.expected_session_checksum != 0) {
        ok &= expect(
            session.current_snapshot().checksum == replay.expected_session_checksum,
            "composed session checksum matches the frozen fixture"
        );
    }

    return {
        ok,
        session.current_snapshot(),
        session.quest_snapshot().checksum,
        quest_ui_checksum,
        arrival_clue,
        mooring_method,
        training_lane,
    };
}

bool test_fixture_is_canonical(const tgd::test::F1ContentReplayFixture& replay) {
    bool ok = expect(
        replay.expected_quest_checksum != 0 &&
            replay.expected_quest_ui_checksum != 0 &&
            replay.expected_session_checksum != 0,
        "the canonical fixture freezes quest, UI, and session checksums"
    );
    const auto left = run_replay(replay);
    const auto right = run_replay(replay);
    ok &= left.ok && right.ok;
    ok &= expect(
        left.snapshot.checksum == right.snapshot.checksum &&
            left.quest_checksum == right.quest_checksum &&
            left.quest_ui_checksum == right.quest_ui_checksum &&
            left.snapshot.player_pose == right.snapshot.player_pose &&
            left.snapshot.playtime.checksum == right.snapshot.playtime.checksum &&
            left.arrival_clue == right.arrival_clue &&
            left.mooring_method == right.mooring_method &&
            left.training_lane == right.training_lane,
        "the same fixed content commands produce the same composed result"
    );
    std::cout << replay.id << " quest=0x" << std::hex << left.quest_checksum
              << " ui=0x" << left.quest_ui_checksum
              << " session=0x" << left.snapshot.checksum << std::dec << '\n';
    return ok;
}

}  // namespace

int main() {
    const auto high_water = tgd::test::make_f1_high_water_windward_replay();
    const auto retry = tgd::test::make_f1_follow_bell_leeward_retry_replay();
    bool ok = test_fixture_is_canonical(high_water);
    ok &= test_fixture_is_canonical(retry);

    const auto high_water_result = run_replay(high_water);
    const auto retry_result = run_replay(retry);
    ok &= expect(
        high_water_result.snapshot.checksum != retry_result.snapshot.checksum &&
            high_water_result.quest_checksum != retry_result.quest_checksum &&
            high_water_result.quest_ui_checksum != retry_result.quest_ui_checksum,
        "different choices and retry history remain distinguishable"
    );
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
