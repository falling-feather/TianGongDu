#include <tgd/content/sandbox_package_service_abi.h>

#include <tgd/content/sandbox_package.hpp>

#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_gameplay_binding.hpp>
#include <tgd/contracts/sandbox_pack.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace tgd::contracts;

static_assert(sizeof(tgd_sandbox_service_identity) == 36);
static_assert(sizeof(tgd_sandbox_service_bounds) == 28);
static_assert(sizeof(tgd_sandbox_service_pose) == 16);
static_assert(sizeof(tgd_sandbox_service_metadata) == 40);
static_assert(sizeof(tgd_sandbox_service_player) == 36);
static_assert(sizeof(tgd_sandbox_service_region) == 32);
static_assert(sizeof(tgd_sandbox_service_asset) == 8);
static_assert(sizeof(tgd_sandbox_service_placement) == 32);
static_assert(sizeof(tgd_sandbox_service_ground_blocker) == 40);
static_assert(sizeof(tgd_sandbox_service_wave) == 20);
static_assert(sizeof(tgd_sandbox_service_wave_spawn) == 16);
static_assert(sizeof(tgd_sandbox_service_objective) == 20);
static_assert(sizeof(tgd_sandbox_service_interaction_binding) == 16);
static_assert(sizeof(tgd_sandbox_service_mechanism_binding) == 12);
static_assert(sizeof(tgd_sandbox_service_actor_binding) == 16);
static_assert(sizeof(tgd_sandbox_service_result_header) == 120);
static_assert(sizeof(tgd_sandbox_service_result_artifact) == 16);
static_assert(sizeof(tgd_sandbox_service_diagnostic) == 48);
static_assert(offsetof(tgd_sandbox_service_result_header, checksum) == 12);
static_assert(offsetof(tgd_sandbox_service_result_header, diagnostics_offset) == 68);
static_assert(offsetof(tgd_sandbox_service_result_header, total_bytes) == 76);
static_assert(offsetof(tgd_sandbox_service_result_header, binding_flags) == 84);
static_assert(offsetof(tgd_sandbox_service_result_header, binding_subject_id_offset) == 88);
static_assert(offsetof(tgd_sandbox_service_result_artifact, package_bytes_offset) == 0);
static_assert(offsetof(tgd_sandbox_service_result_artifact, package_bytes_length) == 4);
static_assert(offsetof(tgd_sandbox_service_result_artifact, reserved) == 8);
static_assert(offsetof(tgd_sandbox_service_diagnostic, subject_key_low) == 12);
static_assert(offsetof(tgd_sandbox_service_diagnostic, subject_id_offset) == 28);
static_assert(offsetof(tgd_sandbox_service_diagnostic, reserved) == 44);
static_assert(TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED == 1);
static_assert(TGD_SANDBOX_SERVICE_TRANSPORT_INVALID == 255);
static_assert(TGD_SANDBOX_SERVICE_PUBLISHED == 1);
static_assert(TGD_SANDBOX_SERVICE_PUBLISH_INVALID == 255);
static_assert(TGD_SANDBOX_COMPILER_SERVICE_MAX_STRING_REFS == 4096);
static_assert(TGD_SANDBOX_COMPILER_SERVICE_MAX_CANONICAL_PACKAGE_BYTES ==
              sandbox_pack_max_bytes);
static_assert(TGD_SANDBOX_COMPILER_SERVICE_MAX_RESULT_BYTES ==
              sandbox_pack_max_bytes +
                  TGD_SANDBOX_COMPILER_SERVICE_RESULT_PREFIX_BYTES);

