#include <tgd/content/sandbox_package.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/sandbox_gameplay_binding.hpp>
#include <tgd/contracts/sha256.hpp>
#include <tgd/integration/sandbox_runtime_coordinator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace tgd::contracts;
using namespace tgd::integration;

constexpr StableActorKey player_actor = 0x706c617965720001ULL;
constexpr std::string_view player_id = "player.system_demo.start";
constexpr std::string_view region_id = "region.system_demo.arena";

constexpr Sha256Digest expected_package_sha{
    0x89, 0xa7, 0xc3, 0x6f, 0xf1, 0x85, 0x86, 0x7f,
    0xab, 0xb0, 0xaa, 0xb7, 0x9c, 0xb6, 0x8f, 0x4f,
    0x42, 0x8a, 0x4a, 0x62, 0x17, 0x9d, 0xad, 0xfb,
    0x8d, 0x22, 0x9e, 0xed, 0xee, 0x8d, 0x7f, 0xf6,
};
constexpr Sha256Digest expected_provider_checksum{
    0xc8, 0x4a, 0xed, 0xee, 0xfc, 0x1e, 0x77, 0xb8,
    0x81, 0x90, 0x6e, 0x2e, 0xd5, 0x1d, 0x0e, 0x9d,
    0x9b, 0xea, 0xf9, 0xbe, 0x33, 0x75, 0xe4, 0x28,
    0xf0, 0xa9, 0xed, 0xa5, 0xac, 0x09, 0xce, 0xf8,
};

