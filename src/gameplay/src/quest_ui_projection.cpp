#include <tgd/gameplay/quest_ui_projection.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;
constexpr std::uint16_t all_projection_sources =
    contracts::quest_ui_projection_source_bit(
        contracts::QuestUiProjectionSource::choice_available
    ) |
    contracts::quest_ui_projection_source_bit(
        contracts::QuestUiProjectionSource::interaction_feedback
    ) |
    contracts::quest_ui_projection_source_bit(
        contracts::QuestUiProjectionSource::objective_state
    ) |
    contracts::quest_ui_projection_source_bit(
        contracts::QuestUiProjectionSource::combat_feedback
    ) |
    contracts::quest_ui_projection_source_bit(
        contracts::QuestUiProjectionSource::recovery_offer
    ) |
    contracts::quest_ui_projection_source_bit(
        contracts::QuestUiProjectionSource::recovery_resume
    );

template <typename Integer, bool IsEnum = std::is_enum_v<Integer>>
struct HashIntegerType final {
    using type = Integer;
};

template <typename Integer>
struct HashIntegerType<Integer, true> final {
    using type = std::underlying_type_t<Integer>;
};

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    static_assert(std::is_integral_v<Integer> || std::is_enum_v<Integer>);
    using Raw = typename HashIntegerType<Integer>::type;
    using Unsigned = std::make_unsigned_t<Raw>;
    auto bits = static_cast<std::uint64_t>(
        static_cast<Unsigned>(static_cast<Raw>(value))
    );
    for (std::size_t index = 0; index < sizeof(Raw); ++index) {
        hash ^= static_cast<std::uint8_t>(bits & 0xffULL);
        hash *= fnv_prime;
        bits >>= 8U;
    }
}

[[nodiscard]] bool empty_content_id(const contracts::ContentId& id) noexcept {
    return id.key == 0 && id.name.empty();
}

[[nodiscard]] bool valid_content_id(const contracts::ContentId& id) noexcept {
    return id.key != 0 && !id.name.empty() &&
           contracts::stable_content_key(id.name) == id.key;
}

[[nodiscard]] const contracts::VerticalSliceBeatDefinition* find_beat(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey beat
) noexcept {
    const auto found = std::find_if(
        definition.beats.begin(),
        definition.beats.end(),
        [beat](const contracts::VerticalSliceBeatDefinition& candidate) {
            return candidate.id.key == beat;
        }
    );
    return found == definition.beats.end() ? nullptr : &*found;
}

[[nodiscard]] const contracts::VerticalSliceBeatDefinition* find_objective_beat(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey objective
) noexcept {
    for (const auto& beat : definition.beats) {
        if (std::any_of(
                beat.objectives.begin(),
                beat.objectives.end(),
                [objective](const contracts::ContentId& candidate) {
                    return candidate.key == objective;
                }
            )) {
            return &beat;
        }
    }
    return nullptr;
}

[[nodiscard]] bool beat_has_objective(
    const contracts::VerticalSliceBeatDefinition& beat,
    contracts::StableContentKey objective
) noexcept {
    return std::any_of(
        beat.objectives.begin(),
        beat.objectives.end(),
        [objective](const contracts::ContentId& candidate) {
            return candidate.key == objective;
        }
    );
}

[[nodiscard]] std::size_t objective_position(
    const contracts::VerticalSliceBeatDefinition& beat,
    contracts::StableContentKey objective
) noexcept {
    for (std::size_t index = 0; index < beat.objectives.size(); ++index) {
        if (beat.objectives[index].key == objective) {
            return index;
        }
    }
    return beat.objectives.size();
}

[[nodiscard]] bool directly_precedes(
    const contracts::VerticalSliceBeatDefinition& beat,
    contracts::StableContentKey previous,
    contracts::StableContentKey current
) noexcept {
    const auto previous_position = objective_position(beat, previous);
    const auto current_position = objective_position(beat, current);
    return previous_position < beat.objectives.size() &&
           current_position < beat.objectives.size() &&
           previous_position + 1 == current_position;
}

[[nodiscard]] const contracts::VerticalSliceSafePointDefinition* find_safe_point(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey safe_point
) noexcept {
    const auto found = std::find_if(
        definition.safe_points.begin(),
        definition.safe_points.end(),
        [safe_point](const contracts::VerticalSliceSafePointDefinition& candidate) {
            return candidate.id.key == safe_point;
        }
    );
    return found == definition.safe_points.end() ? nullptr : &*found;
}

[[nodiscard]] const contracts::QuestInteractionDefinition* find_interaction(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey interaction
) noexcept {
    const auto found = std::find_if(
        definition.quest_interactions.begin(),
        definition.quest_interactions.end(),
        [interaction](const contracts::QuestInteractionDefinition& candidate) {
            return candidate.id.key == interaction;
        }
    );
    return found == definition.quest_interactions.end() ? nullptr : &*found;
}

[[nodiscard]] const contracts::QuestCombatTriggerDefinition* find_combat_trigger(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey trigger
) noexcept {
    const auto found = std::find_if(
        definition.quest_combat_triggers.begin(),
        definition.quest_combat_triggers.end(),
        [trigger](const contracts::QuestCombatTriggerDefinition& candidate) {
            return candidate.id.key == trigger;
        }
    );
    return found == definition.quest_combat_triggers.end() ? nullptr : &*found;
}

[[nodiscard]] const contracts::QuestCombatOutcomeDefinition* find_combat_outcome(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey outcome
) noexcept {
    const auto found = std::find_if(
        definition.quest_combat_outcomes.begin(),
        definition.quest_combat_outcomes.end(),
        [outcome](const contracts::QuestCombatOutcomeDefinition& candidate) {
            return candidate.id.key == outcome;
        }
    );
    return found == definition.quest_combat_outcomes.end() ? nullptr : &*found;
}

