#include <tgd/contracts/profile_progress.hpp>

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

constexpr std::array<std::uint8_t, 8> progress_magic{'T', 'G', 'D', 'P', 'R', 'O', 'F', 0};
constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;

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

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto bits = static_cast<std::uintmax_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash ^= static_cast<std::uint8_t>((bits >> (index * 8U)) & 0xffU);
        hash *= fnv_prime;
    }
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

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

  private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_{};
};

[[nodiscard]] bool operation_precedes(
    const PersistentOperationV1& left,
    const PersistentOperationV1& right
) noexcept {
    if (left.created_logical_time != right.created_logical_time) {
        return left.created_logical_time < right.created_logical_time;
    }
    if (left.operation_id.high != right.operation_id.high) {
        return left.operation_id.high < right.operation_id.high;
    }
    return left.operation_id.low < right.operation_id.low;
}

}  // namespace

StableId128 reward_operation_id(
    StableId128 profile_id,
    StableContentKey reward_dedup_key
) noexcept {
    auto high = fnv_offset;
    hash_integer(high, static_cast<std::uint64_t>(0x7265776172642d31ULL));
    hash_integer(high, profile_id.high);
    hash_integer(high, profile_id.low);
    hash_integer(high, reward_dedup_key);

    auto low = fnv_offset;
    hash_integer(low, static_cast<std::uint64_t>(0x636c61696d2d7631ULL));
    hash_integer(low, reward_dedup_key);
    hash_integer(low, profile_id.low);
    hash_integer(low, profile_id.high);
    if (high == 0 && low == 0) {
        low = 1;
    }
    return {high, low};
}

PersistentOperationV1 make_reward_operation(
    StableId128 profile_id,
    std::uint64_t base_revision,
    std::uint64_t created_logical_time,
    StableContentKey source_id,
    StableContentKey reward_id,
    StableContentKey reward_dedup_key
) noexcept {
    return {
        reward_operation_id(profile_id, reward_dedup_key),
        profile_id,
        base_revision,
        created_logical_time,
        PersistentOperationDomain::quest_reward,
        1,
        source_id,
        reward_id,
        reward_dedup_key,
    };
}

bool contains_reward_dedup(
    const ProfileProgressV1& progress,
    StableContentKey reward_dedup_key
) noexcept {
    return reward_dedup_key != 0 &&
           std::any_of(
               progress.operations.begin(),
               progress.operations.end(),
               [reward_dedup_key](const PersistentOperationV1& operation) {
                   return operation.domain == PersistentOperationDomain::quest_reward &&
                          operation.reward_dedup_key == reward_dedup_key;
               }
           );
}

ProfileProgressError validate_profile_progress(
    const ProfileProgressV1& progress
) noexcept {
    if (progress.profile_id.empty()) {
        return ProfileProgressError::invalid_header;
    }
    if (progress.operations.size() > max_profile_progress_operations) {
        return ProfileProgressError::too_many_operations;
    }
    for (std::size_t index = 0; index < progress.operations.size(); ++index) {
        const auto& operation = progress.operations[index];
        if (operation.operation_id.empty() || operation.profile_id != progress.profile_id ||
            operation.base_revision >= operation.created_logical_time ||
            operation.created_logical_time > progress.revision ||
            operation.domain != PersistentOperationDomain::quest_reward ||
            operation.payload_version != 1 || operation.source_id == 0 ||
            operation.reward_id == 0 || operation.reward_dedup_key == 0 ||
            (index != 0 && !operation_precedes(progress.operations[index - 1], operation))) {
            return ProfileProgressError::invalid_operation;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (progress.operations[prior].reward_dedup_key == operation.reward_dedup_key) {
                return ProfileProgressError::duplicate_reward_dedup;
            }
            if (progress.operations[prior].operation_id == operation.operation_id) {
                return ProfileProgressError::duplicate_operation;
            }
        }
    }
    return ProfileProgressError::none;
}

