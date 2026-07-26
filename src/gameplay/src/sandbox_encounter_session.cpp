#include <tgd/gameplay/sandbox_encounter_session.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;
constexpr auto player_stance =
    contracts::stable_content_key("sandbox_demo_player_stance");
constexpr auto hostile_stance =
    contracts::stable_content_key("sandbox_demo_hostile_stance");

void hash_integer(std::uint64_t& hash, bool value) noexcept {
    hash ^= static_cast<std::uint8_t>(value);
    hash *= fnv_prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash ^= static_cast<std::uint8_t>(bits & 0xffU);
        hash *= fnv_prime;
        bits >>= 8U;
    }
}

[[nodiscard]] std::uint64_t distance_squared(
    contracts::GroundPoseMm left,
    contracts::GroundPoseMm right
) noexcept {
    const auto signed_delta_x =
        static_cast<std::int64_t>(left.x) - static_cast<std::int64_t>(right.x);
    const auto signed_delta_y =
        static_cast<std::int64_t>(left.y) - static_cast<std::int64_t>(right.y);
    const auto magnitude = [](const std::int64_t value) noexcept {
        return value < 0
                   ? static_cast<std::uint64_t>(-(value + 1)) + 1U
                   : static_cast<std::uint64_t>(value);
    };
    const auto delta_x = magnitude(signed_delta_x);
    const auto delta_y = magnitude(signed_delta_y);
    const auto square_x = delta_x * delta_x;
    const auto square_y = delta_y * delta_y;
    return square_y > std::numeric_limits<std::uint64_t>::max() - square_x
               ? std::numeric_limits<std::uint64_t>::max()
               : square_x + square_y;
}

[[nodiscard]] const contracts::SandboxActorDefinition* find_actor_definition(
    const contracts::SandboxDefinition& definition,
    contracts::StableContentKey actor
) noexcept {
    const auto found = std::find_if(
        definition.actors.begin(),
        definition.actors.end(),
        [actor](const auto& value) { return value.id.key == actor; }
    );
    return found == definition.actors.end() ? nullptr : &*found;
}

[[nodiscard]] bool valid_attack(SandboxEncounterAttack attack) noexcept {
    return attack == SandboxEncounterAttack::light ||
           attack == SandboxEncounterAttack::heavy;
}

}  // namespace

void SandboxEncounterSession::EventCollector::clear() noexcept {
    count_ = 0;
    overflowed_ = false;
}

void SandboxEncounterSession::EventCollector::publish(
    std::span<const contracts::CombatEvent> events
) noexcept {
    const auto available = values_.size() - count_;
    const auto copied = std::min(available, events.size());
    std::copy_n(events.begin(), copied, values_.begin() + count_);
    count_ += copied;
    overflowed_ |= copied != events.size();
}

std::span<const contracts::CombatEvent>
SandboxEncounterSession::EventCollector::events() const noexcept {
    return std::span{values_}.first(count_);
}

bool SandboxEncounterSession::EventCollector::overflowed() const noexcept {
    return overflowed_;
}

SandboxEncounterBuildError SandboxEncounterSession::initialize(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& binding,
    contracts::StableActorKey player_actor,
    contracts::GroundPoseMm player_pose
) noexcept {
    if (initialized_ || player_actor == 0 ||
        definition.waves.empty() || definition.wave_spawns.empty() ||
        definition.objectives.empty() ||
        definition.actors.size() > hostile_capacity ||
        definition.waves.size() > wave_capacity ||
        definition.wave_spawns.size() > spawn_capacity ||
        definition.objectives.size() > objective_capacity) {
        return SandboxEncounterBuildError::invalid_definition;
    }
    const auto binding_validation =
        contracts::validate_sandbox_gameplay_binding(definition, binding);
    if (!contracts::sandbox_gameplay_binding_is_valid(binding_validation)) {
        return SandboxEncounterBuildError::invalid_binding;
    }

    definition_ = &definition;
    binding_ = &binding;
    player_actor_ = player_actor;
    return build_runtime(player_pose);
}