[[noreturn]] void fail(std::string_view message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

template <typename Value>
[[nodiscard]] Value read_pod(std::span<const std::uint8_t> bytes, std::size_t offset) {
    expect(offset + sizeof(Value) <= bytes.size(), "result POD was truncated");
    Value value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

struct Result final {
    tgd_sandbox_service_result_header header{};
    tgd_sandbox_service_result_artifact artifact{};
    std::vector<std::uint8_t> bytes{};

    [[nodiscard]] tgd_sandbox_service_diagnostic diagnostic(std::size_t index) const {
        expect(index < header.diagnostic_count, "diagnostic index out of range");
        return read_pod<tgd_sandbox_service_diagnostic>(
            bytes, header.diagnostics_offset + index * sizeof(tgd_sandbox_service_diagnostic)
        );
    }

    [[nodiscard]] std::string_view id_bytes(std::uint32_t offset, std::uint32_t length) const {
        expect(static_cast<std::size_t>(offset) + length <= bytes.size(),
               "diagnostic Stable ID bytes were truncated");
        return {reinterpret_cast<const char*>(bytes.data() + offset), length};
    }

    [[nodiscard]] std::span<const std::uint8_t> package_bytes() const {
        expect(static_cast<std::size_t>(artifact.package_bytes_offset) +
                   artifact.package_bytes_length <= bytes.size(),
               "canonical package bytes were truncated");
        return {bytes.data() + artifact.package_bytes_offset,
                artifact.package_bytes_length};
    }
};

void expect_no_package(const Result& result, std::string_view message) {
    expect(result.artifact.package_bytes_offset == 0 &&
               result.artifact.package_bytes_length == 0,
           message);
}

[[nodiscard]] tgd_sandbox_service_identity identity(tgd_sandbox_service_handle service) {
    tgd_sandbox_service_identity value{};
    expect(tgd_sandbox_compiler_service_read_identity(service, &value) ==
               TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
           "service identity read failed");
    return value;
}

[[nodiscard]] bool zero_checksum(const tgd_sandbox_service_identity& value) {
    return std::all_of(std::begin(value.checksum), std::end(value.checksum),
                       [](std::uint8_t byte) { return byte == 0; });
}

[[nodiscard]] tgd_sandbox_request_string_ref copy_string(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    std::string_view value
) {
    tgd_sandbox_request_string_ref ref{};
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, request,
               reinterpret_cast<const std::uint8_t*>(value.data()),
               static_cast<std::uint32_t>(value.size()), &ref
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
           "request string copy failed");
    return ref;
}

struct Builder final {
    tgd_sandbox_service_handle service{};
    tgd_sandbox_request_handle request{};

    Builder(tgd_sandbox_service_handle owner, tgd_sandbox_service_identity expected)
        : service(owner) {
        expect(tgd_sandbox_compile_request_create(service, &expected, &request) ==
                   TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
               "request create failed");
    }

    [[nodiscard]] tgd_sandbox_request_string_ref text(std::string_view value) const {
        return copy_string(service, request, value);
    }

    void populate(
        bool reverse_assets = false,
        std::string_view player_region = "sandbox.region.main",
        std::uint8_t interaction_operation =
            static_cast<std::uint8_t>(SandboxInteractionOperation::operate),
        std::int32_t interaction_range = 500,
        std::string_view interaction_target = "sandbox.mechanism",
        std::string_view package_id = "sandbox.package.service"
    ) const {
        const auto empty = text("");
        const auto package = text(package_id);
        const auto sandbox = text("sandbox.service");
        const auto region = text("sandbox.region.main");
        const auto missing_or_region = text(player_region);
        const auto actor = text("sandbox.actor");
        const auto blocker = text("sandbox.blocker");
        const auto safe = text("sandbox.safe");
        const auto interaction = text("sandbox.interaction");
        const auto mechanism = text("sandbox.mechanism");
        const auto wave = text("sandbox.wave");
        const auto objective = text("sandbox.objective");
        const auto player_asset = text("asset.player");
        const auto actor_asset = text("asset.actor");
        const auto blocker_asset = text("asset.blocker");
        const auto safe_asset = text("asset.safe");
        const auto interaction_asset = text("asset.interaction");
        const auto mechanism_asset = text("asset.mechanism");

        const tgd_sandbox_service_metadata metadata{
            package, sandbox, objective,
            {-10'000, 10'000, -10'000, 10'000, -1'000, 4'000, 0, 1},
        };
        const tgd_sandbox_service_player player{
            text("sandbox.player"), missing_or_region, player_asset, safe,
            {0, 0, 0, 0, 0}, 0,
        };
        expect(tgd_sandbox_compile_request_set_metadata(service, request, &metadata) == 1,
               "metadata set failed");
        expect(tgd_sandbox_compile_request_set_player(service, request, &player) == 1,
               "player set failed");

        const tgd_sandbox_service_region region_record{
            region, {-5'000, 5'000, -5'000, 5'000, -500, 2'000, 0, 0},
        };
        expect(tgd_sandbox_compile_request_append_region(service, request, &region_record) == 1,
               "region append failed");
        std::array<tgd_sandbox_service_asset, 6> assets{{
            {player_asset, static_cast<std::uint8_t>(SandboxAssetKind::player), {0, 0, 0}},
            {actor_asset, static_cast<std::uint8_t>(SandboxAssetKind::actor), {0, 0, 0}},
            {blocker_asset, static_cast<std::uint8_t>(SandboxAssetKind::obstacle), {0, 0, 0}},
            {safe_asset, static_cast<std::uint8_t>(SandboxAssetKind::safe_point), {0, 0, 0}},
            {interaction_asset, static_cast<std::uint8_t>(SandboxAssetKind::interaction), {0, 0, 0}},
            {mechanism_asset, static_cast<std::uint8_t>(SandboxAssetKind::mechanism), {0, 0, 0}},
        }};
        if (reverse_assets) std::reverse(assets.begin(), assets.end());
        for (const auto& value : assets)
            expect(tgd_sandbox_compile_request_append_asset(service, request, &value) == 1,
                   "asset append failed");
        const tgd_sandbox_service_placement actor_record{
            actor, region, actor_asset, {1'000, 0, 0, 0, 0}, 180'000,
        };
        const tgd_sandbox_service_ground_blocker blocker_record{
            blocker, region, blocker_asset, 400, 600, -100, 100, 0, 500, 0, 0,
        };
        const tgd_sandbox_service_placement safe_record{
            safe, region, safe_asset, {-1'000, 0, 0, 0, 0}, 0,
        };
        const tgd_sandbox_service_placement interaction_record{
            interaction, region, interaction_asset, {0, 500, 0, 0, 0}, 90'000,
        };
        const tgd_sandbox_service_placement mechanism_record{
            mechanism, region, mechanism_asset, {0, 700, 100, 0, 0}, 90'000,
        };
        expect(tgd_sandbox_compile_request_append_actor(service, request, &actor_record) == 1,
               "actor append failed");
        expect(tgd_sandbox_compile_request_append_ground_blocker(service, request, &blocker_record) == 1,
               "blocker append failed");
        expect(tgd_sandbox_compile_request_append_safe_point(service, request, &safe_record) == 1,
               "safe point append failed");
        expect(tgd_sandbox_compile_request_append_interaction(service, request, &interaction_record) == 1,
               "interaction append failed");
        expect(tgd_sandbox_compile_request_append_mechanism(service, request, &mechanism_record) == 1,
               "mechanism append failed");
        const tgd_sandbox_service_wave wave_record{
            wave, region, empty, empty,
            static_cast<std::uint8_t>(SandboxTriggerKind::session_started), {0, 0, 0},
        };
        const tgd_sandbox_service_wave_spawn spawn_record{wave, actor, 10, 1, 0};
        const tgd_sandbox_service_objective objective_record{
            objective, region, empty, wave,
            static_cast<std::uint8_t>(SandboxObjectiveCompletionKind::wave_completed), {0, 0, 0},
        };
        expect(tgd_sandbox_compile_request_append_wave(service, request, &wave_record) == 1,
               "wave append failed");
        expect(tgd_sandbox_compile_request_append_wave_spawn(service, request, &spawn_record) == 1,
               "wave spawn append failed");
        expect(tgd_sandbox_compile_request_append_objective(service, request, &objective_record) == 1,
               "objective append failed");
        const tgd_sandbox_service_interaction_binding interaction_binding{
            interaction, text(interaction_target), interaction_range,
            interaction_operation, {0, 0, 0},
        };
        const tgd_sandbox_service_mechanism_binding mechanism_binding{
            mechanism, blocker,
            static_cast<std::uint8_t>(SandboxMechanismActivation::one_shot_activate), {0, 0, 0},
        };
        const tgd_sandbox_service_actor_binding actor_binding{
            actor, text("profile.actor"), 50,
            static_cast<std::uint8_t>(CombatFaction::hostile),
            static_cast<std::uint8_t>(EncounterTacticalDuty::pressure), {0, 0},
        };
        expect(tgd_sandbox_compile_request_append_interaction_binding(
                   service, request, &interaction_binding) == 1,
               "interaction binding append failed");
        expect(tgd_sandbox_compile_request_append_mechanism_binding(
                   service, request, &mechanism_binding) == 1,
               "mechanism binding append failed");
        expect(tgd_sandbox_compile_request_append_actor_binding(
                   service, request, &actor_binding) == 1,
               "actor binding append failed");
    }

    [[nodiscard]] Result submit(std::uint32_t capacity = TGD_SANDBOX_COMPILER_SERVICE_MAX_RESULT_BYTES) const {
        Result result;
        result.bytes.resize(capacity);
        std::uint32_t written{};
        const auto status = tgd_sandbox_compile_request_submit(
            service, request, result.bytes.data(), capacity, &written
        );
        expect(status == TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED, "request submit failed");
        result.bytes.resize(written);
        result.header = read_pod<tgd_sandbox_service_result_header>(result.bytes, 0);
        result.artifact = read_pod<tgd_sandbox_service_result_artifact>(
            result.bytes, TGD_SANDBOX_COMPILER_SERVICE_RESULT_HEADER_BYTES
        );
        expect(result.header.complete == 1 && result.header.total_bytes == written,
               "service returned an incomplete result");
        expect(result.header.abi_major == 1 && result.header.abi_minor == 2 &&
                   result.header.diagnostics_offset ==
                       TGD_SANDBOX_COMPILER_SERVICE_RESULT_PREFIX_BYTES,
               "service returned a non-1.2 result prefix");
        expect(result.artifact.reserved[0] == 0 && result.artifact.reserved[1] == 0,
               "service returned non-zero artifact reserved bytes");
        expect(result.header.diagnostic_count * sizeof(tgd_sandbox_service_diagnostic) +
                   result.header.diagnostics_offset <= result.header.id_bytes_offset,
               "diagnostic count does not fit the result");
        return result;
    }
};

void check_publish_and_determinism() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    const auto empty = identity(service);
    expect(empty.generation == 0 && zero_checksum(empty), "initial identity is not empty");

    Builder first{service, empty};
    first.populate();
    const auto published = first.submit();
    expect(published.header.outcome == TGD_SANDBOX_SERVICE_PUBLISHED &&
               published.header.generation == 1 && published.header.diagnostic_count == 0,
           "first package was not published");
    expect(published.artifact.package_bytes_length != 0,
           "published result omitted canonical package bytes");
    const auto decoded = tgd::content::decode_sandbox_package(published.package_bytes());
    expect(decoded.validation.valid() && decoded.document != nullptr &&
               std::equal(decoded.document->fingerprint().begin(),
                          decoded.document->fingerprint().end(),
                          std::begin(published.header.checksum)),
           "published package did not decode to its publication checksum");
    expect(decoded.document->gameplay_binding().actor_bindings.size() == 1 &&
               decoded.document->gameplay_binding().actor_bindings.front().profile_id.name ==
                   "profile.actor",
           "published package lost the actor gameplay binding");
    const auto generation_one = identity(service);
    expect(generation_one.generation == 1 &&
               std::equal(std::begin(generation_one.checksum), std::end(generation_one.checksum),
                          std::begin(published.header.checksum)),
           "published identity does not match result");

    Builder second{service, generation_one};
    second.populate(true);
    const auto republished = second.submit();
    expect(republished.header.outcome == TGD_SANDBOX_SERVICE_PUBLISHED &&
               republished.header.generation == 2 &&
               std::equal(std::begin(republished.header.checksum),
                          std::end(republished.header.checksum),
                          std::begin(published.header.checksum)),
           "equivalent reordered package changed its checksum");
    expect(std::ranges::equal(republished.package_bytes(), published.package_bytes()),
           "same-checksum republish changed canonical package bytes");
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
}

void check_diagnostic_fidelity_and_preservation() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    const auto before = identity(service);
    const std::string missing{"sandbox.region.missing"};
    Builder invalid{service, before};
    invalid.populate(false, missing);
    const auto rejected = invalid.submit();
    expect(rejected.header.outcome == TGD_SANDBOX_SERVICE_COMPILER_REJECTED &&
               rejected.header.diagnostic_count != 0,
           "semantic failure did not return diagnostics");
    expect_no_package(rejected, "semantic failure exposed package bytes");
    bool found = false;
    for (std::size_t index = 0; index < rejected.header.diagnostic_count; ++index) {
        const auto diagnostic = rejected.diagnostic(index);
        expect(diagnostic.severity != 0 && diagnostic.section != 255 &&
                   diagnostic.field != 65'535 && diagnostic.reserved == 0,
               "raw diagnostic locator was not preserved");
        if ((diagnostic.flags & TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_RELATED_ID) != 0 &&
            rejected.id_bytes(diagnostic.related_id_offset,
                              diagnostic.related_id_length) == missing) {
            found = true;
        }
    }
    expect(found, "directly resolvable related Stable ID bytes were lost");
    const auto after = identity(service);
    expect(std::memcmp(&before, &after, sizeof(before)) == 0,
           "compiler failure changed publication identity");
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
}

void check_binding_diagnostic_fidelity() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    Builder invalid{service, identity(service)};
    invalid.populate(false, "sandbox.region.main",
                     static_cast<std::uint8_t>(SandboxInteractionOperation::operate), 499);
    const auto rejected = invalid.submit();
    expect(rejected.header.outcome == TGD_SANDBOX_SERVICE_COMPILER_REJECTED &&
               rejected.header.binding_code == static_cast<std::uint8_t>(
                   SandboxGameplayBindingValidationCode::invalid_operate_range),
           "binding diagnostic code was not preserved");
    expect_no_package(rejected, "binding failure exposed package bytes");
    expect((rejected.header.binding_flags &
               TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_SUBJECT_ID) != 0,
           "binding diagnostic subject Stable ID was not resolved");
    expect(rejected.id_bytes(rejected.header.binding_subject_id_offset,
                             rejected.header.binding_subject_id_length) ==
               "sandbox.interaction",
           "binding subject Stable ID bytes drifted");
    const auto interaction_key = stable_content_key("sandbox.interaction");
    expect(rejected.header.binding_subject_key_low ==
               static_cast<std::uint32_t>(interaction_key) &&
               rejected.header.binding_subject_key_high ==
               static_cast<std::uint32_t>(interaction_key >> 32U),
           "binding diagnostic 64-bit key was truncated");
    Builder dangling{service, identity(service)};
    dangling.populate(false, "sandbox.region.main",
                      static_cast<std::uint8_t>(SandboxInteractionOperation::operate),
                      500, "sandbox.mechanism.missing");
    const auto dangling_rejected = dangling.submit();
    expect((dangling_rejected.header.binding_flags &
               TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_RELATED_ID) != 0 &&
               dangling_rejected.id_bytes(
                   dangling_rejected.header.binding_related_id_offset,
                   dangling_rejected.header.binding_related_id_length
               ) == "sandbox.mechanism.missing",
           "binding diagnostic related Stable ID bytes were not preserved");
    const auto related_key = stable_content_key("sandbox.mechanism.missing");
    expect(dangling_rejected.header.binding_related_key_low ==
               static_cast<std::uint32_t>(related_key) &&
               dangling_rejected.header.binding_related_key_high ==
               static_cast<std::uint32_t>(related_key >> 32U),
           "binding related 64-bit key was truncated");
    const auto before = identity(service);
    for (const auto raw : {std::uint8_t{0}, std::uint8_t{255}}) {
        Builder bad_enum{service, before};
        bad_enum.populate(false, "sandbox.region.main", raw, 500);
        const auto enum_rejected = bad_enum.submit();
        expect(enum_rejected.header.outcome == TGD_SANDBOX_SERVICE_COMPILER_REJECTED &&
                   enum_rejected.header.binding_code == static_cast<std::uint8_t>(
                       SandboxGameplayBindingValidationCode::invalid_interaction_operation),
               "raw binding enum did not fail through the sole validator");
        const auto after = identity(service);
        expect(std::memcmp(&before, &after, sizeof(before)) == 0,
               "binding failure changed publication identity");
    }
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
}

void check_stale_competing_and_output_failure() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    const auto empty = identity(service);
    Builder first{service, empty}; first.populate();
    Builder competing{service, empty}; competing.populate();
    const auto published = first.submit();
    const auto stable = identity(service);
    const auto stale = competing.submit();
    expect(published.header.outcome == TGD_SANDBOX_SERVICE_PUBLISHED &&
               stale.header.outcome == TGD_SANDBOX_SERVICE_STALE_GENERATION,
           "competing request did not fail stale");
    expect_no_package(stale, "stale request exposed package bytes");
    const auto after_stale = identity(service);
    expect(std::memcmp(&stable, &after_stale, sizeof(stable)) == 0,
           "stale request changed publication");

    Builder undersized{service, stable}; undersized.populate();
    std::vector<std::uint8_t> tiny(published.bytes.size() - 1U, UINT8_C(0xa5));
    std::uint32_t required{};
    expect(tgd_sandbox_compile_request_submit(
               service, undersized.request, tiny.data(),
               static_cast<std::uint32_t>(tiny.size()), &required
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_OUTPUT_TOO_SMALL &&
               required == published.bytes.size() &&
               std::ranges::all_of(tiny, [](std::uint8_t byte) { return byte == 0xa5; }),
           "undersized output did not fail closed");
    expect(tgd_sandbox_compile_request_submit(
               service, undersized.request, tiny.data(),
               static_cast<std::uint32_t>(tiny.size()), &required
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_REQUEST,
           "failed submit did not consume request");
    const auto after_undersized = identity(service);
    expect(std::memcmp(&stable, &after_undersized, sizeof(stable)) == 0,
           "output truncation changed publication");
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
}

void check_foreign_and_same_storage_lifetime() {
    tgd_sandbox_service_handle first{};
    tgd_sandbox_service_handle second{};
    expect(tgd_sandbox_compiler_service_create(&first) == 1, "first service create failed");
    expect(tgd_sandbox_compiler_service_create(&second) == 1, "second service create failed");
    Builder foreign{first, identity(first)}; foreign.populate();
    std::array<std::uint8_t, 128> output{};
    std::uint32_t written{};
    expect(tgd_sandbox_compile_request_submit(
               second, foreign.request, output.data(),
               static_cast<std::uint32_t>(output.size()), &written
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_FOREIGN_REQUEST,
           "foreign request was accepted");
    expect(tgd_sandbox_compile_request_submit(
               first, foreign.request, output.data(),
               static_cast<std::uint32_t>(output.size()), &written
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_REQUEST,
           "foreign submit did not consume the one-shot request");
    expect(tgd_sandbox_compiler_service_destroy(first) == 1, "first destroy failed");
    tgd_sandbox_service_handle replacement{};
    expect(tgd_sandbox_compiler_service_create(&replacement) == 1,
           "same-slot replacement create failed");
    tgd_sandbox_service_identity ignored{};
    expect(tgd_sandbox_compiler_service_read_identity(first, &ignored) ==
               TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE,
           "old same-storage service handle survived reconstruction");
    expect(replacement != first && identity(replacement).generation == 0,
           "replacement service inherited old lifetime identity");
    expect(tgd_sandbox_compiler_service_destroy(replacement) == 1, "replacement destroy failed");
    expect(tgd_sandbox_compiler_service_destroy(second) == 1, "second destroy failed");
}

void check_malformed_and_reserved() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    Builder malformed{service, identity(service)};
    const auto id = malformed.text("asset.invalid");
    const tgd_sandbox_service_asset bad_reserved{id, 0, {1, 0, 0}};
    expect(tgd_sandbox_compile_request_append_asset(
               service, malformed.request, &bad_reserved
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST,
           "non-zero reserved bytes were accepted");
    std::vector<tgd_sandbox_service_asset> assets(sandbox_asset_capacity + 1U,
        {id, 0, {0, 0, 0}});
    for (const auto& asset : assets)
        expect(tgd_sandbox_compile_request_append_asset(service, malformed.request, &asset) == 1,
               "capacity+1 transport projection was not admitted to compiler boundary");
    expect(tgd_sandbox_compile_request_append_asset(service, malformed.request, &assets.front()) ==
               TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED,
           "transport hard bound was not enforced");
    expect(tgd_sandbox_compile_request_cancel(service, malformed.request) == 1,
           "request cancel failed");
    expect(tgd_sandbox_compile_request_cancel(service, malformed.request) ==
               TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_REQUEST,
           "cancelled request was reusable");
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
}

void check_copied_string_bounds() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    const auto stable = identity(service);
    const std::uint8_t byte = 'x';

    tgd_sandbox_request_handle huge_length{};
    expect(tgd_sandbox_compile_request_create(service, &stable, &huge_length) == 1,
           "huge-length request create failed");
    tgd_sandbox_request_string_ref ref{};
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, huge_length, &byte, 1, &ref
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
           "initial copied byte was rejected");
    tgd_sandbox_request_string_ref unchanged = UINT32_MAX;
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, huge_length, &byte, UINT32_MAX, &unchanged
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED,
           "UINT32_MAX copied length did not fail before reading input");
    expect(unchanged == UINT32_MAX, "failed huge-length copy changed the output ref");
    expect(tgd_sandbox_compile_request_cancel(service, huge_length) == 1,
           "huge-length request was not safely cancellable");

    tgd_sandbox_request_handle byte_limit{};
    expect(tgd_sandbox_compile_request_create(service, &stable, &byte_limit) == 1,
           "byte-limit request create failed");
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, byte_limit, &byte, 1, &ref
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
           "byte-limit initial byte was rejected");
    std::vector<std::uint8_t> remaining(
        TGD_SANDBOX_COMPILER_SERVICE_MAX_COPIED_UTF8_BYTES - 1U, byte);
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, byte_limit, remaining.data(),
               static_cast<std::uint32_t>(remaining.size()), &ref
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
           "exact remaining copied-byte capacity was rejected");
    unchanged = UINT32_MAX;
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, byte_limit, &byte, 1, &unchanged
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED,
           "copied-byte capacity +1 was accepted");
    expect(unchanged == UINT32_MAX, "byte-capacity failure changed the output ref");
    expect(tgd_sandbox_compile_request_cancel(service, byte_limit) == 1,
           "byte-limit request was not safely cancellable");

    tgd_sandbox_request_handle ref_limit{};
    expect(tgd_sandbox_compile_request_create(service, &stable, &ref_limit) == 1,
           "string-ref limit request create failed");
    for (std::uint32_t index = 0;
         index < TGD_SANDBOX_COMPILER_SERVICE_MAX_STRING_REFS; ++index) {
        expect(tgd_sandbox_compile_request_copy_utf8(
                   service, ref_limit, nullptr, 0, &ref
               ) == TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED,
               "null+zero string ref below the limit was rejected");
        expect(ref == index + 1U, "string refs were not stable one-based values");
    }
    unchanged = UINT32_MAX;
    expect(tgd_sandbox_compile_request_copy_utf8(
               service, ref_limit, nullptr, 0, &unchanged
           ) == TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED,
           "string-ref capacity +1 was accepted");
    expect(unchanged == UINT32_MAX, "string-ref capacity failure changed the output ref");
    expect(tgd_sandbox_compile_request_cancel(service, ref_limit) == 1,
           "string-ref limit request was not safely cancellable");

    const auto after = identity(service);
    expect(std::memcmp(&stable, &after, sizeof(stable)) == 0,
           "copied string boundary failures changed publication identity");
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
}

