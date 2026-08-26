#pragma once

#include <tgd/contracts/session_types.hpp>
#include <tgd/runtime/collision_world.hpp>
#include <tgd/runtime/game_session.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace tgd::runtime {

inline constexpr std::uint16_t command_replay_major = 1;
inline constexpr std::uint16_t command_replay_minor = 0;

enum class CommandReplayError : std::uint8_t {
    none,
    allocation_failed,
    invalid_magic,
    unsupported_version,
    truncated,
    trailing_bytes,
    invalid_replay,
    session_error,
    checksum_mismatch,
};

struct CommandReplay final {
    std::uint16_t format_major{command_replay_major};
    std::uint16_t format_minor{command_replay_minor};
    std::uint64_t content_fingerprint{};
    GameSessionConfig session_config{};
    contracts::TickIndex final_tick{};
    std::uint64_t expected_checksum{};
    std::vector<contracts::SessionCommand> commands{};
};

struct EncodeCommandReplayResult final {
    CommandReplayError error{CommandReplayError::none};
    std::vector<std::uint8_t> bytes{};
};

struct DecodeCommandReplayResult final {
    CommandReplayError error{CommandReplayError::none};
    CommandReplay replay{};
};

struct RunCommandReplayResult final {
    CommandReplayError error{CommandReplayError::none};
    PresentationSnapshot snapshot{};
    std::uint64_t render_frames{};
};

[[nodiscard]] CommandReplayError validate_command_replay(
    const CommandReplay& replay
) noexcept;
[[nodiscard]] EncodeCommandReplayResult encode_command_replay(const CommandReplay& replay) noexcept;
[[nodiscard]] DecodeCommandReplayResult decode_command_replay(
    std::span<const std::uint8_t> bytes
) noexcept;
[[nodiscard]] RunCommandReplayResult run_command_replay(
    const CommandReplay& replay,
    std::unique_ptr<ICollisionWorld> collision_world,
    std::uint32_t render_fps
) noexcept;

}  // namespace tgd::runtime
