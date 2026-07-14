#pragma once

#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/quest_ui.hpp>
#include <tgd/contracts/quest_types.hpp>
#include <tgd/gameplay/quest_runtime.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class IF1QuestUiProjectionSink {
  public:
    virtual ~IF1QuestUiProjectionSink() = default;

    [[nodiscard]] virtual bool submitF1QuestUiProjection(
        const tgd::contracts::QuestUiProjectionSnapshot& projection
    ) noexcept = 0;
};

enum class F1QuestUiChoiceMode : std::uint8_t {
    none = 0,
    external,
    native,
};

enum class F1QuestUiChoiceError : std::uint8_t {
    none = 0,
    invalid_projection,
    already_pending,
    unavailable_in_mode,
    option_out_of_range,
    missing_presentation,
    stale_intent,
};

struct F1QuestUiChoiceIntentResult final {
    F1QuestUiChoiceError error{F1QuestUiChoiceError::none};
    tgd::contracts::QuestUiSelectionIntent intent{};
};

enum class F1QuestUiSignalEmitterError : std::uint8_t {
    none = 0,
    invalid_lifecycle,
    invalid_definition,
    invalid_result,
    invalid_quest_context,
};

struct F1QuestUiSignalEmission final {
    F1QuestUiSignalEmitterError error{F1QuestUiSignalEmitterError::none};
    bool found{};
    tgd::contracts::QuestUiProjectionSignal signal{};
};

// App-boundary signal assembly. This class maps already-authoritative Quest/Combat
// facts into raw projection slots; it never applies a Quest command and never reads
// Presentation copy, fixture data, or attempt-time classification.
class F1QuestUiSignalEmitter final {
  public:
    [[nodiscard]] F1QuestUiSignalEmitterError initialize(
        const tgd::contracts::VerticalSliceDefinition& definition
    ) noexcept {
        if (definition_ != nullptr) {
            return F1QuestUiSignalEmitterError::invalid_lifecycle;
        }
        if (definition.id.key == 0 || definition.player.actor == 0 ||
            definition.beats.empty()) {
            return F1QuestUiSignalEmitterError::invalid_definition;
        }
        for (std::size_t beat_index = 0; beat_index < definition.beats.size(); ++beat_index) {
            const auto& beat = definition.beats[beat_index];
            if (beat.id.key == 0 || beat.objectives.empty()) {
                return F1QuestUiSignalEmitterError::invalid_definition;
            }
            for (std::size_t objective_index = 0;
                 objective_index < beat.objectives.size();
                 ++objective_index) {
                if (beat.objectives[objective_index].key == 0) {
                    return F1QuestUiSignalEmitterError::invalid_definition;
                }
                for (std::size_t prior_beat = 0; prior_beat <= beat_index; ++prior_beat) {
                    const auto limit = prior_beat == beat_index
                                           ? objective_index
                                           : definition.beats[prior_beat].objectives.size();
                    for (std::size_t prior_objective = 0;
                         prior_objective < limit;
                         ++prior_objective) {
                        if (definition.beats[prior_beat]
                                .objectives[prior_objective]
                                .key == beat.objectives[objective_index].key) {
                            return F1QuestUiSignalEmitterError::invalid_definition;
                        }
                    }
                }
            }
        }
        definition_ = &definition;
        return F1QuestUiSignalEmitterError::none;
    }

    [[nodiscard]] constexpr bool initialized() const noexcept {
        return definition_ != nullptr;
    }

