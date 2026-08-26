#include <tgd/runtime/command_replay.hpp>

#include <tgd/runtime/fixed_step_driver.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tgd::runtime {
namespace {

inline constexpr std::array<std::uint8_t, 8> replay_magic{
    'T',
    'G',
    'D',
    'R',
    'P',
    'L',
    'Y',
    0,
};
inline constexpr std::size_t max_replay_commands = 100'000;
inline constexpr contracts::TickIndex max_replay_ticks = 10'000'000;
inline constexpr std::size_t command_wire_size =
    sizeof(contracts::TickIndex) + sizeof(contracts::StableActorKey) +
    sizeof(contracts::CommandSequence) + sizeof(std::uint8_t) +
    sizeof(contracts::GroundVectorQ15::x) + sizeof(contracts::GroundVectorQ15::y);
static_assert(command_wire_size == 33, "CommandReplay v1 command bytes changed; bump the format.");

template <typename Integer>
void append_integer(std::vector<std::uint8_t>& bytes, Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto bits = static_cast<std::uintmax_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(static_cast<std::uint8_t>((bits >> (index * 8U)) & 0xffU));
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

    [[nodiscard]] bool read_magic() noexcept {
        if (remaining() < replay_magic.size()) {
            return false;
        }
        const auto candidate = bytes_.subspan(offset_, replay_magic.size());
        offset_ += replay_magic.size();
        return std::equal(candidate.begin(), candidate.end(), replay_magic.begin());
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

  private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_{};
};

[[nodiscard]] bool valid_direction(contracts::GroundVectorQ15 direction) noexcept {
    if (direction.x < -contracts::ground_axis_one || direction.x > contracts::ground_axis_one ||
        direction.y < -contracts::ground_axis_one || direction.y > contracts::ground_axis_one) {
        return false;
    }
    const auto x = static_cast<std::int64_t>(direction.x);
    const auto y = static_cast<std::int64_t>(direction.y);
    const auto limit = static_cast<std::int64_t>(contracts::ground_axis_one) *
                       contracts::ground_axis_one;
    return x * x + y * y <= limit + contracts::ground_axis_one;
}

[[nodiscard]] auto command_key(const contracts::SessionCommand& command) noexcept {
    return std::tuple{
        command.header.tick,
        command.header.actor,
        command.header.sequence,
        static_cast<std::uint8_t>(command.header.type),
    };
}

[[nodiscard]] bool valid_replay_config(const GameSessionConfig& config) noexcept {
    constexpr std::int32_t max_motion_value = 100'000;
    return config.player_actor != 0 && config.move_speed_mm_per_second > 0 &&
           config.move_speed_mm_per_second <= max_motion_value &&
           config.jump_speed_mm_per_second > 0 &&
           config.jump_speed_mm_per_second <= max_motion_value &&
           config.gravity_mm_per_second_squared > 0 &&
           config.gravity_mm_per_second_squared <= max_motion_value &&
           config.collision_radius > 0 && config.collision_radius <= max_motion_value &&
           config.collision_height > 0 && config.collision_height <= max_motion_value &&
           config.initial_pose.height >= config.ground_height;
}

void append_config(std::vector<std::uint8_t>& bytes, const GameSessionConfig& config) {
    append_integer(bytes, config.player_actor);
    append_integer(bytes, config.initial_pose.x);
    append_integer(bytes, config.initial_pose.y);
    append_integer(bytes, config.initial_pose.height);
    append_integer(bytes, config.initial_pose.floor_layer);
    append_integer(bytes, config.ground_height);
    append_integer(bytes, config.move_speed_mm_per_second);
    append_integer(bytes, config.jump_speed_mm_per_second);
    append_integer(bytes, config.gravity_mm_per_second_squared);
    append_integer(bytes, config.collision_radius);
    append_integer(bytes, config.collision_height);
}

[[nodiscard]] bool read_config(ByteReader& reader, GameSessionConfig& config) noexcept {
    return reader.read(config.player_actor) && reader.read(config.initial_pose.x) &&
           reader.read(config.initial_pose.y) && reader.read(config.initial_pose.height) &&
           reader.read(config.initial_pose.floor_layer) && reader.read(config.ground_height) &&
           reader.read(config.move_speed_mm_per_second) &&
           reader.read(config.jump_speed_mm_per_second) &&
           reader.read(config.gravity_mm_per_second_squared) &&
           reader.read(config.collision_radius) && reader.read(config.collision_height);
}

}  // namespace

CommandReplayError validate_command_replay(const CommandReplay& replay) noexcept {
    if (replay.format_major != command_replay_major || replay.format_minor > command_replay_minor) {
        return CommandReplayError::unsupported_version;
    }
    if (replay.content_fingerprint == 0 || replay.expected_checksum == 0 ||
        replay.final_tick == 0 ||
        replay.final_tick > max_replay_ticks || replay.commands.size() > max_replay_commands ||
        !valid_replay_config(replay.session_config)) {
        return CommandReplayError::invalid_replay;
    }

    std::size_t commands_at_tick = 0;
    contracts::TickIndex previous_tick = 0;
    for (std::size_t index = 0; index < replay.commands.size(); ++index) {
        const auto& command = replay.commands[index];
        if (command.header.tick == 0 || command.header.tick > replay.final_tick ||
            command.header.actor != replay.session_config.player_actor) {
            return CommandReplayError::invalid_replay;
        }
        if (index > 0 && !(command_key(replay.commands[index - 1]) < command_key(command))) {
            return CommandReplayError::invalid_replay;
        }
        commands_at_tick = command.header.tick == previous_tick ? commands_at_tick + 1 : 1;
        previous_tick = command.header.tick;
        if (commands_at_tick > GameSession::command_capacity) {
            return CommandReplayError::invalid_replay;
        }
        if (command.header.type == contracts::SessionCommandType::move_intent) {
            if (!valid_direction(command.ground_direction)) {
                return CommandReplayError::invalid_replay;
            }
        } else if (command.header.type != contracts::SessionCommandType::jump_pressed ||
                   command.ground_direction != contracts::GroundVectorQ15{}) {
            return CommandReplayError::invalid_replay;
        }
    }
    return CommandReplayError::none;
}

EncodeCommandReplayResult encode_command_replay(const CommandReplay& replay) noexcept {
    const auto validation = validate_command_replay(replay);
    if (validation != CommandReplayError::none) {
        return {validation, {}};
    }

    try {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(96 + replay.commands.size() * command_wire_size);
        bytes.insert(bytes.end(), replay_magic.begin(), replay_magic.end());
        append_integer(bytes, replay.format_major);
        append_integer(bytes, replay.format_minor);
        append_integer(bytes, replay.content_fingerprint);
        append_integer(bytes, replay.final_tick);
        append_integer(bytes, replay.expected_checksum);
        append_config(bytes, replay.session_config);
        append_integer(bytes, static_cast<std::uint32_t>(replay.commands.size()));
        for (const auto& command : replay.commands) {
            append_integer(bytes, command.header.tick);
            append_integer(bytes, command.header.actor);
            append_integer(bytes, command.header.sequence);
            append_integer(bytes, static_cast<std::uint8_t>(command.header.type));
            append_integer(bytes, command.ground_direction.x);
            append_integer(bytes, command.ground_direction.y);
        }
        return {CommandReplayError::none, std::move(bytes)};
    } catch (const std::bad_alloc&) {
        return {CommandReplayError::allocation_failed, {}};
    }
}

DecodeCommandReplayResult decode_command_replay(std::span<const std::uint8_t> bytes) noexcept {
    ByteReader reader(bytes);
    if (!reader.read_magic()) {
        return {bytes.size() < replay_magic.size() ? CommandReplayError::truncated
                                                   : CommandReplayError::invalid_magic,
                {}};
    }

    CommandReplay replay;
    std::uint32_t command_count = 0;
    if (!reader.read(replay.format_major) || !reader.read(replay.format_minor) ||
        !reader.read(replay.content_fingerprint) || !reader.read(replay.final_tick) ||
        !reader.read(replay.expected_checksum) || !read_config(reader, replay.session_config) ||
        !reader.read(command_count)) {
        return {CommandReplayError::truncated, {}};
    }
    if (replay.format_major != command_replay_major || replay.format_minor > command_replay_minor) {
        return {CommandReplayError::unsupported_version, {}};
    }
    if (command_count > max_replay_commands) {
        return {CommandReplayError::invalid_replay, {}};
    }
    if (command_count > reader.remaining() / command_wire_size) {
        return {CommandReplayError::truncated, {}};
    }

    try {
        replay.commands.resize(command_count);
    } catch (const std::bad_alloc&) {
        return {CommandReplayError::allocation_failed, {}};
    }
    for (auto& command : replay.commands) {
        std::uint8_t type = 0;
        if (!reader.read(command.header.tick) || !reader.read(command.header.actor) ||
            !reader.read(command.header.sequence) || !reader.read(type) ||
            !reader.read(command.ground_direction.x) ||
            !reader.read(command.ground_direction.y)) {
            return {CommandReplayError::truncated, {}};
        }
        command.header.type = static_cast<contracts::SessionCommandType>(type);
    }
    if (reader.remaining() != 0) {
        return {CommandReplayError::trailing_bytes, {}};
    }
    const auto validation = validate_command_replay(replay);
    return {validation, validation == CommandReplayError::none ? std::move(replay) : CommandReplay{}};
}

RunCommandReplayResult run_command_replay(
    const CommandReplay& replay,
    std::unique_ptr<ICollisionWorld> collision_world,
    std::uint32_t render_fps
) noexcept {
    const auto validation = validate_command_replay(replay);
    if (validation != CommandReplayError::none || render_fps == 0 || render_fps > 1'000 ||
        !collision_world) {
        return {validation == CommandReplayError::none ? CommandReplayError::invalid_replay
                                                       : validation,
                {},
                0};
    }

    GameSession session;
    if (session.initialize(replay.session_config, std::move(collision_world)) !=
            GameSessionError::none ||
        session.start() != GameSessionError::none) {
        return {CommandReplayError::session_error, {}, 0};
    }

    FixedStepDriver driver;
    std::size_t command_index = 0;
    std::uint64_t frame_index = 0;
    std::uint64_t elapsed_at_previous_frame = 0;
    while (session.current_snapshot().tick < replay.final_tick) {
        const auto schedule_through = std::min<contracts::TickIndex>(
            replay.final_tick,
            session.current_snapshot().tick + FixedStepDriver::max_ticks_per_frame
        );
        const auto batch_start = command_index;
        while (command_index < replay.commands.size() &&
               replay.commands[command_index].header.tick <= schedule_through) {
            ++command_index;
        }
        if (command_index > batch_start &&
            session.submit(std::span{replay.commands}.subspan(
                batch_start,
                command_index - batch_start
            )) != GameSessionError::none) {
            return {CommandReplayError::session_error, session.current_snapshot(), frame_index};
        }

        ++frame_index;
        const auto elapsed_at_frame = frame_index * 1'000'000'000ULL / render_fps;
        const auto frame_delta = elapsed_at_frame - elapsed_at_previous_frame;
        elapsed_at_previous_frame = elapsed_at_frame;
        const auto remaining = replay.final_tick - session.current_snapshot().tick;
        const auto advanced = driver.advance_frame(
            session,
            frame_delta,
            static_cast<std::uint32_t>(std::min<contracts::TickIndex>(
                remaining,
                FixedStepDriver::max_ticks_per_frame
            ))
        );
        if (advanced.error != GameSessionError::none || advanced.simulation_overrun) {
            return {CommandReplayError::session_error, session.current_snapshot(), frame_index};
        }
    }

    const auto& snapshot = session.current_snapshot();
    if (snapshot.checksum != replay.expected_checksum) {
        return {CommandReplayError::checksum_mismatch, snapshot, frame_index};
    }
    return {CommandReplayError::none, snapshot, frame_index};
}

}  // namespace tgd::runtime
