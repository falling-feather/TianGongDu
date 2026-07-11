#include <tgd/runtime/game_session.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tgd::runtime {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnv_prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & static_cast<Unsigned>(0xffU)));
        bits >>= 8U;
    }
}

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

[[nodiscard]] std::int32_t integrate_axis(
    std::int32_t direction,
    std::int32_t speed,
    std::int64_t& remainder
) noexcept {
    if (direction == 0) {
        remainder = 0;
        return 0;
    }
    constexpr auto denominator = static_cast<std::int64_t>(contracts::ground_axis_one) * 60;
    const auto numerator = static_cast<std::int64_t>(direction) * speed + remainder;
    const auto delta = numerator / denominator;
    remainder = numerator % denominator;
    return static_cast<std::int32_t>(delta);
}

[[nodiscard]] std::int32_t saturating_add(std::int32_t value, std::int32_t delta) noexcept {
    const auto sum = static_cast<std::int64_t>(value) + delta;
    return static_cast<std::int32_t>(std::clamp(
        sum,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
    ));
}

[[nodiscard]] bool same_command_key(
    const contracts::SessionCommand& left,
    const contracts::SessionCommand& right
) noexcept {
    return left.header.tick == right.header.tick && left.header.actor == right.header.actor &&
           left.header.sequence == right.header.sequence && left.header.type == right.header.type;
}

}  // namespace

GameSessionError GameSession::initialize(
    const GameSessionConfig& config,
    std::unique_ptr<ICollisionWorld> collision_world
) noexcept {
    if (lifecycle_ != GameSessionLifecycle::uninitialized) {
        return GameSessionError::invalid_lifecycle;
    }
    if (!valid_config(config)) {
        return GameSessionError::invalid_config;
    }
    if (!collision_world) {
        return GameSessionError::missing_collision_world;
    }

    config_ = config;
    collision_world_ = std::move(collision_world);
    ++generation_;
    lifecycle_ = GameSessionLifecycle::ready_at_safe_point;
    current_snapshot_.tick = 0;
    current_snapshot_.player_actor = config_.player_actor;
    current_snapshot_.player_pose = config_.initial_pose;
    if (current_snapshot_.player_pose.height <= config_.ground_height) {
        current_snapshot_.player_pose.height = config_.ground_height;
        current_snapshot_.grounded = true;
    } else {
        current_snapshot_.grounded = false;
    }
    current_snapshot_.vertical_velocity_mm_per_second = 0;
    update_checksum();
    previous_snapshot_ = current_snapshot_;
    return GameSessionError::none;
}

GameSessionError GameSession::start() noexcept {
    if (lifecycle_ != GameSessionLifecycle::ready_at_safe_point) {
        return GameSessionError::invalid_lifecycle;
    }
    lifecycle_ = GameSessionLifecycle::running;
    return GameSessionError::none;
}

GameSessionError GameSession::pause() noexcept {
    if (lifecycle_ != GameSessionLifecycle::running) {
        return GameSessionError::invalid_lifecycle;
    }
    lifecycle_ = GameSessionLifecycle::paused;
    return GameSessionError::none;
}

GameSessionError GameSession::resume() noexcept {
    if (lifecycle_ != GameSessionLifecycle::paused) {
        return GameSessionError::invalid_lifecycle;
    }
    lifecycle_ = GameSessionLifecycle::running;
    return GameSessionError::none;
}

GameSessionError GameSession::destroy() noexcept {
    if (lifecycle_ == GameSessionLifecycle::uninitialized ||
        lifecycle_ == GameSessionLifecycle::destroyed) {
        return GameSessionError::invalid_lifecycle;
    }
    lifecycle_ = GameSessionLifecycle::destroyed;
    ++generation_;
    command_count_ = 0;
    collision_world_.reset();
    return GameSessionError::none;
}

GameSessionError GameSession::submit(
    std::span<const contracts::SessionCommand> commands
) noexcept {
    if (lifecycle_ != GameSessionLifecycle::running && lifecycle_ != GameSessionLifecycle::paused) {
        return GameSessionError::invalid_lifecycle;
    }
    if (commands.size() > command_capacity - command_count_) {
        return GameSessionError::command_queue_full;
    }
    for (std::size_t index = 0; index < commands.size(); ++index) {
        const auto& command = commands[index];
        if (command.header.tick <= current_snapshot_.tick) {
            return GameSessionError::command_targets_past_tick;
        }
        if (!valid_command(command)) {
            return GameSessionError::invalid_command;
        }
        if (duplicate_command_key(command, commands, index)) {
            return GameSessionError::duplicate_command_key;
        }
    }
    for (const auto& command : commands) {
        commands_[command_count_] = command;
        ++command_count_;
    }
    return GameSessionError::none;
}

AdvanceTicksResult GameSession::advance(std::uint32_t tick_budget) noexcept {
    if (lifecycle_ != GameSessionLifecycle::running) {
        return {GameSessionError::invalid_lifecycle, 0};
    }
    if (tick_budget == 0) {
        return {};
    }

    sort_commands();
    std::size_t consumed = 0;
    std::uint32_t executed = 0;
    while (executed < tick_budget) {
        const auto next_tick = current_snapshot_.tick + 1;
        contracts::GroundVectorQ15 ground_direction{};
        bool jump_pressed = false;
        while (consumed < command_count_ && commands_[consumed].header.tick == next_tick) {
            const auto& command = commands_[consumed];
            if (command.header.type == contracts::SessionCommandType::move_intent) {
                ground_direction = command.ground_direction;
            } else if (command.header.type == contracts::SessionCommandType::jump_pressed) {
                jump_pressed = true;
            }
            ++consumed;
        }
        simulate_tick(ground_direction, jump_pressed);
        ++executed;
    }
    compact_commands(consumed);
    return {GameSessionError::none, executed};
}

