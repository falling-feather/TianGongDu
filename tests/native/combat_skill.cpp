#include <tgd/contracts/action_registry.generated.hpp>
#include <tgd/contracts/combat_types.hpp>
#include <tgd/gameplay/combat_action_intent.hpp>
#include <tgd/gameplay/combat_resolver.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using tgd::contracts::AbilityDefinition;
using tgd::contracts::AbilityTargetPolicy;
using tgd::contracts::CombatActorConfig;
using tgd::contracts::CombatCommand;
using tgd::contracts::CombatCommandType;
using tgd::contracts::CombatEvent;
using tgd::contracts::CombatEventType;
using tgd::contracts::CombatSkillQueryError;
using tgd::contracts::CombatSkillSlot;
using tgd::gameplay::CombatActionIntent;
using tgd::gameplay::CombatActionIntentError;
using tgd::gameplay::CombatError;
using tgd::gameplay::DeterministicCombatActionIntentMapper;
using tgd::gameplay::DeterministicCombatResolver;

constexpr auto eavesguard = tgd::contracts::stable_content_key("stance_skill_eavesguard");
constexpr auto flower_turn = tgd::contracts::stable_content_key("stance_skill_flower_turn");
constexpr auto player_eaves_skill =
    tgd::contracts::stable_content_key("skill_player_eavesguard_pressure");
constexpr auto hostile_eaves_skill =
    tgd::contracts::stable_content_key("skill_hostile_eavesguard_pressure");
constexpr auto flower_skill =
    tgd::contracts::stable_content_key("skill_flower_turn_crosscut");
constexpr auto shared_skill =
    tgd::contracts::stable_content_key("skill_shared_eavesguard_pulse");
constexpr auto legacy_light =
    tgd::contracts::stable_content_key("ability_legacy_light");

template <typename T>
concept CallerCarriesAbilityId = requires(T value) { value.ability; };

static_assert(!CallerCarriesAbilityId<CombatCommand>);

class CollectingSink final : public tgd::gameplay::ICombatEventSink {
  public:
    void publish(std::span<const CombatEvent> events) noexcept override {
        values.insert(values.end(), events.begin(), events.end());
    }

    void clear() { values.clear(); }

    [[nodiscard]] std::size_t count(CombatEventType type) const noexcept {
        return static_cast<std::size_t>(std::count_if(
            values.begin(),
            values.end(),
            [type](const CombatEvent& event) { return event.type == type; }
        ));
    }

    [[nodiscard]] bool contains_ability(
        CombatEventType type,
        tgd::contracts::StableContentKey ability,
        tgd::contracts::StableActorKey source = 0
    ) const noexcept {
        return std::any_of(values.begin(), values.end(), [type, ability, source](const auto& event) {
            return event.type == type && event.ability == ability &&
                   (source == 0 || event.source == source);
        });
    }

    std::vector<CombatEvent> values;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "combat skill failure: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] std::array<CombatActorConfig, 2> actors() {
    const tgd::contracts::CombatResources player_resources{
        120,
        120,
        100,
        100,
        80,
        80,
        0,
        0,
        0,
    };
    const tgd::contracts::CombatResources hostile_resources{
        150,
        150,
        100,
        100,
        60,
        60,
        0,
        0,
        0,
    };
    std::array<CombatActorConfig, 2> result{{
        {1,
         tgd::contracts::content_id("actor_skill_player"),
         tgd::contracts::CombatFaction::player,
         {0, 0, 0, 0},
         player_resources,
         {eavesguard, flower_turn, 0, 0},
         2,
         eavesguard,
         {60, 6, 2, 60, 6, 2}},
        {2,
         tgd::contracts::content_id("actor_skill_hostile"),
         tgd::contracts::CombatFaction::hostile,
         {900, 0, 0, 0},
         hostile_resources,
         {eavesguard, 0, 0, 0},
         1,
         eavesguard,
         {60, 6, 2, 60, 6, 2}},
    }};
    result[0].skill_loadout[0] = {
        eavesguard,
        CombatSkillSlot::primary,
        player_eaves_skill,
    };
    result[0].skill_loadout[1] = {
        flower_turn,
        CombatSkillSlot::primary,
        flower_skill,
    };
    result[0].skill_loadout_count = 2;
    result[1].skill_loadout[0] = {
        eavesguard,
        CombatSkillSlot::primary,
        hostile_eaves_skill,
    };
    result[1].skill_loadout_count = 1;
    return result;
}

