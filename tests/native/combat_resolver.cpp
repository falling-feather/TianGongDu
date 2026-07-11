#include <tgd/contracts/combat_types.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/gameplay/combat_resolver.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using tgd::contracts::CombatCommand;
using tgd::contracts::CombatCommandType;
using tgd::contracts::CombatEvent;
using tgd::contracts::CombatEventType;
using tgd::gameplay::CombatError;
using tgd::gameplay::DeterministicCombatResolver;

constexpr auto eavesguard = tgd::contracts::stable_content_key("stance_eavesguard");
constexpr auto flower_turn = tgd::contracts::stable_content_key("stance_flower_turn");

class CollectingSink final : public tgd::gameplay::ICombatEventSink {
  public:
    void publish(std::span<const CombatEvent> events) noexcept override {
        values.insert(values.end(), events.begin(), events.end());
    }

    void clear() { values.clear(); }

    [[nodiscard]] bool contains(CombatEventType type) const {
        for (const auto& event : values) {
            if (event.type == type) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::size_t count(CombatEventType type) const {
        std::size_t result = 0;
        for (const auto& event : values) {
            result += event.type == type ? 1U : 0U;
        }
        return result;
    }

    [[nodiscard]] std::size_t count_for(
        CombatEventType type,
        tgd::contracts::StableActorKey actor
    ) const {
        std::size_t result = 0;
        for (const auto& event : values) {
            result += event.type == type && event.source == actor ? 1U : 0U;
        }
        return result;
    }

    std::vector<CombatEvent> values;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "combat resolver failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] std::array<tgd::contracts::CombatActorConfig, 2> actors() {
    const tgd::contracts::CombatResources player_resources{120, 120, 100, 100, 80, 80, 30, 30, 0};
    const tgd::contracts::CombatResources enemy_resources{90, 90, 80, 80, 40, 40, 0, 0, 0};
    return {{
        {1,
         tgd::contracts::content_id("actor_f1_player"),
         tgd::contracts::CombatFaction::player,
         {0, 0, 0, 0},
         player_resources,
         {eavesguard, flower_turn, 0},
         2,
         eavesguard,
         {60, 6, 2, 60, 6, 2}},
        {2,
         tgd::contracts::content_id("jn_enemy_leaking_umbrella_doll"),
         tgd::contracts::CombatFaction::hostile,
         {900, 0, 0, 0},
         enemy_resources,
         {eavesguard, 0, 0},
         1,
         eavesguard,
         {60, 6, 2, 60, 6, 2}},
    }};
}

[[nodiscard]] std::array<tgd::contracts::AbilityDefinition, 5> abilities() {
    return {{
        {tgd::contracts::content_id("ability_eavesguard_light"),
         CombatCommandType::light_attack,
         eavesguard,
         8,
         2,
         1,
         3,
         1'250,
         500,
         18,
         18,
         tgd::contracts::feedback_light},
        {tgd::contracts::content_id("ability_eavesguard_heavy"),
         CombatCommandType::heavy_attack,
         eavesguard,
         18,
         4,
         1,
         6,
         1'350,
         500,
         28,
         45,
         tgd::contracts::feedback_heavy},
        {tgd::contracts::content_id("ability_flower_light"),
         CombatCommandType::light_attack,
         flower_turn,
         10,
         1,
         2,
         4,
         1'450,
         650,
         20,
         14,
         tgd::contracts::feedback_light},
        {tgd::contracts::content_id("ability_flower_heavy"),
         CombatCommandType::heavy_attack,
         flower_turn,
         22,
         5,
         2,
         8,
         1'650,
         650,
         36,
         34,
         tgd::contracts::feedback_heavy},
        {tgd::contracts::content_id("ability_common_evade"),
         CombatCommandType::evade,
         0,
         14,
         0,
         5,
         5,
         0,
         0,
         0,
         0,
         tgd::contracts::feedback_evade},
    }};
}

bool test_hit_window_guard_break_and_stance() {
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    const auto actor_configs = actors();
    const auto ability_configs = abilities();
    bool ok = resolver.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= resolver.start() == CombatError::none;

    const std::array first_commands{
        CombatCommand{1, 1, 1, CombatCommandType::switch_stance, 0, flower_turn},
        CombatCommand{1, 2, 1, CombatCommandType::guard_started, 0, 0},
        CombatCommand{2, 1, 2, CombatCommandType::heavy_attack, 2, 0},
    };
    ok &= resolver.submit(first_commands) == CombatError::none;
    for (int tick = 0; tick < 7; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(sink.contains(CombatEventType::stance_changed), "stance change is a command event");
    ok &= expect(sink.contains(CombatEventType::hit_guarded), "guard resolves before health damage");
    ok &= expect(!sink.contains(CombatEventType::poise_broken), "first guarded hit leaves poise");
    const auto enemy_after_guard = resolver.actors()[1];
    ok &= expect(enemy_after_guard.resources.health == 90, "successful guard prevents health damage");
    ok &= expect(enemy_after_guard.resources.poise == 6, "guard consumes independent poise");

    const std::array release_guard{
        CombatCommand{8, 2, 2, CombatCommandType::guard_ended, 0, 0},
    };
    ok &= resolver.submit(release_guard) == CombatError::none;
    for (int tick = 0; tick < 10; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }

    const std::array second_commands{
        CombatCommand{18, 2, 3, CombatCommandType::guard_started, 0, 0},
        CombatCommand{18, 1, 3, CombatCommandType::heavy_attack, 2, 0},
    };
    ok &= resolver.submit(second_commands) == CombatError::none;
    for (int tick = 0; tick < 6; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(sink.contains(CombatEventType::poise_broken), "second guarded hit breaks poise");
    ok &= expect(sink.count(CombatEventType::poise_broken) == 1, "guard break emits once");
    ok &= expect(resolver.actors()[1].resources.health == 81, "guard break leaks quarter health damage");
    return ok;
}

bool test_unguarded_poise_break() {
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    const auto actor_configs = actors();
    const auto ability_configs = abilities();
    bool ok = resolver.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= resolver.start() == CombatError::none;
    const std::array commands{
        CombatCommand{1, 1, 1, CombatCommandType::heavy_attack, 2, 0},
    };
    ok &= resolver.submit(commands) == CombatError::none;
    for (int tick = 0; tick < 5; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(sink.count(CombatEventType::poise_broken) == 1, "unguarded poise break emits once");
    ok &= expect(resolver.actors()[1].resources.health == 62, "unguarded heavy applies full health damage");
    ok &= expect(resolver.actors()[1].resources.poise == 0, "unguarded heavy consumes independent poise");
    return ok;
}

bool test_evade_window_boundaries() {
    DeterministicCombatResolver inside;
    CollectingSink inside_sink;
    const auto actor_configs = actors();
    auto ability_configs = abilities();
    bool ok = inside.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= inside.start() == CombatError::none;
    const std::array commands{
        CombatCommand{1, 2, 1, CombatCommandType::evade, 0, 0},
        CombatCommand{1, 1, 1, CombatCommandType::light_attack, 2, 0},
    };
    ok &= inside.submit(commands) == CombatError::none;
    for (int tick = 0; tick < 3; ++tick) {
        ok &= inside.advance_one_tick(inside_sink) == CombatError::none;
    }
    ok &= expect(inside_sink.contains(CombatEventType::hit_evaded), "ticks inside evade window avoid the hit");
    ok &= expect(inside.actors()[1].resources.health == 90, "evade prevents health damage");

    DeterministicCombatResolver boundary;
    CollectingSink boundary_sink;
    ability_configs[0].windup_ticks = 5;
    ok &= boundary.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= boundary.start() == CombatError::none;
    ok &= boundary.submit(commands) == CombatError::none;
    for (int tick = 0; tick < 6; ++tick) {
        ok &= boundary.advance_one_tick(boundary_sink) == CombatError::none;
    }
    ok &= expect(!boundary_sink.contains(CombatEventType::hit_evaded), "first recovery tick is not invulnerable");
    ok &= expect(boundary_sink.contains(CombatEventType::hit_landed), "hit lands after evade active ticks end");
    return ok;
}

bool test_floor_miss_and_determinism() {
    DeterministicCombatResolver left;
    DeterministicCombatResolver right;
    CollectingSink left_sink;
    CollectingSink right_sink;
    auto actor_configs = actors();
    actor_configs[1].initial_pose.floor_layer = 1;
    const auto ability_configs = abilities();
    bool ok = left.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= right.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= left.start() == CombatError::none;
    ok &= right.start() == CombatError::none;
    const std::array commands{
        CombatCommand{1, 2, 1, CombatCommandType::evade, 0, 0},
        CombatCommand{1, 1, 1, CombatCommandType::light_attack, 2, 0},
    };
    ok &= left.submit(commands) == CombatError::none;
    ok &= right.submit(commands) == CombatError::none;
    const std::array duplicate{
        CombatCommand{2, 1, 9, CombatCommandType::light_attack, 2, 0},
        CombatCommand{2, 1, 9, CombatCommandType::light_attack, 2, 0},
    };
    ok &= expect(
        left.submit(duplicate) == CombatError::duplicate_command_key,
        "duplicate combat ordering keys fail atomically"
    );
    for (int tick = 0; tick < 8; ++tick) {
        ok &= left.advance_one_tick(left_sink) == CombatError::none;
        ok &= right.advance_one_tick(right_sink) == CombatError::none;
    }
    ok &= expect(left_sink.contains(CombatEventType::attack_missed), "floor layers fail target query");
    ok &= expect(
        left.checksum() == right.checksum() && left.actors()[0].resources == right.actors()[0].resources,
        "same combat commands produce the same checksum"
    );
    ok &= expect(left.pause() == CombatError::none, "combat pauses explicitly");
    ok &= expect(left.resume() == CombatError::none, "combat resumes explicitly");
    return ok;
}

bool test_authoritative_pose_sync() {
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    auto actor_configs = actors();
    actor_configs[1].initial_pose.x = 4'000;
    const auto ability_configs = abilities();
    bool ok = resolver.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= resolver.start() == CombatError::none;

    const std::array wrong_tick{
        tgd::contracts::CombatPoseUpdate{2, 2, {900, 0, 0, 0}},
    };
    ok &= expect(
        resolver.synchronize_poses(wrong_tick) == CombatError::pose_targets_wrong_tick,
        "pose sync targets exactly the next simulation tick"
    );
    const std::array atomic_failure{
        tgd::contracts::CombatPoseUpdate{1, 2, {900, 0, 0, 0}},
        tgd::contracts::CombatPoseUpdate{1, 999, {0, 0, 0, 0}},
    };
    ok &= expect(
        resolver.synchronize_poses(atomic_failure) == CombatError::invalid_pose_update,
        "unknown actor rejects the whole pose batch"
    );
    const std::array valid_pose{
        tgd::contracts::CombatPoseUpdate{1, 2, {900, 0, 0, 0}},
    };
    ok &= resolver.synchronize_poses(valid_pose) == CombatError::none;
    ok &= expect(
        resolver.synchronize_poses(valid_pose) == CombatError::duplicate_pose_update,
        "one actor has one authoritative pose per tick"
    );
    const std::array attack{
        CombatCommand{1, 1, 1, CombatCommandType::light_attack, 2, 0},
    };
    ok &= resolver.submit(attack) == CombatError::none;
    for (int tick = 0; tick < 3; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(sink.contains(CombatEventType::hit_landed), "combat reads the movement-owned pose");
    ok &= expect(resolver.actors()[1].pose.x == 900, "pose sync is committed with its tick");
    return ok;
}

bool test_delayed_resource_recovery() {
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    auto actor_configs = actors();
    actor_configs[0].recovery = {3, 2, 3, 4, 2, 5};
    actor_configs[1].recovery = {3, 2, 3, 4, 2, 5};
    const auto ability_configs = abilities();
    bool ok = resolver.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= resolver.start() == CombatError::none;
    const std::array commands{
        CombatCommand{1, 1, 1, CombatCommandType::light_attack, 2, 0},
        CombatCommand{1, 2, 1, CombatCommandType::light_attack, 1, 0},
    };
    ok &= resolver.submit(commands) == CombatError::none;
    for (int tick = 0; tick < 6; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(
        sink.count_for(CombatEventType::stamina_recovered, 1) == 0,
        "stamina does not recover during the active ability"
    );
    ok &= expect(
        sink.count_for(CombatEventType::poise_recovered, 1) == 0,
        "poise waits for its configured recovery window"
    );
    for (int tick = 6; tick < 13; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    const auto player = resolver.actors()[0];
    ok &= expect(player.resources.stamina == 100, "stamina recovers to its independent maximum");
    ok &= expect(player.resources.poise == 80, "poise recovers to its independent maximum");
    ok &= expect(
        sink.count_for(CombatEventType::stamina_recovered, 1) == 3,
        "stamina recovery emits one event per applied interval"
    );
    ok &= expect(
        sink.count_for(CombatEventType::poise_recovered, 1) == 4,
        "poise recovery emits one event per applied interval"
    );

    auto invalid_actors = actors();
    invalid_actors[0].recovery.stamina_interval_ticks = 0;
    DeterministicCombatResolver invalid;
    ok &= expect(
        invalid.initialize(invalid_actors, ability_configs) == CombatError::invalid_config,
        "zero recovery intervals fail configuration closed"
    );
    return ok;
}

bool test_defeat_retry_restores_initial_encounter() {
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    auto actor_configs = actors();
    actor_configs[0].initial_resources.health = 20;
    actor_configs[0].initial_resources.health_max = 20;
    const auto ability_configs = abilities();
    bool ok = resolver.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= resolver.start() == CombatError::none;
    const std::array commands{
        CombatCommand{1, 2, 1, CombatCommandType::heavy_attack, 1, 0},
        CombatCommand{10, 1, 2, CombatCommandType::light_attack, 2, 0},
    };
    ok &= resolver.submit(commands) == CombatError::none;
    for (int tick = 0; tick < 5; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(!resolver.actors()[0].active, "zero health enters the defeated state");
    ok &= expect(sink.contains(CombatEventType::actor_defeated), "defeat emits an event");

    const tgd::contracts::SafePointRetryCommand zero_sequence{5, 1, 0};
    ok &= expect(
        resolver.retry_from_initial(zero_sequence, sink) == CombatError::stale_retry_sequence,
        "retry requires a non-zero monotonic sequence"
    );
    const tgd::contracts::SafePointRetryCommand retry{5, 1, 1};
    ok &= expect(
        resolver.retry_from_initial(retry, sink) == CombatError::none,
        "defeated player can restart the encounter"
    );
    ok &= expect(
        resolver.current_tick() == 5 && resolver.actors()[0].active &&
            resolver.actors()[0].resources == actor_configs[0].initial_resources &&
            resolver.actors()[1].resources == actor_configs[1].initial_resources,
        "retry preserves the tick and restores every actor resource"
    );
    ok &= expect(
        sink.contains(CombatEventType::encounter_restarted),
        "retry emits a presentation-safe restart event"
    );
    ok &= expect(
        resolver.retry_from_initial({5, 1, 2}, sink) == CombatError::retry_not_allowed,
        "active player cannot restart repeatedly"
    );
    for (int tick = 5; tick < 10; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(
        resolver.actors()[0].active_ability == 0,
        "retry clears future combat commands from the defeated attempt"
    );
    return ok;
}

bool test_defeat_cancels_pending_ability() {
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    auto actor_configs = actors();
    actor_configs[1].initial_resources.health = 18;
    actor_configs[1].initial_resources.health_max = 18;
    const auto ability_configs = abilities();
    bool ok = resolver.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= resolver.start() == CombatError::none;
    const std::array commands{
        CombatCommand{1, 1, 1, CombatCommandType::light_attack, 2, 0},
        CombatCommand{1, 2, 1, CombatCommandType::heavy_attack, 1, 0},
    };
    ok &= resolver.submit(commands) == CombatError::none;
    for (int tick = 0; tick < 6; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }
    ok &= expect(!resolver.actors()[1].active, "the earlier hit defeats the hostile");
    ok &= expect(
        resolver.actors()[0].resources.health == 120,
        "a defeated hostile cannot finish its pending heavy attack"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_hit_window_guard_break_and_stance();
    ok &= test_unguarded_poise_break();
    ok &= test_evade_window_boundaries();
    ok &= test_floor_miss_and_determinism();
    ok &= test_authoritative_pose_sync();
    ok &= test_delayed_resource_recovery();
    ok &= test_defeat_retry_restores_initial_encounter();
    ok &= test_defeat_cancels_pending_ability();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
