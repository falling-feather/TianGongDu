#pragma once

#include <tgd/contracts/combat_types.hpp>
#include <tgd/contracts/quest_ui.hpp>
#include <tgd/gameplay/quest_runtime.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class QuestUiProjectionError : std::uint8_t {
    none,
    invalid_lifecycle,
    invalid_definition,
    invalid_signal,
    invalid_snapshot,
    unknown_cue,
    ambiguous_cue,
    capacity_exceeded,
    sequence_overflow,
    missing_attempt_evidence,
};

enum class QuestUiSelectionIntentError : std::uint8_t {
    none,
    invalid_lifecycle,
    no_active_choice,
    invalid_projection,
    stale_projection,
    quest_context_changed,
    objective_mismatch,
    selection_not_authored,
    objective_not_active,
    selection_already_committed,
};

struct QuestUiProjectionResult final {
    QuestUiProjectionError error{QuestUiProjectionError::none};
    contracts::QuestUiProjectionSnapshot projection{};
};

class DeterministicQuestUiProjectionProducer final {
  public:
    static constexpr std::size_t cue_capacity = 32;

    [[nodiscard]] QuestUiProjectionError initialize(
        const contracts::VerticalSliceDefinition& definition
    ) noexcept;
    [[nodiscard]] QuestUiProjectionResult project(
        const contracts::QuestUiProjectionSignal& signal,
        const IQuestRuntime& quest,
        contracts::StableContentKey safe_point,
        std::span<const contracts::CombatActorSnapshot> actors
    ) noexcept;
    [[nodiscard]] QuestUiSelectionIntentError validate_choice_intent(
        const contracts::QuestUiSelectionIntent& intent,
        const IQuestRuntime& quest
    ) const noexcept;
    [[nodiscard]] bool has_authored_cue(
        contracts::StableContentKey beat,
        contracts::StableContentKey objective,
        contracts::QuestUiProjectionSource source
    ) const noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] bool has_projection() const noexcept;
    [[nodiscard]] const contracts::QuestUiProjectionSnapshot& snapshot() const noexcept;

  private:
    [[nodiscard]] QuestUiProjectionError validate_definition(
        const contracts::VerticalSliceDefinition& definition
    ) const noexcept;
    [[nodiscard]] const contracts::QuestUiCueDefinition* find_cue(
        contracts::StableContentKey beat,
        contracts::StableContentKey objective,
        contracts::QuestUiProjectionSource source,
        bool& ambiguous
    ) const noexcept;

    const contracts::VerticalSliceDefinition* definition_{};
    contracts::QuestUiProjectionSnapshot snapshot_{};
    std::uint64_t sequence_{};
    bool initialized_{};
    bool has_projection_{};
};

}  // namespace tgd::gameplay
