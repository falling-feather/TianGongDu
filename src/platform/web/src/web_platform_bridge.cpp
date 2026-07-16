#include <tgd/platform/web/web_platform_bridge.hpp>

#include <tgd/contracts/tgd_web_abi.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace tgd::platform::web {
namespace {

static_assert(WebPlatformBridge::message_header_bytes == sizeof(tgd_web_abi_message_header));
static_assert(
    WebPlatformBridge::max_transfer_bytes ==
    contracts::save_envelope_v1_header_bytes + contracts::max_save_payload_bytes
);
static_assert(
    TGD_WEB_PERSISTENT_OPERATION_V1_BYTES == contracts::persistent_operation_v1_bytes
);
static_assert(
    TGD_WEB_QUEST_UI_SELECTION_INTENT_V1_BYTES ==
    sizeof(std::uint64_t) * 5U
);
static_assert(
    TGD_WEB_QUEST_UI_CLOSE_ACK_V1_BYTES ==
    sizeof(std::uint64_t) * 2U + sizeof(std::uint16_t) * 4U
);
static_assert(
    TGD_WEB_QUEST_UI_EVENT_V1_BYTES ==
    sizeof(std::uint64_t) * 13U +
        sizeof(std::uint16_t) * 16U +
        contracts::quest_ui_choice_option_capacity * sizeof(std::uint64_t) * 2U +
        contracts::quest_ui_selected_option_capacity * sizeof(std::uint64_t) * 2U +
        contracts::quest_ui_actor_capacity * sizeof(std::uint64_t) * 2U +
        contracts::quest_ui_retained_objective_capacity * sizeof(std::uint64_t)
);
static_assert(
    TGD_WEB_QUEST_UI_CHOICE_OPTION_CAPACITY == contracts::quest_ui_choice_option_capacity &&
    TGD_WEB_QUEST_UI_SELECTED_OPTION_CAPACITY ==
        contracts::quest_ui_selected_option_capacity &&
    TGD_WEB_QUEST_UI_ACTOR_CAPACITY == contracts::quest_ui_actor_capacity &&
    TGD_WEB_QUEST_UI_RETAINED_OBJECTIVE_CAPACITY ==
        contracts::quest_ui_retained_objective_capacity
);
static_assert(
    static_cast<std::uint16_t>(runtime::StorageOperation::write_atomic) ==
    TGD_WEB_STORAGE_WRITE_ATOMIC
);
static_assert(static_cast<std::int32_t>(WebAbiError::internal) == TGD_WEB_ERROR_INTERNAL);

struct MessageHeader final {
    std::uint16_t message_type{};
    std::uint16_t payload_version{};
    std::uint32_t payload_length{};
    std::uint32_t session_generation{};
    std::uint64_t sequence{};
    contracts::StableId128 request_id{};
};

class ByteWriter final {
  public:
    explicit ByteWriter(std::span<std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    template <typename Integer>
    [[nodiscard]] bool write(Integer value) noexcept {
        static_assert(std::is_integral_v<Integer>);
        if (remaining() < sizeof(Integer)) {
            return false;
        }
        using Unsigned = std::make_unsigned_t<Integer>;
        const auto bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            bytes_[offset_ + index] =
                static_cast<std::uint8_t>(bits >> static_cast<unsigned>(index * 8U));
        }
        offset_ += sizeof(Integer);
        return true;
    }

    [[nodiscard]] bool write_id(contracts::StableId128 id) noexcept {
        return write(id.high) && write(id.low);
    }

    [[nodiscard]] bool write_digest(const contracts::Sha256Digest& digest) noexcept {
        return write_bytes(digest);
    }

    [[nodiscard]] bool write_bytes(std::span<const std::uint8_t> bytes) noexcept {
        if (remaining() < bytes.size()) {
            return false;
        }
        std::copy(bytes.begin(), bytes.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(offset_));
        offset_ += bytes.size();
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return offset_;
    }

  private:
    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

    std::span<std::uint8_t> bytes_{};
    std::size_t offset_{};
};

class ByteReader final {
  public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    template <typename Integer>
    [[nodiscard]] bool read(Integer& value) noexcept {
        static_assert(std::is_integral_v<Integer>);
        if (remaining() < sizeof(Integer)) {
            return false;
        }
        using Unsigned = std::make_unsigned_t<Integer>;
        Unsigned bits = 0;
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            bits |= static_cast<Unsigned>(bytes_[offset_ + index])
                    << static_cast<unsigned>(index * 8U);
        }
        if constexpr (std::is_signed_v<Integer>) {
            value = std::bit_cast<Integer>(bits);
        } else {
            value = bits;
        }
        offset_ += sizeof(Integer);
        return true;
    }

    [[nodiscard]] bool read_id(contracts::StableId128& id) noexcept {
        return read(id.high) && read(id.low);
    }

    [[nodiscard]] bool read_digest(contracts::Sha256Digest& digest) noexcept {
        if (remaining() < digest.size()) {
            return false;
        }
        std::copy_n(bytes_.data() + offset_, digest.size(), digest.data());
        offset_ += digest.size();
        return true;
    }

