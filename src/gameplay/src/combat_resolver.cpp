#include <tgd/gameplay/combat_resolver.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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

[[nodiscard]] bool same_command_key(
    const contracts::CombatCommand& left,
    const contracts::CombatCommand& right
) noexcept {
    return left.tick == right.tick && left.actor == right.actor &&
           left.sequence == right.sequence && left.type == right.type;
}

[[nodiscard]] bool valid_resources(const contracts::CombatResources& resources) noexcept {
    return resources.health_max > 0 && resources.health >= 0 &&
           resources.health <= resources.health_max && resources.stamina_max > 0 &&
           resources.stamina >= 0 && resources.stamina <= resources.stamina_max &&
           resources.poise_max > 0 && resources.poise >= 0 &&
           resources.poise <= resources.poise_max && resources.lantern_max >= 0 &&
           resources.lantern >= 0 && resources.lantern <= resources.lantern_max &&
           resources.evidence >= 0;
}

[[nodiscard]] std::int32_t clamp_subtract(std::int32_t value, std::int32_t amount) noexcept {
    return std::max(0, value - amount);
}

}  // namespace

CombatError DeterministicCombatResolver::initialize(
    std::span<const contracts::CombatActorConfig> actors,
    std::span<const contracts::AbilityDefinition> abilities
) noexcept {
    if (lifecycle_ != CombatLifecycle::uninitialized) {
        return CombatError::invalid_lifecycle;
    }
    const auto validation = validate_config(actors, abilities);
    if (validation != CombatError::none) {
        return validation;
    }

    actor_count_ = actors.size();
    std::copy(actors.begin(), actors.end(), actor_configs_.begin());
    std::sort(
        actor_configs_.begin(),
        actor_configs_.begin() + static_cast<std::ptrdiff_t>(actor_count_),
        [](const contracts::CombatActorConfig& left, const contracts::CombatActorConfig& right) {
            return left.actor < right.actor;
        }
    );
    for (std::size_t index = 0; index < actor_count_; ++index) {
        const auto& config = actor_configs_[index];
        actor_snapshots_[index] = {
            config.actor,
            config.archetype_id.key,
            config.faction,
            config.initial_pose,
            config.initial_resources,
            config.initial_stance,
            0,
            false,
            config.initial_resources.health > 0,
        };
        actor_runtime_[index] = {};
    }

    ability_count_ = abilities.size();
    std::copy(abilities.begin(), abilities.end(), abilities_.begin());
    std::sort(
        abilities_.begin(),
        abilities_.begin() + static_cast<std::ptrdiff_t>(ability_count_),
        [](const contracts::AbilityDefinition& left, const contracts::AbilityDefinition& right) {
            return left.id.key < right.id.key;
        }
    );
    lifecycle_ = CombatLifecycle::ready;
    update_checksum();
    return CombatError::none;
}

CombatError DeterministicCombatResolver::start() noexcept {
    if (lifecycle_ != CombatLifecycle::ready) {
        return CombatError::invalid_lifecycle;
    }
    lifecycle_ = CombatLifecycle::running;
    return CombatError::none;
}

CombatError DeterministicCombatResolver::pause() noexcept {
    if (lifecycle_ != CombatLifecycle::running) {
        return CombatError::invalid_lifecycle;
    }
    lifecycle_ = CombatLifecycle::paused;
    return CombatError::none;
}

CombatError DeterministicCombatResolver::resume() noexcept {
    if (lifecycle_ != CombatLifecycle::paused) {
        return CombatError::invalid_lifecycle;
    }
    lifecycle_ = CombatLifecycle::running;
    return CombatError::none;
}

CombatError DeterministicCombatResolver::destroy() noexcept {
    if (lifecycle_ == CombatLifecycle::uninitialized || lifecycle_ == CombatLifecycle::destroyed) {
        return CombatError::invalid_lifecycle;
    }
    lifecycle_ = CombatLifecycle::destroyed;
    actor_count_ = 0;
    ability_count_ = 0;
    command_count_ = 0;
    pose_update_count_ = 0;
    event_count_ = 0;
    return CombatError::none;
}

CombatError DeterministicCombatResolver::submit(
    std::span<const contracts::CombatCommand> commands
) noexcept {
    if (lifecycle_ != CombatLifecycle::running && lifecycle_ != CombatLifecycle::paused) {
        return CombatError::invalid_lifecycle;
    }
    if (commands.size() > command_capacity - command_count_) {
        return CombatError::command_queue_full;
    }
    for (std::size_t index = 0; index < commands.size(); ++index) {
        const auto& command = commands[index];
        if (command.tick <= current_tick_) {
            return CombatError::command_targets_past_tick;
        }
        if (!valid_command(command)) {
            return CombatError::invalid_command;
        }
        if (duplicate_command_key(command, commands, index)) {
            return CombatError::duplicate_command_key;
        }
    }
    for (const auto& command : commands) {
        commands_[command_count_++] = command;
    }
    return CombatError::none;
}