[[nodiscard]] bool objective_sets_overlap(
    std::span<const contracts::ContentId> left,
    std::span<const contracts::ContentId> right
) noexcept {
    if (left.empty() || right.empty()) {
        return true;
    }
    return std::any_of(left.begin(), left.end(), [right](const contracts::ContentId& value) {
        return std::any_of(
            right.begin(),
            right.end(),
            [&value](const contracts::ContentId& candidate) {
                return candidate.key == value.key;
            }
        );
    });
}

[[nodiscard]] bool selection_is_authored(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableContentKey objective,
    contracts::StableContentKey selection
) noexcept {
    return std::any_of(
        definition.quest_interactions.begin(),
        definition.quest_interactions.end(),
        [objective, selection](const contracts::QuestInteractionDefinition& interaction) {
            return interaction.kind == contracts::QuestInteractionKind::choose &&
                   interaction.objective_id.key == objective &&
                   interaction.selection_id.key == selection;
        }
    );
}

[[nodiscard]] bool result_not_applicable(
    const contracts::QuestUiResultSlot& result
) noexcept {
    return result.status == contracts::QuestUiResultStatus::not_applicable &&
           result.id == 0 && result.objective == 0 &&
           result.rejection_reason == contracts::QuestUiRejectionReason::none;
}

[[nodiscard]] bool result_shape_valid(
    const contracts::QuestUiResultSlot& result
) noexcept {
    const bool known_reason =
        result.rejection_reason == contracts::QuestUiRejectionReason::none ||
        result.rejection_reason ==
            contracts::QuestUiRejectionReason::prerequisite_incomplete ||
        result.rejection_reason ==
            contracts::QuestUiRejectionReason::selection_already_committed ||
        result.rejection_reason == contracts::QuestUiRejectionReason::wrong_target;
    if (!known_reason) {
        return false;
    }
    if (result.status == contracts::QuestUiResultStatus::not_applicable) {
        return result_not_applicable(result);
    }
    if (result.id == 0 || result.objective == 0) {
        return false;
    }
    switch (result.status) {
        case contracts::QuestUiResultStatus::accepted:
        case contracts::QuestUiResultStatus::pending:
            return result.rejection_reason == contracts::QuestUiRejectionReason::none;
        case contracts::QuestUiResultStatus::rejected:
            return result.rejection_reason != contracts::QuestUiRejectionReason::none;
        case contracts::QuestUiResultStatus::ignored_repeat:
            return result.rejection_reason ==
                   contracts::QuestUiRejectionReason::selection_already_committed;
        case contracts::QuestUiResultStatus::not_applicable:
            break;
        default:
            break;
    }
    return false;
}

[[nodiscard]] bool known_attempt_time_classification(
    contracts::QuestUiAttemptTimeClassification classification
) noexcept {
    switch (classification) {
        case contracts::QuestUiAttemptTimeClassification::qualifying_first_visit:
        case contracts::QuestUiAttemptTimeClassification::repeat_no_progress:
        case contracts::QuestUiAttemptTimeClassification::qualifying_craft_decision:
        case contracts::QuestUiAttemptTimeClassification::qualifying_error_feedback:
        case contracts::QuestUiAttemptTimeClassification::qualifying_wrong_order_feedback:
        case contracts::QuestUiAttemptTimeClassification::qualifying_craft_confirmation:
        case contracts::QuestUiAttemptTimeClassification::qualifying_dialogue_decision:
        case contracts::QuestUiAttemptTimeClassification::qualifying_training_risk:
        case contracts::QuestUiAttemptTimeClassification::qualifying_combat_proof:
        case contracts::QuestUiAttemptTimeClassification::qualifying_combat_feedback:
        case contracts::QuestUiAttemptTimeClassification::failure_retry_excluded:
        case contracts::QuestUiAttemptTimeClassification::resume_no_duplicate_progress:
            return true;
        case contracts::QuestUiAttemptTimeClassification::unspecified:
        default:
            return false;
    }
}

[[nodiscard]] bool attempt_time_classification_matches_source(
    contracts::QuestUiProjectionSource source,
    contracts::QuestUiAttemptTimeClassification classification
) noexcept {
    using Attempt = contracts::QuestUiAttemptTimeClassification;
    switch (source) {
        case contracts::QuestUiProjectionSource::choice_available:
            return classification == Attempt::qualifying_first_visit ||
                   classification == Attempt::qualifying_craft_decision ||
                   classification == Attempt::qualifying_dialogue_decision;
        case contracts::QuestUiProjectionSource::interaction_feedback:
            return classification == Attempt::repeat_no_progress ||
                   classification == Attempt::qualifying_craft_decision ||
                   classification == Attempt::qualifying_error_feedback ||
                   classification == Attempt::qualifying_wrong_order_feedback ||
                   classification == Attempt::qualifying_craft_confirmation;
        case contracts::QuestUiProjectionSource::objective_state:
            return classification == Attempt::qualifying_first_visit ||
                   classification == Attempt::qualifying_training_risk;
        case contracts::QuestUiProjectionSource::combat_feedback:
            return classification == Attempt::qualifying_combat_proof ||
                   classification == Attempt::qualifying_combat_feedback;
        case contracts::QuestUiProjectionSource::recovery_offer:
            return classification == Attempt::failure_retry_excluded;
        case contracts::QuestUiProjectionSource::recovery_resume:
            return classification == Attempt::resume_no_duplicate_progress;
        default:
            return false;
    }
}

