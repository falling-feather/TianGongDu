#include <tgd/gameplay/encounter_director.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
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
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & 0xffU));
        bits >>= 8U;
    }
}

[[nodiscard]] std::uint64_t squared_component(std::int64_t value) noexcept {
    const auto magnitude = value < 0
                               ? static_cast<std::uint64_t>(-(value + 1)) + 1U
                               : static_cast<std::uint64_t>(value);
    if (magnitude != 0 &&
        magnitude > std::numeric_limits<std::uint64_t>::max() / magnitude) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return magnitude * magnitude;
}

[[nodiscard]] std::uint64_t distance_squared(
    const contracts::GroundPoseMm& left,
    const contracts::GroundPoseMm& right
) noexcept {
    const auto x = squared_component(static_cast<std::int64_t>(left.x) - right.x);
    const auto y = squared_component(static_cast<std::int64_t>(left.y) - right.y);
    if (std::numeric_limits<std::uint64_t>::max() - x < y) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return x + y;
}

[[nodiscard]] std::uint64_t integer_sqrt_floor(std::uint64_t value) noexcept {
    std::uint64_t result = 0;
    std::uint64_t bit = 1ULL << 62U;
    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result;
}

[[nodiscard]] std::uint64_t integer_sqrt_ceil(std::uint64_t value) noexcept {
    if (value == 0) {
        return 0;
    }
    const auto floor = integer_sqrt_floor(value);
    return floor == value / floor && value % floor == 0 ? floor : floor + 1U;
}

[[nodiscard]] std::uint64_t range_squared(std::int32_t range_mm) noexcept {
    const auto range = static_cast<std::uint64_t>(range_mm);
    return range * range;
}

[[nodiscard]] bool height_matches(
    const contracts::GroundPoseMm& left,
    const contracts::GroundPoseMm& right,
    std::int32_t tolerance
) noexcept {
    const auto delta = static_cast<std::int64_t>(left.height) - right.height;
    const auto magnitude = delta < 0 ? -delta : delta;
    return magnitude <= tolerance;
}

[[nodiscard]] std::int32_t saturating_add(std::int32_t value, std::int64_t delta) noexcept {
    const auto sum = static_cast<std::int64_t>(value) + delta;
    return static_cast<std::int32_t>(std::clamp(
        sum,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
    ));
}

constexpr std::size_t tactical_duty_count = 4;

[[nodiscard]] std::size_t tactical_duty_index(
    contracts::EncounterTacticalDuty duty
) noexcept {
    switch (duty) {
        case contracts::EncounterTacticalDuty::pressure:
            return 0;
        case contracts::EncounterTacticalDuty::flanker:
            return 1;
        case contracts::EncounterTacticalDuty::harrier:
            return 2;
        case contracts::EncounterTacticalDuty::controller:
            return 3;
        default:
            return tactical_duty_count;
    }
}

[[nodiscard]] contracts::TickIndex token_lease_until(
    contracts::TickIndex tick,
    std::uint16_t lease_ticks
) noexcept {
    const auto additional_ticks = static_cast<contracts::TickIndex>(lease_ticks - 1U);
    if (additional_ticks > std::numeric_limits<contracts::TickIndex>::max() - tick) {
        return std::numeric_limits<contracts::TickIndex>::max();
    }
    return tick + additional_ticks;
}

}  // namespace