CombatError DeterministicCombatResolver::synchronize_poses(
    std::span<const contracts::CombatPoseUpdate> updates
) noexcept {
    if (lifecycle_ != CombatLifecycle::running && lifecycle_ != CombatLifecycle::paused) {
        return CombatError::invalid_lifecycle;
    }
    if (updates.size() > actor_capacity - pose_update_count_) {
        return CombatError::pose_update_queue_full;
    }
    const auto expected_tick = current_tick_ + 1;
    for (std::size_t index = 0; index < updates.size(); ++index) {
        const auto& update = updates[index];
        if (update.tick != expected_tick) {
            return CombatError::pose_targets_wrong_tick;
        }
        if (actor_index(update.actor) == actor_capacity) {
            return CombatError::invalid_pose_update;
        }
        if (duplicate_pose_update(update, updates, index)) {
            return CombatError::duplicate_pose_update;
        }
    }
    for (const auto& update : updates) {
        pose_updates_[pose_update_count_++] = update;
    }
    return CombatError::none;
}

CombatError DeterministicCombatResolver::advance_one_tick(ICombatEventSink& sink) noexcept {
    if (lifecycle_ != CombatLifecycle::running) {
        return CombatError::invalid_lifecycle;
    }
    sort_commands();
    event_count_ = 0;
    const auto next_tick = current_tick_ + 1;
    for (std::size_t index = 0; index < pose_update_count_; ++index) {
        const auto& update = pose_updates_[index];
        actor_snapshots_[actor_index(update.actor)].pose = update.pose;
    }
    std::size_t consumed = 0;
    while (consumed < command_count_ && commands_[consumed].tick == next_tick) {
        if (!process_command(commands_[consumed])) {
            return CombatError::event_capacity_exceeded;
        }
        ++consumed;
    }
    for (std::size_t index = 0; index < actor_count_; ++index) {
        if (!resolve_actor(index, next_tick)) {
            return CombatError::event_capacity_exceeded;
        }
    }
    current_tick_ = next_tick;
    compact_commands(consumed);
    pose_update_count_ = 0;
    update_checksum();
    sink.publish(std::span{events_}.first(event_count_));
    return CombatError::none;
}

CombatLifecycle DeterministicCombatResolver::lifecycle() const noexcept {
    return lifecycle_;
}

contracts::TickIndex DeterministicCombatResolver::current_tick() const noexcept {
    return current_tick_;
}

std::span<const contracts::CombatActorSnapshot> DeterministicCombatResolver::actors() const noexcept {
    return std::span{actor_snapshots_}.first(actor_count_);
}

std::uint64_t DeterministicCombatResolver::checksum() const noexcept {
    return checksum_;
}

CombatError DeterministicCombatResolver::validate_config(
    std::span<const contracts::CombatActorConfig> actors,
    std::span<const contracts::AbilityDefinition> abilities
) const noexcept {
    if (actors.empty() || abilities.empty()) {
        return CombatError::invalid_config;
    }
    if (actors.size() > actor_capacity) {
        return CombatError::too_many_actors;
    }
    if (abilities.size() > ability_capacity) {
        return CombatError::too_many_abilities;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        if (actor.actor == 0 || actor.archetype_id.key == 0 ||
            actor.stance_count == 0 || actor.stance_count > actor.stance_ids.size() ||
            actor.initial_stance == 0 || !valid_resources(actor.initial_resources)) {
            return CombatError::invalid_config;
        }
        bool initial_stance_found = false;
        for (std::size_t stance = 0; stance < actor.stance_count; ++stance) {
            if (actor.stance_ids[stance] == 0) {
                return CombatError::invalid_config;
            }
            initial_stance_found |= actor.stance_ids[stance] == actor.initial_stance;
            for (std::size_t previous = 0; previous < stance; ++previous) {
                if (actor.stance_ids[previous] == actor.stance_ids[stance]) {
                    return CombatError::invalid_config;
                }
            }
        }
        if (!initial_stance_found) {
            return CombatError::invalid_config;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (actors[previous].actor == actor.actor) {
                return CombatError::duplicate_actor;
            }
        }
    }
    for (std::size_t index = 0; index < abilities.size(); ++index) {
        const auto& ability = abilities[index];
        if (ability.id.key == 0 || ability.id.name.empty() || ability.active_ticks == 0 ||
            ability.stamina_cost < 0 || ability.range_mm < 0 ||
            ability.height_tolerance_mm < 0 || ability.health_damage < 0 ||
            ability.poise_damage < 0 ||
            (ability.trigger != contracts::CombatCommandType::evade &&
             ability.required_stance == 0) ||
            (ability.trigger == contracts::CombatCommandType::evade &&
             ability.active_ticks == 0)) {
            return CombatError::invalid_config;
        }
        if (ability.trigger != contracts::CombatCommandType::light_attack &&
            ability.trigger != contracts::CombatCommandType::heavy_attack &&
            ability.trigger != contracts::CombatCommandType::evade) {
            return CombatError::invalid_config;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (abilities[previous].id.key == ability.id.key) {
                return CombatError::duplicate_ability;
            }
            if (abilities[previous].trigger == ability.trigger &&
                abilities[previous].required_stance == ability.required_stance) {
                return CombatError::duplicate_trigger;
            }
        }
    }
    return CombatError::none;
}