[[nodiscard]] bool signal_shape_valid(
    const contracts::QuestUiProjectionSignal& signal
) noexcept {
    if (signal.objective == 0 ||
        contracts::quest_ui_projection_source_bit(signal.source) == 0 ||
        !result_shape_valid(signal.primary_result) ||
        !result_shape_valid(signal.secondary_result) ||
        !known_attempt_time_classification(signal.attempt_time_classification) ||
        !attempt_time_classification_matches_source(
            signal.source,
            signal.attempt_time_classification
        )) {
        return false;
    }
    const bool repeat_time = signal.attempt_time_classification ==
                             contracts::QuestUiAttemptTimeClassification::repeat_no_progress;
    const bool failure_time = signal.attempt_time_classification ==
                              contracts::QuestUiAttemptTimeClassification::failure_retry_excluded;
    const bool resume_time = signal.attempt_time_classification ==
                             contracts::QuestUiAttemptTimeClassification::resume_no_duplicate_progress;
    if ((signal.primary_result.status == contracts::QuestUiResultStatus::ignored_repeat) !=
            repeat_time ||
        (signal.source == contracts::QuestUiProjectionSource::recovery_offer) != failure_time ||
        (signal.source == contracts::QuestUiProjectionSource::recovery_resume) != resume_time) {
        return false;
    }
    switch (signal.source) {
        case contracts::QuestUiProjectionSource::choice_available:
        case contracts::QuestUiProjectionSource::objective_state:
        case contracts::QuestUiProjectionSource::recovery_offer:
        case contracts::QuestUiProjectionSource::recovery_resume:
            return result_not_applicable(signal.primary_result) &&
                   result_not_applicable(signal.secondary_result);
        case contracts::QuestUiProjectionSource::interaction_feedback:
            return signal.primary_result.status !=
                       contracts::QuestUiResultStatus::not_applicable &&
                   signal.primary_result.status != contracts::QuestUiResultStatus::pending &&
                   result_not_applicable(signal.secondary_result);
        case contracts::QuestUiProjectionSource::combat_feedback:
            if (signal.primary_result.status ==
                    contracts::QuestUiResultStatus::not_applicable ||
                signal.primary_result.status ==
                    contracts::QuestUiResultStatus::ignored_repeat ||
                signal.secondary_result.status ==
                    contracts::QuestUiResultStatus::ignored_repeat) {
                return false;
            }
            return result_not_applicable(signal.secondary_result) ||
                   signal.primary_result.status == contracts::QuestUiResultStatus::accepted;
        default:
            return false;
    }
}

[[nodiscard]] contracts::QuestUiObjectiveState map_objective_state(
    QuestObjectiveState state,
    bool& valid
) noexcept {
    valid = true;
    switch (state) {
        case QuestObjectiveState::locked:
            return contracts::QuestUiObjectiveState::locked;
        case QuestObjectiveState::active:
            return contracts::QuestUiObjectiveState::active;
        case QuestObjectiveState::completed:
            return contracts::QuestUiObjectiveState::completed;
        default:
            valid = false;
            return contracts::QuestUiObjectiveState::locked;
    }
}

[[nodiscard]] contracts::QuestUiSurface surface_for(
    contracts::QuestUiProjectionSource source
) noexcept {
    if (source == contracts::QuestUiProjectionSource::choice_available) {
        return contracts::QuestUiSurface::choice;
    }
    if (source == contracts::QuestUiProjectionSource::recovery_offer) {
        return contracts::QuestUiSurface::failure;
    }
    return contracts::QuestUiSurface::gameplay;
}

[[nodiscard]] bool result_is_negative(
    const contracts::QuestUiResultSlot& result
) noexcept {
    return result.status == contracts::QuestUiResultStatus::rejected ||
           result.status == contracts::QuestUiResultStatus::ignored_repeat;
}

[[nodiscard]] const contracts::QuestUiResultSelectorDefinition* find_result_selector(
    const contracts::QuestUiCueDefinition& cue,
    const contracts::QuestUiProjectionSignal& signal
) noexcept {
    const auto primary = signal.primary_result.id;
    const auto secondary = signal.secondary_result.id;
    const auto found = std::find_if(
        cue.result_selectors.begin(),
        cue.result_selectors.end(),
        [&signal, primary, secondary](
            const contracts::QuestUiResultSelectorDefinition& selector
        ) {
            return selector.source == signal.source &&
                   selector.objective_id.key == signal.objective &&
                   selector.primary_result_id.key == primary &&
                   selector.secondary_result_id.key == secondary;
        }
    );
    return found == cue.result_selectors.end() ? nullptr : &*found;
}

[[nodiscard]] bool hostile_actor_is_authored(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableActorKey actor
) noexcept {
    for (const auto& activation : definition.quest_encounter_activations) {
        if (std::find(activation.actor_keys.begin(), activation.actor_keys.end(), actor) !=
            activation.actor_keys.end()) {
            return true;
        }
    }
    return std::any_of(
        definition.quest_boss_phases.begin(),
        definition.quest_boss_phases.end(),
        [actor](const contracts::QuestBossPhaseDefinition& phase) {
            return phase.actor == actor;
        }
    );
}

