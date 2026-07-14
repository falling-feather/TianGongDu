#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/gameplay/vertical_slice_session.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/test/f1_content_replay.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

using tgd::gameplay::VerticalSliceError;
using tgd::gameplay::VerticalSliceSession;

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

    std::uint64_t completed_steps = 0;
    std::uint32_t retry_sequence = 1;
    for (const auto& step : replay.steps) {
        switch (step.type) {
            case tgd::test::F1ContentReplayStepType::complete_objective: {
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
        arrival_clue,
        mooring_method,
        training_lane,
    };
}

bool test_fixture_is_canonical(const tgd::test::F1ContentReplayFixture& replay) {
    const auto left = run_replay(replay);
    const auto right = run_replay(replay);
    bool ok = left.ok && right.ok;
    ok &= expect(
        left.snapshot.checksum == right.snapshot.checksum &&
            left.quest_checksum == right.quest_checksum &&
            left.snapshot.player_pose == right.snapshot.player_pose &&
            left.snapshot.playtime.checksum == right.snapshot.playtime.checksum &&
            left.arrival_clue == right.arrival_clue &&
            left.mooring_method == right.mooring_method &&
            left.training_lane == right.training_lane,
        "the same fixed content commands produce the same composed result"
    );
    std::cout << replay.id << " quest=0x" << std::hex << left.quest_checksum
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
            high_water_result.quest_checksum != retry_result.quest_checksum,
        "different choices and retry history remain distinguishable"
    );
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
