#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace tgd::test {

enum class F1ContentReplayStepType : std::uint8_t {
    complete_objective,
    repeat_objective,
    advance_interaction,
    advance_combat,
    retry_safe_point,
};

struct F1ContentReplayStep final {
    F1ContentReplayStepType type{F1ContentReplayStepType::complete_objective};
    contracts::StableContentKey objective{};
    contracts::StableContentKey selection{};
    std::uint32_t ticks{};
};

struct F1ContentReplayFixture final {
    std::string_view id{};
    std::span<const F1ContentReplayStep> steps{};
    contracts::StableContentKey expected_arrival_clue{};
    contracts::StableContentKey expected_mooring_method{};
    contracts::StableContentKey expected_training_lane{};
    std::uint16_t expected_completed_objectives{};
    std::uint16_t expected_beat_index{};
    std::uint64_t expected_simulation_ticks{};
    std::uint64_t expected_eligible_ticks{};
    std::uint64_t expected_failure_retry_ticks{};
    std::uint64_t expected_quest_checksum{};
    std::uint64_t expected_quest_ui_checksum{};
    std::uint64_t expected_session_checksum{};
};

[[nodiscard]] F1ContentReplayFixture make_f1_high_water_windward_replay();
[[nodiscard]] F1ContentReplayFixture make_f1_follow_bell_leeward_retry_replay();

}  // namespace tgd::test
