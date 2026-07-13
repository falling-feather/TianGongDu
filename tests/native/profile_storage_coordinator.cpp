#include <tgd/contracts/save_envelope.hpp>
#include <tgd/runtime/profile_progress_coordinator.hpp>
#include <tgd/runtime/profile_storage_coordinator.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <iostream>

namespace {

using tgd::contracts::SaveEnvelopeV1;
using tgd::contracts::StableId128;
using tgd::contracts::PersistentOperationV1;
using tgd::runtime::IStorage;
using tgd::runtime::ProfileStorageConfig;
using tgd::runtime::ProfileStorageCoordinator;
using tgd::runtime::ProfileStorageError;
using tgd::runtime::ProfileStorageState;
using tgd::runtime::ProfileProgressCoordinator;
using tgd::runtime::ProfileProgressCoordinatorError;
using tgd::runtime::ProfileProgressPrepareDisposition;
using tgd::runtime::StorageCompletion;
using tgd::runtime::StorageCompletionError;
using tgd::runtime::StorageDeleteRequest;
using tgd::runtime::StorageDurability;
using tgd::runtime::StorageEstimateQuotaRequest;
using tgd::runtime::StorageKey;
using tgd::runtime::StorageListRequest;
using tgd::runtime::StorageOperation;
using tgd::runtime::StoragePersistenceRequest;
using tgd::runtime::StorageProfileHead;
using tgd::runtime::StorageReadRequest;
using tgd::runtime::StorageRecordKind;
using tgd::runtime::StorageSubmitError;
using tgd::runtime::StorageWriteAtomicRequest;

constexpr StableId128 profile_id{0x1000000000000001ULL, 0x2000000000000001ULL};
constexpr StableId128 package_set_id{0x3000000000000001ULL, 0x4000000000000001ULL};
constexpr StableId128 snapshot_one{0x5000000000000001ULL, 0x6000000000000001ULL};
constexpr StableId128 snapshot_two{0x5000000000000002ULL, 0x6000000000000002ULL};
constexpr StableId128 snapshot_external{0x5000000000000003ULL, 0x6000000000000003ULL};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "profile storage failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] bool digest_empty(const tgd::contracts::Sha256Digest& digest) noexcept {
    return std::all_of(digest.begin(), digest.end(), [](std::uint8_t byte) { return byte == 0; });
}

struct SnapshotRecord final {
    StableId128 snapshot_id{};
    std::vector<std::uint8_t> bytes{};
};

struct PendingWrite final {
    tgd::runtime::StorageRequestContext context{};
    StorageProfileHead expected_head{};
    StorageProfileHead next_head{};
    std::vector<std::uint8_t> bytes{};
    std::vector<PersistentOperationV1> operations{};
    StorageDurability durability{StorageDurability::relaxed};
};

class MockStorage final : public IStorage {
  public:
    [[nodiscard]] StorageSubmitError read(const StorageReadRequest& request) noexcept override {
        try {
            StorageCompletion completion;
            completion.context = request.context;
            completion.operation = StorageOperation::read;
            completion.key = request.key;
            if (request.key.kind == StorageRecordKind::profile_head) {
                if (head_.has_value()) {
                    completion.head = *head_;
                } else {
                    completion.error = StorageCompletionError::not_found;
                }
            } else if (request.key.kind == StorageRecordKind::snapshot) {
                const auto record = std::find_if(
                    snapshots_.begin(),
                    snapshots_.end(),
                    [&](const SnapshotRecord& candidate) {
                        return candidate.snapshot_id == request.key.record_id;
                    }
                );
                if (record == snapshots_.end()) {
                    completion.error = StorageCompletionError::not_found;
                } else {
                    completion.bytes = record->bytes;
                }
            } else {
                completion.error = StorageCompletionError::invalid_request;
            }
            completions_.push_back(std::move(completion));
            return StorageSubmitError::none;
        } catch (const std::bad_alloc&) {
            return StorageSubmitError::allocation_failed;
        }
    }

