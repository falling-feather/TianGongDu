#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace tgd::contracts {

inline constexpr std::size_t quest_ui_choice_option_capacity = 8;
inline constexpr std::size_t quest_ui_selected_option_capacity = 16;
inline constexpr std::size_t quest_ui_actor_capacity = 16;
inline constexpr std::size_t quest_ui_retained_objective_capacity = 64;

enum class QuestUiSurface : std::uint8_t {
    gameplay,
    choice,
    failure,
};

enum class QuestUiObjectiveState : std::uint8_t {
    locked,
    active,
    completed,
};

enum class QuestUiResultStatus : std::uint8_t {
    not_applicable,
    accepted,
    rejected,
    ignored_repeat,
    pending,
};

enum class QuestUiRejectionReason : std::uint8_t {
    none,
    prerequisite_incomplete,
    selection_already_committed,
    wrong_target,
};

enum class QuestUiAttemptTimeClassification : std::uint8_t {
    unspecified,
    qualifying_first_visit,
    repeat_no_progress,
    qualifying_craft_decision,
    qualifying_error_feedback,
    qualifying_wrong_order_feedback,
    qualifying_craft_confirmation,
    qualifying_dialogue_decision,
    qualifying_training_risk,
    qualifying_combat_proof,
    qualifying_combat_feedback,
    failure_retry_excluded,
    resume_no_duplicate_progress,
};

struct QuestUiChoiceOption final {
    StableContentKey interaction{};
    StableContentKey selection{};

    [[nodiscard]] friend constexpr bool operator==(
        const QuestUiChoiceOption&,
        const QuestUiChoiceOption&
    ) noexcept = default;
};

struct QuestUiSelectedOption final {
    StableContentKey objective{};
    StableContentKey selection{};

    [[nodiscard]] friend constexpr bool operator==(
        const QuestUiSelectedOption&,
        const QuestUiSelectedOption&
    ) noexcept = default;
};

struct QuestUiResultSlot final {
    StableContentKey id{};
    StableContentKey objective{};
    QuestUiResultStatus status{QuestUiResultStatus::not_applicable};
    QuestUiRejectionReason rejection_reason{QuestUiRejectionReason::none};

    [[nodiscard]] friend constexpr bool operator==(
        const QuestUiResultSlot&,
        const QuestUiResultSlot&
    ) noexcept = default;
};

struct QuestUiProjectionSignal final {
    QuestUiProjectionSource source{QuestUiProjectionSource::objective_state};
    // objective is the projection focus. Result owners are explicit because a
    // completed result may transition focus to the next active objective.
    StableContentKey objective{};
    QuestUiResultSlot primary_result{};
    QuestUiResultSlot secondary_result{};
    QuestUiAttemptTimeClassification attempt_time_classification{
        QuestUiAttemptTimeClassification::unspecified
    };
};

struct QuestUiProjectionSnapshot final {
    std::uint64_t sequence{};
    TickIndex tick{};
    std::uint64_t quest_checksum{};
    StableContentKey cue{};
    StableContentKey beat{};
    StableContentKey objective{};
    StableContentKey safe_point{};
    StableContentKey pending_objective{};
    QuestUiResultSlot primary_result{};
    QuestUiResultSlot secondary_result{};
    QuestUiProjectionSource source{QuestUiProjectionSource::objective_state};
    QuestUiSurface surface{QuestUiSurface::gameplay};
    QuestUiPolarity polarity{QuestUiPolarity::positive};
    QuestUiObjectiveState objective_state{QuestUiObjectiveState::locked};
    QuestUiAttemptTimeClassification attempt_time_classification{
        QuestUiAttemptTimeClassification::unspecified
    };
    std::array<QuestUiChoiceOption, quest_ui_choice_option_capacity> choice_options{};
    std::uint8_t choice_option_count{};
    std::array<QuestUiSelectedOption, quest_ui_selected_option_capacity> selected_options{};
    std::uint8_t selected_option_count{};
    std::array<StableActorKey, quest_ui_actor_capacity> active_actor_keys{};
    std::uint8_t active_actor_count{};
    std::array<StableActorKey, quest_ui_actor_capacity> defeated_actor_keys{};
    std::uint8_t defeated_actor_count{};
    std::array<StableContentKey, quest_ui_retained_objective_capacity>
        retained_objectives{};
    std::uint8_t retained_objective_count{};
    std::uint64_t checksum{};

    [[nodiscard]] friend constexpr bool operator==(
        const QuestUiProjectionSnapshot&,
        const QuestUiProjectionSnapshot&
    ) noexcept = default;
};

struct QuestUiSelectionIntent final {
    std::uint64_t projection_sequence{};
    std::uint64_t projection_checksum{};
    StableContentKey objective{};
    StableContentKey interaction{};
    StableContentKey selection{};
};

}  // namespace tgd::contracts