    [[nodiscard]] F1QuestUiSignalEmission interaction_feedback(
        const tgd::gameplay::QuestInteractionResult& interaction,
        bool quest_command_accepted,
        const tgd::gameplay::IQuestRuntime& quest
    ) const noexcept {
        using tgd::contracts::QuestUiProjectionSource;
        using tgd::contracts::QuestUiRejectionReason;
        using tgd::contracts::QuestUiResultStatus;
        using tgd::gameplay::QuestInteractionAvailability;
        using tgd::gameplay::QuestInteractionError;
        using tgd::gameplay::QuestObjectiveState;

        const auto* beat = current_beat(quest);
        if (definition_ == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_lifecycle, false, {}};
        }
        if (beat == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
        }
        const auto* authored = find_interaction(interaction.interaction);
        if (interaction.error != QuestInteractionError::none || !interaction.found ||
            authored == nullptr || authored->objective_id.key != interaction.objective ||
            authored->selection_id.key != interaction.selection ||
            authored->kind != interaction.kind ||
            !beat_owns_objective(*beat, interaction.objective)) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }

        auto status = QuestUiResultStatus::not_applicable;
        auto reason = QuestUiRejectionReason::none;
        const auto state = quest.objective_state(interaction.objective);
        switch (interaction.availability) {
            case QuestInteractionAvailability::eligible:
                if (!quest_command_accepted) {
                    return {};
                }
                if (state != QuestObjectiveState::completed) {
                    return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
                }
                status = QuestUiResultStatus::accepted;
                break;
            case QuestInteractionAvailability::prerequisite_incomplete:
                if (quest_command_accepted || state != QuestObjectiveState::active) {
                    return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
                }
                status = QuestUiResultStatus::rejected;
                reason = QuestUiRejectionReason::prerequisite_incomplete;
                break;
            case QuestInteractionAvailability::selection_already_committed:
                if (quest_command_accepted || authored->kind !=
                                                  tgd::contracts::QuestInteractionKind::choose ||
                    authored->selection_id.key == 0 ||
                    state != QuestObjectiveState::completed ||
                    quest.selected_option(interaction.objective) == 0) {
                    return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
                }
                status = QuestUiResultStatus::ignored_repeat;
                reason = QuestUiRejectionReason::selection_already_committed;
                break;
            default:
                return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }

        tgd::contracts::QuestUiProjectionSignal signal;
        signal.source = QuestUiProjectionSource::interaction_feedback;
        signal.objective = interaction.objective;
        signal.primary_result = {
            interaction.interaction,
            interaction.objective,
            status,
            reason,
        };
        return {F1QuestUiSignalEmitterError::none, true, signal};
    }

    [[nodiscard]] F1QuestUiSignalEmission accepted_choice_feedback(
        const tgd::contracts::QuestUiSelectionIntent& intent,
        const tgd::gameplay::IQuestRuntime& quest
    ) const noexcept {
        using tgd::gameplay::QuestObjectiveState;
        const auto* beat = current_beat(quest);
        if (definition_ == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_lifecycle, false, {}};
        }
        if (beat == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
        }
        const auto* interaction = find_interaction(intent.interaction);
        if (intent.objective == 0 || intent.interaction == 0 || intent.selection == 0 ||
            interaction == nullptr ||
            interaction->kind != tgd::contracts::QuestInteractionKind::choose ||
            interaction->objective_id.key != intent.objective ||
            interaction->selection_id.key != intent.selection ||
            quest.objective_state(intent.objective) != QuestObjectiveState::completed ||
            quest.selected_option(intent.objective) != intent.selection) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }
        const auto origin = std::find_if(
            beat->objectives.begin(),
            beat->objectives.end(),
            [&intent](const tgd::contracts::ContentId& objective) {
                return objective.key == intent.objective;
            }
        );
        if (origin == beat->objectives.end()) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }

        auto focus = intent.objective;
        const auto next = origin + 1;
        if (next != beat->objectives.end() &&
            quest.objective_state(next->key) == QuestObjectiveState::active) {
            focus = next->key;
        }
        tgd::contracts::QuestUiProjectionSignal signal;
        signal.source = tgd::contracts::QuestUiProjectionSource::interaction_feedback;
        signal.objective = focus;
        signal.primary_result = {
            intent.interaction,
            intent.objective,
            tgd::contracts::QuestUiResultStatus::accepted,
            tgd::contracts::QuestUiRejectionReason::none,
        };
        return {F1QuestUiSignalEmitterError::none, true, signal};
    }

    [[nodiscard]] F1QuestUiSignalEmission objective_state_after(
        const tgd::contracts::QuestEvent& event,
        const tgd::gameplay::IQuestRuntime& quest
    ) const noexcept {
        using tgd::gameplay::QuestObjectiveState;
        if (definition_ == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_lifecycle, false, {}};
        }
        if (event.type != tgd::contracts::QuestEventType::objective_completed ||
            event.quest != definition_->id.key) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }
        const auto* beat = current_beat(quest);
        if (beat == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
        }
        if (event.stage != beat->id.key) {
            return {};
        }
        const auto completed = std::find_if(
            beat->objectives.begin(),
            beat->objectives.end(),
            [&event](const tgd::contracts::ContentId& objective) {
                return objective.key == event.objective;
            }
        );
        if (completed == beat->objectives.end() ||
            quest.objective_state(event.objective) != QuestObjectiveState::completed) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }
        const auto next = completed + 1;
        if (next == beat->objectives.end() ||
            quest.objective_state(next->key) != QuestObjectiveState::active) {
            return {};
        }
        tgd::contracts::QuestUiProjectionSignal signal;
        signal.source = tgd::contracts::QuestUiProjectionSource::objective_state;
        signal.objective = next->key;
        return {F1QuestUiSignalEmitterError::none, true, signal};
    }

    [[nodiscard]] F1QuestUiSignalEmission combat_feedback(
        const tgd::gameplay::QuestCombatTriggerResult& accepted_trigger,
        const tgd::gameplay::QuestCombatOutcomeAttemptResult& outcome_attempt,
        const tgd::gameplay::IQuestRuntime& quest
    ) const noexcept {
        using tgd::gameplay::QuestCombatOutcomeAttemptDisposition;
        using tgd::gameplay::QuestCombatOutcomeAttemptError;
        using tgd::gameplay::QuestCombatTriggerError;
        using tgd::gameplay::QuestObjectiveState;

        const auto* beat = current_beat(quest);
        if (definition_ == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_lifecycle, false, {}};
        }
        if (beat == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
        }
        const auto* trigger = find_trigger(accepted_trigger.trigger);
        if (accepted_trigger.error != QuestCombatTriggerError::none ||
            !accepted_trigger.found || trigger == nullptr ||
            trigger->objective_id.key != accepted_trigger.objective ||
            !beat_owns_objective(*beat, accepted_trigger.objective) ||
            quest.objective_state(accepted_trigger.objective) !=
                QuestObjectiveState::completed ||
            outcome_attempt.error != QuestCombatOutcomeAttemptError::none) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }

        tgd::contracts::QuestUiProjectionSignal signal;
        signal.source = tgd::contracts::QuestUiProjectionSource::combat_feedback;
        signal.objective = accepted_trigger.objective;
        signal.primary_result = {
            accepted_trigger.trigger,
            accepted_trigger.objective,
            tgd::contracts::QuestUiResultStatus::accepted,
            tgd::contracts::QuestUiRejectionReason::none,
        };

        if (outcome_attempt.disposition ==
            QuestCombatOutcomeAttemptDisposition::no_candidate) {
            if (outcome_attempt.found || outcome_attempt.outcome != 0 ||
                outcome_attempt.objective != 0) {
                return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
            }
            return {F1QuestUiSignalEmitterError::none, true, signal};
        }

        const auto* outcome = find_outcome(outcome_attempt.outcome);
        if (!outcome_attempt.found || outcome == nullptr ||
            outcome->objective_id.key != outcome_attempt.objective ||
            !is_immediate_next(*beat, accepted_trigger.objective, outcome_attempt.objective) ||
            quest.objective_state(outcome_attempt.objective) != QuestObjectiveState::active) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }
        if (outcome_attempt.disposition ==
            QuestCombatOutcomeAttemptDisposition::target_matches_pending) {
            return {};
        }
        if (outcome_attempt.disposition != QuestCombatOutcomeAttemptDisposition::wrong_target) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }
        signal.objective = outcome_attempt.objective;
        signal.secondary_result = {
            outcome_attempt.outcome,
            outcome_attempt.objective,
            tgd::contracts::QuestUiResultStatus::rejected,
            tgd::contracts::QuestUiRejectionReason::wrong_target,
        };
        return {F1QuestUiSignalEmitterError::none, true, signal};
    }

    [[nodiscard]] F1QuestUiSignalEmission recovery(
        tgd::contracts::QuestUiProjectionSource source,
        const tgd::gameplay::IQuestRuntime& quest
    ) const noexcept {
        using tgd::contracts::QuestUiProjectionSource;
        using tgd::gameplay::QuestObjectiveState;
        if (definition_ == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_lifecycle, false, {}};
        }
        if (source != QuestUiProjectionSource::recovery_offer &&
            source != QuestUiProjectionSource::recovery_resume) {
            return {F1QuestUiSignalEmitterError::invalid_result, false, {}};
        }
        const auto* beat = current_beat(quest);
        if (beat == nullptr) {
            return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
        }
        for (const auto& objective : beat->objectives) {
            const auto state = quest.objective_state(objective.key);
            if (state == QuestObjectiveState::completed) {
                continue;
            }
            if (state != QuestObjectiveState::active) {
                return {F1QuestUiSignalEmitterError::invalid_quest_context, false, {}};
            }
            tgd::contracts::QuestUiProjectionSignal signal;
            signal.source = source;
            signal.objective = objective.key;
            return {F1QuestUiSignalEmitterError::none, true, signal};
        }
        return {};
    }

  private:
    [[nodiscard]] const tgd::contracts::VerticalSliceBeatDefinition* current_beat(
        const tgd::gameplay::IQuestRuntime& quest
    ) const noexcept {
        if (definition_ == nullptr) {
            return nullptr;
        }
        const auto& snapshot = quest.snapshot();
        if (snapshot.quest != definition_->id.key || snapshot.stage == 0) {
            return nullptr;
        }
        const auto beat = std::find_if(
            definition_->beats.begin(),
            definition_->beats.end(),
            [&snapshot](const tgd::contracts::VerticalSliceBeatDefinition& candidate) {
                return candidate.id.key == snapshot.stage;
            }
        );
        return beat == definition_->beats.end() ? nullptr : &*beat;
    }

    [[nodiscard]] static bool beat_owns_objective(
        const tgd::contracts::VerticalSliceBeatDefinition& beat,
        tgd::contracts::StableContentKey objective
    ) noexcept {
        return objective != 0 && std::any_of(
                   beat.objectives.begin(),
                   beat.objectives.end(),
                   [objective](const tgd::contracts::ContentId& candidate) {
                       return candidate.key == objective;
                   }
               );
    }

    [[nodiscard]] static bool is_immediate_next(
        const tgd::contracts::VerticalSliceBeatDefinition& beat,
        tgd::contracts::StableContentKey origin,
        tgd::contracts::StableContentKey next
    ) noexcept {
        const auto found = std::find_if(
            beat.objectives.begin(),
            beat.objectives.end(),
            [origin](const tgd::contracts::ContentId& objective) {
                return objective.key == origin;
            }
        );
        return found != beat.objectives.end() && found + 1 != beat.objectives.end() &&
               (found + 1)->key == next;
    }

    [[nodiscard]] const tgd::contracts::QuestInteractionDefinition* find_interaction(
        tgd::contracts::StableContentKey id
    ) const noexcept {
        if (definition_ == nullptr || id == 0) {
            return nullptr;
        }
        const auto found = std::find_if(
            definition_->quest_interactions.begin(),
            definition_->quest_interactions.end(),
            [id](const tgd::contracts::QuestInteractionDefinition& interaction) {
                return interaction.id.key == id;
            }
        );
        return found == definition_->quest_interactions.end() ? nullptr : &*found;
    }

    [[nodiscard]] const tgd::contracts::QuestCombatTriggerDefinition* find_trigger(
        tgd::contracts::StableContentKey id
    ) const noexcept {
        if (definition_ == nullptr || id == 0) {
            return nullptr;
        }
        const auto found = std::find_if(
            definition_->quest_combat_triggers.begin(),
            definition_->quest_combat_triggers.end(),
            [id](const tgd::contracts::QuestCombatTriggerDefinition& trigger) {
                return trigger.id.key == id;
            }
        );
        return found == definition_->quest_combat_triggers.end() ? nullptr : &*found;
    }

    [[nodiscard]] const tgd::contracts::QuestCombatOutcomeDefinition* find_outcome(
        tgd::contracts::StableContentKey id
    ) const noexcept {
        if (definition_ == nullptr || id == 0) {
            return nullptr;
        }
        const auto found = std::find_if(
            definition_->quest_combat_outcomes.begin(),
            definition_->quest_combat_outcomes.end(),
            [id](const tgd::contracts::QuestCombatOutcomeDefinition& outcome) {
                return outcome.id.key == id;
            }
        );
        return found == definition_->quest_combat_outcomes.end() ? nullptr : &*found;
    }

    const tgd::contracts::VerticalSliceDefinition* definition_{};
};

