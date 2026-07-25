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
        auto& destination = candidate.regions[candidate.region_count++];
        destination.region_key = region.id.key;
        const auto bounds_error = destination.world.configure_bounds({
            region.bounds.min_x,
            region.bounds.max_x,
            region.bounds.min_y,
            region.bounds.max_y,
            region.bounds.min_height,
            region.bounds.max_height,
            region.bounds.min_floor_layer,
            region.bounds.max_floor_layer,
        });
        if (bounds_error != Error::none) {
            return bounds_error;
        }
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
        record.enabled = true;
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
                record.enabled,
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

struct RuntimeLiveState final {
    gameplay::SandboxSession session{};
    CollisionCandidate collision{};
    SandboxRuntimeSnapshot snapshot{};
};

struct SandboxRuntimeCoordinator::LiveAggregate final {
    content::SandboxPackagePublicationIdentity package_identity{};
    std::vector<std::uint8_t> canonical_bytes{};
    std::unique_ptr<content::SandboxPackageDocument> document{};
    AssetSetCandidate assets{};
    std::unique_ptr<RuntimeLiveState> state{};
};

namespace {

constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;

void hash_u64(
    std::uint64_t& hash,
    std::uint64_t value,
    const std::size_t byte_count
) noexcept {
    for (std::size_t index = 0; index < byte_count; ++index) {
        hash ^= value & 0xffU;
        hash *= fnv_prime;
        value >>= 8U;
    }
}

void hash_digest(
    std::uint64_t& hash,
    const contracts::Sha256Digest& digest
) noexcept {
    for (const auto byte : digest) {
        hash_u64(hash, byte, sizeof(byte));
    }
}

[[nodiscard]] RegionCollisionCandidate* find_region(
    CollisionCandidate& collision,
    const contracts::StableContentKey region_key
) noexcept {
    const auto end = collision.regions.begin()
        + static_cast<std::ptrdiff_t>(collision.region_count);
    const auto found = std::find_if(
        collision.regions.begin(),
        end,
        [&](const RegionCollisionCandidate& region) noexcept {
            return region.region_key == region_key;
        }
    );
    return found == end ? nullptr : &*found;
}

[[nodiscard]] const RegionCollisionCandidate* find_region(
    const CollisionCandidate& collision,
    const contracts::StableContentKey region_key
) noexcept {
    const auto end = collision.regions.begin()
        + static_cast<std::ptrdiff_t>(collision.region_count);
    const auto found = std::find_if(
        collision.regions.begin(),
        end,
        [&](const RegionCollisionCandidate& region) noexcept {
            return region.region_key == region_key;
        }
    );
    return found == end ? nullptr : &*found;
}

[[nodiscard]] bool project_blocker_state(RuntimeLiveState& state) noexcept {
    using BlockerState = contracts::SandboxGroundBlockerState;
    for (std::size_t index = 0; index < state.collision.record_count; ++index) {
        auto& record = state.collision.records[index];
        const auto source = state.session.ground_blocker_state(record.blocker_key);
        bool enabled{};
        if (source == BlockerState::enabled_solid) {
            enabled = true;
        } else if (source == BlockerState::disabled_non_solid) {
            enabled = false;
        } else {
            return false;
        }

        auto* region = find_region(state.collision, record.region_key);
        if (region == nullptr
            || !region->world.set_blocker_enabled(record.shape_id, enabled)) {
            return false;
        }
        record.enabled = enabled;
    }
    return true;
}

[[nodiscard]] std::uint64_t compute_runtime_checksum(
    const RuntimeLiveState& state
) noexcept {
    std::uint64_t hash = fnv_offset_basis;
    const auto& snapshot = state.snapshot;
    hash_u64(hash, snapshot.initialized ? 1U : 0U, 1U);
    hash_u64(hash, snapshot.runtime_generation, sizeof(snapshot.runtime_generation));
    hash_u64(hash, snapshot.package_generation, sizeof(snapshot.package_generation));
    hash_digest(hash, snapshot.package_checksum);
    hash_u64(
        hash,
        snapshot.canonical_byte_count,
        sizeof(snapshot.canonical_byte_count)
    );
    hash_u64(hash, snapshot.session.checksum, sizeof(snapshot.session.checksum));
    hash_u64(
        hash,
        snapshot.collision_region_count,
        sizeof(snapshot.collision_region_count)
    );
    hash_u64(
        hash,
        snapshot.collision_record_count,
        sizeof(snapshot.collision_record_count)
    );
    for (std::size_t index = 0; index < state.collision.record_count; ++index) {
        const auto& record = state.collision.records[index];
        hash_u64(hash, record.blocker_key, sizeof(record.blocker_key));
        hash_u64(hash, record.region_key, sizeof(record.region_key));
        hash_u64(hash, record.shape_id, sizeof(record.shape_id));
        hash_u64(hash, record.enabled ? 1U : 0U, 1U);
    }
    hash_u64(hash, snapshot.asset_count, sizeof(snapshot.asset_count));
    hash_u64(
        hash,
        static_cast<std::uint32_t>(snapshot.player_config.max_move_delta_mm),
        sizeof(snapshot.player_config.max_move_delta_mm)
    );
    hash_u64(
        hash,
        static_cast<std::uint32_t>(snapshot.player_config.collision_radius_mm),
        sizeof(snapshot.player_config.collision_radius_mm)
    );
    hash_u64(
        hash,
        static_cast<std::uint32_t>(snapshot.player_config.collision_height_mm),
        sizeof(snapshot.player_config.collision_height_mm)
    );
    hash_u64(
        hash,
        snapshot.authoritative_tick,
        sizeof(snapshot.authoritative_tick)
    );
    hash_u64(
        hash,
        snapshot.movement_sequence,
        sizeof(snapshot.movement_sequence)
    );
    return hash;
}

void refresh_runtime_snapshot(
    RuntimeLiveState& state,
    const content::SandboxPackagePublicationIdentity& identity,
    const std::size_t canonical_byte_count,
    const std::size_t asset_count
) noexcept {
    state.snapshot.initialized = true;
    state.snapshot.package_generation = identity.generation();
    state.snapshot.package_checksum = identity.checksum();
    state.snapshot.canonical_byte_count =
        static_cast<std::uint32_t>(canonical_byte_count);
    state.snapshot.session = state.session.snapshot();
    state.snapshot.collision_region_count =
        static_cast<std::uint16_t>(state.collision.region_count);
    state.snapshot.collision_record_count =
        static_cast<std::uint16_t>(state.collision.record_count);
    state.snapshot.asset_count = static_cast<std::uint16_t>(asset_count);
    state.snapshot.checksum = 0;
    state.snapshot.checksum = compute_runtime_checksum(state);
}

[[nodiscard]] bool valid_runtime_state(
    const RuntimeLiveState& state,
    const content::SandboxPackagePublicationIdentity& identity,
    const std::size_t canonical_byte_count,
    const std::size_t asset_count
) noexcept {
    if (!state.snapshot.initialized
        || state.snapshot.runtime_generation == 0
        || state.snapshot.package_generation != identity.generation()
        || state.snapshot.package_checksum != identity.checksum()
        || state.snapshot.canonical_byte_count != canonical_byte_count
        || state.snapshot.session != state.session.snapshot()
        || state.snapshot.collision_region_count != state.collision.region_count
        || state.snapshot.collision_record_count != state.collision.record_count
        || state.snapshot.asset_count != asset_count
        || !gameplay::validate_sandbox_player_movement_config(
            state.snapshot.player_config
        )
        || state.snapshot.checksum != compute_runtime_checksum(state)) {
        return false;
    }

    using BlockerState = contracts::SandboxGroundBlockerState;
    for (std::size_t index = 0; index < state.collision.record_count; ++index) {
        const auto& record = state.collision.records[index];
        const auto source = state.session.ground_blocker_state(record.blocker_key);
        const auto expected = source == BlockerState::enabled_solid;
        if ((source != BlockerState::enabled_solid
             && source != BlockerState::disabled_non_solid)
            || record.enabled != expected) {
            return false;
        }
        const auto* region = find_region(state.collision, record.region_key);
        if (region == nullptr
            || region->world.blocker_enabled(record.shape_id)
                != std::optional<bool>{expected}) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] SandboxRuntimeCommandDisposition validate_sequence(
    const contracts::CommandSequence requested,
    const contracts::CommandSequence committed
) noexcept {
    if (requested <= committed) {
        return SandboxRuntimeCommandDisposition::stale_sequence;
    }
    if (committed == std::numeric_limits<contracts::CommandSequence>::max()
        || requested != committed + 1U) {
        return SandboxRuntimeCommandDisposition::out_of_order_sequence;
    }
    return SandboxRuntimeCommandDisposition::applied;
}

}  // namespace

SandboxRuntimeCoordinator::SandboxRuntimeCoordinator() noexcept = default;
SandboxRuntimeCoordinator::~SandboxRuntimeCoordinator() = default;

SandboxRuntimePublishResult SandboxRuntimeCoordinator::publish(
    SandboxPublishedPackageArtifact artifact,
    const gameplay::SandboxPlayerRuntimeBinding& player_binding,
    const SandboxThinRuntimePlayerConfig& player_config
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
    const auto player_binding_error =
        gameplay::validate_sandbox_player_runtime_binding(
            decoded.document->definition().player,
            player_binding
        );
    if (player_binding_error != gameplay::SandboxSessionBuildError::none) {
        return result_for(SandboxRuntimePublishDisposition::session_prepare_failed);
    }
    if (!gameplay::validate_sandbox_player_movement_config(
            decoded.document->definition(),
            player_config
        )) {
        return result_for(
            SandboxRuntimePublishDisposition::invalid_player_runtime_config
        );
    }

    if (live_ != nullptr &&
        artifact.identity.checksum() == live_->package_identity.checksum()) {
        if (player_binding.actor_key != live_->state->snapshot.session.player_actor
            || player_config != live_->state->snapshot.player_config) {
            return result_for(SandboxRuntimePublishDisposition::identity_conflict);
        }
        return result_for(SandboxRuntimePublishDisposition::unchanged);
    }

    const auto next_generation = sandbox_next_runtime_generation(
        live_ == nullptr ? 0U : live_->state->snapshot.runtime_generation
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
        candidate->canonical_bytes = std::move(artifact.canonical_bytes);
        candidate->document = std::move(decoded.document);
        candidate->state = std::make_unique<RuntimeLiveState>();
        candidate->state->snapshot.runtime_generation = next_generation.generation;
        candidate->state->snapshot.player_config = player_config;

        const auto session_result = initialize_sandbox_session_from_blueprint(
            candidate->state->session,
            *blueprint,
            player_binding
        );
        if (session_result.error != gameplay::SandboxSessionBuildError::none) {
            return result_for(SandboxRuntimePublishDisposition::session_prepare_failed);
        }

        const auto collision_error = prepare_collision(
            candidate->document->definition(),
            candidate->state->collision
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

        if (!project_blocker_state(*candidate->state)) {
            return result_for(SandboxRuntimePublishDisposition::collision_prepare_failed);
        }
        refresh_runtime_snapshot(
            *candidate->state,
            candidate->package_identity,
            candidate->canonical_bytes.size(),
            candidate->assets.count
        );
        if (!valid_runtime_state(
                *candidate->state,
                candidate->package_identity,
                candidate->canonical_bytes.size(),
                candidate->assets.count
            )) {
            return result_for(SandboxRuntimePublishDisposition::collision_prepare_failed);
        }

        const auto published_snapshot = candidate->state->snapshot;
        live_.swap(candidate);

        SandboxRuntimePublishResult result{};
        result.disposition = SandboxRuntimePublishDisposition::published;
        result.snapshot = published_snapshot;
        return result;
    } catch (const std::bad_alloc&) {
        return result_for(SandboxRuntimePublishDisposition::allocation_failed);
    }
}

SandboxRuntimeMoveResult SandboxRuntimeCoordinator::advance_player(
    const SandboxRuntimeMoveCommand& command
) noexcept {
    static_assert(noexcept(live_->state.swap(live_->state)));
    static_assert(std::is_nothrow_move_assignable_v<gameplay::SandboxSession>);

    const auto result_for = [&](
        const SandboxRuntimeCommandDisposition disposition,
        const runtime::GroundMoveResolution resolution =
            runtime::GroundMoveResolution{}
    ) noexcept {
        return SandboxRuntimeMoveResult{disposition, resolution, snapshot()};
    };
    if (live_ == nullptr || live_->state == nullptr || live_->document == nullptr) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    auto& current = *live_->state;
    if (!valid_runtime_state(
            current,
            live_->package_identity,
            live_->canonical_bytes.size(),
            live_->assets.count
        )) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    if (command.runtime_generation != current.snapshot.runtime_generation) {
        return result_for(SandboxRuntimeCommandDisposition::stale_generation);
    }
    const auto sequence = validate_sequence(
        command.sequence,
        current.snapshot.movement_sequence
    );
    if (sequence != SandboxRuntimeCommandDisposition::applied) {
        return result_for(sequence);
    }
    if (current.snapshot.authoritative_tick
            == std::numeric_limits<contracts::TickIndex>::max()
        || command.tick != current.snapshot.authoritative_tick + 1U) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_tick);
    }
    const auto player_region =
        live_->document->definition().player.region_id.key;
    const auto* region = find_region(current.collision, player_region);
    if (region == nullptr) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    try {
        auto candidate = std::make_unique<RuntimeLiveState>(current);
        const auto move_result = candidate->session.move_player_relative(
            {
                command.actor,
                command.floor_layer,
                command.delta_x_mm,
                command.delta_y_mm,
            },
            candidate->snapshot.player_config,
            region->world
        );
        switch (move_result.disposition) {
            case gameplay::SandboxPlayerRelativeMoveDisposition::moved:
                break;
            case gameplay::SandboxPlayerRelativeMoveDisposition::invalid_actor:
                return result_for(
                    SandboxRuntimeCommandDisposition::invalid_actor,
                    move_result.resolution
                );
            case gameplay::SandboxPlayerRelativeMoveDisposition::floor_mismatch:
                return result_for(
                    SandboxRuntimeCommandDisposition::floor_mismatch,
                    move_result.resolution
                );
            case gameplay::SandboxPlayerRelativeMoveDisposition::invalid_delta:
                return result_for(
                    SandboxRuntimeCommandDisposition::invalid_delta,
                    move_result.resolution
                );
            case gameplay::SandboxPlayerRelativeMoveDisposition::collision_blocked:
                return result_for(
                    SandboxRuntimeCommandDisposition::collision_blocked,
                    move_result.resolution
                );
            case gameplay::SandboxPlayerRelativeMoveDisposition::invalid_config:
            case gameplay::SandboxPlayerRelativeMoveDisposition::invalid_state:
            case gameplay::SandboxPlayerRelativeMoveDisposition::invalid:
                return result_for(
                    SandboxRuntimeCommandDisposition::invalid_state,
                    move_result.resolution
                );
        }
        candidate->snapshot.authoritative_tick = command.tick;
        candidate->snapshot.movement_sequence = command.sequence;
        refresh_runtime_snapshot(
            *candidate,
            live_->package_identity,
            live_->canonical_bytes.size(),
            live_->assets.count
        );
        if (!valid_runtime_state(
                *candidate,
                live_->package_identity,
                live_->canonical_bytes.size(),
                live_->assets.count
            )) {
            return result_for(SandboxRuntimeCommandDisposition::invalid_state);
        }
        const auto published = candidate->snapshot;
        live_->state.swap(candidate);
        return {
            SandboxRuntimeCommandDisposition::applied,
            move_result.resolution,
            published,
        };
    } catch (const std::bad_alloc&) {
        return result_for(SandboxRuntimeCommandDisposition::allocation_failed);
    }
}