[[nodiscard]] std::array<CombatActorConfig, 2> single_binding_actors() {
    auto result = actors();
    result[0].skill_loadout[1] = {};
    result[0].skill_loadout_count = 1;
    return result;
}

[[nodiscard]] std::array<AbilityDefinition, 5> abilities() {
    return {{
        {tgd::contracts::content_id("skill_player_eavesguard_pressure"),
         CombatCommandType::weapon_skill,
         eavesguard,
         20,
         0,
         1,
         2,
         1'200,
         500,
         25,
         20,
         tgd::contracts::feedback_heavy,
         AbilityTargetPolicy::opposing_actor,
         6,
         0},
        {tgd::contracts::content_id("skill_flower_turn_crosscut"),
         CombatCommandType::weapon_skill,
         flower_turn,
         35,
         3,
         2,
         1,
         1'800,
         700,
         40,
         12,
         tgd::contracts::feedback_light,
         AbilityTargetPolicy::opposing_actor,
         10,
         3},
        {tgd::contracts::content_id("skill_hostile_eavesguard_pressure"),
         CombatCommandType::weapon_skill,
         eavesguard,
         15,
         0,
         1,
         2,
         1'200,
         500,
         20,
         10,
         tgd::contracts::feedback_heavy,
         AbilityTargetPolicy::opposing_actor,
         5,
         0},
        {tgd::contracts::content_id("skill_shared_eavesguard_pulse"),
         CombatCommandType::weapon_skill,
         eavesguard,
         10,
         0,
         1,
         1,
         1'200,
         500,
         10,
         5,
         tgd::contracts::feedback_light,
         AbilityTargetPolicy::opposing_actor,
         8,
         0},
        {tgd::contracts::content_id("ability_legacy_light"),
         CombatCommandType::light_attack,
         eavesguard,
         5,
         0,
         1,
         1,
         1'200,
         500,
         5,
         2,
         tgd::contracts::feedback_light,
         AbilityTargetPolicy::trigger_default,
         0,
         0},
    }};
}

[[nodiscard]] tgd::contracts::ScalarActionSample action_sample(
    tgd::contracts::PlatformSequence platform_sequence,
    tgd::contracts::ActionId action = tgd::contracts::action_id("weapon_skill"),
    tgd::contracts::ActionSampleEdge edge = tgd::contracts::ActionSampleEdge::pressed,
    bool repeated = false
) {
    return {
        platform_sequence,
        action,
        edge == tgd::contracts::ActionSampleEdge::pressed
            ? tgd::contracts::ground_axis_one
            : 0,
        edge,
        repeated,
    };
}

[[nodiscard]] CombatActionIntent skill_intent(
    tgd::contracts::PlatformSequence platform_sequence,
    tgd::contracts::TickIndex tick,
    tgd::contracts::StableActorKey actor,
    tgd::contracts::CommandSequence sequence,
    tgd::contracts::StableActorKey target
) {
    return {action_sample(platform_sequence), tick, actor, sequence, target};
}

[[nodiscard]] CombatCommand skill_command(
    tgd::contracts::TickIndex tick,
    tgd::contracts::StableActorKey actor,
    tgd::contracts::CommandSequence sequence,
    tgd::contracts::StableActorKey target,
    CombatSkillSlot slot = CombatSkillSlot::primary
) {
    return {tick, actor, sequence, CombatCommandType::weapon_skill, target, 0, slot};
}

