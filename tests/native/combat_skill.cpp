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
    const auto hostile_ready_before = reinforcement.query_skill_cooldown(
        2,
        CombatSkillSlot::primary,
        hostile_eaves_skill
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
                2,
                CombatSkillSlot::primary,
                hostile_eaves_skill
            ).ready_tick == hostile_ready_before &&
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

struct PlayerAbilityFixture final {
    CombatCommandType trigger{CombatCommandType::weapon_skill};
    std::int32_t stamina_cost{20};
    std::uint16_t windup_ticks{0};
    std::uint16_t hit_ticks{1};
    std::uint16_t recovery_ticks{2};
    std::int32_t horizontal_range_mm{1'200};
    std::int32_t vertical_range_mm{500};
    std::int32_t health_damage{25};
    std::int32_t guard_damage{20};
    std::uint32_t feedback{tgd::contracts::feedback_heavy};
    AbilityTargetPolicy target_policy{AbilityTargetPolicy::opposing_actor};
    std::uint16_t cooldown_ticks{6};
    std::uint16_t initial_cooldown_ticks{0};
};

[[nodiscard]] AbilityDefinition player_ability(PlayerAbilityFixture fixture = {}) {
    return {
        tgd::contracts::content_id("skill_player_eavesguard_pressure"),
        fixture.trigger,
        eavesguard,
        fixture.stamina_cost,
        fixture.windup_ticks,
        fixture.hit_ticks,
        fixture.recovery_ticks,
        fixture.horizontal_range_mm,
        fixture.vertical_range_mm,
        fixture.health_damage,
        fixture.guard_damage,
        fixture.feedback,
        fixture.target_policy,
        fixture.cooldown_ticks,
        fixture.initial_cooldown_ticks,
    };
}

[[nodiscard]] std::array<AbilityDefinition, 5> abilities_with_player(
    PlayerAbilityFixture fixture
) {
    auto result = abilities();
    result[0] = player_ability(fixture);
    return result;
}

[[nodiscard]] bool same_actor_resources(
    const DeterministicCombatResolver& left,
    const DeterministicCombatResolver& right
) {
    const auto left_actors = left.actors();
    const auto right_actors = right.actors();
    if (left_actors.size() != right_actors.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left_actors.size(); ++index) {
        if (!(left_actors[index].resources == right_actors[index].resources)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_cooldown_query(
    const tgd::contracts::CombatSkillCooldownResult& left,
    const tgd::contracts::CombatSkillCooldownResult& right
) {
    return left.error == right.error && left.ability == right.ability &&
           left.ready_tick == right.ready_tick;
}

template <std::size_t ActorCount, std::size_t AbilityCount>
bool expect_ignored_without_state_change(
    const std::array<CombatActorConfig, ActorCount>& actor_configs,
    const std::array<AbilityDefinition, AbilityCount>& ability_configs,
    const CombatCommand& command,
    tgd::contracts::StableContentKey expected_ability,
    std::string_view message
) {
    DeterministicCombatResolver observed;
    DeterministicCombatResolver control;
    CollectingSink observed_sink;
    CollectingSink control_sink;
    bool ok = observed.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= control.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= observed.start() == CombatError::none;
    ok &= control.start() == CombatError::none;
    if (!ok) {
        return expect(false, message);
    }

    const std::array batch{command};
    ok &= observed.submit(batch) == CombatError::none;
    ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
    ok &= control.advance_one_tick(control_sink) == CombatError::none;
    const auto observed_ready = observed.query_skill_cooldown(
        command.actor,
        command.skill_slot,
        expected_ability
    );
    const auto control_ready = control.query_skill_cooldown(
        command.actor,
        command.skill_slot,
        expected_ability
    );
    ok &= expect(
        observed_sink.count(CombatEventType::command_ignored) == 1 &&
            same_actor_resources(observed, control) &&
            same_cooldown_query(observed_ready, control_ready) &&
            observed.checksum() == control.checksum(),
        message
    );
    return ok;
}

bool test_cooldown_boundaries_and_initial_cooldown() {
    const auto actor_configs = single_binding_actors();
    const auto ability_configs = abilities();
    DeterministicCombatResolver observed;
    DeterministicCombatResolver control;
    CollectingSink observed_sink;
    CollectingSink control_sink;
    bool ok = observed.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= control.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= observed.start() == CombatError::none;
    ok &= control.start() == CombatError::none;

    const std::array first_use{skill_command(1, 1, 1, 2)};
    ok &= observed.submit(first_use) == CombatError::none;
    ok &= control.submit(first_use) == CombatError::none;
    ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
    ok &= control.advance_one_tick(control_sink) == CombatError::none;
    ok &= expect(
        observed.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            player_eaves_skill
        ).ready_tick == 7,
        "accepted skill commits its authored cooldown at start"
    );
    for (int tick = 2; tick <= 3; ++tick) {
        ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
        ok &= control.advance_one_tick(control_sink) == CombatError::none;
    }

    observed_sink.clear();
    const std::array early_use{skill_command(4, 1, 2, 2)};
    ok &= observed.submit(early_use) == CombatError::none;
    ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
    ok &= control.advance_one_tick(control_sink) == CombatError::none;
    ok &= expect(
        observed_sink.count(CombatEventType::command_ignored) == 1 &&
            same_actor_resources(observed, control) &&
            same_cooldown_query(
                observed.query_skill_cooldown(
                    1,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                ),
                control.query_skill_cooldown(
                    1,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                )
            ) &&
            observed.checksum() == control.checksum(),
        "early cooldown retry changes no resource, damage, ready tick, or checksum state"
    );
    for (int tick = 5; tick <= 6; ++tick) {
        ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
        ok &= control.advance_one_tick(control_sink) == CombatError::none;
    }

    observed_sink.clear();
    const auto source_before_exact = observed.actors()[0].resources;
    const auto target_before_exact = observed.actors()[1].resources;
    const std::array exact_use{skill_command(7, 1, 3, 2)};
    ok &= observed.submit(exact_use) == CombatError::none;
    ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
    ok &= expect(
        observed_sink.contains_ability(
            CombatEventType::ability_started,
            player_eaves_skill,
            1
        ) &&
            observed.actors()[0].resources.stamina == source_before_exact.stamina - 20 &&
            observed.actors()[1].resources.health == target_before_exact.health - 25 &&
            observed.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 13,
        "the exact ready Tick accepts and preserves authored stamina, damage, and cooldown"
    );

    PlayerAbilityFixture initial_fixture;
    initial_fixture.initial_cooldown_ticks = 3;
    const auto initial_abilities = abilities_with_player(initial_fixture);
    DeterministicCombatResolver initial;
    DeterministicCombatResolver initial_control;
    CollectingSink initial_sink;
    CollectingSink initial_control_sink;
    ok &= initial.initialize(actor_configs, initial_abilities) == CombatError::none;
    ok &= initial_control.initialize(actor_configs, initial_abilities) == CombatError::none;
    ok &= initial.start() == CombatError::none;
    ok &= initial_control.start() == CombatError::none;
    ok &= expect(
        initial.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            player_eaves_skill
        ).ready_tick == 3,
        "authored initial cooldown is visible before the first command"
    );
    const std::array initial_early{skill_command(1, 1, 1, 2)};
    ok &= initial.submit(initial_early) == CombatError::none;
    ok &= initial.advance_one_tick(initial_sink) == CombatError::none;
    ok &= initial_control.advance_one_tick(initial_control_sink) == CombatError::none;
    ok &= expect(
        initial_sink.count(CombatEventType::command_ignored) == 1 &&
            same_actor_resources(initial, initial_control) &&
            same_cooldown_query(
                initial.query_skill_cooldown(
                    1,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                ),
                initial_control.query_skill_cooldown(
                    1,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                )
            ) &&
            initial.checksum() == initial_control.checksum(),
        "initial cooldown rejects an early command without state mutation"
    );
    ok &= initial.advance_one_tick(initial_sink) == CombatError::none;
    ok &= initial_control.advance_one_tick(initial_control_sink) == CombatError::none;
    initial_sink.clear();
    const auto initial_source_before = initial.actors()[0].resources;
    const auto initial_target_before = initial.actors()[1].resources;
    const std::array initial_exact{skill_command(3, 1, 2, 2)};
    ok &= initial.submit(initial_exact) == CombatError::none;
    ok &= initial.advance_one_tick(initial_sink) == CombatError::none;
    ok &= expect(
        initial_sink.contains_ability(
            CombatEventType::ability_started,
            player_eaves_skill,
            1
        ) &&
            initial.actors()[0].resources.stamina == initial_source_before.stamina - 20 &&
            initial.actors()[1].resources.health == initial_target_before.health - 25 &&
            initial.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 9,
        "initial cooldown accepts at its exact ready Tick"
    );
    return ok;
}

bool test_resource_stance_and_target_failures_are_inert() {
    bool ok = true;
    auto insufficient = single_binding_actors();
    insufficient[0].initial_resources = {120, 120, 19, 19, 80, 80, 0, 0, 0};
    ok &= expect_ignored_without_state_change(
        insufficient,
        abilities(),
        skill_command(1, 1, 1, 2),
        player_eaves_skill,
        "insufficient stamina spends nothing, writes no cooldown, and deals no damage"
    );

    auto inactive = single_binding_actors();
    inactive[1].initially_active = false;
    ok &= expect_ignored_without_state_change(
        inactive,
        abilities(),
        skill_command(1, 1, 1, 2),
        player_eaves_skill,
        "inactive opposing target fails without resource, cooldown, or damage mutation"
    );

    auto wrong_faction = single_binding_actors();
    wrong_faction[1].faction = tgd::contracts::CombatFaction::player;
    ok &= expect_ignored_without_state_change(
        wrong_faction,
        abilities(),
        skill_command(1, 1, 1, 2),
        player_eaves_skill,
        "same-faction target fails opposing policy without state mutation"
    );

    DeterministicCombatResolver unknown;
    CollectingSink unknown_sink;
    const auto actor_configs = single_binding_actors();
    const auto ability_configs = abilities();
    ok &= unknown.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= unknown.start() == CombatError::none;
    const auto unknown_source_before = unknown.actors()[0].resources;
    const auto unknown_target_before = unknown.actors()[1].resources;
    const auto unknown_ready_before = unknown.query_skill_cooldown(
        1,
        CombatSkillSlot::primary,
        player_eaves_skill
    );
    const auto unknown_checksum_before = unknown.checksum();
    const std::array unknown_target{skill_command(1, 1, 1, 999)};
    ok &= expect(
        unknown.submit(unknown_target) == CombatError::invalid_command &&
            unknown.actors()[0].resources == unknown_source_before &&
            unknown.actors()[1].resources == unknown_target_before &&
            same_cooldown_query(
                unknown.query_skill_cooldown(
                    1,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                ),
                unknown_ready_before
            ) &&
            unknown.checksum() == unknown_checksum_before,
        "unknown target is rejected before resource, cooldown, damage, or checksum mutation"
    );

    DeterministicCombatResolver wrong_stance_observed;
    DeterministicCombatResolver wrong_stance_control;
    CollectingSink wrong_stance_sink;
    CollectingSink wrong_stance_control_sink;
    ok &= wrong_stance_observed.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= wrong_stance_control.initialize(actor_configs, ability_configs) == CombatError::none;
    ok &= wrong_stance_observed.start() == CombatError::none;
    ok &= wrong_stance_control.start() == CombatError::none;
    const std::array wrong_stance_commands{
        CombatCommand{1, 1, 1, CombatCommandType::switch_stance, 0, flower_turn},
        skill_command(1, 1, 2, 2),
    };
    const std::array stance_control{
        CombatCommand{1, 1, 1, CombatCommandType::switch_stance, 0, flower_turn},
    };
    ok &= wrong_stance_observed.submit(wrong_stance_commands) == CombatError::none;
    ok &= wrong_stance_control.submit(stance_control) == CombatError::none;
    ok &= wrong_stance_observed.advance_one_tick(wrong_stance_sink) == CombatError::none;
    ok &= wrong_stance_control.advance_one_tick(wrong_stance_control_sink) == CombatError::none;
    ok &= expect(
        wrong_stance_sink.count(CombatEventType::command_ignored) == 1 &&
            same_actor_resources(wrong_stance_observed, wrong_stance_control) &&
            wrong_stance_observed.query_skill_cooldown(
                1,
                CombatSkillSlot::primary
            ).error == CombatSkillQueryError::slot_unbound &&
            wrong_stance_observed.checksum() == wrong_stance_control.checksum(),
        "a stance with no owned slot cannot spend, damage, or create cooldown state"
    );
    const std::array switch_back{
        CombatCommand{2, 1, 3, CombatCommandType::switch_stance, 0, eavesguard},
    };
    ok &= wrong_stance_observed.submit(switch_back) == CombatError::none;
    ok &= wrong_stance_control.submit(switch_back) == CombatError::none;
    ok &= wrong_stance_observed.advance_one_tick(wrong_stance_sink) == CombatError::none;
    ok &= wrong_stance_control.advance_one_tick(wrong_stance_control_sink) == CombatError::none;
    ok &= expect(
        wrong_stance_observed.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            player_eaves_skill
        ).ready_tick == 0 &&
            wrong_stance_observed.checksum() == wrong_stance_control.checksum(),
        "returning to the bound stance proves wrong-stance failure wrote no cooldown"
    );

    PlayerAbilityFixture self_fixture;
    self_fixture.health_damage = 0;
    self_fixture.guard_damage = 0;
    self_fixture.target_policy = AbilityTargetPolicy::self_actor;
    const auto self_abilities = abilities_with_player(self_fixture);
    DeterministicCombatResolver self_positive;
    CollectingSink self_sink;
    ok &= self_positive.initialize(actor_configs, self_abilities) == CombatError::none;
    ok &= self_positive.start() == CombatError::none;
    const auto self_before = self_positive.actors()[0].resources;
    const std::array self_command{skill_command(1, 1, 1, 1)};
    ok &= self_positive.submit(self_command) == CombatError::none;
    ok &= self_positive.advance_one_tick(self_sink) == CombatError::none;
    ok &= expect(
        self_sink.contains_ability(
            CombatEventType::ability_started,
            player_eaves_skill,
            1
        ) &&
            self_positive.actors()[0].resources.stamina == self_before.stamina - 20 &&
            self_positive.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 7,
        "self_actor accepts only the source actor as its target"
    );
    ok &= expect_ignored_without_state_change(
        actor_configs,
        self_abilities,
        skill_command(1, 1, 1, 2),
        player_eaves_skill,
        "self_actor rejects another actor without state mutation"
    );

    PlayerAbilityFixture no_target_fixture;
    no_target_fixture.health_damage = 0;
    no_target_fixture.guard_damage = 0;
    no_target_fixture.target_policy = AbilityTargetPolicy::no_target;
    const auto no_target_abilities = abilities_with_player(no_target_fixture);
    DeterministicCombatResolver no_target_positive;
    CollectingSink no_target_sink;
    ok &= no_target_positive.initialize(actor_configs, no_target_abilities) == CombatError::none;
    ok &= no_target_positive.start() == CombatError::none;
    const auto no_target_before = no_target_positive.actors()[0].resources;
    const std::array no_target_command{skill_command(1, 1, 1, 0)};
    ok &= no_target_positive.submit(no_target_command) == CombatError::none;
    ok &= no_target_positive.advance_one_tick(no_target_sink) == CombatError::none;
    ok &= expect(
        no_target_sink.contains_ability(
            CombatEventType::ability_started,
            player_eaves_skill,
            1
        ) &&
            no_target_positive.actors()[0].resources.stamina == no_target_before.stamina - 20 &&
            no_target_positive.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 7,
        "no_target accepts an empty target and still commits authored cost and cooldown"
    );
    ok &= expect_ignored_without_state_change(
        actor_configs,
        no_target_abilities,
        skill_command(1, 1, 1, 2),
        player_eaves_skill,
        "no_target rejects a supplied target without state mutation"
    );
    return ok;
}

bool test_defeated_target_is_inert() {
    const auto base = single_binding_actors();
    std::array<CombatActorConfig, 3> actor_configs{base[0], base[1], base[0]};
    actor_configs[1].initial_resources.health = 20;
    actor_configs[1].initial_resources.health_max = 20;
    actor_configs[2].actor = 3;
    actor_configs[2].archetype_id = tgd::contracts::content_id("actor_skill_player_ally");
    DeterministicCombatResolver observed;
    DeterministicCombatResolver control;
    CollectingSink observed_sink;
    CollectingSink control_sink;
    bool ok = observed.initialize(actor_configs, abilities()) == CombatError::none;
    ok &= control.initialize(actor_configs, abilities()) == CombatError::none;
    ok &= observed.start() == CombatError::none;
    ok &= control.start() == CombatError::none;
    const std::array defeat{skill_command(1, 1, 1, 2)};
    ok &= observed.submit(defeat) == CombatError::none;
    ok &= control.submit(defeat) == CombatError::none;
    ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
    ok &= control.advance_one_tick(control_sink) == CombatError::none;
    ok &= expect(
        observed.actors()[1].defeated && control.actors()[1].defeated,
        "defeated-target fixture reaches a deterministic defeated state"
    );

    observed_sink.clear();
    const std::array defeated_target{skill_command(2, 3, 1, 2)};
    ok &= observed.submit(defeated_target) == CombatError::none;
    ok &= observed.advance_one_tick(observed_sink) == CombatError::none;
    ok &= control.advance_one_tick(control_sink) == CombatError::none;
    ok &= expect(
        observed_sink.count(CombatEventType::command_ignored) == 1 &&
            same_actor_resources(observed, control) &&
            same_cooldown_query(
                observed.query_skill_cooldown(
                    3,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                ),
                control.query_skill_cooldown(
                    3,
                    CombatSkillSlot::primary,
                    player_eaves_skill
                )
            ) &&
            observed.checksum() == control.checksum(),
        "defeated target cannot consume ally resources, start cooldown, or take more damage"
    );
    return ok;
}

bool test_authored_cost_windup_hit_and_damage() {
    PlayerAbilityFixture fixture;
    fixture.windup_ticks = 2;
    const auto actor_configs = single_binding_actors();
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    bool ok = resolver.initialize(actor_configs, abilities_with_player(fixture)) ==
              CombatError::none;
    ok &= resolver.start() == CombatError::none;
    const auto source_before = resolver.actors()[0].resources;
    const auto target_before = resolver.actors()[1].resources;
    const std::array command{skill_command(1, 1, 1, 2)};
    ok &= resolver.submit(command) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        sink.contains_ability(CombatEventType::ability_started, player_eaves_skill, 1) &&
            resolver.actors()[0].active_ability == player_eaves_skill &&
            resolver.actors()[0].resources.stamina == source_before.stamina - 20 &&
            resolver.actors()[1].resources.health == target_before.health &&
            resolver.query_skill_cooldown(
                1,
                CombatSkillSlot::primary,
                player_eaves_skill
            ).ready_tick == 7,
        "skill start spends exact stamina and honors a two-Tick windup before damage"
    );
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        resolver.actors()[1].resources.health == target_before.health,
        "the target remains unchanged through the full authored windup"
    );
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    ok &= expect(
        resolver.actors()[1].resources.health == target_before.health - 25,
        "authored hit Tick applies the exact health damage after windup completes"
    );
    return ok;
}

