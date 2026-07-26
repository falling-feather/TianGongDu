#include <tgd/content/sandbox_package_compiler.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using namespace tgd::contracts;
using namespace tgd::content;

static_assert(tgd::contracts::sandbox_authoring_schema_major == 1);
static_assert(tgd::contracts::sandbox_authoring_schema_minor == 2);
static_assert(tgd::contracts::sandbox_authoring_schema_patch == 0);
static_assert(static_cast<std::uint8_t>(SandboxPackageCompileStatus::succeeded) == 1);
static_assert(static_cast<std::uint8_t>(SandboxPackageCompileStatus::producer_rejected) == 2);
static_assert(static_cast<std::uint8_t>(SandboxPackageCompileStatus::decoder_rejected) == 3);
static_assert(static_cast<std::uint8_t>(SandboxPackageCompileStatus::fingerprint_mismatch) == 4);
static_assert(static_cast<std::uint8_t>(SandboxPackageCompileStatus::invalid) == 255);
static_assert(!std::is_copy_constructible_v<SandboxPackageCandidate>);
static_assert(!std::is_copy_assignable_v<SandboxPackageCandidate>);
static_assert(std::is_move_constructible_v<SandboxPackageCandidate>);
static_assert(!std::is_copy_constructible_v<SandboxPackageCompileResult>);
static_assert(std::is_move_constructible_v<SandboxPackageCompileResult>);

[[noreturn]] void fail(std::string_view message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

struct Fixture final {
    std::string package_name{"sandbox.package.compiler"};
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
        {"sandbox.actor", "sandbox.region.main", "asset.actor", {1'000, 0, 0, 0}, 180'000},
    }};
    std::array<SandboxAuthoringGroundBlocker, 1> blockers{{
        {"sandbox.blocker", "sandbox.region.main", "asset.blocker",
         400, 600, -100, 100, 0, 500, 0},
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
        {"sandbox.interaction", SandboxInteractionOperation::operate,
         500, "sandbox.mechanism"},
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
            "sandbox.compiler",
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
};

bool has_diagnostic(
    const SandboxPackageValidation& validation,
    SandboxDiagnosticCode code
) {
    return std::any_of(
        validation.diagnostics.begin(),
        validation.diagnostics.end(),
        [code](const auto& diagnostic) { return diagnostic.code == code; }
    );
}

void expect_no_candidate(
    const SandboxPackageCompileResult& result,
    SandboxPackageError error,
    std::string_view message
) {
    expect(!result.succeeded(), message);
    expect(result.status() == SandboxPackageCompileStatus::producer_rejected, message);
    expect(result.validation().error == error, message);
    expect(result.candidate() == nullptr, "failed compile exposed a partial candidate");
}

void check_success_and_canonical_bytes() {
    Fixture fixture;
    auto first = compile_sandbox_package(fixture.runtime);
    expect(first.succeeded(), "valid authoring runtime was rejected");
    expect(first.validation().valid(), "successful compile retained invalid validation");
    expect(first.candidate() != nullptr && !first.candidate()->bytes().empty(),
           "successful compile did not own canonical bytes");
    expect(first.candidate()->document().definition().package_id.name == fixture.package_name,
           "compiled document package id mismatch");
    expect(first.candidate()->document().definition().package_id.key ==
               stable_content_key(fixture.package_name),
           "Stable key was not generated from the authored C++ name");
    expect(first.candidate()->fingerprint() ==
               first.candidate()->document().fingerprint(),
           "candidate fingerprint has a second truth");

    const auto decoded = decode_sandbox_package(first.candidate()->bytes());
    expect(decoded.validation.valid() && decoded.document != nullptr,
           "candidate bytes did not decode through the existing decoder");
    expect(decoded.document->fingerprint() == first.candidate()->fingerprint(),
           "producer and decoded fingerprints diverged");

    const std::vector<std::uint8_t> canonical{
        first.candidate()->bytes().begin(), first.candidate()->bytes().end()
    };
    std::reverse(fixture.assets.begin(), fixture.assets.end());
    fixture.rebind();
    const auto permuted = compile_sandbox_package(fixture.runtime);
    expect(permuted.succeeded(), "permuted valid authoring runtime was rejected");
    expect(std::equal(
               canonical.begin(), canonical.end(),
               permuted.candidate()->bytes().begin(), permuted.candidate()->bytes().end()
           ),
           "legal authoring permutation changed canonical bytes");
    expect(first.candidate()->fingerprint() == permuted.candidate()->fingerprint(),
           "legal authoring permutation changed fingerprint");
}

void check_owned_lifetime() {
    std::unique_ptr<SandboxPackageCandidate> candidate;
    {
        Fixture fixture;
        fixture.package_name = "sandbox.package.dynamic-owned";
        fixture.rebind();
        auto compiled = compile_sandbox_package(fixture.runtime);
        expect(compiled.succeeded(), "dynamic authoring input was rejected");
        auto relocated = std::move(compiled);
        expect(compiled.status() == SandboxPackageCompileStatus::invalid &&
                   !compiled.succeeded() && compiled.candidate() == nullptr,
               "moved-from result retained a contradictory success state");
        candidate = std::move(relocated).take_candidate();
        expect(candidate != nullptr, "successful result did not release candidate");
        expect(relocated.status() == SandboxPackageCompileStatus::invalid &&
                   !relocated.succeeded() && relocated.candidate() == nullptr,
               "consumed result retained a contradictory success state");
        fixture.package_name.assign("destroyed-input-storage");
    }
    expect(candidate->document().definition().package_id.name ==
               "sandbox.package.dynamic-owned",
           "document retained the authoring string lifetime");
    expect(candidate->document().definition().regions.front().bounds.min_x == -5'000,
           "document retained the authoring array lifetime");
    expect(candidate->document().gameplay_binding().interaction_bindings.front().range_mm ==
               500,
           "document retained the authoring binding lifetime");
}

void check_id_and_utf8_failures() {
    Fixture fixture;
    fixture.package_name.clear();
    fixture.rebind();
    auto result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "empty authoring id was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::invalid_stable_id),
           "empty authoring id lost its existing diagnostic");

    fixture.package_name.assign("\xc3\x28", 2);
    fixture.rebind();
    result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "invalid UTF-8 authoring id was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::invalid_stable_id),
           "invalid UTF-8 lost its existing diagnostic");
}

