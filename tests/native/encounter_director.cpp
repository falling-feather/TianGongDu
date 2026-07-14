#include <tgd/contracts/combat_types.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/gameplay/combat_resolver.hpp>
#include <tgd/gameplay/encounter_director.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using tgd::contracts::CombatCommandType;
using tgd::gameplay::CombatError;
using tgd::gameplay::DeterministicCombatResolver;
using tgd::gameplay::DeterministicEncounterDirector;
using tgd::gameplay::EncounterDirectorError;

constexpr auto player_stance = tgd::contracts::stable_content_key("stance_test_player");
constexpr auto hostile_stance = tgd::contracts::stable_content_key("stance_test_hostile");

class CollectingSink final : public tgd::gameplay::ICombatEventSink {
  public:
    void publish(std::span<const tgd::contracts::CombatEvent> events) noexcept override {
        values.insert(values.end(), events.begin(), events.end());
    }

    [[nodiscard]] bool has_hostile_hit() const noexcept {
        return std::any_of(values.begin(), values.end(), [](const auto& event) {
            return event.type == tgd::contracts::CombatEventType::hit_landed &&
                   event.source != 1 && event.target == 1;
        });
    }

    std::vector<tgd::contracts::CombatEvent> values;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "encounter director failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] std::array<tgd::contracts::CombatActorConfig, 3> actors() {
    const tgd::contracts::CombatResources player_resources{120, 120, 100, 100, 80, 80, 30, 30, 0};
    const tgd::contracts::CombatResources hostile_resources{80, 80, 100, 100, 35, 35, 0, 0, 0};
    return {{
        {1,
         tgd::contracts::content_id("actor_test_player"),
         tgd::contracts::CombatFaction::player,
         {0, 0, 0, 0},
         player_resources,
         {player_stance, 0, 0},
         1,
         player_stance,
         {60, 6, 2, 120, 12, 2}},
        {2,
         tgd::contracts::content_id("actor_test_hostile"),
         tgd::contracts::CombatFaction::hostile,
         {4'000, 0, 0, 0},
         hostile_resources,
         {hostile_stance, 0, 0},
         1,
         hostile_stance,
         {60, 6, 2, 120, 12, 2}},
        {3,
         tgd::contracts::content_id("actor_test_hostile"),
         tgd::contracts::CombatFaction::hostile,
         {4'200, 300, 0, 0},
         hostile_resources,
         {hostile_stance, 0, 0},
         1,
         hostile_stance,
         {60, 6, 2, 120, 12, 2}},
    }};
}

[[nodiscard]] std::array<tgd::contracts::AbilityDefinition, 2> abilities() {
    return {{
        {tgd::contracts::content_id("ability_test_hostile_light"),
         CombatCommandType::light_attack,
         hostile_stance,
         2,
         3,
         1,
         5,
         1'000,
         500,
         10,
         5,
         tgd::contracts::feedback_light},
        {tgd::contracts::content_id("ability_test_hostile_heavy"),
         CombatCommandType::heavy_attack,
         hostile_stance,
         5,
         6,
         2,
         8,
         1'100,
         500,
         20,
         15,
         tgd::contracts::feedback_heavy},
    }};
}

[[nodiscard]] tgd::contracts::EncounterDirectorDefinition director_definition() {
    return {1, 5'000, 7'000, 1'800, 900, 1, 3, 1};
}

struct SimulationResult final {
    std::uint64_t combat_checksum{};
    std::uint64_t director_checksum{};
    std::int32_t player_health{};
    std::int32_t nearest_hostile_x{};
    std::size_t maximum_commands_per_tick{};
    bool hostile_hit{};
    bool ok{};
};

[[nodiscard]] SimulationResult run_simulation() {
    const auto actor_configs = actors();
    const auto ability_configs = abilities();
    DeterministicCombatResolver combat;
    DeterministicEncounterDirector director;
    CollectingSink sink;
    bool ok = combat.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= combat.start() == CombatError::none;
    ok &= director.initialize(director_definition(), actor_configs, ability_configs) ==
          EncounterDirectorError::none;
    tgd::contracts::CommandSequence sequence = 1;
    std::size_t maximum_commands = 0;
    for (tgd::contracts::TickIndex tick = 1; tick <= 160; ++tick) {
        const auto plan = director.plan_tick(tick, combat.actors(), sequence);
        ok &= plan.error == EncounterDirectorError::none;
        maximum_commands = std::max(maximum_commands, plan.batch.command_count);
        sequence += plan.batch.command_count;
        if (!plan.batch.poses().empty()) {
            ok &= combat.synchronize_poses(plan.batch.poses()) == CombatError::none;
        }
        if (!plan.batch.command_view().empty()) {
            ok &= combat.submit(plan.batch.command_view()) == CombatError::none;
        }
        ok &= combat.advance_one_tick(sink) == CombatError::none;
    }
    const auto snapshots = combat.actors();
    return {
        combat.checksum(),
        director.checksum(),
        snapshots[0].resources.health,
        std::min(snapshots[1].pose.x, snapshots[2].pose.x),
        maximum_commands,
        sink.has_hostile_hit(),
        ok,
    };
}

bool test_chase_attack_tokens_and_determinism() {
    const auto left = run_simulation();
    const auto right = run_simulation();
    bool ok = expect(left.ok && right.ok, "integrated simulations complete");
    ok &= expect(left.nearest_hostile_x < 4'000, "hostiles chase in authoritative ground space");
    ok &= expect(left.hostile_hit && left.player_health < 120, "director commands resolve through combat");
    ok &= expect(left.maximum_commands_per_tick == 1, "one attack token limits simultaneous attackers");
    ok &= expect(
        left.combat_checksum == right.combat_checksum &&
            left.director_checksum == right.director_checksum &&
            left.player_health == right.player_health,
        "same snapshots produce the same director and combat result"
    );
    return ok;
}

bool test_wrong_tick_and_leash_return() {
    const auto actor_configs = actors();
    const auto ability_configs = abilities();
    DeterministicEncounterDirector director;
    bool ok = director.initialize(director_definition(), actor_configs, ability_configs) ==
              EncounterDirectorError::none;
    std::array<tgd::contracts::CombatActorSnapshot, 3> snapshots{{
        {1, actor_configs[0].archetype_id.key, tgd::contracts::CombatFaction::player, {-10'000, 0, 0, 0}, actor_configs[0].initial_resources, player_stance, 0, false, true},
        {2, actor_configs[1].archetype_id.key, tgd::contracts::CombatFaction::hostile, {5'000, 0, 0, 0}, actor_configs[1].initial_resources, hostile_stance, 0, false, true},
        {3, actor_configs[2].archetype_id.key, tgd::contracts::CombatFaction::hostile, actor_configs[2].initial_pose, actor_configs[2].initial_resources, hostile_stance, 0, false, true},
    }};
    ok &= expect(
        director.plan_tick(2, snapshots, 1).error == EncounterDirectorError::wrong_tick,
        "director rejects skipped ticks"
    );
    const auto plan = director.plan_tick(1, snapshots, 1);
    ok &= expect(plan.error == EncounterDirectorError::none, "valid tick follows a rejected plan");
    const auto returning = std::find_if(
        plan.batch.poses().begin(),
        plan.batch.poses().end(),
        [](const auto& update) { return update.actor == 2; }
    );
    ok &= expect(returning != plan.batch.poses().end(), "disengaged hostile returns home");
    if (returning != plan.batch.poses().end()) {
        ok &= expect(returning->pose.x == 4'970, "leash return uses the quantized chase step");
    }
    return ok;
}

bool test_invalid_definition_fails_closed() {
    const auto actor_configs = actors();
    const auto ability_configs = abilities();
    auto invalid = director_definition();
    invalid.chase_speed_mm_per_second = 1'801;
    DeterministicEncounterDirector director;
    bool ok = expect(
        director.initialize(invalid, actor_configs, ability_configs) ==
            EncounterDirectorError::invalid_definition,
        "fractional per-tick chase speed is rejected"
    );
    const std::array missing_heavy{ability_configs[0]};
    ok &= expect(
        director.initialize(director_definition(), actor_configs, missing_heavy) ==
            EncounterDirectorError::missing_ability,
        "hostile stance requires light and heavy abilities"
    );
    return ok;
}

bool test_retry_resets_director_boundary() {
    DeterministicEncounterDirector director;
    const auto actor_configs = actors();
    const auto ability_configs = abilities();
    bool ok = director.initialize(director_definition(), actor_configs, ability_configs) ==
              EncounterDirectorError::none;
    std::array<tgd::contracts::CombatActorSnapshot, 3> snapshots{{
        {1, actor_configs[0].archetype_id.key, tgd::contracts::CombatFaction::player, {0, 0, 0, 0}, actor_configs[0].initial_resources, player_stance, 0, false, true},
        {2, actor_configs[1].archetype_id.key, tgd::contracts::CombatFaction::hostile, {900, 0, 0, 0}, actor_configs[1].initial_resources, hostile_stance, 0, false, true},
        {3, actor_configs[2].archetype_id.key, tgd::contracts::CombatFaction::hostile, actor_configs[2].initial_pose, actor_configs[2].initial_resources, hostile_stance, 0, false, true},
    }};
    ok &= director.plan_tick(1, snapshots, 1).error == EncounterDirectorError::none;
    ok &= expect(
        director.retry_from_initial({0, 1, 1}) ==
            EncounterDirectorError::retry_targets_wrong_tick,
        "director retry binds to the completed tick"
    );
    ok &= expect(
        director.retry_from_initial({1, 1, 1}) == EncounterDirectorError::none,
        "director clears its attack token runtime on retry"
    );
    ok &= expect(
        director.retry_from_initial({1, 1, 1}) ==
            EncounterDirectorError::stale_retry_sequence,
        "director rejects a duplicate retry sequence"
    );
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 2>
        duplicate_slots{{
            {2, {2'000, 0, 0, 0}, 2},
            {3, {-2'000, 0, 0, 0}, 2},
        }};
    const tgd::contracts::EncounterActivationCommand stage_activation{
        1,
        1,
        2,
        tgd::contracts::EncounterActivationMode::replace,
    };
    ok &= expect(
        director.activate_group(stage_activation, duplicate_slots, snapshots) ==
            EncounterDirectorError::activation_not_allowed,
        "one authored group cannot overlap formation slots"
    );
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 2> placements{{
        {2, {2'000, 0, 0, 0}, 2},
        {3, {-2'000, 0, 0, 0}, 6},
    }};
    ok &= expect(
        director.activate_group(stage_activation, placements, snapshots) ==
            EncounterDirectorError::none,
        "director accepts authored home poses and formation slots"
    );
    snapshots[1].pose = placements[0].pose;
    snapshots[2].pose = placements[1].pose;
    const auto formation_plan = director.plan_tick(2, snapshots, 10);
    ok &= expect(
        formation_plan.error == EncounterDirectorError::none,
        "director continues from the next monotonic tick after retry"
    );
    const auto upper = std::find_if(
        formation_plan.batch.poses().begin(),
        formation_plan.batch.poses().end(),
        [](const auto& update) { return update.actor == 2; }
    );
    const auto lower = std::find_if(
        formation_plan.batch.poses().begin(),
        formation_plan.batch.poses().end(),
        [](const auto& update) { return update.actor == 3; }
    );
    ok &= expect(
        upper != formation_plan.batch.poses().end() && upper->pose.y > 0 &&
            lower != formation_plan.batch.poses().end() && lower->pose.y < 0,
        "authored formation slots drive distinct approach vectors"
    );
    snapshots[2].resources.health = 0;
    snapshots[2].active = false;
    snapshots[2].defeated = false;
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 1>
        overlapping_reinforcement{{
            {3, {-1'000, -1'500, 0, 0}, 2},
        }};
    const tgd::contracts::EncounterActivationCommand reinforce{
        2,
        1,
        3,
        tgd::contracts::EncounterActivationMode::reinforce,
    };
    ok &= expect(
        director.activate_group(reinforce, overlapping_reinforcement, snapshots) ==
            EncounterDirectorError::activation_not_allowed,
        "reinforcement cannot overlap an active formation slot"
    );
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 1>
        reinforcement{{
            {3, {-1'000, -1'500, 0, 0}, 6},
        }};
    ok &= expect(
        director.activate_group(reinforce, reinforcement, snapshots) ==
            EncounterDirectorError::none,
        "director adds a dormant reinforcement without replacing active runtimes"
    );
    snapshots[2].resources = actor_configs[2].initial_resources;
    snapshots[2].pose = reinforcement[0].pose;
    snapshots[2].active = true;
    ok &= expect(
        director.plan_tick(3, snapshots, 20).error == EncounterDirectorError::none,
        "reinforced formations continue on the next deterministic tick"
    );
    return ok;
}

bool test_multi_actor_reinforcement_is_atomic() {
    const auto base = actors();
    std::array<tgd::contracts::CombatActorConfig, 5> actor_configs{
        base[0],
        base[1],
        base[2],
        base[1],
        base[1],
    };
    actor_configs[2].initially_active = false;
    actor_configs[3].actor = 4;
    actor_configs[3].archetype_id = tgd::contracts::content_id("actor_test_hostile_b");
    actor_configs[3].initial_pose = {-4'200, -300, 0, 0};
    actor_configs[3].initially_active = false;
    actor_configs[4].actor = 5;
    actor_configs[4].archetype_id = tgd::contracts::content_id("actor_test_hostile_reserve");
    actor_configs[4].initial_pose = {4'400, 600, 0, 0};
    actor_configs[4].initially_active = false;

    DeterministicEncounterDirector director;
    bool ok = director.initialize(director_definition(), actor_configs, abilities()) ==
              EncounterDirectorError::none;
    std::array<tgd::contracts::CombatActorSnapshot, 5> snapshots{{
        {1,
         actor_configs[0].archetype_id.key,
         tgd::contracts::CombatFaction::player,
         actor_configs[0].initial_pose,
         actor_configs[0].initial_resources,
         player_stance,
         0,
         false,
         true},
        {2,
         actor_configs[1].archetype_id.key,
         tgd::contracts::CombatFaction::hostile,
         actor_configs[1].initial_pose,
         actor_configs[1].initial_resources,
         hostile_stance,
         0,
         false,
         true},
        {3,
         actor_configs[2].archetype_id.key,
         tgd::contracts::CombatFaction::hostile,
         actor_configs[2].initial_pose,
         actor_configs[2].initial_resources,
         hostile_stance,
         0,
         false,
         false},
        {4,
         actor_configs[3].archetype_id.key,
         tgd::contracts::CombatFaction::hostile,
         actor_configs[3].initial_pose,
         actor_configs[3].initial_resources,
         hostile_stance,
         0,
         false,
         false},
        {5,
         actor_configs[4].archetype_id.key,
         tgd::contracts::CombatFaction::hostile,
         actor_configs[4].initial_pose,
         actor_configs[4].initial_resources,
         hostile_stance,
         0,
         false,
         false},
    }};
    const tgd::contracts::EncounterActivationCommand reinforce{
        0,
        1,
        1,
        tgd::contracts::EncounterActivationMode::reinforce,
    };
    const auto checksum_before_invalid = director.checksum();
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 2> invalid_batch{{
        {3, {3'000, 1'500, 0, 0}, 6},
        {2, {-3'000, -1'500, 0, 0}, 8},
    }};
    ok &= expect(
        director.activate_group(reinforce, invalid_batch, snapshots) ==
            EncounterDirectorError::activation_not_allowed,
        "director rejects a whole reinforcement batch when a later actor is already active"
    );
    ok &= expect(
        director.checksum() == checksum_before_invalid,
        "a rejected reinforcement does not partially change an earlier hostile runtime"
    );

    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 2> valid_batch{{
        {3, {3'000, 1'500, 0, 0}, 6},
        {4, {-3'000, -1'500, 0, 0}, 8},
    }};
    ok &= expect(
        director.activate_group(reinforce, valid_batch, snapshots) ==
            EncounterDirectorError::none,
        "director accepts two dormant reinforcements in one monotonic boundary"
    );
    const auto checksum_after_commit = director.checksum();
    ok &= expect(
        checksum_after_commit != checksum_before_invalid,
        "committed multi-actor reinforcement updates the director checksum once"
    );
    ok &= expect(
        director.activate_group(reinforce, valid_batch, snapshots) ==
            EncounterDirectorError::stale_activation_sequence,
        "director rejects replay of a committed multi-actor reinforcement"
    );
    ok &= expect(
        director.checksum() == checksum_after_commit,
        "replayed reinforcement leaves the committed director runtime unchanged"
    );

    snapshots[2].pose = valid_batch[0].pose;
    snapshots[2].active = true;
    snapshots[3].pose = valid_batch[1].pose;
    snapshots[3].active = true;
    const auto plan = director.plan_tick(1, snapshots, 10);
    ok &= expect(
        plan.error == EncounterDirectorError::none,
        "director plans the next tick after a multi-actor reinforcement"
    );
    const auto first_reinforcement = std::find_if(
        plan.batch.poses().begin(),
        plan.batch.poses().end(),
        [](const auto& update) { return update.actor == 3; }
    );
    const auto second_reinforcement = std::find_if(
        plan.batch.poses().begin(),
        plan.batch.poses().end(),
        [](const auto& update) { return update.actor == 4; }
    );
    ok &= expect(
        first_reinforcement != plan.batch.poses().end() &&
            second_reinforcement != plan.batch.poses().end(),
        "both reinforced actors participate in the same deterministic encounter plan"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_chase_attack_tokens_and_determinism();
    ok &= test_wrong_tick_and_leash_return();
    ok &= test_invalid_definition_fails_closed();
    ok &= test_retry_resets_director_boundary();
    ok &= test_multi_actor_reinforcement_is_atomic();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