bool test_ability_definition_limits_fail_closed() {
    const auto actor_configs = single_binding_actors();
    bool ok = true;
    PlayerAbilityFixture maximum;
    maximum.stamina_cost = tgd::contracts::max_ability_stamina_cost;
    maximum.windup_ticks = tgd::contracts::max_ability_phase_ticks;
    maximum.hit_ticks = tgd::contracts::max_ability_phase_ticks;
    maximum.recovery_ticks = tgd::contracts::max_ability_phase_ticks;
    maximum.horizontal_range_mm = tgd::contracts::max_ability_range_mm;
    maximum.vertical_range_mm = tgd::contracts::max_ability_range_mm;
    maximum.health_damage = tgd::contracts::max_ability_damage;
    maximum.guard_damage = tgd::contracts::max_ability_damage;
    maximum.cooldown_ticks = tgd::contracts::max_ability_cooldown_ticks;
    maximum.initial_cooldown_ticks = tgd::contracts::max_ability_cooldown_ticks;
    DeterministicCombatResolver maximum_resolver;
    ok &= expect(
        maximum_resolver.initialize(actor_configs, abilities_with_player(maximum)) ==
            CombatError::none,
        "Ability numeric limits are inclusive"
    );

    std::array<PlayerAbilityFixture, 12> invalid{};
    invalid[0].target_policy = static_cast<AbilityTargetPolicy>(255);
    invalid[1].trigger = static_cast<CombatCommandType>(255);
    invalid[2].cooldown_ticks =
        static_cast<std::uint16_t>(tgd::contracts::max_ability_cooldown_ticks + 1);
    invalid[3].initial_cooldown_ticks =
        static_cast<std::uint16_t>(tgd::contracts::max_ability_cooldown_ticks + 1);
    invalid[4].windup_ticks =
        static_cast<std::uint16_t>(tgd::contracts::max_ability_phase_ticks + 1);
    invalid[5].hit_ticks =
        static_cast<std::uint16_t>(tgd::contracts::max_ability_phase_ticks + 1);
    invalid[6].recovery_ticks =
        static_cast<std::uint16_t>(tgd::contracts::max_ability_phase_ticks + 1);
    invalid[7].stamina_cost = tgd::contracts::max_ability_stamina_cost + 1;
    invalid[8].horizontal_range_mm = tgd::contracts::max_ability_range_mm + 1;
    invalid[9].vertical_range_mm = tgd::contracts::max_ability_range_mm + 1;
    invalid[10].health_damage = tgd::contracts::max_ability_damage + 1;
    invalid[11].guard_damage = tgd::contracts::max_ability_damage + 1;
    const std::array<std::string_view, 12> messages{
        "unknown Ability target policy fails closed",
        "unknown Ability trigger enum fails closed",
        "cooldown upper bound fails closed",
        "initial cooldown upper bound fails closed",
        "windup upper bound fails closed",
        "hit phase upper bound fails closed",
        "recovery upper bound fails closed",
        "stamina cost upper bound fails closed",
        "horizontal range upper bound fails closed",
        "vertical range upper bound fails closed",
        "health damage upper bound fails closed",
        "guard damage upper bound fails closed",
    };
    for (std::size_t index = 0; index < invalid.size(); ++index) {
        DeterministicCombatResolver resolver;
        ok &= expect(
            resolver.initialize(actor_configs, abilities_with_player(invalid[index])) ==
                CombatError::invalid_config,
            messages[index]
        );
    }
    return ok;
}