bool test_action_intent_maps_only_primary_slot() {
    DeterministicCombatActionIntentMapper mapper;
    bool ok = true;
    const auto mapped = mapper.resolve(skill_intent(1, 1, 1, 10, 2));
    ok &= expect(
        mapped.error == CombatActionIntentError::none && mapped.has_command &&
            mapped.command.type == CombatCommandType::weapon_skill &&
            mapped.command.skill_slot == CombatSkillSlot::primary &&
            mapped.command.actor == 1 && mapped.command.target == 2 &&
            mapper.last_platform_sequence() == 1,
        "weapon_skill maps to primary without a caller-provided Ability ID"
    );

    auto repeated = skill_intent(2, 2, 1, 11, 2);
    repeated.sample.repeated = true;
    ok &= expect(
        mapper.resolve(repeated).error == CombatActionIntentError::invalid_sample &&
            mapper.last_platform_sequence() == 1,
        "physical repeat does not consume the last-valid platform sequence"
    );
    auto released = skill_intent(2, 2, 1, 11, 2);
    released.sample = action_sample(
        2,
        tgd::contracts::action_id("weapon_skill"),
        tgd::contracts::ActionSampleEdge::released
    );
    ok &= expect(
        mapper.resolve(released).error == CombatActionIntentError::invalid_sample &&
            mapper.last_platform_sequence() == 1,
        "release cannot manufacture another skill command"
    );
    auto unknown = skill_intent(2, 2, 1, 11, 2);
    unknown.sample.action = tgd::contracts::action_id("unknown_skill_action");
    ok &= expect(
        mapper.resolve(unknown).error == CombatActionIntentError::unsupported_action &&
            mapper.last_platform_sequence() == 1,
        "unknown ActionId fails closed"
    );
    const auto invalid_identity = mapper.resolve(skill_intent(2, 2, 0, 11, 2));
    ok &= expect(
        invalid_identity.error == CombatActionIntentError::invalid_identity &&
            mapper.last_platform_sequence() == 1,
        "empty Gameplay actor identity fails without consuming input"
    );
    const auto accepted_after_failures = mapper.resolve(skill_intent(2, 2, 1, 11, 2));
    ok &= expect(
        accepted_after_failures.error == CombatActionIntentError::none &&
            mapper.last_platform_sequence() == 2,
        "a valid sample can reuse the sequence rejected by malformed samples"
    );
    ok &= expect(
        mapper.resolve(skill_intent(2, 3, 1, 12, 2)).error ==
            CombatActionIntentError::out_of_order_platform_sequence,
        "accepted Action samples cannot be replayed"
    );
    return ok;
}

bool test_actor_ownership_stance_and_normalization() {
    auto actor_configs = actors();
    auto ability_configs = abilities();
    auto reordered_actors = actor_configs;
    std::swap(
        reordered_actors[0].skill_loadout[0],
        reordered_actors[0].skill_loadout[1]
    );
    auto reordered_abilities = ability_configs;
    std::reverse(reordered_abilities.begin(), reordered_abilities.end());

    DeterministicCombatResolver left;
    DeterministicCombatResolver right;
    CollectingSink sink;
    bool ok = left.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= right.initialize(reordered_actors, reordered_abilities) == CombatError::none;
    ok &= expect(
        left.checksum() == right.checksum(),
        "private loadout and Definition normalization removes authored input order"
    );
    ok &= left.start() == CombatError::none;

    const auto player_initial = left.query_skill_cooldown(
        1,
        CombatSkillSlot::primary,
        player_eaves_skill
    );
    const auto hostile_initial = left.query_skill_cooldown(
        2,
        CombatSkillSlot::primary,
        hostile_eaves_skill
    );
    ok &= expect(
        player_initial.error == CombatSkillQueryError::none &&
            player_initial.ability == player_eaves_skill &&
            player_initial.ready_tick == 0 &&
            hostile_initial.error == CombatSkillQueryError::none &&
            hostile_initial.ability == hostile_eaves_skill &&
            hostile_initial.ready_tick == 0,
        "ready Tick zero is an explicit successful query for each actor-owned skill"
    );
    ok &= expect(
        left.query_skill_cooldown(1, CombatSkillSlot::primary, hostile_eaves_skill).error ==
                CombatSkillQueryError::ability_not_owned &&
            left.query_skill_cooldown(2, CombatSkillSlot::primary, player_eaves_skill).error ==
                CombatSkillQueryError::ability_not_owned,
        "actors sharing a stance cannot claim each other's equipped Ability"
    );

    const std::array simultaneous{
        skill_command(1, 1, 1, 2),
        skill_command(1, 2, 1, 1),
    };
    ok &= left.submit(simultaneous) == CombatError::none;
    ok &= left.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        sink.contains_ability(CombatEventType::ability_started, player_eaves_skill, 1) &&
            sink.contains_ability(CombatEventType::ability_started, hostile_eaves_skill, 2) &&
            left.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 7 &&
            left.query_skill_cooldown(
                2,
                CombatSkillSlot::primary,
                hostile_eaves_skill
            ).ready_tick == 6,
        "player and enemy use the same slot path but resolve their own definitions"
    );

    for (int tick = 2; tick <= 4; ++tick) {
        ok &= left.advance_one_tick(sink) == CombatError::none;
    }
    sink.clear();
    const std::array stance_and_skill{
        CombatCommand{5, 1, 2, CombatCommandType::switch_stance, 0, flower_turn},
        skill_command(5, 1, 3, 2),
    };
    ok &= left.submit(stance_and_skill) == CombatError::none;
    ok &= left.advance_one_tick(sink) == CombatError::none;
    const auto flower_state = left.query_skill_cooldown(
        1,
        CombatSkillSlot::primary,
        flower_skill
    );
    ok &= expect(
        left.actors()[0].stance == flower_turn &&
            sink.contains_ability(CombatEventType::ability_started, flower_skill, 1) &&
            flower_state.error == CombatSkillQueryError::none &&
            flower_state.ready_tick == 15,
        "the same primary slot resolves a different owned Ability after stance changes"
    );
    return ok;
}

