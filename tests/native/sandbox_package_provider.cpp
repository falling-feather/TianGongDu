#include <tgd/content/sandbox_package_provider.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

using namespace tgd::contracts;
using namespace tgd::content;

static_assert(static_cast<std::uint8_t>(SandboxPackageGenerationAdvanceStatus::advanced) ==
              1);
static_assert(static_cast<std::uint8_t>(SandboxPackageGenerationAdvanceStatus::exhausted) ==
              2);
static_assert(static_cast<std::uint8_t>(SandboxPackageGenerationAdvanceStatus::invalid) ==
              255);
static_assert(static_cast<std::uint8_t>(SandboxPackagePrepareStatus::prepared) == 1);
static_assert(static_cast<std::uint8_t>(SandboxPackagePrepareStatus::stale_generation) ==
              2);
static_assert(static_cast<std::uint8_t>(SandboxPackagePrepareStatus::stale_checksum) == 3);
static_assert(static_cast<std::uint8_t>(SandboxPackagePrepareStatus::missing_candidate) ==
              4);
static_assert(static_cast<std::uint8_t>(SandboxPackagePrepareStatus::generation_exhausted) ==
              5);
static_assert(static_cast<std::uint8_t>(SandboxPackagePrepareStatus::invalid) == 255);
static_assert(static_cast<std::uint8_t>(SandboxPackageCommitStatus::committed) == 1);
static_assert(static_cast<std::uint8_t>(SandboxPackageCommitStatus::foreign_provider) == 2);
static_assert(static_cast<std::uint8_t>(SandboxPackageCommitStatus::stale_generation) == 3);
static_assert(static_cast<std::uint8_t>(SandboxPackageCommitStatus::stale_checksum) == 4);
static_assert(static_cast<std::uint8_t>(SandboxPackageCommitStatus::invalid_prepared_update) ==
              5);
static_assert(static_cast<std::uint8_t>(SandboxPackageCommitStatus::invalid) == 255);

static_assert(!std::is_copy_constructible_v<SandboxPackageProvider>);
static_assert(!std::is_copy_assignable_v<SandboxPackageProvider>);
static_assert(!std::is_move_constructible_v<SandboxPackageProvider>);
static_assert(!std::is_move_assignable_v<SandboxPackageProvider>);
static_assert(!std::is_aggregate_v<SandboxPackageProvider>);
static_assert(!std::is_copy_constructible_v<SandboxPackagePreparedUpdate>);
static_assert(std::is_move_constructible_v<SandboxPackagePreparedUpdate>);
static_assert(!std::is_default_constructible_v<SandboxPackagePreparedUpdate>);
static_assert(!std::is_aggregate_v<SandboxPackagePreparedUpdate>);
static_assert(!std::is_copy_constructible_v<SandboxPackagePrepareResult>);
static_assert(std::is_move_constructible_v<SandboxPackagePrepareResult>);
static_assert(!std::is_aggregate_v<SandboxPackagePrepareResult>);
static_assert(!std::is_aggregate_v<SandboxPackageCommitResult>);
static_assert(!std::is_aggregate_v<SandboxPackagePublicationIdentity>);

static_assert(!sandbox_package_generation_advance_status_valid(
    static_cast<SandboxPackageGenerationAdvanceStatus>(0)
));
static_assert(!sandbox_package_generation_advance_status_valid(
    static_cast<SandboxPackageGenerationAdvanceStatus>(3)
));
static_assert(!sandbox_package_generation_advance_status_valid(
    SandboxPackageGenerationAdvanceStatus::invalid
));
static_assert(!sandbox_package_prepare_status_valid(
    static_cast<SandboxPackagePrepareStatus>(0)
));
static_assert(!sandbox_package_prepare_status_valid(
    static_cast<SandboxPackagePrepareStatus>(6)
));
static_assert(!sandbox_package_prepare_status_valid(SandboxPackagePrepareStatus::invalid));
static_assert(!sandbox_package_commit_status_valid(
    static_cast<SandboxPackageCommitStatus>(0)
));
static_assert(!sandbox_package_commit_status_valid(
    static_cast<SandboxPackageCommitStatus>(6)
));
static_assert(!sandbox_package_commit_status_valid(SandboxPackageCommitStatus::invalid));