[[nodiscard]] bool validate_result_semantics(
    const contracts::VerticalSliceDefinition& definition,
    const contracts::VerticalSliceBeatDefinition& beat,
    const contracts::QuestUiCueDefinition& cue,
    const contracts::QuestUiProjectionSignal& signal,
    const IQuestRuntime& quest,
    contracts::StableContentKey pending_objective,
    contracts::QuestUiPolarity& polarity
) noexcept {
    if (signal.source == contracts::QuestUiProjectionSource::recovery_offer ||
        signal.source == contracts::QuestUiProjectionSource::recovery_resume) {
        polarity = contracts::QuestUiPolarity::recovery;
        return true;
    }
    if (signal.source == contracts::QuestUiProjectionSource::choice_available ||
        signal.source == contracts::QuestUiProjectionSource::objective_state) {
        polarity = contracts::QuestUiPolarity::positive;
        return true;
    }

    const auto* selector = find_result_selector(cue, signal);
    if (signal.source == contracts::QuestUiProjectionSource::interaction_feedback) {
        const auto* interaction = find_interaction(definition, signal.primary_result.id);
        if (interaction == nullptr ||
            interaction->objective_id.key != signal.primary_result.objective ||
            find_objective_beat(definition, interaction->objective_id.key) != &beat) {
            return false;
        }
        if (interaction->objective_id.key != signal.objective) {
            if (signal.primary_result.status != contracts::QuestUiResultStatus::accepted ||
                interaction->kind != contracts::QuestInteractionKind::choose ||
                interaction->selection_id.key == 0 || selector == nullptr ||
                quest.objective_state(interaction->objective_id.key) !=
                    QuestObjectiveState::completed ||
                quest.selected_option(interaction->objective_id.key) !=
                    interaction->selection_id.key ||
                quest.objective_state(signal.objective) != QuestObjectiveState::active ||
                pending_objective != signal.objective ||
                !directly_precedes(
                    beat,
                    interaction->objective_id.key,
                    signal.objective
                )) {
                return false;
            }
        }
    } else if (signal.source == contracts::QuestUiProjectionSource::combat_feedback) {
        const auto* trigger = find_combat_trigger(definition, signal.primary_result.id);
        if (trigger == nullptr ||
            trigger->objective_id.key != signal.primary_result.objective ||
            find_objective_beat(definition, trigger->objective_id.key) != &beat) {
            return false;
        }
        if (result_not_applicable(signal.secondary_result)) {
            if (trigger->objective_id.key != signal.objective) {
                return false;
            }
        } else {
            const auto* outcome = find_combat_outcome(definition, signal.secondary_result.id);
            if (outcome == nullptr ||
                outcome->objective_id.key != signal.secondary_result.objective ||
                outcome->objective_id.key != signal.objective ||
                find_objective_beat(definition, outcome->objective_id.key) != &beat) {
                return false;
            }
            if (trigger->objective_id.key != signal.objective &&
                (selector == nullptr ||
                 quest.objective_state(trigger->objective_id.key) !=
                     QuestObjectiveState::completed ||
                 !directly_precedes(
                     beat,
                     trigger->objective_id.key,
                     signal.objective
                 ))) {
                return false;
            }
        }
    } else {
        return false;
    }

    const auto& effective_result = result_not_applicable(signal.secondary_result)
                                       ? signal.primary_result
                                       : signal.secondary_result;
    if (result_is_negative(effective_result)) {
        polarity = contracts::QuestUiPolarity::negative;
    } else if (selector != nullptr &&
               selector->polarity_override ==
                   contracts::QuestUiPolarityOverride::negative) {
        polarity = contracts::QuestUiPolarity::negative;
    } else {
        polarity = contracts::QuestUiPolarity::positive;
    }
    return polarity != contracts::QuestUiPolarity::recovery;
}

void hash_result_slot(
    std::uint64_t& hash,
    const contracts::QuestUiResultSlot& result
) noexcept {
    hash_integer(hash, result.id);
    hash_integer(hash, result.objective);
    hash_integer(hash, result.status);
    hash_integer(hash, result.rejection_reason);
}

void update_checksum(contracts::QuestUiProjectionSnapshot& projection) noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, projection.sequence);
    hash_integer(hash, projection.tick);
    hash_integer(hash, projection.quest_checksum);
    hash_integer(hash, projection.cue);
    hash_integer(hash, projection.beat);
    hash_integer(hash, projection.objective);
    hash_integer(hash, projection.safe_point);
    hash_integer(hash, projection.pending_objective);
    hash_result_slot(hash, projection.primary_result);
    hash_result_slot(hash, projection.secondary_result);
    hash_integer(hash, projection.source);
    hash_integer(hash, projection.surface);
    hash_integer(hash, projection.polarity);
    hash_integer(hash, projection.objective_state);
    hash_integer(hash, projection.attempt_time_classification);
    hash_integer(hash, projection.choice_option_count);
    for (std::size_t index = 0; index < projection.choice_option_count; ++index) {
        hash_integer(hash, projection.choice_options[index].interaction);
        hash_integer(hash, projection.choice_options[index].selection);
    }
    hash_integer(hash, projection.selected_option_count);
    for (std::size_t index = 0; index < projection.selected_option_count; ++index) {
        hash_integer(hash, projection.selected_options[index].objective);
        hash_integer(hash, projection.selected_options[index].selection);
    }
    hash_integer(hash, projection.active_actor_count);
    for (std::size_t index = 0; index < projection.active_actor_count; ++index) {
        hash_integer(hash, projection.active_actor_keys[index]);
    }
    hash_integer(hash, projection.defeated_actor_count);
    for (std::size_t index = 0; index < projection.defeated_actor_count; ++index) {
        hash_integer(hash, projection.defeated_actor_keys[index]);
    }
    hash_integer(hash, projection.retained_objective_count);
    for (std::size_t index = 0; index < projection.retained_objective_count; ++index) {
        hash_integer(hash, projection.retained_objectives[index]);
    }
    projection.checksum = hash;
}

}  // namespace

QuestUiProjectionError DeterministicQuestUiProjectionProducer::initialize(
    const contracts::VerticalSliceDefinition& definition
) noexcept {
    if (initialized_) {
        return QuestUiProjectionError::invalid_lifecycle;
    }
    const auto validation = validate_definition(definition);
    if (validation != QuestUiProjectionError::none) {
        return validation;
    }
    definition_ = &definition;
    snapshot_ = {};
    sequence_ = 0;
    has_projection_ = false;
    initialized_ = true;
    return QuestUiProjectionError::none;
}