[[nodiscard]] constexpr std::string_view f1QuestUiNativeChoiceLabel(
    tgd::contracts::StableContentKey selection
) noexcept {
    using tgd::contracts::stable_content_key;
    if (selection == stable_content_key("f1_choice_arrival_high_water_tags")) {
        return "High-water marks";
    }
    if (selection == stable_content_key("f1_choice_arrival_drowned_manifest")) {
        return "Drowned manifest";
    }
    if (selection == stable_content_key("f1_choice_arrival_follow_bell")) {
        return "Follow the bell";
    }
    if (selection == stable_content_key("f1_choice_mooring_cross_belay")) {
        return "Cross-belay";
    }
    if (selection == stable_content_key("f1_choice_mooring_quick_hitch")) {
        return "Quick hitch";
    }
    if (selection == stable_content_key("f1_choice_training_windward_lane")) {
        return "Windward lane";
    }
    if (selection == stable_content_key("f1_choice_training_leeward_lane")) {
        return "Leeward lane";
    }
    return {};
}

class F1QuestUiChoiceState final {
  public:
    [[nodiscard]] F1QuestUiChoiceError begin(
        const tgd::contracts::QuestUiProjectionSnapshot& projection,
        bool external_consumer_accepted
    ) noexcept {
        using tgd::contracts::QuestUiProjectionSource;
        using tgd::contracts::QuestUiSurface;
        if (mode_ != F1QuestUiChoiceMode::none) {
            return F1QuestUiChoiceError::already_pending;
        }
        if (projection.sequence == 0 || projection.checksum == 0 ||
            projection.objective == 0 ||
            projection.source != QuestUiProjectionSource::choice_available ||
            projection.surface != QuestUiSurface::choice ||
            projection.choice_option_count == 0 ||
            projection.choice_option_count > projection.choice_options.size()) {
            return F1QuestUiChoiceError::invalid_projection;
        }
        for (std::size_t index = 0; index < projection.choice_option_count; ++index) {
            const auto& option = projection.choice_options[index];
            if (option.interaction == 0 || option.selection == 0) {
                return F1QuestUiChoiceError::invalid_projection;
            }
            labels_[index] = f1QuestUiNativeChoiceLabel(option.selection);
            if (!external_consumer_accepted && labels_[index].empty()) {
                labels_ = {};
                return F1QuestUiChoiceError::missing_presentation;
            }
        }
        projection_ = projection;
        mode_ = external_consumer_accepted ? F1QuestUiChoiceMode::external
                                           : F1QuestUiChoiceMode::native;
        return F1QuestUiChoiceError::none;
    }

