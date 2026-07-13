#pragma once

#include <tgd/contracts/save_envelope.hpp>
#include <tgd/runtime/storage.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace tgd::runtime {

enum class ProfileStorageState : std::uint8_t {
    uninitialized,
    configured,
    loading_head,
    loading_snapshot,
    ready,
    saving,
    restore_failed,
    save_failed,
    conflict_read_only,
    recovery_required,
    storage_unavailable,
};

enum class ProfileStorageError : std::uint8_t {
    none,
    invalid_config,
    invalid_state,
    invalid_snapshot,
    allocation_failed,
    backpressure,
    storage_unavailable,
    storage_conflict,
    storage_quota,
    storage_corrupt,
    cancelled,
    timeout,
    protocol_violation,
    internal,
};

struct ProfileStorageConfig final {
    contracts::StableId128 profile_id{};
    contracts::StableId128 package_set_id{};
    contracts::StableId128 request_id_seed{};
    std::uint32_t session_generation{};
    StorageChannel channel{StorageChannel::prototype_f1};
};

struct ProfileStoragePumpResult final {
    bool completion_consumed{};
    bool state_changed{};
    ProfileStorageError error{ProfileStorageError::none};
};

class ProfileStorageCoordinator final {
  public:
    [[nodiscard]] ProfileStorageError initialize(
        IStorage& storage,
        const ProfileStorageConfig& config
    ) noexcept;
    [[nodiscard]] ProfileStorageError begin_restore() noexcept;
    [[nodiscard]] ProfileStorageError begin_save(
        const contracts::SaveEnvelopeV1& snapshot,
        StorageDurability durability = StorageDurability::relaxed,
        std::span<const contracts::PersistentOperationV1> operations = {}
    ) noexcept;
    [[nodiscard]] ProfileStorageError retry_pending_save() noexcept;
    [[nodiscard]] ProfileStoragePumpResult pump() noexcept;

    [[nodiscard]] ProfileStorageState state() const noexcept;
    [[nodiscard]] ProfileStorageError last_error() const noexcept;
    [[nodiscard]] bool has_snapshot() const noexcept;
    [[nodiscard]] bool has_pending_save() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> current_snapshot_bytes() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> pending_snapshot_bytes() const noexcept;
    [[nodiscard]] std::span<const contracts::PersistentOperationV1>
    pending_operations() const noexcept;
    [[nodiscard]] const StorageProfileHead& current_head() const noexcept;
    [[nodiscard]] std::uint64_t committed_save_count() const noexcept;
    [[nodiscard]] contracts::StableId128 last_committed_request_id() const noexcept;
    [[nodiscard]] std::uint64_t ignored_stale_completion_count() const noexcept;

  private:
    [[nodiscard]] contracts::StableId128 next_request_id() noexcept;
    [[nodiscard]] ProfileStorageError submit_head_read() noexcept;
    [[nodiscard]] ProfileStorageError submit_snapshot_read(
        const StorageProfileHead& head
    ) noexcept;
    [[nodiscard]] ProfileStorageError submit_pending_save() noexcept;
    [[nodiscard]] ProfileStoragePumpResult handle_head_completion(
        StorageCompletion&& completion
    ) noexcept;
    [[nodiscard]] ProfileStoragePumpResult handle_snapshot_completion(
        StorageCompletion&& completion
    ) noexcept;
    [[nodiscard]] ProfileStoragePumpResult handle_save_completion(
        StorageCompletion&& completion
    ) noexcept;
    [[nodiscard]] ProfileStoragePumpResult fail_active(
        ProfileStorageState next_state,
        ProfileStorageError error
    ) noexcept;
    void clear_active_request() noexcept;

    IStorage* storage_{};
    ProfileStorageConfig config_{};
    ProfileStorageState state_{ProfileStorageState::uninitialized};
    ProfileStorageError last_error_{ProfileStorageError::none};
    contracts::StableId128 active_request_id_{};
    StorageOperation active_operation_{StorageOperation::read};
    std::uint64_t request_counter_{};
    StorageProfileHead restore_head_{};
    StorageProfileHead current_head_{};
    StorageProfileHead pending_expected_head_{};
    StorageProfileHead pending_head_{};
    bool has_current_head_{};
    StorageDurability pending_durability_{StorageDurability::relaxed};
    std::vector<std::uint8_t> current_snapshot_bytes_{};
    std::vector<std::uint8_t> pending_snapshot_bytes_{};
    std::vector<contracts::PersistentOperationV1> pending_operations_{};
    std::uint64_t committed_save_count_{};
    contracts::StableId128 last_committed_request_id_{};
    std::uint64_t ignored_stale_completion_count_{};
};

}  // namespace tgd::runtime