SandboxEncounterBuildError SandboxEncounterSession::build_runtime(
    contracts::GroundPoseMm player_pose
) noexcept {
    if (definition_ == nullptr || binding_ == nullptr || player_actor_ == 0) {
        return SandboxEncounterBuildError::invalid_definition;
    }

    combat_ = DeterministicCombatResolver{};
    director_ = DeterministicEncounterDirector{};
    events_ = EventCollector{};
    actor_configs_.fill({});
    duties_.fill({});
    waves_.fill({});
    spawns_.fill({});
    objectives_.fill({});
    activated_mechanisms_.fill(0);
    actor_config_count_ = 0;
    duty_count_ = 0;
    wave_count_ = 0;
    spawn_count_ = 0;
    objective_count_ = 0;
    activated_mechanism_count_ = 0;
    pending_attack_ = SandboxEncounterAttack::invalid;
    sequence_cursor_ = 0;
    accepted_attack_count_ = 0;
    repeated_trigger_count_ = 0;
    has_activated_group_ = false;
    initialized_ = false;
    snapshot_ = {};

    constexpr contracts::CombatResources player_resources{
        160, 160, 100, 100, 80, 80, 0, 0, 0,
    };
    actor_configs_[actor_config_count_++] = {
        player_actor_,
        contracts::content_id("sandbox_demo_player"),
        contracts::CombatFaction::player,
        player_pose,
        player_resources,
        {player_stance, 0, 0, 0},
        1,
        player_stance,
        {45, 6, 3, 90, 12, 3},
        true,
    };

    for (const auto& actor_binding : binding_->actor_bindings) {
        const auto* actor =
            find_actor_definition(*definition_, actor_binding.actor_id.key);
        if (actor == nullptr || actor_config_count_ >= actor_configs_.size() ||
            duty_count_ >= duties_.size()) {
            return SandboxEncounterBuildError::invalid_binding;
        }
        const contracts::CombatResources resources{
            actor_binding.max_health,
            actor_binding.max_health,
            100,
            100,
            45,
            45,
            0,
            0,
            0,
        };
        actor_configs_[actor_config_count_++] = {
            actor_binding.actor_id.key,
            actor_binding.profile_id,
            actor_binding.faction,
            actor->pose,
            resources,
            {hostile_stance, 0, 0, 0},
            1,
            hostile_stance,
            {60, 8, 2, 120, 12, 2},
            false,
        };
        duties_[duty_count_++] = {
            actor_binding.actor_id.key,
            actor_binding.duty,
        };
    }
    if (actor_config_count_ < 2 || duty_count_ == 0) {
        return SandboxEncounterBuildError::invalid_binding;
    }

    abilities_ = {{
        {
            contracts::content_id("sandbox_demo_player_light"),
            contracts::CombatCommandType::light_attack,
            player_stance,
            8,
            4,
            2,
            10,
            1'450,
            600,
            18,
            14,
            contracts::feedback_light,
        },
        {
            contracts::content_id("sandbox_demo_player_heavy"),
            contracts::CombatCommandType::heavy_attack,
            player_stance,
            18,
            8,
            2,
            16,
            1'650,
            650,
            30,
            30,
            contracts::feedback_heavy,
        },
        {
            contracts::content_id("sandbox_demo_hostile_light"),
            contracts::CombatCommandType::light_attack,
            hostile_stance,
            6,
            10,
            2,
            22,
            1'050,
            600,
            7,
            8,
            contracts::feedback_light,
        },
        {
            contracts::content_id("sandbox_demo_hostile_heavy"),
            contracts::CombatCommandType::heavy_attack,
            hostile_stance,
            14,
            18,
            3,
            32,
            1'150,
            650,
            13,
            18,
            contracts::feedback_heavy,
        },
    }};

    const auto combat_error = combat_.initialize(
        std::span{actor_configs_}.first(actor_config_count_),
        abilities_
    );
    if (combat_error != CombatError::none ||
        combat_.start() != CombatError::none) {
        return SandboxEncounterBuildError::combat_initialize_failed;
    }

    contracts::EncounterDirectorDefinition director_definition{
        player_actor_,
        6'000,
        12'000,
        1'200,
        900,
        15,
        28,
        static_cast<std::uint8_t>(std::min<std::size_t>(2, duty_count_)),
        60,
        std::span{duties_}.first(duty_count_),
    };
    if (director_.initialize(
            director_definition,
            std::span{actor_configs_}.first(actor_config_count_),
            abilities_
        ) != EncounterDirectorError::none) {
        return SandboxEncounterBuildError::director_initialize_failed;
    }

    for (const auto& wave : definition_->waves) {
        waves_[wave_count_++] = {
            wave.id.key,
            SandboxWaveRuntimeState::waiting,
            0,
        };
    }
    for (const auto& spawn : definition_->wave_spawns) {
        spawns_[spawn_count_++] = {
            spawn.wave_id.key,
            spawn.actor_id.key,
            spawn.delay_ticks,
            spawn.spawn_order,
            false,
        };
    }
    for (const auto& objective : definition_->objectives) {
        objectives_[objective_count_++] = {
            objective.id.key,
            objective.predecessor_objective_id.key == 0
                ? SandboxObjectiveRuntimeState::active
                : SandboxObjectiveRuntimeState::locked,
        };
    }

    initialized_ = true;
    static_cast<void>(evaluate_graph());
    if (!activate_due_spawns()) {
        initialized_ = false;
        return SandboxEncounterBuildError::combat_initialize_failed;
    }
    refresh_snapshot();
    return SandboxEncounterBuildError::none;
}