bool test_shared_definition_has_actor_local_cooldown() {
    auto actor_configs = single_binding_actors();
    actor_configs[0].skill_loadout[0].ability = shared_skill;
    actor_configs[1].skill_loadout[0].ability = shared_skill;
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    bool ok = resolver.initialize(actor_configs, abilities()) == CombatError::none;
    ok &= resolver.start() == CombatError::none;

    const std::array player_use{skill_command(1, 1, 1, 2)};
    ok &= resolver.submit(player_use) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        resolver.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            shared_skill
        ).ready_tick == 9 &&
            resolver.query_skill_cooldown(
                2,
                CombatSkillSlot::primary,
                shared_skill
            ).ready_tick == 0,
        "one actor's use does not start another actor's cooldown"
    );

    sink.clear();
    const std::array hostile_use{skill_command(2, 2, 1, 1)};
    ok &= resolver.submit(hostile_use) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        sink.contains_ability(CombatEventType::ability_started, shared_skill, 2) &&
            resolver.query_skill_cooldown(
                2,
                CombatSkillSlot::primary,
                shared_skill
            ).ready_tick == 10,
        "two actors may share a Definition while retaining independent ready ticks"
    );
    return ok;
}

bool test_query_and_unbound_commands_fail_closed() {
    const auto actor_configs = single_binding_actors();
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    bool ok = resolver.initialize(actor_configs, abilities()) == CombatError::none;
    ok &= resolver.start() == CombatError::none;

    ok &= expect(
        resolver.query_skill_cooldown(
            999,
            CombatSkillSlot::primary,
            player_eaves_skill
        ).error == CombatSkillQueryError::unknown_actor,
        "unknown actor query is explicit"
    );
    ok &= expect(
        resolver.query_skill_cooldown(
            1,
            static_cast<CombatSkillSlot>(255),
            player_eaves_skill
        ).error == CombatSkillQueryError::invalid_slot,
        "unknown slot enum query is explicit"
    );
    ok &= expect(
        resolver.query_skill_cooldown(1, CombatSkillSlot::secondary).error ==
            CombatSkillQueryError::slot_unbound,
        "an authored empty slot is distinct from ready Tick zero"
    );
    ok &= expect(
        resolver.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            hostile_eaves_skill
        ).error == CombatSkillQueryError::ability_not_owned,
        "querying another actor's Ability fails closed"
    );

    const auto player_before = resolver.actors()[0].resources;
    const auto hostile_before = resolver.actors()[1].resources;
    const std::array unbound{skill_command(1, 1, 1, 2, CombatSkillSlot::secondary)};
    ok &= resolver.submit(unbound) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        sink.count(CombatEventType::command_ignored) == 1 &&
            resolver.actors()[0].resources == player_before &&
            resolver.actors()[1].resources == hostile_before &&
            resolver.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 0,
        "unbound slot spends no resource, deals no damage and writes no cooldown"
    );

    const auto checksum_before_invalid = resolver.checksum();
    const std::array zero_skill_sequence{skill_command(2, 1, 0, 2)};
    ok &= expect(
        resolver.submit(zero_skill_sequence) == CombatError::invalid_command &&
            resolver.checksum() == checksum_before_invalid,
        "weapon_skill alone requires a non-zero command sequence"
    );
    const std::array unknown_target{skill_command(2, 1, 2, 999)};
    ok &= expect(
        resolver.submit(unknown_target) == CombatError::invalid_command &&
            resolver.checksum() == checksum_before_invalid,
        "unknown target is rejected before queue mutation"
    );

    sink.clear();
    const std::array legacy_zero_sequence{
        CombatCommand{2, 1, 0, CombatCommandType::light_attack, 2, 0},
    };
    ok &= resolver.submit(legacy_zero_sequence) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        sink.contains_ability(CombatEventType::ability_started, legacy_light, 1),
        "legacy commands retain their baseline zero-sequence behavior"
    );
    return ok;
}

