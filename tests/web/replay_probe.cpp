#include <tgd/contracts/build_identity.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/runtime/command_replay.hpp>
#include <tgd/test/f1_golden_replay.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

[[nodiscard]] std::unique_ptr<tgd::runtime::StaticCollisionWorld> empty_world() {
    auto world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    if (world->configure({}) != tgd::runtime::CollisionWorldError::none) {
        return {};
    }
    return world;
}

[[nodiscard]] std::string hex64(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

}  // namespace

int main() {
    using tgd::runtime::CommandReplayError;
    using tgd::runtime::RunCommandReplayResult;

    constexpr std::array<std::uint32_t, 3> frame_rates{30, 60, 144};
    const auto identity = tgd::contracts::current_build_identity();
    const auto replay = tgd::test::make_f1_golden_replay();
    const auto validation = tgd::runtime::validate_command_replay(replay);
    const auto encoded = tgd::runtime::encode_command_replay(replay);
    const auto decoded = tgd::runtime::decode_command_replay(encoded.bytes);
    const auto reencoded = tgd::runtime::encode_command_replay(decoded.replay);

    std::array<RunCommandReplayResult, frame_rates.size()> runs{};
    if (decoded.error == CommandReplayError::none) {
        for (std::size_t index = 0; index < frame_rates.size(); ++index) {
            runs[index] = tgd::runtime::run_command_replay(
                decoded.replay,
                empty_world(),
                frame_rates[index]
            );
        }
    }

    bool passed = validation == CommandReplayError::none &&
                  encoded.error == CommandReplayError::none &&
                  decoded.error == CommandReplayError::none &&
                  reencoded.error == CommandReplayError::none &&
                  reencoded.bytes == encoded.bytes;
    for (const auto& run : runs) {
        passed = passed && run.error == CommandReplayError::none &&
                 run.snapshot.tick == replay.final_tick &&
                 run.snapshot.checksum == replay.expected_checksum &&
                 run.snapshot.player_pose == runs.front().snapshot.player_pose;
    }

    std::cout << "[tgd.replay] {"
              << "\"schemaVersion\":\"1.0.0\","
              << "\"status\":\"" << (passed ? "passed" : "failed") << "\","
              << "\"build\":{"
              << "\"version\":\"" << identity.semantic_version << "\","
              << "\"commit\":\"" << identity.git_commit << "\","
              << "\"channel\":\"" << identity.channel << "\"},"
              << "\"formatMajor\":" << replay.format_major << ','
              << "\"formatMinor\":" << replay.format_minor << ','
              << "\"contentFingerprint\":\"" << hex64(replay.content_fingerprint) << "\","
              << "\"finalTick\":" << replay.final_tick << ','
              << "\"expectedChecksum\":\"" << hex64(replay.expected_checksum) << "\","
              << "\"canonicalBytes\":" << encoded.bytes.size() << ','
              << "\"validationError\":" << static_cast<unsigned>(validation) << ','
              << "\"encodeError\":" << static_cast<unsigned>(encoded.error) << ','
              << "\"decodeError\":" << static_cast<unsigned>(decoded.error) << ','
              << "\"reencodeError\":" << static_cast<unsigned>(reencoded.error) << ','
              << "\"cadences\":[";
    for (std::size_t index = 0; index < runs.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        const auto& run = runs[index];
        std::cout << '{'
                  << "\"fps\":" << frame_rates[index] << ','
                  << "\"frames\":" << run.render_frames << ','
                  << "\"error\":" << static_cast<unsigned>(run.error) << ','
                  << "\"tick\":" << run.snapshot.tick << ','
                  << "\"checksum\":\"" << hex64(run.snapshot.checksum) << "\","
                  << "\"pose\":{"
                  << "\"x\":" << run.snapshot.player_pose.x << ','
                  << "\"y\":" << run.snapshot.player_pose.y << ','
                  << "\"height\":" << run.snapshot.player_pose.height << ','
                  << "\"floorLayer\":" << run.snapshot.player_pose.floor_layer
                  << "}}";
    }
    std::cout << "]}" << std::endl;
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