SandboxEncounterEventDisposition
SandboxEncounterSession::notify_mechanism_activated(
    contracts::StableContentKey mechanism
) noexcept {
    if (!initialized_ || mechanism == 0) {
        return SandboxEncounterEventDisposition::invalid_state;
    }
    const bool known = std::any_of(
        definition_->mechanisms.begin(),
        definition_->mechanisms.end(),
        [mechanism](const auto& value) { return value.id.key == mechanism; }
    );
    if (!known) {
        return SandboxEncounterEventDisposition::unknown_target;
    }
    if (std::find(
            activated_mechanisms_.begin(),
            activated_mechanisms_.begin() +
                static_cast<std::ptrdiff_t>(activated_mechanism_count_),
            mechanism
        ) != activated_mechanisms_.begin() +
                static_cast<std::ptrdiff_t>(activated_mechanism_count_)) {
        ++repeated_trigger_count_;
        refresh_snapshot();
        return SandboxEncounterEventDisposition::repeated;
    }
    if (activated_mechanism_count_ >= activated_mechanisms_.size()) {
        return SandboxEncounterEventDisposition::invalid_state;
    }
    auto candidate = *this;
    candidate.activated_mechanisms_[candidate.activated_mechanism_count_++] =
        mechanism;
    static_cast<void>(candidate.evaluate_graph());
    if (!candidate.activate_due_spawns()) {
        return SandboxEncounterEventDisposition::invalid_state;
    }
    candidate.refresh_snapshot();
    *this = std::move(candidate);
    return SandboxEncounterEventDisposition::applied;
}

SandboxEncounterAttackDisposition SandboxEncounterSession::queue_player_attack(
    SandboxEncounterAttack attack
) noexcept {
    if (!initialized_ || !valid_attack(attack)) {
        return SandboxEncounterAttackDisposition::invalid_state;
    }
    if (snapshot_.player_defeated) {
        return SandboxEncounterAttackDisposition::player_defeated;
    }
    if (pending_attack_ != SandboxEncounterAttack::invalid) {
        return SandboxEncounterAttackDisposition::already_queued;
    }
    if (nearest_active_hostile() == 0) {
        return SandboxEncounterAttackDisposition::no_active_target;
    }
    pending_attack_ = attack;
    return SandboxEncounterAttackDisposition::queued;
}

SandboxEncounterStepResult SandboxEncounterSession::advance_one_tick(
    contracts::GroundPoseMm player_pose
) noexcept {
    auto candidate = *this;
    const auto result = candidate.advance_one_tick_candidate(player_pose);
    switch (result.disposition) {
        case SandboxEncounterStepDisposition::advanced:
        case SandboxEncounterStepDisposition::player_defeated:
        case SandboxEncounterStepDisposition::terminal_completed:
            *this = std::move(candidate);
            return {result.disposition, snapshot_};
        case SandboxEncounterStepDisposition::invalid_state:
        case SandboxEncounterStepDisposition::combat_rejected:
        case SandboxEncounterStepDisposition::director_rejected:
        case SandboxEncounterStepDisposition::activation_rejected:
        case SandboxEncounterStepDisposition::invalid:
            return {result.disposition, snapshot_};
    }
    return {SandboxEncounterStepDisposition::invalid_state, snapshot_};
}