    [[nodiscard]] std::span<const std::uint8_t> remaining_bytes() const noexcept {
        return bytes_.subspan(offset_);
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

  private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_{};
};

[[nodiscard]] bool valid_context(const runtime::StorageRequestContext& context) noexcept {
    return !context.request_id.empty() && context.session_generation != 0 &&
           context.channel == runtime::StorageChannel::prototype_f1;
}

[[nodiscard]] bool valid_record_kind(runtime::StorageRecordKind kind) noexcept {
    return kind >= runtime::StorageRecordKind::profile_head &&
           kind <= runtime::StorageRecordKind::migration_workspace;
}

[[nodiscard]] bool write_head(ByteWriter& writer, const runtime::StorageProfileHead& head) noexcept {
    return writer.write_id(head.profile_id) && writer.write_id(head.snapshot_id) &&
           writer.write(head.logical_sequence) && writer.write_digest(head.envelope_hash);
}

[[nodiscard]] bool read_head(ByteReader& reader, runtime::StorageProfileHead& head) noexcept {
    return reader.read_id(head.profile_id) && reader.read_id(head.snapshot_id) &&
           reader.read(head.logical_sequence) && reader.read_digest(head.envelope_hash);
}

[[nodiscard]] bool write_operation(
    ByteWriter& writer,
    const contracts::PersistentOperationV1& operation
) noexcept {
    return writer.write_id(operation.operation_id) && writer.write_id(operation.profile_id) &&
           writer.write(operation.base_revision) &&
           writer.write(operation.created_logical_time) &&
           writer.write(static_cast<std::uint16_t>(operation.domain)) &&
           writer.write(operation.payload_version) && writer.write(static_cast<std::uint32_t>(0)) &&
           writer.write(operation.source_id) && writer.write(operation.reward_id) &&
           writer.write(operation.reward_dedup_key);
}

[[nodiscard]] bool valid_atomic_operations(
    const runtime::StorageWriteAtomicRequest& request
) noexcept {
    if (request.operations.size() > TGD_WEB_MAX_ATOMIC_OPERATIONS_PER_WRITE) {
        return false;
    }
    for (std::size_t index = 0; index < request.operations.size(); ++index) {
        const auto& operation = request.operations[index];
        if (operation.operation_id.empty() ||
            operation.operation_id != contracts::reward_operation_id(
                                          request.next_head.profile_id,
                                          operation.reward_dedup_key
                                      ) ||
            operation.profile_id != request.next_head.profile_id ||
            operation.base_revision < request.expected_head.logical_sequence ||
            operation.base_revision >= operation.created_logical_time ||
            operation.created_logical_time > request.next_head.logical_sequence ||
            operation.domain != contracts::PersistentOperationDomain::quest_reward ||
            operation.payload_version != 1 || operation.source_id == 0 ||
            operation.reward_id == 0 || operation.reward_dedup_key == 0) {
            return false;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (request.operations[prior].operation_id == operation.operation_id ||
                request.operations[prior].reward_dedup_key == operation.reward_dedup_key) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool valid_quest_ui_result_slot(
    const contracts::QuestUiResultSlot& slot
) noexcept {
    using contracts::QuestUiRejectionReason;
    using contracts::QuestUiResultStatus;

    if (slot.status < QuestUiResultStatus::not_applicable ||
        slot.status > QuestUiResultStatus::pending ||
        slot.rejection_reason < QuestUiRejectionReason::none ||
        slot.rejection_reason > QuestUiRejectionReason::wrong_target) {
        return false;
    }
    if (slot.status == QuestUiResultStatus::not_applicable) {
        return slot.id == 0 && slot.objective == 0 &&
               slot.rejection_reason == QuestUiRejectionReason::none;
    }
    if (slot.id == 0 || slot.objective == 0) {
        return false;
    }
    switch (slot.status) {
        case QuestUiResultStatus::accepted:
        case QuestUiResultStatus::pending:
            return slot.rejection_reason == QuestUiRejectionReason::none;
        case QuestUiResultStatus::rejected:
            return slot.rejection_reason != QuestUiRejectionReason::none;
        case QuestUiResultStatus::ignored_repeat:
            return slot.rejection_reason ==
                   QuestUiRejectionReason::selection_already_committed;
        case QuestUiResultStatus::not_applicable:
            break;
    }
    return false;
}

[[nodiscard]] bool quest_ui_result_not_applicable(
    const contracts::QuestUiResultSlot& slot
) noexcept {
    return slot.status == contracts::QuestUiResultStatus::not_applicable &&
           slot.id == 0 && slot.objective == 0 &&
           slot.rejection_reason == contracts::QuestUiRejectionReason::none;
}

[[nodiscard]] bool quest_ui_result_is_negative(
    const contracts::QuestUiResultSlot& slot
) noexcept {
    return slot.status == contracts::QuestUiResultStatus::rejected ||
           slot.status == contracts::QuestUiResultStatus::ignored_repeat;
}

[[nodiscard]] bool quest_ui_classification_matches_source(
    contracts::QuestUiProjectionSource source,
    contracts::QuestUiAttemptTimeClassification classification
) noexcept {
    using Attempt = contracts::QuestUiAttemptTimeClassification;
    using Source = contracts::QuestUiProjectionSource;
    switch (source) {
        case Source::choice_available:
            return classification == Attempt::qualifying_first_visit ||
                   classification == Attempt::qualifying_craft_decision ||
                   classification == Attempt::qualifying_dialogue_decision;
        case Source::interaction_feedback:
            return classification == Attempt::repeat_no_progress ||
                   classification == Attempt::qualifying_craft_decision ||
                   classification == Attempt::qualifying_error_feedback ||
                   classification == Attempt::qualifying_wrong_order_feedback ||
                   classification == Attempt::qualifying_craft_confirmation;
        case Source::objective_state:
            return classification == Attempt::qualifying_first_visit ||
                   classification == Attempt::qualifying_training_risk;
        case Source::combat_feedback:
            return classification == Attempt::qualifying_combat_proof ||
                   classification == Attempt::qualifying_combat_feedback;
        case Source::recovery_offer:
            return classification == Attempt::failure_retry_excluded;
        case Source::recovery_resume:
            return classification == Attempt::resume_no_duplicate_progress;
    }
    return false;
}

[[nodiscard]] bool quest_ui_source_shape_valid(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    using Source = contracts::QuestUiProjectionSource;
    using Status = contracts::QuestUiResultStatus;
    switch (event.source) {
        case Source::choice_available:
        case Source::objective_state:
        case Source::recovery_offer:
        case Source::recovery_resume:
            return quest_ui_result_not_applicable(event.primary_result) &&
                   quest_ui_result_not_applicable(event.secondary_result);
        case Source::interaction_feedback:
            return event.primary_result.status != Status::not_applicable &&
                   event.primary_result.status != Status::pending &&
                   quest_ui_result_not_applicable(event.secondary_result);
        case Source::combat_feedback:
            if (event.primary_result.status == Status::not_applicable ||
                event.primary_result.status == Status::ignored_repeat ||
                event.secondary_result.status == Status::ignored_repeat) {
                return false;
            }
            return quest_ui_result_not_applicable(event.secondary_result) ||
                   event.primary_result.status == Status::accepted;
    }
    return false;
}

[[nodiscard]] bool quest_ui_surface_and_polarity_valid(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    using Polarity = contracts::QuestUiPolarity;
    using Source = contracts::QuestUiProjectionSource;
    using Surface = contracts::QuestUiSurface;

    const auto expected_surface = event.source == Source::choice_available
                                      ? Surface::choice
                                  : event.source == Source::recovery_offer
                                      ? Surface::failure
                                      : Surface::gameplay;
    if (event.surface != expected_surface) {
        return false;
    }
    const bool recovery = event.source == Source::recovery_offer ||
                          event.source == Source::recovery_resume;
    if (recovery) {
        return event.polarity == Polarity::recovery;
    }
    if (event.polarity == Polarity::recovery) {
        return false;
    }
    if (event.source == Source::choice_available ||
        event.source == Source::objective_state) {
        return event.polarity == Polarity::positive;
    }
    const auto& effective_result = quest_ui_result_not_applicable(event.secondary_result)
                                       ? event.primary_result
                                       : event.secondary_result;
    return !quest_ui_result_is_negative(effective_result) ||
           event.polarity == Polarity::negative;
}

[[nodiscard]] bool quest_ui_choice_options_valid(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    const bool choice =
        event.source == contracts::QuestUiProjectionSource::choice_available;
    if ((choice && event.choice_option_count == 0) ||
        (!choice && event.choice_option_count != 0) ||
        event.choice_option_count > event.choice_options.size()) {
        return false;
    }
    for (std::size_t index = 0; index < event.choice_options.size(); ++index) {
        const auto& option = event.choice_options[index];
        if (index >= event.choice_option_count) {
            if (option.interaction != 0 || option.selection != 0) {
                return false;
            }
            continue;
        }
        if (option.interaction == 0 || option.selection == 0) {
            return false;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (event.choice_options[prior].interaction == option.interaction ||
                event.choice_options[prior].selection == option.selection) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool quest_ui_selected_options_valid(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    if (event.selected_option_count > event.selected_options.size()) {
        return false;
    }
    for (std::size_t index = 0; index < event.selected_options.size(); ++index) {
        const auto& option = event.selected_options[index];
        if (index >= event.selected_option_count) {
            if (option.objective != 0 || option.selection != 0) {
                return false;
            }
            continue;
        }
        if (option.objective == 0 || option.selection == 0) {
            return false;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (event.selected_options[prior].objective == option.objective) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool quest_ui_actors_valid(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    if (event.active_actor_count > event.active_actor_keys.size() ||
        event.defeated_actor_count > event.defeated_actor_keys.size()) {
        return false;
    }
    for (std::size_t index = 0; index < event.active_actor_keys.size(); ++index) {
        const auto actor = event.active_actor_keys[index];
        if (index >= event.active_actor_count) {
            if (actor != 0) {
                return false;
            }
            continue;
        }
        if (actor == 0 ||
            (index != 0 && event.active_actor_keys[index - 1] >= actor)) {
            return false;
        }
        for (std::size_t defeated = 0; defeated < event.defeated_actor_count; ++defeated) {
            if (event.defeated_actor_keys[defeated] == actor) {
                return false;
            }
        }
    }
    for (std::size_t index = 0; index < event.defeated_actor_keys.size(); ++index) {
        const auto actor = event.defeated_actor_keys[index];
        if (index >= event.defeated_actor_count) {
            if (actor != 0) {
                return false;
            }
            continue;
        }
        if (actor == 0 ||
            (index != 0 && event.defeated_actor_keys[index - 1] >= actor)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool quest_ui_retained_objectives_valid(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    if (event.retained_objective_count > event.retained_objectives.size()) {
        return false;
    }
    for (std::size_t index = 0; index < event.retained_objectives.size(); ++index) {
        const auto objective = event.retained_objectives[index];
        if (index >= event.retained_objective_count) {
            if (objective != 0) {
                return false;
            }
            continue;
        }
        if (objective == 0) {
            return false;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (event.retained_objectives[prior] == objective) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool valid_quest_ui_event(
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    using contracts::QuestUiAttemptTimeClassification;
    using contracts::QuestUiObjectiveState;
    using contracts::QuestUiPolarity;
    using contracts::QuestUiProjectionSource;
    using contracts::QuestUiSurface;

    const bool pending_focus_valid =
        event.objective_state == QuestUiObjectiveState::active
            ? event.pending_objective == event.objective
            : event.pending_objective == 0;
    return event.sequence != 0 && event.quest_checksum != 0 && event.checksum != 0 &&
           event.cue != 0 && event.beat != 0 && event.objective != 0 &&
           event.safe_point != 0 && pending_focus_valid &&
           event.source >= QuestUiProjectionSource::choice_available &&
           event.source <= QuestUiProjectionSource::recovery_resume &&
           event.surface >= QuestUiSurface::gameplay && event.surface <= QuestUiSurface::failure &&
           event.polarity >= QuestUiPolarity::positive &&
           event.polarity <= QuestUiPolarity::recovery &&
           event.objective_state >= QuestUiObjectiveState::locked &&
           event.objective_state <= QuestUiObjectiveState::completed &&
           event.attempt_time_classification > QuestUiAttemptTimeClassification::unspecified &&
           event.attempt_time_classification <=
               QuestUiAttemptTimeClassification::resume_no_duplicate_progress &&
           quest_ui_classification_matches_source(
               event.source,
               event.attempt_time_classification
           ) &&
           valid_quest_ui_result_slot(event.primary_result) &&
           valid_quest_ui_result_slot(event.secondary_result) &&
           quest_ui_source_shape_valid(event) &&
           quest_ui_surface_and_polarity_valid(event) &&
           quest_ui_choice_options_valid(event) &&
           quest_ui_selected_options_valid(event) && quest_ui_actors_valid(event) &&
           quest_ui_retained_objectives_valid(event);
}

[[nodiscard]] bool write_quest_ui_event(
    ByteWriter& writer,
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    const auto write_result_identity = [&writer](const contracts::QuestUiResultSlot& result) {
        return writer.write(result.id) && writer.write(result.objective);
    };
    if (!writer.write(event.sequence) || !writer.write(event.tick) ||
        !writer.write(event.quest_checksum) || !writer.write(event.checksum) ||
        !writer.write(event.cue) || !writer.write(event.beat) ||
        !writer.write(event.objective) || !writer.write(event.safe_point) ||
        !writer.write(event.pending_objective) ||
        !write_result_identity(event.primary_result) ||
        !write_result_identity(event.secondary_result) ||
        !writer.write(static_cast<std::uint16_t>(event.source)) ||
        !writer.write(static_cast<std::uint16_t>(event.surface)) ||
        !writer.write(static_cast<std::uint16_t>(event.polarity)) ||
        !writer.write(static_cast<std::uint16_t>(event.objective_state)) ||
        !writer.write(static_cast<std::uint16_t>(event.attempt_time_classification)) ||
        !writer.write(static_cast<std::uint16_t>(event.primary_result.status)) ||
        !writer.write(static_cast<std::uint16_t>(event.primary_result.rejection_reason)) ||
        !writer.write(static_cast<std::uint16_t>(event.secondary_result.status)) ||
        !writer.write(static_cast<std::uint16_t>(event.secondary_result.rejection_reason)) ||
        !writer.write(static_cast<std::uint16_t>(event.choice_option_count)) ||
        !writer.write(static_cast<std::uint16_t>(event.selected_option_count)) ||
        !writer.write(static_cast<std::uint16_t>(event.active_actor_count)) ||
        !writer.write(static_cast<std::uint16_t>(event.defeated_actor_count)) ||
        !writer.write(static_cast<std::uint16_t>(event.retained_objective_count)) ||
        !writer.write(static_cast<std::uint16_t>(0)) ||
        !writer.write(static_cast<std::uint16_t>(0))) {
        return false;
    }
    for (const auto& option : event.choice_options) {
        if (!writer.write(option.interaction) || !writer.write(option.selection)) {
            return false;
        }
    }
    for (const auto& option : event.selected_options) {
        if (!writer.write(option.objective) || !writer.write(option.selection)) {
            return false;
        }
    }
    for (const auto actor : event.active_actor_keys) {
        if (!writer.write(actor)) {
            return false;
        }
    }
    for (const auto actor : event.defeated_actor_keys) {
        if (!writer.write(actor)) {
            return false;
        }
    }
    for (const auto objective : event.retained_objectives) {
        if (!writer.write(objective)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool write_quest_ui_close_ack(
    ByteWriter& writer,
    const WebQuestUiCloseAck& acknowledgement
) noexcept {
    return writer.write(acknowledgement.projection_sequence) &&
           writer.write(acknowledgement.projection_checksum) &&
           writer.write(static_cast<std::uint16_t>(acknowledgement.reason)) &&
           writer.write(static_cast<std::uint16_t>(0)) &&
           writer.write(static_cast<std::uint16_t>(0)) &&
           writer.write(static_cast<std::uint16_t>(0));
}

[[nodiscard]] bool write_message_header(
    ByteWriter& writer,
    std::uint16_t message_type,
    std::uint32_t payload_length,
    std::uint32_t session_generation,
    std::uint64_t sequence,
    contracts::StableId128 request_id
) noexcept {
    return writer.write(static_cast<std::uint16_t>(TGD_WEB_ABI_MAJOR)) &&
           writer.write(static_cast<std::uint16_t>(TGD_WEB_ABI_MINOR)) &&
           writer.write(message_type) && writer.write(static_cast<std::uint16_t>(1)) &&
           writer.write(payload_length) && writer.write(session_generation) &&
           writer.write(sequence) && writer.write_id(request_id);
}

[[nodiscard]] WebAbiError parse_message(
    std::span<const std::uint8_t> message,
    std::uint16_t expected_type,
    MessageHeader& header,
    std::span<const std::uint8_t>& payload
) noexcept {
    if (message.size() < WebPlatformBridge::message_header_bytes ||
        message.size() > WebPlatformBridge::max_message_bytes) {
        return message.size() > WebPlatformBridge::max_message_bytes
                   ? WebAbiError::payload_too_large
                   : WebAbiError::invalid_message;
    }

    ByteReader reader(message);
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    if (!reader.read(major) || !reader.read(minor) || !reader.read(header.message_type) ||
        !reader.read(header.payload_version) || !reader.read(header.payload_length) ||
        !reader.read(header.session_generation) || !reader.read(header.sequence) ||
        !reader.read_id(header.request_id)) {
        return WebAbiError::invalid_message;
    }
    if (major != TGD_WEB_ABI_MAJOR || minor > TGD_WEB_ABI_MINOR) {
        return WebAbiError::incompatible_abi;
    }
    if (header.message_type != expected_type) {
        return WebAbiError::unknown_message_type;
    }
    if (header.payload_version != 1 ||
        header.payload_length != reader.remaining() ||
        header.payload_length + WebPlatformBridge::message_header_bytes != message.size()) {
        return WebAbiError::invalid_message;
    }
    payload = reader.remaining_bytes();
    return WebAbiError::none;
}

[[nodiscard]] bool completion_metadata_equal(
    const runtime::StorageCompletion& left,
    const runtime::StorageCompletion& right
) noexcept {
    return left.context == right.context && left.operation == right.operation &&
           left.error == right.error && left.key == right.key && left.head == right.head &&
           left.usage_bytes == right.usage_bytes && left.quota_bytes == right.quota_bytes &&
           left.persistence_granted == right.persistence_granted;
}

}  // namespace

runtime::StorageSubmitError WebPlatformBridge::read(
    const runtime::StorageReadRequest& request
) noexcept {
    if (!valid_context(request.context) || request.key.profile_id.empty() ||
        !valid_record_kind(request.key.kind) ||
        (request.key.kind == runtime::StorageRecordKind::profile_head &&
         !request.key.record_id.empty()) ||
        (request.key.kind != runtime::StorageRecordKind::profile_head &&
         request.key.record_id.empty())) {
        return runtime::StorageSubmitError::invalid_request;
    }
    return begin_request(OutboundKind::read, request.context, request.key);
}

runtime::StorageSubmitError WebPlatformBridge::write_atomic(
    const runtime::StorageWriteAtomicRequest& request
) noexcept {
    if (request.operations.size() > TGD_WEB_MAX_ATOMIC_OPERATIONS_PER_WRITE) {
        return runtime::StorageSubmitError::invalid_request;
    }
    const auto operation_bytes =
        request.operations.size() * contracts::persistent_operation_v1_bytes;
    if (!valid_context(request.context) || request.snapshot_bytes.empty() ||
        request.snapshot_bytes.size() > max_transfer_bytes ||
        operation_bytes > max_transfer_bytes - request.snapshot_bytes.size() ||
        request.expected_head.profile_id != request.next_head.profile_id ||
        request.next_head.profile_id.empty() || request.next_head.snapshot_id.empty() ||
        request.next_head.logical_sequence <= request.expected_head.logical_sequence ||
        !valid_atomic_operations(request)) {
        return runtime::StorageSubmitError::invalid_request;
    }
    if (!request_slot_available()) {
        return runtime::StorageSubmitError::backpressure;
    }

    try {
        outbound_ = {};
        outbound_.kind = OutboundKind::write_atomic;
        outbound_.context = request.context;
        outbound_.key = {
            runtime::StorageRecordKind::snapshot,
            request.next_head.profile_id,
            request.next_head.snapshot_id,
        };
        outbound_.expected_head = request.expected_head;
        outbound_.next_head = request.next_head;
        outbound_.durability = request.durability;
        outbound_.snapshot_byte_length =
            static_cast<std::uint32_t>(request.snapshot_bytes.size());
        outbound_.operation_count = static_cast<std::uint16_t>(request.operations.size());
        outbound_.bytes.resize(request.snapshot_bytes.size() + operation_bytes);
        std::copy(
            request.snapshot_bytes.begin(),
            request.snapshot_bytes.end(),
            outbound_.bytes.begin()
        );
        ByteWriter operation_writer(
            std::span<std::uint8_t>{outbound_.bytes}.subspan(request.snapshot_bytes.size())
        );
        for (const auto& operation : request.operations) {
            if (!write_operation(operation_writer, operation)) {
                outbound_ = {};
                return runtime::StorageSubmitError::invalid_request;
            }
        }
        if (operation_writer.size() != operation_bytes) {
            outbound_ = {};
            return runtime::StorageSubmitError::invalid_request;
        }
        outbound_.chunk_count = static_cast<std::uint32_t>(
            (outbound_.bytes.size() + max_request_chunk_bytes - 1) /
            max_request_chunk_bytes
        );
        return runtime::StorageSubmitError::none;
    } catch (const std::bad_alloc&) {
        outbound_ = {};
        return runtime::StorageSubmitError::allocation_failed;
    }
}

runtime::StorageSubmitError WebPlatformBridge::list(
    const runtime::StorageListRequest& request
) noexcept {
    if (!valid_context(request.context) || request.profile_id.empty() ||
        !valid_record_kind(request.kind)) {
        return runtime::StorageSubmitError::invalid_request;
    }
    return begin_request(
        OutboundKind::list,
        request.context,
        {request.kind, request.profile_id, {}}
    );
}

runtime::StorageSubmitError WebPlatformBridge::delete_record(
    const runtime::StorageDeleteRequest& request
) noexcept {
    if (!valid_context(request.context) || request.key.profile_id.empty() ||
        !valid_record_kind(request.key.kind) ||
        (request.key.kind != runtime::StorageRecordKind::profile_head &&
         request.key.record_id.empty())) {
        return runtime::StorageSubmitError::invalid_request;
    }
    return begin_request(OutboundKind::delete_record, request.context, request.key);
}

runtime::StorageSubmitError WebPlatformBridge::estimate_quota(
    const runtime::StorageEstimateQuotaRequest& request
) noexcept {
    if (!valid_context(request.context)) {
        return runtime::StorageSubmitError::invalid_request;
    }
    return begin_request(OutboundKind::estimate_quota, request.context, {});
}

runtime::StorageSubmitError WebPlatformBridge::request_persistence(
    const runtime::StoragePersistenceRequest& request
) noexcept {
    if (!valid_context(request.context)) {
        return runtime::StorageSubmitError::invalid_request;
    }
    return begin_request(OutboundKind::request_persistence, request.context, {});
}

bool WebPlatformBridge::poll_completion(
    contracts::StableId128 request_id,
    runtime::StorageCompletion& output
) noexcept {
    const auto completion = std::find_if(
        completions_.begin(),
        completions_.end(),
        [&](const runtime::StorageCompletion& candidate) {
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

std::uint32_t WebPlatformBridge::peek_platform_request_size() const noexcept {
    if (outbound_.kind == OutboundKind::none) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        message_header_bytes + request_payload_header_bytes + current_request_chunk_size()
    );
}

std::int32_t WebPlatformBridge::poll_platform_request(
    std::span<std::uint8_t> output
) noexcept {
    const auto required = peek_platform_request_size();
    if (required == 0) {
        return 0;
    }
    if (output.size() < required) {
        return -static_cast<std::int32_t>(WebAbiError::buffer_too_small);
    }

    const auto operation = outbound_operation();
    const auto chunk_size = current_request_chunk_size();
    const auto payload_size = static_cast<std::uint32_t>(request_payload_header_bytes + chunk_size);
    ByteWriter writer(output.first(required));
    const auto wrote_header = write_message_header(
        writer,
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_STORAGE_REQUEST),
        payload_size,
        outbound_.context.session_generation,
        ++wire_sequence_,
        outbound_.context.request_id
    );
    const auto total_bytes = static_cast<std::uint32_t>(outbound_.bytes.size());
    const auto wrote_payload =
        writer.write(static_cast<std::uint16_t>(operation)) &&
        writer.write(static_cast<std::uint16_t>(outbound_.key.kind)) &&
        writer.write(static_cast<std::uint16_t>(outbound_.durability)) &&
        writer.write(outbound_.operation_count) && writer.write(outbound_.chunk_index) &&
        writer.write(outbound_.chunk_count) && writer.write(total_bytes) &&
        writer.write(outbound_.chunk_offset) && writer.write(chunk_size) &&
        writer.write(outbound_.snapshot_byte_length) &&
        writer.write_id(outbound_.key.profile_id) && writer.write_id(outbound_.key.record_id) &&
        write_head(writer, outbound_.expected_head) && write_head(writer, outbound_.next_head) &&
        writer.write_bytes(
            std::span<const std::uint8_t>{outbound_.bytes}.subspan(
                outbound_.chunk_offset,
                chunk_size
            )
        );
    if (!wrote_header || !wrote_payload || writer.size() != required) {
        return -static_cast<std::int32_t>(WebAbiError::internal);
    }

    outbound_.chunk_offset += chunk_size;
    ++outbound_.chunk_index;
    if (outbound_.chunk_index == outbound_.chunk_count) {
        outstanding_ = OutstandingRequest{outbound_.context, operation};
        outbound_ = {};
    }
    return static_cast<std::int32_t>(required);
}

WebAbiError WebPlatformBridge::accept_async_completion(
    std::span<const std::uint8_t> message
) noexcept {
    MessageHeader header;
    std::span<const std::uint8_t> payload;
    const auto parsed = parse_message(
        message,
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_STORAGE_COMPLETION),
        header,
        payload
    );
    if (parsed != WebAbiError::none) {
        return parsed;
    }
    if (!outstanding_.has_value() || header.request_id != outstanding_->context.request_id) {
        return WebAbiError::duplicate_completion;
    }
    if (payload.size() < completion_payload_header_bytes) {
        return WebAbiError::invalid_message;
    }

    ByteReader reader(payload);
    std::uint16_t operation_value = 0;
    std::uint16_t error_value = 0;
    std::uint16_t kind_value = 0;
    std::uint16_t flags = 0;
    std::uint32_t chunk_index = 0;
    std::uint32_t chunk_count = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t chunk_offset = 0;
    std::uint32_t chunk_length = 0;
    std::uint32_t reserved = 0;
    runtime::StorageCompletion completion;
    completion.context = {
        header.request_id,
        header.session_generation,
        runtime::StorageChannel::prototype_f1,
    };
    if (!reader.read(operation_value) || !reader.read(error_value) ||
        !reader.read(kind_value) || !reader.read(flags) || !reader.read(chunk_index) ||
        !reader.read(chunk_count) || !reader.read(total_bytes) ||
        !reader.read(chunk_offset) || !reader.read(chunk_length) ||
        !reader.read(reserved) || !reader.read_id(completion.key.profile_id) ||
        !reader.read_id(completion.key.record_id) || !read_head(reader, completion.head) ||
        !reader.read(completion.usage_bytes) || !reader.read(completion.quota_bytes)) {
        return WebAbiError::invalid_message;
    }
    if (operation_value < static_cast<std::uint16_t>(runtime::StorageOperation::read) ||
        operation_value > static_cast<std::uint16_t>(runtime::StorageOperation::request_persistence) ||
        error_value > static_cast<std::uint16_t>(runtime::StorageCompletionError::internal) ||
        kind_value < static_cast<std::uint16_t>(runtime::StorageRecordKind::profile_head) ||
        kind_value > static_cast<std::uint16_t>(runtime::StorageRecordKind::migration_workspace) ||
        (flags & ~std::uint16_t{1}) != 0 || reserved != 0 || chunk_count == 0 ||
        total_bytes > max_transfer_bytes || chunk_length != reader.remaining() ||
        chunk_length > max_completion_chunk_bytes) {
        return total_bytes > max_transfer_bytes ? WebAbiError::payload_too_large
                                                : WebAbiError::invalid_message;
    }
    completion.operation = static_cast<runtime::StorageOperation>(operation_value);
    completion.error = static_cast<runtime::StorageCompletionError>(error_value);
    completion.key.kind = static_cast<runtime::StorageRecordKind>(kind_value);
    completion.persistence_granted = (flags & 1U) != 0;
    if (completion.operation != outstanding_->operation) {
        return WebAbiError::invalid_message;
    }
    if (completion.error != runtime::StorageCompletionError::none &&
        (total_bytes != 0 || chunk_length != 0 || chunk_count != 1 || chunk_index != 0 ||
         chunk_offset != 0)) {
        return WebAbiError::invalid_message;
    }

    if (total_bytes == 0) {
        if (chunk_count != 1 || chunk_index != 0 || chunk_offset != 0 || chunk_length != 0) {
            return WebAbiError::invalid_message;
        }
        return finish_completion(std::move(completion));
    }
    if (completion.error != runtime::StorageCompletionError::none || chunk_length == 0 ||
        chunk_offset > total_bytes || chunk_length > total_bytes - chunk_offset) {
        return WebAbiError::invalid_message;
    }

    try {
        if (!inbound_.has_value()) {
            if (chunk_index != 0 || chunk_offset != 0) {
                return WebAbiError::invalid_message;
            }
            completion.bytes.resize(total_bytes);
            inbound_ = InboundTransfer{
                std::move(completion),
                chunk_count,
                0,
                0,
                total_bytes,
            };
        } else if (!completion_metadata_equal(inbound_->completion, completion) ||
                   inbound_->chunk_count != chunk_count ||
                   inbound_->total_bytes != total_bytes) {
            return WebAbiError::invalid_message;
        }

        if (chunk_index != inbound_->next_chunk_index ||
            chunk_offset != inbound_->next_chunk_offset) {
            return WebAbiError::invalid_message;
        }
        std::copy(
            reader.remaining_bytes().begin(),
            reader.remaining_bytes().end(),
            inbound_->completion.bytes.begin() + static_cast<std::ptrdiff_t>(chunk_offset)
        );
        ++inbound_->next_chunk_index;
        inbound_->next_chunk_offset += chunk_length;
        if (inbound_->next_chunk_index == inbound_->chunk_count) {
            if (inbound_->next_chunk_offset != inbound_->total_bytes) {
                return WebAbiError::invalid_message;
            }
            auto completed = std::move(inbound_->completion);
            inbound_.reset();
            return finish_completion(std::move(completed));
        }
        if (inbound_->next_chunk_offset >= inbound_->total_bytes) {
            return WebAbiError::invalid_message;
        }
        return WebAbiError::none;
    } catch (const std::bad_alloc&) {
        inbound_.reset();
        return WebAbiError::internal;
    }
}

void WebPlatformBridge::publish_profile_ui(
    std::uint32_t session_generation,
    const WebProfileUiEvent& event
) noexcept {
    if (session_generation == 0) {
        return;
    }
    if (ui_session_generation_ != session_generation || ui_event_ != event) {
        ui_session_generation_ = session_generation;
        ui_event_ = event;
        ui_dirty_ = true;
    }
}

WebAbiError WebPlatformBridge::publish_quest_ui(
    std::uint32_t session_generation,
    const contracts::QuestUiProjectionSnapshot& event
) noexcept {
    if (session_generation == 0 || !valid_quest_ui_event(event)) {
        return WebAbiError::invalid_message;
    }
    const bool same_generation = quest_ui_session_generation_ == session_generation;
    if (same_generation && quest_ui_event_.sequence != 0) {
        if (event.sequence < quest_ui_event_.sequence ||
            (event.sequence == quest_ui_event_.sequence && event != quest_ui_event_)) {
            return WebAbiError::invalid_message;
        }
    }

    if (!same_generation) {
        quest_ui_close_dirty_ = false;
        quest_ui_close_session_generation_ = 0;
        quest_ui_close_ack_ = {};
        quest_ui_choice_identity_valid_ = false;
        quest_ui_choice_replaced_ = false;
        quest_ui_choice_closed_ = false;
        quest_ui_choice_session_generation_ = 0;
        quest_ui_choice_sequence_ = 0;
        quest_ui_choice_checksum_ = 0;
    }

    if (event.source == contracts::QuestUiProjectionSource::choice_available) {
        const bool same_choice = quest_ui_choice_identity_valid_ &&
                                 quest_ui_choice_session_generation_ == session_generation &&
                                 quest_ui_choice_sequence_ == event.sequence &&
                                 quest_ui_choice_checksum_ == event.checksum;
        if (!same_choice) {
            quest_ui_close_dirty_ = false;
            quest_ui_close_session_generation_ = 0;
            quest_ui_close_ack_ = {};
            quest_ui_choice_identity_valid_ = true;
            quest_ui_choice_replaced_ = false;
            quest_ui_choice_closed_ = false;
            quest_ui_choice_session_generation_ = session_generation;
            quest_ui_choice_sequence_ = event.sequence;
            quest_ui_choice_checksum_ = event.checksum;
        }
    } else if (quest_ui_choice_identity_valid_ &&
               quest_ui_choice_session_generation_ == session_generation &&
               event.sequence > quest_ui_choice_sequence_) {
        quest_ui_choice_replaced_ = true;
        quest_ui_close_dirty_ = false;
        quest_ui_close_session_generation_ = 0;
        quest_ui_close_ack_ = {};
    }

    if (!same_generation || quest_ui_event_ != event) {
        quest_ui_session_generation_ = session_generation;
        quest_ui_event_ = event;
        quest_ui_dirty_ = true;
    }
    return WebAbiError::none;
}

WebAbiError WebPlatformBridge::publish_quest_ui_close(
    std::uint32_t session_generation,
    const WebQuestUiCloseAck& acknowledgement
) noexcept {
    if (session_generation == 0 || acknowledgement.projection_sequence == 0 ||
        acknowledgement.projection_checksum == 0 ||
        acknowledgement.reason != TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED) {
        return WebAbiError::invalid_message;
    }
    if (!quest_ui_choice_identity_valid_ ||
        quest_ui_choice_session_generation_ != session_generation ||
        quest_ui_choice_sequence_ != acknowledgement.projection_sequence ||
        quest_ui_choice_checksum_ != acknowledgement.projection_checksum) {
        return WebAbiError::invalid_message;
    }
    if (quest_ui_choice_replaced_ || quest_ui_choice_closed_) {
        return WebAbiError::none;
    }
    if (quest_ui_close_session_generation_ != session_generation ||
        quest_ui_close_ack_ != acknowledgement) {
        quest_ui_close_session_generation_ = session_generation;
        quest_ui_close_ack_ = acknowledgement;
        quest_ui_close_dirty_ = true;
    }
    return WebAbiError::none;
}

std::uint32_t WebPlatformBridge::peek_ui_event_size() const noexcept {
    if (ui_dirty_) {
        return static_cast<std::uint32_t>(message_header_bytes + TGD_WEB_UI_EVENT_V1_BYTES);
    }
    if (quest_ui_dirty_) {
        return static_cast<std::uint32_t>(
            message_header_bytes + TGD_WEB_QUEST_UI_EVENT_V1_BYTES
        );
    }
    return quest_ui_close_dirty_
               ? static_cast<std::uint32_t>(
                     message_header_bytes + TGD_WEB_QUEST_UI_CLOSE_ACK_V1_BYTES
                 )
               : 0;
}

std::int32_t WebPlatformBridge::poll_ui_event(std::span<std::uint8_t> output) noexcept {
    const auto required = peek_ui_event_size();
    if (required == 0) {
        return 0;
    }
    if (output.size() < required) {
        return -static_cast<std::int32_t>(WebAbiError::buffer_too_small);
    }

    ByteWriter writer(output.first(required));
    if (ui_dirty_) {
        const auto flags = static_cast<std::uint32_t>(ui_event_.has_snapshot ? 1U : 0U) |
                           static_cast<std::uint32_t>(ui_event_.has_pending_save ? 2U : 0U);
        const auto wrote = write_message_header(
                               writer,
                               static_cast<std::uint16_t>(TGD_WEB_MESSAGE_UI_EVENT),
                               TGD_WEB_UI_EVENT_V1_BYTES,
                               ui_session_generation_,
                               ++wire_sequence_,
                               ui_event_.snapshot_id
                           ) &&
                           writer.write(ui_event_.state) && writer.write(ui_event_.error) &&
                           writer.write(flags) && writer.write(ui_event_.committed_save_count) &&
                           writer.write(ui_event_.logical_sequence) &&
                           writer.write_id(ui_event_.snapshot_id);
        if (!wrote || writer.size() != required) {
            return -static_cast<std::int32_t>(WebAbiError::internal);
        }
        ui_dirty_ = false;
        return static_cast<std::int32_t>(required);
    }

    if (quest_ui_dirty_) {
        const contracts::StableId128 projection_identity{
            quest_ui_event_.checksum,
            quest_ui_event_.sequence,
        };
        const auto wrote = write_message_header(
                               writer,
                               static_cast<std::uint16_t>(TGD_WEB_MESSAGE_QUEST_UI_EVENT),
                               TGD_WEB_QUEST_UI_EVENT_V1_BYTES,
                               quest_ui_session_generation_,
                               ++wire_sequence_,
                               projection_identity
                           ) &&
                           write_quest_ui_event(writer, quest_ui_event_);
        if (!wrote || writer.size() != required) {
            return -static_cast<std::int32_t>(WebAbiError::internal);
        }
        quest_ui_dirty_ = false;
        return static_cast<std::int32_t>(required);
    }

    const contracts::StableId128 closed_projection_identity{
        quest_ui_close_ack_.projection_checksum,
        quest_ui_close_ack_.projection_sequence,
    };
    const auto wrote = write_message_header(
                           writer,
                           static_cast<std::uint16_t>(TGD_WEB_MESSAGE_QUEST_UI_CLOSE_ACK),
                           TGD_WEB_QUEST_UI_CLOSE_ACK_V1_BYTES,
                           quest_ui_close_session_generation_,
                           ++wire_sequence_,
                           closed_projection_identity
                       ) &&
                       write_quest_ui_close_ack(writer, quest_ui_close_ack_);
    if (!wrote || writer.size() != required) {
        return -static_cast<std::int32_t>(WebAbiError::internal);
    }
    quest_ui_close_dirty_ = false;
    if (quest_ui_choice_identity_valid_ &&
        quest_ui_choice_session_generation_ == quest_ui_close_session_generation_ &&
        quest_ui_choice_sequence_ == quest_ui_close_ack_.projection_sequence &&
        quest_ui_choice_checksum_ == quest_ui_close_ack_.projection_checksum) {
        quest_ui_choice_closed_ = true;
    }
    return static_cast<std::int32_t>(required);
}

void WebPlatformBridge::reset() noexcept {
    outbound_ = {};
    outstanding_.reset();
    inbound_.reset();
    completions_.clear();
    ui_dirty_ = false;
    ui_session_generation_ = 0;
    ui_event_ = {};
    quest_ui_dirty_ = false;
    quest_ui_session_generation_ = 0;
    quest_ui_event_ = {};
    quest_ui_close_dirty_ = false;
    quest_ui_close_session_generation_ = 0;
    quest_ui_close_ack_ = {};
    quest_ui_choice_identity_valid_ = false;
    quest_ui_choice_replaced_ = false;
    quest_ui_choice_closed_ = false;
    quest_ui_choice_session_generation_ = 0;
    quest_ui_choice_sequence_ = 0;
    quest_ui_choice_checksum_ = 0;
}

WebAbiError WebPlatformBridge::decode_boot_config(
    std::span<const std::uint8_t> message,
    WebBootConfig& output
) noexcept {
    MessageHeader header;
    std::span<const std::uint8_t> payload;
    const auto parsed = parse_message(
        message,
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_BOOT_CONFIG),
        header,
        payload
    );
    if (parsed != WebAbiError::none) {
        return parsed;
    }
    if (payload.size() != TGD_WEB_BOOT_CONFIG_V1_BYTES) {
        return WebAbiError::invalid_message;
    }
    ByteReader reader(payload);
    if (!reader.read_id(output.profile_id) || !reader.read_id(output.package_set_id) ||
        !reader.read_id(output.request_id_seed) || !reader.read(output.session_generation) ||
        reader.remaining() != 0 || output.profile_id.empty() || output.package_set_id.empty() ||
        output.request_id_seed.high == 0 || output.session_generation == 0 ||
        header.session_generation != output.session_generation ||
        header.request_id != output.request_id_seed) {
        return WebAbiError::invalid_message;
    }
    return WebAbiError::none;
}

WebAbiError WebPlatformBridge::decode_ui_command(
    std::span<const std::uint8_t> message,
    WebUiCommand& output
) noexcept {
    MessageHeader header;
    std::span<const std::uint8_t> payload;
    const auto parsed = parse_message(
        message,
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_UI_COMMAND),
        header,
        payload
    );
    if (parsed != WebAbiError::none) {
        return parsed;
    }
    if (payload.size() != TGD_WEB_UI_COMMAND_V1_BYTES) {
        return WebAbiError::invalid_message;
    }
    ByteReader reader(payload);
    std::uint16_t command = 0;
    std::uint16_t checkpoint = 0;
    if (!reader.read(command) || !reader.read(checkpoint) ||
        !reader.read_id(output.command_id) || reader.remaining() != 0 ||
        command < static_cast<std::uint16_t>(WebUiCommandType::save_guest_checkpoint) ||
        command > static_cast<std::uint16_t>(WebUiCommandType::retry_pending_save) ||
        checkpoint < static_cast<std::uint16_t>(contracts::CheckpointKind::automatic) ||
        checkpoint > static_cast<std::uint16_t>(contracts::CheckpointKind::imported) ||
        output.command_id.empty() || header.session_generation == 0 ||
        header.request_id != output.command_id) {
        return WebAbiError::invalid_message;
    }
    output.type = static_cast<WebUiCommandType>(command);
    output.checkpoint_kind = static_cast<contracts::CheckpointKind>(checkpoint);
    output.session_generation = header.session_generation;
    return WebAbiError::none;
}

WebAbiError WebPlatformBridge::decode_quest_ui_selection_intent(
    std::span<const std::uint8_t> message,
    WebQuestUiSelectionIntent& output
) noexcept {
    MessageHeader header;
    std::span<const std::uint8_t> payload;
    const auto parsed = parse_message(
        message,
        static_cast<std::uint16_t>(TGD_WEB_MESSAGE_QUEST_UI_SELECTION_INTENT),
        header,
        payload
    );
    if (parsed != WebAbiError::none) {
        return parsed;
    }
    if (payload.size() != TGD_WEB_QUEST_UI_SELECTION_INTENT_V1_BYTES) {
        return WebAbiError::invalid_message;
    }

    contracts::QuestUiSelectionIntent intent;
    ByteReader reader(payload);
    if (!reader.read(intent.projection_sequence) ||
        !reader.read(intent.projection_checksum) || !reader.read(intent.objective) ||
        !reader.read(intent.interaction) || !reader.read(intent.selection) ||
        reader.remaining() != 0 || header.session_generation == 0 ||
        intent.projection_sequence == 0 || intent.projection_checksum == 0 ||
        intent.objective == 0 || intent.interaction == 0 || intent.selection == 0 ||
        header.request_id != contracts::StableId128{
                                 intent.projection_checksum,
                                 intent.projection_sequence,
                             }) {
        return WebAbiError::invalid_message;
    }
    output.session_generation = header.session_generation;
    output.intent = intent;
    return WebAbiError::none;
}

bool WebPlatformBridge::request_slot_available() const noexcept {
    return outbound_.kind == OutboundKind::none && !outstanding_.has_value();
}

runtime::StorageSubmitError WebPlatformBridge::begin_request(
    OutboundKind kind,
    const runtime::StorageRequestContext& context,
    const runtime::StorageKey& key
) noexcept {
    if (!request_slot_available()) {
        return runtime::StorageSubmitError::backpressure;
    }
    outbound_ = {};
    outbound_.kind = kind;
    outbound_.context = context;
    outbound_.key = key;
    return runtime::StorageSubmitError::none;
}

std::uint32_t WebPlatformBridge::current_request_chunk_size() const noexcept {
    if (outbound_.bytes.empty()) {
        return 0;
    }
    return static_cast<std::uint32_t>(std::min<std::size_t>(
        max_request_chunk_bytes,
        outbound_.bytes.size() - outbound_.chunk_offset
    ));
}

runtime::StorageOperation WebPlatformBridge::outbound_operation() const noexcept {
    switch (outbound_.kind) {
        case OutboundKind::read:
            return runtime::StorageOperation::read;
        case OutboundKind::write_atomic:
            return runtime::StorageOperation::write_atomic;
        case OutboundKind::list:
            return runtime::StorageOperation::list;
        case OutboundKind::delete_record:
            return runtime::StorageOperation::delete_record;
        case OutboundKind::estimate_quota:
            return runtime::StorageOperation::estimate_quota;
        case OutboundKind::request_persistence:
            return runtime::StorageOperation::request_persistence;
        case OutboundKind::none:
            break;
    }
    return runtime::StorageOperation::read;
}

WebAbiError WebPlatformBridge::finish_completion(
    runtime::StorageCompletion&& completion
) noexcept {
    try {
        const bool current_generation =
            outstanding_.has_value() &&
            completion.context.session_generation ==
                outstanding_->context.session_generation;
        completions_.push_back(std::move(completion));
        if (current_generation) {
            outstanding_.reset();
        }
        return WebAbiError::none;
    } catch (const std::bad_alloc&) {
        return WebAbiError::internal;
    }
}

}  // namespace tgd::platform::web