EncounterDirectorError DeterministicEncounterDirector::initialize(
    const contracts::EncounterDirectorDefinition& definition,
    std::span<const contracts::CombatActorConfig> actors,
    std::span<const contracts::AbilityDefinition> abilities
) noexcept {
    if (initialized_) {
        return EncounterDirectorError::invalid_lifecycle;
    }
    const bool has_token_lease = definition.attack_token_lease_ticks != 0;
    const bool has_actor_duties = !definition.actor_duties.empty();
    if (definition.player_actor == 0 || definition.aggro_range_mm <= 0 ||
        definition.leash_range_mm < definition.aggro_range_mm ||
        definition.chase_speed_mm_per_second <= 0 ||
        definition.chase_speed_mm_per_second % 60 != 0 ||
        definition.formation_radius_mm <= 0 ||
        definition.formation_radius_mm >= definition.aggro_range_mm ||
        definition.decision_interval_ticks == 0 ||
        definition.max_simultaneous_attackers == 0 ||
        definition.max_simultaneous_attackers > hostile_capacity || actors.empty() ||
        actors.size() > hostile_capacity + 1 || abilities.empty() ||
        abilities.size() > ability_capacity || has_token_lease != has_actor_duties ||
        definition.attack_token_lease_ticks > max_attack_token_lease_ticks ||
        definition.actor_duties.size() > hostile_capacity) {
        return EncounterDirectorError::invalid_definition;
    }

    definition_ = definition;
    definition_.actor_duties = {};
    tactical_tokens_enabled_ = has_token_lease;
    duty_cursor_ = 0;
    ability_count_ = abilities.size();
    std::copy(abilities.begin(), abilities.end(), abilities_.begin());
    std::sort(
        abilities_.begin(),
        abilities_.begin() + static_cast<std::ptrdiff_t>(ability_count_),
        [](const contracts::AbilityDefinition& left, const contracts::AbilityDefinition& right) {
            return left.id.key < right.id.key;
        }
    );

    bool player_found = false;
    hostile_count_ = 0;
    for (const auto& actor : actors) {
        if (actor.actor == 0 || actor.initial_stance == 0 || actor.stance_count == 0 ||
            actor.stance_count > actor.stance_ids.size()) {
            return EncounterDirectorError::invalid_actor_config;
        }
        if (actor.actor == definition.player_actor) {
            if (player_found || actor.faction != contracts::CombatFaction::player) {
                return EncounterDirectorError::invalid_actor_config;
            }
            player_found = true;
        } else if (actor.faction == contracts::CombatFaction::hostile) {
            if (hostile_count_ >= hostile_capacity) {
                return EncounterDirectorError::invalid_actor_config;
            }
            hostiles_[hostile_count_] = {
                actor.actor,
                actor.initial_pose,
                static_cast<std::uint8_t>(hostile_count_),
                contracts::EncounterTacticalDuty::pressure,
                0,
                0,
                0,
                0,
            };
            ++hostile_count_;
            for (std::size_t stance = 0; stance < actor.stance_count; ++stance) {
                if (find_ability(
                        contracts::CombatCommandType::light_attack,
                        actor.stance_ids[stance]
                    ) == nullptr ||
                    find_ability(
                        contracts::CombatCommandType::heavy_attack,
                        actor.stance_ids[stance]
                    ) == nullptr) {
                    return EncounterDirectorError::missing_ability;
                }
            }
        }
    }
    if (!player_found) {
        return EncounterDirectorError::missing_player;
    }
    if (hostile_count_ == 0) {
        return EncounterDirectorError::missing_hostile;
    }
    if (definition_.max_simultaneous_attackers > hostile_count_) {
        return EncounterDirectorError::invalid_definition;
    }
    std::sort(
        hostiles_.begin(),
        hostiles_.begin() + static_cast<std::ptrdiff_t>(hostile_count_),
        [](const HostileRuntime& left, const HostileRuntime& right) {
            return left.actor < right.actor;
        }
    );
    for (std::size_t index = 1; index < hostile_count_; ++index) {
        if (hostiles_[index - 1].actor == hostiles_[index].actor) {
            return EncounterDirectorError::invalid_actor_config;
        }
    }
    if (tactical_tokens_enabled_) {
        if (definition.actor_duties.size() != hostile_count_) {
            return EncounterDirectorError::invalid_definition;
        }
        for (std::size_t index = 0; index < definition.actor_duties.size(); ++index) {
            const auto& binding = definition.actor_duties[index];
            if (binding.actor == 0 || tactical_duty_index(binding.duty) >= tactical_duty_count) {
                return EncounterDirectorError::invalid_definition;
            }
            const auto runtime = std::find_if(
                hostiles_.begin(),
                hostiles_.begin() + static_cast<std::ptrdiff_t>(hostile_count_),
                [&binding](const HostileRuntime& hostile) {
                    return hostile.actor == binding.actor;
                }
            );
            if (runtime == hostiles_.begin() + static_cast<std::ptrdiff_t>(hostile_count_)) {
                return EncounterDirectorError::invalid_definition;
            }
            for (std::size_t prior = 0; prior < index; ++prior) {
                if (definition.actor_duties[prior].actor == binding.actor) {
                    return EncounterDirectorError::invalid_definition;
                }
            }
            runtime->duty = binding.duty;
        }
    }

    initialized_ = true;
    update_checksum();
    return EncounterDirectorError::none;
}