SandboxRuntimeOperateResult SandboxRuntimeCoordinator::submit_operate(
    const SandboxRuntimeOperateCommand& command
) noexcept {
    const auto result_for = [&](
        const SandboxRuntimeCommandDisposition disposition,
        const gameplay::SandboxOperateDispatch dispatch =
            gameplay::SandboxOperateDispatch{}
    ) noexcept {
        return SandboxRuntimeOperateResult{disposition, dispatch, snapshot()};
    };
    if (live_ == nullptr || live_->state == nullptr) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    auto& current = *live_->state;
    if (!valid_runtime_state(
            current,
            live_->package_identity,
            live_->canonical_bytes.size(),
            live_->assets.count
        )) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    if (command.runtime_generation != current.snapshot.runtime_generation) {
        return result_for(SandboxRuntimeCommandDisposition::stale_generation);
    }
    const auto sequence = validate_sequence(
        command.sequence,
        current.snapshot.session.last_command_sequence
    );
    if (sequence != SandboxRuntimeCommandDisposition::applied) {
        return result_for(sequence);
    }
    if (command.actor == 0 || command.actor != current.snapshot.session.player_actor) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_actor);
    }

    try {
        auto candidate = std::make_unique<RuntimeLiveState>(current);
        const auto dispatch = candidate->session.submit_operate({
            candidate->session.snapshot().generation,
            candidate->snapshot.authoritative_tick,
            command.actor,
            command.sequence,
            command.interaction,
        });
        using OperateDisposition = contracts::SandboxOperateDisposition;
        if (dispatch.result.disposition == OperateDisposition::repeated_chain) {
            return result_for(SandboxRuntimeCommandDisposition::repeated, dispatch);
        }
        if (dispatch.result.disposition != OperateDisposition::completed_chain) {
            return result_for(
                SandboxRuntimeCommandDisposition::session_rejected,
                dispatch
            );
        }
        if (!project_blocker_state(*candidate)) {
            return result_for(SandboxRuntimeCommandDisposition::invalid_state);
        }
        refresh_runtime_snapshot(
            *candidate,
            live_->package_identity,
            live_->canonical_bytes.size(),
            live_->assets.count
        );
        if (!valid_runtime_state(
                *candidate,
                live_->package_identity,
                live_->canonical_bytes.size(),
                live_->assets.count
            )) {
            return result_for(SandboxRuntimeCommandDisposition::invalid_state);
        }
        const auto published = candidate->snapshot;
        live_->state.swap(candidate);
        return {SandboxRuntimeCommandDisposition::applied, dispatch, published};
    } catch (const std::bad_alloc&) {
        return result_for(SandboxRuntimeCommandDisposition::allocation_failed);
    }
}

