#include <tgd/integration/sandbox_runtime_coordinator.hpp>

#include <tgd/content/sandbox_package.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/integration/sandbox_session_adapter.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace tgd::integration {
namespace {

struct AssetSetCandidate final {
    std::array<SandboxOwnedStableAsset, contracts::sandbox_asset_capacity> records{};
    std::size_t count{};
};

struct RegionCollisionCandidate final {
    contracts::StableContentKey region_key{};
    runtime::StaticCollisionWorld world{};
};

struct CollisionCandidate final {
    std::array<RegionCollisionCandidate, contracts::sandbox_region_capacity> regions{};
    std::size_t region_count{};
    std::array<SandboxStaticCollisionRecord, contracts::sandbox_ground_blocker_capacity> records{};
    std::size_t record_count{};
};

[[nodiscard]] bool checksum_is_nonzero(const contracts::Sha256Digest& checksum) noexcept {
    return std::any_of(
        checksum.begin(),
        checksum.end(),
        [](std::uint8_t value) noexcept { return value != 0; }
    );
}

[[nodiscard]] bool valid_asset_kind(contracts::SandboxAssetKind kind) noexcept {
    using Kind = contracts::SandboxAssetKind;
    switch (kind) {
        case Kind::player:
        case Kind::actor:
        case Kind::obstacle:
        case Kind::interaction:
        case Kind::mechanism:
        case Kind::safe_point:
        case Kind::effect:
            return true;
    }
    return false;
}

[[nodiscard]] SandboxAssetSetBuildError prepare_asset_set(
    const contracts::SandboxDefinition& definition,
    AssetSetCandidate& candidate
) noexcept {
    using Error = SandboxAssetSetBuildError;
    if (definition.assets.size() > candidate.records.size()) {
        return Error::capacity_exceeded;
    }

    for (const auto& source : definition.assets) {
        if (source.id.key == 0 || source.id.name.empty() ||
            source.id.name.size() > contracts::sandbox_pack_max_id_bytes ||
            contracts::content_id(source.id.name).key != source.id.key) {
            return Error::invalid_content_id;
        }
        if (!valid_asset_kind(source.kind)) {
            return Error::invalid_kind;
        }

        auto& destination = candidate.records[candidate.count++];
        destination.key = source.id.key;
        destination.id_byte_count = static_cast<std::uint16_t>(source.id.name.size());
        std::copy(source.id.name.begin(), source.id.name.end(), destination.id_bytes.begin());
        destination.kind = source.kind;
    }

    std::sort(
        candidate.records.begin(),
        candidate.records.begin() + static_cast<std::ptrdiff_t>(candidate.count),
        [](const SandboxOwnedStableAsset& left, const SandboxOwnedStableAsset& right) noexcept {
            return left.key < right.key;
        }
    );
    for (std::size_t index = 1; index < candidate.count; ++index) {
        if (candidate.records[index - 1].key == candidate.records[index].key) {
            return Error::duplicate_content_id;
        }
    }
    return Error::none;
}

[[nodiscard]] runtime::CollisionWorldError prepare_collision(
    const contracts::SandboxDefinition& definition,
    CollisionCandidate& candidate
) noexcept {
    using Error = runtime::CollisionWorldError;
    if (definition.regions.size() > candidate.regions.size() ||
        definition.ground_blockers.size() > candidate.records.size()) {
        return Error::too_many_blockers;
    }

    for (const auto& region : definition.regions) {
        if (region.id.key == 0) {
            return Error::invalid_blocker;
        }
        candidate.regions[candidate.region_count++].region_key = region.id.key;
    }
    std::sort(
        candidate.regions.begin(),
        candidate.regions.begin() + static_cast<std::ptrdiff_t>(candidate.region_count),
        [](const RegionCollisionCandidate& left, const RegionCollisionCandidate& right) noexcept {
            return left.region_key < right.region_key;
        }
    );

    for (const auto& blocker : definition.ground_blockers) {
        if (blocker.id.key == 0 || blocker.region_id.key == 0) {
            return Error::invalid_blocker;
        }
        auto& record = candidate.records[candidate.record_count++];
        record.blocker_key = blocker.id.key;
        record.region_key = blocker.region_id.key;
        record.min_x = blocker.min_x;
        record.max_x = blocker.max_x;
        record.min_y = blocker.min_y;
        record.max_y = blocker.max_y;
        record.min_height = blocker.min_height;
        record.max_height = blocker.max_height;
        record.floor_layer = blocker.floor_layer;
    }
    std::sort(
        candidate.records.begin(),
        candidate.records.begin() + static_cast<std::ptrdiff_t>(candidate.record_count),
        [](const SandboxStaticCollisionRecord& left,
           const SandboxStaticCollisionRecord& right) noexcept {
            return left.blocker_key < right.blocker_key;
        }
    );
    for (std::size_t index = 0; index < candidate.record_count; ++index) {
        if (index != 0 &&
            candidate.records[index - 1].blocker_key ==
                candidate.records[index].blocker_key) {
            return Error::duplicate_shape_id;
        }
        candidate.records[index].shape_id =
            static_cast<contracts::CollisionShapeId>(index + 1U);
    }

    for (std::size_t region_index = 0;
         region_index < candidate.region_count;
         ++region_index) {
        std::array<runtime::GroundBlocker, runtime::StaticCollisionWorld::max_blockers>
            blockers{};
        std::size_t blocker_count{};
        for (const auto& record :
             std::span{candidate.records}.first(candidate.record_count)) {
            if (record.region_key != candidate.regions[region_index].region_key) {
                continue;
            }
            blockers[blocker_count++] = {
                record.shape_id,
                record.min_x,
                record.max_x,
                record.min_y,
                record.max_y,
                record.min_height,
                record.max_height,
                record.floor_layer,
            };
        }
        const auto error = candidate.regions[region_index].world.configure(
            std::span{blockers}.first(blocker_count)
        );
        if (error != Error::none) {
            return error;
        }
    }

    for (const auto& record : std::span{candidate.records}.first(candidate.record_count)) {
        const auto owner = std::find_if(
            candidate.regions.begin(),
            candidate.regions.begin() +
                static_cast<std::ptrdiff_t>(candidate.region_count),
            [&](const RegionCollisionCandidate& region) noexcept {
                return region.region_key == record.region_key;
            }
        );
        if (owner == candidate.regions.begin() +
                static_cast<std::ptrdiff_t>(candidate.region_count)) {
            return Error::invalid_blocker;
        }
    }
    return Error::none;
}

}  // namespace