    [[nodiscard]] StorageSubmitError write_atomic(
        const StorageWriteAtomicRequest& request
    ) noexcept override {
        if (pending_write_.has_value()) {
            return StorageSubmitError::backpressure;
        }
        if (request.context.request_id.empty() || request.context.session_generation == 0 ||
            request.expected_head.profile_id != request.next_head.profile_id ||
            request.next_head.profile_id.empty() || request.next_head.snapshot_id.empty() ||
            request.next_head.logical_sequence == 0 || request.snapshot_bytes.empty()) {
            return StorageSubmitError::invalid_request;
        }
        try {
            PendingWrite pending;
            pending.context = request.context;
            pending.expected_head = request.expected_head;
            pending.next_head = request.next_head;
            pending.bytes.assign(request.snapshot_bytes.begin(), request.snapshot_bytes.end());
            pending.operations.assign(request.operations.begin(), request.operations.end());
            pending.durability = request.durability;
            pending_write_ = std::move(pending);
            return StorageSubmitError::none;
        } catch (const std::bad_alloc&) {
            pending_write_.reset();
            return StorageSubmitError::allocation_failed;
        }
    }

    [[nodiscard]] StorageSubmitError list(const StorageListRequest&) noexcept override {
        return StorageSubmitError::none;
    }

    [[nodiscard]] StorageSubmitError delete_record(
        const StorageDeleteRequest&
    ) noexcept override {
        return StorageSubmitError::none;
    }

    [[nodiscard]] StorageSubmitError estimate_quota(
        const StorageEstimateQuotaRequest&
    ) noexcept override {
        return StorageSubmitError::none;
    }

    [[nodiscard]] StorageSubmitError request_persistence(
        const StoragePersistenceRequest&
    ) noexcept override {
        return StorageSubmitError::none;
    }

    [[nodiscard]] bool poll_completion(
        StableId128 request_id,
        StorageCompletion& output
    ) noexcept override {
        const auto completion = std::find_if(
            completions_.begin(),
            completions_.end(),
            [&](const StorageCompletion& candidate) {
                return candidate.context.request_id == request_id;
            }
        );
        if (completion == completions_.end()) {
            return false;
        }
        output = std::move(*completion);
        completions_.erase(completion);
        return true;
    }

    void complete_write(StorageCompletionError forced_error = StorageCompletionError::none) {
        if (!pending_write_.has_value()) {
            std::abort();
        }

        auto pending = std::move(*pending_write_);
        pending_write_.reset();
        StorageCompletion completion;
        completion.context = pending.context;
        completion.operation = StorageOperation::write_atomic;
        completion.key = {
            StorageRecordKind::snapshot,
            pending.next_head.profile_id,
            pending.next_head.snapshot_id,
        };
        completion.head = pending.next_head;
        completion.error = forced_error;

        if (completion.error == StorageCompletionError::none && !expected_head_matches(pending)) {
            completion.error = StorageCompletionError::conflict;
        }
        if (completion.error == StorageCompletionError::none) {
            snapshots_.push_back({pending.next_head.snapshot_id, std::move(pending.bytes)});
            operations_.insert(
                operations_.end(),
                pending.operations.begin(),
                pending.operations.end()
            );
            head_ = pending.next_head;
        }
        completions_.push_back(std::move(completion));
    }

    [[nodiscard]] bool has_pending_write() const noexcept {
        return pending_write_.has_value();
    }

    [[nodiscard]] StorageDurability pending_durability() const noexcept {
        return pending_write_.has_value() ? pending_write_->durability
                                          : StorageDurability::relaxed;
    }

    [[nodiscard]] StableId128 pending_request_id() const noexcept {
        return pending_write_.has_value() ? pending_write_->context.request_id : StableId128{};
    }

    [[nodiscard]] std::size_t pending_operation_count() const noexcept {
        return pending_write_.has_value() ? pending_write_->operations.size() : 0;
    }

    [[nodiscard]] std::size_t operation_count() const noexcept {
        return operations_.size();
    }

    [[nodiscard]] bool has_reward_dedup(std::uint64_t reward_dedup_key) const noexcept {
        return std::any_of(
            operations_.begin(),
            operations_.end(),
            [reward_dedup_key](const PersistentOperationV1& operation) {
                return operation.reward_dedup_key == reward_dedup_key;
            }
        );
    }

    void force_head(const StorageProfileHead& head) {
        head_ = head;
    }