EncounterPlanResult DeterministicEncounterDirector::plan_tick(
    contracts::TickIndex tick,
    std::span<const contracts::CombatActorSnapshot> actors,
    contracts::CommandSequence first_sequence
) noexcept {
    EncounterPlanResult result{};
    if (!initialized_) {
        result.error = EncounterDirectorError::invalid_lifecycle;
        return result;
    }
    if (tick != current_tick_ + 1) {
        result.error = EncounterDirectorError::wrong_tick;
        return result;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        if (actors[index].actor == 0 ||
            (actors[index].active && actors[index].defeated)) {
            result.error = EncounterDirectorError::invalid_snapshot;
            return result;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (actors[previous].actor == actors[index].actor) {
                result.error = EncounterDirectorError::invalid_snapshot;
                return result;
            }
        }
    }
    const auto* player = find_snapshot(actors, definition_.player_actor);
    if (player == nullptr) {
        result.error = EncounterDirectorError::invalid_snapshot;
        return result;
    }
    for (std::size_t index = 0; index < hostile_count_; ++index) {
        if (find_snapshot(actors, hostiles_[index].actor) == nullptr) {
            result.error = EncounterDirectorError::invalid_snapshot;
            return result;
        }
    }

    std::array<AttackCandidate, hostile_capacity> candidates{};
    std::size_t candidate_count = 0;
    std::size_t occupied_attackers = 0;
    const auto aggro_squared = range_squared(definition_.aggro_range_mm);
    const auto leash_squared = range_squared(definition_.leash_range_mm);
    for (std::size_t index = 0; index < hostile_count_; ++index) {
        const auto* hostile = find_snapshot(actors, hostiles_[index].actor);
        if (!hostile->active || hostile->defeated) {
            continue;
        }
        const bool holds_token = tactical_tokens_enabled_ &&
                                 hostiles_[index].token_lease_until_tick >= tick;
        if (hostile->active_ability != 0 || holds_token) {
            ++occupied_attackers;
            continue;
        }

        const auto player_distance = distance_squared(hostile->pose, player->pose);
        const auto home_distance = distance_squared(hostile->pose, hostiles_[index].home_pose);
        const bool engaged = player->active && hostile->pose.floor_layer == player->pose.floor_layer &&
                             player_distance <= aggro_squared && home_distance <= leash_squared;
        const auto* light = find_ability(
            contracts::CombatCommandType::light_attack,
            hostile->stance
        );
        if (light == nullptr) {
            result.error = EncounterDirectorError::missing_ability;
            return result;
        }
        const auto approach = formation_target(player->pose, index);
        const auto approach_distance = distance_squared(hostile->pose, approach);
        const auto chase_step = definition_.chase_speed_mm_per_second / 60;
        const bool at_approach = approach_distance <= range_squared(chase_step);
        const bool attack_range = engaged && at_approach &&
                                  height_matches(hostile->pose, player->pose, light->height_tolerance_mm) &&
                                  player_distance <= range_squared(light->range_mm);
        if (attack_range) {
            if (tick >= hostiles_[index].next_attack_tick &&
                tick % definition_.decision_interval_ticks == 0) {
                candidates[candidate_count++] = {index, player_distance};
            }
            continue;
        }

        const auto& target = engaged ? approach : hostiles_[index].home_pose;
        const auto next_pose = move_toward(hostile->pose, target);
        if (next_pose != hostile->pose) {
            if (result.batch.pose_update_count >= EncounterPlanBatch::capacity) {
                result.error = EncounterDirectorError::output_capacity_exceeded;
                return result;
            }
            result.batch.pose_updates[result.batch.pose_update_count++] = {
                tick,
                hostile->actor,
                next_pose,
            };
        }
    }

    const auto attacker_limit = static_cast<std::size_t>(definition_.max_simultaneous_attackers);
    std::size_t available_attackers = occupied_attackers >= attacker_limit
                                          ? 0
                                          : attacker_limit - occupied_attackers;
    std::array<std::size_t, hostile_capacity> selected_runtime_indices{};
    std::size_t selected_count = 0;
    auto next_duty_cursor = duty_cursor_;
    if (!tactical_tokens_enabled_) {
        std::sort(
            candidates.begin(),
            candidates.begin() + static_cast<std::ptrdiff_t>(candidate_count),
            [this](const AttackCandidate& left, const AttackCandidate& right) {
                return std::tuple{left.distance_squared, hostiles_[left.runtime_index].actor} <
                       std::tuple{right.distance_squared, hostiles_[right.runtime_index].actor};
            }
        );
        selected_count = std::min(candidate_count, available_attackers);
        for (std::size_t index = 0; index < selected_count; ++index) {
            selected_runtime_indices[index] = candidates[index].runtime_index;
        }
    } else {
        std::array<bool, hostile_capacity> selected_candidates{};
        while (selected_count < available_attackers) {
            std::size_t selected_candidate = hostile_capacity;
            std::size_t selected_duty = tactical_duty_count;
            for (std::size_t duty_offset = 0; duty_offset < tactical_duty_count;
                 ++duty_offset) {
                const auto duty =
                    (static_cast<std::size_t>(next_duty_cursor) + duty_offset) %
                    tactical_duty_count;
                for (std::size_t candidate = 0; candidate < candidate_count; ++candidate) {
                    const auto& value = candidates[candidate];
                    const auto& runtime = hostiles_[value.runtime_index];
                    if (selected_candidates[candidate] ||
                        tactical_duty_index(runtime.duty) != duty) {
                        continue;
                    }
                    if (selected_candidate == hostile_capacity) {
                        selected_candidate = candidate;
                        continue;
                    }
                    const auto& selected = candidates[selected_candidate];
                    const auto& selected_runtime = hostiles_[selected.runtime_index];
                    if (std::tuple{runtime.last_token_grant_tick, value.distance_squared,
                                   runtime.actor} <
                        std::tuple{selected_runtime.last_token_grant_tick,
                                   selected.distance_squared, selected_runtime.actor}) {
                        selected_candidate = candidate;
                    }
                }
                if (selected_candidate != hostile_capacity) {
                    selected_duty = duty;
                    break;
                }
            }
            if (selected_candidate == hostile_capacity) {
                break;
            }
            selected_candidates[selected_candidate] = true;
            selected_runtime_indices[selected_count++] =
                candidates[selected_candidate].runtime_index;
            next_duty_cursor = static_cast<std::uint8_t>(
                (selected_duty + 1U) % tactical_duty_count
            );
        }
    }
    std::array<const contracts::AbilityDefinition*, hostile_capacity> selected_abilities{};
    for (std::size_t index = 0; index < selected_count; ++index) {
        const auto& runtime = hostiles_[selected_runtime_indices[index]];
        const auto* snapshot = find_snapshot(actors, runtime.actor);
        const bool use_heavy = (runtime.attack_count + 1U) % 3U == 0U;
        const auto trigger = use_heavy ? contracts::CombatCommandType::heavy_attack
                                       : contracts::CombatCommandType::light_attack;
        selected_abilities[index] = find_ability(trigger, snapshot->stance);
        if (selected_abilities[index] == nullptr) {
            result.error = EncounterDirectorError::missing_ability;
            return result;
        }
    }
    if (tactical_tokens_enabled_) {
        for (std::size_t index = 0; index < hostile_count_; ++index) {
            const auto* hostile = find_snapshot(actors, hostiles_[index].actor);
            if (!hostile->active || hostile->defeated ||
                hostiles_[index].token_lease_until_tick < tick) {
                hostiles_[index].token_lease_until_tick = 0;
            }
        }
    }
    for (std::size_t index = 0; index < selected_count; ++index) {
        auto& runtime = hostiles_[selected_runtime_indices[index]];
        const auto* ability = selected_abilities[index];
        const bool use_heavy = (runtime.attack_count + 1U) % 3U == 0U;
        const auto trigger = use_heavy ? contracts::CombatCommandType::heavy_attack
                                       : contracts::CombatCommandType::light_attack;
        result.batch.commands[result.batch.command_count++] = {
            tick,
            runtime.actor,
            first_sequence + result.batch.command_count,
            trigger,
            definition_.player_actor,
            0,
        };
        ++runtime.attack_count;
        runtime.next_attack_tick = tick + ability->windup_ticks + ability->active_ticks +
                                   ability->recovery_ticks +
                                   definition_.post_attack_cooldown_ticks;
        if (tactical_tokens_enabled_) {
            runtime.token_lease_until_tick =
                token_lease_until(tick, definition_.attack_token_lease_ticks);
            runtime.last_token_grant_tick = tick;
        }
    }
    if (tactical_tokens_enabled_ && selected_count != 0) {
        duty_cursor_ = next_duty_cursor;
    }

    current_tick_ = tick;
    update_checksum();
    return result;
}

