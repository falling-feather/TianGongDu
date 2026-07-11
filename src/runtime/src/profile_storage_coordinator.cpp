#include <tgd/runtime/profile_storage_coordinator.hpp>

#include <tgd/contracts/sha256.hpp>

#include <algorithm>
#include <utility>

namespace tgd::runtime {
namespace {

[[nodiscard]] bool digest_empty(const contracts::Sha256Digest& digest) noexcept {
    return std::all_of(digest.begin(), digest.end(), [](std::uint8_t byte) { return byte == 0; });
}

[[nodiscard]] bool valid_head(const StorageProfileHead& head) noexcept {
    return !head.profile_id.empty() && !head.snapshot_id.empty() &&
           head.logical_sequence > 0 && !digest_empty(head.envelope_hash);
}

[[nodiscard]] ProfileStorageError map_submit_error(StorageSubmitError error) noexcept {
    switch (error) {
        case StorageSubmitError::none:
            return ProfileStorageError::none;
        case StorageSubmitError::invalid_request:
            return ProfileStorageError::protocol_violation;
        case StorageSubmitError::backpressure:
            return ProfileStorageError::backpressure;
        case StorageSubmitError::unavailable:
            return ProfileStorageError::storage_unavailable;
        case StorageSubmitError::allocation_failed:
            return ProfileStorageError::allocation_failed;
    }
    return ProfileStorageError::internal;
}

[[nodiscard]] ProfileStorageError map_completion_error(
    StorageCompletionError error
) noexcept {
    switch (error) {
        case StorageCompletionError::none:
            return ProfileStorageError::none;
        case StorageCompletionError::conflict:
            return ProfileStorageError::storage_conflict;
        case StorageCompletionError::quota:
            return ProfileStorageError::storage_quota;
        case StorageCompletionError::corrupt:
        case StorageCompletionError::not_found:
            return ProfileStorageError::storage_corrupt;
        case StorageCompletionError::unavailable:
            return ProfileStorageError::storage_unavailable;
        case StorageCompletionError::cancelled:
            return ProfileStorageError::cancelled;
        case StorageCompletionError::timeout:
            return ProfileStorageError::timeout;
        case StorageCompletionError::invalid_request:
            return ProfileStorageError::protocol_violation;
        case StorageCompletionError::internal:
            return ProfileStorageError::internal;
    }
    return ProfileStorageError::internal;
}

[[nodiscard]] ProfileStorageState restore_failure_state(ProfileStorageError error) noexcept {
    if (error == ProfileStorageError::storage_corrupt) {
        return ProfileStorageState::recovery_required;
    }
    if (error == ProfileStorageError::storage_unavailable) {
        return ProfileStorageState::storage_unavailable;
    }
    return ProfileStorageState::restore_failed;
}

}  // namespace

ProfileStorageError ProfileStorageCoordinator::initialize(
    IStorage& storage,
    const ProfileStorageConfig& config
) noexcept {
    if (config.profile_id.empty() || config.package_set_id.empty() ||
        config.request_id_seed.high == 0 || config.session_generation == 0 ||
        config.channel != StorageChannel::prototype_f1) {
        return ProfileStorageError::invalid_config;
    }

    storage_ = &storage;
    config_ = config;
    state_ = ProfileStorageState::configured;
    last_error_ = ProfileStorageError::none;
    active_request_id_ = {};
    active_operation_ = StorageOperation::read;
    request_counter_ = 0;
    restore_head_ = {};
    current_head_ = {};
    pending_expected_head_ = {};
    pending_head_ = {};
    has_current_head_ = false;
    pending_durability_ = StorageDurability::relaxed;
    current_snapshot_bytes_.clear();
    pending_snapshot_bytes_.clear();
    committed_save_count_ = 0;
    last_committed_request_id_ = {};
    ignored_stale_completion_count_ = 0;
    return ProfileStorageError::none;
}

ProfileStorageError ProfileStorageCoordinator::begin_restore() noexcept {
    if (storage_ == nullptr ||
        (state_ != ProfileStorageState::configured &&
         state_ != ProfileStorageState::restore_failed &&
         state_ != ProfileStorageState::recovery_required &&
         state_ != ProfileStorageState::storage_unavailable)) {
        return ProfileStorageError::invalid_state;
    }

    clear_active_request();
    restore_head_ = {};
    current_head_ = {};
    has_current_head_ = false;
    current_snapshot_bytes_.clear();
    pending_snapshot_bytes_.clear();
    pending_expected_head_ = {};
    pending_head_ = {};
    last_error_ = ProfileStorageError::none;

    const auto submitted = submit_head_read();
    if (submitted != ProfileStorageError::none) {
        state_ = restore_failure_state(submitted);
        last_error_ = submitted;
    }
    return submitted;
}

ProfileStorageError ProfileStorageCoordinator::begin_save(
    const contracts::SaveEnvelopeV1& snapshot,
    StorageDurability durability
) noexcept {
    if (storage_ == nullptr || state_ != ProfileStorageState::ready ||
        !pending_snapshot_bytes_.empty()) {
        return ProfileStorageError::invalid_state;
    }

    const auto expected_parent = has_current_head_ ? current_head_.snapshot_id
                                                   : contracts::StableId128{};
    const auto expected_sequence = has_current_head_ ? current_head_.logical_sequence : 0;
    if (snapshot.profile_id != config_.profile_id ||
        snapshot.package_set_id != config_.package_set_id ||
        snapshot.parent_snapshot_id != expected_parent ||
        snapshot.created_logical_sequence <= expected_sequence ||
        (has_current_head_ && snapshot.snapshot_id == current_head_.snapshot_id) ||
        contracts::validate_save_envelope_descriptor(snapshot) !=
            contracts::SaveEnvelopeError::none) {
        return ProfileStorageError::invalid_snapshot;
    }

    auto encoded = contracts::encode_save_envelope(snapshot);
    if (encoded.error != contracts::SaveEnvelopeError::none) {
        return encoded.error == contracts::SaveEnvelopeError::allocation_failed
                   ? ProfileStorageError::allocation_failed
                   : ProfileStorageError::invalid_snapshot;
    }

    pending_snapshot_bytes_ = std::move(encoded.bytes);
    pending_expected_head_ = has_current_head_
                                 ? current_head_
                                 : StorageProfileHead{config_.profile_id, {}, 0, {}};
    pending_head_ = {
        config_.profile_id,
        snapshot.snapshot_id,
        snapshot.created_logical_sequence,
        contracts::sha256(
            std::span<const std::uint8_t>{pending_snapshot_bytes_}.first(144)
        ),
    };
    pending_durability_ = durability;
    last_error_ = ProfileStorageError::none;

    const auto submitted = submit_pending_save();
    if (submitted != ProfileStorageError::none) {
        state_ = ProfileStorageState::save_failed;
        last_error_ = submitted;
    }
    return submitted;
}

ProfileStorageError ProfileStorageCoordinator::retry_pending_save() noexcept {
    if (storage_ == nullptr || state_ != ProfileStorageState::save_failed ||
        pending_snapshot_bytes_.empty()) {
        return ProfileStorageError::invalid_state;
    }

    last_error_ = ProfileStorageError::none;
    const auto submitted = submit_pending_save();
    if (submitted != ProfileStorageError::none) {
        state_ = ProfileStorageState::save_failed;
        last_error_ = submitted;
    }
    return submitted;
}

ProfileStoragePumpResult ProfileStorageCoordinator::pump() noexcept {
    if (storage_ == nullptr || active_request_id_.empty()) {
        return {};
    }

    StorageCompletion completion;
    if (!storage_->poll_completion(active_request_id_, completion)) {
        return {};
    }

    if (completion.context.request_id != active_request_id_) {
        const auto failure_state = state_ == ProfileStorageState::saving
                                       ? ProfileStorageState::save_failed
                                       : ProfileStorageState::restore_failed;
        return fail_active(failure_state, ProfileStorageError::protocol_violation);
    }
    if (completion.context.session_generation != config_.session_generation) {
        ++ignored_stale_completion_count_;
        return {true, false, ProfileStorageError::none};
    }
    if (completion.context.channel != config_.channel ||
        completion.operation != active_operation_) {
        const auto failure_state = state_ == ProfileStorageState::saving
                                       ? ProfileStorageState::save_failed
                                       : ProfileStorageState::restore_failed;
        return fail_active(failure_state, ProfileStorageError::protocol_violation);
    }

    if (state_ == ProfileStorageState::loading_head) {
        return handle_head_completion(std::move(completion));
    }
    if (state_ == ProfileStorageState::loading_snapshot) {
        return handle_snapshot_completion(std::move(completion));
    }
    if (state_ == ProfileStorageState::saving) {
        return handle_save_completion(std::move(completion));
    }
    return fail_active(ProfileStorageState::restore_failed, ProfileStorageError::invalid_state);
}

ProfileStorageState ProfileStorageCoordinator::state() const noexcept {
    return state_;
}

ProfileStorageError ProfileStorageCoordinator::last_error() const noexcept {
    return last_error_;
}

bool ProfileStorageCoordinator::has_snapshot() const noexcept {
    return has_current_head_ && !current_snapshot_bytes_.empty();
}

bool ProfileStorageCoordinator::has_pending_save() const noexcept {
    return !pending_snapshot_bytes_.empty();
}

std::span<const std::uint8_t> ProfileStorageCoordinator::current_snapshot_bytes() const noexcept {
    return current_snapshot_bytes_;
}

std::span<const std::uint8_t> ProfileStorageCoordinator::pending_snapshot_bytes() const noexcept {
    return pending_snapshot_bytes_;
}

const StorageProfileHead& ProfileStorageCoordinator::current_head() const noexcept {
    return current_head_;
}

std::uint64_t ProfileStorageCoordinator::committed_save_count() const noexcept {
    return committed_save_count_;
}

contracts::StableId128 ProfileStorageCoordinator::last_committed_request_id() const noexcept {
    return last_committed_request_id_;
}

std::uint64_t ProfileStorageCoordinator::ignored_stale_completion_count() const noexcept {
    return ignored_stale_completion_count_;
}

contracts::StableId128 ProfileStorageCoordinator::next_request_id() noexcept {
    const auto request_id = contracts::StableId128{
        config_.request_id_seed.high,
        config_.request_id_seed.low + request_counter_,
    };
    ++request_counter_;
    return request_id;
}

ProfileStorageError ProfileStorageCoordinator::submit_head_read() noexcept {
    const auto request_id = next_request_id();
    const StorageReadRequest request{
        {request_id, config_.session_generation, config_.channel},
        {StorageRecordKind::profile_head, config_.profile_id, {}},
    };
    const auto submitted = storage_->read(request);
    if (submitted != StorageSubmitError::none) {
        return map_submit_error(submitted);
    }
    active_request_id_ = request_id;
    active_operation_ = StorageOperation::read;
    state_ = ProfileStorageState::loading_head;
    return ProfileStorageError::none;
}

ProfileStorageError ProfileStorageCoordinator::submit_snapshot_read(
    const StorageProfileHead& head
) noexcept {
    const auto request_id = next_request_id();
    const StorageReadRequest request{
        {request_id, config_.session_generation, config_.channel},
        {StorageRecordKind::snapshot, config_.profile_id, head.snapshot_id},
    };
    const auto submitted = storage_->read(request);
    if (submitted != StorageSubmitError::none) {
        return map_submit_error(submitted);
    }
    active_request_id_ = request_id;
    active_operation_ = StorageOperation::read;
    state_ = ProfileStorageState::loading_snapshot;
    return ProfileStorageError::none;
}

ProfileStorageError ProfileStorageCoordinator::submit_pending_save() noexcept {
    const auto request_id = next_request_id();
    const StorageWriteAtomicRequest request{
        {request_id, config_.session_generation, config_.channel},
        pending_expected_head_,
        pending_head_,
        pending_snapshot_bytes_,
        pending_durability_,
    };
    const auto submitted = storage_->write_atomic(request);
    if (submitted != StorageSubmitError::none) {
        return map_submit_error(submitted);
    }
    active_request_id_ = request_id;
    active_operation_ = StorageOperation::write_atomic;
    state_ = ProfileStorageState::saving;
    return ProfileStorageError::none;
}

ProfileStoragePumpResult ProfileStorageCoordinator::handle_head_completion(
    StorageCompletion&& completion
) noexcept {
    if (completion.error == StorageCompletionError::not_found) {
        clear_active_request();
        current_head_ = {};
        has_current_head_ = false;
        current_snapshot_bytes_.clear();
        state_ = ProfileStorageState::ready;
        last_error_ = ProfileStorageError::none;
        return {true, true, ProfileStorageError::none};
    }
    if (completion.error != StorageCompletionError::none) {
        const auto error = map_completion_error(completion.error);
        return fail_active(restore_failure_state(error), error);
    }
    if (completion.key != StorageKey{StorageRecordKind::profile_head, config_.profile_id, {}} ||
        !valid_head(completion.head) || completion.head.profile_id != config_.profile_id) {
        return fail_active(
            ProfileStorageState::recovery_required,
            ProfileStorageError::storage_corrupt
        );
    }

    restore_head_ = completion.head;
    clear_active_request();
    const auto submitted = submit_snapshot_read(restore_head_);
    if (submitted != ProfileStorageError::none) {
        state_ = restore_failure_state(submitted);
        last_error_ = submitted;
    }
    return {true, true, submitted};
}

ProfileStoragePumpResult ProfileStorageCoordinator::handle_snapshot_completion(
    StorageCompletion&& completion
) noexcept {
    if (completion.error != StorageCompletionError::none) {
        const auto error = map_completion_error(completion.error);
        return fail_active(restore_failure_state(error), error);
    }
    const StorageKey expected_key{
        StorageRecordKind::snapshot,
        config_.profile_id,
        restore_head_.snapshot_id,
    };
    if (completion.key != expected_key) {
        return fail_active(
            ProfileStorageState::recovery_required,
            ProfileStorageError::storage_corrupt
        );
    }

    const auto decoded = contracts::decode_save_envelope(completion.bytes);
    if (decoded.error != contracts::SaveEnvelopeError::none) {
        const auto error = decoded.error == contracts::SaveEnvelopeError::allocation_failed
                               ? ProfileStorageError::allocation_failed
                               : ProfileStorageError::storage_corrupt;
        return fail_active(restore_failure_state(error), error);
    }
    if (decoded.envelope.profile_id != config_.profile_id ||
        decoded.envelope.package_set_id != config_.package_set_id ||
        decoded.envelope.snapshot_id != restore_head_.snapshot_id ||
        decoded.envelope.created_logical_sequence != restore_head_.logical_sequence ||
        decoded.envelope.envelope_hash != restore_head_.envelope_hash) {
        return fail_active(
            ProfileStorageState::recovery_required,
            ProfileStorageError::storage_corrupt
        );
    }

    clear_active_request();
    current_head_ = restore_head_;
    restore_head_ = {};
    has_current_head_ = true;
    current_snapshot_bytes_ = std::move(completion.bytes);
    state_ = ProfileStorageState::ready;
    last_error_ = ProfileStorageError::none;
    return {true, true, ProfileStorageError::none};
}

ProfileStoragePumpResult ProfileStorageCoordinator::handle_save_completion(
    StorageCompletion&& completion
) noexcept {
    if (completion.error != StorageCompletionError::none) {
        const auto error = map_completion_error(completion.error);
        const auto failure_state = error == ProfileStorageError::storage_conflict
                                       ? ProfileStorageState::conflict_read_only
                                       : ProfileStorageState::save_failed;
        return fail_active(failure_state, error);
    }
    const StorageKey expected_key{
        StorageRecordKind::snapshot,
        config_.profile_id,
        pending_head_.snapshot_id,
    };
    if (completion.key != expected_key || completion.head != pending_head_) {
        return fail_active(
            ProfileStorageState::save_failed,
            ProfileStorageError::protocol_violation
        );
    }

    const auto committed_request_id = active_request_id_;
    clear_active_request();
    current_head_ = pending_head_;
    has_current_head_ = true;
    current_snapshot_bytes_ = std::move(pending_snapshot_bytes_);
    pending_expected_head_ = {};
    pending_head_ = {};
    pending_durability_ = StorageDurability::relaxed;
    ++committed_save_count_;
    last_committed_request_id_ = committed_request_id;
    state_ = ProfileStorageState::ready;
    last_error_ = ProfileStorageError::none;
    return {true, true, ProfileStorageError::none};
}

ProfileStoragePumpResult ProfileStorageCoordinator::fail_active(
    ProfileStorageState next_state,
    ProfileStorageError error
) noexcept {
    clear_active_request();
    state_ = next_state;
    last_error_ = error;
    return {true, true, error};
}

void ProfileStorageCoordinator::clear_active_request() noexcept {
    active_request_id_ = {};
    active_operation_ = StorageOperation::read;
}

}  // namespace tgd::runtime
