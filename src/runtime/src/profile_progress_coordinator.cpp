#include <tgd/runtime/profile_progress_coordinator.hpp>

#include <algorithm>
#include <limits>
#include <new>
#include <string_view>
#include <utility>

namespace tgd::runtime {
namespace {

constexpr std::string_view legacy_checkpoint_magic = "tgd.f1.guest.profile.checkpoint.v1";

[[nodiscard]] ProfileProgressCoordinatorError map_progress_error(
    contracts::ProfileProgressError error
) noexcept {
    switch (error) {
        case contracts::ProfileProgressError::none:
            return ProfileProgressCoordinatorError::none;
        case contracts::ProfileProgressError::allocation_failed:
            return ProfileProgressCoordinatorError::allocation_failed;
        default:
            return ProfileProgressCoordinatorError::invalid_progress;
    }
}

[[nodiscard]] bool read_legacy_sequence(
    const contracts::SaveEnvelopeV1& snapshot,
    std::uint64_t& sequence
) noexcept {
    if (snapshot.payload.size() != legacy_checkpoint_magic.size() + sizeof(sequence) ||
        !std::equal(
            legacy_checkpoint_magic.begin(),
            legacy_checkpoint_magic.end(),
            snapshot.payload.begin()
        )) {
        return false;
    }
    sequence = 0;
    for (std::size_t index = 0; index < sizeof(sequence); ++index) {
        sequence |= static_cast<std::uint64_t>(
                        snapshot.payload[legacy_checkpoint_magic.size() + index]
                    )
                    << (index * 8U);
    }
    return true;
}

[[nodiscard]] contracts::StableId128 reward_snapshot_id(
    contracts::StableId128 operation_id
) noexcept {
    auto snapshot_id = contracts::StableId128{
        operation_id.high ^ 0x736e617073686f74ULL,
        operation_id.low ^ 0x7265776172642d31ULL,
    };
    if (snapshot_id.empty()) {
        snapshot_id.low = 1;
    }
    return snapshot_id;
}

}  // namespace

ProfileProgressCoordinatorError ProfileProgressCoordinator::initialize(
    contracts::StableId128 profile_id,
    contracts::StableId128 package_set_id
) noexcept {
    if (profile_id.empty() || package_set_id.empty()) {
        return ProfileProgressCoordinatorError::invalid_config;
    }
    profile_id_ = profile_id;
    package_set_id_ = package_set_id;
    committed_snapshot_id_ = {};
    committed_progress_ = {profile_id, 0, {}};
    clear_pending();
    state_ = ProfileProgressCoordinatorState::ready;
    return ProfileProgressCoordinatorError::none;
}

ProfileProgressCoordinatorError ProfileProgressCoordinator::restore(
    const contracts::SaveEnvelopeV1& snapshot
) noexcept {
    if (snapshot.profile_id.empty() || snapshot.package_set_id.empty() ||
        contracts::validate_save_envelope_descriptor(snapshot) !=
            contracts::SaveEnvelopeError::none) {
        return ProfileProgressCoordinatorError::invalid_snapshot;
    }

    auto decoded = contracts::decode_profile_progress(snapshot.payload);
    contracts::ProfileProgressV1 progress;
    if (decoded.error == contracts::ProfileProgressError::none) {
        progress = std::move(decoded.progress);
    } else {
        std::uint64_t legacy_sequence = 0;
        if (!read_legacy_sequence(snapshot, legacy_sequence) ||
            legacy_sequence != snapshot.created_logical_sequence) {
            return map_progress_error(decoded.error);
        }
        progress = {snapshot.profile_id, snapshot.created_logical_sequence, {}};
    }
    if (progress.profile_id != snapshot.profile_id ||
        progress.revision != snapshot.created_logical_sequence) {
        return ProfileProgressCoordinatorError::invalid_progress;
    }

    profile_id_ = snapshot.profile_id;
    package_set_id_ = snapshot.package_set_id;
    committed_snapshot_id_ = snapshot.snapshot_id;
    committed_progress_ = std::move(progress);
    clear_pending();
    state_ = ProfileProgressCoordinatorState::ready;
    return ProfileProgressCoordinatorError::none;
}

ProfileProgressPrepareResult ProfileProgressCoordinator::prepare_checkpoint(
    contracts::StableId128 snapshot_id,
    contracts::CheckpointKind checkpoint_kind
) noexcept {
    if (state_ != ProfileProgressCoordinatorState::ready || snapshot_id.empty() ||
        snapshot_id == committed_snapshot_id_) {
        return {ProfileProgressCoordinatorError::invalid_state, {}};
    }
    if (committed_progress_.revision == std::numeric_limits<std::uint64_t>::max()) {
        return {ProfileProgressCoordinatorError::revision_overflow, {}};
    }
    try {
        auto next = committed_progress_;
        ++next.revision;
        return build_pending(
            std::move(next),
            snapshot_id,
            checkpoint_kind,
            ProfileProgressSaveKind::checkpoint
        );
    } catch (const std::bad_alloc&) {
        return {ProfileProgressCoordinatorError::allocation_failed, {}};
    }
}

ProfileProgressPrepareResult ProfileProgressCoordinator::prepare_reward_claim(
    contracts::StableContentKey source_id,
    contracts::StableContentKey reward_id,
    contracts::StableContentKey reward_dedup_key
) noexcept {
    if (state_ == ProfileProgressCoordinatorState::uninitialized || source_id == 0 ||
        reward_id == 0 || reward_dedup_key == 0) {
        return {ProfileProgressCoordinatorError::invalid_state, {}};
    }
    if (contracts::contains_reward_dedup(committed_progress_, reward_dedup_key)) {
        return {
            ProfileProgressCoordinatorError::none,
            ProfileProgressPrepareDisposition::already_committed,
        };
    }
    if (state_ == ProfileProgressCoordinatorState::pending) {
        return contracts::contains_reward_dedup(pending_progress_, reward_dedup_key)
                   ? ProfileProgressPrepareResult{
                         ProfileProgressCoordinatorError::none,
                         ProfileProgressPrepareDisposition::already_pending,
                     }
                   : ProfileProgressPrepareResult{
                         ProfileProgressCoordinatorError::invalid_state,
                         ProfileProgressPrepareDisposition::none,
                     };
    }
    if (committed_progress_.revision == std::numeric_limits<std::uint64_t>::max()) {
        return {ProfileProgressCoordinatorError::revision_overflow, {}};
    }
    try {
        auto next = committed_progress_;
        const auto next_revision = next.revision + 1;
        const auto operation = contracts::make_reward_operation(
            profile_id_,
            next.revision,
            next_revision,
            source_id,
            reward_id,
            reward_dedup_key
        );
        next.revision = next_revision;
        next.operations.push_back(operation);
        return build_pending(
            std::move(next),
            reward_snapshot_id(operation.operation_id),
            contracts::CheckpointKind::chapter_milestone,
            ProfileProgressSaveKind::reward_claim
        );
    } catch (const std::bad_alloc&) {
        return {ProfileProgressCoordinatorError::allocation_failed, {}};
    }
}

ProfileProgressCoordinatorError ProfileProgressCoordinator::accept_commit(
    contracts::StableId128 snapshot_id
) noexcept {
    if (state_ != ProfileProgressCoordinatorState::pending || snapshot_id.empty() ||
        snapshot_id != pending_snapshot_.snapshot_id) {
        return ProfileProgressCoordinatorError::invalid_state;
    }
    committed_progress_ = std::move(pending_progress_);
    committed_snapshot_id_ = snapshot_id;
    clear_pending();
    state_ = ProfileProgressCoordinatorState::ready;
    return ProfileProgressCoordinatorError::none;
}

ProfileProgressCoordinatorError ProfileProgressCoordinator::discard_pending() noexcept {
    if (state_ != ProfileProgressCoordinatorState::pending) {
        return ProfileProgressCoordinatorError::invalid_state;
    }
    clear_pending();
    state_ = ProfileProgressCoordinatorState::ready;
    return ProfileProgressCoordinatorError::none;
}

ProfileProgressCoordinatorState ProfileProgressCoordinator::state() const noexcept {
    return state_;
}

bool ProfileProgressCoordinator::has_pending() const noexcept {
    return state_ == ProfileProgressCoordinatorState::pending;
}

ProfileProgressSaveKind ProfileProgressCoordinator::pending_kind() const noexcept {
    return pending_kind_;
}

const contracts::SaveEnvelopeV1& ProfileProgressCoordinator::pending_snapshot() const noexcept {
    return pending_snapshot_;
}

const contracts::ProfileProgressV1&
ProfileProgressCoordinator::committed_progress() const noexcept {
    return committed_progress_;
}

contracts::StableId128 ProfileProgressCoordinator::committed_snapshot_id() const noexcept {
    return committed_snapshot_id_;
}

bool ProfileProgressCoordinator::has_reward_claim(
    contracts::StableContentKey reward_dedup_key
) const noexcept {
    return contracts::contains_reward_dedup(committed_progress_, reward_dedup_key);
}

std::size_t ProfileProgressCoordinator::committed_operation_count() const noexcept {
    return committed_progress_.operations.size();
}

ProfileProgressPrepareResult ProfileProgressCoordinator::build_pending(
    contracts::ProfileProgressV1&& progress,
    contracts::StableId128 snapshot_id,
    contracts::CheckpointKind checkpoint_kind,
    ProfileProgressSaveKind kind
) noexcept {
    auto encoded = contracts::encode_profile_progress(progress);
    if (encoded.error != contracts::ProfileProgressError::none) {
        return {map_progress_error(encoded.error), {}};
    }
    contracts::SaveEnvelopeV1 snapshot;
    snapshot.profile_id = profile_id_;
    snapshot.snapshot_id = snapshot_id;
    snapshot.parent_snapshot_id = committed_snapshot_id_;
    snapshot.package_set_id = package_set_id_;
    snapshot.created_logical_sequence = progress.revision;
    snapshot.checkpoint_kind = checkpoint_kind;
    snapshot.payload = std::move(encoded.bytes);
    if (snapshot.snapshot_id.empty() || snapshot.snapshot_id == committed_snapshot_id_ ||
        contracts::validate_save_envelope_descriptor(snapshot) !=
            contracts::SaveEnvelopeError::none) {
        return {ProfileProgressCoordinatorError::invalid_snapshot, {}};
    }
    pending_progress_ = std::move(progress);
    pending_snapshot_ = std::move(snapshot);
    pending_kind_ = kind;
    state_ = ProfileProgressCoordinatorState::pending;
    return {
        ProfileProgressCoordinatorError::none,
        ProfileProgressPrepareDisposition::prepared,
    };
}

void ProfileProgressCoordinator::clear_pending() noexcept {
    pending_progress_ = {};
    pending_snapshot_ = {};
    pending_kind_ = ProfileProgressSaveKind::checkpoint;
}

}  // namespace tgd::runtime
