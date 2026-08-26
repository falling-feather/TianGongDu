#include <tgd/contracts/save_envelope.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace tgd::contracts {
namespace {

constexpr std::array<std::uint8_t, 8> save_magic{'T', 'G', 'D', 'S', 'A', 'V', 'E', 0};
constexpr std::size_t envelope_hash_offset = 144;

template <typename Integer>
void append_integer(std::vector<std::uint8_t>& bytes, Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto bits = static_cast<std::uintmax_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(static_cast<std::uint8_t>((bits >> (index * 8U)) & 0xffU));
    }
}

void append_id(std::vector<std::uint8_t>& bytes, StableId128 id) {
    append_integer(bytes, id.high);
    append_integer(bytes, id.low);
}

class ByteReader final {
  public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    template <typename Integer>
    [[nodiscard]] bool read(Integer& value) noexcept {
        if (remaining() < sizeof(Integer)) {
            return false;
        }
        using Unsigned = std::make_unsigned_t<Integer>;
        Unsigned bits = 0;
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            bits |= static_cast<Unsigned>(bytes_[offset_ + index]) << (index * 8U);
        }
        if constexpr (std::is_signed_v<Integer>) {
            value = std::bit_cast<Integer>(bits);
        } else {
            value = bits;
        }
        offset_ += sizeof(Integer);
        return true;
    }

    [[nodiscard]] bool read_id(StableId128& id) noexcept {
        return read(id.high) && read(id.low);
    }

    template <std::size_t Size>
    [[nodiscard]] bool read_array(std::array<std::uint8_t, Size>& output) noexcept {
        if (remaining() < Size) {
            return false;
        }
        std::copy_n(bytes_.data() + offset_, Size, output.data());
        offset_ += Size;
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

  private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_{};
};

[[nodiscard]] bool valid_checkpoint(CheckpointKind kind) noexcept {
    return kind >= CheckpointKind::automatic && kind <= CheckpointKind::imported;
}

[[nodiscard]] bool digest_empty(const Sha256Digest& digest) noexcept {
    return std::all_of(digest.begin(), digest.end(), [](std::uint8_t byte) { return byte == 0; });
}

}  // namespace

SaveEnvelopeError validate_save_envelope_descriptor(const SaveEnvelopeV1& envelope) noexcept {
    if (envelope.profile_id.empty() || envelope.snapshot_id.empty() ||
        envelope.package_set_id.empty() || envelope.snapshot_id == envelope.parent_snapshot_id ||
        envelope.save_version == 0 || envelope.content_api_version == 0 ||
        envelope.executable_major == 0 || envelope.created_logical_sequence == 0 ||
        envelope.payload_encoding != SavePayloadEncoding::raw ||
        !valid_checkpoint(envelope.checkpoint_kind) || envelope.payload.empty()) {
        return SaveEnvelopeError::invalid_header;
    }
    if (envelope.payload.size() > max_save_payload_bytes) {
        return SaveEnvelopeError::payload_too_large;
    }
    return SaveEnvelopeError::none;
}

EncodeSaveEnvelopeResult encode_save_envelope(const SaveEnvelopeV1& envelope) noexcept {
    const auto validation = validate_save_envelope_descriptor(envelope);
    if (validation != SaveEnvelopeError::none) {
        return {validation, {}};
    }

    try {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(save_envelope_v1_header_bytes + envelope.payload.size());
        bytes.insert(bytes.end(), save_magic.begin(), save_magic.end());
        append_integer(bytes, save_envelope_major);
        append_integer(bytes, save_envelope_minor);
        append_integer(bytes, static_cast<std::uint16_t>(envelope.payload_encoding));
        append_integer(bytes, static_cast<std::uint16_t>(envelope.checkpoint_kind));
        append_id(bytes, envelope.profile_id);
        append_id(bytes, envelope.snapshot_id);
        append_id(bytes, envelope.parent_snapshot_id);
        append_id(bytes, envelope.package_set_id);
        append_integer(bytes, envelope.save_version);
        append_integer(bytes, envelope.content_api_version);
        append_integer(bytes, envelope.executable_major);
        append_integer(bytes, envelope.executable_minor);
        append_integer(bytes, envelope.executable_patch);
        append_integer(bytes, static_cast<std::uint16_t>(0));
        append_integer(bytes, envelope.created_logical_sequence);
        append_integer(bytes, static_cast<std::uint32_t>(envelope.payload.size()));
        append_integer(bytes, static_cast<std::uint32_t>(envelope.payload.size()));
        const auto payload_hash = sha256(envelope.payload);
        bytes.insert(bytes.end(), payload_hash.begin(), payload_hash.end());
        const auto envelope_hash = sha256(std::span{bytes}.first(envelope_hash_offset));
        bytes.insert(bytes.end(), envelope_hash.begin(), envelope_hash.end());
        bytes.insert(bytes.end(), envelope.payload.begin(), envelope.payload.end());
        return {SaveEnvelopeError::none, std::move(bytes)};
    } catch (const std::bad_alloc&) {
        return {SaveEnvelopeError::allocation_failed, {}};
    }
}

