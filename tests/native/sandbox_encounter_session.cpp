#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_gameplay_binding.hpp>
#include <tgd/gameplay/sandbox_encounter_session.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using tgd::contracts::CombatFaction;
using tgd::contracts::EncounterTacticalDuty;
using tgd::contracts::SandboxActorDefinition;
using tgd::contracts::SandboxActorGameplayBinding;
using tgd::contracts::SandboxGameplayBindingDefinition;
using tgd::contracts::SandboxMechanismActivation;
using tgd::contracts::SandboxMechanismGameplayBinding;
using tgd::contracts::SandboxObjectiveCompletionKind;
using tgd::contracts::SandboxTriggerKind;
using tgd::contracts::content_id;
using tgd::gameplay::SandboxEncounterAttack;
using tgd::gameplay::SandboxEncounterAttackDisposition;
using tgd::gameplay::SandboxEncounterBuildError;
using tgd::gameplay::SandboxEncounterEventDisposition;
using tgd::gameplay::SandboxEncounterSession;
using tgd::gameplay::SandboxEncounterStepDisposition;

constexpr auto player_actor = std::uint64_t{0x706c617965720083ULL};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "sandbox encounter session failure: " << message << '\n';
    }
    return condition;
}

struct Fixture final {
    std::array<tgd::contracts::SandboxRegionDefinition, 1> regions{{
        {content_id("region.demo"), {-3'000, 3'000, -3'000, 10'000, -500, 3'000, 0, 0}},
    }};
    std::array<SandboxActorDefinition, 4> actors{{
        {content_id("actor.entry.pressure"), regions[0].id, content_id("asset.actor"),
         {-800, 800, 0, 0}, 180'000},
        {content_id("actor.entry.flanker"), regions[0].id, content_id("asset.actor"),
         {800, 900, 0, 0}, 180'000},
        {content_id("actor.followup.pressure"), regions[0].id, content_id("asset.actor"),
         {-700, 1'000, 0, 0}, 180'000},
        {content_id("actor.followup.flanker"), regions[0].id, content_id("asset.actor"),
         {700, 1'100, 0, 0}, 180'000},
    }};
    std::array<tgd::contracts::SandboxInteractionDefinition, 1> interactions{{
        {content_id("interaction.console"), regions[0].id, content_id("asset.interaction"),
         {0, 0, 0, 0}, 0},
    }};
    std::array<tgd::contracts::SandboxMechanismDefinition, 1> mechanisms{{
        {content_id("mechanism.gate"), regions[0].id, content_id("asset.mechanism"),
         {0, 0, 0, 0}, 0},
    }};
    std::array<tgd::contracts::SandboxGroundBlockerDefinition, 1> blockers{{
        {content_id("blocker.gate"), regions[0].id, content_id("asset.blocker"),
         -500, 500, 200, 400, 0, 2'000, 0},
    }};
    std::array<tgd::contracts::SandboxWaveDefinition, 2> waves{{
        {content_id("wave.entry"), regions[0].id, {},
         {SandboxTriggerKind::objective_completed, content_id("objective.open")}},
        {content_id("wave.followup"), regions[0].id, content_id("wave.entry"),
         {SandboxTriggerKind::wave_completed, content_id("wave.entry")}},
    }};
    std::array<tgd::contracts::SandboxWaveSpawnDefinition, 4> spawns{{
        {waves[0].id, actors[0].id, 0, 0},
        {waves[0].id, actors[1].id, 0, 1},
        {waves[1].id, actors[2].id, 0, 0},
        {waves[1].id, actors[3].id, 0, 1},
    }};
    std::array<tgd::contracts::SandboxObjectiveDefinition, 2> objectives{{
        {content_id("objective.open"), regions[0].id, {},
         {SandboxObjectiveCompletionKind::mechanism_activated, mechanisms[0].id}},
        {content_id("objective.terminal"), regions[0].id, content_id("objective.open"),
         {SandboxObjectiveCompletionKind::wave_completed, waves[1].id}},
    }};
    std::array<tgd::contracts::SandboxInteractionGameplayBinding, 1>
        interaction_bindings{{
            {interactions[0].id,
             tgd::contracts::SandboxInteractionOperation::operate,
             1'200,
             mechanisms[0].id},
        }};
    std::array<SandboxMechanismGameplayBinding, 1> mechanism_bindings{{
        {mechanisms[0].id, SandboxMechanismActivation::one_shot_activate,
         blockers[0].id},
    }};
    std::array<SandboxActorGameplayBinding, 4> actor_bindings{{
        {actors[0].id, content_id("jn_enemy_leaking_umbrella_doll"),
         CombatFaction::hostile, EncounterTacticalDuty::pressure, 18},
        {actors[1].id, content_id("jn_enemy_towline_water_hand"),
         CombatFaction::hostile, EncounterTacticalDuty::flanker, 18},
        {actors[2].id, content_id("jn_enemy_leaking_umbrella_doll"),
         CombatFaction::hostile, EncounterTacticalDuty::pressure, 18},
        {actors[3].id, content_id("jn_enemy_towline_water_hand"),
         CombatFaction::hostile, EncounterTacticalDuty::flanker, 18},
    }};
    tgd::contracts::SandboxDefinition definition{};
    SandboxGameplayBindingDefinition binding{};

    Fixture() {
        definition.id = content_id("sandbox.demo");
        definition.package_id = content_id("package.demo");
        definition.bounds = regions[0].bounds;
        definition.completion_objective_id = objectives[1].id;
        definition.regions = regions;
        definition.actors = actors;
        definition.ground_blockers = blockers;
        definition.interactions = interactions;
        definition.mechanisms = mechanisms;
        definition.waves = waves;
        definition.wave_spawns = spawns;
        definition.objectives = objectives;
        binding = {
            interaction_bindings,
            mechanism_bindings,
            actor_bindings,
        };
    }
};

struct CompletionRun final {
    bool ok{};
    std::uint64_t checksum{};
    tgd::gameplay::SandboxEncounterSnapshot snapshot{};
};

CompletionRun complete_two_waves() {
    Fixture fixture;
    SandboxEncounterSession session;
    bool ok = session.initialize(
        fixture.definition,
        fixture.binding,
        player_actor,
        {0, 0, 0, 0}
    ) == SandboxEncounterBuildError::none;
    ok &= session.snapshot().active_hostile_count == 0;
    ok &= session.notify_mechanism_activated(fixture.mechanisms[0].id.key) ==
          SandboxEncounterEventDisposition::applied;
    ok &= session.notify_mechanism_activated(fixture.mechanisms[0].id.key) ==
          SandboxEncounterEventDisposition::repeated;
    ok &= session.snapshot().active_hostile_count == 2;

    for (std::uint32_t tick = 0;
         tick < 600 && !session.snapshot().terminal_completed;
         ++tick) {
        if (tick % 28U == 0U) {
            const auto queued = session.queue_player_attack(
                tick % 56U == 0U
                    ? SandboxEncounterAttack::heavy
                    : SandboxEncounterAttack::light
            );
            ok &= queued == SandboxEncounterAttackDisposition::queued ||
                  queued == SandboxEncounterAttackDisposition::already_queued;
        }
        const auto step = session.advance_one_tick({0, 0, 0, 0});
        ok &= step.disposition == SandboxEncounterStepDisposition::advanced ||
              step.disposition ==
                  SandboxEncounterStepDisposition::terminal_completed;
    }
    const auto snapshot = session.snapshot();
    ok &= snapshot.terminal_completed;
    ok &= snapshot.completed_wave_count == 2;
    ok &= snapshot.completed_objective_count == 2;
    ok &= snapshot.defeated_hostile_count == 4;
    ok &= snapshot.active_hostile_count == 0;
    ok &= snapshot.repeated_trigger_count == 1;

    ok &= session.restart({0, 0, 0, 0}) ==
          SandboxEncounterBuildError::none;
    const auto restarted = session.snapshot();
    ok &= !restarted.terminal_completed &&
          restarted.completed_wave_count == 0 &&
          restarted.completed_objective_count == 0 &&
          restarted.active_hostile_count == 0 &&
          restarted.player_health == restarted.player_health_max;
    return {ok, snapshot.checksum, snapshot};
}

bool test_completion_repeat_and_restart() {
    const auto first = complete_two_waves();
    const auto second = complete_two_waves();
    return expect(first.ok && second.ok, "two authored waves complete and restart") &&
           expect(first.checksum == second.checksum,
                  "fixed input produces the same encounter checksum");
}

bool test_player_death_can_restart_local_route() {
    Fixture fixture;
    SandboxEncounterSession session;
    bool ok = session.initialize(
        fixture.definition,
        fixture.binding,
        player_actor,
        {0, 0, 0, 0}
    ) == SandboxEncounterBuildError::none;
    ok &= session.notify_mechanism_activated(fixture.mechanisms[0].id.key) ==
          SandboxEncounterEventDisposition::applied;
    for (std::uint32_t tick = 0;
         tick < 2'400 && !session.snapshot().player_defeated;
         ++tick) {
        const auto step = session.advance_one_tick({0, 0, 0, 0});
        ok &= step.disposition == SandboxEncounterStepDisposition::advanced ||
              step.disposition == SandboxEncounterStepDisposition::player_defeated;
    }
    ok &= session.snapshot().player_defeated;
    ok &= session.restart({-200, -300, 0, 0}) ==
          SandboxEncounterBuildError::none;
    const auto restarted = session.snapshot();
    ok &= !restarted.player_defeated &&
          restarted.player_health == restarted.player_health_max &&
          restarted.active_hostile_count == 0 &&
          restarted.completed_wave_count == 0;
    return expect(ok, "player death restores a clean local encounter route");
}

bool test_failed_activation_preserves_pre_trigger_state() {
    Fixture fixture;
    fixture.spawns[1].spawn_order = fixture.spawns[0].spawn_order;
    SandboxEncounterSession session;
    bool ok = session.initialize(
        fixture.definition,
        fixture.binding,
        player_actor,
        {0, 0, 0, 0}
    ) == SandboxEncounterBuildError::none;
    const auto before = session.snapshot();
    ok &= session.notify_mechanism_activated(fixture.mechanisms[0].id.key) ==
          SandboxEncounterEventDisposition::invalid_state;
    ok &= session.snapshot() == before;
    ok &= session.wave_state(fixture.waves[0].id.key) ==
          tgd::gameplay::SandboxWaveRuntimeState::waiting;
    ok &= session.objective_state(fixture.objectives[0].id.key) ==
          tgd::gameplay::SandboxObjectiveRuntimeState::active;
    return expect(ok, "failed activation preserves the pre-trigger route");
}

bool test_failed_delayed_activation_preserves_pre_step_state() {
    Fixture fixture;
    fixture.spawns[0].delay_ticks = 1;
    fixture.spawns[1].delay_ticks = 1;
    fixture.spawns[1].spawn_order = fixture.spawns[0].spawn_order;
    SandboxEncounterSession session;
    bool ok = session.initialize(
        fixture.definition,
        fixture.binding,
        player_actor,
        {0, 0, 0, 0}
    ) == SandboxEncounterBuildError::none;
    ok &= session.notify_mechanism_activated(fixture.mechanisms[0].id.key) ==
          SandboxEncounterEventDisposition::applied;
    ok &= session.advance_one_tick({0, 0, 0, 0}).disposition ==
          SandboxEncounterStepDisposition::advanced;
    const auto before = session.snapshot();
    ok &= session.advance_one_tick({0, 0, 0, 0}).disposition ==
          SandboxEncounterStepDisposition::activation_rejected;
    ok &= session.snapshot() == before;
    return expect(ok, "failed delayed activation preserves pre-step state");
}

bool test_saturated_distance_keeps_an_active_target() {
    Fixture fixture;
    fixture.actors[0].pose = {
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::max(),
        0,
        0,
    };
    fixture.actors[1].pose = {
        std::numeric_limits<std::int32_t>::max() - 1,
        std::numeric_limits<std::int32_t>::max(),
        0,
        0,
    };
    const tgd::contracts::GroundPoseMm player_pose{
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::min(),
        0,
        0,
    };
    SandboxEncounterSession session;
    bool ok = session.initialize(
        fixture.definition,
        fixture.binding,
        player_actor,
        player_pose
    ) == SandboxEncounterBuildError::none;
    ok &= session.notify_mechanism_activated(fixture.mechanisms[0].id.key) ==
          SandboxEncounterEventDisposition::applied;
    ok &= session.queue_player_attack(SandboxEncounterAttack::light) ==
          SandboxEncounterAttackDisposition::queued;
    return expect(ok, "saturated distance still selects an active hostile");
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_completion_repeat_and_restart();
    ok &= test_player_death_can_restart_local_route();
    ok &= test_failed_activation_preserves_pre_trigger_state();
    ok &= test_failed_delayed_activation_preserves_pre_step_state();
    ok &= test_saturated_distance_keeps_an_active_target();
    return ok ? 0 : 1;
}