void check_capacity_and_enum_failures() {
    Fixture fixture;
    std::vector<SandboxAuthoringAsset> oversized(
        sandbox_asset_capacity + 1,
        {"asset.player", SandboxAssetKind::player}
    );
    fixture.runtime.assets = oversized;
    auto result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "over-capacity authoring array was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::capacity_exceeded),
           "capacity failure lost its existing diagnostic");

    fixture.rebind();
    fixture.assets.front().kind = static_cast<SandboxAssetKind>(0);
    fixture.rebind();
    result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "raw zero asset enum was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::invalid_stable_id) ||
               has_diagnostic(result.validation(), SandboxDiagnosticCode::asset_kind_mismatch),
           "bad enum did not retain package diagnostics");

    fixture.assets.front().kind = static_cast<SandboxAssetKind>(255);
    fixture.rebind();
    result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "raw 255 asset enum was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::asset_kind_mismatch),
           "raw 255 enum lost its existing diagnostic");
}

void check_reference_and_graph_failures() {
    Fixture fixture;
    fixture.runtime.player.region_id = "sandbox.region.missing";
    auto result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "missing authoring reference was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::missing_region),
           "missing reference lost its existing diagnostic");

    fixture.rebind();
    fixture.objectives.front().predecessor_objective_id = "sandbox.objective";
    fixture.rebind();
    result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::semantic_validation_failed,
                        "authoring graph cycle was accepted");
    expect(has_diagnostic(result.validation(), SandboxDiagnosticCode::dependency_cycle),
           "graph cycle lost its existing diagnostic");
}

void check_binding_failure() {
    Fixture fixture;
    fixture.interaction_bindings.front().range_mm = 499;
    fixture.rebind();
    const auto result = compile_sandbox_package(fixture.runtime);
    expect_no_candidate(result, SandboxPackageError::gameplay_binding_validation_failed,
                        "invalid gameplay binding was accepted");
    expect(result.validation().gameplay_binding_validation.code ==
               SandboxGameplayBindingValidationCode::invalid_operate_range,
           "binding failure did not preserve the existing validation result");

    for (const auto raw : {std::uint8_t{0}, std::uint8_t{255}}) {
        fixture.interaction_bindings.front().range_mm = 500;
        fixture.interaction_bindings.front().operation =
            static_cast<SandboxInteractionOperation>(raw);
        fixture.rebind();
        const auto enum_result = compile_sandbox_package(fixture.runtime);
        expect_no_candidate(enum_result,
                            SandboxPackageError::gameplay_binding_validation_failed,
                            "invalid binding operation enum was accepted");
        expect(enum_result.validation().gameplay_binding_validation.code ==
                   SandboxGameplayBindingValidationCode::invalid_interaction_operation,
               "binding enum failure lost its existing validation result");
    }
}

}  // namespace

int main() {
    check_success_and_canonical_bytes();
    check_owned_lifetime();
    check_id_and_utf8_failures();
    check_capacity_and_enum_failures();
    check_reference_and_graph_failures();
    check_binding_failure();
    return EXIT_SUCCESS;
}
