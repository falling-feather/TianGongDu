#include <tgd/test/f1_golden_replay.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace tgd::test {
namespace {

[[nodiscard]] constexpr std::uint64_t fingerprint(std::string_view value) noexcept {
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    for (const auto character : value) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

[[nodiscard]] contracts::GroundVectorQ15 direction_for_tick(contracts::TickIndex tick) {
    constexpr std::int32_t diagonal = 23'170;
    constexpr std::array<contracts::GroundVectorQ15, 8> directions{{
        {contracts::ground_axis_one, 0},
        {diagonal, diagonal},
        {0, contracts::ground_axis_one},
        {-diagonal, diagonal},
        {-contracts::ground_axis_one, 0},
        {-diagonal, -diagonal},
        {0, -contracts::ground_axis_one},
        {diagonal, -diagonal},
    }};
    return directions[((tick - 1) / 125) % directions.size()];
}

}  // namespace

runtime::CommandReplay make_f1_golden_replay() {
    runtime::CommandReplay replay;
    replay.content_fingerprint = fingerprint("f1_rainy_umbrella_trial");
    replay.session_config.initial_pose.x = 1'250;
    replay.session_config.initial_pose.y = -2'500;
    replay.session_config.initial_pose.floor_layer = 2;
    replay.session_config.collision_radius = 100;
    replay.final_tick = 10'000;
    replay.expected_checksum = 0xfde5024f1286eeadULL;
    replay.commands.reserve(10'050);
    for (contracts::TickIndex tick = 1; tick <= replay.final_tick; ++tick) {
        replay.commands.push_back({
            {tick, replay.session_config.player_actor, tick * 2, contracts::SessionCommandType::move_intent},
            direction_for_tick(tick),
        });
        if (tick % 240 == 1 || tick == 9'991) {
            replay.commands.push_back({
                {tick,
                 replay.session_config.player_actor,
                 tick * 2 + 1,
                 contracts::SessionCommandType::jump_pressed},
                {},
            });
        }
    }
    return replay;
}

}  // namespace tgd::test
