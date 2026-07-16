#include <tgd/gameplay/sandbox_session.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace tgd::contracts;
using namespace tgd::gameplay;

constexpr ContentId player_id = content_id("sandbox.player");
constexpr ContentId other_player_id = content_id("sandbox.player.other");
constexpr ContentId region_id = content_id("sandbox.region");
constexpr ContentId other_region_id = content_id("sandbox.region.other");
constexpr ContentId safe_point_id = content_id("sandbox.safe-point.initial");
constexpr ContentId other_safe_point_id = content_id("sandbox.safe-point.other");
constexpr ContentId interaction_one = content_id("sandbox.interaction.one");
constexpr ContentId interaction_two = content_id("sandbox.interaction.two");
constexpr ContentId mechanism_one = content_id("sandbox.mechanism.one");
constexpr ContentId mechanism_two = content_id("sandbox.mechanism.two");
constexpr ContentId blocker_one = content_id("sandbox.blocker.one");
constexpr ContentId blocker_two = content_id("sandbox.blocker.two");
constexpr StableActorKey player_actor = 0xA001U;
constexpr StableActorKey other_actor = 0xA002U;

constexpr GroundPoseMm spawn_pose{0, 0, 100, 1};
constexpr GroundPoseMm retry_pose{1'000, 1'000, 250, 1};
constexpr std::uint32_t spawn_facing = 12'000;
constexpr std::uint32_t retry_facing = 87'000;

static_assert(sandbox_next_generation(1) == SandboxGenerationAdvance{true, 2});
static_assert(!sandbox_next_generation(0).valid);
static_assert(
    !sandbox_next_generation(std::numeric_limits<std::uint32_t>::max()).valid
);

SandboxInteractionDefinition interaction_definition(ContentId id, GroundPoseMm pose) {
    SandboxInteractionDefinition result{};
    result.id = id;
    result.region_id = region_id;
    result.pose = pose;
    return result;
}

SandboxMechanismDefinition mechanism_definition(ContentId id) {
    SandboxMechanismDefinition result{};
    result.id = id;
    result.region_id = region_id;
    return result;
}

SandboxGroundBlockerDefinition blocker_definition(ContentId id) {
    SandboxGroundBlockerDefinition result{};
    result.id = id;
    result.region_id = region_id;
    result.floor_layer = 1;
    return result;
}

SandboxSafePointDefinition safe_point_definition() {
    SandboxSafePointDefinition result{};
    result.id = safe_point_id;
    result.region_id = region_id;
    result.pose = retry_pose;
    result.facing_millidegrees = retry_facing;
    return result;
}

struct Fixture final {
    SandboxPlayerDefinition player{};
    std::array<SandboxInteractionDefinition, 2> interactions{
        interaction_definition(interaction_one, {300, 400, 20'000, 1}),
        interaction_definition(interaction_two, {3'000, 0, -20'000, 1}),
    };
    std::array<SandboxMechanismDefinition, 2> mechanisms{
        mechanism_definition(mechanism_one),
        mechanism_definition(mechanism_two),
    };
    std::array<SandboxGroundBlockerDefinition, 2> blockers{
        blocker_definition(blocker_one),
        blocker_definition(blocker_two),
    };
    std::array<SandboxSafePointDefinition, 1> safe_points{safe_point_definition()};
    std::array<SandboxInteractionGameplayBinding, 2> interaction_bindings{
        SandboxInteractionGameplayBinding{
            interaction_one,
            SandboxInteractionOperation::operate,
            500,
            mechanism_one,
        },
        SandboxInteractionGameplayBinding{
            interaction_two,
            SandboxInteractionOperation::operate,
            3'000,
            mechanism_two,
        },
    };
    std::array<SandboxMechanismGameplayBinding, 2> mechanism_bindings{
        SandboxMechanismGameplayBinding{
            mechanism_one,
            SandboxMechanismActivation::one_shot_activate,
            blocker_one,
        },
        SandboxMechanismGameplayBinding{
            mechanism_two,
            SandboxMechanismActivation::one_shot_activate,
            blocker_two,
        },
    };

    Fixture() {
        player.id = player_id;
        player.region_id = region_id;
        player.initial_safe_point_id = safe_point_id;
        player.pose = spawn_pose;
        player.facing_millidegrees = spawn_facing;
    }

    [[nodiscard]] SandboxDefinition core() const noexcept {
        SandboxDefinition result{};
        result.player = player;
        result.interactions = interactions;
        result.mechanisms = mechanisms;
        result.ground_blockers = blockers;
        result.safe_points = safe_points;
        return result;
    }

    [[nodiscard]] SandboxGameplayBindingDefinition binding() const noexcept {
        return {interaction_bindings, mechanism_bindings};
    }

    [[nodiscard]] SandboxPlayerRuntimeBinding player_binding() const noexcept {
        return {player.id, player_actor};
    }
};

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "sandbox session failure: " << message << '\n';
    }
}

