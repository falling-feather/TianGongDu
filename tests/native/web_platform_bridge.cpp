#include <tgd/contracts/tgd_web_abi.h>
#include <tgd/platform/web/web_platform_bridge.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using tgd::contracts::StableId128;
using tgd::platform::web::WebAbiError;
using tgd::platform::web::WebPlatformBridge;
using tgd::runtime::StorageChannel;
using tgd::runtime::StorageCompletion;
using tgd::runtime::StorageCompletionError;
using tgd::runtime::StorageDurability;
using tgd::runtime::StorageKey;
using tgd::runtime::StorageOperation;
using tgd::runtime::StorageProfileHead;
using tgd::runtime::StorageReadRequest;
using tgd::runtime::StorageRecordKind;
using tgd::runtime::StorageRequestContext;
using tgd::runtime::StorageSubmitError;
using tgd::runtime::StorageWriteAtomicRequest;

constexpr StableId128 profile_id{0x0102030405060708ULL, 0x1112131415161718ULL};
constexpr StableId128 snapshot_one{0x2122232425262728ULL, 0x3132333435363738ULL};
constexpr StableId128 snapshot_two{0x4142434445464748ULL, 0x5152535455565758ULL};
constexpr StableId128 package_set_id{0x6162636465666768ULL, 0x7172737475767778ULL};
constexpr StableId128 request_seed{0x8182838485868788ULL, 0x9192939495969798ULL};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "web platform bridge failure: " << message << '\n';
    }
    return condition;
}

template <typename Integer>
void append_integer(std::vector<std::uint8_t>& bytes, Integer value) {
    static_assert(std::is_integral_v<Integer>);
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(
            static_cast<std::uint8_t>(bits >> static_cast<unsigned>(index * 8U))
        );
    }
}

void append_id(std::vector<std::uint8_t>& bytes, StableId128 id) {
    append_integer(bytes, id.high);
    append_integer(bytes, id.low);
}

void append_head(std::vector<std::uint8_t>& bytes, const StorageProfileHead& head) {
    append_id(bytes, head.profile_id);
    append_id(bytes, head.snapshot_id);
    append_integer(bytes, head.logical_sequence);
    bytes.insert(bytes.end(), head.envelope_hash.begin(), head.envelope_hash.end());
}

[[nodiscard]] std::vector<std::uint8_t> wrap_message(
    std::uint16_t message_type,
    std::uint32_t generation,
    StableId128 request_id,
    std::span<const std::uint8_t> payload
) {
    std::vector<std::uint8_t> message;
    message.reserve(WebPlatformBridge::message_header_bytes + payload.size());
    append_integer(message, static_cast<std::uint16_t>(TGD_WEB_ABI_MAJOR));
    append_integer(message, static_cast<std::uint16_t>(TGD_WEB_ABI_MINOR));
    append_integer(message, message_type);
    append_integer(message, static_cast<std::uint16_t>(1));
    append_integer(message, static_cast<std::uint32_t>(payload.size()));
    append_integer(message, generation);
    append_integer(message, static_cast<std::uint64_t>(1));
    append_id(message, request_id);
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

[[nodiscard]] std::vector<std::uint8_t> completion_message(
    const StorageRequestContext& context,
    StorageOperation operation,
    StorageCompletionError error,
    const StorageKey& key,
    const StorageProfileHead& head,
    std::uint32_t chunk_index,
    std::uint32_t chunk_count,
    std::uint32_t total_bytes,
    std::uint32_t chunk_offset,
    std::span<const std::uint8_t> chunk
) {
    std::vector<std::uint8_t> payload;
    payload.reserve(WebPlatformBridge::completion_payload_header_bytes + chunk.size());
    append_integer(payload, static_cast<std::uint16_t>(operation));
    append_integer(payload, static_cast<std::uint16_t>(error));
    append_integer(payload, static_cast<std::uint16_t>(key.kind));
    append_integer(payload, static_cast<std::uint16_t>(0));
    append_integer(payload, chunk_index);
    append_integer(payload, chunk_count);
    append_integer(payload, total_bytes);
    append_integer(payload, chunk_offset);
    append_integer(payload, static_cast<std::uint32_t>(chunk.size()));
    append_integer(payload, static_cast<std::uint32_t>(0));
    append_id(payload, key.profile_id);
    append_id(payload, key.record_id);
    append_head(payload, head);
    append_integer(payload, static_cast<std::uint64_t>(0));
    append_integer(payload, static_cast<std::uint64_t>(0));
    payload.insert(payload.end(), chunk.begin(), chunk.end());
    return wrap_message(
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_STORAGE_COMPLETION),
        context.session_generation,
        context.request_id,
        payload
    );
}

