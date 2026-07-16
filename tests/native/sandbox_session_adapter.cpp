#include <tgd/content/sandbox_package_compiler.hpp>
#include <tgd/integration/sandbox_session_adapter.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace tgd::content;
using namespace tgd::contracts;
using namespace tgd::gameplay;
using namespace tgd::integration;

constexpr ContentId player_id = content_id("sandbox.adapter.player");
constexpr ContentId other_player_id = content_id("sandbox.adapter.player.other");
constexpr StableActorKey player_actor = 0xA301U;
constexpr StableContentKey interaction_one_key =
    stable_content_key("sandbox.adapter.interaction.one");
constexpr StableContentKey interaction_two_key =
    stable_content_key("sandbox.adapter.interaction.two");
constexpr StableContentKey mechanism_one_key =
    stable_content_key("sandbox.adapter.mechanism.one");
constexpr StableContentKey blocker_one_key =
    stable_content_key("sandbox.adapter.blocker.one");
constexpr GroundPoseMm spawn_pose{-1'000, 0, 100, 1};
constexpr GroundPoseMm retry_pose{0, 0, 250, 1};
constexpr std::uint32_t spawn_facing = 12'000;
constexpr std::uint32_t retry_facing = 87'000;

static_assert(!std::is_copy_constructible_v<SandboxSessionBlueprint>);
static_assert(std::is_move_constructible_v<SandboxSessionBlueprint>);
static_assert(SandboxSessionBlueprint::id_byte_capacity == sandbox_pack_max_id_bytes);

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "sandbox session adapter failure: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

struct Fixture final {
    std::string player_name{"sandbox.adapter.player"};
    std::array<SandboxAuthoringRegion, 1> regions{{
        {"sandbox.adapter.region", {-5'000, 5'000, -5'000, 5'000, -500, 2'000, 1, 1}},
    }};
    std::array<SandboxAuthoringAsset, 9> assets{{
        {"sandbox.adapter.asset.player", SandboxAssetKind::player},
        {"sandbox.adapter.asset.actor", SandboxAssetKind::actor},
        {"sandbox.adapter.asset.blocker.one", SandboxAssetKind::obstacle},
        {"sandbox.adapter.asset.blocker.two", SandboxAssetKind::obstacle},
        {"sandbox.adapter.asset.safe", SandboxAssetKind::safe_point},
        {"sandbox.adapter.asset.interaction.one", SandboxAssetKind::interaction},
        {"sandbox.adapter.asset.interaction.two", SandboxAssetKind::interaction},
        {"sandbox.adapter.asset.mechanism.one", SandboxAssetKind::mechanism},
        {"sandbox.adapter.asset.mechanism.two", SandboxAssetKind::mechanism},
    }};
    std::array<SandboxAuthoringPlacement, 1> actors{{
        {"sandbox.adapter.actor", "sandbox.adapter.region", "sandbox.adapter.asset.actor",
         {1'000, 0, 0, 1}, 180'000},
    }};
    std::array<SandboxAuthoringGroundBlocker, 2> blockers{{
        {"sandbox.adapter.blocker.one", "sandbox.adapter.region",
         "sandbox.adapter.asset.blocker.one", -100, 100, 400, 600, 0, 500, 1},
        {"sandbox.adapter.blocker.two", "sandbox.adapter.region",
         "sandbox.adapter.asset.blocker.two", 1'900, 2'100, -100, 100, 0, 500, 1},
    }};
    std::array<SandboxAuthoringPlacement, 1> safe_points{{
        {"sandbox.adapter.safe", "sandbox.adapter.region", "sandbox.adapter.asset.safe",
         retry_pose, retry_facing},
    }};
    std::array<SandboxAuthoringPlacement, 2> interactions{{
        {"sandbox.adapter.interaction.one", "sandbox.adapter.region",
         "sandbox.adapter.asset.interaction.one", {-700, 400, 2'000, 1}, 90'000},
        {"sandbox.adapter.interaction.two", "sandbox.adapter.region",
         "sandbox.adapter.asset.interaction.two", {2'000, 0, -500, 1}, 90'000},
    }};
    std::array<SandboxAuthoringPlacement, 2> mechanisms{{
        {"sandbox.adapter.mechanism.one", "sandbox.adapter.region",
         "sandbox.adapter.asset.mechanism.one", {-500, 500, 0, 1}, 90'000},
        {"sandbox.adapter.mechanism.two", "sandbox.adapter.region",
         "sandbox.adapter.asset.mechanism.two", {2'000, 200, 0, 1}, 90'000},
    }};
    std::array<SandboxAuthoringWave, 1> waves{{
        {"sandbox.adapter.wave", "sandbox.adapter.region", "",
         {SandboxTriggerKind::session_started, ""}},
    }};
    std::array<SandboxAuthoringWaveSpawn, 1> wave_spawns{{
        {"sandbox.adapter.wave", "sandbox.adapter.actor", 10, 1},
    }};
    std::array<SandboxAuthoringObjective, 1> objectives{{
        {"sandbox.adapter.objective", "sandbox.adapter.region", "",
         {SandboxObjectiveCompletionKind::wave_completed, "sandbox.adapter.wave"}},
    }};
    std::array<SandboxAuthoringInteractionBinding, 2> interaction_bindings{{
        {"sandbox.adapter.interaction.one", SandboxInteractionOperation::operate,
         500, "sandbox.adapter.mechanism.one"},
        {"sandbox.adapter.interaction.two", SandboxInteractionOperation::operate,
         3'000, "sandbox.adapter.mechanism.two"},
    }};
    std::array<SandboxAuthoringMechanismBinding, 2> mechanism_bindings{{
        {"sandbox.adapter.mechanism.one", SandboxMechanismActivation::one_shot_activate,
         "sandbox.adapter.blocker.one"},
        {"sandbox.adapter.mechanism.two", SandboxMechanismActivation::one_shot_activate,
         "sandbox.adapter.blocker.two"},
    }};
    SandboxAuthoringRuntimeView runtime{};

    Fixture() { rebind(); }

    void rebind() {
        runtime = {
            "sandbox.adapter.package",
            "sandbox.adapter",
            {-10'000, 10'000, -10'000, 10'000, -1'000, 4'000, 1, 1},
            "sandbox.adapter.objective",
            {player_name, "sandbox.adapter.region", "sandbox.adapter.asset.player",
             "sandbox.adapter.safe", spawn_pose, spawn_facing},
            regions,
            assets,
            actors,
            blockers,
            safe_points,
            interactions,
            mechanisms,
            waves,
            wave_spawns,
            objectives,
            interaction_bindings,
            mechanism_bindings,
        };
    }

    void reverse_session_records() {
        std::reverse(assets.begin(), assets.end());
        std::reverse(blockers.begin(), blockers.end());
        std::reverse(interactions.begin(), interactions.end());
        std::reverse(mechanisms.begin(), mechanisms.end());
        std::reverse(interaction_bindings.begin(), interaction_bindings.end());
        std::reverse(mechanism_bindings.begin(), mechanism_bindings.end());
        rebind();
    }
};

[[nodiscard]] std::optional<SandboxSessionBlueprint> compile_blueprint(
    Fixture& fixture
) {
    fixture.rebind();
    auto compiled = compile_sandbox_package(fixture.runtime);
    expect(compiled.succeeded(), "valid authoring input did not compile");
    auto candidate = std::move(compiled).take_candidate();
    expect(candidate != nullptr, "successful compile did not transfer candidate");
    auto built = build_sandbox_session_blueprint(candidate->document());
    expect(built.succeeded() && built.blueprint() != nullptr,
           "valid package document did not build blueprint");
    auto result = std::move(built).take_blueprint();
    expect(result.has_value(), "successful blueprint result did not transfer value");
    candidate.reset();
    return result;
}

[[nodiscard]] SandboxOperateCommand operate_command(
    const SandboxSession& session,
    StableContentKey interaction,
    CommandSequence sequence,
    TickIndex tick
) {
    return {
        session.snapshot().generation,
        tick,
        player_actor,
        sequence,
        interaction,
    };
}

void expect_unchanged(
    const SandboxSessionSnapshot& before,
    const SandboxSession& session,
    std::string_view message
) {
    expect(before == session.snapshot(), message);
}

void test_owned_lifetime_operate_and_retry() {
    std::optional<SandboxSessionBlueprint> blueprint;
    {
        Fixture fixture;
        auto compiled = compile_sandbox_package(fixture.runtime);
        expect(compiled.succeeded(), "lifetime fixture did not compile");
        auto candidate = std::move(compiled).take_candidate();
        auto built = build_sandbox_session_blueprint(candidate->document());
        blueprint = std::move(built).take_blueprint();
        fixture.player_name.assign("destroyed.authoring.player");
        candidate.reset();
    }
    expect(blueprint.has_value(), "blueprint retained package input lifetime");

    SandboxSession session;
    expect(
        initialize_sandbox_session_from_blueprint(
            session, *blueprint, {player_id, player_actor}
        ).error == SandboxSessionBuildError::none,
        "detached blueprint did not initialize session"
    );
    expect(
        session.snapshot().player_pose == spawn_pose &&
            session.snapshot().player_facing_millidegrees == spawn_facing,
        "initial session did not use player spawn"
    );
    const auto first = session.submit_operate(
        operate_command(session, interaction_one_key, 1, 20)
    );
    expect(
        first.result.disposition == SandboxOperateDisposition::completed_chain &&
            first.events[0].sequence == 1 && first.events[1].sequence == 2 &&
            first.events[0].mechanism == mechanism_one_key &&
            first.events[0].ground_blocker == blocker_one_key,
        "500 mm typed chain did not commit ordered events"
    );

    const auto old_generation = session.snapshot().generation;
    expect(
        session.retry({old_generation, 2}) == SandboxSessionRetryDisposition::restored,
        "blueprint-backed retry failed"
    );
    expect(
        session.snapshot().generation == old_generation + 1U &&
            session.snapshot().player_pose == retry_pose &&
            session.snapshot().player_pose != spawn_pose &&
            session.snapshot().player_facing_millidegrees == retry_facing &&
            session.snapshot().last_event_sequence == 2 &&
            session.interaction_state(interaction_one_key) ==
                SandboxInteractionState::uncompleted &&
            session.mechanism_state(mechanism_one_key) == SandboxMechanismState::inactive &&
            session.ground_blocker_state(blocker_one_key) ==
                SandboxGroundBlockerState::enabled_solid,
        "retry did not restore standalone blueprint state"
    );

    const auto after_retry = session.snapshot();
    auto stale = operate_command(session, interaction_one_key, 1, 21);
    stale.generation = old_generation;
    expect(
        session.submit_operate(stale).result.disposition ==
            SandboxOperateDisposition::stale_generation,
        "old generation command survived retry"
    );
    expect_unchanged(after_retry, session, "stale generation changed session");
    const auto second = session.submit_operate(
        operate_command(session, interaction_two_key, 1, 22)
    );
    expect(
        second.result.disposition == SandboxOperateDisposition::completed_chain &&
            second.events[0].sequence == 3 && second.events[1].sequence == 4,
        "event sequence was not monotonic across retry"
    );
}

void test_normalization_and_multi_chain_determinism() {
    Fixture ordered;
    Fixture reversed;
    reversed.reverse_session_records();
    auto first_blueprint = compile_blueprint(ordered);
    auto second_blueprint = compile_blueprint(reversed);
    expect(*first_blueprint == *second_blueprint, "permutation changed blueprint");

    SandboxSession first;
    SandboxSession second;
    expect(
        initialize_sandbox_session_from_blueprint(
            first, *first_blueprint, {player_id, player_actor}
        ).error == SandboxSessionBuildError::none &&
            initialize_sandbox_session_from_blueprint(
                second, *second_blueprint, {player_id, player_actor}
            ).error == SandboxSessionBuildError::none,
        "normalized blueprints did not initialize"
    );
    expect(first.snapshot() == second.snapshot(), "permutation changed session checksum");
    const auto left = first.submit_operate(
        operate_command(first, interaction_one_key, 1, 30)
    );
    const auto right = second.submit_operate(
        operate_command(second, interaction_one_key, 1, 30)
    );
    expect(left == right && first.snapshot() == second.snapshot(),
           "normalized sessions diverged");
    expect(
        first.interaction_state(interaction_two_key) ==
            SandboxInteractionState::uncompleted,
        "first chain polluted second chain"
    );
}

void expect_compile_failure(Fixture& fixture, std::string_view message) {
    fixture.rebind();
    const auto result = compile_sandbox_package(fixture.runtime);
    expect(!result.succeeded() && result.candidate() == nullptr, message);
}

void test_compiler_failures_and_id_capacity() {
    {
        Fixture fixture;
        fixture.player_name.clear();
        expect_compile_failure(fixture, "missing ID exposed a document");
    }
    {
        Fixture fixture;
        fixture.player_name.assign(sandbox_pack_max_id_bytes + 1U, 'p');
        expect_compile_failure(fixture, "over-capacity ID exposed a document");
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[0].target_mechanism_id =
            "sandbox.adapter.mechanism.missing";
        expect_compile_failure(fixture, "dangling target exposed a document");
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[1].interaction_id =
            fixture.interaction_bindings[0].interaction_id;
        expect_compile_failure(fixture, "duplicate binding exposed a document");
    }
    {
        Fixture fixture;
        fixture.interaction_bindings[0].operation = SandboxInteractionOperation::invalid;
        expect_compile_failure(fixture, "invalid enum exposed a document");
    }
    {
        Fixture fixture;
        std::vector<SandboxAuthoringPlacement> oversized(
            sandbox_interaction_capacity + 1U, fixture.interactions[0]
        );
        fixture.runtime.interactions = oversized;
        const auto result = compile_sandbox_package(fixture.runtime);
        expect(!result.succeeded() && result.candidate() == nullptr,
               "over-capacity section exposed a document");
    }

    Fixture exact;
    exact.player_name.assign(sandbox_pack_max_id_bytes, 'p');
    exact.rebind();
    auto compiled = compile_sandbox_package(exact.runtime);
    expect(compiled.succeeded(), "exact ID byte capacity was rejected");
    auto built = build_sandbox_session_blueprint(compiled.candidate()->document());
    expect(built.succeeded(), "blueprint rejected exact ID byte capacity");
    SandboxSession session;
    expect(
        initialize_sandbox_session_from_blueprint(
            session,
            *built.blueprint(),
            {content_id(std::string_view{exact.player_name}), player_actor}
        ).error == SandboxSessionBuildError::none,
        "exact-capacity owned ID failed initialize"
    );
}

void test_atomic_failure_and_move_lifecycle() {
    Fixture fixture;
    auto compiled = compile_sandbox_package(fixture.runtime);
    auto built = build_sandbox_session_blueprint(compiled.candidate()->document());
    expect(built.succeeded(), "atomic fixture blueprint failed");
    auto owned = std::move(built).take_blueprint();
    expect(owned.has_value() && !built.succeeded() && built.blueprint() == nullptr,
           "consumed result retained success");
    expect(!std::move(built).take_blueprint().has_value(),
           "repeated blueprint take succeeded");

    SandboxSessionBlueprint moved{std::move(*owned)};
    SandboxSession destination;
    expect(
        initialize_sandbox_session_from_blueprint(
            destination, moved, {player_id, player_actor}
        ).error == SandboxSessionBuildError::none,
        "moved-to blueprint did not initialize"
    );
    expect(
        destination.submit_operate(
            operate_command(destination, interaction_one_key, 1, 40)
        ).result.disposition == SandboxOperateDisposition::completed_chain,
        "live destination setup failed"
    );
    const auto live = destination.snapshot();
    expect(
        initialize_sandbox_session_from_blueprint(
            destination, *owned, {player_id, player_actor}
        ).error == SandboxSessionBuildError::invalid_owned_state,
        "moved-from blueprint did not fail closed"
    );
    expect_unchanged(live, destination, "moved-from blueprint changed destination");

    for (const auto binding : std::array<SandboxPlayerRuntimeBinding, 3>{
             SandboxPlayerRuntimeBinding{},
             SandboxPlayerRuntimeBinding{player_id, 0},
             SandboxPlayerRuntimeBinding{other_player_id, player_actor},
         }) {
        const auto result = initialize_sandbox_session_from_blueprint(
            destination, moved, binding
        );
        expect(result.error != SandboxSessionBuildError::none,
               "invalid player binding initialized candidate");
        expect_unchanged(live, destination, "failed candidate changed destination");
    }
    expect(
        destination.initialize(
            compiled.candidate()->document().definition(),
            compiled.candidate()->document().gameplay_binding(),
            {player_id, player_actor}
        ).error == SandboxSessionBuildError::already_initialized,
        "direct already-initialized lifecycle changed"
    );
    expect_unchanged(live, destination, "already initialized changed destination");

    SandboxSession fresh_candidate;
    expect(
        initialize_sandbox_session_from_blueprint(
            fresh_candidate, moved, {player_id, player_actor}
        ).error == SandboxSessionBuildError::none,
        "fresh comparison candidate did not initialize"
    );
    expect(
        initialize_sandbox_session_from_blueprint(
            destination, moved, {player_id, player_actor}
        ).error == SandboxSessionBuildError::none,
        "valid blueprint did not replace live destination"
    );
    expect(
        destination.snapshot() == fresh_candidate.snapshot() &&
            destination.snapshot().last_event_sequence == 0 &&
            destination.interaction_state(interaction_one_key) ==
                SandboxInteractionState::uncompleted,
        "successful replacement retained old event or completed-chain state"
    );
    expect(
        !sandbox_next_generation(std::numeric_limits<std::uint32_t>::max()).valid,
        "generation overflow helper did not fail closed"
    );
}

}  // namespace

int main() {
    test_owned_lifetime_operate_and_retry();
    test_normalization_and_multi_chain_determinism();
    test_compiler_failures_and_id_capacity();
    test_atomic_failure_and_move_lifecycle();
    std::cout << "sandbox session adapter checks passed\n";
    return EXIT_SUCCESS;
}