bool test_replace_rebuilds_encounter_boundary_cooldowns() {
    const auto base = actors();
    std::array<CombatActorConfig, 3> actor_configs{base[0], base[1], base[1]};
    actor_configs[2].actor = 3;
    actor_configs[2].archetype_id =
        tgd::contracts::content_id("actor_skill_replace_target");
    actor_configs[2].initially_active = false;
    DeterministicCombatResolver resolver;
    CollectingSink sink;
    bool ok = resolver.initialize(actor_configs, abilities()) == CombatError::none;
    ok &= resolver.start() == CombatError::none;
    const std::array use{skill_command(1, 1, 1, 2)};
    ok &= resolver.submit(use) == CombatError::none;
    ok &= resolver.advance_one_tick(sink) == CombatError::none;
    const std::array<tgd::contracts::EncounterActorPlacementDefinition, 1> placement{{
        {3, {3'000, 1'000, 0, 0}, 4},
    }};
    ok &= resolver.activate_group(
              {1, 1, 1, tgd::contracts::EncounterActivationMode::replace},
              placement,
              sink
          ) == CombatError::none;
    ok &= expect(
        resolver.query_skill_cooldown(
            1,
            CombatSkillSlot::primary,
            player_eaves_skill
        ).ready_tick == 1,
        "replace rebuilds player cooldown at the new Encounter boundary"
    );
    ok &= expect(
        resolver.query_skill_cooldown(
            2,
            CombatSkillSlot::primary,
            hostile_eaves_skill
        ).ready_tick == 1,
        "replace rebuilds cooldown for hostile actors entering isolation"
    );
    ok &= expect(
        resolver.query_skill_cooldown(
            3,
            CombatSkillSlot::primary,
            hostile_eaves_skill
        ).ready_tick == 1,
        "replace rebuilds cooldown for the hostile restored by the placement"
    );
    ok &= expect(
        resolver.actors()[0].active && !resolver.actors()[1].active &&
            resolver.actors()[1].resources.health == 0 &&
            resolver.actors()[2].active,
        "replace restores player, isolates unplaced hostile, and activates placement"
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
    ok &= test_cooldown_boundaries_and_initial_cooldown();
    ok &= test_resource_stance_and_target_failures_are_inert();
    ok &= test_defeated_target_is_inert();
    ok &= test_authored_cost_windup_hit_and_damage();
    ok &= test_ability_definition_limits_fail_closed();
    ok &= test_replace_rebuilds_encounter_boundary_cooldowns();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