struct SandboxRuntimeCoordinator::LiveAggregate final {
    content::SandboxPackagePublicationIdentity package_identity{};
    std::uint32_t runtime_generation{};
    std::vector<std::uint8_t> canonical_bytes{};
    std::unique_ptr<content::SandboxPackageDocument> document{};
    gameplay::SandboxSession session{};
    CollisionCandidate collision{};
    AssetSetCandidate assets{};
    SandboxRuntimeSnapshot snapshot{};
};

SandboxRuntimeCoordinator::SandboxRuntimeCoordinator() noexcept = default;
SandboxRuntimeCoordinator::~SandboxRuntimeCoordinator() = default;

SandboxRuntimePublishResult SandboxRuntimeCoordinator::publish(
    SandboxPublishedPackageArtifact artifact,
    const gameplay::SandboxPlayerRuntimeBinding& player_binding
) noexcept {
    static_assert(noexcept(live_.swap(live_)));
    static_assert(std::is_nothrow_destructible_v<LiveAggregate>);

    const auto result_for = [&](SandboxRuntimePublishDisposition disposition) noexcept {
        SandboxRuntimePublishResult result{};
        result.disposition = disposition;
        result.snapshot = snapshot();
        return result;
    };

    if (artifact.identity.generation() == 0 ||
        !checksum_is_nonzero(artifact.identity.checksum()) ||
        artifact.canonical_bytes.empty() ||
        artifact.canonical_bytes.size() > contracts::sandbox_pack_max_bytes) {
        return result_for(SandboxRuntimePublishDisposition::invalid_artifact);
    }

    if (live_ != nullptr) {
        if (artifact.identity.generation() < live_->package_identity.generation()) {
            return result_for(SandboxRuntimePublishDisposition::stale_generation);
        }
        if (artifact.identity.generation() == live_->package_identity.generation() &&
            artifact.identity.checksum() != live_->package_identity.checksum()) {
            return result_for(SandboxRuntimePublishDisposition::identity_conflict);
        }
    }

    auto decoded = content::decode_sandbox_package(artifact.canonical_bytes);
    if (!decoded.validation.valid() || decoded.document == nullptr) {
        return result_for(SandboxRuntimePublishDisposition::decode_failed);
    }
    if (decoded.document->fingerprint() != artifact.identity.checksum()) {
        return result_for(SandboxRuntimePublishDisposition::fingerprint_mismatch);
    }

    if (live_ != nullptr &&
        artifact.identity.checksum() == live_->package_identity.checksum()) {
        return result_for(SandboxRuntimePublishDisposition::unchanged);
    }

    const auto next_generation = sandbox_next_runtime_generation(
        live_ == nullptr ? 0U : live_->runtime_generation
    );
    if (!next_generation.valid) {
        return result_for(SandboxRuntimePublishDisposition::generation_exhausted);
    }

    auto built = build_sandbox_session_blueprint(*decoded.document);
    const auto* blueprint = built.blueprint();
    if (!built.succeeded() || blueprint == nullptr) {
        return result_for(SandboxRuntimePublishDisposition::blueprint_prepare_failed);
    }

    try {
        auto candidate = std::make_unique<LiveAggregate>();
        candidate->package_identity = artifact.identity;
        candidate->runtime_generation = next_generation.generation;
        candidate->canonical_bytes = std::move(artifact.canonical_bytes);
        candidate->document = std::move(decoded.document);

        const auto session_result = initialize_sandbox_session_from_blueprint(
            candidate->session,
            *blueprint,
            player_binding
        );
        if (session_result.error != gameplay::SandboxSessionBuildError::none) {
            return result_for(SandboxRuntimePublishDisposition::session_prepare_failed);
        }

        const auto collision_error = prepare_collision(
            candidate->document->definition(),
            candidate->collision
        );
        if (collision_error != runtime::CollisionWorldError::none) {
            auto result = result_for(
                SandboxRuntimePublishDisposition::collision_prepare_failed
            );
            result.collision_error = collision_error;
            return result;
        }

        const auto asset_error = prepare_asset_set(
            candidate->document->definition(),
            candidate->assets
        );
        if (asset_error != SandboxAssetSetBuildError::none) {
            auto result = result_for(
                SandboxRuntimePublishDisposition::asset_set_prepare_failed
            );
            result.asset_error = asset_error;
            return result;
        }

        candidate->snapshot.initialized = true;
        candidate->snapshot.runtime_generation = candidate->runtime_generation;
        candidate->snapshot.package_generation =
            candidate->package_identity.generation();
        candidate->snapshot.package_checksum =
            candidate->package_identity.checksum();
        candidate->snapshot.canonical_byte_count =
            static_cast<std::uint32_t>(candidate->canonical_bytes.size());
        candidate->snapshot.session = candidate->session.snapshot();
        candidate->snapshot.collision_region_count =
            static_cast<std::uint16_t>(candidate->collision.region_count);
        candidate->snapshot.collision_record_count =
            static_cast<std::uint16_t>(candidate->collision.record_count);
        candidate->snapshot.asset_count =
            static_cast<std::uint16_t>(candidate->assets.count);

        const auto published_snapshot = candidate->snapshot;
        live_.swap(candidate);

        SandboxRuntimePublishResult result{};
        result.disposition = SandboxRuntimePublishDisposition::published;
        result.snapshot = published_snapshot;
        return result;
    } catch (const std::bad_alloc&) {
        return result_for(SandboxRuntimePublishDisposition::allocation_failed);
    }
}

