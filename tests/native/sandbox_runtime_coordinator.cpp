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
    0xa0, 0x9c, 0xe3, 0x5e, 0x58, 0xa4, 0x8c, 0x01,
    0xb1, 0x5d, 0x49, 0x46, 0xde, 0x8e, 0x38, 0x0d,
    0x75, 0x27, 0x60, 0x14, 0x2d, 0xdb, 0xef, 0xee,
    0x7b, 0xce, 0x7f, 0x74, 0x76, 0xb0, 0x25, 0x2e,
};
constexpr Sha256Digest expected_provider_checksum{
    0x0c, 0xfd, 0x8c, 0x17, 0x73, 0x22, 0x78, 0x07,
    0x4d, 0xeb, 0x08, 0xdc, 0xcf, 0x5f, 0x1e, 0xb5,
    0xd4, 0xab, 0xf1, 0xf5, 0x9f, 0xfb, 0xaa, 0x1d,
    0xe6, 0x10, 0xee, 0xac, 0x7d, 0x87, 0xe2, 0xe9,
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

[[nodiscard]] bool exercise_thin_runtime(
    SandboxRuntimeCoordinator& coordinator
);

void run_probe(const char* canonical_path, const char* changed_path) {
    auto canonical_bytes = read_bytes(canonical_path);
    auto changed_bytes = read_bytes(changed_path);
    expect(canonical_bytes.size() == 2'960,
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
    const SandboxThinRuntimePlayerConfig player_config{500, 100, 1'800};
    const auto first = coordinator.publish(canonical, player_binding, player_config);
    expect(
        first.disposition == SandboxRuntimePublishDisposition::published &&
        first.snapshot.initialized &&
            first.snapshot.player_config == player_config &&
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
    SandboxRuntimeCoordinator thin_coordinator;
    const auto thin_published = thin_coordinator.publish(
        canonical,
        player_binding,
        player_config
    );
    expect(
        thin_published.disposition == SandboxRuntimePublishDisposition::published
            && thin_published.snapshot.canonical_byte_count == 2'960
            && thin_coordinator.document()->gameplay_binding()
                    .interaction_bindings.front().range_mm == 1'200,
        "thin-runtime route did not start from the unique canonical package"
    );
    expect(
        exercise_thin_runtime(thin_coordinator),
        "canonical thin-runtime movement/operate/retry probe failed"
    );

    const auto first_live = capture(coordinator);
    auto same_checksum = canonical;
    same_checksum.identity = tgd::content::SandboxPackagePublicationIdentity{
        2,
        canonical.identity.checksum(),
    };
    const auto unchanged = coordinator.publish(
        std::move(same_checksum),
        player_binding,
        player_config
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
    auto config_conflict = canonical;
    config_conflict.identity = tgd::content::SandboxPackagePublicationIdentity{
        2,
        canonical.identity.checksum(),
    };
    expect(
        coordinator.publish(
            std::move(config_conflict),
            player_binding,
            SandboxThinRuntimePlayerConfig{500, 101, 1'800}
        ).disposition == SandboxRuntimePublishDisposition::identity_conflict,
        "same-package runtime config conflict was not rejected"
    );
    expect_preserved(
        coordinator,
        first_live,
        "runtime config conflict changed the live aggregate"
    );
    const auto expect_same_checksum_binding_failure = [&](
        const tgd::gameplay::SandboxPlayerRuntimeBinding& attempted_binding,
        const SandboxRuntimePublishDisposition expected,
        std::string_view message
    ) {
        auto republish = canonical;
        republish.identity = tgd::content::SandboxPackagePublicationIdentity{
            2,
            canonical.identity.checksum(),
        };
        const auto before = capture(coordinator);
        const auto result = coordinator.publish(
            std::move(republish),
            attempted_binding,
            player_config
        );
        expect(result.disposition == expected, message);
        expect_preserved(coordinator, before, message);
    };
    expect_same_checksum_binding_failure(
        {},
        SandboxRuntimePublishDisposition::session_prepare_failed,
        "missing same-checksum player binding changed the live aggregate"
    );
    auto malformed_player_id = player_binding.player_content_id;
    ++malformed_player_id.key;
    expect_same_checksum_binding_failure(
        {malformed_player_id, player_actor},
        SandboxRuntimePublishDisposition::session_prepare_failed,
        "malformed same-checksum player id changed the live aggregate"
    );
    expect_same_checksum_binding_failure(
        {content_id("sandbox.player.not-published"), player_actor},
        SandboxRuntimePublishDisposition::session_prepare_failed,
        "wrong same-checksum player id changed the live aggregate"
    );
    expect_same_checksum_binding_failure(
        {player_binding.player_content_id, 0},
        SandboxRuntimePublishDisposition::session_prepare_failed,
        "zero same-checksum player actor changed the live aggregate"
    );
    expect_same_checksum_binding_failure(
        {player_binding.player_content_id, player_actor + 1U},
        SandboxRuntimePublishDisposition::identity_conflict,
        "wrong nonzero same-checksum actor changed the live aggregate"
    );

    const auto before_prepare_failure = capture(coordinator);
    const auto bad_player = coordinator.publish(
        changed,
        {content_id(player_id), 0},
        player_config
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
    for (const auto invalid_config : std::array{
             SandboxThinRuntimePlayerConfig{0, 100, 1'800},
             SandboxThinRuntimePlayerConfig{500, -1, 1'800},
             SandboxThinRuntimePlayerConfig{
                 2'147'483'647,
                 2'147'483'647,
                 2'147'483'647,
             },
         }) {
        expect(
            coordinator.publish(changed, player_binding, invalid_config).disposition
                == SandboxRuntimePublishDisposition::invalid_player_runtime_config,
            "invalid thin-runtime player config was accepted"
        );
        expect_preserved(
            coordinator,
            before_prepare_failure,
            "invalid player config changed the live aggregate"
        );
    }

    const auto old_document = coordinator.document();
    const auto old_session = coordinator.session();
    const auto old_collision =
        coordinator.collision_world(stable_content_key(region_id));
    const auto replaced = coordinator.publish(changed, player_binding, player_config);
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
        coordinator.publish(std::move(stale), player_binding, player_config).disposition ==
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
        coordinator.publish(std::move(conflict), player_binding, player_config).disposition ==
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
        coordinator.publish(std::move(corrupt), player_binding, player_config).disposition ==
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
            player_binding,
            player_config
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
        coordinator.publish(std::move(invalid), player_binding, player_config).disposition ==
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
            player_binding,
            player_config
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

[[nodiscard]] bool exercise_thin_runtime(
    tgd::integration::SandboxRuntimeCoordinator& coordinator
) {
    using namespace tgd;
    using CommandDisposition = integration::SandboxRuntimeCommandDisposition;

    const auto* document = coordinator.document();
    const auto initial = coordinator.snapshot();
    if (document == nullptr || !initial.initialized
        || document->definition().interactions.size() != 1U
        || document->definition().ground_blockers.size() != 1U) {
        std::cerr << "thin runtime requires the validated system-demo chain\n";
        return false;
    }
    const auto& definition = document->definition();
    const auto& interaction_definition = definition.interactions.front();
    const auto interaction = interaction_definition.id.key;
    const auto region = definition.player.region_id.key;
    const auto actor = initial.session.player_actor;
    const auto safe_point = std::find_if(
        definition.safe_points.begin(),
        definition.safe_points.end(),
        [&](const contracts::SandboxSafePointDefinition& candidate) {
            return candidate.id.key == definition.player.initial_safe_point_id.key;
        }
    );
    if (safe_point == definition.safe_points.end()) {
        std::cerr << "missing authored initial safe point\n";
        return false;
    }

    const auto preserved = [&](const auto* old_document,
                               const auto* old_session,
                               const auto* old_collision,
                               const integration::SandboxRuntimeSnapshot& old_snapshot) {
        return coordinator.document() == old_document
            && coordinator.session() == old_session
            && coordinator.collision_world(region) == old_collision
            && coordinator.snapshot() == old_snapshot;
    };
    struct PreservationWitness final {
        const content::SandboxPackageDocument* document{};
        const gameplay::SandboxSession* session{};
        const runtime::StaticCollisionWorld* collision{};
        integration::SandboxRuntimeSnapshot snapshot{};
    };
    const auto capture = [&]() {
        return PreservationWitness{
            coordinator.document(),
            coordinator.session(),
            coordinator.collision_world(region),
            coordinator.snapshot(),
        };
    };
    const auto expect_preserved = [&](const auto& result,
                                      const CommandDisposition disposition,
                                      const char* label,
                                      const PreservationWitness& old) {
        if (result.disposition != disposition
            || result.snapshot != old.snapshot
            || !preserved(old.document, old.session, old.collision, old.snapshot)) {
            std::cerr << label << " did not preserve the live aggregate\n";
            return false;
        }
        return true;
    };

    const auto move = [&](const std::int32_t dx, const std::int32_t dy) {
        const auto before = coordinator.snapshot();
        return coordinator.advance_player({
            before.runtime_generation,
            before.movement_sequence + 1U,
            before.authoritative_tick + 1U,
            before.session.player_actor,
            before.session.player_pose.floor_layer,
            dx,
            dy,
        });
    };

    const auto base = coordinator.snapshot();
    const auto base_witness = capture();
    const auto spawn_rejected = coordinator.submit_operate({
        base.runtime_generation,
        1U,
        actor,
        interaction,
    });
    const auto unknown_rejected = coordinator.submit_operate({
        base.runtime_generation,
        1U,
        actor,
        interaction + 1U,
    });
    if (spawn_rejected.dispatch.result.disposition
            != contracts::SandboxOperateDisposition::out_of_range
        || unknown_rejected.dispatch.result.disposition
            != contracts::SandboxOperateDisposition::unknown_interaction
        || !expect_preserved(
            spawn_rejected,
            CommandDisposition::session_rejected,
            "spawn out-of-range operate",
            base_witness
        )
        || !expect_preserved(
            unknown_rejected,
            CommandDisposition::session_rejected,
            "unknown interaction",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation + 1U,
                1U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                -500,
                0,
            }),
            CommandDisposition::stale_generation,
            "wrong-generation movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                0U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                -500,
                0,
            }),
            CommandDisposition::stale_sequence,
            "stale movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                2U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                -500,
                0,
            }),
            CommandDisposition::out_of_order_sequence,
            "out-of-order movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                2U,
                actor,
                base.session.player_pose.floor_layer,
                -500,
                0,
            }),
            CommandDisposition::invalid_tick,
            "wrong-tick movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                1U,
                actor + 1U,
                base.session.player_pose.floor_layer,
                -500,
                0,
            }),
            CommandDisposition::invalid_actor,
            "wrong-actor movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                1U,
                actor,
                static_cast<std::int16_t>(base.session.player_pose.floor_layer + 1),
                -500,
                0,
            }),
            CommandDisposition::floor_mismatch,
            "wrong-floor movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                0,
                0,
            }),
            CommandDisposition::invalid_delta,
            "zero movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                base.player_config.max_move_delta_mm + 1,
                0,
            }),
            CommandDisposition::invalid_delta,
            "oversized movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                (-2147483647 - 1),
                (-2147483647 - 1),
            }),
            CommandDisposition::invalid_delta,
            "minimum-int movement",
            base_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                base.runtime_generation,
                1U,
                1U,
                actor,
                base.session.player_pose.floor_layer,
                2147483647,
                2147483647,
            }),
            CommandDisposition::invalid_delta,
            "maximum-int movement",
            base_witness
        )) {
        return false;
    }

    const auto* collision = coordinator.collision_world(region);
    const auto region_definition = std::find_if(
        document->definition().regions.begin(),
        document->definition().regions.end(),
        [&](const contracts::SandboxRegionDefinition& candidate) {
            return candidate.id.key == region;
        }
    );
    if (collision == nullptr
        || region_definition == document->definition().regions.end()) {
        std::cerr << "missing authored player region collision\n";
        return false;
    }
    auto boundary_pose = base.session.player_pose;
    boundary_pose.x = region_definition->bounds.max_x
        - base.player_config.collision_radius_mm;
    const auto boundary = collision->resolve_ground_move(
        boundary_pose,
        1,
        0,
        base.player_config.collision_radius_mm,
        base.player_config.collision_height_mm
    );
    if (!boundary.blocked_x || boundary.pose != boundary_pose) {
        std::cerr << "authored region bounds permitted a gate bypass\n";
        return false;
    }
    const auto initial_record = coordinator.collision_at(0);
    const contracts::GroundPoseMm gate_approach{-1500, 0, 0, 0};
    const auto closed_probe = collision->resolve_ground_move(
        gate_approach,
        0,
        500,
        base.player_config.collision_radius_mm,
        base.player_config.collision_height_mm
    );
    if (!initial_record.has_value() || !initial_record->enabled
        || !closed_probe.blocked_y || closed_probe.pose != gate_approach) {
        std::cerr << "canonical closed gate collision probe failed\n";
        return false;
    }

    for (const auto [dx, dy] : std::array{
             std::pair{-500, 0},
             std::pair{-500, 0},
             std::pair{-500, 0},
             std::pair{0, 500},
             std::pair{0, 500},
             std::pair{0, 500},
             std::pair{0, 250},
         }) {
        const auto result = move(dx, dy);
        if (result.disposition != CommandDisposition::applied) {
            std::cerr << "movement to the authored operate proof point failed\n";
            return false;
        }
    }
    if (coordinator.snapshot().session.player_pose
            != contracts::GroundPoseMm{-1500, -1500, 0, 0}
        || interaction_definition.pose.height
            == coordinator.snapshot().session.player_pose.height) {
        std::cerr << "authoritative Session pose missed the proof point\n";
        return false;
    }

    const auto before_operate = coordinator.snapshot();
    const auto proof_witness = capture();
    if (!expect_preserved(
            coordinator.submit_operate({
                before_operate.runtime_generation,
                2U,
                actor,
                interaction,
            }),
            CommandDisposition::out_of_order_sequence,
            "out-of-order operate",
            proof_witness
        )) {
        return false;
    }
    const auto operated = coordinator.submit_operate({
        before_operate.runtime_generation,
        1U,
        actor,
        interaction,
    });
    if (operated.disposition != CommandDisposition::applied
        || operated.dispatch.result.disposition
            != contracts::SandboxOperateDisposition::completed_chain
        || operated.dispatch.events[0].kind
            != contracts::SandboxGameplayEventKind::interaction_completed
        || operated.dispatch.events[1].kind
            != contracts::SandboxGameplayEventKind::mechanism_activated) {
        std::cerr << "first operate did not publish the fixed event chain\n";
        return false;
    }
    const auto collision_record = coordinator.collision_at(0);
    const auto* opened_world = coordinator.collision_world(region);
    if (!collision_record.has_value() || collision_record->enabled
        || opened_world == nullptr
        || opened_world->blocker_enabled(collision_record->shape_id)
            != std::optional<bool>{false}) {
        std::cerr << "Session blocker state did not project by exact key\n";
        return false;
    }

    for (int step = 0; step < 3; ++step) {
        if (move(0, 500).disposition != CommandDisposition::applied) {
            std::cerr << "opened route to the gate failed\n";
            return false;
        }
    }
    if (move(0, 500).disposition != CommandDisposition::applied) {
        std::cerr << "opened gate did not permit the same movement\n";
        return false;
    }
    const auto completed = coordinator.snapshot();
    const auto* completed_document = coordinator.document();
    const auto* completed_session = coordinator.session();
    const auto* completed_collision = coordinator.collision_world(region);
    const auto replay = coordinator.submit_operate({
        completed.runtime_generation,
        1U,
        actor,
        interaction,
    });
    if (replay.disposition != CommandDisposition::stale_sequence
        || !preserved(
            completed_document,
            completed_session,
            completed_collision,
            completed
        )) {
        std::cerr << "committed operate replay drifted\n";
        return false;
    }
    const auto repeated = coordinator.submit_operate({
        completed.runtime_generation,
        2U,
        actor,
        interaction,
    });
    if (repeated.disposition != CommandDisposition::repeated
        || repeated.dispatch.result.disposition
            != contracts::SandboxOperateDisposition::repeated_chain
        || !preserved(
            completed_document,
            completed_session,
            completed_collision,
            completed
        )) {
        std::cerr << "fresh repeated operate drifted\n";
        return false;
    }
    const PreservationWitness completed_witness{
        completed_document,
        completed_session,
        completed_collision,
        completed,
    };
    if (!expect_preserved(
            coordinator.retry_standalone({
                completed.runtime_generation,
                1U,
            }),
            CommandDisposition::stale_sequence,
            "stale retry",
            completed_witness
        )
        || !expect_preserved(
            coordinator.retry_standalone({
                completed.runtime_generation,
                3U,
            }),
            CommandDisposition::out_of_order_sequence,
            "out-of-order retry",
            completed_witness
        )) {
        return false;
    }

    const auto retry = coordinator.retry_standalone({
        completed.runtime_generation,
        2U,
    });
    if (retry.disposition != CommandDisposition::applied
        || retry.session_disposition
            != gameplay::SandboxSessionRetryDisposition::restored
        || retry.snapshot.runtime_generation != completed.runtime_generation + 1U
        || retry.snapshot.session.generation != completed.session.generation + 1U
        || retry.snapshot.authoritative_tick != 0U
        || retry.snapshot.movement_sequence != 0U) {
        std::cerr << "standalone retry identity did not advance atomically\n";
        return false;
    }
    const auto retry_record = coordinator.collision_at(0);
    const auto* retry_world = coordinator.collision_world(region);
    if (!retry_record.has_value() || !retry_record->enabled
        || retry_world == nullptr
        || retry_world->blocker_enabled(retry_record->shape_id)
            != std::optional<bool>{true}
        || coordinator.snapshot().session.player_pose != safe_point->pose
        || coordinator.snapshot().session.player_pose
            != contracts::GroundPoseMm{-1250, -3000, 0, 0}
        || coordinator.snapshot().session.player_facing_millidegrees
            != safe_point->facing_millidegrees) {
        std::cerr << "retry did not restore safe point and closed collision\n";
        return false;
    }
    const auto retry_snapshot = coordinator.snapshot();
    const auto retry_witness = capture();
    const auto safe_point_rejected = coordinator.submit_operate({
        retry_snapshot.runtime_generation,
        1U,
        actor,
        interaction,
    });
    if (safe_point_rejected.dispatch.result.disposition
            != contracts::SandboxOperateDisposition::out_of_range
        || !expect_preserved(
            safe_point_rejected,
            CommandDisposition::session_rejected,
            "safe-point out-of-range operate",
            retry_witness
        )
        || !expect_preserved(
            coordinator.advance_player({
                completed.runtime_generation,
                1U,
                1U,
                actor,
                retry_snapshot.session.player_pose.floor_layer,
                -250,
                0,
            }),
            CommandDisposition::stale_generation,
            "old-generation movement",
            retry_witness
        )
        || !expect_preserved(
            coordinator.submit_operate({
                completed.runtime_generation,
                1U,
                actor,
                interaction,
            }),
            CommandDisposition::stale_generation,
            "old-generation operate",
            retry_witness
        )
        || !expect_preserved(
            coordinator.retry_standalone({
                completed.runtime_generation,
                1U,
            }),
            CommandDisposition::stale_generation,
            "old-generation retry",
            retry_witness
        )) {
        return false;
    }

    for (const auto [dx, dy] : std::array{
             std::pair{-250, 0},
             std::pair{0, 500},
             std::pair{0, 500},
             std::pair{0, 500},
         }) {
        const auto result = move(dx, dy);
        if (result.disposition != CommandDisposition::applied) {
            std::cerr << "post-retry movement failed\n";
            return false;
        }
    }
    if (coordinator.snapshot().session.player_pose
        != contracts::GroundPoseMm{-1500, -1500, 0, 0}) {
        std::cerr << "post-retry route missed the authored proof point\n";
        return false;
    }
    const auto* restored_world = coordinator.collision_world(region);
    if (restored_world == nullptr
        || !restored_world->resolve_ground_move(
                gate_approach,
                0,
                500,
                retry_snapshot.player_config.collision_radius_mm,
                retry_snapshot.player_config.collision_height_mm
            ).blocked_y) {
        std::cerr << "retry did not restore the closed gate\n";
        return false;
    }
    const auto second_before = coordinator.snapshot();
    const auto second_operate = coordinator.submit_operate({
        second_before.runtime_generation,
        1U,
        actor,
        interaction,
    });
    if (second_operate.disposition != CommandDisposition::applied
        || second_operate.snapshot.session.last_event_sequence
            <= completed.session.last_event_sequence) {
        std::cerr << "post-retry chain could not complete exactly once\n";
        return false;
    }
    for (int step = 0; step < 3; ++step) {
        if (move(0, 500).disposition != CommandDisposition::applied) {
            std::cerr << "post-retry opened route to gate failed\n";
            return false;
        }
    }
    if (move(0, 500).disposition != CommandDisposition::applied) {
        std::cerr << "post-retry opened gate remained solid\n";
        return false;
    }

    std::cout << "sandbox runtime coordinator trace "
              << std::hex << coordinator.snapshot().checksum << std::dec << '\n';
    return true;
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