bool DeterministicCombatResolver::valid_command(
    const contracts::CombatCommand& command
) const noexcept {
    const auto source = actor_index(command.actor);
    if (source == actor_capacity) {
        return false;
    }
    switch (command.type) {
        case contracts::CombatCommandType::light_attack:
        case contracts::CombatCommandType::heavy_attack:
            return command.target != 0 && command.target != command.actor && command.stance == 0 &&
                   actor_index(command.target) != actor_capacity;
        case contracts::CombatCommandType::guard_started:
        case contracts::CombatCommandType::guard_ended:
        case contracts::CombatCommandType::evade:
            return command.target == 0 && command.stance == 0;
        case contracts::CombatCommandType::switch_stance:
            return command.target == 0 && command.stance != 0 &&
                   actor_allows_stance(source, command.stance);
    }
    return false;
}

bool DeterministicCombatResolver::duplicate_command_key(
    const contracts::CombatCommand& command,
    std::span<const contracts::CombatCommand> pending,
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

bool DeterministicCombatResolver::duplicate_pose_update(
    const contracts::CombatPoseUpdate& update,
    std::span<const contracts::CombatPoseUpdate> pending,
    std::size_t pending_index
) const noexcept {
    for (std::size_t index = 0; index < pose_update_count_; ++index) {
        if (pose_updates_[index].actor == update.actor) {
            return true;
        }
    }
    for (std::size_t index = 0; index < pending_index; ++index) {
        if (pending[index].actor == update.actor) {
            return true;
        }
    }
    return false;
}

std::size_t DeterministicCombatResolver::actor_index(
    contracts::StableActorKey actor
) const noexcept {
    for (std::size_t index = 0; index < actor_count_; ++index) {
        if (actor_snapshots_[index].actor == actor) {
            return index;
        }
    }
    return actor_capacity;
}

std::size_t DeterministicCombatResolver::ability_index(
    contracts::CombatCommandType trigger,
    contracts::StableContentKey stance
) const noexcept {
    for (std::size_t index = 0; index < ability_count_; ++index) {
        const auto& ability = abilities_[index];
        if (ability.trigger == trigger &&
            (ability.required_stance == stance ||
             (trigger == contracts::CombatCommandType::evade && ability.required_stance == 0))) {
            return index;
        }
    }
    return ability_capacity;
}

bool DeterministicCombatResolver::actor_allows_stance(
    std::size_t index,
    contracts::StableContentKey stance
) const noexcept {
    const auto& config = actor_configs_[index];
    return std::find(
               config.stance_ids.begin(),
               config.stance_ids.begin() + config.stance_count,
               stance
           ) != config.stance_ids.begin() + config.stance_count;
}

bool DeterministicCombatResolver::target_in_range(
    const contracts::CombatActorSnapshot& source,
    const contracts::CombatActorSnapshot& target,
    const contracts::AbilityDefinition& ability
) const noexcept {
    if (source.pose.floor_layer != target.pose.floor_layer) {
        return false;
    }
    const auto height_delta = std::abs(
        static_cast<std::int64_t>(source.pose.height) - target.pose.height
    );
    if (height_delta > ability.height_tolerance_mm) {
        return false;
    }
    const auto delta_x = static_cast<std::int64_t>(source.pose.x) - target.pose.x;
    const auto delta_y = static_cast<std::int64_t>(source.pose.y) - target.pose.y;
    const auto range = static_cast<std::int64_t>(ability.range_mm);
    return delta_x * delta_x + delta_y * delta_y <= range * range;
}

void DeterministicCombatResolver::sort_commands() noexcept {
    std::sort(
        commands_.begin(),
        commands_.begin() + static_cast<std::ptrdiff_t>(command_count_),
        [](const contracts::CombatCommand& left, const contracts::CombatCommand& right) {
            return std::tuple{left.tick, left.actor, left.sequence, static_cast<std::uint8_t>(left.type)} <
                   std::tuple{right.tick, right.actor, right.sequence, static_cast<std::uint8_t>(right.type)};
        }
    );
}

void DeterministicCombatResolver::compact_commands(std::size_t consumed) noexcept {
    for (std::size_t index = consumed; index < command_count_; ++index) {
        commands_[index - consumed] = commands_[index];
    }
    command_count_ -= consumed;
}

bool DeterministicCombatResolver::emit(contracts::CombatEvent event) noexcept {
    if (event_count_ >= event_capacity) {
        return false;
    }
    events_[event_count_++] = event;
    return true;
}

bool DeterministicCombatResolver::process_command(
    const contracts::CombatCommand& command
) noexcept {
    const auto source_index = actor_index(command.actor);
    auto& actor = actor_snapshots_[source_index];
    auto& runtime = actor_runtime_[source_index];
    if (!actor.active) {
        return emit({command.tick, contracts::CombatEventType::command_ignored, command.actor});
    }
    if (command.type == contracts::CombatCommandType::switch_stance) {
        if (runtime.active_ability != ability_capacity || actor.stance == command.stance) {
            return emit({command.tick, contracts::CombatEventType::command_ignored, command.actor});
        }
        actor.stance = command.stance;
        return emit({command.tick, contracts::CombatEventType::stance_changed, command.actor, 0, command.stance});
    }
    if (command.type == contracts::CombatCommandType::guard_started ||
        command.type == contracts::CombatCommandType::guard_ended) {
        if (runtime.active_ability != ability_capacity) {
            return emit({command.tick, contracts::CombatEventType::command_ignored, command.actor});
        }
        const auto guard_requested = command.type == contracts::CombatCommandType::guard_started;
        if (actor.guarding == guard_requested ||
            (guard_requested && actor.resources.poise == 0)) {
            return emit({command.tick, contracts::CombatEventType::command_ignored, command.actor});
        }
        actor.guarding = guard_requested;
        return emit({
            command.tick,
            contracts::CombatEventType::guard_changed,
            command.actor,
            0,
            0,
            actor.guarding ? 1 : 0,
            contracts::feedback_guard,
        });
    }
    if (runtime.active_ability != ability_capacity) {
        return emit({command.tick, contracts::CombatEventType::command_ignored, command.actor});
    }
    const auto selected = ability_index(command.type, actor.stance);
    if (selected == ability_capacity || actor.resources.stamina < abilities_[selected].stamina_cost) {
        return emit({command.tick, contracts::CombatEventType::command_ignored, command.actor});
    }
    const auto& ability = abilities_[selected];
    actor.guarding = false;
    actor.resources.stamina -= ability.stamina_cost;
    actor.active_ability = ability.id.key;
    runtime.active_ability = static_cast<std::uint16_t>(selected);
    runtime.target = command.target;
    runtime.ability_started_tick = command.tick;
    runtime.hit_applied = false;
    if (command.type == contracts::CombatCommandType::evade) {
        runtime.evade_until_tick = command.tick + ability.active_ticks - 1U;
    }
    return emit({
        command.tick,
        contracts::CombatEventType::ability_started,
        command.actor,
        command.target,
        ability.id.key,
        -ability.stamina_cost,
        ability.feedback_tags,
    });
}

bool DeterministicCombatResolver::resolve_actor(
    std::size_t index,
    contracts::TickIndex tick
) noexcept {
    auto& runtime = actor_runtime_[index];
    auto& actor = actor_snapshots_[index];
    if (runtime.active_ability == ability_capacity) {
        return true;
    }
    const auto& ability = abilities_[runtime.active_ability];
    const auto hit_tick = runtime.ability_started_tick + ability.windup_ticks;
    if (!runtime.hit_applied && ability.trigger != contracts::CombatCommandType::evade &&
        tick >= hit_tick) {
        runtime.hit_applied = true;
        const auto target = actor_index(runtime.target);
        if (target == actor_capacity) {
            if (!emit({tick, contracts::CombatEventType::attack_missed, actor.actor, runtime.target, ability.id.key})) {
                return false;
            }
        } else if (!resolve_hit(index, target, ability, tick)) {
            return false;
        }
    }
    const auto complete_tick = runtime.ability_started_tick + ability.windup_ticks +
                               ability.active_ticks + ability.recovery_ticks;
    if (tick >= complete_tick) {
        actor.active_ability = 0;
        runtime.active_ability = ability_capacity;
        runtime.target = 0;
        runtime.hit_applied = false;
    }
    return true;
}

bool DeterministicCombatResolver::resolve_hit(
    std::size_t source_index,
    std::size_t target_index,
    const contracts::AbilityDefinition& ability,
    contracts::TickIndex tick
) noexcept {
    auto& source = actor_snapshots_[source_index];
    auto& target = actor_snapshots_[target_index];
    if (!target.active || source.faction == target.faction ||
        !target_in_range(source, target, ability)) {
        return emit({tick, contracts::CombatEventType::attack_missed, source.actor, target.actor, ability.id.key});
    }
    if (actor_runtime_[target_index].evade_until_tick >= tick) {
        return emit({
            tick,
            contracts::CombatEventType::hit_evaded,
            source.actor,
            target.actor,
            ability.id.key,
            0,
            contracts::feedback_evade,
        });
    }

    const auto poise_before = target.resources.poise;
    target.resources.poise = clamp_subtract(target.resources.poise, ability.poise_damage);
    const auto poise_damage = poise_before - target.resources.poise;
    const auto poise_broken = poise_before > 0 && target.resources.poise == 0;
    const auto was_guarding = target.guarding;
    auto health_damage = ability.health_damage;
    if (was_guarding) {
        if (!emit({
                tick,
                contracts::CombatEventType::hit_guarded,
                source.actor,
                target.actor,
                ability.id.key,
                poise_damage,
                ability.feedback_tags | contracts::feedback_guard,
            })) {
            return false;
        }
        if (target.resources.poise > 0) {
            return true;
        }
        target.guarding = false;
        health_damage /= 4;
        if (!emit({
                tick,
                contracts::CombatEventType::poise_broken,
                source.actor,
                target.actor,
                ability.id.key,
                poise_damage,
                contracts::feedback_poise_break,
            })) {
            return false;
        }
    }
    target.resources.health = clamp_subtract(target.resources.health, health_damage);
    if (!emit({
            tick,
            contracts::CombatEventType::hit_landed,
            source.actor,
            target.actor,
            ability.id.key,
            health_damage,
            ability.feedback_tags,
        })) {
        return false;
    }
    if (!was_guarding && poise_broken &&
        !emit({
            tick,
            contracts::CombatEventType::poise_broken,
            source.actor,
            target.actor,
            ability.id.key,
            poise_damage,
            contracts::feedback_poise_break,
        })) {
        return false;
    }
    if (target.resources.health == 0) {
        target.active = false;
        target.guarding = false;
        return emit({
            tick,
            contracts::CombatEventType::actor_defeated,
            source.actor,
            target.actor,
            ability.id.key,
            0,
            ability.feedback_tags,
        });
    }
    return true;
}

void DeterministicCombatResolver::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, current_tick_);
    for (std::size_t index = 0; index < actor_count_; ++index) {
        const auto& actor = actor_snapshots_[index];
        const auto& runtime = actor_runtime_[index];
        hash_integer(hash, actor.actor);
        hash_integer(hash, actor.archetype);
        hash_integer(hash, static_cast<std::uint8_t>(actor.faction));
        hash_integer(hash, actor.pose.x);
        hash_integer(hash, actor.pose.y);
        hash_integer(hash, actor.pose.height);
        hash_integer(hash, actor.pose.floor_layer);
        hash_integer(hash, actor.resources.health);
        hash_integer(hash, actor.resources.stamina);
        hash_integer(hash, actor.resources.poise);
        hash_integer(hash, actor.resources.lantern);
        hash_integer(hash, actor.resources.evidence);
        hash_integer(hash, actor.stance);
        hash_integer(hash, actor.active_ability);
        hash_byte(hash, actor.guarding ? 1U : 0U);
        hash_byte(hash, actor.active ? 1U : 0U);
        hash_integer(hash, runtime.target);
        hash_integer(hash, runtime.ability_started_tick);
        hash_integer(hash, runtime.evade_until_tick);
        hash_byte(hash, runtime.hit_applied ? 1U : 0U);
    }
    checksum_ = hash;
}

}  // namespace tgd::gameplay
