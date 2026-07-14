#include <tgd/test/f1_content_replay.hpp>

#include <array>

namespace tgd::test {
namespace {

[[nodiscard]] constexpr F1ContentReplayStep complete(
    std::string_view objective,
    std::string_view selection = {}
) noexcept {
    return {
        F1ContentReplayStepType::complete_objective,
        contracts::stable_content_key(objective),
        selection.empty() ? 0 : contracts::stable_content_key(selection),
        0,
    };
}

[[nodiscard]] constexpr F1ContentReplayStep repeat(
    std::string_view objective,
    std::string_view selection = {}
) noexcept {
    return {
        F1ContentReplayStepType::repeat_objective,
        contracts::stable_content_key(objective),
        selection.empty() ? 0 : contracts::stable_content_key(selection),
        0,
    };
}

[[nodiscard]] constexpr F1ContentReplayStep advance(
    F1ContentReplayStepType type,
    std::uint32_t ticks
) noexcept {
    return {type, 0, 0, ticks};
}

[[nodiscard]] constexpr F1ContentReplayStep retry_safe_point() noexcept {
    return {F1ContentReplayStepType::retry_safe_point, 0, 0, 0};
}

constexpr std::array high_water_windward_steps{
    complete("f1_objective_inspect_travel_writ"),
    complete(
        "f1_objective_choose_arrival_clue",
        "f1_choice_arrival_high_water_tags"
    ),
    repeat(
        "f1_objective_choose_arrival_clue",
        "f1_choice_arrival_high_water_tags"
    ),
    complete("f1_objective_read_ferry_condition"),
    complete(
        "f1_objective_choose_mooring_method",
        "f1_choice_mooring_cross_belay"
    ),
    repeat(
        "f1_objective_choose_mooring_method",
        "f1_choice_mooring_cross_belay"
    ),
    complete("f1_objective_secure_ferry_mooring"),
    complete("f1_objective_inspect_bilge_counterweight"),
    complete("f1_objective_release_ferry_bilge"),
    complete("f1_objective_raise_wayfinding_lantern"),
    complete("f1_objective_read_workshop_bell_code"),
    complete("f1_objective_sound_workshop_bell"),
    advance(F1ContentReplayStepType::advance_interaction, 3),
    complete("f1_objective_reach_ferry_gate"),
    complete("f1_objective_meet_shen_yan"),
    complete(
        "f1_objective_choose_training_lane",
        "f1_choice_training_windward_lane"
    ),
    repeat(
        "f1_objective_choose_training_lane",
        "f1_choice_training_windward_lane"
    ),
    complete("f1_objective_take_eavesguard_mark"),
    complete("f1_objective_eavesguard_counter"),
    complete("f1_objective_commit_eavesguard_heavy"),
    complete("f1_objective_break_eavesguard_target"),
    complete("f1_objective_review_eavesguard_with_shen_yan"),
    complete("f1_objective_enter_flower_turn"),
    complete("f1_objective_cross_flower_turn_line"),
    complete("f1_objective_flower_turn_counter"),
    complete("f1_objective_commit_flower_turn_light"),
    complete("f1_objective_commit_flower_turn_heavy"),
    complete("f1_objective_break_flower_turn_target"),
    advance(F1ContentReplayStepType::advance_combat, 4),
    complete("f1_objective_finish_shen_yan_training"),
};

constexpr std::array follow_bell_leeward_retry_steps{
    complete("f1_objective_inspect_travel_writ"),
    complete(
        "f1_objective_choose_arrival_clue",
        "f1_choice_arrival_follow_bell"
    ),
    repeat(
        "f1_objective_choose_arrival_clue",
        "f1_choice_arrival_follow_bell"
    ),
    complete("f1_objective_read_ferry_condition"),
    complete(
        "f1_objective_choose_mooring_method",
        "f1_choice_mooring_quick_hitch"
    ),
    complete("f1_objective_secure_ferry_mooring"),
    repeat("f1_objective_secure_ferry_mooring"),
    complete("f1_objective_inspect_bilge_counterweight"),
    complete("f1_objective_release_ferry_bilge"),
    complete("f1_objective_raise_wayfinding_lantern"),
    complete("f1_objective_read_workshop_bell_code"),
    complete("f1_objective_sound_workshop_bell"),
    complete("f1_objective_reach_ferry_gate"),
    complete("f1_objective_meet_shen_yan"),
    complete(
        "f1_objective_choose_training_lane",
        "f1_choice_training_leeward_lane"
    ),
    complete("f1_objective_take_eavesguard_mark"),
    complete("f1_objective_eavesguard_counter"),
    advance(F1ContentReplayStepType::advance_combat, 2),
    retry_safe_point(),
    repeat("f1_objective_eavesguard_counter"),
    complete("f1_objective_commit_eavesguard_heavy"),
    complete("f1_objective_break_eavesguard_target"),
    complete("f1_objective_review_eavesguard_with_shen_yan"),
    complete("f1_objective_enter_flower_turn"),
    complete("f1_objective_cross_flower_turn_line"),
    complete("f1_objective_flower_turn_counter"),
    complete("f1_objective_commit_flower_turn_light"),
    complete("f1_objective_commit_flower_turn_heavy"),
    complete("f1_objective_break_flower_turn_target"),
    advance(F1ContentReplayStepType::advance_combat, 1),
    complete("f1_objective_finish_shen_yan_training"),
    repeat("f1_objective_finish_shen_yan_training"),
};

}  // namespace

F1ContentReplayFixture make_f1_high_water_windward_replay() {
    return {
        "f1_first_two_beats_high_water_windward",
        high_water_windward_steps,
        contracts::stable_content_key("f1_choice_arrival_high_water_tags"),
        contracts::stable_content_key("f1_choice_mooring_cross_belay"),
        contracts::stable_content_key("f1_choice_training_windward_lane"),
        25,
        2,
        7,
        7,
        0,
        0x70ad65be94540ee2ULL,
        0x42996cbdb4246c7cULL,
    };
}

F1ContentReplayFixture make_f1_follow_bell_leeward_retry_replay() {
    return {
        "f1_first_two_beats_follow_bell_leeward_retry",
        follow_bell_leeward_retry_steps,
        contracts::stable_content_key("f1_choice_arrival_follow_bell"),
        contracts::stable_content_key("f1_choice_mooring_quick_hitch"),
        contracts::stable_content_key("f1_choice_training_leeward_lane"),
        25,
        2,
        3,
        1,
        2,
        0x68cc30dc9b72e99eULL,
        0x13a2d7a427b77175ULL,
    };
}

}  // namespace tgd::test