template <typename Integer>
[[nodiscard]] Integer read_integer(std::span<const std::uint8_t> bytes, std::size_t offset) {
    static_assert(std::is_unsigned_v<Integer>);
    Integer value = 0;
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        value |= static_cast<Integer>(bytes[offset + index])
                 << static_cast<unsigned>(index * 8U);
    }
    return value;
}

[[nodiscard]] std::vector<std::uint8_t> poll_request(WebPlatformBridge& bridge) {
    std::vector<std::uint8_t> message(bridge.peek_platform_request_size());
    const auto written = bridge.poll_platform_request(message);
    if (written <= 0 || static_cast<std::size_t>(written) != message.size()) {
        std::abort();
    }
    return message;
}

[[nodiscard]] StorageProfileHead head(StableId128 snapshot_id, std::uint64_t sequence) {
    StorageProfileHead result;
    result.profile_id = profile_id;
    result.snapshot_id = snapshot_id;
    result.logical_sequence = sequence;
    result.envelope_hash[0] = static_cast<std::uint8_t>(sequence);
    return result;
}

}  // namespace

int main() {
    bool ok = true;
    WebPlatformBridge bridge;
    const StorageRequestContext read_context{
        {request_seed.high, request_seed.low + 1},
        7,
        StorageChannel::prototype_f1,
    };
    const StorageKey head_key{StorageRecordKind::profile_head, profile_id, {}};
    ok &= expect(
        bridge.read(StorageReadRequest{read_context, head_key}) == StorageSubmitError::none,
        "Head read enters the outbound bridge"
    );
    ok &= expect(
        bridge.peek_platform_request_size() ==
            WebPlatformBridge::message_header_bytes +
                WebPlatformBridge::request_payload_header_bytes,
        "fixed read request size is exact"
    );
    const auto head_request = poll_request(bridge);
    ok &= expect(
        read_integer<std::uint16_t>(head_request, 4) == TGD_WEB_MESSAGE_STORAGE_REQUEST &&
            read_integer<std::uint32_t>(head_request, 8) ==
                WebPlatformBridge::request_payload_header_bytes &&
            read_integer<std::uint32_t>(head_request, 12) == read_context.session_generation &&
            read_integer<std::uint64_t>(head_request, 24) == read_context.request_id.high &&
            read_integer<std::uint64_t>(head_request, 32) == read_context.request_id.low &&
            read_integer<std::uint16_t>(head_request, 40) ==
                static_cast<std::uint16_t>(StorageOperation::read) &&
            read_integer<std::uint16_t>(head_request, 42) ==
                static_cast<std::uint16_t>(StorageRecordKind::profile_head),
        "read request uses the explicit little-endian ABI header"
    );
    ok &= expect(
        bridge.read(StorageReadRequest{read_context, head_key}) ==
            StorageSubmitError::backpressure,
        "one outstanding request owns the bridge slot"
    );

    const auto first_head = head(snapshot_one, 1);
    const auto head_completion = completion_message(
        read_context,
        StorageOperation::read,
        StorageCompletionError::none,
        head_key,
        first_head,
        0,
        1,
        0,
        0,
        {}
    );
    ok &= expect(
        bridge.accept_async_completion(head_completion) == WebAbiError::none,
        "Head completion is accepted"
    );
    StorageCompletion completion;
    ok &= expect(
        bridge.poll_completion(read_context.request_id, completion) &&
            completion.head == first_head && completion.key == head_key,
        "Head completion round-trips into IStorage"
    );

    const StorageRequestContext write_context{
        {request_seed.high, request_seed.low + 2},
        7,
        StorageChannel::prototype_f1,
    };
    const auto next_head = head(snapshot_two, 2);
    std::vector<std::uint8_t> large_write(WebPlatformBridge::max_request_chunk_bytes + 17);
    for (std::size_t index = 0; index < large_write.size(); ++index) {
        large_write[index] = static_cast<std::uint8_t>(index & 0xffU);
    }
    ok &= expect(
        bridge.write_atomic(StorageWriteAtomicRequest{
            write_context,
            first_head,
            next_head,
            large_write,
            StorageDurability::strict_if_supported,
        }) == StorageSubmitError::none,
        "large atomic write enters the bridge"
    );
    ok &= expect(
        bridge.peek_platform_request_size() == WebPlatformBridge::max_message_bytes,
        "first large write chunk exactly respects the 256 KiB ceiling"
    );
    const auto write_chunk_one = poll_request(bridge);
    ok &= expect(
        read_integer<std::uint32_t>(write_chunk_one, 48) == 0 &&
            read_integer<std::uint32_t>(write_chunk_one, 52) == 2 &&
            read_integer<std::uint32_t>(write_chunk_one, 56) == large_write.size() &&
            read_integer<std::uint32_t>(write_chunk_one, 60) == 0 &&
            read_integer<std::uint32_t>(write_chunk_one, 64) ==
                WebPlatformBridge::max_request_chunk_bytes,
        "first write chunk declares canonical offset and total"
    );
    const auto write_chunk_two = poll_request(bridge);
    ok &= expect(
        write_chunk_two.size() ==
            WebPlatformBridge::message_header_bytes +
                WebPlatformBridge::request_payload_header_bytes + 17 &&
            read_integer<std::uint32_t>(write_chunk_two, 48) == 1 &&
            read_integer<std::uint32_t>(write_chunk_two, 60) ==
                WebPlatformBridge::max_request_chunk_bytes &&
            read_integer<std::uint32_t>(write_chunk_two, 64) == 17 &&
            bridge.peek_platform_request_size() == 0,
        "last write chunk is bounded and closes the outbound transfer"
    );
    const StorageKey next_key{StorageRecordKind::snapshot, profile_id, snapshot_two};
    ok &= expect(
        bridge.accept_async_completion(completion_message(
            write_context,
            StorageOperation::write_atomic,
            StorageCompletionError::none,
            next_key,
            next_head,
            0,
            1,
            0,
            0,
            {}
        )) == WebAbiError::none &&
            bridge.poll_completion(write_context.request_id, completion) &&
            completion.head == next_head,
        "atomic write ack returns only after the final outbound chunk"
    );

    const StorageRequestContext snapshot_read_context{
        {request_seed.high, request_seed.low + 3},
        7,
        StorageChannel::prototype_f1,
    };
    const StorageKey snapshot_key{StorageRecordKind::snapshot, profile_id, snapshot_two};
    ok &= bridge.read(StorageReadRequest{snapshot_read_context, snapshot_key}) ==
          StorageSubmitError::none;
    static_cast<void>(poll_request(bridge));
    std::vector<std::uint8_t> large_read(WebPlatformBridge::max_completion_chunk_bytes + 9);
    for (std::size_t index = 0; index < large_read.size(); ++index) {
        large_read[index] = static_cast<std::uint8_t>((index * 3U) & 0xffU);
    }
    const auto inbound_one = completion_message(
        snapshot_read_context,
        StorageOperation::read,
        StorageCompletionError::none,
        snapshot_key,
        {},
        0,
        2,
        static_cast<std::uint32_t>(large_read.size()),
        0,
        std::span<const std::uint8_t>{large_read}.first(
            WebPlatformBridge::max_completion_chunk_bytes
        )
    );
    ok &= expect(
        inbound_one.size() == WebPlatformBridge::max_message_bytes &&
            bridge.accept_async_completion(inbound_one) == WebAbiError::none &&
            !bridge.poll_completion(snapshot_read_context.request_id, completion),
        "first inbound chunk is retained but not exposed as a completion"
    );
    const auto inbound_two = completion_message(
        snapshot_read_context,
        StorageOperation::read,
        StorageCompletionError::none,
        snapshot_key,
        {},
        1,
        2,
        static_cast<std::uint32_t>(large_read.size()),
        static_cast<std::uint32_t>(WebPlatformBridge::max_completion_chunk_bytes),
        std::span<const std::uint8_t>{large_read}.last(9)
    );
    ok &= expect(
        bridge.accept_async_completion(inbound_two) == WebAbiError::none &&
            bridge.poll_completion(snapshot_read_context.request_id, completion) &&
            completion.bytes == large_read,
        "ordered inbound chunks reassemble byte-for-byte"
    );
    ok &= expect(
        bridge.accept_async_completion(inbound_two) == WebAbiError::duplicate_completion,
        "late duplicate completion fails closed"
    );

    std::vector<std::uint8_t> boot_payload;
    append_id(boot_payload, profile_id);
    append_id(boot_payload, package_set_id);
    append_id(boot_payload, request_seed);
    append_integer(boot_payload, static_cast<std::uint32_t>(9));
    auto boot_message = wrap_message(
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_BOOT_CONFIG),
        9,
        request_seed,
        boot_payload
    );
    tgd::platform::web::WebBootConfig boot;
    ok &= expect(
        WebPlatformBridge::decode_boot_config(boot_message, boot) == WebAbiError::none &&
            boot.profile_id == profile_id && boot.package_set_id == package_set_id &&
            boot.request_id_seed == request_seed && boot.session_generation == 9,
        "boot config preserves 128-bit identifiers without JS numbers"
    );
    boot_message[0] = 2;
    ok &= expect(
        WebPlatformBridge::decode_boot_config(boot_message, boot) ==
            WebAbiError::incompatible_abi,
        "unknown boot ABI major is rejected"
    );

    std::vector<std::uint8_t> command_payload;
    append_integer(
        command_payload,
        static_cast<std::uint16_t>(
            tgd::platform::web::WebUiCommandType::save_guest_checkpoint
        )
    );
    append_integer(
        command_payload,
        static_cast<std::uint16_t>(tgd::contracts::CheckpointKind::safe_point)
    );
    append_id(command_payload, snapshot_two);
    const auto command_message = wrap_message(
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_UI_COMMAND),
        9,
        snapshot_two,
        command_payload
    );
    tgd::platform::web::WebUiCommand command;
    ok &= expect(
        WebPlatformBridge::decode_ui_command(command_message, command) == WebAbiError::none &&
            command.snapshot_id == snapshot_two && command.session_generation == 9,
        "UI save command decodes through the same ABI header"
    );

    const tgd::platform::web::WebProfileUiEvent ui_event{
        4,
        0,
        true,
        false,
        3,
        12,
        snapshot_two,
    };
    bridge.publish_profile_ui(9, ui_event);
    ok &= expect(bridge.peek_ui_event_size() == 80, "profile UI event has a fixed v1 size");
    std::vector<std::uint8_t> ui_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(ui_message) == 80 &&
            read_integer<std::uint16_t>(ui_message, 4) == TGD_WEB_MESSAGE_UI_EVENT &&
            read_integer<std::uint16_t>(ui_message, 40) == ui_event.state &&
            read_integer<std::uint64_t>(ui_message, 48) == ui_event.committed_save_count &&
            bridge.peek_ui_event_size() == 0,
        "profile UI event is explicit, little-endian, and consumed once"
    );
    bridge.publish_profile_ui(9, ui_event);
    ok &= expect(bridge.peek_ui_event_size() == 0, "identical UI state is coalesced");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