[[nodiscard]] SandboxOperateCommand operate_command(
    const SandboxSession& session,
    StableContentKey interaction,
    CommandSequence sequence,
    TickIndex tick = 10,
    StableActorKey actor = player_actor
) {
    SandboxOperateCommand command{};
    command.generation = session.snapshot().generation;
    command.completed_tick = tick;
    command.actor = actor;
    command.sequence = sequence;
    command.interaction = interaction;
    return command;
}

void expect_unchanged(
    const SandboxSessionSnapshot& before,
    const SandboxSession& session,
    std::string_view message
) {
    expect(before == session.snapshot(), message);
}

void test_build_owns_and_normalizes_inputs() {
    Fixture ordered;
    Fixture reversed;
    std::swap(reversed.interactions[0], reversed.interactions[1]);
    std::swap(reversed.mechanisms[0], reversed.mechanisms[1]);
    std::swap(reversed.blockers[0], reversed.blockers[1]);
    std::swap(reversed.interaction_bindings[0], reversed.interaction_bindings[1]);
    std::swap(reversed.mechanism_bindings[0], reversed.mechanism_bindings[1]);

    SandboxSession first;
    SandboxSession second;
    expect(
        first.initialize(ordered.core(), ordered.binding(), ordered.player_binding()).error ==
            SandboxSessionBuildError::none,
        "ordered session builds"
    );
    expect(
        second.initialize(reversed.core(), reversed.binding(), reversed.player_binding()).error ==
            SandboxSessionBuildError::none,
        "reversed session builds"
    );
    expect(first.snapshot() == second.snapshot(), "input order normalizes to one snapshot");

    SandboxSession detached;
    {
        Fixture fixture;
        std::string dynamic_player_name{"sandbox.player"};
        const auto dynamic_player_id = content_id(std::string_view{dynamic_player_name});
        fixture.player.id = dynamic_player_id;
        const SandboxPlayerRuntimeBinding runtime_binding{dynamic_player_id, player_actor};
        expect(
            detached.initialize(fixture.core(), fixture.binding(), runtime_binding).error ==
                SandboxSessionBuildError::none,
            "session accepts temporary ContentId names"
        );
        fixture.interactions[0] = {};
        fixture.mechanisms[0] = {};
        fixture.blockers[0] = {};
        fixture.interaction_bindings[0] = {};
        fixture.mechanism_bindings[0] = {};
        dynamic_player_name.assign("destroyed.source.name");
    }
    const auto dispatch = detached.submit_operate(
        operate_command(detached, interaction_one.key, 1)
    );
    expect(
        dispatch.result.disposition == SandboxOperateDisposition::completed_chain,
        "session retains no span, string view, or input pointer"
    );
}