[[noreturn]] void fail(std::string_view message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

[[nodiscard]] std::vector<std::uint8_t> read_bytes(const char* path) {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    expect(input.is_open(), "canonical package input could not be opened");
    const auto end = input.tellg();
    expect(end > 0, "canonical package input was empty");
    const auto size = static_cast<std::size_t>(end);
    std::vector<std::uint8_t> bytes(size);
    input.seekg(0, std::ios::beg);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    expect(input.good(), "canonical package input could not be read");
    return bytes;
}

[[nodiscard]] SandboxPublishedPackageArtifact artifact_from(
    const std::vector<std::uint8_t>& bytes,
    std::uint32_t generation,
    std::optional<Sha256Digest> required_checksum = std::nullopt
) {
    const auto decoded = tgd::content::decode_sandbox_package(bytes);
    expect(decoded.validation.valid() && decoded.document != nullptr,
           "trusted canonical package did not decode");
    const auto checksum = decoded.document->fingerprint();
    if (required_checksum.has_value()) {
        expect(checksum == *required_checksum,
               "canonical package provider checksum drifted");
    }
    return {{generation, checksum}, bytes};
}

[[nodiscard]] std::string asset_name(const SandboxOwnedStableAsset& asset) {
    return {asset.id_bytes.data(), asset.id_byte_count};
}

[[nodiscard]] bool contains_asset(
    const SandboxRuntimeCoordinator& coordinator,
    std::string_view name,
    SandboxAssetKind kind
) {
    const auto current = coordinator.snapshot();
    for (std::size_t index = 0; index < current.asset_count; ++index) {
        const auto asset = coordinator.asset_at(index);
        if (asset.has_value() && asset->key == stable_content_key(name) &&
            asset_name(*asset) == name && asset->kind == kind) {
            return true;
        }
    }
    return false;
}

struct LiveEvidence final {
    SandboxRuntimeSnapshot snapshot{};
    const tgd::content::SandboxPackageDocument* document{};
    const tgd::gameplay::SandboxSession* session{};
    const tgd::runtime::StaticCollisionWorld* collision{};
    std::optional<SandboxOwnedStableAsset> first_asset{};
    std::optional<SandboxStaticCollisionRecord> first_collision{};
};

[[nodiscard]] LiveEvidence capture(const SandboxRuntimeCoordinator& coordinator) {
    return {
        coordinator.snapshot(),
        coordinator.document(),
        coordinator.session(),
        coordinator.collision_world(stable_content_key(region_id)),
        coordinator.asset_at(0),
        coordinator.collision_at(0),
    };
}

void expect_preserved(
    const SandboxRuntimeCoordinator& coordinator,
    const LiveEvidence& before,
    std::string_view message
) {
    expect(coordinator.snapshot() == before.snapshot, message);
    expect(coordinator.document() == before.document, message);
    expect(coordinator.session() == before.session, message);
    expect(
        coordinator.collision_world(stable_content_key(region_id)) ==
            before.collision,
        message
    );
    expect(coordinator.asset_at(0) == before.first_asset, message);
    expect(coordinator.collision_at(0) == before.first_collision, message);
}

void check_system_demo_projection(const SandboxRuntimeCoordinator& coordinator) {
    const auto* document = coordinator.document();
    expect(document != nullptr, "published coordinator omitted owned document");
    const auto& definition = document->definition();
    const auto& binding = document->gameplay_binding();
    expect(
        definition.assets.size() == 12 &&
            definition.actors.size() == 4 &&
            definition.ground_blockers.size() == 1 &&
            definition.safe_points.size() == 1 &&
            definition.interactions.size() == 1 &&
            definition.mechanisms.size() == 1,
        "unique system-demo package cardinalities drifted"
    );
    expect(
        binding.interaction_bindings.size() == 1 &&
            binding.mechanism_bindings.size() == 1 &&
            binding.interaction_bindings.front().operation ==
                SandboxInteractionOperation::operate &&
            binding.interaction_bindings.front().range_mm == 1'200 &&
            binding.interaction_bindings.front().target_mechanism_id.key ==
                definition.mechanisms.front().id.key &&
            binding.mechanism_bindings.front().activation ==
                SandboxMechanismActivation::one_shot_activate &&
            binding.mechanism_bindings.front().target_ground_blocker_id.key ==
                definition.ground_blockers.front().id.key,
        "unique system-demo typed interaction chain drifted"
    );

    const auto interaction_pose = definition.interactions.front().pose;
    const GroundPoseMm proof_point{-1'500, -1'500, 0, 0};
    const GroundPoseMm range_plus_one{-1'500, -1'701, 0, 0};
    const GroundPoseMm wrong_floor{-1'500, -1'500, 0, 1};
    const GroundPoseMm different_height{-1'500, -1'500, 3'000, 0};
    expect(
        sandbox_check_operate_range(proof_point, interaction_pose, 1'200) ==
            SandboxOperateRangeCheck::eligible,
        "test-only proof point was not eligible at 1000mm"
    );
    expect(
        sandbox_check_operate_range(different_height, interaction_pose, 1'200) ==
            SandboxOperateRangeCheck::eligible,
        "operate range incorrectly consumed height"
    );
    expect(
        sandbox_check_operate_range(range_plus_one, interaction_pose, 1'200) !=
                SandboxOperateRangeCheck::eligible &&
            sandbox_check_operate_range(wrong_floor, interaction_pose, 1'200) !=
                SandboxOperateRangeCheck::eligible,
        "operate range accepted range+1mm or wrong floor"
    );
    expect(
        sandbox_check_operate_range(
            definition.player.pose,
            interaction_pose,
            1'200
        ) != SandboxOperateRangeCheck::eligible &&
            sandbox_check_operate_range(
                definition.safe_points.front().pose,
                interaction_pose,
                1'200
            ) != SandboxOperateRangeCheck::eligible,
        "authored spawn or safe point unexpectedly began in operate range"
    );

    const auto blocker = coordinator.collision_at(0);
    expect(
        blocker.has_value() &&
            blocker->blocker_key ==
                stable_content_key("blocker.system_demo.gate") &&
            blocker->region_key == stable_content_key(region_id) &&
            blocker->shape_id == 1 &&
            blocker->min_x == -2'500 && blocker->max_x == 2'500 &&
            blocker->min_y == 500 && blocker->max_y == 1'000 &&
            blocker->min_height == 0 && blocker->max_height == 2'500 &&
            blocker->floor_layer == 0,
        "static blocker geometry or Stable key mapping drifted"
    );
    const auto* world = coordinator.collision_world(stable_content_key(region_id));
    expect(world != nullptr && world->blocker_count() == 1,
           "region static collision candidate was not configured");
    const auto blocked = world->resolve_ground_move(
        GroundPoseMm{0, 0, 0, 0},
        0,
        1'000,
        100,
        1'800
    );
    expect(blocked.blocked_y, "authored initial blocker was not solid");

    const std::array expected_assets{
        std::pair{"asset.system_demo.enemy.elite", SandboxAssetKind::actor},
        std::pair{"asset.system_demo.enemy.flanker", SandboxAssetKind::actor},
        std::pair{"asset.system_demo.enemy.pressure", SandboxAssetKind::actor},
        std::pair{"asset.system_demo.interaction.console", SandboxAssetKind::interaction},
        std::pair{"asset.system_demo.mechanism.gate", SandboxAssetKind::mechanism},
        std::pair{"asset.system_demo.obstacle.tension_gate", SandboxAssetKind::obstacle},
        std::pair{"asset.system_demo.player", SandboxAssetKind::player},
        std::pair{"asset.system_demo.safe_point.lamp_shelter", SandboxAssetKind::safe_point},
        std::pair{"asset.system_demo.skill.eavesguard.hit", SandboxAssetKind::effect},
        std::pair{"asset.system_demo.skill.eavesguard.telegraph", SandboxAssetKind::effect},
        std::pair{"asset.system_demo.skill.flower_turn.hit", SandboxAssetKind::effect},
        std::pair{"asset.system_demo.skill.flower_turn.telegraph", SandboxAssetKind::effect},
    };
    for (const auto& [name, kind] : expected_assets) {
        expect(contains_asset(coordinator, name, kind),
               "owned Stable Asset ID/kind set drifted");
    }

    StableContentKey previous{};
    for (std::size_t index = 0; index < coordinator.snapshot().asset_count; ++index) {
        const auto asset = coordinator.asset_at(index);
        expect(asset.has_value() && (index == 0 || previous < asset->key),
               "asset set was not normalized by Stable key");
        previous = asset->key;
    }
}

void run_probe(const char* canonical_path, const char* changed_path) {
    auto canonical_bytes = read_bytes(canonical_path);
    auto changed_bytes = read_bytes(changed_path);
    expect(canonical_bytes.size() == 2'712,
           "trusted canonical package byte count drifted");
    expect(sha256(canonical_bytes) == expected_package_sha,
           "trusted canonical package SHA-256 drifted");

    const auto canonical = artifact_from(
        canonical_bytes,
        1,
        expected_provider_checksum
    );
    const auto changed = artifact_from(changed_bytes, 3);
    expect(
        changed.identity.checksum() != canonical.identity.checksum() &&
            changed.canonical_bytes != canonical.canonical_bytes,
        "trusted changed package did not change identity"
    );

    SandboxRuntimeCoordinator coordinator;
    const tgd::gameplay::SandboxPlayerRuntimeBinding player_binding{
        content_id(player_id),
        player_actor,
    };
    const auto first = coordinator.publish(canonical, player_binding);
    expect(
        first.disposition == SandboxRuntimePublishDisposition::published &&
            first.snapshot.initialized &&
            first.snapshot.runtime_generation == 1 &&
            first.snapshot.package_generation == 1 &&
            first.snapshot.package_checksum == expected_provider_checksum &&
            first.snapshot.canonical_byte_count == canonical_bytes.size() &&
            first.snapshot.session.generation == 1 &&
            first.snapshot.asset_count == 12 &&
            first.snapshot.collision_record_count == 1,
        "first system-demo aggregate publication failed"
    );
    expect(
        coordinator.document()->fingerprint() == first.snapshot.package_checksum &&
            coordinator.session()->snapshot() == first.snapshot.session,
        "aggregate components did not share the published package identity"
    );
    check_system_demo_projection(coordinator);

    const auto first_live = capture(coordinator);
    auto same_checksum = canonical;
    same_checksum.identity = tgd::content::SandboxPackagePublicationIdentity{
        2,
        canonical.identity.checksum(),
    };
    const auto unchanged = coordinator.publish(
        std::move(same_checksum),
        player_binding
    );
    expect(
        unchanged.disposition == SandboxRuntimePublishDisposition::unchanged &&
            unchanged.snapshot.package_generation == 1,
        "same-checksum provider republish changed Host package identity"
    );
    expect_preserved(
        coordinator,
        first_live,
        "same-checksum republish partially replaced the aggregate"
    );

    const auto before_prepare_failure = capture(coordinator);
    const auto bad_player = coordinator.publish(
        changed,
        {content_id(player_id), 0}
    );
    expect(
        bad_player.disposition ==
            SandboxRuntimePublishDisposition::session_prepare_failed,
        "bad player binding did not fail during private Session prepare"
    );
    expect_preserved(
        coordinator,
        before_prepare_failure,
        "Session prepare failure changed the live aggregate"
    );

    const auto old_document = coordinator.document();
    const auto old_session = coordinator.session();
    const auto old_collision =
        coordinator.collision_world(stable_content_key(region_id));
    const auto replaced = coordinator.publish(changed, player_binding);
    expect(
        replaced.disposition == SandboxRuntimePublishDisposition::published &&
            replaced.snapshot.runtime_generation == 2 &&
            replaced.snapshot.package_generation == 3 &&
            replaced.snapshot.session.generation == 1 &&
            replaced.snapshot.package_checksum == changed.identity.checksum(),
        "changed package did not publish one complete aggregate"
    );
    expect(
        coordinator.document() != old_document &&
            coordinator.session() != old_session &&
            coordinator.collision_world(stable_content_key(region_id)) !=
                old_collision,
        "changed package retained a component from the old aggregate"
    );
    expect(
        coordinator.document()->gameplay_binding()
                .interaction_bindings.front().range_mm == 1'300,
        "trusted changed package did not reach the live Session source"
    );

    const auto changed_live = capture(coordinator);
    auto stale = canonical;
    stale.identity = tgd::content::SandboxPackagePublicationIdentity{
        2,
        canonical.identity.checksum(),
    };
    expect(
        coordinator.publish(std::move(stale), player_binding).disposition ==
            SandboxRuntimePublishDisposition::stale_generation,
        "stale package generation was not rejected"
    );
    expect_preserved(
        coordinator,
        changed_live,
        "stale package changed the live aggregate"
    );

    auto conflict = canonical;
    conflict.identity = tgd::content::SandboxPackagePublicationIdentity{
        3,
        canonical.identity.checksum(),
    };
    expect(
        coordinator.publish(std::move(conflict), player_binding).disposition ==
            SandboxRuntimePublishDisposition::identity_conflict,
        "same-generation different-checksum package was not rejected"
    );
    expect_preserved(
        coordinator,
        changed_live,
        "identity conflict changed the live aggregate"
    );

    auto corrupt = changed;
    corrupt.identity = tgd::content::SandboxPackagePublicationIdentity{
        4,
        changed.identity.checksum(),
    };
    corrupt.canonical_bytes.front() ^= 0xffU;
    expect(
        coordinator.publish(std::move(corrupt), player_binding).disposition ==
            SandboxRuntimePublishDisposition::decode_failed,
        "corrupt same-checksum canonical bytes were treated as unchanged"
    );
    expect_preserved(
        coordinator,
        changed_live,
        "decode failure changed the live aggregate"
    );

    auto wrong_checksum = canonical;
    auto mismatched_checksum = canonical.identity.checksum();
    mismatched_checksum.front() ^= 0xffU;
    wrong_checksum.identity = tgd::content::SandboxPackagePublicationIdentity{
        4,
        mismatched_checksum,
    };
    expect(
        coordinator.publish(
            std::move(wrong_checksum),
            player_binding
        ).disposition == SandboxRuntimePublishDisposition::fingerprint_mismatch,
        "publication checksum mismatch was not rejected"
    );
    expect_preserved(
        coordinator,
        changed_live,
        "fingerprint mismatch changed the live aggregate"
    );

    auto invalid = canonical;
    invalid.identity = tgd::content::SandboxPackagePublicationIdentity{
        0,
        canonical.identity.checksum(),
    };
    expect(
        coordinator.publish(std::move(invalid), player_binding).disposition ==
            SandboxRuntimePublishDisposition::invalid_artifact,
        "zero package generation was accepted"
    );
    expect_preserved(
        coordinator,
        changed_live,
        "invalid artifact changed the live aggregate"
    );

    auto changed_republish = changed;
    changed_republish.identity = tgd::content::SandboxPackagePublicationIdentity{
        4,
        changed.identity.checksum(),
    };
    expect(
        coordinator.publish(
            std::move(changed_republish),
            player_binding
        ).disposition == SandboxRuntimePublishDisposition::unchanged,
        "valid same-checksum changed-package republish was not unchanged"
    );
    expect_preserved(
        coordinator,
        changed_live,
        "changed-package same-checksum republish drifted live state"
    );

    canonical_bytes.clear();
    canonical_bytes.shrink_to_fit();
    changed_bytes.clear();
    changed_bytes.shrink_to_fit();
    expect(
        coordinator.document() != nullptr &&
            coordinator.document()->fingerprint() ==
                coordinator.snapshot().package_checksum &&
            coordinator.session() != nullptr &&
            coordinator.asset_at(0).has_value() &&
            coordinator.collision_at(0).has_value(),
        "destroying package inputs invalidated the owned aggregate"
    );

    expect(
        sandbox_next_runtime_generation(0).valid &&
            sandbox_next_runtime_generation(0).generation == 1 &&
            !sandbox_next_runtime_generation(
                std::numeric_limits<std::uint32_t>::max()
            ).valid,
        "runtime generation saturation helper drifted"
    );
}

}  // namespace

int main(int argc, char** argv) {
#if defined(TGD_SYSTEM_DEMO_CANONICAL_PACKAGE_PATH) && \
    defined(TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE_PATH)
    static_cast<void>(argc);
    static_cast<void>(argv);
    run_probe(
        TGD_SYSTEM_DEMO_CANONICAL_PACKAGE_PATH,
        TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE_PATH
    );
#else
    if (argc != 3) {
        fail("expected canonical and changed system-demo package paths");
    }
    run_probe(argv[1], argv[2]);
#endif
    std::cout << "sandbox runtime coordinator probe passed\n";
    return EXIT_SUCCESS;
}
