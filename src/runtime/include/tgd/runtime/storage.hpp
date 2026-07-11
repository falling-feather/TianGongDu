#pragma once

#include <tgd/contracts/save_envelope.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace tgd::runtime {

enum class StorageChannel : std::uint8_t {
    prototype_f1 = 1,
};

enum class StorageOperation : std::uint8_t {
    read = 1,
    write_atomic = 2,
    list = 3,
    delete_record = 4,
    estimate_quota = 5,
    request_persistence = 6,
};

enum class StorageRecordKind : std::uint8_t {
    profile_head = 1,
    snapshot = 2,
    operation = 3,
    profile_meta = 4,
    device_settings = 5,
    migration_workspace = 6,
};

enum class StorageDurability : std::uint8_t {
    relaxed,
    strict_if_supported,
};

enum class StorageSubmitError : std::uint8_t {
    none,
    invalid_request,
    backpressure,
    unavailable,
    allocation_failed,
};

enum class StorageCompletionError : std::uint8_t {
    none,
    not_found,
    conflict,
    quota,
    corrupt,
    unavailable,
    cancelled,
    timeout,
    invalid_request,
    internal,
};

struct StorageRequestContext final {
    contracts::StableId128 request_id{};
    std::uint32_t session_generation{};
    StorageChannel channel{StorageChannel::prototype_f1};

    [[nodiscard]] friend constexpr bool operator==(
        const StorageRequestContext&,
        const StorageRequestContext&
    ) noexcept = default;
};

struct StorageKey final {
    StorageRecordKind kind{StorageRecordKind::profile_head};
    contracts::StableId128 profile_id{};
    contracts::StableId128 record_id{};

    [[nodiscard]] friend constexpr bool operator==(
        const StorageKey&,
        const StorageKey&
    ) noexcept = default;
};

struct StorageProfileHead final {
    contracts::StableId128 profile_id{};
    contracts::StableId128 snapshot_id{};
    std::uint64_t logical_sequence{};
    contracts::Sha256Digest envelope_hash{};

    [[nodiscard]] friend constexpr bool operator==(
        const StorageProfileHead&,
        const StorageProfileHead&
    ) noexcept = default;
};

struct StorageReadRequest final {
    StorageRequestContext context{};
    StorageKey key{};
};

struct StorageWriteAtomicRequest final {
    StorageRequestContext context{};
    StorageProfileHead expected_head{};
    StorageProfileHead next_head{};
    std::span<const std::uint8_t> snapshot_bytes{};
    StorageDurability durability{StorageDurability::relaxed};
};

struct StorageListRequest final {
    StorageRequestContext context{};
    StorageRecordKind kind{StorageRecordKind::snapshot};
    contracts::StableId128 profile_id{};
};

struct StorageDeleteRequest final {
    StorageRequestContext context{};
    StorageKey key{};
};

struct StorageEstimateQuotaRequest final {
    StorageRequestContext context{};
};

struct StoragePersistenceRequest final {
    StorageRequestContext context{};
};

struct StorageRecordMetadata final {
    StorageKey key{};
    std::uint64_t byte_size{};
};

struct StorageCompletion final {
    StorageRequestContext context{};
    StorageOperation operation{StorageOperation::read};
    StorageCompletionError error{StorageCompletionError::none};
    StorageKey key{};
    StorageProfileHead head{};
    std::vector<std::uint8_t> bytes{};
    std::vector<StorageRecordMetadata> records{};
    std::uint64_t usage_bytes{};
    std::uint64_t quota_bytes{};
    bool persistence_granted{};
};

class IStorage {
  public:
    virtual ~IStorage() = default;

    // Implementations must validate and copy every request span before returning. A successful
    // write_atomic completion is emitted only after the snapshot, Head CAS, and transaction
    // completion all succeed. strict_if_supported is a capability hint, not an fsync promise.
    [[nodiscard]] virtual StorageSubmitError read(
        const StorageReadRequest& request
    ) noexcept = 0;
    [[nodiscard]] virtual StorageSubmitError write_atomic(
        const StorageWriteAtomicRequest& request
    ) noexcept = 0;
    [[nodiscard]] virtual StorageSubmitError list(
        const StorageListRequest& request
    ) noexcept = 0;
    [[nodiscard]] virtual StorageSubmitError delete_record(
        const StorageDeleteRequest& request
    ) noexcept = 0;
    [[nodiscard]] virtual StorageSubmitError estimate_quota(
        const StorageEstimateQuotaRequest& request
    ) noexcept = 0;
    [[nodiscard]] virtual StorageSubmitError request_persistence(
        const StoragePersistenceRequest& request
    ) noexcept = 0;

    // Targeted polling prevents one coordinator from consuming another profile's completion.
    [[nodiscard]] virtual bool poll_completion(
        contracts::StableId128 request_id,
        StorageCompletion& output
    ) noexcept = 0;
};

}  // namespace tgd::runtime