    [[nodiscard]] constexpr bool pending() const noexcept {
        return mode_ != F1QuestUiChoiceMode::none;
    }

    [[nodiscard]] constexpr bool native_pending() const noexcept {
        return mode_ == F1QuestUiChoiceMode::native;
    }

    [[nodiscard]] constexpr F1QuestUiChoiceMode mode() const noexcept {
        return mode_;
    }

    [[nodiscard]] constexpr std::size_t option_count() const noexcept {
        return projection_.choice_option_count;
    }

    [[nodiscard]] constexpr std::string_view option_label(
        std::size_t index
    ) const noexcept {
        return native_pending() && index < option_count() ? labels_[index]
                                                          : std::string_view{};
    }

    [[nodiscard]] F1QuestUiChoiceIntentResult native_intent(
        std::size_t index
    ) const noexcept {
        if (!native_pending()) {
            return {F1QuestUiChoiceError::unavailable_in_mode, {}};
        }
        if (index >= option_count()) {
            return {F1QuestUiChoiceError::option_out_of_range, {}};
        }
        const auto& option = projection_.choice_options[index];
        return {
            F1QuestUiChoiceError::none,
            {
                projection_.sequence,
                projection_.checksum,
                projection_.objective,
                option.interaction,
                option.selection,
            },
        };
    }

    [[nodiscard]] bool matches(
        const tgd::contracts::QuestUiSelectionIntent& intent
    ) const noexcept {
        if (!pending() || intent.projection_sequence != projection_.sequence ||
            intent.projection_checksum != projection_.checksum ||
            intent.objective != projection_.objective) {
            return false;
        }
        for (std::size_t index = 0; index < option_count(); ++index) {
            const auto& option = projection_.choice_options[index];
            if (intent.interaction == option.interaction &&
                intent.selection == option.selection) {
                return true;
            }
        }
        return false;
    }

    void finish() noexcept {
        mode_ = F1QuestUiChoiceMode::none;
        projection_ = {};
        labels_ = {};
    }

  private:
    F1QuestUiChoiceMode mode_{F1QuestUiChoiceMode::none};
    tgd::contracts::QuestUiProjectionSnapshot projection_{};
    std::array<std::string_view, tgd::contracts::quest_ui_choice_option_capacity> labels_{};
};