SandboxRuntimeRetryResult SandboxRuntimeCoordinator::retry_standalone(
    const SandboxRuntimeRetryCommand& command
) noexcept {
    const auto result_for = [&](
        const SandboxRuntimeCommandDisposition disposition,
        const gameplay::SandboxSessionRetryDisposition session_disposition =
            gameplay::SandboxSessionRetryDisposition::invalid
    ) noexcept {
        return SandboxRuntimeRetryResult{
            disposition,
            session_disposition,
            snapshot(),
        };
    };
    if (live_ == nullptr || live_->state == nullptr) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    auto& current = *live_->state;
    if (!valid_runtime_state(
            current,
            live_->package_identity,
            live_->canonical_bytes.size(),
            live_->assets.count
        )) {
        return result_for(SandboxRuntimeCommandDisposition::invalid_state);
    }
    if (command.runtime_generation != current.snapshot.runtime_generation) {
        return result_for(SandboxRuntimeCommandDisposition::stale_generation);
    }
    const auto sequence = validate_sequence(
        command.sequence,
        current.snapshot.session.last_command_sequence
    );
    if (sequence != SandboxRuntimeCommandDisposition::applied) {
        return result_for(sequence);
    }
    const auto next_generation = sandbox_next_runtime_generation(
        current.snapshot.runtime_generation
    );
    if (!next_generation.valid) {
        return result_for(SandboxRuntimeCommandDisposition::generation_exhausted);
    }

    try {
        auto candidate = std::make_unique<RuntimeLiveState>(current);
        const auto session_disposition = candidate->session.retry({
            candidate->session.snapshot().generation,
            command.sequence,
        });
        if (session_disposition
            != gameplay::SandboxSessionRetryDisposition::restored) {
            const auto disposition = session_disposition
                    == gameplay::SandboxSessionRetryDisposition::generation_exhausted
                ? SandboxRuntimeCommandDisposition::generation_exhausted
                : SandboxRuntimeCommandDisposition::invalid_state;
            return result_for(disposition, session_disposition);
        }
        if (!project_blocker_state(*candidate)) {
            return result_for(SandboxRuntimeCommandDisposition::invalid_state);
        }
        candidate->snapshot.runtime_generation = next_generation.generation;
        candidate->snapshot.authoritative_tick = 0;
        candidate->snapshot.movement_sequence = 0;
        refresh_runtime_snapshot(
            *candidate,
            live_->package_identity,
            live_->canonical_bytes.size(),
            live_->assets.count
        );
        if (!valid_runtime_state(
                *candidate,
                live_->package_identity,
                live_->canonical_bytes.size(),
                live_->assets.count
            )) {
            return result_for(SandboxRuntimeCommandDisposition::invalid_state);
        }
        const auto published = candidate->snapshot;
        live_->state.swap(candidate);
        return {
            SandboxRuntimeCommandDisposition::applied,
            session_disposition,
            published,
        };
    } catch (const std::bad_alloc&) {
        return result_for(SandboxRuntimeCommandDisposition::allocation_failed);
    }
}

SandboxRuntimeSnapshot SandboxRuntimeCoordinator::snapshot() const noexcept {
    return live_ == nullptr ? SandboxRuntimeSnapshot{} : live_->state->snapshot;
}

const content::SandboxPackageDocument* SandboxRuntimeCoordinator::document() const noexcept {
    return live_ == nullptr ? nullptr : live_->document.get();
}

const gameplay::SandboxSession* SandboxRuntimeCoordinator::session() const noexcept {
    return live_ == nullptr ? nullptr : &live_->state->session;
}

const runtime::StaticCollisionWorld* SandboxRuntimeCoordinator::collision_world(
    contracts::StableContentKey region_key
) const noexcept {
    if (live_ == nullptr || region_key == 0) {
        return nullptr;
    }
    const auto& collision = live_->state->collision;
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
    if (live_ == nullptr || index >= live_->state->collision.record_count) {
        return std::nullopt;
    }
    return live_->state->collision.records[index];
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