QuestUiProjectionResult DeterministicQuestUiProjectionProducer::project(
    const contracts::QuestUiProjectionSignal& signal,
    const IQuestRuntime& quest,
    contracts::StableContentKey safe_point,
    std::span<const contracts::CombatActorSnapshot> actors
) noexcept {
    if (!initialized_ || definition_ == nullptr) {
        return {QuestUiProjectionError::invalid_lifecycle, {}};
    }
    if (!signal_shape_valid(signal)) {
        return {QuestUiProjectionError::invalid_signal, {}};
    }
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        return {QuestUiProjectionError::sequence_overflow, {}};
    }

    const auto& quest_snapshot = quest.snapshot();
    const auto* beat = find_beat(*definition_, quest_snapshot.stage);
    const auto* safe_point_definition = find_safe_point(*definition_, safe_point);
    if (quest_snapshot.quest != definition_->id.key || beat == nullptr ||
        !beat_has_objective(*beat, signal.objective) || safe_point_definition == nullptr ||
        safe_point_definition->beat_id.key != beat->id.key) {
        return {QuestUiProjectionError::invalid_snapshot, {}};
    }

    contracts::QuestUiProjectionSnapshot next;
    for (const auto& authored_beat : definition_->beats) {
        for (const auto& objective : authored_beat.objectives) {
            bool state_valid = false;
            const auto state = map_objective_state(
                quest.objective_state(objective.key),
                state_valid
            );
            if (!state_valid) {
                return {QuestUiProjectionError::invalid_snapshot, {}};
            }
            if (state == contracts::QuestUiObjectiveState::active) {
                if (authored_beat.id.key != beat->id.key) {
                    return {QuestUiProjectionError::invalid_snapshot, {}};
                }
            }
            if (state == contracts::QuestUiObjectiveState::completed) {
                if (next.retained_objective_count >= next.retained_objectives.size()) {
                    return {QuestUiProjectionError::capacity_exceeded, {}};
                }
                next.retained_objectives[next.retained_objective_count++] = objective.key;
            }
            const auto selection = quest.selected_option(objective.key);
            if (selection == 0) {
                continue;
            }
            if (state != contracts::QuestUiObjectiveState::completed ||
                !selection_is_authored(*definition_, objective.key, selection)) {
                return {QuestUiProjectionError::invalid_snapshot, {}};
            }
            if (next.selected_option_count >= next.selected_options.size()) {
                return {QuestUiProjectionError::capacity_exceeded, {}};
            }
            next.selected_options[next.selected_option_count++] = {objective.key, selection};
        }
    }

    bool focus_state_valid = false;
    const auto focus_state = map_objective_state(
        quest.objective_state(signal.objective),
        focus_state_valid
    );
    const auto pending_objective =
        focus_state == contracts::QuestUiObjectiveState::active ? signal.objective : 0;
    if (!focus_state_valid ||
        ((signal.source == contracts::QuestUiProjectionSource::choice_available ||
          signal.source == contracts::QuestUiProjectionSource::recovery_offer ||
          signal.source == contracts::QuestUiProjectionSource::recovery_resume) &&
         (focus_state != contracts::QuestUiObjectiveState::active ||
          pending_objective != signal.objective))) {
        return {QuestUiProjectionError::invalid_snapshot, {}};
    }

    bool ambiguous = false;
    const auto* cue = find_cue(beat->id.key, signal.objective, signal.source, ambiguous);
    if (ambiguous) {
        return {QuestUiProjectionError::ambiguous_cue, {}};
    }
    if (cue == nullptr) {
        return {QuestUiProjectionError::unknown_cue, {}};
    }
    contracts::QuestUiPolarity polarity{contracts::QuestUiPolarity::positive};
    if (!validate_result_semantics(
            *definition_,
            *beat,
            *cue,
            signal,
            quest,
            pending_objective,
            polarity
        )) {
        return {QuestUiProjectionError::invalid_signal, {}};
    }

    next.sequence = sequence_ + 1;
    next.tick = quest_snapshot.tick;
    next.quest_checksum = quest_snapshot.checksum;
    next.cue = cue->cue_id.key;
    next.beat = beat->id.key;
    next.objective = signal.objective;
    next.safe_point = safe_point;
    next.pending_objective = pending_objective;
    next.primary_result = signal.primary_result;
    next.secondary_result = signal.secondary_result;
    next.source = signal.source;
    next.surface = surface_for(signal.source);
    next.polarity = polarity;
    next.objective_state = focus_state;
    next.attempt_time_classification = signal.attempt_time_classification;

    if (signal.source == contracts::QuestUiProjectionSource::choice_available) {
        if (quest.selected_option(signal.objective) != 0) {
            return {QuestUiProjectionError::invalid_snapshot, {}};
        }
        for (const auto& interaction : definition_->quest_interactions) {
            if (interaction.kind != contracts::QuestInteractionKind::choose ||
                interaction.objective_id.key != signal.objective) {
                continue;
            }
            if (interaction.id.key == 0 || interaction.selection_id.key == 0) {
                return {QuestUiProjectionError::invalid_definition, {}};
            }
            if (next.choice_option_count >= next.choice_options.size()) {
                return {QuestUiProjectionError::capacity_exceeded, {}};
            }
            next.choice_options[next.choice_option_count++] = {
                interaction.id.key,
                interaction.selection_id.key,
            };
        }
        if (next.choice_option_count == 0) {
            return {QuestUiProjectionError::invalid_definition, {}};
        }
    }

    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        const bool authored_player = actor.actor == definition_->player.actor;
        const bool authored_hostile = hostile_actor_is_authored(*definition_, actor.actor);
        if (actor.actor == 0 || authored_player == authored_hostile ||
            (authored_player && actor.faction != contracts::CombatFaction::player) ||
            (authored_hostile && actor.faction != contracts::CombatFaction::hostile) ||
            (actor.active && actor.defeated)) {
            return {QuestUiProjectionError::invalid_snapshot, {}};
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (actors[prior].actor == actor.actor) {
                return {QuestUiProjectionError::invalid_snapshot, {}};
            }
        }
        if (!authored_hostile) {
            continue;
        }
        if (actor.active) {
            if (next.active_actor_count >= next.active_actor_keys.size()) {
                return {QuestUiProjectionError::capacity_exceeded, {}};
            }
            next.active_actor_keys[next.active_actor_count++] = actor.actor;
        }
        if (actor.defeated) {
            if (next.defeated_actor_count >= next.defeated_actor_keys.size()) {
                return {QuestUiProjectionError::capacity_exceeded, {}};
            }
            next.defeated_actor_keys[next.defeated_actor_count++] = actor.actor;
        }
    }
    std::sort(
        next.active_actor_keys.begin(),
        next.active_actor_keys.begin() + next.active_actor_count
    );
    std::sort(
        next.defeated_actor_keys.begin(),
        next.defeated_actor_keys.begin() + next.defeated_actor_count
    );
    update_checksum(next);

    snapshot_ = next;
    sequence_ = next.sequence;
    has_projection_ = true;
    return {QuestUiProjectionError::none, next};
}

