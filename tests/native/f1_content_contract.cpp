#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/content_definition.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unordered_set>

namespace {

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "F1 content contract failure: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    tgd::content::BuiltInF1ContentDefinitionProvider provider;
    const auto* definition = provider.find_vertical_slice(
        tgd::contracts::stable_content_key("f1_rainy_umbrella_trial")
    );
    bool ok = expect(definition != nullptr, "built-in provider resolves the F1 slice");
    if (definition == nullptr) {
        return EXIT_FAILURE;
    }

    ok &= expect(
        provider.find_vertical_slice(tgd::contracts::stable_content_key("missing_slice")) ==
            nullptr,
        "unknown content fails closed"
    );
    const auto* combat = provider.find_combat_encounter(
        tgd::contracts::stable_content_key("f1_encounter_umbrella_lane_bootstrap")
    );
    ok &= expect(combat != nullptr, "built-in provider resolves the F1 combat bootstrap");
    ok &= expect(
        provider.find_combat_encounter(tgd::contracts::stable_content_key("missing_encounter")) ==
            nullptr,
        "unknown combat content fails closed"
    );
    ok &= expect(
        definition->view_model == "2.5d-oblique-panoramic" &&
            definition->primary_guidance == "douzhanshen" &&
            definition->secondary_reference == "warm-snow-combat-readability",
        "Douzhanshen remains the first view and staging guide"
    );
    ok &= expect(
        definition->playable_target_minutes == 60 &&
            definition->end_to_end_test_budget_minutes == 70,
        "playable and E2E budgets stay separate"
    );
    ok &= expect(
        definition->beats.size() == 7 && definition->cell_ids.size() == 5 &&
            definition->enemy_family_ids.size() == 2,
        "the first route, cells, and enemy families are explicit"
    );
    ok &= expect(
        definition->quest_interactions.size() == 3 &&
            definition->quest_interactions.front().objective_id.key ==
                tgd::contracts::stable_content_key("f1_objective_inspect_travel_writ"),
        "the opening scene interactions are generated content, not presentation rules"
    );
    ok &= expect(
        definition->quest_combat_triggers.size() == 2 &&
            definition->quest_combat_triggers.front().required_stance ==
                tgd::contracts::stable_content_key("stance_eavesguard"),
        "training counters are generated combat-to-quest bindings"
    );

    std::uint32_t minutes = 0;
    std::unordered_set<tgd::contracts::StableContentKey> ids;
    for (const auto& beat : definition->beats) {
        minutes += beat.target_minutes;
        ok &= expect(beat.id.key != 0 && ids.insert(beat.id.key).second, "beat key is unique");
        ok &= expect(beat.cell_id.key != 0, "beat has an explicit cell");
        ok &= expect(!beat.objectives.empty(), "beat advances from objectives, not a timer");
        for (const auto& objective : beat.objectives) {
            ok &= expect(
                objective.key != 0 && ids.insert(objective.key).second,
                "objective key is unique"
            );
        }
    }
    ok &= expect(minutes == 60, "seven playable beat budgets total exactly 60 minutes");

    if (combat != nullptr) {
        ok &= expect(
            combat->actors.size() == 4 && combat->abilities.size() == 9,
            "the first player and enemy combat set is explicit"
        );
        ok &= expect(
            combat->director.player_actor == definition->player.actor &&
                combat->director.max_simultaneous_attackers == 1 &&
                combat->director.formation_radius_mm > 0 &&
                combat->director.chase_speed_mm_per_second % 60 == 0,
            "the encounter director has deterministic player and attack-token rules"
        );
        bool found_player = false;
        std::unordered_set<tgd::contracts::StableContentKey> ability_ids;
        for (const auto& actor : combat->actors) {
            found_player |= actor.actor == definition->player.actor &&
                            actor.faction == tgd::contracts::CombatFaction::player &&
                            actor.initial_pose == definition->player.initial_pose;
            ok &= expect(
                actor.stance_count > 0 && actor.initial_stance != 0,
                "each combat actor declares a stance"
            );
            ok &= expect(
                actor.recovery.stamina_delay_ticks > 0 &&
                    actor.recovery.stamina_interval_ticks > 0 &&
                    actor.recovery.stamina_per_interval > 0 &&
                    actor.recovery.poise_delay_ticks > 0 &&
                    actor.recovery.poise_interval_ticks > 0 &&
                    actor.recovery.poise_per_interval > 0,
                "each combat actor declares deterministic resource recovery"
            );
        }
        for (const auto& ability : combat->abilities) {
            ok &= expect(
                ability.id.key != 0 && ability_ids.insert(ability.id.key).second,
                "combat ability keys are unique"
            );
            ok &= expect(ability.active_ticks > 0, "ability has an authoritative active window");
        }
        ok &= expect(found_player, "combat and movement share the same player seed");
    }

    const auto& basis = definition->player.camera_basis;
    const auto dot = static_cast<std::int64_t>(basis.screen_right_world.x) *
                         basis.screen_forward_world.x +
                     static_cast<std::int64_t>(basis.screen_right_world.y) *
                         basis.screen_forward_world.y;
    const auto determinant = static_cast<std::int64_t>(basis.screen_right_world.x) *
                                 basis.screen_forward_world.y -
                             static_cast<std::int64_t>(basis.screen_right_world.y) *
                                 basis.screen_forward_world.x;
    ok &= expect(dot == 0 && determinant > 0, "camera-relative movement uses a right-handed oblique basis");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