[[nodiscard]] Result invalid_utf8_result() {
    tgd_sandbox_service_handle service{};
    expect(tgd_sandbox_compiler_service_create(&service) == 1, "service create failed");
    Builder invalid{service, identity(service)};
    const std::string invalid_package{"sandbox.package.\xc3\x28", 18};
    invalid.populate(false, "sandbox.region.main",
                     static_cast<std::uint8_t>(SandboxInteractionOperation::operate),
                     500, "sandbox.mechanism", invalid_package);
    auto result = invalid.submit();
    expect(result.header.outcome == TGD_SANDBOX_SERVICE_COMPILER_REJECTED,
           "invalid UTF-8 did not return a semantic result");
    for (std::size_t index = 0; index < result.header.diagnostic_count; ++index) {
        const auto diagnostic = result.diagnostic(index);
        if ((diagnostic.flags & TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_SUBJECT_ID) != 0) {
            expect(result.id_bytes(diagnostic.subject_id_offset,
                                   diagnostic.subject_id_length) != invalid_package,
                   "invalid UTF-8 was exposed as a subject Stable ID");
        }
        if ((diagnostic.flags & TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_RELATED_ID) != 0) {
            expect(result.id_bytes(diagnostic.related_id_offset,
                                   diagnostic.related_id_length) != invalid_package,
                   "invalid UTF-8 was exposed as a related Stable ID");
        }
    }
    expect(identity(service).generation == 0,
           "invalid UTF-8 semantic failure changed publication");
    expect(tgd_sandbox_compiler_service_destroy(service) == 1, "service destroy failed");
    return result;
}