QuestUiSelectionIntentError DeterministicQuestUiProjectionProducer::validate_choice_intent(
    const contracts::QuestUiSelectionIntent& intent,
    const IQuestRuntime& quest
) const noexcept {
    if (!initialized_ || definition_ == nullptr) {
        return QuestUiSelectionIntentError::invalid_lifecycle;
    }
    if (!has_projection_ || snapshot_.surface != contracts::QuestUiSurface::choice ||
        snapshot_.source != contracts::QuestUiProjectionSource::choice_available) {
        return QuestUiSelectionIntentError::no_active_choice;
    }
    const auto& quest_snapshot = quest.snapshot();
    if (quest_snapshot.quest != definition_->id.key ||
        quest_snapshot.stage != snapshot_.beat ||
        quest_snapshot.checksum != snapshot_.quest_checksum) {
        return QuestUiSelectionIntentError::quest_context_changed;
    }
    if (intent.projection_sequence == 0 || intent.projection_sequence != snapshot_.sequence ||
        intent.projection_checksum == 0 ||
        intent.projection_checksum != snapshot_.checksum) {
        return QuestUiSelectionIntentError::stale_projection;
    }
    if (intent.objective == 0 || intent.objective != snapshot_.objective) {
        return QuestUiSelectionIntentError::objective_mismatch;
    }
    const auto* beat = find_beat(*definition_, snapshot_.beat);
    if (beat == nullptr || !beat_has_objective(*beat, snapshot_.objective) ||
        snapshot_.choice_option_count == 0 ||
        snapshot_.choice_option_count > snapshot_.choice_options.size()) {
        return QuestUiSelectionIntentError::invalid_projection;
    }

    std::size_t authored_option_count{};
    for (const auto& interaction : definition_->quest_interactions) {
        if (interaction.kind == contracts::QuestInteractionKind::choose &&
            interaction.objective_id.key == snapshot_.objective) {
            ++authored_option_count;
        }
    }
    if (authored_option_count != snapshot_.choice_option_count) {
        return QuestUiSelectionIntentError::invalid_projection;
    }
    for (std::size_t index = 0; index < snapshot_.choice_option_count; ++index) {
        const auto& option = snapshot_.choice_options[index];
        const auto* interaction = find_interaction(*definition_, option.interaction);
        if (option.interaction == 0 || option.selection == 0 || interaction == nullptr ||
            interaction->kind != contracts::QuestInteractionKind::choose ||
            interaction->objective_id.key != snapshot_.objective ||
            interaction->selection_id.key != option.selection ||
            find_objective_beat(*definition_, interaction->objective_id.key) != beat) {
            return QuestUiSelectionIntentError::invalid_projection;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (snapshot_.choice_options[prior].selection == option.selection) {
                return QuestUiSelectionIntentError::invalid_projection;
            }
        }
    }

    const bool authored = intent.interaction != 0 && intent.selection != 0 && std::any_of(
        snapshot_.choice_options.begin(),
        snapshot_.choice_options.begin() + snapshot_.choice_option_count,
        [&intent](const contracts::QuestUiChoiceOption& option) {
            return option.interaction == intent.interaction &&
                   option.selection == intent.selection;
        }
    );
    if (!authored) {
        return QuestUiSelectionIntentError::selection_not_authored;
    }
    if (quest.selected_option(intent.objective) != 0) {
        return QuestUiSelectionIntentError::selection_already_committed;
    }
    if (quest.objective_state(intent.objective) != QuestObjectiveState::active) {
        return QuestUiSelectionIntentError::objective_not_active;
    }
    return QuestUiSelectionIntentError::none;
}

bool DeterministicQuestUiProjectionProducer::initialized() const noexcept {
    return initialized_;
}

bool DeterministicQuestUiProjectionProducer::has_projection() const noexcept {
    return has_projection_;
}

const contracts::QuestUiProjectionSnapshot&
DeterministicQuestUiProjectionProducer::snapshot() const noexcept {
    return snapshot_;
}