    void corrupt_current_snapshot() {
        if (!head_.has_value()) {
            std::abort();
        }
        const auto record = std::find_if(
            snapshots_.begin(),
            snapshots_.end(),
            [&](const SnapshotRecord& candidate) {
                return candidate.snapshot_id == head_->snapshot_id;
            }
        );
        if (record == snapshots_.end() || record->bytes.empty()) {
            std::abort();
        }
        record->bytes.back() ^= 0xffU;
    }

    void rewrite_queued_generation(std::uint32_t generation) {
        if (completions_.empty()) {
            std::abort();
        }
        completions_.front().context.session_generation = generation;
    }

  private:
    [[nodiscard]] bool expected_head_matches(const PendingWrite& pending) const noexcept {
        if (head_.has_value()) {
            return *head_ == pending.expected_head;
        }
        return pending.expected_head.profile_id == pending.next_head.profile_id &&
               pending.expected_head.snapshot_id.empty() &&
               pending.expected_head.logical_sequence == 0 &&
               digest_empty(pending.expected_head.envelope_hash);
    }

    std::optional<StorageProfileHead> head_{};
    std::vector<SnapshotRecord> snapshots_{};
    std::vector<PersistentOperationV1> operations_{};
    std::optional<PendingWrite> pending_write_{};
    std::vector<StorageCompletion> completions_{};
};

[[nodiscard]] ProfileStorageConfig config(std::uint64_t seed, std::uint32_t generation) {
    return {
        profile_id,
        package_set_id,
        {0xa000000000000000ULL + seed, 0xb000000000000000ULL + seed},
        generation,
    };
}

[[nodiscard]] SaveEnvelopeV1 snapshot(
    StableId128 snapshot_id,
    StableId128 parent_id,
    std::uint64_t sequence,
    std::string_view payload
) {
    SaveEnvelopeV1 envelope;
    envelope.profile_id = profile_id;
    envelope.snapshot_id = snapshot_id;
    envelope.parent_snapshot_id = parent_id;
    envelope.package_set_id = package_set_id;
    envelope.created_logical_sequence = sequence;
    envelope.payload.assign(payload.begin(), payload.end());
    return envelope;
}

bool restore(ProfileStorageCoordinator& coordinator) {
    bool ok = coordinator.begin_restore() == ProfileStorageError::none;
    ok &= expect(
        coordinator.state() == ProfileStorageState::loading_head,
        "restore begins with a Head read"
    );
    const auto head = coordinator.pump();
    ok &= expect(head.completion_consumed && head.state_changed, "Head completion advances restore");
    if (coordinator.state() == ProfileStorageState::ready) {
        return ok;
    }
    ok &= expect(
        coordinator.state() == ProfileStorageState::loading_snapshot,
        "existing Head schedules its snapshot read"
    );
    const auto body = coordinator.pump();
    ok &= expect(body.completion_consumed && body.state_changed, "snapshot completion advances restore");
    return ok;
}

}  // namespace