EncounterDirectorError DeterministicEncounterDirector::retry_from_initial(
    const contracts::SafePointRetryCommand& command
) noexcept {
    if (!initialized_) {
        return EncounterDirectorError::invalid_lifecycle;
    }
    if (command.completed_tick != current_tick_) {
        return EncounterDirectorError::retry_targets_wrong_tick;
    }
    if (command.actor != definition_.player_actor ||
        command.reason != contracts::SafePointRetryReason::player_defeated) {
        return EncounterDirectorError::retry_not_allowed;
    }
    if (command.sequence == 0 || command.sequence <= last_boundary_sequence_) {
        return EncounterDirectorError::stale_retry_sequence;
    }
    for (std::size_t index = 0; index < hostile_count_; ++index) {
        hostiles_[index].next_attack_tick = 0;
        hostiles_[index].token_lease_until_tick = 0;
        hostiles_[index].last_token_grant_tick = 0;
        hostiles_[index].attack_count = 0;
    }
    duty_cursor_ = 0;
    last_boundary_sequence_ = command.sequence;
    update_checksum();
    return EncounterDirectorError::none;
}

EncounterDirectorError DeterministicEncounterDirector::activate_group(
    const contracts::EncounterActivationCommand& command,
    std::span<const contracts::EncounterActorPlacementDefinition> actor_placements,
    std::span<const contracts::CombatActorSnapshot> actors
) noexcept {
    if (!initialized_) {
        return EncounterDirectorError::invalid_lifecycle;
    }
    if (command.completed_tick != current_tick_) {
        return EncounterDirectorError::activation_targets_wrong_tick;
    }
    const bool replace = command.mode == contracts::EncounterActivationMode::replace;
    const bool reinforce = command.mode == contracts::EncounterActivationMode::reinforce;
    const auto* player = find_snapshot(actors, definition_.player_actor);
    if (command.actor != definition_.player_actor || player == nullptr || !player->active ||
        (!replace && !reinforce) ||
        actor_placements.empty() || actor_placements.size() > hostile_count_) {
        return EncounterDirectorError::activation_not_allowed;
    }
    if (command.sequence == 0 || command.sequence <= last_boundary_sequence_) {
        return EncounterDirectorError::stale_activation_sequence;
    }
    for (std::size_t index = 0; index < hostile_count_; ++index) {
        if (find_snapshot(actors, hostiles_[index].actor) == nullptr) {
            return EncounterDirectorError::invalid_snapshot;
        }
    }

    std::array<std::size_t, hostile_capacity> placement_indices{};
    for (std::size_t index = 0; index < actor_placements.size(); ++index) {
        const auto runtime = std::find_if(
            hostiles_.begin(),
            hostiles_.begin() + static_cast<std::ptrdiff_t>(hostile_count_),
            [&actor_placements, index](const HostileRuntime& hostile) {
                return hostile.actor == actor_placements[index].actor;
            }
        );
        if (runtime == hostiles_.begin() + static_cast<std::ptrdiff_t>(hostile_count_) ||
            actor_placements[index].formation_slot >=
                contracts::encounter_formation_slot_capacity) {
            return EncounterDirectorError::activation_not_allowed;
        }
        const auto* snapshot = find_snapshot(actors, actor_placements[index].actor);
        if (snapshot == nullptr ||
            (reinforce && (snapshot->active || snapshot->defeated))) {
            return EncounterDirectorError::activation_not_allowed;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (placement_indices[prior] ==
                    static_cast<std::size_t>(runtime - hostiles_.begin()) ||
                actor_placements[prior].formation_slot ==
                    actor_placements[index].formation_slot) {
                return EncounterDirectorError::activation_not_allowed;
            }
        }
        placement_indices[index] = static_cast<std::size_t>(runtime - hostiles_.begin());
    }

    if (reinforce) {
        for (std::size_t existing = 0; existing < hostile_count_; ++existing) {
            const auto* snapshot = find_snapshot(actors, hostiles_[existing].actor);
            if (!snapshot->active) {
                continue;
            }
            for (const auto& placement : actor_placements) {
                if (hostiles_[existing].formation_slot == placement.formation_slot) {
                    return EncounterDirectorError::activation_not_allowed;
                }
            }
        }
    }

    if (replace) {
        for (std::size_t index = 0; index < hostile_count_; ++index) {
            hostiles_[index].next_attack_tick = 0;
            hostiles_[index].token_lease_until_tick = 0;
            hostiles_[index].last_token_grant_tick = 0;
            hostiles_[index].attack_count = 0;
        }
        duty_cursor_ = 0;
    }
    for (std::size_t index = 0; index < actor_placements.size(); ++index) {
        auto& runtime = hostiles_[placement_indices[index]];
        runtime.home_pose = actor_placements[index].pose;
        runtime.formation_slot = actor_placements[index].formation_slot;
        runtime.next_attack_tick = 0;
        runtime.token_lease_until_tick = 0;
        runtime.last_token_grant_tick = 0;
        runtime.attack_count = 0;
    }
    last_boundary_sequence_ = command.sequence;
    update_checksum();
    return EncounterDirectorError::none;
}