bool test_invalid_loadouts_fail_initialization() {
    const auto ability_configs = abilities();
    const auto rejects = [&ability_configs](
                             std::array<CombatActorConfig, 2> invalid,
                             std::string_view message
                         ) {
        DeterministicCombatResolver resolver;
        return expect(
            resolver.initialize(invalid, ability_configs) == CombatError::invalid_config,
            message
        );
    };

    bool ok = true;
    auto overflow = single_binding_actors();
    overflow[0].skill_loadout_count =
        static_cast<std::uint8_t>(tgd::contracts::combat_skill_binding_capacity + 1);
    ok &= rejects(overflow, "loadout count is bounded by fixed capacity");

    auto nonzero_tail = single_binding_actors();
    nonzero_tail[0].skill_loadout[1] = {
        flower_turn,
        CombatSkillSlot::primary,
        flower_skill,
    };
    ok &= rejects(nonzero_tail, "entries after count must remain zero");

    auto unknown_stance = single_binding_actors();
    unknown_stance[0].skill_loadout[0].stance =
        tgd::contracts::stable_content_key("stance_not_owned");
    ok &= rejects(unknown_stance, "binding stance must belong to the actor");

    auto duplicate_pair = single_binding_actors();
    duplicate_pair[0].skill_loadout[1] = {
        eavesguard,
        CombatSkillSlot::primary,
        hostile_eaves_skill,
    };
    duplicate_pair[0].skill_loadout_count = 2;
    ok &= rejects(duplicate_pair, "stance and slot pair is unique per actor");

    auto duplicate_ability = single_binding_actors();
    duplicate_ability[0].skill_loadout[1] = {
        eavesguard,
        CombatSkillSlot::secondary,
        player_eaves_skill,
    };
    duplicate_ability[0].skill_loadout_count = 2;
    ok &= rejects(duplicate_ability, "one actor cannot duplicate an Ability binding");

    auto unknown_ability = single_binding_actors();
    unknown_ability[0].skill_loadout[0].ability =
        tgd::contracts::stable_content_key("skill_not_defined");
    ok &= rejects(unknown_ability, "binding must resolve an authored Ability");

    auto legacy_binding = single_binding_actors();
    legacy_binding[0].skill_loadout[0].ability = legacy_light;
    ok &= rejects(legacy_binding, "loadout cannot bind a legacy non-skill Ability");

    auto stance_mismatch = single_binding_actors();
    stance_mismatch[0].skill_loadout[0].ability = flower_skill;
    ok &= rejects(stance_mismatch, "binding stance must match Ability required stance");

    auto invalid_slot = single_binding_actors();
    invalid_slot[0].skill_loadout[0].slot = static_cast<CombatSkillSlot>(255);
    ok &= rejects(invalid_slot, "unknown slot enums fail closed");
    return ok;
}

bool test_empty_loadout_preserves_legacy_checksum_domain() {
    auto empty_loadouts = actors();
    for (auto& actor : empty_loadouts) {
        actor.skill_loadout.fill({});
        actor.skill_loadout_count = 0;
    }
    const auto all_abilities = abilities();
    const std::array legacy_abilities{all_abilities[4]};

    DeterministicCombatResolver legacy;
    DeterministicCombatResolver with_unbound_skills;
    CollectingSink legacy_sink;
    CollectingSink unbound_sink;
    bool ok = legacy.initialize(empty_loadouts, legacy_abilities) == CombatError::none;
    ok &= with_unbound_skills.initialize(empty_loadouts, all_abilities) == CombatError::none;
    ok &= legacy.start() == CombatError::none;
    ok &= with_unbound_skills.start() == CombatError::none;
    ok &= expect(
        legacy.checksum() == with_unbound_skills.checksum(),
        "empty loadout does not activate a Skill checksum domain"
    );
    for (int tick = 1; tick <= 3; ++tick) {
        ok &= legacy.advance_one_tick(legacy_sink) == CombatError::none;
        ok &= with_unbound_skills.advance_one_tick(unbound_sink) == CombatError::none;
        ok &= expect(
            legacy.checksum() == with_unbound_skills.checksum(),
            "unbound Skill Definitions cannot perturb legacy Tick checksum"
        );
    }
    return ok;
}