SandboxEncounterStepResult
SandboxEncounterSession::advance_one_tick_candidate(
    contracts::GroundPoseMm player_pose
) noexcept {
    const auto fail = [&](SandboxEncounterStepDisposition disposition) noexcept {
        return SandboxEncounterStepResult{disposition, snapshot_};
    };
    if (!initialized_ ||
        combat_.current_tick() == std::numeric_limits<contracts::TickIndex>::max()) {
        return fail(SandboxEncounterStepDisposition::invalid_state);
    }
    if (!activate_due_spawns()) {
        return fail(SandboxEncounterStepDisposition::activation_rejected);
    }

    const auto next_tick = combat_.current_tick() + 1U;
    std::array<contracts::CombatPoseUpdate, combat_actor_capacity> poses{};
    std::size_t pose_count = 0;
    poses[pose_count++] = {next_tick, player_actor_, player_pose};

    const auto plan = director_.plan_tick(
        next_tick,
        combat_.actors(),
        sequence_cursor_ + 1U
    );
    if (plan.error != EncounterDirectorError::none) {
        return fail(SandboxEncounterStepDisposition::director_rejected);
    }
    sequence_cursor_ += plan.batch.command_count;
    for (const auto& pose : plan.batch.poses()) {
        if (pose_count >= poses.size()) {
            return fail(SandboxEncounterStepDisposition::director_rejected);
        }
        poses[pose_count++] = pose;
    }
    if (combat_.synchronize_poses(std::span{poses}.first(pose_count)) !=
        CombatError::none) {
        return fail(SandboxEncounterStepDisposition::combat_rejected);
    }

    if (pending_attack_ != SandboxEncounterAttack::invalid) {
        const auto target = nearest_active_hostile();
        if (target != 0) {
            const contracts::CombatCommand command{
                next_tick,
                player_actor_,
                ++sequence_cursor_,
                pending_attack_ == SandboxEncounterAttack::heavy
                    ? contracts::CombatCommandType::heavy_attack
                    : contracts::CombatCommandType::light_attack,
                target,
                0,
            };
            if (combat_.submit(std::span{&command, 1}) != CombatError::none) {
                return fail(SandboxEncounterStepDisposition::combat_rejected);
            }
            ++accepted_attack_count_;
        }
        pending_attack_ = SandboxEncounterAttack::invalid;
    }
    if (!plan.batch.command_view().empty() &&
        combat_.submit(plan.batch.command_view()) != CombatError::none) {
        return fail(SandboxEncounterStepDisposition::combat_rejected);
    }

    events_.clear();
    if (combat_.advance_one_tick(events_) != CombatError::none ||
        events_.overflowed()) {
        return fail(SandboxEncounterStepDisposition::combat_rejected);
    }
    if (!update_wave_completion()) {
        return fail(SandboxEncounterStepDisposition::activation_rejected);
    }
    refresh_snapshot();
    if (snapshot_.terminal_completed) {
        return {
            SandboxEncounterStepDisposition::terminal_completed,
            snapshot_,
        };
    }
    if (snapshot_.player_defeated) {
        return {
            SandboxEncounterStepDisposition::player_defeated,
            snapshot_,
        };
    }
    return {SandboxEncounterStepDisposition::advanced, snapshot_};
}

SandboxEncounterBuildError SandboxEncounterSession::restart(
    contracts::GroundPoseMm player_pose
) noexcept {
    if (!initialized_ || definition_ == nullptr || binding_ == nullptr) {
        return SandboxEncounterBuildError::invalid_definition;
    }
    auto candidate = *this;
    const auto result = candidate.build_runtime(player_pose);
    if (result == SandboxEncounterBuildError::none) {
        *this = std::move(candidate);
    }
    return result;
}

SandboxEncounterSnapshot SandboxEncounterSession::snapshot() const noexcept {
    return snapshot_;
}

std::span<const contracts::CombatActorSnapshot>
SandboxEncounterSession::combat_actors() const noexcept {
    return initialized_ ? combat_.actors()
                        : std::span<const contracts::CombatActorSnapshot>{};
}

