#include <tgd/gameplay/vertical_slice_session.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace tgd::gameplay {
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

[[nodiscard]] bool contains_content_id(
    std::span<const contracts::ContentId> ids,
    contracts::StableContentKey key
) noexcept {
    return std::any_of(ids.begin(), ids.end(), [key](const contracts::ContentId& id) {
        return id.key == key;
    });
}

[[nodiscard]] bool valid_basis(const contracts::CameraBasisQ15& basis) noexcept {
    if (basis.revision == 0) {
        return false;
    }
    const auto right_x = static_cast<std::int64_t>(basis.screen_right_world.x);
    const auto right_y = static_cast<std::int64_t>(basis.screen_right_world.y);
    const auto forward_x = static_cast<std::int64_t>(basis.screen_forward_world.x);
    const auto forward_y = static_cast<std::int64_t>(basis.screen_forward_world.y);
    const auto target = static_cast<std::int64_t>(contracts::ground_axis_one) *
                        contracts::ground_axis_one;
    const auto tolerance = target / 100;
    const auto right_length = right_x * right_x + right_y * right_y;
    const auto forward_length = forward_x * forward_x + forward_y * forward_y;
    const auto dot = right_x * forward_x + right_y * forward_y;
    const auto determinant = right_x * forward_y - right_y * forward_x;
    return right_length >= target - tolerance && right_length <= target + tolerance &&
           forward_length >= target - tolerance && forward_length <= target + tolerance &&
           dot >= -tolerance && dot <= tolerance && determinant >= target - tolerance &&
           determinant <= target + tolerance;
}

class DiscardingQuestEventSink final : public IQuestEventSink {
  public:
    void publish(std::span<const contracts::QuestEvent>) noexcept override {}
};

}  // namespace

VerticalSliceError VerticalSliceSession::initialize(
    const contracts::VerticalSliceDefinition& definition,
    std::unique_ptr<runtime::ICollisionWorld> collision_world
) noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::uninitialized) {
        return VerticalSliceError::invalid_lifecycle;
    }
    if (!valid_definition(definition)) {
        return VerticalSliceError::invalid_definition;
    }
    if (!collision_world) {
        return VerticalSliceError::missing_collision_world;
    }
    if (quest_.initialize(definition, definition.player.actor) != QuestError::none) {
        return VerticalSliceError::invalid_definition;
    }

    runtime::GameSessionConfig movement_config{};
    movement_config.player_actor = definition.player.actor;
    movement_config.initial_pose = definition.player.initial_pose;
    movement_config.ground_height = definition.player.initial_pose.height;
    movement_config.move_speed_mm_per_second = definition.player.move_speed_mm_per_second;
    movement_config.jump_speed_mm_per_second = definition.player.jump_speed_mm_per_second;
    movement_config.gravity_mm_per_second_squared =
        definition.player.gravity_mm_per_second_squared;
    movement_config.collision_radius = definition.player.collision_radius_mm;
    movement_config.collision_height = definition.player.collision_height_mm;
    last_movement_error_ = movement_.initialize(movement_config, std::move(collision_world));
    if (last_movement_error_ != runtime::GameSessionError::none) {
        static_cast<void>(quest_.destroy());
        return VerticalSliceError::movement_session_error;
    }

    definition_ = &definition;
    ++generation_;
    lifecycle_ = VerticalSliceLifecycle::ready_at_safe_point;
    refresh_snapshot();
    previous_snapshot_ = current_snapshot_;
    return VerticalSliceError::none;
}

VerticalSliceError VerticalSliceSession::start() noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::ready_at_safe_point) {
        return VerticalSliceError::invalid_lifecycle;
    }
    last_movement_error_ = movement_.start();
    if (last_movement_error_ != runtime::GameSessionError::none) {
        return VerticalSliceError::movement_session_error;
    }
    if (quest_.start() != QuestError::none) {
        return VerticalSliceError::quest_runtime_error;
    }
    lifecycle_ = VerticalSliceLifecycle::running;
    return VerticalSliceError::none;
}

VerticalSliceError VerticalSliceSession::pause() noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::running) {
        return VerticalSliceError::invalid_lifecycle;
    }
    last_movement_error_ = movement_.pause();
    if (last_movement_error_ != runtime::GameSessionError::none) {
        return VerticalSliceError::movement_session_error;
    }
    if (quest_.pause() != QuestError::none) {
        return VerticalSliceError::quest_runtime_error;
    }
    lifecycle_ = VerticalSliceLifecycle::paused;
    return VerticalSliceError::none;
}

VerticalSliceError VerticalSliceSession::resume() noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::paused) {
        return VerticalSliceError::invalid_lifecycle;
    }
    last_movement_error_ = movement_.resume();
    if (last_movement_error_ != runtime::GameSessionError::none) {
        return VerticalSliceError::movement_session_error;
    }
    if (quest_.resume() != QuestError::none) {
        return VerticalSliceError::quest_runtime_error;
    }
    lifecycle_ = VerticalSliceLifecycle::running;
    return VerticalSliceError::none;
}

