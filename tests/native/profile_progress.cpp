#include <tgd/contracts/profile_progress.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

using tgd::contracts::PersistentOperationV1;
using tgd::contracts::ProfileProgressError;
using tgd::contracts::ProfileProgressV1;
using tgd::contracts::StableId128;

constexpr StableId128 profile_id{0x1000000000000001ULL, 0x2000000000000001ULL};
constexpr std::uint64_t source_one = 0x3000000000000001ULL;
constexpr std::uint64_t source_two = 0x3000000000000002ULL;
constexpr std::uint64_t reward_one = 0x4000000000000001ULL;
constexpr std::uint64_t reward_two = 0x4000000000000002ULL;
constexpr std::uint64_t dedup_one = 0x5000000000000001ULL;
constexpr std::uint64_t dedup_two = 0x5000000000000002ULL;

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "profile progress failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] PersistentOperationV1 operation(
    std::uint64_t base_revision,
    std::uint64_t created_logical_time,
    std::uint64_t source,
    std::uint64_t reward,
    std::uint64_t dedup
) {
    return tgd::contracts::make_reward_operation(
        profile_id,
        base_revision,
        created_logical_time,
        source,
        reward,
        dedup
    );
}

}  // namespace

int main() {
    bool ok = true;

    ProfileProgressV1 empty{profile_id, 0, {}};
    const auto encoded_empty = tgd::contracts::encode_profile_progress(empty);
    ok &= expect(
        encoded_empty.error == ProfileProgressError::none &&
            encoded_empty.bytes.size() == tgd::contracts::profile_progress_v1_header_bytes,
        "an empty new Profile has one canonical v1 header"
    );
    const auto decoded_empty = tgd::contracts::decode_profile_progress(encoded_empty.bytes);
    ok &= expect(
        decoded_empty.error == ProfileProgressError::none && decoded_empty.progress == empty,
        "an empty Profile round-trips without inventing operations"
    );

    ProfileProgressV1 progress{profile_id, 3, {}};
    progress.operations.push_back(operation(0, 1, source_one, reward_one, dedup_one));
    progress.operations.push_back(operation(2, 3, source_two, reward_two, dedup_two));
    const auto encoded = tgd::contracts::encode_profile_progress(progress);
    const auto decoded = tgd::contracts::decode_profile_progress(encoded.bytes);
    const auto reencoded = tgd::contracts::encode_profile_progress(decoded.progress);
    ok &= expect(
        encoded.error == ProfileProgressError::none &&
            decoded.error == ProfileProgressError::none && decoded.progress == progress &&
            reencoded.error == ProfileProgressError::none && reencoded.bytes == encoded.bytes,
        "reward Operations round-trip to identical canonical bytes"
    );
    ok &= expect(
        tgd::contracts::contains_reward_dedup(progress, dedup_one) &&
            tgd::contracts::contains_reward_dedup(progress, dedup_two) &&
            !tgd::contracts::contains_reward_dedup(progress, 0x5000000000000003ULL),
        "reward claims are queried by their stable local deduplication key"
    );
    ok &= expect(
        progress.operations.front().operation_id ==
            tgd::contracts::reward_operation_id(profile_id, dedup_one) &&
            tgd::contracts::reward_operation_id(
                {0x0102030405060708ULL, 0x1112131415161718ULL},
                0x7300000000000001ULL
            ) == StableId128{0xd41837c7680ae62eULL, 0x2bd4eb04b4860ecfULL},
        "the same Profile and reward key always derive the same Operation ID"
    );

    auto duplicate_operation = progress;
    duplicate_operation.revision = 4;
    auto repeated_id = operation(3, 4, source_two, reward_two, 0x5000000000000003ULL);
    repeated_id.operation_id = duplicate_operation.operations.front().operation_id;
    duplicate_operation.operations.push_back(repeated_id);
    ok &= expect(
        tgd::contracts::validate_profile_progress(duplicate_operation) ==
            ProfileProgressError::duplicate_operation,
        "duplicate Operation IDs fail before persistence"
    );

    auto duplicate_reward = progress;
    duplicate_reward.revision = 4;
    duplicate_reward.operations.push_back(operation(3, 4, source_two, reward_two, dedup_one));
    ok &= expect(
        tgd::contracts::validate_profile_progress(duplicate_reward) ==
            ProfileProgressError::duplicate_reward_dedup,
        "duplicate reward deduplication keys cannot grant twice"
    );

    auto wrong_profile = progress;
    wrong_profile.operations.back().profile_id.low ^= 1U;
    ok &= expect(
        tgd::contracts::validate_profile_progress(wrong_profile) ==
            ProfileProgressError::invalid_operation,
        "an Operation cannot cross Profile ownership"
    );

    auto noncanonical_id = progress;
    noncanonical_id.operations.back().operation_id.low ^= 1U;
    ok &= expect(
        tgd::contracts::validate_profile_progress(noncanonical_id) ==
            ProfileProgressError::invalid_operation,
        "a reward Operation ID must be derived from its Profile and deduplication key"
    );

    auto future_operation = progress;
    future_operation.revision = 2;
    ok &= expect(
        tgd::contracts::validate_profile_progress(future_operation) ==
            ProfileProgressError::invalid_operation,
        "an Operation cannot claim a logical time after its Snapshot revision"
    );

    auto out_of_order = progress;
    std::reverse(out_of_order.operations.begin(), out_of_order.operations.end());
    ok &= expect(
        tgd::contracts::validate_profile_progress(out_of_order) ==
            ProfileProgressError::invalid_operation,
        "Operation order is canonical and independent of container insertion accidents"
    );

    auto unsupported = encoded.bytes;
    unsupported[8] = 2;
    ok &= expect(
        tgd::contracts::decode_profile_progress(unsupported).error ==
            ProfileProgressError::unsupported_version,
        "unknown Profile progress major versions fail closed"
    );
    auto trailing = encoded.bytes;
    trailing.push_back(0);
    ok &= expect(
        tgd::contracts::decode_profile_progress(trailing).error ==
            ProfileProgressError::trailing_bytes,
        "Profile progress rejects trailing bytes"
    );
    auto truncated = encoded.bytes;
    truncated.pop_back();
    ok &= expect(
        tgd::contracts::decode_profile_progress(truncated).error ==
            ProfileProgressError::truncated,
        "Profile progress rejects truncated Operation payloads"
    );

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