const contracts::SandboxActorGameplayBinding*
SandboxEncounterSession::actor_binding(
    contracts::StableActorKey actor
) const noexcept {
    if (binding_ == nullptr || actor == 0) {
        return nullptr;
    }
    const auto found = std::find_if(
        binding_->actor_bindings.begin(),
        binding_->actor_bindings.end(),
        [actor](const auto& value) { return value.actor_id.key == actor; }
    );
    return found == binding_->actor_bindings.end() ? nullptr : &*found;
}

SandboxWaveRuntimeState SandboxEncounterSession::wave_state(
    contracts::StableContentKey wave
) const noexcept {
    const auto found = std::find_if(
        waves_.begin(),
        waves_.begin() + static_cast<std::ptrdiff_t>(wave_count_),
        [wave](const auto& value) { return value.id == wave; }
    );
    return found == waves_.begin() + static_cast<std::ptrdiff_t>(wave_count_)
               ? SandboxWaveRuntimeState::invalid
               : found->state;
}

SandboxObjectiveRuntimeState SandboxEncounterSession::objective_state(
    contracts::StableContentKey objective
) const noexcept {
    const auto found = std::find_if(
        objectives_.begin(),
        objectives_.begin() + static_cast<std::ptrdiff_t>(objective_count_),
        [objective](const auto& value) { return value.id == objective; }
    );
    return found ==
            objectives_.begin() + static_cast<std::ptrdiff_t>(objective_count_)
               ? SandboxObjectiveRuntimeState::invalid
               : found->state;
}

bool SandboxEncounterSession::evaluate_graph() noexcept {
    bool changed = false;
    bool pass_changed = true;
    while (pass_changed) {
        pass_changed = false;
        for (std::size_t index = 0; index < objective_count_; ++index) {
            auto& runtime = objectives_[index];
            const auto& definition = definition_->objectives[index];
            if (runtime.state == SandboxObjectiveRuntimeState::locked) {
                const auto predecessor = objective_state(
                    definition.predecessor_objective_id.key
                );
                if (definition.predecessor_objective_id.key == 0 ||
                    predecessor == SandboxObjectiveRuntimeState::completed) {
                    runtime.state = SandboxObjectiveRuntimeState::active;
                    changed = true;
                    pass_changed = true;
                }
            }
            if (runtime.state == SandboxObjectiveRuntimeState::active &&
                completion_satisfied(definition.completion)) {
                runtime.state = SandboxObjectiveRuntimeState::completed;
                changed = true;
                pass_changed = true;
            }
        }
        for (std::size_t index = 0; index < wave_count_; ++index) {
            auto& runtime = waves_[index];
            const auto& definition = definition_->waves[index];
            if (runtime.state != SandboxWaveRuntimeState::waiting) {
                continue;
            }
            const auto predecessor = wave_state(
                definition.predecessor_wave_id.key
            );
            const bool predecessor_ready =
                definition.predecessor_wave_id.key == 0 ||
                predecessor == SandboxWaveRuntimeState::completed;
            if (predecessor_ready && trigger_satisfied(definition.trigger)) {
                runtime.state = SandboxWaveRuntimeState::active;
                runtime.activated_tick = combat_.current_tick();
                changed = true;
                pass_changed = true;
            }
        }
    }
    return changed;
}

bool SandboxEncounterSession::trigger_satisfied(
    const contracts::SandboxTriggerDefinition& trigger
) const noexcept {
    switch (trigger.kind) {
        case contracts::SandboxTriggerKind::session_started:
            return trigger.target_id.key == 0;
        case contracts::SandboxTriggerKind::mechanism_activated:
            return std::find(
                       activated_mechanisms_.begin(),
                       activated_mechanisms_.begin() +
                           static_cast<std::ptrdiff_t>(
                               activated_mechanism_count_
                           ),
                       trigger.target_id.key
                   ) != activated_mechanisms_.begin() +
                           static_cast<std::ptrdiff_t>(
                               activated_mechanism_count_
                           );
        case contracts::SandboxTriggerKind::objective_completed:
            return objective_state(trigger.target_id.key) ==
                   SandboxObjectiveRuntimeState::completed;
        case contracts::SandboxTriggerKind::wave_completed:
            return wave_state(trigger.target_id.key) ==
                   SandboxWaveRuntimeState::completed;
        case contracts::SandboxTriggerKind::interaction_completed:
            return false;
    }
    return false;
}