void test_player_binding_and_safe_point_failures() {
    {
        Fixture fixture;
        SandboxSession session;
        expect(
            session.initialize(fixture.core(), fixture.binding(), {}).error ==
                SandboxSessionBuildError::missing_player_runtime_binding,
            "missing player runtime binding fails"
        );
    }
    {
        Fixture fixture;
        auto malformed = fixture.player.id;
        ++malformed.key;
        SandboxSession session;
        expect(
            session.initialize(
                fixture.core(),
                fixture.binding(),
                {malformed, player_actor}
            ).error == SandboxSessionBuildError::invalid_player_runtime_id,
            "non-canonical player ContentId fails"
        );
    }
    {
        Fixture fixture;
        SandboxSession session;
        expect(
            session.initialize(
                fixture.core(),
                fixture.binding(),
                {other_player_id, player_actor}
            ).error == SandboxSessionBuildError::player_runtime_id_mismatch,
            "wrong player ContentId fails"
        );
    }
    {
        Fixture fixture;
        SandboxSession session;
        expect(
            session.initialize(
                fixture.core(),
                fixture.binding(),
                {fixture.player.id, 0}
            ).error == SandboxSessionBuildError::invalid_player_actor,
            "zero runtime actor fails"
        );
    }
    {
        Fixture fixture;
        fixture.player.initial_safe_point_id = other_safe_point_id;
        SandboxSession session;
        expect(
            session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::invalid_initial_safe_point,
            "missing initial safe point fails"
        );
    }
    {
        Fixture fixture;
        std::array<SandboxSafePointDefinition, 2> duplicates{
            fixture.safe_points[0],
            fixture.safe_points[0],
        };
        auto core = fixture.core();
        core.safe_points = duplicates;
        SandboxSession session;
        expect(
            session.initialize(core, fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::invalid_initial_safe_point,
            "duplicate initial safe point fails"
        );
    }
    {
        Fixture fixture;
        fixture.safe_points[0].region_id = other_region_id;
        SandboxSession session;
        expect(
            session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::invalid_initial_safe_point,
            "cross-region initial safe point fails"
        );
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[0].range_mm = 499;
        SandboxSession session;
        const auto before = session.snapshot();
        expect(
            session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::invalid_gameplay_binding,
            "invalid gameplay binding fails through the unique validator"
        );
        expect_unchanged(before, session, "failed build leaves default snapshot unchanged");
    }
    {
        Fixture fixture;
        std::array<SandboxInteractionDefinition, sandbox_interaction_capacity + 1>
            too_many_interactions{};
        auto core = fixture.core();
        core.interactions = too_many_interactions;
        SandboxSession session;
        expect(
            session.initialize(core, fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::capacity_exceeded,
            "consumed capacity fails before copying"
        );
    }
}

void test_first_operate_repeat_replay_and_isolation() {
    Fixture fixture;
    SandboxSession session;
    expect(
        session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
            SandboxSessionBuildError::none,
        "atomic fixture builds"
    );
    const auto initial = session.snapshot();
    expect(initial.player_pose == spawn_pose, "initial session starts at player spawn");
    expect(initial.player_facing_millidegrees == spawn_facing, "spawn facing is owned");
    expect(
        session.interaction_state(interaction_one.key) == SandboxInteractionState::uncompleted &&
            session.mechanism_state(mechanism_one.key) == SandboxMechanismState::inactive &&
            session.ground_blocker_state(blocker_one.key) ==
                SandboxGroundBlockerState::enabled_solid,
        "first chain starts canonical"
    );

    const auto first = session.submit_operate(
        operate_command(session, interaction_one.key, 1, 10)
    );
    expect(
        first.result.disposition == SandboxOperateDisposition::completed_chain,
        "500 mm x/y boundary accepts while height is ignored"
    );
    expect(sandbox_operate_event_count(first.result) == 2, "first completion has two events");
    expect(
        first.events[0].kind == SandboxGameplayEventKind::interaction_completed &&
            first.events[1].kind == SandboxGameplayEventKind::mechanism_activated &&
            first.events[0].sequence == 1 && first.events[1].sequence == 2,
        "first events have fixed order and contiguous sequence"
    );
    expect(
        first.events[0].actor == player_actor &&
            first.events[0].interaction == interaction_one.key &&
            first.events[0].mechanism == mechanism_one.key &&
            first.events[0].ground_blocker == blocker_one.key &&
            first.events[1].interaction == interaction_one.key &&
            first.events[1].mechanism == mechanism_one.key,
        "both events carry the explicit player and full typed chain"
    );
    expect(
        session.interaction_state(interaction_one.key) == SandboxInteractionState::completed &&
            session.mechanism_state(mechanism_one.key) == SandboxMechanismState::activated &&
            session.ground_blocker_state(blocker_one.key) ==
                SandboxGroundBlockerState::disabled_non_solid,
        "first completion atomically commits all three states"
    );
    expect(
        session.interaction_state(interaction_two.key) == SandboxInteractionState::uncompleted &&
            session.mechanism_state(mechanism_two.key) == SandboxMechanismState::inactive &&
            session.ground_blocker_state(blocker_two.key) ==
                SandboxGroundBlockerState::enabled_solid,
        "first chain does not drift the second chain"
    );
    expect(
        session.snapshot().last_command_sequence == 1 &&
            session.snapshot().last_completed_tick == 10 &&
            session.snapshot().last_event_sequence == 2 &&
            session.snapshot().checksum != initial.checksum,
        "only completed chain advances last-valid state"
    );

    const auto completed_snapshot = session.snapshot();
    const auto repeat = session.submit_operate(
        operate_command(session, interaction_one.key, 2, 11)
    );
    expect(
        repeat.result.disposition == SandboxOperateDisposition::repeated_chain &&
            sandbox_operate_event_count(repeat.result) == 0,
        "fresh sequence on a complete chain is an event-free repeat"
    );
    expect_unchanged(completed_snapshot, session, "repeat has zero live/checksum drift");

    const auto replay = session.submit_operate(
        operate_command(session, interaction_one.key, 1, 12)
    );
    expect(
        replay.result.disposition == SandboxOperateDisposition::stale_sequence,
        "committed sequence replay is stale, not repeat"
    );
    expect_unchanged(completed_snapshot, session, "stale replay has zero drift");

    const auto second = session.submit_operate(
        operate_command(session, interaction_two.key, 2, 13)
    );
    expect(
        second.result.disposition == SandboxOperateDisposition::completed_chain,
        "3000 mm boundary accepts"
    );
    expect(
        second.events[0].sequence == 3 && second.events[1].sequence == 4,
        "second chain continues event sequence after repeat and replay"
    );
}

void test_rejection_priority_and_range() {
    {
        SandboxSession uninitialized;
        const auto before = uninitialized.snapshot();
        SandboxOperateCommand command{};
        const auto result = uninitialized.submit_operate(command);
        expect(
            result.result.disposition == SandboxOperateDisposition::invalid_binding,
            "uninitialized state has highest operate rejection priority"
        );
        expect_unchanged(before, uninitialized, "uninitialized rejection has no drift");
        expect(
            uninitialized.retry({1, 1}) == SandboxSessionRetryDisposition::invalid_state,
            "uninitialized retry fails as invalid state"
        );
        expect_unchanged(before, uninitialized, "invalid retry has no drift");
    }
    {
        Fixture fixture;
        SandboxSession session;
        expect(
            session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::none,
            "priority fixture builds"
        );
        const auto before = session.snapshot();
        SandboxOperateCommand command{};
        command.generation = 0;
        command.sequence = 0;
        command.actor = other_actor;
        command.interaction = content_id("unknown.interaction").key;
        expect(
            session.submit_operate(command).result.disposition ==
                SandboxOperateDisposition::stale_generation,
            "stale generation precedes every command-level rejection"
        );
        command.generation = session.snapshot().generation;
        expect(
            session.submit_operate(command).result.disposition ==
                SandboxOperateDisposition::stale_sequence,
            "stale sequence precedes actor and interaction lookup"
        );
        command.sequence = 1;
        expect(
            session.submit_operate(command).result.disposition ==
                SandboxOperateDisposition::invalid_binding,
            "actor mismatch precedes unknown interaction"
        );
        command.actor = player_actor;
        expect(
            session.submit_operate(command).result.disposition ==
                SandboxOperateDisposition::unknown_interaction,
            "unknown interaction fails after actor identity"
        );
        expect_unchanged(before, session, "all priority rejections preserve last-valid");
    }
    {
        Fixture fixture;
        fixture.interactions[1].pose.x = 3'001;
        SandboxSession session;
        expect(
            session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::none,
            "+1 mm fixture builds"
        );
        const auto before = session.snapshot();
        expect(
            session.submit_operate(
                operate_command(session, interaction_two.key, 1)
            ).result.disposition == SandboxOperateDisposition::out_of_range,
            "range plus one millimetre rejects"
        );
        expect_unchanged(before, session, "out of range has zero drift");
    }
    {
        Fixture fixture;
        fixture.interactions[0].pose.floor_layer = 2;
        SandboxSession session;
        expect(
            session.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::none,
            "wrong-floor fixture builds"
        );
        const auto before = session.snapshot();
        expect(
            session.submit_operate(
                operate_command(session, interaction_one.key, 1)
            ).result.disposition == SandboxOperateDisposition::floor_mismatch,
            "wrong floor rejects before distance"
        );
        expect_unchanged(before, session, "floor mismatch has zero drift");
    }
}

void test_retry_generation_and_determinism() {
    Fixture fixture;
    SandboxSession first;
    SandboxSession second;
    expect(
        first.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::none &&
            second.initialize(fixture.core(), fixture.binding(), fixture.player_binding()).error ==
                SandboxSessionBuildError::none,
        "deterministic retry sessions build"
    );
    const auto first_dispatch = first.submit_operate(
        operate_command(first, interaction_one.key, 1, 20)
    );
    const auto second_dispatch = second.submit_operate(
        operate_command(second, interaction_one.key, 1, 20)
    );
    expect(first_dispatch == second_dispatch, "matching first commands emit identical batches");
    expect(first.snapshot() == second.snapshot(), "matching first commands produce one checksum");

    const auto before_retry = first.snapshot();
    expect(
        first.retry({0, 2}) == SandboxSessionRetryDisposition::stale_generation,
        "retry rejects stale generation"
    );
    expect_unchanged(before_retry, first, "stale retry generation preserves old state");
    expect(
        first.retry({before_retry.generation, 1}) ==
            SandboxSessionRetryDisposition::stale_sequence,
        "retry rejects committed sequence"
    );
    expect_unchanged(before_retry, first, "stale retry sequence preserves old state");

    expect(
        first.retry({before_retry.generation, 2}) == SandboxSessionRetryDisposition::restored &&
            second.retry({before_retry.generation, 2}) == SandboxSessionRetryDisposition::restored,
        "valid retry restores both sessions"
    );
    expect(first.snapshot() == second.snapshot(), "retry remains deterministic");
    expect(
        first.snapshot().generation == 2 && first.snapshot().player_pose == retry_pose &&
            first.snapshot().player_pose != spawn_pose &&
            first.snapshot().player_facing_millidegrees == retry_facing,
        "retry uses authored initial safe-point pose and facing, not spawn"
    );
    expect(
        first.snapshot().last_command_sequence == 0 &&
            first.snapshot().last_completed_tick == 0 &&
            first.snapshot().last_event_sequence == 2,
        "retry resets generation-local command state but preserves event sequence"
    );
    expect(
        first.interaction_state(interaction_one.key) == SandboxInteractionState::uncompleted &&
            first.mechanism_state(mechanism_one.key) == SandboxMechanismState::inactive &&
            first.ground_blocker_state(blocker_one.key) ==
                SandboxGroundBlockerState::enabled_solid &&
            first.interaction_state(interaction_two.key) == SandboxInteractionState::uncompleted,
        "retry restores every owned chain atomically"
    );

    const auto after_retry = first.snapshot();
    SandboxOperateCommand old_generation{};
    old_generation.generation = 1;
    old_generation.completed_tick = 21;
    old_generation.actor = player_actor;
    old_generation.sequence = 2;
    old_generation.interaction = interaction_one.key;
    expect(
        first.submit_operate(old_generation).result.disposition ==
            SandboxOperateDisposition::stale_generation,
        "old generation command is stale after retry"
    );
    expect_unchanged(after_retry, first, "old generation command has zero drift");

    const auto post_retry_dispatch = first.submit_operate(
        operate_command(first, interaction_two.key, 1, 22)
    );
    expect(
        post_retry_dispatch.result.disposition == SandboxOperateDisposition::completed_chain &&
            post_retry_dispatch.events[0].sequence == 3 &&
            post_retry_dispatch.events[1].sequence == 4,
        "events remain monotonic across generations"
    );
    const auto mirrored_post_retry = second.submit_operate(
        operate_command(second, interaction_two.key, 1, 22)
    );
    expect(
        post_retry_dispatch == mirrored_post_retry && first.snapshot() == second.snapshot(),
        "post-retry operation is deterministic"
    );

    const auto exhausted = sandbox_next_generation(
        std::numeric_limits<std::uint32_t>::max()
    );
    expect(!exhausted.valid, "production generation helper fails closed at uint32 max");
}

}  // namespace

int main() {
    test_build_owns_and_normalizes_inputs();
    test_player_binding_and_safe_point_failures();
    test_first_operate_repeat_replay_and_isolation();
    test_rejection_priority_and_range();
    test_retry_generation_and_determinism();

    if (failures != 0) {
        std::cerr << failures << " sandbox session checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "sandbox session checks passed\n";
    return EXIT_SUCCESS;
}