contracts::TickIndex DeterministicEncounterDirector::current_tick() const noexcept {
    return current_tick_;
}

std::uint64_t DeterministicEncounterDirector::checksum() const noexcept {
    return checksum_;
}

const contracts::CombatActorSnapshot* DeterministicEncounterDirector::find_snapshot(
    std::span<const contracts::CombatActorSnapshot> actors,
    contracts::StableActorKey actor
) const noexcept {
    const auto value = std::find_if(
        actors.begin(),
        actors.end(),
        [actor](const contracts::CombatActorSnapshot& candidate) {
            return candidate.actor == actor;
        }
    );
    return value == actors.end() ? nullptr : &*value;
}

const contracts::AbilityDefinition* DeterministicEncounterDirector::find_ability(
    contracts::CombatCommandType trigger,
    contracts::StableContentKey stance
) const noexcept {
    const auto value = std::find_if(
        abilities_.begin(),
        abilities_.begin() + static_cast<std::ptrdiff_t>(ability_count_),
        [trigger, stance](const contracts::AbilityDefinition& ability) {
            return ability.trigger == trigger && ability.required_stance == stance;
        }
    );
    return value == abilities_.begin() + static_cast<std::ptrdiff_t>(ability_count_)
               ? nullptr
               : &*value;
}