[[noreturn]] void fail(std::string_view message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

struct Fixture final {
    std::string package_name{"sandbox.package.provider"};
    std::array<SandboxAuthoringRegion, 1> regions{{
        {"sandbox.region.main", {-5'000, 5'000, -5'000, 5'000, -500, 2'000, 0, 0}},
    }};
    std::array<SandboxAuthoringAsset, 6> assets{{
        {"asset.player", SandboxAssetKind::player},
        {"asset.actor", SandboxAssetKind::actor},
        {"asset.blocker", SandboxAssetKind::obstacle},
        {"asset.safe", SandboxAssetKind::safe_point},
        {"asset.interaction", SandboxAssetKind::interaction},
        {"asset.mechanism", SandboxAssetKind::mechanism},
    }};
    std::array<SandboxAuthoringPlacement, 1> actors{{
        {"sandbox.actor", "sandbox.region.main", "asset.actor", {1'000, 0, 0, 0},
         180'000},
    }};
    std::array<SandboxAuthoringGroundBlocker, 1> blockers{{
        {"sandbox.blocker", "sandbox.region.main", "asset.blocker", 400, 600, -100,
         100, 0, 500, 0},
    }};
    std::array<SandboxAuthoringPlacement, 1> safe_points{{
        {"sandbox.safe", "sandbox.region.main", "asset.safe", {-1'000, 0, 0, 0}, 0},
    }};
    std::array<SandboxAuthoringPlacement, 1> interactions{{
        {"sandbox.interaction", "sandbox.region.main", "asset.interaction",
         {0, 500, 0, 0}, 90'000},
    }};
    std::array<SandboxAuthoringPlacement, 1> mechanisms{{
        {"sandbox.mechanism", "sandbox.region.main", "asset.mechanism",
         {0, 700, 100, 0}, 90'000},
    }};
    std::array<SandboxAuthoringWave, 1> waves{{
        {"sandbox.wave", "sandbox.region.main", "",
         {SandboxTriggerKind::session_started, ""}},
    }};
    std::array<SandboxAuthoringWaveSpawn, 1> wave_spawns{{
        {"sandbox.wave", "sandbox.actor", 10, 1},
    }};
    std::array<SandboxAuthoringObjective, 1> objectives{{
        {"sandbox.objective", "sandbox.region.main", "",
         {SandboxObjectiveCompletionKind::wave_completed, "sandbox.wave"}},
    }};
    std::array<SandboxAuthoringInteractionBinding, 1> interaction_bindings{{
        {"sandbox.interaction", SandboxInteractionOperation::operate, 500,
         "sandbox.mechanism"},
    }};
    std::array<SandboxAuthoringMechanismBinding, 1> mechanism_bindings{{
        {"sandbox.mechanism", SandboxMechanismActivation::one_shot_activate,
         "sandbox.blocker"},
    }};
    std::array<SandboxAuthoringActorBinding, 1> actor_bindings{{
        {"sandbox.actor", "profile.actor", CombatFaction::hostile,
         EncounterTacticalDuty::pressure, 50},
    }};
    SandboxAuthoringRuntimeView runtime{};

    Fixture() { rebind(); }

    void rebind() {
        runtime = {
            package_name,
            "sandbox.provider",
            {-10'000, 10'000, -10'000, 10'000, -1'000, 4'000, 0, 1},
            "sandbox.objective",
            {
                "sandbox.player",
                "sandbox.region.main",
                "asset.player",
                "sandbox.safe",
                {0, 0, 0, 0},
                0,
            },
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
            actor_bindings,
        };
    }

    void reverse_ordered_sections() {
        std::reverse(assets.begin(), assets.end());
        rebind();
    }
};

std::unique_ptr<SandboxPackageCandidate> compile_candidate(Fixture& fixture) {
    fixture.rebind();
    auto compiled = compile_sandbox_package(fixture.runtime);
    expect(compiled.status() == SandboxPackageCompileStatus::succeeded,
           "valid provider fixture did not compile");
    auto candidate = std::move(compiled).take_candidate();
    expect(candidate != nullptr, "successful compile did not transfer candidate");
    return candidate;
}

struct PublicationSnapshot final {
    SandboxPackagePublicationIdentity identity{};
    const SandboxPackageCandidate* candidate{};
    const SandboxPackageDocument* document{};
};

PublicationSnapshot snapshot(const SandboxPackageProvider& provider) {
    return {provider.identity(), provider.candidate(), provider.document()};
}

void expect_preserved(
    const SandboxPackageProvider& provider,
    const PublicationSnapshot& before,
    std::string_view message
) {
    expect(provider.identity() == before.identity, message);
    expect(provider.candidate() == before.candidate, message);
    expect(provider.document() == before.document, message);
}

std::optional<SandboxPackagePreparedUpdate> prepare_candidate(
    SandboxPackageProvider& provider,
    const SandboxPackagePublicationIdentity& expected,
    std::unique_ptr<SandboxPackageCandidate> candidate
) {
    auto result = provider.prepare(expected, std::move(candidate));
    expect(result.status() == SandboxPackagePrepareStatus::prepared,
           "valid candidate was not prepared");
    expect(result.prepared_update() != nullptr,
           "prepared result omitted its unique token");
    auto update = std::move(result).take_prepared_update();
    expect(update.has_value(), "prepared token could not be transferred");
    expect(result.status() == SandboxPackagePrepareStatus::invalid &&
               result.prepared_update() == nullptr &&
               !std::move(result).take_prepared_update().has_value(),
           "consumed prepare result retained a token");
    return update;
}

void check_initial_prepare_commit_and_lifetime() {
    SandboxPackageProvider provider;
    const SandboxPackagePublicationIdentity empty{};
    expect(provider.identity() == empty && provider.identity().generation() == 0 &&
               provider.identity().checksum() == Sha256Digest{} &&
               provider.candidate() == nullptr && provider.document() == nullptr,
           "provider did not start at the empty identity");

    std::optional<SandboxPackagePreparedUpdate> prepared;
    Sha256Digest fingerprint{};
    const SandboxPackageCandidate* prepared_candidate{};
    {
        Fixture fixture;
        auto compiled = compile_sandbox_package(fixture.runtime);
        expect(compiled.succeeded(), "owned lifetime fixture did not compile");
        auto candidate = std::move(compiled).take_candidate();
        fingerprint = candidate->fingerprint();
        const auto before = snapshot(provider);
        prepared = prepare_candidate(provider, empty, std::move(candidate));
        prepared_candidate = prepared->candidate();
        expect(prepared->document() != nullptr &&
                   prepared->document()->definition().package_id.name ==
                       "sandbox.package.provider",
               "prepared update did not expose its owning document");
        expect(prepared->next_identity().generation() == 1 &&
                   prepared->next_identity().checksum() == fingerprint,
               "prepared next identity did not derive the candidate fingerprint");
        expect_preserved(provider, before, "prepare mutated the empty publication");
        fixture.package_name.assign("destroyed-authoring-input");
    }
    expect(prepared->document()->definition().package_id.name ==
               "sandbox.package.provider",
           "prepared update retained compiler input lifetime");
    expect(provider.commit(std::move(*prepared)).status() ==
               SandboxPackageCommitStatus::committed,
           "first prepared update did not commit");
    expect(provider.identity().generation() == 1 &&
               provider.identity().checksum() == fingerprint &&
               provider.candidate() == prepared_candidate &&
               provider.document() != nullptr &&
               provider.document()->definition().package_id.name ==
                   "sandbox.package.provider",
           "first commit did not publish the exact owning candidate");
    const auto published = snapshot(provider);
    expect(provider.commit(std::move(*prepared)).status() ==
               SandboxPackageCommitStatus::invalid_prepared_update,
           "reused committed token was accepted");
    expect_preserved(provider, published, "reused token changed last-valid");
}

void check_determinism_and_same_checksum_republish() {
    SandboxPackageProvider provider;
    Fixture ordered;
    Fixture shuffled;
    shuffled.reverse_ordered_sections();
    auto first = compile_candidate(ordered);
    auto second = compile_candidate(shuffled);
    const auto fingerprint = first->fingerprint();
    expect(std::equal(first->bytes().begin(), first->bytes().end(),
                      second->bytes().begin(), second->bytes().end()),
           "shuffled authoring changed the canonical package bytes");
    expect(second->fingerprint() == fingerprint,
           "shuffled authoring changed the candidate fingerprint");

    auto first_update = prepare_candidate(provider, provider.identity(), std::move(first));
    expect(provider.commit(std::move(*first_update)).status() ==
               SandboxPackageCommitStatus::committed,
           "deterministic first publication failed");
    const auto first_address = provider.candidate();
    const auto before_prepare = snapshot(provider);
    auto second_update = prepare_candidate(provider, provider.identity(), std::move(second));
    expect(second_update->candidate() != first_address,
           "two simultaneously owned candidates shared an address");
    expect_preserved(provider, before_prepare,
                     "same-checksum prepare changed the current publication");
    expect(provider.commit(std::move(*second_update)).status() ==
               SandboxPackageCommitStatus::committed,
           "same-checksum republish failed");
    expect(provider.identity().generation() == 2 &&
               provider.identity().checksum() == fingerprint &&
               provider.candidate() != first_address,
           "same checksum did not advance publication generation");
}

void check_compile_and_prepare_failures_preserve() {
    SandboxPackageProvider provider;
    Fixture initial;
    auto initial_update =
        prepare_candidate(provider, provider.identity(), compile_candidate(initial));
    expect(provider.commit(std::move(*initial_update)).status() ==
               SandboxPackageCommitStatus::committed,
           "failure fixture initial publication failed");

    Fixture invalid;
    invalid.interaction_bindings.front().range_mm = 499;
    invalid.rebind();
    const auto before_compile = snapshot(provider);
    auto failed = compile_sandbox_package(invalid.runtime);
    expect(!failed.succeeded() && failed.candidate() == nullptr &&
               failed.validation().error ==
                   SandboxPackageError::gameplay_binding_validation_failed &&
               failed.validation().gameplay_binding_validation.code ==
                   SandboxGameplayBindingValidationCode::invalid_operate_range,
           "compiler semantic failure lost its existing validation");
    const auto validation_error = failed.validation().error;
    const auto diagnostics = failed.validation().diagnostics;
    const auto binding_validation = failed.validation().gameplay_binding_validation;
    auto missing = provider.prepare(
        provider.identity(), std::move(failed).take_candidate()
    );
    expect(missing.status() == SandboxPackagePrepareStatus::missing_candidate &&
               missing.prepared_update() == nullptr,
           "missing compiler candidate produced a token");
    expect(failed.validation().error == validation_error &&
               failed.validation().diagnostics == diagnostics &&
               failed.validation().gameplay_binding_validation == binding_validation,
           "provider path rewrote compiler validation/diagnostics");
    expect_preserved(provider, before_compile,
                     "compiler/missing-candidate failure replaced last-valid");

    Fixture fixture;
    auto stale_generation_identity = provider.identity();
    stale_generation_identity = SandboxPackagePublicationIdentity{
        stale_generation_identity.generation() - 1U,
        stale_generation_identity.checksum(),
    };
    auto before = snapshot(provider);
    auto stale_generation = provider.prepare(
        stale_generation_identity, compile_candidate(fixture)
    );
    expect(stale_generation.status() == SandboxPackagePrepareStatus::stale_generation &&
               stale_generation.prepared_update() == nullptr,
           "stale generation prepared a token");
    expect_preserved(provider, before, "stale generation changed last-valid");

    auto wrong_checksum = provider.identity().checksum();
    wrong_checksum.front() ^= 0xffU;
    before = snapshot(provider);
    auto stale_checksum = provider.prepare(
        SandboxPackagePublicationIdentity{provider.identity().generation(), wrong_checksum},
        compile_candidate(fixture)
    );
    expect(stale_checksum.status() == SandboxPackagePrepareStatus::stale_checksum &&
               stale_checksum.prepared_update() == nullptr,
           "stale checksum prepared a token");
    expect_preserved(provider, before, "stale checksum changed last-valid");

    before = snapshot(provider);
    auto missing_again = provider.prepare(provider.identity(), nullptr);
    expect(missing_again.status() == SandboxPackagePrepareStatus::missing_candidate,
           "explicit missing candidate was not rejected");
    expect_preserved(provider, before, "missing candidate changed last-valid");
}

void check_token_exclusivity_and_failure_preservation() {
    Fixture fixture;
    SandboxPackageProvider provider;
    auto first = prepare_candidate(provider, provider.identity(), compile_candidate(fixture));
    auto second = prepare_candidate(provider, provider.identity(), compile_candidate(fixture));
    expect(provider.commit(std::move(*first)).status() ==
               SandboxPackageCommitStatus::committed,
           "first competing token failed");
    auto before = snapshot(provider);
    expect(provider.commit(std::move(*second)).status() ==
               SandboxPackageCommitStatus::stale_generation,
           "second same-generation token did not fail stale");
    expect_preserved(provider, before, "stale competing token changed last-valid");

    SandboxPackageProvider origin;
    SandboxPackageProvider foreign;
    auto foreign_token =
        prepare_candidate(origin, origin.identity(), compile_candidate(fixture));
    const auto origin_before = snapshot(origin);
    const auto foreign_before = snapshot(foreign);
    expect(foreign.commit(std::move(*foreign_token)).status() ==
               SandboxPackageCommitStatus::foreign_provider,
           "foreign provider accepted a prepared token");
    expect_preserved(origin, origin_before, "foreign commit changed origin provider");
    expect_preserved(foreign, foreign_before, "foreign commit changed target provider");
    expect(origin.commit(std::move(*foreign_token)).status() ==
               SandboxPackageCommitStatus::invalid_prepared_update,
           "foreign-consumed token was reusable");
    expect_preserved(origin, origin_before, "reused foreign token changed origin");

    auto movable = prepare_candidate(origin, origin.identity(), compile_candidate(fixture));
    SandboxPackagePreparedUpdate moved{std::move(*movable)};
    const auto moved_before = snapshot(origin);
    expect(origin.commit(std::move(*movable)).status() ==
               SandboxPackageCommitStatus::invalid_prepared_update,
           "moved-from token was accepted");
    expect_preserved(origin, moved_before, "moved-from token changed provider");
    expect(origin.commit(std::move(moved)).status() ==
               SandboxPackageCommitStatus::committed,
           "moved-to token could not commit");
}

void check_same_storage_provider_lifetime_rejected() {
    alignas(SandboxPackageProvider)
        std::array<std::byte, sizeof(SandboxPackageProvider)> storage{};
    auto* const provider_address =
        reinterpret_cast<SandboxPackageProvider*>(storage.data());

    auto* old_provider = std::construct_at(provider_address);
    Fixture fixture;
    auto old_lifetime_token = prepare_candidate(
        *old_provider, old_provider->identity(), compile_candidate(fixture)
    );
    std::destroy_at(old_provider);

    auto* new_provider = std::construct_at(provider_address);
    expect(new_provider == old_provider,
           "provider was not reconstructed at the identical address");
    const auto before = snapshot(*new_provider);
    expect(new_provider->commit(std::move(*old_lifetime_token)).status() ==
               SandboxPackageCommitStatus::foreign_provider,
           "same-address provider accepted a token from an old lifetime");
    expect_preserved(*new_provider, before,
                     "cross-lifetime token changed the reconstructed provider");
    std::destroy_at(new_provider);
}

void check_generation_and_raw_status_boundaries() {
    constexpr auto first = sandbox_next_package_generation(0);
    static_assert(first.status() == SandboxPackageGenerationAdvanceStatus::advanced &&
                  first.generation() == 1);
    constexpr auto exhausted = sandbox_next_package_generation(
        std::numeric_limits<std::uint32_t>::max()
    );
    static_assert(exhausted.status() == SandboxPackageGenerationAdvanceStatus::exhausted &&
                  exhausted.generation() == 0);

    for (const auto raw : {std::uint8_t{0}, std::uint8_t{6}, std::uint8_t{254},
                           std::uint8_t{255}}) {
        expect(!sandbox_package_prepare_status_valid(
                   static_cast<SandboxPackagePrepareStatus>(raw)
               ),
               "unknown prepare status was accepted");
        expect(!sandbox_package_commit_status_valid(
                   static_cast<SandboxPackageCommitStatus>(raw)
               ),
               "unknown commit status was accepted");
    }
    for (const auto raw : {std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{254},
                           std::uint8_t{255}}) {
        expect(!sandbox_package_generation_advance_status_valid(
                   static_cast<SandboxPackageGenerationAdvanceStatus>(raw)
               ),
               "unknown generation status was accepted");
    }
}

}  // namespace

int main() {
    check_initial_prepare_commit_and_lifetime();
    check_determinism_and_same_checksum_republish();
    check_compile_and_prepare_failures_preserve();
    check_token_exclusivity_and_failure_preservation();
    check_same_storage_provider_lifetime_rejected();
    check_generation_and_raw_status_boundaries();
    return EXIT_SUCCESS;
}
