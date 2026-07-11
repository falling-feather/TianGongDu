#include <tgd/runtime/collision_world.hpp>
#include <tgd/runtime/command_replay.hpp>
#include <tgd/test/f1_golden_replay.hpp>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "command replay failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] std::unique_ptr<tgd::runtime::StaticCollisionWorld> empty_world() {
    auto world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    if (world->configure({}) != tgd::runtime::CollisionWorldError::none) {
        std::abort();
    }
    return world;
}

}  // namespace

int main() {
    using tgd::runtime::CommandReplayError;

    bool ok = true;
    const auto replay = tgd::test::make_f1_golden_replay();
    ok &= expect(
        tgd::runtime::validate_command_replay(replay) == CommandReplayError::none,
        "golden replay validates"
    );
    const auto encoded = tgd::runtime::encode_command_replay(replay);
    ok &= expect(encoded.error == CommandReplayError::none, "golden replay encodes");
    if (encoded.error != CommandReplayError::none) {
        return EXIT_FAILURE;
    }
    const auto decoded = tgd::runtime::decode_command_replay(encoded.bytes);
    ok &= expect(decoded.error == CommandReplayError::none, "golden replay decodes");
    if (decoded.error != CommandReplayError::none) {
        return EXIT_FAILURE;
    }
    const auto reencoded = tgd::runtime::encode_command_replay(decoded.replay);
    ok &= expect(
        reencoded.error == CommandReplayError::none && reencoded.bytes == encoded.bytes,
        "canonical replay bytes round-trip"
    );

    auto invalid_magic = encoded.bytes;
    invalid_magic[0] ^= 0xffU;
    ok &= expect(
        tgd::runtime::decode_command_replay(invalid_magic).error ==
            CommandReplayError::invalid_magic,
        "invalid replay magic is rejected"
    );
    ok &= expect(
        tgd::runtime::decode_command_replay(
            std::span{encoded.bytes}.first(encoded.bytes.size() - 1)
        ).error == CommandReplayError::truncated,
        "truncated replay is rejected"
    );
    auto unsupported_version = encoded.bytes;
    unsupported_version[8] = 2;
    ok &= expect(
        tgd::runtime::decode_command_replay(unsupported_version).error ==
            CommandReplayError::unsupported_version,
        "unknown replay major is rejected"
    );
    auto unsupported_minor = encoded.bytes;
    unsupported_minor[10] = 1;
    ok &= expect(
        tgd::runtime::decode_command_replay(unsupported_minor).error ==
            CommandReplayError::unsupported_version,
        "unknown replay minor is rejected"
    );
    auto trailing_bytes = encoded.bytes;
    trailing_bytes.push_back(0);
    ok &= expect(
        tgd::runtime::decode_command_replay(trailing_bytes).error ==
            CommandReplayError::trailing_bytes,
        "trailing replay bytes are rejected"
    );
    auto duplicate_key = decoded.replay;
    duplicate_key.commands[1] = duplicate_key.commands[0];
    ok &= expect(
        tgd::runtime::validate_command_replay(duplicate_key) ==
            CommandReplayError::invalid_replay,
        "duplicate replay ordering keys are rejected"
    );
    auto invalid_direction = decoded.replay;
    invalid_direction.commands[0].ground_direction.x = tgd::contracts::ground_axis_one + 1;
    ok &= expect(
        tgd::runtime::validate_command_replay(invalid_direction) ==
            CommandReplayError::invalid_replay,
        "out-of-range replay directions are rejected"
    );

    const auto replay_30 = tgd::runtime::run_command_replay(decoded.replay, empty_world(), 30);
    const auto replay_60 = tgd::runtime::run_command_replay(decoded.replay, empty_world(), 60);
    const auto replay_144 = tgd::runtime::run_command_replay(decoded.replay, empty_world(), 144);
    ok &= expect(
        replay_30.error == CommandReplayError::none &&
            replay_60.error == CommandReplayError::none &&
            replay_144.error == CommandReplayError::none,
        "30/60/144 command replays execute"
    );
    ok &= expect(
        replay_30.snapshot.tick == replay.final_tick &&
            replay_60.snapshot.tick == replay.final_tick &&
            replay_144.snapshot.tick == replay.final_tick,
        "30/60/144 command replays reach exactly 10,000 ticks"
    );
    ok &= expect(
        replay_30.snapshot.checksum == replay.expected_checksum &&
            replay_30.snapshot.checksum == replay_60.snapshot.checksum &&
            replay_60.snapshot.checksum == replay_144.snapshot.checksum,
        "30/60/144 command replays share the golden checksum"
    );
    ok &= expect(
        replay_30.snapshot.player_pose == replay_60.snapshot.player_pose &&
            replay_60.snapshot.player_pose == replay_144.snapshot.player_pose,
        "30/60/144 command replays share one quantized pose"
    );
    ok &= expect(
        replay_60.snapshot.player_pose.x != 0 &&
            replay_60.snapshot.player_pose.y != 0 &&
            replay_60.snapshot.player_pose.height > 0 &&
            replay_60.snapshot.player_pose.floor_layer != 0,
        "golden final pose exercises x/y/height/floorLayer"
    );
    auto wrong_checksum = decoded.replay;
    wrong_checksum.expected_checksum ^= 1U;
    ok &= expect(
        tgd::runtime::run_command_replay(wrong_checksum, empty_world(), 60).error ==
            CommandReplayError::checksum_mismatch,
        "checksum mismatches fail closed"
    );
    std::cout << "golden replay checksum: 0x" << std::hex << std::setw(16) << std::setfill('0')
              << replay_60.snapshot.checksum << std::dec << '\n';
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
