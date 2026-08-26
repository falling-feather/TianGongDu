#pragma once

#include <tgd/contracts/sha256.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tgd::contracts {

inline constexpr std::uint16_t save_envelope_major = 1;
inline constexpr std::uint16_t save_envelope_minor = 0;
inline constexpr std::size_t save_envelope_v1_header_bytes = 176;
inline constexpr std::size_t max_save_payload_bytes = 16U * 1024U * 1024U;

struct StableId128 final {
    std::uint64_t high{};
    std::uint64_t low{};

    [[nodiscard]] constexpr bool empty() const noexcept {
        return high == 0 && low == 0;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const StableId128&,
        const StableId128&
    ) noexcept = default;
};

enum class SavePayloadEncoding : std::uint16_t {
    raw = 0,
};

enum class CheckpointKind : std::uint16_t {
    automatic = 1,
    safe_point = 2,
    chapter_milestone = 3,
    region_milestone = 4,
    user_export = 5,
    imported = 6,
};

enum class SaveEnvelopeError : std::uint8_t {
    none,
    allocation_failed,
    invalid_magic,
    unsupported_version,
    truncated,
    trailing_bytes,
    invalid_header,
    payload_too_large,
    payload_hash_mismatch,
    envelope_hash_mismatch,
};

struct SaveEnvelopeV1 final {
    StableId128 profile_id{};
    StableId128 snapshot_id{};
    StableId128 parent_snapshot_id{};
    StableId128 package_set_id{};
    std::uint32_t save_version{1};
    std::uint32_t content_api_version{1};
    std::uint16_t executable_major{2};
    std::uint16_t executable_minor{2};
    std::uint16_t executable_patch{1};
    std::uint64_t created_logical_sequence{};
    CheckpointKind checkpoint_kind{CheckpointKind::safe_point};
    SavePayloadEncoding payload_encoding{SavePayloadEncoding::raw};
    Sha256Digest payload_hash{};
    Sha256Digest envelope_hash{};
    std::vector<std::uint8_t> payload{};
};

struct EncodeSaveEnvelopeResult final {
    SaveEnvelopeError error{SaveEnvelopeError::none};
    std::vector<std::uint8_t> bytes{};
};

struct DecodeSaveEnvelopeResult final {
    SaveEnvelopeError error{SaveEnvelopeError::none};
    SaveEnvelopeV1 envelope{};
};

[[nodiscard]] SaveEnvelopeError validate_save_envelope_descriptor(
    const SaveEnvelopeV1& envelope
) noexcept;
// payload_hash and envelope_hash are derived fields. Encoding always recomputes them from the
// canonical descriptor and payload; caller-provided digest values are deliberately ignored.
[[nodiscard]] EncodeSaveEnvelopeResult encode_save_envelope(
    const SaveEnvelopeV1& envelope
) noexcept;
[[nodiscard]] DecodeSaveEnvelopeResult decode_save_envelope(
    std::span<const std::uint8_t> bytes
) noexcept;

}  // namespace tgd::contracts
