#include <tgd/contracts/tgd_web_abi.h>
#include <tgd/platform/web/web_platform_bridge.hpp>

#include <algorithm>
#include <array>
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
using tgd::contracts::PersistentOperationV1;
using tgd::contracts::QuestUiAttemptTimeClassification;
using tgd::contracts::QuestUiObjectiveState;
using tgd::contracts::QuestUiPolarity;
using tgd::contracts::QuestUiProjectionSnapshot;
using tgd::contracts::QuestUiProjectionSource;
using tgd::contracts::QuestUiRejectionReason;
using tgd::contracts::QuestUiResultStatus;
using tgd::contracts::QuestUiSurface;
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
    const auto reward_operation = tgd::contracts::make_reward_operation(
        profile_id,
        1,
        2,
        0x7100000000000001ULL,
        0x7200000000000001ULL,
        0x7300000000000001ULL
    );
    const std::array<PersistentOperationV1, 1> reward_operations{reward_operation};
    auto stale_reward_operation = reward_operation;
    stale_reward_operation.base_revision = 0;
    const std::array<PersistentOperationV1, 1> stale_reward_operations{
        stale_reward_operation
    };
    std::vector<std::uint8_t> large_write(WebPlatformBridge::max_request_chunk_bytes + 17);
    for (std::size_t index = 0; index < large_write.size(); ++index) {
        large_write[index] = static_cast<std::uint8_t>(index & 0xffU);
    }
    ok &= expect(
        bridge.write_atomic({
            write_context,
            first_head,
            next_head,
            large_write,
            StorageDurability::strict_if_supported,
            stale_reward_operations,
        }) == StorageSubmitError::invalid_request,
        "an atomic Operation cannot claim a base revision older than the expected Head"
    );
    ok &= expect(
        bridge.write_atomic(StorageWriteAtomicRequest{
            write_context,
            first_head,
            next_head,
            large_write,
            StorageDurability::strict_if_supported,
            reward_operations,
        }) == StorageSubmitError::none,
        "large atomic snapshot and Operation enter the bridge together"
    );
    ok &= expect(
        bridge.peek_platform_request_size() == WebPlatformBridge::max_message_bytes,
        "first large write chunk exactly respects the 256 KiB ceiling"
    );
    const auto write_chunk_one = poll_request(bridge);
    ok &= expect(
        read_integer<std::uint16_t>(write_chunk_one, 46) == 1 &&
            read_integer<std::uint32_t>(write_chunk_one, 48) == 0 &&
            read_integer<std::uint32_t>(write_chunk_one, 52) == 2 &&
            read_integer<std::uint32_t>(write_chunk_one, 56) ==
                large_write.size() + tgd::contracts::persistent_operation_v1_bytes &&
            read_integer<std::uint32_t>(write_chunk_one, 60) == 0 &&
            read_integer<std::uint32_t>(write_chunk_one, 64) ==
                WebPlatformBridge::max_request_chunk_bytes &&
            read_integer<std::uint32_t>(write_chunk_one, 68) == large_write.size(),
        "first write chunk declares canonical snapshot and Operation boundaries"
    );
    const auto write_chunk_two = poll_request(bridge);
    ok &= expect(
        write_chunk_two.size() ==
            WebPlatformBridge::message_header_bytes +
                WebPlatformBridge::request_payload_header_bytes + 17 +
                tgd::contracts::persistent_operation_v1_bytes &&
            read_integer<std::uint32_t>(write_chunk_two, 48) == 1 &&
            read_integer<std::uint32_t>(write_chunk_two, 60) ==
                WebPlatformBridge::max_request_chunk_bytes &&
            read_integer<std::uint32_t>(write_chunk_two, 64) ==
                17 + tgd::contracts::persistent_operation_v1_bytes &&
            read_integer<std::uint64_t>(
                write_chunk_two,
                WebPlatformBridge::message_header_bytes +
                    WebPlatformBridge::request_payload_header_bytes + 17
            ) == reward_operation.operation_id.high &&
            bridge.peek_platform_request_size() == 0,
        "last chunk carries the canonical Operation bytes after the snapshot"
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
            command.type == tgd::platform::web::WebUiCommandType::save_guest_checkpoint &&
            command.command_id == snapshot_two && command.session_generation == 9,
        "UI save command decodes through the same ABI header"
    );
    command_payload[0] = static_cast<std::uint8_t>(TGD_WEB_UI_COMMAND_RETRY_PENDING_SAVE);
    const auto retry_message = wrap_message(
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_UI_COMMAND),
        9,
        snapshot_two,
        command_payload
    );
    ok &= expect(
        WebPlatformBridge::decode_ui_command(retry_message, command) == WebAbiError::none &&
            command.type == tgd::platform::web::WebUiCommandType::retry_pending_save &&
            command.command_id == snapshot_two,
        "UI retry command keeps the original pending snapshot inside the coordinator"
    );
    command_payload[0] = 3;
    const auto unknown_command_message = wrap_message(
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_UI_COMMAND),
        9,
        snapshot_two,
        command_payload
    );
    ok &= expect(
        WebPlatformBridge::decode_ui_command(unknown_command_message, command) ==
            WebAbiError::invalid_message,
        "unknown UI commands fail closed"
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

    QuestUiProjectionSnapshot quest_ui_event;
    quest_ui_event.sequence = 41;
    quest_ui_event.tick = 97;
    quest_ui_event.quest_checksum = 0x1111222233334444ULL;
    quest_ui_event.checksum = 0x5555666677778888ULL;
    quest_ui_event.cue = 0x101ULL;
    quest_ui_event.beat = 0x102ULL;
    quest_ui_event.objective = 0x103ULL;
    quest_ui_event.safe_point = 0x104ULL;
    quest_ui_event.pending_objective = 0x103ULL;
    quest_ui_event.source = QuestUiProjectionSource::choice_available;
    quest_ui_event.surface = QuestUiSurface::choice;
    quest_ui_event.polarity = QuestUiPolarity::positive;
    quest_ui_event.objective_state = QuestUiObjectiveState::active;
    quest_ui_event.attempt_time_classification =
        QuestUiAttemptTimeClassification::qualifying_craft_decision;
    quest_ui_event.choice_options[0] = {0x201ULL, 0x301ULL};
    quest_ui_event.choice_options[1] = {0x202ULL, 0x302ULL};
    quest_ui_event.choice_option_count = 2;
    quest_ui_event.selected_options[0] = {0x99ULL, 0x98ULL};
    quest_ui_event.selected_option_count = 1;
    quest_ui_event.active_actor_keys[0] = 104;
    quest_ui_event.active_actor_count = 1;
    quest_ui_event.retained_objectives[0] = 0x90ULL;
    quest_ui_event.retained_objective_count = 1;

    const tgd::platform::web::WebProfileUiEvent saving_ui_event{
        5,
        0,
        true,
        true,
        3,
        12,
        snapshot_two,
    };
    bridge.publish_profile_ui(9, saving_ui_event);
    ok &= expect(
        bridge.publish_quest_ui(9, quest_ui_event) == WebAbiError::none &&
            bridge.peek_ui_event_size() == 80,
        "profile and Quest UI events keep independent pending slots"
    );
    std::vector<std::uint8_t> pending_profile(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(pending_profile) == 80 &&
            read_integer<std::uint16_t>(pending_profile, 4) == TGD_WEB_MESSAGE_UI_EVENT &&
            bridge.peek_ui_event_size() ==
                WebPlatformBridge::message_header_bytes +
                    WebPlatformBridge::quest_ui_event_payload_bytes,
        "profile state drains before the pending Quest projection without replacing it"
    );
    std::vector<std::uint8_t> quest_ui_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(quest_ui_message) ==
                static_cast<std::int32_t>(quest_ui_message.size()) &&
            quest_ui_message.size() == 1'328 &&
            read_integer<std::uint16_t>(quest_ui_message, 4) ==
                TGD_WEB_MESSAGE_QUEST_UI_EVENT &&
            read_integer<std::uint32_t>(quest_ui_message, 8) ==
                TGD_WEB_QUEST_UI_EVENT_V1_BYTES &&
            read_integer<std::uint64_t>(quest_ui_message, 24) ==
                quest_ui_event.checksum &&
            read_integer<std::uint64_t>(quest_ui_message, 32) ==
                quest_ui_event.sequence &&
            read_integer<std::uint64_t>(quest_ui_message, 40) ==
                quest_ui_event.sequence &&
            read_integer<std::uint64_t>(quest_ui_message, 64) ==
                quest_ui_event.checksum &&
            read_integer<std::uint16_t>(quest_ui_message, 144) ==
                static_cast<std::uint16_t>(quest_ui_event.source) &&
            read_integer<std::uint16_t>(quest_ui_message, 162) == 2 &&
            read_integer<std::uint64_t>(quest_ui_message, 176) ==
                quest_ui_event.choice_options[0].interaction &&
            read_integer<std::uint64_t>(quest_ui_message, 184) ==
                quest_ui_event.choice_options[0].selection &&
            bridge.peek_ui_event_size() == 0,
        "Quest projection uses the fixed explicit little-endian v1 payload"
    );
    ok &= expect(
        bridge.publish_quest_ui(9, quest_ui_event) == WebAbiError::none &&
            bridge.peek_ui_event_size() == 0,
        "identical Quest projections are coalesced"
    );

    auto pending_quest_ui_event = quest_ui_event;
    pending_quest_ui_event.sequence = 42;
    pending_quest_ui_event.tick = 98;
    pending_quest_ui_event.checksum = 0x88889999aaaabbbbULL;
    ok &= expect(
        bridge.publish_quest_ui(9, pending_quest_ui_event) == WebAbiError::none,
        "a newer valid Quest projection enters the outbound slot"
    );
    const auto quest_ui_wire_bytes = static_cast<std::uint32_t>(
        WebPlatformBridge::message_header_bytes +
        WebPlatformBridge::quest_ui_event_payload_bytes
    );
    const auto rejects_without_replacing = [&bridge](
                                               QuestUiProjectionSnapshot candidate,
                                               std::string_view label
                                           ) {
        candidate.sequence = 43;
        candidate.tick = 99;
        candidate.checksum = 0x9999aaaabbbbccccULL;
        return expect(
            bridge.publish_quest_ui(9, candidate) == WebAbiError::invalid_message &&
                bridge.peek_ui_event_size() == quest_ui_wire_bytes,
            label
        );
    };
    {
        auto invalid = pending_quest_ui_event;
        invalid.choice_option_count = 9;
        ok &= rejects_without_replacing(
            invalid,
            "over-capacity choice options fail closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.choice_options[2] = {0x203ULL, 0x303ULL};
        ok &= rejects_without_replacing(
            invalid,
            "nonzero choice tail data fails closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.choice_options[1].selection = invalid.choice_options[0].selection;
        ok &= rejects_without_replacing(
            invalid,
            "duplicate authored choice targets fail closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.selected_options[1] = {0x99ULL, 0x97ULL};
        invalid.selected_option_count = 2;
        ok &= rejects_without_replacing(
            invalid,
            "duplicate selected Objective entries fail closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.selected_options[1] = {0x97ULL, 0x96ULL};
        ok &= rejects_without_replacing(
            invalid,
            "nonzero selected-option tail data fails closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.active_actor_keys[1] = 103;
        invalid.active_actor_count = 2;
        ok &= rejects_without_replacing(
            invalid,
            "unsorted active Actor identities fail closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.active_actor_keys[1] = 105;
        ok &= rejects_without_replacing(
            invalid,
            "nonzero active-Actor tail data fails closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.defeated_actor_keys[0] = 104;
        invalid.defeated_actor_count = 1;
        ok &= rejects_without_replacing(
            invalid,
            "an Actor cannot be active and defeated in the same projection"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.retained_objectives[1] = invalid.retained_objectives[0];
        invalid.retained_objective_count = 2;
        ok &= rejects_without_replacing(
            invalid,
            "duplicate retained Objectives fail closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.attempt_time_classification =
            QuestUiAttemptTimeClassification::qualifying_combat_proof;
        ok &= rejects_without_replacing(
            invalid,
            "source-incompatible attempt evidence fails closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.surface = QuestUiSurface::gameplay;
        ok &= rejects_without_replacing(
            invalid,
            "source-incompatible UI surfaces fail closed without replacing the pending event"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.pending_objective = 0;
        ok &= rejects_without_replacing(
            invalid,
            "an active projection focus cannot lose its pending Objective identity"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.primary_result = {
            0x401ULL,
            invalid.objective,
            QuestUiResultStatus::accepted,
            QuestUiRejectionReason::wrong_target,
        };
        ok &= rejects_without_replacing(
            invalid,
            "accepted results carrying a rejection reason fail closed"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.source = QuestUiProjectionSource::interaction_feedback;
        invalid.surface = QuestUiSurface::gameplay;
        invalid.polarity = QuestUiPolarity::negative;
        invalid.attempt_time_classification =
            QuestUiAttemptTimeClassification::repeat_no_progress;
        invalid.choice_options = {};
        invalid.choice_option_count = 0;
        invalid.primary_result = {
            0x401ULL,
            invalid.objective,
            QuestUiResultStatus::ignored_repeat,
            QuestUiRejectionReason::wrong_target,
        };
        ok &= rejects_without_replacing(
            invalid,
            "ignored-repeat results accept only selection_already_committed"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.source = QuestUiProjectionSource::objective_state;
        invalid.surface = QuestUiSurface::gameplay;
        invalid.polarity = QuestUiPolarity::negative;
        invalid.attempt_time_classification =
            QuestUiAttemptTimeClassification::qualifying_training_risk;
        invalid.choice_options = {};
        invalid.choice_option_count = 0;
        ok &= rejects_without_replacing(
            invalid,
            "objective-state projections cannot override Definition-derived polarity"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.source = QuestUiProjectionSource::recovery_offer;
        invalid.surface = QuestUiSurface::failure;
        invalid.polarity = QuestUiPolarity::positive;
        invalid.attempt_time_classification =
            QuestUiAttemptTimeClassification::failure_retry_excluded;
        invalid.choice_options = {};
        invalid.choice_option_count = 0;
        ok &= rejects_without_replacing(
            invalid,
            "recovery projections require recovery polarity"
        );
    }
    {
        auto invalid = pending_quest_ui_event;
        invalid.source = QuestUiProjectionSource::combat_feedback;
        invalid.surface = QuestUiSurface::gameplay;
        invalid.polarity = QuestUiPolarity::negative;
        invalid.attempt_time_classification =
            QuestUiAttemptTimeClassification::qualifying_combat_feedback;
        invalid.choice_options = {};
        invalid.choice_option_count = 0;
        invalid.primary_result = {
            0x401ULL,
            0x400ULL,
            QuestUiResultStatus::rejected,
            QuestUiRejectionReason::wrong_target,
        };
        invalid.secondary_result = {
            0x402ULL,
            invalid.objective,
            QuestUiResultStatus::accepted,
            QuestUiRejectionReason::none,
        };
        ok &= rejects_without_replacing(
            invalid,
            "combat outcomes cannot apply unless the trigger was accepted"
        );
    }
    std::vector<std::uint8_t> retained_quest_ui_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(retained_quest_ui_message) ==
                static_cast<std::int32_t>(retained_quest_ui_message.size()) &&
            read_integer<std::uint64_t>(retained_quest_ui_message, 32) ==
                pending_quest_ui_event.sequence &&
            read_integer<std::uint64_t>(retained_quest_ui_message, 40) ==
                pending_quest_ui_event.sequence &&
            read_integer<std::uint64_t>(retained_quest_ui_message, 64) ==
                pending_quest_ui_event.checksum,
        "all invalid publications preserve the exact prior pending sequence and checksum"
    );
    const auto active_choice_message_sequence =
        read_integer<std::uint64_t>(retained_quest_ui_message, 16);

    const tgd::platform::web::WebQuestUiCloseAck choice_close{
        pending_quest_ui_event.sequence,
        pending_quest_ui_event.checksum,
        TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED,
    };
    ok &= expect(
        bridge.publish_quest_ui_close(9, choice_close) == WebAbiError::none &&
            bridge.publish_quest_ui_close(9, choice_close) == WebAbiError::none &&
            bridge.peek_ui_event_size() ==
                WebPlatformBridge::message_header_bytes +
                    WebPlatformBridge::quest_ui_close_ack_payload_bytes,
        "an exact active-choice close acknowledgement is queued idempotently"
    );
    const auto rejects_close_without_replacing = [&bridge](
                                                     std::uint32_t generation,
                                                     tgd::platform::web::WebQuestUiCloseAck candidate,
                                                     std::string_view label
                                                 ) {
        return expect(
            bridge.publish_quest_ui_close(generation, candidate) ==
                    WebAbiError::invalid_message &&
                bridge.peek_ui_event_size() ==
                    WebPlatformBridge::message_header_bytes +
                        WebPlatformBridge::quest_ui_close_ack_payload_bytes,
            label
        );
    };
    ok &= rejects_close_without_replacing(
        10,
        choice_close,
        "a wrong-generation close cannot replace the pending valid acknowledgement"
    );
    {
        auto invalid = choice_close;
        ++invalid.projection_sequence;
        ok &= rejects_close_without_replacing(
            9,
            invalid,
            "a wrong-sequence close cannot replace the pending valid acknowledgement"
        );
    }
    {
        auto invalid = choice_close;
        ++invalid.projection_checksum;
        ok &= rejects_close_without_replacing(
            9,
            invalid,
            "a wrong-checksum close cannot replace the pending valid acknowledgement"
        );
    }
    {
        auto invalid = choice_close;
        invalid.reason = static_cast<tgd_web_quest_ui_close_reason>(0);
        ok &= rejects_close_without_replacing(
            9,
            invalid,
            "an unknown close reason cannot replace the pending valid acknowledgement"
        );
    }
    std::vector<std::uint8_t> choice_close_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(choice_close_message) ==
                static_cast<std::int32_t>(choice_close_message.size()) &&
            read_integer<std::uint16_t>(choice_close_message, 4) ==
                TGD_WEB_MESSAGE_QUEST_UI_CLOSE_ACK &&
            read_integer<std::uint32_t>(choice_close_message, 8) ==
                TGD_WEB_QUEST_UI_CLOSE_ACK_V1_BYTES &&
            read_integer<std::uint64_t>(choice_close_message, 24) ==
                choice_close.projection_checksum &&
            read_integer<std::uint64_t>(choice_close_message, 32) ==
                choice_close.projection_sequence &&
            read_integer<std::uint64_t>(choice_close_message, 40) ==
                choice_close.projection_sequence &&
            read_integer<std::uint64_t>(choice_close_message, 48) ==
                choice_close.projection_checksum &&
            read_integer<std::uint16_t>(choice_close_message, 56) ==
                TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED &&
            read_integer<std::uint64_t>(choice_close_message, 16) >
                active_choice_message_sequence &&
            bridge.publish_quest_ui_close(9, choice_close) == WebAbiError::none &&
            bridge.peek_ui_event_size() == 0,
        "the fixed close acknowledgement carries exact identity and stays consumed on replay"
    );

    auto combat_quest_ui_event = pending_quest_ui_event;
    combat_quest_ui_event.sequence = 43;
    combat_quest_ui_event.tick = 99;
    combat_quest_ui_event.checksum = 0xbbbbccccddddeeeeULL;
    combat_quest_ui_event.source = QuestUiProjectionSource::combat_feedback;
    combat_quest_ui_event.surface = QuestUiSurface::gameplay;
    combat_quest_ui_event.polarity = QuestUiPolarity::negative;
    combat_quest_ui_event.attempt_time_classification =
        QuestUiAttemptTimeClassification::qualifying_combat_feedback;
    combat_quest_ui_event.choice_options = {};
    combat_quest_ui_event.choice_option_count = 0;
    combat_quest_ui_event.primary_result = {
        0x401ULL,
        0x400ULL,
        QuestUiResultStatus::accepted,
        QuestUiRejectionReason::none,
    };
    combat_quest_ui_event.secondary_result = {
        0x402ULL,
        combat_quest_ui_event.objective,
        QuestUiResultStatus::rejected,
        QuestUiRejectionReason::wrong_target,
    };
    ok &= expect(
        bridge.publish_quest_ui(9, combat_quest_ui_event) == WebAbiError::none &&
            bridge.publish_quest_ui_close(
                9,
                {
                    combat_quest_ui_event.sequence,
                    combat_quest_ui_event.checksum,
                    TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED,
                }
            ) == WebAbiError::invalid_message &&
            bridge.peek_ui_event_size() == quest_ui_wire_bytes,
        "accepted trigger plus rejected outcome remains lossless and cannot pose as a choice close"
    );
    std::vector<std::uint8_t> combat_quest_ui_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(combat_quest_ui_message) ==
                static_cast<std::int32_t>(combat_quest_ui_message.size()) &&
            read_integer<std::uint16_t>(combat_quest_ui_message, 154) ==
                static_cast<std::uint16_t>(QuestUiResultStatus::accepted) &&
            read_integer<std::uint16_t>(combat_quest_ui_message, 158) ==
                static_cast<std::uint16_t>(QuestUiResultStatus::rejected) &&
            read_integer<std::uint16_t>(combat_quest_ui_message, 160) ==
                static_cast<std::uint16_t>(QuestUiRejectionReason::wrong_target),
        "the Web envelope preserves independent trigger and outcome status/reason slots"
    );

    auto next_choice = pending_quest_ui_event;
    next_choice.sequence = 44;
    next_choice.tick = 100;
    next_choice.checksum = 0x11119999aaaabbbbULL;
    const tgd::platform::web::WebQuestUiCloseAck next_choice_close{
        next_choice.sequence,
        next_choice.checksum,
        TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED,
    };
    ok &= expect(
        bridge.publish_quest_ui(9, next_choice) == WebAbiError::none &&
            bridge.publish_quest_ui_close(9, next_choice_close) == WebAbiError::none,
        "a later choice can queue its own exact close acknowledgement"
    );
    auto newest_choice = next_choice;
    newest_choice.sequence = 45;
    newest_choice.tick = 101;
    newest_choice.checksum = 0x2222aaaabbbbccccULL;
    ok &= expect(
        bridge.publish_quest_ui(9, newest_choice) == WebAbiError::none &&
            bridge.peek_ui_event_size() == quest_ui_wire_bytes,
        "a new choice projection discards an older pending close before it can close the new panel"
    );
    std::vector<std::uint8_t> newest_choice_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(newest_choice_message) ==
                static_cast<std::int32_t>(newest_choice_message.size()) &&
            read_integer<std::uint64_t>(newest_choice_message, 40) ==
                newest_choice.sequence &&
            bridge.publish_quest_ui_close(9, next_choice_close) ==
                WebAbiError::invalid_message &&
            bridge.peek_ui_event_size() == 0,
        "an old close identity cannot be replayed after a new choice becomes authoritative"
    );
    auto replacement_feedback = combat_quest_ui_event;
    replacement_feedback.sequence = 46;
    replacement_feedback.tick = 102;
    replacement_feedback.checksum = 0x3333bbbbccccddddULL;
    const tgd::platform::web::WebQuestUiCloseAck newest_choice_close{
        newest_choice.sequence,
        newest_choice.checksum,
        TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED,
    };
    ok &= expect(
        bridge.publish_quest_ui(9, replacement_feedback) == WebAbiError::none &&
            bridge.publish_quest_ui_close(9, newest_choice_close) == WebAbiError::none &&
            bridge.peek_ui_event_size() == quest_ui_wire_bytes,
        "an authoritative replacement makes the matching close redundant without queuing it"
    );
    std::vector<std::uint8_t> replacement_message(bridge.peek_ui_event_size());
    ok &= expect(
        bridge.poll_ui_event(replacement_message) ==
                static_cast<std::int32_t>(replacement_message.size()) &&
            read_integer<std::uint64_t>(replacement_message, 40) ==
                replacement_feedback.sequence &&
            bridge.peek_ui_event_size() == 0,
        "replace-before-close emits only the newer authoritative projection"
    );

    std::vector<std::uint8_t> intent_payload;
    append_integer(intent_payload, quest_ui_event.sequence);
    append_integer(intent_payload, quest_ui_event.checksum);
    append_integer(intent_payload, quest_ui_event.objective);
    append_integer(intent_payload, quest_ui_event.choice_options[1].interaction);
    append_integer(intent_payload, quest_ui_event.choice_options[1].selection);
    const auto intent_message = wrap_message(
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_QUEST_UI_SELECTION_INTENT),
        9,
        {quest_ui_event.checksum, quest_ui_event.sequence},
        intent_payload
    );
    tgd::platform::web::WebQuestUiSelectionIntent decoded_intent;
    ok &= expect(
        WebPlatformBridge::decode_quest_ui_selection_intent(
            intent_message,
            decoded_intent
        ) == WebAbiError::none &&
            decoded_intent.session_generation == 9 &&
            decoded_intent.intent.projection_sequence == quest_ui_event.sequence &&
            decoded_intent.intent.projection_checksum == quest_ui_event.checksum &&
            decoded_intent.intent.objective == quest_ui_event.objective &&
            decoded_intent.intent.interaction ==
                quest_ui_event.choice_options[1].interaction &&
            decoded_intent.intent.selection == quest_ui_event.choice_options[1].selection,
        "Quest selection intent binds the exact projection and authored option"
    );
    auto mismatched_intent_message = intent_message;
    mismatched_intent_message[24] ^= 1U;
    const auto accepted_intent = decoded_intent;
    ok &= expect(
        WebPlatformBridge::decode_quest_ui_selection_intent(
            mismatched_intent_message,
            decoded_intent
        ) == WebAbiError::invalid_message &&
            decoded_intent.session_generation == accepted_intent.session_generation &&
            decoded_intent.intent.projection_sequence ==
                accepted_intent.intent.projection_sequence &&
            decoded_intent.intent.projection_checksum ==
                accepted_intent.intent.projection_checksum &&
            decoded_intent.intent.objective == accepted_intent.intent.objective &&
            decoded_intent.intent.interaction == accepted_intent.intent.interaction &&
            decoded_intent.intent.selection == accepted_intent.intent.selection,
        "mismatched intent identity fails closed without mutating the prior decoded value"
    );
    auto empty_selection_intent_message = intent_message;
    std::fill(
        empty_selection_intent_message.begin() +
            static_cast<std::ptrdiff_t>(WebPlatformBridge::message_header_bytes + 32),
        empty_selection_intent_message.begin() +
            static_cast<std::ptrdiff_t>(WebPlatformBridge::message_header_bytes + 40),
        std::uint8_t{0}
    );
    ok &= expect(
        WebPlatformBridge::decode_quest_ui_selection_intent(
            empty_selection_intent_message,
            decoded_intent
        ) == WebAbiError::invalid_message &&
            decoded_intent.intent.selection == accepted_intent.intent.selection,
        "malformed intent payloads fail closed without partially replacing the output"
    );

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