int main() {
    bool ok = true;

    MockStorage storage;
    ProfileStorageCoordinator first_session;
    ok &= expect(
        first_session.initialize(storage, config(1, 1)) == ProfileStorageError::none,
        "coordinator initializes with an isolated profile and request seed"
    );
    ok &= restore(first_session);
    ok &= expect(
        first_session.state() == ProfileStorageState::ready && !first_session.has_snapshot(),
        "missing Guest Head creates a new ready profile"
    );

    const auto first_snapshot = snapshot(snapshot_one, {}, 1, "guest-f1-first-save");
    ok &= expect(
        first_session.begin_save(first_snapshot, StorageDurability::strict_if_supported) ==
            ProfileStorageError::none,
        "first Guest snapshot enters the atomic save pipeline"
    );
    ok &= expect(
        first_session.state() == ProfileStorageState::saving &&
            first_session.committed_save_count() == 0 && storage.has_pending_write(),
        "request submission alone is not reported as saved"
    );
    ok &= expect(
        storage.pending_durability() == StorageDurability::strict_if_supported,
        "critical save requests strict durability when supported"
    );
    storage.complete_write();
    ok &= expect(
        first_session.state() == ProfileStorageState::saving &&
            first_session.committed_save_count() == 0,
        "transaction completion is not visible before the C++ ack is pumped"
    );
    const auto first_commit = first_session.pump();
    ok &= expect(
        first_commit.error == ProfileStorageError::none &&
            first_session.state() == ProfileStorageState::ready &&
            first_session.committed_save_count() == 1 &&
            !first_session.last_committed_request_id().empty(),
        "only a verified transaction ack advances the saved revision"
    );
    ok &= expect(
        first_session.has_snapshot() && !first_session.has_pending_save(),
        "committed bytes replace the current snapshot and clear pending state"
    );
    const auto decoded_first = tgd::contracts::decode_save_envelope(
        first_session.current_snapshot_bytes()
    );
    ok &= expect(
        decoded_first.error == tgd::contracts::SaveEnvelopeError::none &&
            decoded_first.envelope.snapshot_id == snapshot_one,
        "committed bytes remain a valid SaveEnvelopeV1"
    );

    const MockStorage persisted = storage;
    MockStorage restored_storage = persisted;
    ProfileStorageCoordinator restored_session;
    ok &= expect(
        restored_session.initialize(restored_storage, config(2, 2)) == ProfileStorageError::none,
        "a new session initializes against persisted storage"
    );
    ok &= restore(restored_session);
    ok &= expect(
        restored_session.state() == ProfileStorageState::ready &&
            restored_session.has_snapshot() &&
            restored_session.current_head().snapshot_id == snapshot_one,
        "Head and snapshot restore together after hash and identity validation"
    );

    MockStorage conflict_storage = persisted;
    ProfileStorageCoordinator conflict_session;
    ok &= conflict_session.initialize(conflict_storage, config(3, 3)) == ProfileStorageError::none;
    ok &= restore(conflict_session);
    const auto conflicting_snapshot = snapshot(snapshot_two, snapshot_one, 2, "local-tab-save");
    ok &= conflict_session.begin_save(conflicting_snapshot) == ProfileStorageError::none;
    auto external_head = conflict_session.current_head();
    external_head.snapshot_id = snapshot_external;
    external_head.logical_sequence = 2;
    external_head.envelope_hash[0] ^= 0x7fU;
    conflict_storage.force_head(external_head);
    conflict_storage.complete_write();
    const auto conflict = conflict_session.pump();
    ok &= expect(
        conflict.error == ProfileStorageError::storage_conflict &&
            conflict_session.state() == ProfileStorageState::conflict_read_only &&
            conflict_session.has_pending_save() &&
            conflict_session.current_head().snapshot_id == snapshot_one,
        "CAS loser becomes read-only without replacing its known Head or pending export"
    );

    MockStorage quota_storage = persisted;
    ProfileStorageCoordinator quota_session;
    ok &= quota_session.initialize(quota_storage, config(4, 4)) == ProfileStorageError::none;
    ok &= restore(quota_session);
    const auto quota_snapshot = snapshot(snapshot_two, snapshot_one, 2, "quota-retry-save");
    ok &= quota_session.begin_save(quota_snapshot) == ProfileStorageError::none;
    const auto failed_request = quota_storage.pending_request_id();
    quota_storage.complete_write(StorageCompletionError::quota);
    const auto quota = quota_session.pump();
    ok &= expect(
        quota.error == ProfileStorageError::storage_quota &&
            quota_session.state() == ProfileStorageState::save_failed &&
            quota_session.has_pending_save() &&
            quota_session.current_head().snapshot_id == snapshot_one,
        "quota failure preserves the previous Head and unsaved bytes"
    );
    ok &= expect(
        quota_session.retry_pending_save() == ProfileStorageError::none &&
            quota_storage.pending_request_id() != failed_request,
        "retry uses a fresh 128-bit request id"
    );
    quota_storage.complete_write();
    ok &= expect(
        quota_session.pump().error == ProfileStorageError::none &&
            quota_session.current_head().snapshot_id == snapshot_two,
        "retry commits the retained canonical snapshot"
    );

    MockStorage corrupt_storage = persisted;
    corrupt_storage.corrupt_current_snapshot();
    ProfileStorageCoordinator corrupt_session;
    ok &= corrupt_session.initialize(corrupt_storage, config(5, 5)) == ProfileStorageError::none;
    ok &= restore(corrupt_session);
    ok &= expect(
        corrupt_session.state() == ProfileStorageState::recovery_required &&
            corrupt_session.last_error() == ProfileStorageError::storage_corrupt &&
            !corrupt_session.has_snapshot(),
        "corrupt snapshot is isolated instead of becoming current"
    );

    MockStorage stale_storage;
    ProfileStorageCoordinator stale_session;
    ok &= stale_session.initialize(stale_storage, config(6, 6)) == ProfileStorageError::none;
    ok &= stale_session.begin_restore() == ProfileStorageError::none;
    stale_storage.rewrite_queued_generation(5);
    const auto stale = stale_session.pump();
    ok &= expect(
        stale.completion_consumed && !stale.state_changed &&
            stale_session.state() == ProfileStorageState::loading_head &&
            stale_session.ignored_stale_completion_count() == 1,
        "old session generation completion is consumed and ignored"
    );

    constexpr std::uint64_t resolution_source = 0x7100000000000001ULL;
    constexpr std::uint64_t reward_one = 0x7200000000000001ULL;
    constexpr std::uint64_t reward_two = 0x7200000000000002ULL;
    constexpr std::uint64_t reward_dedup_one = 0x7300000000000001ULL;
    constexpr std::uint64_t reward_dedup_two = 0x7300000000000002ULL;
    MockStorage reward_storage;
    ProfileStorageCoordinator reward_storage_session;
    ProfileProgressCoordinator reward_progress;
    ok &= reward_storage_session.initialize(reward_storage, config(7, 7)) ==
          ProfileStorageError::none;
    ok &= restore(reward_storage_session);
    ok &= expect(
        reward_progress.initialize(profile_id, package_set_id) ==
                ProfileProgressCoordinatorError::none &&
            reward_progress.committed_operation_count() == 0,
        "a new Profile progress ledger starts empty beside the storage Head"
    );
    const auto prepared_checkpoint = reward_progress.prepare_checkpoint(
        snapshot_one,
        tgd::contracts::CheckpointKind::safe_point
    );
    ok &= expect(
        prepared_checkpoint.error == ProfileProgressCoordinatorError::none &&
            prepared_checkpoint.disposition ==
                ProfileProgressPrepareDisposition::prepared &&
            reward_storage_session.begin_save(reward_progress.pending_snapshot()) ==
                ProfileStorageError::none,
        "Profile progress produces the SaveEnvelope consumed by the existing atomic coordinator"
    );
    reward_storage.complete_write();
    ok &= expect(
        reward_storage_session.pump().error == ProfileStorageError::none &&
            reward_progress.accept_commit(reward_storage_session.current_head().snapshot_id) ==
                ProfileProgressCoordinatorError::none &&
            reward_progress.committed_progress().revision == 1,
        "a verified storage ack advances both the Head and committed Profile progress"
    );

    const auto prepared_reward = reward_progress.prepare_reward_claim(
        resolution_source,
        reward_one,
        reward_dedup_one
    );
    ok &= expect(
        prepared_reward.error == ProfileProgressCoordinatorError::none &&
            prepared_reward.disposition == ProfileProgressPrepareDisposition::prepared &&
            reward_progress.has_pending() &&
            !reward_progress.has_reward_claim(reward_dedup_one),
        "a reward stays pending and invisible until its snapshot transaction commits"
    );
    ok &= reward_progress.prepare_reward_claim(
              resolution_source,
              reward_one,
              reward_dedup_one
           ).disposition == ProfileProgressPrepareDisposition::already_pending;
    ok &= expect(
        reward_progress.prepare_reward_claim(
            resolution_source,
            reward_two,
            reward_dedup_one
        ).error == ProfileProgressCoordinatorError::invalid_claim,
        "a pending deduplication key cannot be rebound to a different reward"
    );
    ok &= expect(
        reward_storage_session.begin_save(
            reward_progress.pending_snapshot(),
            StorageDurability::strict_if_supported
        ) == ProfileStorageError::invalid_snapshot,
        "a reward snapshot cannot advance its ledger without the matching atomic Operation"
    );
    auto illegal_operation = reward_progress.pending_new_operations().front();
    illegal_operation.reward_id = 0;
    const std::array<PersistentOperationV1, 1> illegal_operations{illegal_operation};
    ok &= expect(
        reward_storage_session.begin_save(
            reward_progress.pending_snapshot(),
            StorageDurability::strict_if_supported,
            illegal_operations
        ) == ProfileStorageError::invalid_snapshot &&
            reward_storage_session.state() == ProfileStorageState::ready &&
            !reward_storage.has_pending_write(),
        "an Operation that drifts from its canonical Profile snapshot fails before submission"
    );
    ok &= expect(
        reward_progress.pending_new_operations().size() == 1 &&
            reward_storage_session.begin_save(
                reward_progress.pending_snapshot(),
                StorageDurability::strict_if_supported,
                reward_progress.pending_new_operations()
            ) == ProfileStorageError::none &&
            reward_storage.pending_operation_count() == 1,
        "the reward Operation is copied into the same strict atomic write as its snapshot"
    );
    reward_storage.complete_write();
    ok &= expect(
        reward_storage_session.pump().error == ProfileStorageError::none &&
            reward_progress.accept_commit(reward_storage_session.current_head().snapshot_id) ==
                ProfileProgressCoordinatorError::none &&
            reward_progress.has_reward_claim(reward_dedup_one) &&
            reward_progress.committed_operation_count() == 1 &&
            reward_storage.operation_count() == 1 &&
            reward_storage.has_reward_dedup(reward_dedup_one),
        "the reward and its Operation become visible only after one atomic storage acknowledgement"
    );
    ok &= expect(
        reward_progress.prepare_reward_claim(
            resolution_source,
            reward_one,
            reward_dedup_one
        ).disposition == ProfileProgressPrepareDisposition::already_committed,
        "replaying a committed reward receipt cannot prepare another save"
    );
    ok &= expect(
        reward_progress.prepare_reward_claim(
            resolution_source,
            reward_two,
            reward_dedup_one
        ).error == ProfileProgressCoordinatorError::invalid_claim,
        "a committed deduplication key remains bound to its original reward"
    );

    MockStorage reward_conflict_storage = reward_storage;
    ProfileStorageCoordinator reward_conflict_session;
    ProfileProgressCoordinator reward_conflict_progress;
    ok &= reward_conflict_session.initialize(reward_conflict_storage, config(8, 8)) ==
          ProfileStorageError::none;
    ok &= restore(reward_conflict_session);
    const auto conflict_reward_envelope = tgd::contracts::decode_save_envelope(
        reward_conflict_session.current_snapshot_bytes()
    );
    ok &= conflict_reward_envelope.error == tgd::contracts::SaveEnvelopeError::none &&
          reward_conflict_progress.restore(conflict_reward_envelope.envelope) ==
              ProfileProgressCoordinatorError::none;
    ok &= reward_conflict_progress.prepare_reward_claim(
              resolution_source,
              reward_two,
              reward_dedup_two
          ).disposition == ProfileProgressPrepareDisposition::prepared;
    ok &= reward_conflict_session.begin_save(
              reward_conflict_progress.pending_snapshot(),
              StorageDurability::strict_if_supported,
              reward_conflict_progress.pending_new_operations()
          ) == ProfileStorageError::none;
    auto competing_reward_head = reward_conflict_session.current_head();
    competing_reward_head.snapshot_id = snapshot_external;
    competing_reward_head.logical_sequence += 1;
    competing_reward_head.envelope_hash[0] ^= 0x3cU;
    reward_conflict_storage.force_head(competing_reward_head);
    reward_conflict_storage.complete_write();
    ok &= expect(
        reward_conflict_session.pump().error == ProfileStorageError::storage_conflict &&
            reward_conflict_session.state() == ProfileStorageState::conflict_read_only &&
            reward_conflict_session.pending_operations().size() == 1 &&
            reward_conflict_storage.operation_count() == 1 &&
            reward_conflict_progress.has_pending() &&
            !reward_conflict_progress.has_reward_claim(reward_dedup_two),
        "a reward Head CAS loser keeps its pending Operation without duplicating the committed ledger"
    );

    const auto prepared_quota_reward = reward_progress.prepare_reward_claim(
        resolution_source,
        reward_two,
        reward_dedup_two
    );
    ok &= prepared_quota_reward.disposition == ProfileProgressPrepareDisposition::prepared;
    ok &= reward_storage_session.begin_save(
              reward_progress.pending_snapshot(),
              StorageDurability::strict_if_supported,
              reward_progress.pending_new_operations()
          ) == ProfileStorageError::none;
    reward_storage.complete_write(StorageCompletionError::quota);
    ok &= expect(
        reward_storage_session.pump().error == ProfileStorageError::storage_quota &&
            reward_progress.has_pending() &&
            !reward_progress.has_reward_claim(reward_dedup_two) &&
            reward_storage.operation_count() == 1 &&
            reward_storage_session.pending_operations().size() == 1,
        "quota failure rolls back the Operation and retains its canonical retry bytes"
    );
    ok &= reward_storage_session.retry_pending_save() == ProfileStorageError::none;
    reward_storage.complete_write();
    ok &= expect(
        reward_storage_session.pump().error == ProfileStorageError::none &&
            reward_progress.accept_commit(reward_storage_session.current_head().snapshot_id) ==
                ProfileProgressCoordinatorError::none &&
            reward_progress.has_reward_claim(reward_dedup_two) &&
            reward_progress.committed_operation_count() == 2 &&
            reward_storage.operation_count() == 2 &&
            reward_storage.has_reward_dedup(reward_dedup_two),
        "retry commits the identical retained Operation exactly once"
    );

    const auto restored_reward_envelope = tgd::contracts::decode_save_envelope(
        reward_storage_session.current_snapshot_bytes()
    );
    ProfileProgressCoordinator restored_progress;
    ok &= expect(
        restored_reward_envelope.error == tgd::contracts::SaveEnvelopeError::none &&
            restored_progress.restore(restored_reward_envelope.envelope) ==
                ProfileProgressCoordinatorError::none &&
            restored_progress.has_reward_claim(reward_dedup_one) &&
            restored_progress.has_reward_claim(reward_dedup_two) &&
            restored_progress.prepare_reward_claim(
                resolution_source,
                reward_two,
                reward_dedup_two
            ).disposition == ProfileProgressPrepareDisposition::already_committed,
        "restoring a new session preserves reward deduplication across refresh"
    );

    auto legacy_snapshot = snapshot(snapshot_external, {}, 5, "");
    constexpr std::string_view legacy_magic = "tgd.f1.guest.profile.checkpoint.v1";
    legacy_snapshot.payload.assign(legacy_magic.begin(), legacy_magic.end());
    for (std::size_t index = 0; index < sizeof(legacy_snapshot.created_logical_sequence); ++index) {
        legacy_snapshot.payload.push_back(static_cast<std::uint8_t>(
            legacy_snapshot.created_logical_sequence >> static_cast<unsigned>(index * 8U)
        ));
    }
    ProfileProgressCoordinator migrated_legacy_progress;
    ok &= expect(
        migrated_legacy_progress.restore(legacy_snapshot) ==
                ProfileProgressCoordinatorError::none &&
            migrated_legacy_progress.committed_progress().revision == 5 &&
            migrated_legacy_progress.committed_operation_count() == 0,
        "the previous F1 checkpoint payload migrates to an empty v1 Operation ledger"
    );

    ok &= expect(
        ProfileProgressCoordinator{}.prepare_reward_claim(
            resolution_source,
            reward_one,
            reward_dedup_one
        ).error == ProfileProgressCoordinatorError::invalid_state,
        "a reward claim cannot initialize Profile ownership implicitly"
    );
    ok &= expect(
        migrated_legacy_progress.prepare_reward_claim(0, reward_one, reward_dedup_one).error ==
                ProfileProgressCoordinatorError::invalid_claim &&
            migrated_legacy_progress.prepare_reward_claim(
                resolution_source,
                0,
                reward_dedup_one
            ).error == ProfileProgressCoordinatorError::invalid_claim &&
            migrated_legacy_progress.prepare_reward_claim(
                resolution_source,
                reward_one,
                0
            ).error == ProfileProgressCoordinatorError::invalid_claim,
        "zero source, reward, or dedup identifiers are rejected as invalid claims"
    );

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