contracts::GroundPoseMm DeterministicEncounterDirector::move_toward(
    const contracts::GroundPoseMm& source,
    const contracts::GroundPoseMm& target
) const noexcept {
    if (source.floor_layer != target.floor_layer) {
        return source;
    }
    const auto distance = integer_sqrt_ceil(distance_squared(source, target));
    if (distance == 0) {
        return source;
    }
    const auto step = static_cast<std::uint64_t>(definition_.chase_speed_mm_per_second / 60);
    if (distance <= step) {
        return {target.x, target.y, source.height, source.floor_layer};
    }
    const auto delta_x = static_cast<std::int64_t>(target.x) - source.x;
    const auto delta_y = static_cast<std::int64_t>(target.y) - source.y;
    return {
        saturating_add(source.x, delta_x * static_cast<std::int64_t>(step) /
                                     static_cast<std::int64_t>(distance)),
        saturating_add(source.y, delta_y * static_cast<std::int64_t>(step) /
                                     static_cast<std::int64_t>(distance)),
        source.height,
        source.floor_layer,
    };
}

contracts::GroundPoseMm DeterministicEncounterDirector::formation_target(
    const contracts::GroundPoseMm& player,
    std::size_t hostile_index
) const noexcept {
    constexpr std::array<contracts::GroundVectorQ15, 8> directions{{
        {contracts::ground_axis_one, 0},
        {23'170, 23'170},
        {0, contracts::ground_axis_one},
        {-23'170, 23'170},
        {-contracts::ground_axis_one, 0},
        {-23'170, -23'170},
        {0, -contracts::ground_axis_one},
        {23'170, -23'170},
    }};
    const auto slot = static_cast<std::size_t>(hostiles_[hostile_index].formation_slot);
    const auto& direction = directions[slot % directions.size()];
    const auto ring = static_cast<std::int64_t>(slot / directions.size() + 1U);
    const auto radius = static_cast<std::int64_t>(definition_.formation_radius_mm) * ring;
    return {
        saturating_add(
            player.x,
            radius * direction.x / contracts::ground_axis_one
        ),
        saturating_add(
            player.y,
            radius * direction.y / contracts::ground_axis_one
        ),
        player.height,
        player.floor_layer,
    };
}