bool SandboxEncounterSession::completion_satisfied(
    const contracts::SandboxObjectiveCompletionDefinition& completion
) const noexcept {
    switch (completion.kind) {
        case contracts::SandboxObjectiveCompletionKind::mechanism_activated:
            return std::find(
                       activated_mechanisms_.begin(),
                       activated_mechanisms_.begin() +
                           static_cast<std::ptrdiff_t>(
                               activated_mechanism_count_
                           ),
                       completion.target_id.key
                   ) != activated_mechanisms_.begin() +
                           static_cast<std::ptrdiff_t>(
                               activated_mechanism_count_
                           );
        case contracts::SandboxObjectiveCompletionKind::wave_completed:
            return wave_state(completion.target_id.key) ==
                   SandboxWaveRuntimeState::completed;
        case contracts::SandboxObjectiveCompletionKind::interaction_completed:
            return false;
    }
    return false;
}

bool SandboxEncounterSession::activate_due_spawns() noexcept {
    if (!initialized_) {
        return false;
    }
    for (std::size_t wave_index = 0; wave_index < wave_count_; ++wave_index) {
        const auto& wave = waves_[wave_index];
        if (wave.state != SandboxWaveRuntimeState::active) {
            continue;
        }
        std::array<contracts::EncounterActorPlacementDefinition, spawn_capacity>
            placements{};
        std::array<std::size_t, spawn_capacity> spawn_indices{};
        std::size_t count = 0;
        for (std::size_t spawn_index = 0; spawn_index < spawn_count_;
             ++spawn_index) {
            const auto& spawn = spawns_[spawn_index];
            if (spawn.wave != wave.id || spawn.activated ||
                spawn.delay_ticks >
                    combat_.current_tick() - wave.activated_tick) {
                continue;
            }
            const auto* actor =
                find_actor_definition(*definition_, spawn.actor);
            if (actor == nullptr || count >= placements.size()) {
                return false;
            }
            placements[count] = {
                spawn.actor,
                actor->pose,
                static_cast<std::uint8_t>(
                    spawn.spawn_order %
                    contracts::encounter_formation_slot_capacity
                ),
            };
            spawn_indices[count] = spawn_index;
            ++count;
        }
        if (count == 0) {
            continue;
        }
        const auto mode = has_activated_group_
                              ? contracts::EncounterActivationMode::reinforce
                              : contracts::EncounterActivationMode::replace;
        if (!activate_spawn_group(std::span{placements}.first(count), mode)) {
            return false;
        }
        has_activated_group_ = true;
        for (std::size_t index = 0; index < count; ++index) {
            spawns_[spawn_indices[index]].activated = true;
        }
    }
    return true;
}

bool SandboxEncounterSession::activate_spawn_group(
    std::span<const contracts::EncounterActorPlacementDefinition> placements,
    contracts::EncounterActivationMode mode
) noexcept {
    if (placements.empty()) {
        return false;
    }
    const contracts::EncounterActivationCommand command{
        combat_.current_tick(),
        player_actor_,
        ++sequence_cursor_,
        mode,
    };
    auto director_candidate = director_;
    auto combat_candidate = combat_;
    events_.clear();
    if (director_candidate.activate_group(
            command,
            placements,
            combat_candidate.actors()
        ) != EncounterDirectorError::none ||
        combat_candidate.activate_group(command, placements, events_) !=
            CombatError::none ||
        events_.overflowed()) {
        return false;
    }
    director_ = director_candidate;
    combat_ = combat_candidate;
    return true;
}

bool SandboxEncounterSession::update_wave_completion() noexcept {
    bool changed = false;
    for (std::size_t wave_index = 0; wave_index < wave_count_; ++wave_index) {
        auto& wave = waves_[wave_index];
        if (wave.state != SandboxWaveRuntimeState::active) {
            continue;
        }
        bool has_spawn = false;
        bool complete = true;
        for (std::size_t spawn_index = 0; spawn_index < spawn_count_;
             ++spawn_index) {
            const auto& spawn = spawns_[spawn_index];
            if (spawn.wave != wave.id) {
                continue;
            }
            has_spawn = true;
            const auto* actor = find_combat_actor(spawn.actor);
            complete &= spawn.activated && actor != nullptr && actor->defeated;
        }
        if (has_spawn && complete) {
            wave.state = SandboxWaveRuntimeState::completed;
            changed = true;
        }
    }
    if (changed) {
        static_cast<void>(evaluate_graph());
        return activate_due_spawns();
    }
    return true;
}