QuestUiProjectionError DeterministicQuestUiProjectionProducer::validate_definition(
    const contracts::VerticalSliceDefinition& definition
) const noexcept {
    if (!valid_content_id(definition.id) || definition.player.actor == 0 ||
        definition.beats.empty() ||
        definition.safe_points.empty() || definition.quest_ui_cues.empty() ||
        definition.quest_ui_cues.size() > cue_capacity) {
        return QuestUiProjectionError::invalid_definition;
    }

    for (std::size_t beat_index = 0; beat_index < definition.beats.size(); ++beat_index) {
        const auto& beat = definition.beats[beat_index];
        if (!valid_content_id(beat.id) || beat.objectives.empty()) {
            return QuestUiProjectionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < beat_index; ++prior) {
            if (definition.beats[prior].id.key == beat.id.key) {
                return QuestUiProjectionError::invalid_definition;
            }
        }
        for (std::size_t objective_index = 0;
             objective_index < beat.objectives.size();
             ++objective_index) {
            const auto& objective = beat.objectives[objective_index];
            if (!valid_content_id(objective)) {
                return QuestUiProjectionError::invalid_definition;
            }
            for (std::size_t earlier_beat = 0; earlier_beat <= beat_index; ++earlier_beat) {
                const auto limit = earlier_beat == beat_index
                                       ? objective_index
                                       : definition.beats[earlier_beat].objectives.size();
                for (std::size_t earlier_objective = 0;
                     earlier_objective < limit;
                     ++earlier_objective) {
                    if (definition.beats[earlier_beat]
                            .objectives[earlier_objective]
                            .key == objective.key) {
                        return QuestUiProjectionError::invalid_definition;
                    }
                }
            }
        }
    }

    for (std::size_t index = 0; index < definition.safe_points.size(); ++index) {
        const auto& safe_point = definition.safe_points[index];
        if (!valid_content_id(safe_point.id) || !valid_content_id(safe_point.beat_id) ||
            find_beat(definition, safe_point.beat_id.key) == nullptr) {
            return QuestUiProjectionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definition.safe_points[prior].id.key == safe_point.id.key) {
                return QuestUiProjectionError::invalid_definition;
            }
        }
    }

    for (const auto& activation : definition.quest_encounter_activations) {
        for (const auto actor : activation.actor_keys) {
            if (actor == 0 || actor == definition.player.actor) {
                return QuestUiProjectionError::invalid_definition;
            }
        }
    }
    for (const auto& phase : definition.quest_boss_phases) {
        if (phase.actor == 0 || phase.actor == definition.player.actor) {
            return QuestUiProjectionError::invalid_definition;
        }
    }

    for (std::size_t index = 0; index < definition.quest_interactions.size(); ++index) {
        const auto& interaction = definition.quest_interactions[index];
        if (!valid_content_id(interaction.id) ||
            !valid_content_id(interaction.objective_id) ||
            find_objective_beat(definition, interaction.objective_id.key) == nullptr) {
            return QuestUiProjectionError::invalid_definition;
        }
        const bool choose = interaction.kind == contracts::QuestInteractionKind::choose;
        if ((choose && !valid_content_id(interaction.selection_id)) ||
            (!choose && !empty_content_id(interaction.selection_id))) {
            return QuestUiProjectionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            const auto& existing = definition.quest_interactions[prior];
            if (existing.id.key == interaction.id.key ||
                (choose && existing.kind == contracts::QuestInteractionKind::choose &&
                 existing.objective_id.key == interaction.objective_id.key &&
                 existing.selection_id.key == interaction.selection_id.key)) {
                return QuestUiProjectionError::invalid_definition;
            }
        }
    }

    for (std::size_t index = 0; index < definition.quest_combat_triggers.size(); ++index) {
        const auto& trigger = definition.quest_combat_triggers[index];
        if (!valid_content_id(trigger.id) || !valid_content_id(trigger.objective_id) ||
            find_objective_beat(definition, trigger.objective_id.key) == nullptr) {
            return QuestUiProjectionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definition.quest_combat_triggers[prior].id.key == trigger.id.key) {
                return QuestUiProjectionError::invalid_definition;
            }
        }
    }
    for (std::size_t index = 0; index < definition.quest_combat_outcomes.size(); ++index) {
        const auto& outcome = definition.quest_combat_outcomes[index];
        if (!valid_content_id(outcome.id) || !valid_content_id(outcome.objective_id) ||
            find_objective_beat(definition, outcome.objective_id.key) == nullptr) {
            return QuestUiProjectionError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definition.quest_combat_outcomes[prior].id.key == outcome.id.key) {
                return QuestUiProjectionError::invalid_definition;
            }
        }
    }

    for (std::size_t index = 0; index < definition.quest_ui_cues.size(); ++index) {
        const auto& cue = definition.quest_ui_cues[index];
        const auto* beat = find_beat(definition, cue.beat_id.key);
        if (!valid_content_id(cue.cue_id) || !valid_content_id(cue.beat_id) ||
            beat == nullptr || cue.source_mask == 0 ||
            (cue.source_mask & ~all_projection_sources) != 0 ||
            cue.objective_ids.size() > contracts::quest_ui_cue_objective_capacity ||
            cue.result_selectors.size() > contracts::quest_ui_result_selector_capacity) {
            return QuestUiProjectionError::invalid_definition;
        }
        for (std::size_t objective = 0; objective < cue.objective_ids.size(); ++objective) {
            if (!valid_content_id(cue.objective_ids[objective]) ||
                !beat_has_objective(*beat, cue.objective_ids[objective].key)) {
                return QuestUiProjectionError::invalid_definition;
            }
            for (std::size_t prior = 0; prior < objective; ++prior) {
                if (cue.objective_ids[prior].key == cue.objective_ids[objective].key) {
                    return QuestUiProjectionError::invalid_definition;
                }
            }
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            const auto& existing = definition.quest_ui_cues[prior];
            if (existing.cue_id.key == cue.cue_id.key ||
                (existing.beat_id.key == cue.beat_id.key &&
                 (existing.source_mask & cue.source_mask) != 0 &&
                 objective_sets_overlap(existing.objective_ids, cue.objective_ids))) {
                return QuestUiProjectionError::invalid_definition;
            }
        }

        if ((cue.source_mask & contracts::quest_ui_projection_source_bit(
                                   contracts::QuestUiProjectionSource::choice_available
                               )) != 0) {
            if (cue.objective_ids.empty()) {
                return QuestUiProjectionError::invalid_definition;
            }
            for (const auto& objective : cue.objective_ids) {
                std::size_t option_count{};
                for (const auto& interaction : definition.quest_interactions) {
                    if (interaction.kind == contracts::QuestInteractionKind::choose &&
                        interaction.objective_id.key == objective.key) {
                        ++option_count;
                    }
                }
                if (option_count == 0 ||
                    option_count > contracts::quest_ui_choice_option_capacity) {
                    return QuestUiProjectionError::invalid_definition;
                }
            }
        }

        for (std::size_t selector_index = 0;
             selector_index < cue.result_selectors.size();
             ++selector_index) {
            const auto& selector = cue.result_selectors[selector_index];
            const auto source_bit = contracts::quest_ui_projection_source_bit(selector.source);
            if ((selector.source != contracts::QuestUiProjectionSource::interaction_feedback &&
                 selector.source != contracts::QuestUiProjectionSource::combat_feedback) ||
                source_bit == 0 || (cue.source_mask & source_bit) == 0 ||
                !valid_content_id(selector.objective_id) ||
                !beat_has_objective(*beat, selector.objective_id.key) ||
                (!cue.objective_ids.empty() &&
                 std::none_of(
                     cue.objective_ids.begin(),
                     cue.objective_ids.end(),
                     [&selector](const contracts::ContentId& objective) {
                         return objective.key == selector.objective_id.key;
                     }
                 )) ||
                !valid_content_id(selector.primary_result_id) ||
                (!empty_content_id(selector.secondary_result_id) &&
                 !valid_content_id(selector.secondary_result_id)) ||
                (selector.polarity_override !=
                     contracts::QuestUiPolarityOverride::none &&
                 selector.polarity_override !=
                     contracts::QuestUiPolarityOverride::negative)) {
                return QuestUiProjectionError::invalid_definition;
            }
            if (selector.source == contracts::QuestUiProjectionSource::interaction_feedback) {
                const auto* interaction =
                    find_interaction(definition, selector.primary_result_id.key);
                if (interaction == nullptr || !empty_content_id(selector.secondary_result_id) ||
                    find_objective_beat(definition, interaction->objective_id.key) != beat ||
                    (interaction->objective_id.key != selector.objective_id.key &&
                     (interaction->kind != contracts::QuestInteractionKind::choose ||
                      !directly_precedes(
                          *beat,
                          interaction->objective_id.key,
                          selector.objective_id.key
                      )))) {
                    return QuestUiProjectionError::invalid_definition;
                }
            } else {
                const auto* trigger =
                    find_combat_trigger(definition, selector.primary_result_id.key);
                if (trigger == nullptr ||
                    find_objective_beat(definition, trigger->objective_id.key) != beat) {
                    return QuestUiProjectionError::invalid_definition;
                }
                if (empty_content_id(selector.secondary_result_id)) {
                    if (trigger->objective_id.key != selector.objective_id.key) {
                        return QuestUiProjectionError::invalid_definition;
                    }
                } else {
                    const auto* outcome =
                        find_combat_outcome(definition, selector.secondary_result_id.key);
                    if (outcome == nullptr ||
                        outcome->objective_id.key != selector.objective_id.key ||
                        find_objective_beat(definition, outcome->objective_id.key) != beat ||
                        (trigger->objective_id.key != selector.objective_id.key &&
                         !directly_precedes(
                             *beat,
                             trigger->objective_id.key,
                             selector.objective_id.key
                         ))) {
                        return QuestUiProjectionError::invalid_definition;
                    }
                }
            }
            for (std::size_t prior = 0; prior < selector_index; ++prior) {
                const auto& existing = cue.result_selectors[prior];
                if (existing.source == selector.source &&
                    existing.objective_id.key == selector.objective_id.key &&
                    existing.primary_result_id.key == selector.primary_result_id.key &&
                    existing.secondary_result_id.key == selector.secondary_result_id.key) {
                    return QuestUiProjectionError::invalid_definition;
                }
            }
        }
    }
    return QuestUiProjectionError::none;
}

const contracts::QuestUiCueDefinition* DeterministicQuestUiProjectionProducer::find_cue(
    contracts::StableContentKey beat,
    contracts::StableContentKey objective,
    contracts::QuestUiProjectionSource source,
    bool& ambiguous
) const noexcept {
    ambiguous = false;
    const auto source_bit = contracts::quest_ui_projection_source_bit(source);
    const contracts::QuestUiCueDefinition* found = nullptr;
    for (const auto& cue : definition_->quest_ui_cues) {
        if (cue.beat_id.key != beat || (cue.source_mask & source_bit) == 0) {
            continue;
        }
        const bool objective_matches = cue.objective_ids.empty() || std::any_of(
            cue.objective_ids.begin(),
            cue.objective_ids.end(),
            [objective](const contracts::ContentId& candidate) {
                return candidate.key == objective;
            }
        );
        if (!objective_matches) {
            continue;
        }
        if (found != nullptr) {
            ambiguous = true;
            return nullptr;
        }
        found = &cue;
    }
    return found;
}

}  // namespace tgd::gameplay