EncodeProfileProgressResult encode_profile_progress(
    const ProfileProgressV1& progress
) noexcept {
    const auto validation = validate_profile_progress(progress);
    if (validation != ProfileProgressError::none) {
        return {validation, {}};
    }
    try {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(
            profile_progress_v1_header_bytes +
            progress.operations.size() * persistent_operation_v1_bytes
        );
        bytes.insert(bytes.end(), progress_magic.begin(), progress_magic.end());
        append_integer(bytes, profile_progress_major);
        append_integer(bytes, profile_progress_minor);
        append_integer(bytes, static_cast<std::uint32_t>(0));
        append_id(bytes, progress.profile_id);
        append_integer(bytes, progress.revision);
        append_integer(bytes, static_cast<std::uint32_t>(progress.operations.size()));
        append_integer(bytes, static_cast<std::uint32_t>(0));
        for (const auto& operation : progress.operations) {
            append_id(bytes, operation.operation_id);
            append_id(bytes, operation.profile_id);
            append_integer(bytes, operation.base_revision);
            append_integer(bytes, operation.created_logical_time);
            append_integer(bytes, static_cast<std::uint16_t>(operation.domain));
            append_integer(bytes, operation.payload_version);
            append_integer(bytes, static_cast<std::uint32_t>(0));
            append_integer(bytes, operation.source_id);
            append_integer(bytes, operation.reward_id);
            append_integer(bytes, operation.reward_dedup_key);
        }
        return {ProfileProgressError::none, std::move(bytes)};
    } catch (const std::bad_alloc&) {
        return {ProfileProgressError::allocation_failed, {}};
    }
}

DecodeProfileProgressResult decode_profile_progress(
    std::span<const std::uint8_t> bytes
) noexcept {
    if (bytes.size() < progress_magic.size()) {
        return {ProfileProgressError::truncated, {}};
    }
    if (!std::equal(progress_magic.begin(), progress_magic.end(), bytes.begin())) {
        return {ProfileProgressError::invalid_magic, {}};
    }
    if (bytes.size() < profile_progress_v1_header_bytes) {
        return {ProfileProgressError::truncated, {}};
    }

    ByteReader reader(bytes.subspan(progress_magic.size()));
    ProfileProgressV1 progress;
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint32_t reserved_header = 0;
    std::uint32_t operation_count = 0;
    std::uint32_t reserved_count = 0;
    if (!reader.read(major) || !reader.read(minor) || !reader.read(reserved_header) ||
        !reader.read_id(progress.profile_id) || !reader.read(progress.revision) ||
        !reader.read(operation_count) || !reader.read(reserved_count)) {
        return {ProfileProgressError::truncated, {}};
    }
    if (major != profile_progress_major || minor > profile_progress_minor) {
        return {ProfileProgressError::unsupported_version, {}};
    }
    if (reserved_header != 0 || reserved_count != 0 || progress.profile_id.empty()) {
        return {ProfileProgressError::invalid_header, {}};
    }
    if (operation_count > max_profile_progress_operations) {
        return {ProfileProgressError::too_many_operations, {}};
    }
    const auto expected_size = profile_progress_v1_header_bytes +
                               static_cast<std::size_t>(operation_count) *
                                   persistent_operation_v1_bytes;
    if (bytes.size() < expected_size) {
        return {ProfileProgressError::truncated, {}};
    }
    if (bytes.size() > expected_size) {
        return {ProfileProgressError::trailing_bytes, {}};
    }

    try {
        progress.operations.reserve(operation_count);
        for (std::uint32_t index = 0; index < operation_count; ++index) {
            PersistentOperationV1 operation;
            std::uint16_t domain = 0;
            std::uint32_t reserved_operation = 0;
            if (!reader.read_id(operation.operation_id) ||
                !reader.read_id(operation.profile_id) ||
                !reader.read(operation.base_revision) ||
                !reader.read(operation.created_logical_time) || !reader.read(domain) ||
                !reader.read(operation.payload_version) || !reader.read(reserved_operation) ||
                !reader.read(operation.source_id) || !reader.read(operation.reward_id) ||
                !reader.read(operation.reward_dedup_key)) {
                return {ProfileProgressError::truncated, {}};
            }
            if (reserved_operation != 0) {
                return {ProfileProgressError::invalid_operation, {}};
            }
            operation.domain = static_cast<PersistentOperationDomain>(domain);
            progress.operations.push_back(operation);
        }
    } catch (const std::bad_alloc&) {
        return {ProfileProgressError::allocation_failed, {}};
    }
    const auto validation = validate_profile_progress(progress);
    return validation == ProfileProgressError::none
               ? DecodeProfileProgressResult{ProfileProgressError::none, std::move(progress)}
               : DecodeProfileProgressResult{validation, {}};
}

}  // namespace tgd::contracts