VerticalSliceError VerticalSliceSession::destroy() noexcept {
    if (lifecycle_ == VerticalSliceLifecycle::uninitialized ||
        lifecycle_ == VerticalSliceLifecycle::destroyed) {
        return VerticalSliceError::invalid_lifecycle;
    }
    last_movement_error_ = movement_.destroy();
    if (last_movement_error_ != runtime::GameSessionError::none) {
        return VerticalSliceError::movement_session_error;
    }
    if (quest_.destroy() != QuestError::none) {
        return VerticalSliceError::quest_runtime_error;
    }
    lifecycle_ = VerticalSliceLifecycle::destroyed;
    ++generation_;
    definition_ = nullptr;
    return VerticalSliceError::none;
}

VerticalSliceError VerticalSliceSession::submit_movement(
    std::span<const contracts::SessionCommand> commands
) noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::running &&
        lifecycle_ != VerticalSliceLifecycle::paused) {
        return VerticalSliceError::invalid_lifecycle;
    }
    last_movement_error_ = movement_.submit(commands);
    return last_movement_error_ == runtime::GameSessionError::none
               ? VerticalSliceError::none
               : VerticalSliceError::movement_session_error;
}

VerticalSliceError VerticalSliceSession::retry_from_safe_point(
    const contracts::SafePointRetryCommand& command
) noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::running &&
        lifecycle_ != VerticalSliceLifecycle::paused) {
        return VerticalSliceError::invalid_lifecycle;
    }
    last_movement_error_ = movement_.retry_from_safe_point(command);
    if (last_movement_error_ != runtime::GameSessionError::none) {
        return VerticalSliceError::movement_session_error;
    }
    previous_snapshot_ = current_snapshot_;
    ++generation_;
    refresh_snapshot();
    return VerticalSliceError::none;
}

VerticalSliceAdvanceResult VerticalSliceSession::advance(std::uint32_t tick_budget) noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::running) {
        return {VerticalSliceError::invalid_lifecycle, 0};
    }
    const auto result = movement_.advance(tick_budget);
    last_movement_error_ = result.error;
    if (result.error != runtime::GameSessionError::none) {
        return {VerticalSliceError::movement_session_error, result.executed_ticks};
    }
    if (result.executed_ticks != 0) {
        previous_snapshot_ = current_snapshot_;
        simulation_ticks_ += result.executed_ticks;
        refresh_snapshot();
    }
    return {VerticalSliceError::none, result.executed_ticks};
}

CompleteObjectiveResult VerticalSliceSession::complete_objective(
    contracts::StableContentKey objective
) noexcept {
    DiscardingQuestEventSink sink;
    return complete_objective(objective, sink);
}

CompleteObjectiveResult VerticalSliceSession::complete_objective(
    contracts::StableContentKey objective,
    IQuestEventSink& sink
) noexcept {
    if (lifecycle_ != VerticalSliceLifecycle::running &&
        lifecycle_ != VerticalSliceLifecycle::resolved) {
        return {VerticalSliceError::invalid_lifecycle};
    }
    const auto state = quest_.objective_state(objective);
    if (state == QuestObjectiveState::unknown) {
        return {VerticalSliceError::unknown_objective};
    }
    if (state == QuestObjectiveState::locked) {
        return {VerticalSliceError::objective_not_active};
    }
    const contracts::QuestCommand command{
        current_snapshot_.tick,
        definition_->player.actor,
        quest_command_sequence_,
        contracts::QuestCommandType::complete_objective,
        objective,
    };
    const auto result = quest_.apply(command, sink);
    if (result.error == QuestError::unknown_objective) {
        return {VerticalSliceError::unknown_objective};
    }
    if (result.error == QuestError::objective_not_active) {
        return {VerticalSliceError::objective_not_active};
    }
    if (result.error != QuestError::none) {
        return {VerticalSliceError::quest_runtime_error};
    }
    ++quest_command_sequence_;
    previous_snapshot_ = current_snapshot_;
    if (result.quest_resolved && lifecycle_ != VerticalSliceLifecycle::resolved) {
        last_movement_error_ = movement_.pause();
        if (last_movement_error_ != runtime::GameSessionError::none) {
            return {VerticalSliceError::movement_session_error, result.accepted, false, false};
        }
        lifecycle_ = VerticalSliceLifecycle::resolved;
    }
    refresh_snapshot();
    return {
        VerticalSliceError::none,
        result.accepted,
        result.stage_advanced,
        result.quest_resolved,
    };
}

VerticalSliceLifecycle VerticalSliceSession::lifecycle() const noexcept {
    return lifecycle_;
}

std::uint32_t VerticalSliceSession::generation() const noexcept {
    return generation_;
}

