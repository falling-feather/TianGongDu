#include <tgd/content/sandbox_package.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace tgd::contracts;
using namespace tgd::content;

[[noreturn]] void fail(std::string_view message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

constexpr auto package_id = content_id("sandbox.package.demo");
constexpr auto sandbox_id = content_id("sandbox.demo");
constexpr auto region_id = content_id("sandbox.region.main");
constexpr auto player_id = content_id("sandbox.player");
constexpr auto actor_id = content_id("sandbox.actor");
constexpr auto blocker_id = content_id("sandbox.blocker");
constexpr auto safe_id = content_id("sandbox.safe");
constexpr auto interaction_id = content_id("sandbox.interaction");
constexpr auto mechanism_id = content_id("sandbox.mechanism");
constexpr auto wave_id = content_id("sandbox.wave");
constexpr auto objective_id = content_id("sandbox.objective");
constexpr auto player_asset = content_id("asset.player");
constexpr auto actor_asset = content_id("asset.actor");
constexpr auto blocker_asset = content_id("asset.blocker");
constexpr auto safe_asset = content_id("asset.safe");
constexpr auto interaction_asset = content_id("asset.interaction");
constexpr auto mechanism_asset = content_id("asset.mechanism");

struct Fixture final {
    std::array<SandboxRegionDefinition, 1> regions{{
        {region_id, {-5'000, 5'000, -5'000, 5'000, -500, 2'000, 0, 0}},
    }};
    std::array<SandboxAssetReferenceDefinition, 6> assets{{
        {player_asset, SandboxAssetKind::player},
        {actor_asset, SandboxAssetKind::actor},
        {blocker_asset, SandboxAssetKind::obstacle},
        {safe_asset, SandboxAssetKind::safe_point},
        {interaction_asset, SandboxAssetKind::interaction},
        {mechanism_asset, SandboxAssetKind::mechanism},
    }};
    std::array<SandboxActorDefinition, 1> actors{{
        {actor_id, region_id, actor_asset, {1'000, 0, 0, 0}, 180'000},
    }};
    std::array<SandboxGroundBlockerDefinition, 1> blockers{{
        {blocker_id, region_id, blocker_asset, 400, 600, -100, 100, 0, 500, 0},
    }};
    std::array<SandboxSafePointDefinition, 1> safe_points{{
        {safe_id, region_id, safe_asset, {-1'000, 0, 0, 0}, 0},
    }};
    std::array<SandboxInteractionDefinition, 1> interactions{{
        {interaction_id, region_id, interaction_asset, {0, 500, 0, 0}, 90'000},
    }};
    std::array<SandboxMechanismDefinition, 1> mechanisms{{
        {mechanism_id, region_id, mechanism_asset, {0, 700, 100, 0}, 90'000},
    }};
    std::array<SandboxWaveDefinition, 1> waves{{
        {wave_id, region_id, {}, {SandboxTriggerKind::session_started, {}}},
    }};
    std::array<SandboxWaveSpawnDefinition, 1> spawns{{
        {wave_id, actor_id, 10, 1},
    }};
    std::array<SandboxObjectiveDefinition, 1> objectives{{
        {objective_id, region_id, {}, {SandboxObjectiveCompletionKind::wave_completed, wave_id}},
    }};
    std::array<SandboxInteractionGameplayBinding, 1> interaction_bindings{{
        {interaction_id, SandboxInteractionOperation::operate, 500, mechanism_id},
    }};
    std::array<SandboxMechanismGameplayBinding, 1> mechanism_bindings{{
        {mechanism_id, SandboxMechanismActivation::one_shot_activate, blocker_id},
    }};
    SandboxDefinition definition{};
    SandboxGameplayBindingDefinition binding{};

    Fixture() { rebind(); }

    void rebind() {
        definition = {
            package_id,
            sandbox_id,
            {-10'000, 10'000, -10'000, 10'000, -1'000, 4'000, 0, 1},
            objective_id,
            {player_id, region_id, player_asset, safe_id, {0, 0, 0, 0}, 0},
            regions,
            assets,
            actors,
            blockers,
            safe_points,
            interactions,
            mechanisms,
            waves,
            spawns,
            objectives,
        };
        binding = {interaction_bindings, mechanism_bindings};
    }
};

template <typename Integer>
Integer read_le(std::span<const std::uint8_t> bytes, std::size_t offset) {
    using Unsigned = std::make_unsigned_t<Integer>;
    Unsigned value = 0;
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        value |= static_cast<Unsigned>(bytes[offset + index]) << (index * 8U);
    }
    return static_cast<Integer>(value);
}

template <typename Integer>
void patch_le(std::vector<std::uint8_t>& bytes, std::size_t offset, Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto raw = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(raw >> (index * 8U));
    }
}

void reseal(std::vector<std::uint8_t>& bytes) {
    std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(sandbox_pack_hash_offset),
                sandbox_pack_hash_bytes, std::uint8_t{0});
    const auto digest = sha256(bytes);
    std::copy(digest.begin(), digest.end(),
              bytes.begin() + static_cast<std::ptrdiff_t>(sandbox_pack_hash_offset));
}

std::size_t directory_entry(std::uint16_t type) {
    return sandbox_pack_header_bytes +
           (static_cast<std::size_t>(type) - 1U) * sandbox_pack_directory_entry_bytes;
}

void expect_decode_failure(std::vector<std::uint8_t> bytes, SandboxPackageError expected) {
    const auto decoded = decode_sandbox_package(bytes);
    if (decoded.validation.error != expected) {
        std::cerr << "decoder error actual="
                  << static_cast<unsigned>(decoded.validation.error)
                  << " expected=" << static_cast<unsigned>(expected) << '\n';
        fail("unexpected decoder error");
    }
    expect(decoded.document == nullptr, "failure exposed a partial document");
}

void check_round_trip() {
    Fixture fixture;
    const auto encoded = encode_sandbox_package(fixture.definition, fixture.binding);
    expect(encoded.validation.valid(), "valid fixture failed validation");
    expect(!encoded.bytes.empty(), "valid fixture produced no bytes");
    constexpr Sha256Digest expected_hash{
        0xcd, 0x05, 0x23, 0x19, 0xe0, 0x92, 0x3e, 0xf4,
        0x17, 0x74, 0xd0, 0xaa, 0xe4, 0xd1, 0x8c, 0xcc,
        0x28, 0x48, 0xa8, 0x12, 0xe4, 0x7f, 0x64, 0x78,
        0x25, 0x14, 0x4b, 0xed, 0xea, 0xa8, 0x45, 0x73,
    };
    expect(encoded.fingerprint == expected_hash, "canonical hash changed");
    expect(read_le<std::uint16_t>(encoded.bytes, 10) == 1, "format minor mismatch");
    expect(read_le<std::uint32_t>(encoded.bytes, 32) == 15, "section count mismatch");
    expect(read_le<std::uint32_t>(encoded.bytes, directory_entry(14) + 8) == 1,
           "interaction binding section missing");
    expect(read_le<std::uint32_t>(encoded.bytes, directory_entry(15) + 12) == 32,
           "mechanism binding width mismatch");

    auto decoded = decode_sandbox_package(encoded.bytes);
    expect(decoded.validation.valid() && decoded.document != nullptr, "round trip failed");
    expect(decoded.document->definition().package_id.name == "sandbox.package.demo",
           "owned string mismatch");
    expect(decoded.document->gameplay_binding().interaction_bindings.size() == 1,
           "owned binding mismatch");
    const auto reencoded = encode_sandbox_package(
        decoded.document->definition(), decoded.document->gameplay_binding()
    );
    expect(reencoded.bytes == encoded.bytes, "round trip was not canonical");
    expect(reencoded.fingerprint == encoded.fingerprint, "round trip hash changed");

    std::reverse(fixture.assets.begin(), fixture.assets.end());
    fixture.definition.assets = fixture.assets;
    const auto permuted = encode_sandbox_package(fixture.definition, fixture.binding);
    expect(permuted.bytes == encoded.bytes, "input permutation changed canonical bytes");
}

void check_owned_lifetime() {
    std::unique_ptr<SandboxPackageDocument> document;
    {
        Fixture fixture;
        auto encoded = encode_sandbox_package(fixture.definition, fixture.binding);
        auto decoded = decode_sandbox_package(encoded.bytes);
        expect(decoded.validation.valid(), "lifetime decode failed");
        document = std::move(decoded.document);
    }
    expect(document != nullptr && document->definition().id.name == "sandbox.demo",
           "document retained an input string view");
    expect(document->definition().regions.front().id.name == "sandbox.region.main",
           "document retained an input array");
    expect(document->gameplay_binding().mechanism_bindings.front().mechanism_id.name ==
               "sandbox.mechanism",
           "document retained an external binding array");
}

bool has_diagnostic(const SandboxPackageValidation& validation,
                    SandboxDiagnosticCode code,
                    SandboxDiagnosticField field = SandboxDiagnosticField::none) {
    return std::any_of(validation.diagnostics.begin(), validation.diagnostics.end(),
                       [=](const auto& value) {
                           return value.code == code &&
                                  (field == SandboxDiagnosticField::none || value.field == field);
                       });
}

bool has_exact_diagnostic(const SandboxPackageValidation& validation,
                          SandboxDiagnosticCode code,
                          SandboxDiagnosticDomain domain,
                          std::uint32_t record_index,
                          SandboxDiagnosticField field,
                          StableContentKey subject,
                          StableContentKey related = 0) {
    return std::any_of(validation.diagnostics.begin(), validation.diagnostics.end(),
                       [=](const auto& value) {
                           return value.code == code && value.domain == domain &&
                                  value.record_index == record_index &&
                                  value.field == field && value.subject == subject &&
                                  value.related == related;
                       });
}

void check_core_validation() {
    Fixture fixture;
    fixture.actors[0].id = wave_id;
    fixture.definition.actors = fixture.actors;
    auto validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_diagnostic(validation, SandboxDiagnosticCode::duplicate_id),
           "cross-section duplicate ID was accepted");
    const auto repeated_validation =
        validate_sandbox_package(fixture.definition, fixture.binding);
    expect(repeated_validation.error == validation.error &&
               repeated_validation.diagnostics == validation.diagnostics &&
               repeated_validation.gameplay_binding_validation ==
                   validation.gameplay_binding_validation,
           "repeated validation changed its diagnostic");

    fixture = Fixture{};
    fixture.rebind();
    fixture.definition.package_id = sandbox_id;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_diagnostic(validation, SandboxDiagnosticCode::duplicate_id,
                          SandboxDiagnosticField::sandbox_id),
           "metadata duplicate was not precisely located");

    fixture = Fixture{};
    fixture.rebind();
    fixture.regions[0].bounds.max_y = 10'001;
    fixture.definition.regions = fixture.regions;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_exact_diagnostic(validation,
                                SandboxDiagnosticCode::invalid_region_bounds,
                                SandboxDiagnosticDomain::regions, 0,
                                SandboxDiagnosticField::max_y, region_id.key,
                                sandbox_id.key),
           "region world overflow did not identify max_y");

    fixture = Fixture{};
    fixture.rebind();
    fixture.definition.player.pose.height = 2'500;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_exact_diagnostic(validation,
                                SandboxDiagnosticCode::object_out_of_bounds,
                                SandboxDiagnosticDomain::player, 0,
                                SandboxDiagnosticField::height, player_id.key,
                                region_id.key),
           "placement region overflow did not identify height");
    const auto direct_height_validation = validation;
    Fixture encoded_fixture;
    auto encoded_height =
        encode_sandbox_package(encoded_fixture.definition, encoded_fixture.binding);
    expect(encoded_height.validation.valid(),
           "semantic revalidation fixture failed to encode");
    const auto player_offset = read_le<std::uint32_t>(
        encoded_height.bytes, directory_entry(5) + 16
    );
    patch_le(encoded_height.bytes, player_offset + 56, std::int32_t{2'500});
    reseal(encoded_height.bytes);
    const auto decoded_height = decode_sandbox_package(encoded_height.bytes);
    expect(decoded_height.validation.error ==
               SandboxPackageError::semantic_validation_failed,
           "decoder did not revalidate placement height");
    expect(decoded_height.document == nullptr,
           "semantic decode failure exposed a partial document");
    expect(decoded_height.validation.diagnostics ==
               direct_height_validation.diagnostics,
           "direct and decoded validation produced different locators");

    fixture = Fixture{};
    fixture.rebind();
    fixture.definition.player.pose.floor_layer = 1;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_exact_diagnostic(validation,
                                SandboxDiagnosticCode::object_out_of_bounds,
                                SandboxDiagnosticDomain::player, 0,
                                SandboxDiagnosticField::floor_layer, player_id.key,
                                region_id.key),
           "placement region overflow did not identify floor_layer");

    fixture = Fixture{};
    fixture.rebind();
    fixture.blockers[0].min_x = -6'000;
    fixture.definition.ground_blockers = fixture.blockers;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_exact_diagnostic(validation,
                                SandboxDiagnosticCode::invalid_blocker,
                                SandboxDiagnosticDomain::ground_blockers, 0,
                                SandboxDiagnosticField::min_x, blocker_id.key,
                                region_id.key),
           "blocker region overflow did not identify min_x");

    fixture = Fixture{};
    fixture.rebind();
    fixture.definition.wave_spawns = {};
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_exact_diagnostic(validation, SandboxDiagnosticCode::unreachable_node,
                                SandboxDiagnosticDomain::waves, 0,
                                SandboxDiagnosticField::id, wave_id.key),
           "wave without a spawn used an imprecise locator");
    expect(has_exact_diagnostic(validation, SandboxDiagnosticCode::unreachable_node,
                                SandboxDiagnosticDomain::actors, 0,
                                SandboxDiagnosticField::id, actor_id.key),
           "actor without a spawn used an imprecise locator");

    fixture = Fixture{};
    fixture.rebind();
    fixture.definition.player.pose = {500, 0, 0, 0};
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_diagnostic(validation, SandboxDiagnosticCode::player_start_blocked),
           "blocked player start was accepted");

    fixture = Fixture{};
    fixture.rebind();
    constexpr auto other_region_id = content_id("sandbox.region.other");
    std::array<SandboxRegionDefinition, 2> overlapping_regions{{
        fixture.regions[0],
        {other_region_id, fixture.regions[0].bounds},
    }};
    fixture.blockers[0].region_id = other_region_id;
    fixture.definition.regions = overlapping_regions;
    fixture.definition.ground_blockers = fixture.blockers;
    fixture.definition.player.pose = {500, 0, 0, 0};
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(!has_diagnostic(validation, SandboxDiagnosticCode::player_start_blocked),
           "blocker in another region blocked the player");

    fixture = Fixture{};
    fixture.rebind();
    fixture.safe_points[0].pose = {500, 0, 0, 0};
    fixture.definition.safe_points = fixture.safe_points;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_diagnostic(validation, SandboxDiagnosticCode::safe_point_blocked),
           "blocked safe point was accepted");

    fixture = Fixture{};
    fixture.rebind();
    std::array<SandboxWaveSpawnDefinition, 2> duplicate_actor{{
        fixture.spawns[0], {wave_id, actor_id, 20, 1},
    }};
    fixture.definition.wave_spawns = duplicate_actor;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    expect(has_diagnostic(validation, SandboxDiagnosticCode::invalid_wave_spawn,
                          SandboxDiagnosticField::actor_id),
           "actor assigned to multiple spawns was accepted");
    expect(has_diagnostic(validation, SandboxDiagnosticCode::invalid_wave_spawn,
                          SandboxDiagnosticField::spawn_order),
           "simultaneous duplicate spawn order diagnostic was lost");

    fixture = Fixture{};
    fixture.rebind();
    std::array<SandboxWaveDefinition, 2> terminal_waves{{
        fixture.waves[0],
        {content_id("sandbox.wave.after-completion"), region_id, {},
         {SandboxTriggerKind::objective_completed, objective_id}},
    }};
    std::array<SandboxActorDefinition, 2> terminal_actors{{
        fixture.actors[0],
        {content_id("sandbox.actor.after-completion"), region_id, actor_asset,
         {2'000, 0, 0, 0}, 0},
    }};
    std::array<SandboxWaveSpawnDefinition, 2> terminal_spawns{{
        fixture.spawns[0],
        {terminal_waves[1].id, terminal_actors[1].id, 0, 1},
    }};
    fixture.definition.waves = terminal_waves;
    fixture.definition.actors = terminal_actors;
    fixture.definition.wave_spawns = terminal_spawns;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    const auto terminal_diagnostic = std::find_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const auto& value) {
            return value.code == SandboxDiagnosticCode::unreachable_node &&
                   value.domain == SandboxDiagnosticDomain::metadata &&
                   value.record_index == 0 &&
                   value.field == SandboxDiagnosticField::completion_objective_id &&
                   value.related == objective_id.key;
        }
    );
    expect(terminal_diagnostic != validation.diagnostics.end(),
           "completion objective with a successor was not precisely rejected");

    fixture = Fixture{};
    fixture.rebind();
    std::array<SandboxWaveDefinition, 2> waves{{
        fixture.waves[0],
        {content_id("sandbox.wave.orphan"), region_id, {},
         {SandboxTriggerKind::session_started, {}}},
    }};
    std::array<SandboxActorDefinition, 2> actors{{
        fixture.actors[0],
        {content_id("sandbox.actor.orphan"), region_id, actor_asset,
         {2'000, 0, 0, 0}, 0},
    }};
    std::array<SandboxWaveSpawnDefinition, 2> spawns{{
        fixture.spawns[0],
        {waves[1].id, actors[1].id, 0, 1},
    }};
    fixture.definition.waves = waves;
    fixture.definition.actors = actors;
    fixture.definition.wave_spawns = spawns;
    validation = validate_sandbox_package(fixture.definition, fixture.binding);
    const auto orphan_diagnostic = std::find_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [&](const auto& value) {
            return value.code == SandboxDiagnosticCode::unreachable_node &&
                   value.domain == SandboxDiagnosticDomain::waves &&
                   value.record_index == 1 && value.subject == waves[1].id.key;
        }
    );
    expect(orphan_diagnostic != validation.diagnostics.end(),
           "orphan branch diagnostic lost its domain/index/key locator");
}