SandboxRuntimeSnapshot SandboxRuntimeCoordinator::snapshot() const noexcept {
    return live_ == nullptr ? SandboxRuntimeSnapshot{} : live_->snapshot;
}

const content::SandboxPackageDocument* SandboxRuntimeCoordinator::document() const noexcept {
    return live_ == nullptr ? nullptr : live_->document.get();
}

const gameplay::SandboxSession* SandboxRuntimeCoordinator::session() const noexcept {
    return live_ == nullptr ? nullptr : &live_->session;
}

const runtime::StaticCollisionWorld* SandboxRuntimeCoordinator::collision_world(
    contracts::StableContentKey region_key
) const noexcept {
    if (live_ == nullptr || region_key == 0) {
        return nullptr;
    }
    const auto& collision = live_->collision;
    const auto found = std::find_if(
        collision.regions.begin(),
        collision.regions.begin() +
            static_cast<std::ptrdiff_t>(collision.region_count),
        [&](const RegionCollisionCandidate& region) noexcept {
            return region.region_key == region_key;
        }
    );
    return found == collision.regions.begin() +
            static_cast<std::ptrdiff_t>(collision.region_count)
        ? nullptr
        : &found->world;
}

std::optional<SandboxStaticCollisionRecord> SandboxRuntimeCoordinator::collision_at(
    std::size_t index
) const noexcept {
    if (live_ == nullptr || index >= live_->collision.record_count) {
        return std::nullopt;
    }
    return live_->collision.records[index];
}

std::optional<SandboxOwnedStableAsset> SandboxRuntimeCoordinator::asset_at(
    std::size_t index
) const noexcept {
    if (live_ == nullptr || index >= live_->assets.count) {
        return std::nullopt;
    }
    return live_->assets.records[index];
}

}  // namespace tgd::integration