runtime::GameSessionError VerticalSliceSession::last_movement_error() const noexcept {
    return last_movement_error_;
}

const contracts::VerticalSliceDefinition* VerticalSliceSession::definition() const noexcept {
    return definition_;
}

const VerticalSliceSnapshot& VerticalSliceSession::previous_snapshot() const noexcept {
    return previous_snapshot_;
}

const VerticalSliceSnapshot& VerticalSliceSession::current_snapshot() const noexcept {
    return current_snapshot_;
}

const contracts::QuestSnapshot& VerticalSliceSession::quest_snapshot() const noexcept {
    return quest_.snapshot();
}

const IQuestRuntime& VerticalSliceSession::quest_runtime() const noexcept {
    return quest_;
}

QuestObjectiveState VerticalSliceSession::objective_state(
    contracts::StableContentKey objective
) const noexcept {
    return quest_.objective_state(objective);
}

bool VerticalSliceSession::valid_definition(
    const contracts::VerticalSliceDefinition& definition
) const noexcept {
    if (definition.id.key == 0 || definition.id.name.empty() || definition.beats.empty() ||
        definition.beats.size() > max_beats || definition.cell_ids.empty() ||
        definition.player.actor == 0 || definition.player.move_speed_mm_per_second <= 0 ||
        definition.player.jump_speed_mm_per_second <= 0 ||
        definition.player.gravity_mm_per_second_squared <= 0 ||
        definition.player.collision_radius_mm <= 0 ||
        definition.player.collision_height_mm <= 0 || !valid_basis(definition.player.camera_basis)) {
        return false;
    }

    std::array<contracts::StableContentKey, max_beats + max_objectives> keys{};
    std::size_t key_count = 0;
    std::size_t objective_count = 0;
    std::uint32_t minutes = 0;
    for (const auto& beat : definition.beats) {
        if (beat.id.key == 0 || beat.id.name.empty() || beat.cell_id.key == 0 ||
            beat.target_minutes == 0 || beat.objectives.empty() ||
            !contains_content_id(definition.cell_ids, beat.cell_id.key)) {
            return false;
        }
        for (std::size_t index = 0; index < key_count; ++index) {
            if (keys[index] == beat.id.key) {
                return false;
            }
        }
        keys[key_count++] = beat.id.key;
        minutes += beat.target_minutes;
        for (const auto& objective : beat.objectives) {
            if (objective.key == 0 || objective.name.empty() || objective_count >= max_objectives) {
                return false;
            }
            for (std::size_t index = 0; index < key_count; ++index) {
                if (keys[index] == objective.key) {
                    return false;
                }
            }
            keys[key_count++] = objective.key;
            ++objective_count;
        }
    }
    return minutes == definition.playable_target_minutes &&
           definition.playable_target_minutes >= 60 &&
           definition.end_to_end_test_budget_minutes >= definition.playable_target_minutes;
}

void VerticalSliceSession::refresh_snapshot() noexcept {
    const auto& movement_snapshot = movement_.current_snapshot();
    const auto& quest_snapshot = quest_.snapshot();
    const auto& beat = definition_->beats[quest_snapshot.stage_index];
    current_snapshot_.tick = movement_snapshot.tick;
    current_snapshot_.slice_id = definition_->id;
    current_snapshot_.beat_id = beat.id;
    current_snapshot_.cell_id = beat.cell_id;
    current_snapshot_.player_pose = movement_snapshot.player_pose;
    current_snapshot_.beat_index = quest_snapshot.stage_index;
    current_snapshot_.beat_count = quest_snapshot.stage_count;
    current_snapshot_.completed_objectives = quest_snapshot.completed_in_stage;
    current_snapshot_.required_objectives = quest_snapshot.required_in_stage;
    current_snapshot_.simulation_ticks = simulation_ticks_;
    current_snapshot_.resolved = quest_snapshot.resolved;
    update_checksum();
}

void VerticalSliceSession::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, current_snapshot_.tick);
    hash_integer(hash, current_snapshot_.slice_id.key);
    hash_integer(hash, current_snapshot_.beat_id.key);
    hash_integer(hash, current_snapshot_.cell_id.key);
    hash_integer(hash, current_snapshot_.player_pose.x);
    hash_integer(hash, current_snapshot_.player_pose.y);
    hash_integer(hash, current_snapshot_.player_pose.height);
    hash_integer(hash, current_snapshot_.player_pose.floor_layer);
    hash_integer(hash, current_snapshot_.beat_index);
    hash_integer(hash, current_snapshot_.completed_objectives);
    hash_integer(hash, current_snapshot_.required_objectives);
    hash_integer(hash, current_snapshot_.simulation_ticks);
    hash_byte(hash, current_snapshot_.resolved ? 1U : 0U);
    hash_integer(hash, quest_.snapshot().checksum);
    current_snapshot_.checksum = hash;
}

}  // namespace tgd::gameplay
