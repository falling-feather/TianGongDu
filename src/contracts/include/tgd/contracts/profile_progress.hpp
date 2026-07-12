#pragma once

#include <tgd/contracts/save_envelope.hpp>
#include <tgd/contracts/session_types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tgd::contracts {

inline constexpr std::uint16_t profile_progress_major = 1;
inline constexpr std::uint16_t profile_progress_minor = 0;
inline constexpr std::size_t profile_progress_v1_header_bytes = 48;
inline constexpr std::size_t persistent_operation_v1_bytes = 80;
inline constexpr std::size_t max_profile_progress_operations = 512;

enum class PersistentOperationDomain : std::uint16_t {
    quest_reward = 1,
};

struct PersistentOperationV1 final {
    StableId128 operation_id{};
    StableId128 profile_id{};
    std::uint64_t base_revision{};
    std::uint64_t created_logical_time{};
    PersistentOperationDomain domain{PersistentOperationDomain::quest_reward};
    std::uint16_t payload_version{1};
    StableContentKey source_id{};
    StableContentKey reward_id{};
    StableContentKey reward_dedup_key{};

    [[nodiscard]] friend constexpr bool operator==(
        const PersistentOperationV1&,
        const PersistentOperationV1&
    ) noexcept = default;
};

struct ProfileProgressV1 final {
    StableId128 profile_id{};
    std::uint64_t revision{};
    std::vector<PersistentOperationV1> operations{};

    [[nodiscard]] friend bool operator==(
        const ProfileProgressV1&,
        const ProfileProgressV1&
    ) noexcept = default;
};

enum class ProfileProgressError : std::uint8_t {
    none,
    allocation_failed,
    invalid_magic,
    unsupported_version,
    truncated,
    trailing_bytes,
    invalid_header,
    invalid_operation,
    duplicate_operation,
    duplicate_reward_dedup,
    too_many_operations,
};

struct EncodeProfileProgressResult final {
    ProfileProgressError error{ProfileProgressError::none};
    std::vector<std::uint8_t> bytes{};
};

struct DecodeProfileProgressResult final {
    ProfileProgressError error{ProfileProgressError::none};
    ProfileProgressV1 progress{};
};

[[nodiscard]] StableId128 reward_operation_id(
    StableId128 profile_id,
    StableContentKey reward_dedup_key
) noexcept;
[[nodiscard]] PersistentOperationV1 make_reward_operation(
    StableId128 profile_id,
    std::uint64_t base_revision,
    std::uint64_t created_logical_time,
    StableContentKey source_id,
    StableContentKey reward_id,
    StableContentKey reward_dedup_key
) noexcept;
[[nodiscard]] bool contains_reward_dedup(
    const ProfileProgressV1& progress,
    StableContentKey reward_dedup_key
) noexcept;
[[nodiscard]] ProfileProgressError validate_profile_progress(
    const ProfileProgressV1& progress
) noexcept;
[[nodiscard]] EncodeProfileProgressResult encode_profile_progress(
    const ProfileProgressV1& progress
) noexcept;
[[nodiscard]] DecodeProfileProgressResult decode_profile_progress(
    std::span<const std::uint8_t> bytes
) noexcept;

}  // namespace tgd::contracts
