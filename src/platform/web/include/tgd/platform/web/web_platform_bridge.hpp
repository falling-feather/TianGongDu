#pragma once

#include <tgd/contracts/save_envelope.hpp>
#include <tgd/contracts/tgd_web_abi.h>
#include <tgd/runtime/storage.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tgd::platform::web {

enum class WebAbiError : std::int32_t {
    none = 0,
    incompatible_abi = 1,
    invalid_message = 2,
    buffer_too_small = 3,
    unknown_message_type = 4,
    payload_too_large = 5,
    backpressure = 6,
    duplicate_completion = 7,
    cancelled = 8,
    stale_generation = 9,
    timeout = 10,
    storage_not_found = 11,
    storage_conflict = 12,
    storage_quota = 13,
    storage_corrupt = 14,
    storage_unavailable = 15,
    internal = 16,
};

enum class WebUiCommandType : std::uint16_t {
    save_guest_checkpoint = 1,
};

struct WebBootConfig final {
    contracts::StableId128 profile_id{};
    contracts::StableId128 package_set_id{};
    contracts::StableId128 request_id_seed{};
    std::uint32_t session_generation{};
};

struct WebUiCommand final {
    WebUiCommandType type{WebUiCommandType::save_guest_checkpoint};
    contracts::CheckpointKind checkpoint_kind{contracts::CheckpointKind::safe_point};
    contracts::StableId128 snapshot_id{};
    std::uint32_t session_generation{};
};

struct WebProfileUiEvent final {
    std::uint16_t state{};
    std::uint16_t error{};
    bool has_snapshot{};
    bool has_pending_save{};
    std::uint64_t committed_save_count{};
    std::uint64_t logical_sequence{};
    contracts::StableId128 snapshot_id{};

    [[nodiscard]] friend constexpr bool operator==(
        const WebProfileUiEvent&,
        const WebProfileUiEvent&
    ) noexcept = default;
};

class WebPlatformBridge final : public runtime::IStorage {
  public:
    static constexpr std::size_t message_header_bytes = TGD_WEB_ABI_MESSAGE_HEADER_BYTES;
    static constexpr std::size_t request_payload_header_bytes =
        TGD_WEB_STORAGE_REQUEST_V1_HEADER_BYTES;
    static constexpr std::size_t completion_payload_header_bytes =
        TGD_WEB_STORAGE_COMPLETION_V1_HEADER_BYTES;
    static constexpr std::size_t max_message_bytes = TGD_WEB_ABI_MAX_MESSAGE_BYTES;
    static constexpr std::size_t max_transfer_bytes = TGD_WEB_MAX_STORAGE_TRANSFER_BYTES;
    static constexpr std::size_t max_request_chunk_bytes =
        max_message_bytes - message_header_bytes - request_payload_header_bytes;
    static constexpr std::size_t max_completion_chunk_bytes =
        max_message_bytes - message_header_bytes - completion_payload_header_bytes;

    [[nodiscard]] runtime::StorageSubmitError read(
        const runtime::StorageReadRequest& request
    ) noexcept override;
    [[nodiscard]] runtime::StorageSubmitError write_atomic(
        const runtime::StorageWriteAtomicRequest& request
    ) noexcept override;
    [[nodiscard]] runtime::StorageSubmitError list(
        const runtime::StorageListRequest& request
    ) noexcept override;
    [[nodiscard]] runtime::StorageSubmitError delete_record(
        const runtime::StorageDeleteRequest& request
    ) noexcept override;
    [[nodiscard]] runtime::StorageSubmitError estimate_quota(
        const runtime::StorageEstimateQuotaRequest& request
    ) noexcept override;
    [[nodiscard]] runtime::StorageSubmitError request_persistence(
        const runtime::StoragePersistenceRequest& request
    ) noexcept override;
    [[nodiscard]] bool poll_completion(
        contracts::StableId128 request_id,
        runtime::StorageCompletion& output
    ) noexcept override;

    [[nodiscard]] std::uint32_t peek_platform_request_size() const noexcept;
    [[nodiscard]] std::int32_t poll_platform_request(
        std::span<std::uint8_t> output
    ) noexcept;
    [[nodiscard]] WebAbiError accept_async_completion(
        std::span<const std::uint8_t> message
    ) noexcept;

    void publish_profile_ui(
        std::uint32_t session_generation,
        const WebProfileUiEvent& event
    ) noexcept;
    [[nodiscard]] std::uint32_t peek_ui_event_size() const noexcept;
    [[nodiscard]] std::int32_t poll_ui_event(std::span<std::uint8_t> output) noexcept;
    void reset() noexcept;

    [[nodiscard]] static WebAbiError decode_boot_config(
        std::span<const std::uint8_t> message,
        WebBootConfig& output
    ) noexcept;
    [[nodiscard]] static WebAbiError decode_ui_command(
        std::span<const std::uint8_t> message,
        WebUiCommand& output
    ) noexcept;

  private:
    enum class OutboundKind : std::uint8_t {
        none,
        read,
        write_atomic,
        list,
        delete_record,
        estimate_quota,
        request_persistence,
    };

    struct OutboundRequest final {
        OutboundKind kind{OutboundKind::none};
        runtime::StorageRequestContext context{};
        runtime::StorageKey key{};
        runtime::StorageProfileHead expected_head{};
        runtime::StorageProfileHead next_head{};
        runtime::StorageDurability durability{runtime::StorageDurability::relaxed};
        std::vector<std::uint8_t> bytes{};
        std::uint32_t chunk_index{};
        std::uint32_t chunk_count{1};
        std::uint32_t chunk_offset{};
    };

    struct OutstandingRequest final {
        runtime::StorageRequestContext context{};
        runtime::StorageOperation operation{runtime::StorageOperation::read};
    };

    struct InboundTransfer final {
        runtime::StorageCompletion completion{};
        std::uint32_t chunk_count{};
        std::uint32_t next_chunk_index{};
        std::uint32_t next_chunk_offset{};
        std::uint32_t total_bytes{};
    };

    [[nodiscard]] bool request_slot_available() const noexcept;
    [[nodiscard]] runtime::StorageSubmitError begin_request(
        OutboundKind kind,
        const runtime::StorageRequestContext& context,
        const runtime::StorageKey& key
    ) noexcept;
    [[nodiscard]] std::uint32_t current_request_chunk_size() const noexcept;
    [[nodiscard]] runtime::StorageOperation outbound_operation() const noexcept;
    [[nodiscard]] WebAbiError finish_completion(
        runtime::StorageCompletion&& completion
    ) noexcept;

    OutboundRequest outbound_{};
    std::optional<OutstandingRequest> outstanding_{};
    std::optional<InboundTransfer> inbound_{};
    std::vector<runtime::StorageCompletion> completions_{};
    std::uint64_t wire_sequence_{};
    bool ui_dirty_{};
    std::uint32_t ui_session_generation_{};
    WebProfileUiEvent ui_event_{};
};

}  // namespace tgd::platform::web