void check_invalid_utf8_diagnostic_output() {
    const auto result = invalid_utf8_result();
    expect(result.header.total_bytes == result.bytes.size(),
           "invalid UTF-8 result was incomplete");
    expect_no_package(result, "invalid UTF-8 failure exposed package bytes");
}

}  // namespace

extern "C" std::int32_t tgd_sandbox_service_run_contract_probe() {
    expect(tgd_sandbox_compiler_service_abi_version() == 0x0001'0002U,
           "compiler service ABI version mismatch");
    check_publish_and_determinism();
    check_diagnostic_fidelity_and_preservation();
    check_binding_diagnostic_fidelity();
    check_stale_competing_and_output_failure();
    check_foreign_and_same_storage_lifetime();
    check_malformed_and_reserved();
    check_copied_string_bounds();
    check_invalid_utf8_diagnostic_output();
    return 0;
}

extern "C" std::int32_t tgd_sandbox_service_write_invalid_utf8_result(
    std::uint8_t* output,
    std::uint32_t capacity,
    std::uint32_t* written
) {
    if (output == nullptr || written == nullptr) return 1;
    const auto result = invalid_utf8_result();
    *written = static_cast<std::uint32_t>(result.bytes.size());
    if (capacity < result.bytes.size()) return 2;
    std::memcpy(output, result.bytes.data(), result.bytes.size());
    return 0;
}

int main() {
    return tgd_sandbox_service_run_contract_probe();
}