void DeterministicEncounterDirector::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, current_tick_);
    if (last_boundary_sequence_ != 0) {
        hash_integer(hash, last_boundary_sequence_);
    }
    hash_integer(hash, definition_.player_actor);
    hash_integer(hash, definition_.aggro_range_mm);
    hash_integer(hash, definition_.leash_range_mm);
    hash_integer(hash, definition_.chase_speed_mm_per_second);
    hash_integer(hash, definition_.formation_radius_mm);
    hash_integer(hash, definition_.decision_interval_ticks);
    hash_integer(hash, definition_.post_attack_cooldown_ticks);
    hash_integer(hash, definition_.max_simultaneous_attackers);
    if (tactical_tokens_enabled_) {
        hash_integer(hash, definition_.attack_token_lease_ticks);
        hash_integer(hash, duty_cursor_);
    }
    for (std::size_t index = 0; index < hostile_count_; ++index) {
        const auto& hostile = hostiles_[index];
        hash_integer(hash, hostile.actor);
        hash_integer(hash, hostile.home_pose.x);
        hash_integer(hash, hostile.home_pose.y);
        hash_integer(hash, hostile.home_pose.height);
        hash_integer(hash, hostile.home_pose.floor_layer);
        hash_integer(hash, hostile.formation_slot);
        if (tactical_tokens_enabled_) {
            hash_integer(hash, static_cast<std::uint8_t>(hostile.duty));
            hash_integer(hash, hostile.token_lease_until_tick);
            hash_integer(hash, hostile.last_token_grant_tick);
        }
        hash_integer(hash, hostile.next_attack_tick);
        hash_integer(hash, hostile.attack_count);
    }
    checksum_ = hash;
}

}  // namespace tgd::gameplay
