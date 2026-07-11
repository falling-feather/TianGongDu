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
         player_stance},
        {2,
         tgd::contracts::content_id("actor_test_hostile"),
         tgd::contracts::CombatFaction::hostile,
         {4'000, 0, 0, 0},
         hostile_resources,
         {hostile_stance, 0, 0},
         1,
         hostile_stance},
        {3,
         tgd::contracts::content_id("actor_test_hostile"),
         tgd::contracts::CombatFaction::hostile,
         {4'200, 300, 0, 0},
         hostile_resources,
         {hostile_stance, 0, 0},
         1,
         hostile_stance},
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
    return {1, 5'000, 7'000, 1'800, 1, 3, 1};
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

}  // namespace

int main() {
    bool ok = true;
    ok &= test_chase_attack_tokens_and_determinism();
    ok &= test_wrong_tick_and_leash_return();
    ok &= test_invalid_definition_fails_closed();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