DecodeSaveEnvelopeResult decode_save_envelope(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < save_magic.size()) {
        return {SaveEnvelopeError::truncated, {}};
    }
    if (!std::equal(save_magic.begin(), save_magic.end(), bytes.begin())) {
        return {SaveEnvelopeError::invalid_magic, {}};
    }
    if (bytes.size() < save_envelope_v1_header_bytes) {
        return {SaveEnvelopeError::truncated, {}};
    }

    ByteReader reader(bytes.subspan(save_magic.size()));
    std::uint16_t format_major = 0;
    std::uint16_t format_minor = 0;
    std::uint16_t encoding = 0;
    std::uint16_t checkpoint = 0;
    std::uint16_t reserved = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint32_t payload_size = 0;
    SaveEnvelopeV1 envelope;
    if (!reader.read(format_major) || !reader.read(format_minor) || !reader.read(encoding) ||
        !reader.read(checkpoint) || !reader.read_id(envelope.profile_id) ||
        !reader.read_id(envelope.snapshot_id) || !reader.read_id(envelope.parent_snapshot_id) ||
        !reader.read_id(envelope.package_set_id) || !reader.read(envelope.save_version) ||
        !reader.read(envelope.content_api_version) || !reader.read(envelope.executable_major) ||
        !reader.read(envelope.executable_minor) || !reader.read(envelope.executable_patch) ||
        !reader.read(reserved) || !reader.read(envelope.created_logical_sequence) ||
        !reader.read(uncompressed_size) || !reader.read(payload_size) ||
        !reader.read_array(envelope.payload_hash) || !reader.read_array(envelope.envelope_hash)) {
        return {SaveEnvelopeError::truncated, {}};
    }
    if (format_major != save_envelope_major || format_minor > save_envelope_minor) {
        return {SaveEnvelopeError::unsupported_version, {}};
    }
    envelope.payload_encoding = static_cast<SavePayloadEncoding>(encoding);
    envelope.checkpoint_kind = static_cast<CheckpointKind>(checkpoint);
    if (reserved != 0 || payload_size != uncompressed_size ||
        payload_size > max_save_payload_bytes) {
        return {payload_size > max_save_payload_bytes ? SaveEnvelopeError::payload_too_large
                                                       : SaveEnvelopeError::invalid_header,
                {}};
    }
    if (reader.remaining() < payload_size) {
        return {SaveEnvelopeError::truncated, {}};
    }
    if (reader.remaining() > payload_size) {
        return {SaveEnvelopeError::trailing_bytes, {}};
    }

    try {
        envelope.payload.assign(bytes.end() - static_cast<std::ptrdiff_t>(payload_size), bytes.end());
    } catch (const std::bad_alloc&) {
        return {SaveEnvelopeError::allocation_failed, {}};
    }
    const auto descriptor_validation = validate_save_envelope_descriptor(envelope);
    if (descriptor_validation != SaveEnvelopeError::none) {
        return {descriptor_validation, {}};
    }
    if (sha256(envelope.payload) != envelope.payload_hash) {
        return {SaveEnvelopeError::payload_hash_mismatch, {}};
    }
    if (sha256(bytes.first(envelope_hash_offset)) != envelope.envelope_hash) {
        return {SaveEnvelopeError::envelope_hash_mismatch, {}};
    }
    if (digest_empty(envelope.payload_hash) || digest_empty(envelope.envelope_hash)) {
        return {SaveEnvelopeError::invalid_header, {}};
    }
    return {SaveEnvelopeError::none, std::move(envelope)};
}

}  // namespace tgd::contracts