bool test_defeat_retry_and_reinforcement_restore_owned_cooldowns() {
    auto actor_configs = actors();
    actor_configs[0].initial_resources.health = 20;
    actor_configs[0].initial_resources.health_max = 20;
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    bool ok = resolver.initialize(actor_configs, abilities()) == CombatError::none;
    ok &= resolver.start() == CombatError::none;
    for (int tick = 1; tick <= 3; ++tick) {
        ok &= resolver.advance_one_tick(sink) == CombatError::none;
    }

    sink.clear();
    const std::array defeat{
        CombatCommand{4, 1, 1, CombatCommandType::switch_stance, 0, flower_turn},
        skill_command(4, 1, 2, 2),
        skill_command(4, 2, 1, 1),
    };
    ok &= resolver.submit(defeat) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    const auto defeated_cooldown = resolver.query_skill_cooldown(
        1,
        CombatSkillSlot::primary,
        flower_skill
    );
    ok &= expect(
        resolver.actors()[0].defeated && !resolver.actors()[0].active &&
            resolver.actors()[0].active_ability == 0 &&
            defeated_cooldown.error == CombatSkillQueryError::none &&
            defeated_cooldown.ready_tick == 14,
        "defeat cancels the timeline but retains its committed owned cooldown"
    );

    ok &= resolver.retry_from_initial({4, 1, 100}, sink) == CombatError::none;
    const auto retried_primary = resolver.query_skill_cooldown(
        1,
        CombatSkillSlot::primary,
        player_eaves_skill
    );
    ok &= expect(
        resolver.actors()[0].active && !resolver.actors()[0].defeated &&
            retried_primary.error == CombatSkillQueryError::none &&
            retried_primary.ready_tick == 4,
        "retry restores current-stance bindings from the retry boundary Tick"
    );
    const std::array switch_after_retry{
        CombatCommand{5, 1, 101, CombatCommandType::switch_stance, 0, flower_turn},
    };
    ok &= resolver.submit(switch_after_retry) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        resolver.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            flower_skill
        ).ready_tick == 7,
        "retry reapplies the authored initial cooldown for another stance binding"
    );

    const auto base = actors();
    std::array<CombatActorConfig, 3> reinforcement_configs{
        base[0],
        base[1],
        base[1],
    };
    reinforcement_configs[2].actor = 3;
    reinforcement_configs[2].archetype_id =
        tgd::contracts::content_id("actor_skill_reinforcement");
    reinforcement_configs[2].initially_active = false;
    DeterministicCombatResolver reinforcement;
    CollectingSink reinforcement_sink;
    ok &= reinforcement.initialize(reinforcement_configs, abilities()) == CombatError::none;
    ok &= reinforcement.start() == CombatError::none;
    const std::array player_use{skill_command(1, 1, 1, 2)};
    ok &= reinforcement.submit(player_use) == CombatError::none;
    ok &= reinforcement.advance_one_tick(reinforcement_sink) == CombatError::none;
    const auto player_ready_before = reinforcement.query_skill_cooldown(
        1,
        CombatSkillSlot::primary,
        player_eaves_skill
    ).ready_tick;
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 1> placement{{
        {3, {3'000, 1'000, 0, 0}, 4},
    }};
    ok &= reinforcement.activate_group(
              {1, 1, 1, tgd::contracts::EncounterActivationMode::reinforce},
              placement,
              reinforcement_sink
          ) == CombatError::none;
    ok &= expect(
        reinforcement.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            player_eaves_skill
        ).ready_tick == player_ready_before &&
            reinforcement.query_skill_cooldown(
                3,
                CombatSkillSlot::primary,
                hostile_eaves_skill
            ).ready_tick == 1 &&
            reinforcement.actors()[2].active,
        "reinforcement resets only the restored actor's bindings"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_action_intent_maps_only_primary_slot();
    ok &= test_actor_ownership_stance_and_normalization();
    ok &= test_shared_definition_has_actor_local_cooldown();
    ok &= test_query_and_unbound_commands_fail_closed();
    ok &= test_invalid_loadouts_fail_initialization();
    ok &= test_empty_loadout_preserves_legacy_checksum_domain();
    ok &= test_defeat_retry_and_reinforcement_restore_owned_cooldowns();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