GameSessionLifecycle GameSession::lifecycle() const noexcept {
    return lifecycle_;
}

std::uint32_t GameSession::generation() const noexcept {
    return generation_;
}

std::size_t GameSession::queued_command_count() const noexcept {
    return command_count_;
}

const PresentationSnapshot& GameSession::previous_snapshot() const noexcept {
    return previous_snapshot_;
}

const PresentationSnapshot& GameSession::current_snapshot() const noexcept {
    return current_snapshot_;
}

bool GameSession::valid_config(const GameSessionConfig& config) const noexcept {
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

bool GameSession::valid_command(const contracts::SessionCommand& command) const noexcept {
    if (command.header.actor != config_.player_actor) {
        return false;
    }
    if (command.header.type == contracts::SessionCommandType::move_intent) {
        return valid_direction(command.ground_direction);
    }
    return command.header.type == contracts::SessionCommandType::jump_pressed &&
           command.ground_direction == contracts::GroundVectorQ15{};
}

bool GameSession::duplicate_command_key(
    const contracts::SessionCommand& command,
    std::span<const contracts::SessionCommand> pending,
    std::size_t pending_index
) const noexcept {
    for (std::size_t index = 0; index < command_count_; ++index) {
        if (same_command_key(command, commands_[index])) {
            return true;
        }
    }
    for (std::size_t index = 0; index < pending_index; ++index) {
        if (same_command_key(command, pending[index])) {
            return true;
        }
    }
    return false;
}

void GameSession::sort_commands() noexcept {
    std::sort(
        commands_.begin(),
        commands_.begin() + static_cast<std::ptrdiff_t>(command_count_),
        [](const contracts::SessionCommand& left, const contracts::SessionCommand& right) {
            return std::tuple{
                       left.header.tick,
                       left.header.actor,
                       left.header.sequence,
                       static_cast<std::uint8_t>(left.header.type),
                   } <
                   std::tuple{
                       right.header.tick,
                       right.header.actor,
                       right.header.sequence,
                       static_cast<std::uint8_t>(right.header.type),
                   };
        }
    );
}

void GameSession::compact_commands(std::size_t consumed) noexcept {
    if (consumed == 0) {
        return;
    }
    for (std::size_t index = consumed; index < command_count_; ++index) {
        commands_[index - consumed] = commands_[index];
    }
    command_count_ -= consumed;
}

void GameSession::simulate_tick(
    contracts::GroundVectorQ15 ground_direction,
    bool jump_pressed
) noexcept {
    previous_snapshot_ = current_snapshot_;
    ++current_snapshot_.tick;

    if (jump_pressed && current_snapshot_.grounded) {
        current_snapshot_.grounded = false;
        current_snapshot_.vertical_velocity_mm_per_second = config_.jump_speed_mm_per_second;
        vertical_remainder_ = 0;
        gravity_remainder_ = 0;
    }
    if (!current_snapshot_.grounded) {
        vertical_remainder_ += current_snapshot_.vertical_velocity_mm_per_second;
        current_snapshot_.player_pose.height = saturating_add(
            current_snapshot_.player_pose.height,
            static_cast<std::int32_t>(vertical_remainder_ / 60)
        );
        vertical_remainder_ %= 60;

        gravity_remainder_ += config_.gravity_mm_per_second_squared;
        current_snapshot_.vertical_velocity_mm_per_second -=
            static_cast<std::int32_t>(gravity_remainder_ / 60);
        gravity_remainder_ %= 60;
        if (current_snapshot_.player_pose.height <= config_.ground_height) {
            current_snapshot_.player_pose.height = config_.ground_height;
            current_snapshot_.vertical_velocity_mm_per_second = 0;
            current_snapshot_.grounded = true;
            vertical_remainder_ = 0;
            gravity_remainder_ = 0;
        }
    }

    const auto delta_x = integrate_axis(
        ground_direction.x,
        config_.move_speed_mm_per_second,
        movement_remainder_x_
    );
    const auto delta_y = integrate_axis(
        ground_direction.y,
        config_.move_speed_mm_per_second,
        movement_remainder_y_
    );
    const auto resolved = collision_world_->resolve_ground_move(
        current_snapshot_.player_pose,
        delta_x,
        delta_y,
        config_.collision_radius,
        config_.collision_height
    );
    current_snapshot_.player_pose = resolved.pose;
    update_checksum();
}

void GameSession::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, current_snapshot_.tick);
    hash_integer(hash, current_snapshot_.player_actor);
    hash_integer(hash, current_snapshot_.player_pose.x);
    hash_integer(hash, current_snapshot_.player_pose.y);
    hash_integer(hash, current_snapshot_.player_pose.height);
    hash_integer(hash, current_snapshot_.player_pose.floor_layer);
    hash_integer(hash, current_snapshot_.vertical_velocity_mm_per_second);
    hash_byte(hash, current_snapshot_.grounded ? 1U : 0U);
    current_snapshot_.checksum = hash;
}

}  // namespace tgd::runtime