const contracts::CombatActorSnapshot*
SandboxEncounterSession::find_combat_actor(
    contracts::StableActorKey actor
) const noexcept {
    const auto values = combat_.actors();
    const auto found = std::find_if(
        values.begin(),
        values.end(),
        [actor](const auto& value) { return value.actor == actor; }
    );
    return found == values.end() ? nullptr : &*found;
}

contracts::StableActorKey
SandboxEncounterSession::nearest_active_hostile() const noexcept {
    const auto* player = find_combat_actor(player_actor_);
    if (player == nullptr || !player->active || player->defeated) {
        return 0;
    }
    contracts::StableActorKey selected = 0;
    auto selected_distance = std::numeric_limits<std::uint64_t>::max();
    for (const auto& actor : combat_.actors()) {
        if (actor.faction != contracts::CombatFaction::hostile ||
            !actor.active || actor.defeated ||
            actor.pose.floor_layer != player->pose.floor_layer) {
            continue;
        }
        const auto distance = distance_squared(actor.pose, player->pose);
        if (selected == 0 ||
            std::tuple{distance, actor.actor} <
            std::tuple{selected_distance, selected}) {
            selected = actor.actor;
            selected_distance = distance;
        }
    }
    return selected;
}

void SandboxEncounterSession::refresh_snapshot() noexcept {
    SandboxEncounterSnapshot next{};
    next.initialized = initialized_;
    next.tick = initialized_ ? combat_.current_tick() : 0;
    next.player_actor = player_actor_;
    next.accepted_attack_count = accepted_attack_count_;
    next.repeated_trigger_count = repeated_trigger_count_;

    if (const auto* player = find_combat_actor(player_actor_); player != nullptr) {
        next.player_health = player->resources.health;
        next.player_health_max = player->resources.health_max;
        next.player_defeated = player->defeated;
    }
    for (const auto& actor : combat_.actors()) {
        if (actor.faction != contracts::CombatFaction::hostile) {
            continue;
        }
        next.active_hostile_count +=
            static_cast<std::uint16_t>(actor.active && !actor.defeated);
        next.defeated_hostile_count +=
            static_cast<std::uint16_t>(actor.defeated);
    }
    for (const auto& wave :
         std::span{waves_}.first(wave_count_)) {
        if (wave.state == SandboxWaveRuntimeState::active &&
            next.active_wave == 0) {
            next.active_wave = wave.id;
        }
        next.completed_wave_count += static_cast<std::uint16_t>(
            wave.state == SandboxWaveRuntimeState::completed
        );
    }
    for (const auto& objective :
         std::span{objectives_}.first(objective_count_)) {
        next.completed_objective_count += static_cast<std::uint16_t>(
            objective.state == SandboxObjectiveRuntimeState::completed
        );
    }
    next.terminal_completed =
        objective_state(definition_->completion_objective_id.key) ==
        SandboxObjectiveRuntimeState::completed;

    auto hash = fnv_offset;
    hash_integer(hash, next.tick);
    hash_integer(hash, next.player_actor);
    hash_integer(hash, next.player_health);
    hash_integer(hash, next.player_health_max);
    hash_integer(hash, next.player_defeated);
    hash_integer(hash, next.active_wave);
    hash_integer(hash, next.active_hostile_count);
    hash_integer(hash, next.defeated_hostile_count);
    hash_integer(hash, next.completed_wave_count);
    hash_integer(hash, next.completed_objective_count);
    hash_integer(hash, next.terminal_completed);
    hash_integer(hash, next.accepted_attack_count);
    hash_integer(hash, next.repeated_trigger_count);
    hash_integer(hash, combat_.checksum());
    hash_integer(hash, director_.checksum());
    next.checksum = hash;
    snapshot_ = next;
}

}  // namespace tgd::gameplay