void check_empty_binding_sections() {
    Fixture fixture;
    fixture.definition.interactions = {};
    fixture.definition.mechanisms = {};
    fixture.binding = {};
    const auto encoded = encode_sandbox_package(fixture.definition, fixture.binding);
    expect(encoded.validation.valid(), "legal core with empty binding sets failed");
    expect(read_le<std::uint32_t>(encoded.bytes, directory_entry(14) + 8) == 0,
           "empty section 14 missing");
    expect(read_le<std::uint32_t>(encoded.bytes, directory_entry(15) + 8) == 0,
           "empty section 15 missing");
    SandboxDefinition empty{};
    const auto shell = encode_sandbox_package(empty, {});
    expect(!shell.validation.valid() && shell.bytes.empty(), "empty shell was accepted");
}

void check_corruption() {
    Fixture fixture;
    const auto encoded = encode_sandbox_package(fixture.definition, fixture.binding);
    auto bytes = encoded.bytes;
    bytes[0] = 'X';
    expect_decode_failure(bytes, SandboxPackageError::invalid_magic);
    bytes = encoded.bytes;
    bytes.pop_back();
    expect_decode_failure(bytes, SandboxPackageError::truncated);
    bytes = encoded.bytes;
    bytes.push_back(0);
    expect_decode_failure(bytes, SandboxPackageError::trailing_bytes);
    bytes = encoded.bytes;
    bytes.back() ^= 1U;
    expect_decode_failure(bytes, SandboxPackageError::hash_mismatch);
    bytes = encoded.bytes;
    patch_le(bytes, 10, std::uint16_t{0});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::unsupported_version);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(14), std::uint16_t{13});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::duplicate_section);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(14), std::uint16_t{15});
    patch_le(bytes, directory_entry(15), std::uint16_t{14});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_directory);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(14) + 2, std::uint16_t{2});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::unsupported_version);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(14) + 4, std::uint32_t{1});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_section);
    bytes = encoded.bytes;
    const auto binding_offset = read_le<std::uint32_t>(bytes, directory_entry(14) + 16);
    bytes[binding_offset + 12] = 0;
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::gameplay_binding_validation_failed);

    bytes = encoded.bytes;
    bytes[binding_offset + 12] = 255;
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::gameplay_binding_validation_failed);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(14) + 12, std::uint32_t{31});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_section);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(14) + 8, std::uint32_t{65});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_section);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(15) + 16,
             read_le<std::uint32_t>(bytes, directory_entry(14) + 16));
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_directory);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(15) + 16,
             read_le<std::uint32_t>(bytes, directory_entry(15) + 16) + 1U);
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_directory);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(15), std::uint16_t{16});
    patch_le(bytes, directory_entry(15) + 4, sandbox_pack_section_optional);
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::missing_section);
    bytes = encoded.bytes;
    patch_le(bytes, 16, std::uint32_t{1});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_reserved);
    bytes = encoded.bytes;
    const auto metadata_offset = read_le<std::uint32_t>(bytes, directory_entry(2) + 16);
    patch_le(bytes, metadata_offset + 8, std::uint32_t{0xffff'fffeU});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_stable_id);
    bytes = encoded.bytes;
    bytes[metadata_offset] ^= 1U;
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_stable_id);
    bytes = encoded.bytes;
    patch_le(bytes, directory_entry(15) + 16, std::uint32_t{0xffff'fff8U});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::invalid_directory);

    bytes = encoded.bytes;
    constexpr std::size_t old_directory_end =
        sandbox_pack_header_bytes + 15U * sandbox_pack_directory_entry_bytes;
    bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(old_directory_end),
                 sandbox_pack_directory_entry_bytes, 0);
    for (std::uint16_t type = 1; type <= 15; ++type) {
        const auto offset_field = directory_entry(type) + 16;
        patch_le(bytes, offset_field, read_le<std::uint32_t>(bytes, offset_field) + 24U);
    }
    const auto optional = old_directory_end;
    patch_le(bytes, optional, std::uint16_t{16});
    patch_le(bytes, optional + 2, std::uint16_t{99});
    patch_le(bytes, optional + 4, sandbox_pack_section_optional);
    patch_le(bytes, optional + 16, static_cast<std::uint32_t>(bytes.size()));
    patch_le(bytes, 32, std::uint32_t{16});
    patch_le(bytes, 40, static_cast<std::uint32_t>(bytes.size()));
    reseal(bytes);
    auto optional_decoded = decode_sandbox_package(bytes);
    expect(optional_decoded.validation.valid() && optional_decoded.document != nullptr,
           "unknown optional section was not skipped");
    patch_le(bytes, optional + 4, std::uint32_t{0});
    reseal(bytes);
    expect_decode_failure(bytes, SandboxPackageError::unknown_required_section);
}

}  // namespace

int main() {
    static_assert(sandbox_pack_format_major == 1 && sandbox_pack_format_minor == 1);
    static_assert(sandbox_content_api_major == 1 && sandbox_content_api_minor == 0);
    static_assert(sandbox_authoring_schema_major == 1);
    static_assert(sandbox_authoring_schema_minor == 1);
    static_assert(sandbox_authoring_schema_patch == 0);
    static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::interaction_gameplay_bindings) == 14);
    static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::mechanism_gameplay_bindings) == 15);
    static_assert(sandbox_pack_interaction_gameplay_binding_record_bytes == 32);
    static_assert(sandbox_pack_mechanism_gameplay_binding_record_bytes == 32);
    check_round_trip();
    check_owned_lifetime();
    check_core_validation();
    check_empty_binding_sections();
    check_corruption();
    return EXIT_SUCCESS;
}
