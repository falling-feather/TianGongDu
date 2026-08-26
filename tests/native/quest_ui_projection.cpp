#include "F1QuestUiProjection.hpp"

#include <tgd/gameplay/quest_ui_projection.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace contracts = tgd::contracts;
namespace gameplay = tgd::gameplay;

using contracts::ContentId;
using contracts::QuestUiAttemptTimeClassification;
using contracts::QuestUiPolarity;
using contracts::QuestUiProjectionSignal;
using contracts::QuestUiProjectionSource;
using contracts::QuestUiRejectionReason;
using contracts::QuestUiResultSlot;
using contracts::QuestUiResultStatus;
using gameplay::DeterministicQuestUiProjectionProducer;
using gameplay::QuestObjectiveState;
using gameplay::QuestUiProjectionError;
using gameplay::QuestUiSelectionIntentError;

template <typename Signal>
concept HasAttemptTimeClassificationOverride = requires(Signal value) {
    value.attempt_time_classification;
};

static_assert(!HasAttemptTimeClassificationOverride<QuestUiProjectionSignal>);

int failures{};

void expect(bool condition, std::string_view label) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}

inline constexpr auto slice_id = contracts::content_id("test_projection_slice");
inline constexpr auto alpha_beat = contracts::content_id("test_projection_beat_alpha");
inline constexpr auto beta_beat = contracts::content_id("test_projection_beat_beta");
inline constexpr auto alpha_safe = contracts::content_id("test_projection_safe_alpha");
inline constexpr auto beta_safe = contracts::content_id("test_projection_safe_beta");
inline constexpr auto cell_id = contracts::content_id("test_projection_cell");

inline constexpr auto alpha_inspect = contracts::content_id("test_objective_alpha_inspect");
inline constexpr auto arrival_choice = contracts::content_id("test_objective_arrival_choice");
inline constexpr auto mooring_choice = contracts::content_id("test_objective_mooring_choice");
inline constexpr auto secure_mooring = contracts::content_id("test_objective_secure_mooring");
inline constexpr auto bell_code = contracts::content_id("test_objective_bell_code");
inline constexpr auto sound_bell = contracts::content_id("test_objective_sound_bell");
inline constexpr auto alpha_finish = contracts::content_id("test_objective_alpha_finish");

inline constexpr auto meet_trainer = contracts::content_id("test_objective_meet_trainer");
inline constexpr auto training_choice = contracts::content_id("test_objective_training_choice");
inline constexpr auto guard_counter = contracts::content_id("test_objective_guard_counter");
inline constexpr auto flower_heavy = contracts::content_id("test_objective_flower_heavy");
inline constexpr auto break_target = contracts::content_id("test_objective_break_target");
inline constexpr auto flower_counter = contracts::content_id("test_objective_flower_counter");
inline constexpr auto beta_finish = contracts::content_id("test_objective_beta_finish");

inline constexpr auto arrival_high = contracts::content_id("test_selection_arrival_high");
inline constexpr auto arrival_low = contracts::content_id("test_selection_arrival_low");
inline constexpr auto arrival_direct = contracts::content_id("test_selection_arrival_direct");
inline constexpr auto mooring_cross = contracts::content_id("test_selection_mooring_cross");
inline constexpr auto mooring_quick = contracts::content_id("test_selection_mooring_quick");
inline constexpr auto training_windward = contracts::content_id("test_selection_training_windward");
inline constexpr auto training_leeward = contracts::content_id("test_selection_training_leeward");

inline constexpr auto arrival_high_interaction =
    contracts::content_id("test_interaction_arrival_high");
inline constexpr auto arrival_low_interaction =
    contracts::content_id("test_interaction_arrival_low");
inline constexpr auto arrival_direct_interaction =
    contracts::content_id("test_interaction_arrival_direct");
inline constexpr auto mooring_cross_interaction =
    contracts::content_id("test_interaction_mooring_cross");
inline constexpr auto mooring_quick_interaction =
    contracts::content_id("test_interaction_mooring_quick");
inline constexpr auto lock_cross_interaction =
    contracts::content_id("test_interaction_lock_cross");
inline constexpr auto bell_interaction = contracts::content_id("test_interaction_bell");
inline constexpr auto training_windward_interaction =
    contracts::content_id("test_interaction_training_windward");
inline constexpr auto training_leeward_interaction =
    contracts::content_id("test_interaction_training_leeward");

inline constexpr auto guard_trigger = contracts::content_id("test_trigger_guard_counter");
inline constexpr auto flower_heavy_trigger =
    contracts::content_id("test_trigger_flower_heavy");
inline constexpr auto break_target_outcome =
    contracts::content_id("test_outcome_break_target");

inline constexpr auto cue_arrival = contracts::content_id("ui.test.choice.arrival");
inline constexpr auto cue_mooring_choice = contracts::content_id("ui.test.choice.mooring");
inline constexpr auto cue_mooring_load = contracts::content_id("ui.test.mooring.load");
inline constexpr auto cue_bell = contracts::content_id("ui.test.bell.feedback");
inline constexpr auto cue_training_choice = contracts::content_id("ui.test.choice.training");
inline constexpr auto cue_training_phase = contracts::content_id("ui.test.training.phase");
inline constexpr auto cue_action_proof = contracts::content_id("ui.test.training.action");
inline constexpr auto cue_recovery = contracts::content_id("ui.test.training.recovery");

[[nodiscard]] constexpr std::uint16_t source_bit(QuestUiProjectionSource source) noexcept {
    return contracts::quest_ui_projection_source_bit(source);
}

[[nodiscard]] constexpr contracts::QuestInteractionDefinition make_choice(
    ContentId id,
    ContentId objective,
    ContentId selection
) noexcept {
    return {
        id,
        contracts::QuestInteractionKind::choose,
        cell_id,
        objective,
        selection,
        {},
        {},
        {},
        1'000,
        {},
    };
}

[[nodiscard]] constexpr contracts::QuestInteractionDefinition make_operate(
    ContentId id,
    ContentId objective
) noexcept {
    return {
        id,
        contracts::QuestInteractionKind::operate,
        cell_id,
        objective,
        {},
        {},
        {},
        {},
        1'000,
        {},
    };
}

[[nodiscard]] constexpr contracts::QuestCombatTriggerDefinition make_trigger(
    ContentId id,
    ContentId objective
) noexcept {
    return {
        id,
        contracts::QuestCombatTriggerKind::player_ability_started,
        objective,
        contracts::stable_content_key("test_stance"),
        contracts::stable_content_key("test_ability"),
        {},
        {},
        {},
    };
}

[[nodiscard]] constexpr contracts::QuestCombatOutcomeDefinition make_outcome(
    ContentId id,
    ContentId objective
) noexcept {
    return {
        id,
        contracts::QuestCombatOutcomeKind::all_hostiles_defeated,
        objective,
        {},
        0,
    };
}

[[nodiscard]] constexpr contracts::QuestUiAttemptEvidenceResultSelectorDefinition
attempt_result(
    ContentId id,
    QuestUiResultStatus status,
    QuestUiRejectionReason reason = QuestUiRejectionReason::none
) noexcept {
    return {id, status, reason};
}

[[nodiscard]] constexpr contracts::QuestUiAttemptEvidenceRuleDefinition attempt_rule(
    QuestUiProjectionSource source,
    ContentId objective,
    QuestUiAttemptTimeClassification classification,
    contracts::QuestUiAttemptEvidenceResultSelectorDefinition primary = {},
    contracts::QuestUiAttemptEvidenceResultSelectorDefinition secondary = {}
) noexcept {
    return {source, objective, primary, secondary, classification};
}

struct ProjectionFixture final {
    std::array<ContentId, 7> alpha_objectives{{
        alpha_inspect,
        arrival_choice,
        mooring_choice,
        secure_mooring,
        bell_code,
        sound_bell,
        alpha_finish,
    }};
    std::array<ContentId, 7> beta_objectives{{
        meet_trainer,
        training_choice,
        guard_counter,
        flower_heavy,
        break_target,
        flower_counter,
        beta_finish,
    }};
    std::array<contracts::VerticalSliceBeatDefinition, 2> beats{};
    std::array<contracts::VerticalSliceSafePointDefinition, 2> safe_points{};
    std::array<contracts::QuestInteractionDefinition, 9> interactions{{
        make_choice(arrival_high_interaction, arrival_choice, arrival_high),
        make_choice(arrival_low_interaction, arrival_choice, arrival_low),
        make_choice(arrival_direct_interaction, arrival_choice, arrival_direct),
        make_choice(mooring_cross_interaction, mooring_choice, mooring_cross),
        make_choice(mooring_quick_interaction, mooring_choice, mooring_quick),
        make_operate(lock_cross_interaction, secure_mooring),
        make_operate(bell_interaction, sound_bell),
        make_choice(training_windward_interaction, training_choice, training_windward),
        make_choice(training_leeward_interaction, training_choice, training_leeward),
    }};
    std::array<contracts::QuestCombatTriggerDefinition, 2> triggers{{
        make_trigger(guard_trigger, guard_counter),
        make_trigger(flower_heavy_trigger, flower_heavy),
    }};
    std::array<contracts::QuestCombatOutcomeDefinition, 1> outcomes{{
        make_outcome(break_target_outcome, break_target),
    }};
    std::array<contracts::StableActorKey, 17> actor_keys{{
        101, 102, 103, 104, 105, 106, 107, 108, 109,
        110, 111, 112, 113, 114, 115, 116, 117,
    }};
    std::array<contracts::QuestEncounterActivationDefinition, 1> activations{};

    std::array<ContentId, 1> arrival_cue_objectives{{arrival_choice}};
    std::array<ContentId, 1> mooring_choice_cue_objectives{{mooring_choice}};
    std::array<ContentId, 1> mooring_load_cue_objectives{{secure_mooring}};
    std::array<ContentId, 1> bell_cue_objectives{{sound_bell}};
    std::array<ContentId, 1> training_choice_cue_objectives{{training_choice}};
    std::array<ContentId, 2> phase_cue_objectives{{guard_counter, flower_counter}};
    std::array<ContentId, 2> action_cue_objectives{{guard_counter, break_target}};
    std::array<contracts::QuestUiResultSelectorDefinition, 1> mooring_selectors{{
        {
            QuestUiProjectionSource::interaction_feedback,
            secure_mooring,
            mooring_quick_interaction,
            {},
            contracts::QuestUiPolarityOverride::negative,
        },
    }};
    std::array<contracts::QuestUiResultSelectorDefinition, 1> action_selectors{{
        {
            QuestUiProjectionSource::combat_feedback,
            break_target,
            flower_heavy_trigger,
            break_target_outcome,
            contracts::QuestUiPolarityOverride::none,
        },
    }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> arrival_attempt_rules{{
        attempt_rule(
            QuestUiProjectionSource::choice_available,
            arrival_choice,
            QuestUiAttemptTimeClassification::qualifying_first_visit
        ),
        attempt_rule(
            QuestUiProjectionSource::interaction_feedback,
            arrival_choice,
            QuestUiAttemptTimeClassification::repeat_no_progress,
            attempt_result(
                arrival_low_interaction,
                QuestUiResultStatus::ignored_repeat,
                QuestUiRejectionReason::selection_already_committed
            )
        ),
    }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 1>
        mooring_choice_attempt_rules{{
            attempt_rule(
                QuestUiProjectionSource::choice_available,
                mooring_choice,
                QuestUiAttemptTimeClassification::qualifying_craft_decision
            ),
        }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2>
        mooring_load_attempt_rules{{
            attempt_rule(
                QuestUiProjectionSource::interaction_feedback,
                secure_mooring,
                QuestUiAttemptTimeClassification::qualifying_craft_decision,
                attempt_result(
                    lock_cross_interaction,
                    QuestUiResultStatus::accepted
                )
            ),
            attempt_rule(
                QuestUiProjectionSource::interaction_feedback,
                secure_mooring,
                QuestUiAttemptTimeClassification::qualifying_error_feedback,
                attempt_result(
                    mooring_quick_interaction,
                    QuestUiResultStatus::accepted
                )
            ),
        }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> bell_attempt_rules{{
        attempt_rule(
            QuestUiProjectionSource::interaction_feedback,
            sound_bell,
            QuestUiAttemptTimeClassification::qualifying_wrong_order_feedback,
            attempt_result(
                bell_interaction,
                QuestUiResultStatus::rejected,
                QuestUiRejectionReason::prerequisite_incomplete
            )
        ),
        attempt_rule(
            QuestUiProjectionSource::interaction_feedback,
            sound_bell,
            QuestUiAttemptTimeClassification::qualifying_craft_confirmation,
            attempt_result(bell_interaction, QuestUiResultStatus::accepted)
        ),
    }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 1>
        training_choice_attempt_rules{{
            attempt_rule(
                QuestUiProjectionSource::choice_available,
                training_choice,
                QuestUiAttemptTimeClassification::qualifying_dialogue_decision
            ),
        }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> phase_attempt_rules{{
        attempt_rule(
            QuestUiProjectionSource::objective_state,
            guard_counter,
            QuestUiAttemptTimeClassification::qualifying_training_risk
        ),
        attempt_rule(
            QuestUiProjectionSource::objective_state,
            flower_counter,
            QuestUiAttemptTimeClassification::qualifying_training_risk
        ),
    }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 3> action_attempt_rules{{
        attempt_rule(
            QuestUiProjectionSource::combat_feedback,
            guard_counter,
            QuestUiAttemptTimeClassification::qualifying_combat_proof,
            attempt_result(guard_trigger, QuestUiResultStatus::accepted)
        ),
        attempt_rule(
            QuestUiProjectionSource::combat_feedback,
            break_target,
            QuestUiAttemptTimeClassification::qualifying_combat_feedback,
            attempt_result(flower_heavy_trigger, QuestUiResultStatus::accepted),
            attempt_result(
                break_target_outcome,
                QuestUiResultStatus::rejected,
                QuestUiRejectionReason::wrong_target
            )
        ),
        attempt_rule(
            QuestUiProjectionSource::combat_feedback,
            break_target,
            QuestUiAttemptTimeClassification::qualifying_combat_feedback,
            attempt_result(flower_heavy_trigger, QuestUiResultStatus::accepted),
            attempt_result(break_target_outcome, QuestUiResultStatus::accepted)
        ),
    }};
    std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> recovery_attempt_rules{{
        attempt_rule(
            QuestUiProjectionSource::recovery_offer,
            guard_counter,
            QuestUiAttemptTimeClassification::failure_retry_excluded
        ),
        attempt_rule(
            QuestUiProjectionSource::recovery_resume,
            flower_counter,
            QuestUiAttemptTimeClassification::resume_no_duplicate_progress
        ),
    }};
    std::array<contracts::QuestUiCueDefinition, 8> cues{};

    ProjectionFixture() {
        beats = {{
            {
                alpha_beat,
                contracts::VerticalSliceBeatKind::exploration,
                1,
                cell_id,
                std::span<const ContentId>{alpha_objectives},
            },
            {
                beta_beat,
                contracts::VerticalSliceBeatKind::training,
                1,
                cell_id,
                std::span<const ContentId>{beta_objectives},
            },
        }};
        safe_points = {{
            {alpha_safe, alpha_beat, {}},
            {beta_safe, beta_beat, {}},
        }};
        activations[0].id = contracts::content_id("test_projection_activation");
        activations[0].beat_id = beta_beat;
        activations[0].encounter_id = contracts::content_id("test_projection_encounter");
        activations[0].actor_keys = std::span<const contracts::StableActorKey>{actor_keys};
        cues = {{
            {
                cue_arrival,
                alpha_beat,
                static_cast<std::uint16_t>(
                    source_bit(QuestUiProjectionSource::choice_available) |
                    source_bit(QuestUiProjectionSource::interaction_feedback)
                ),
                std::span<const ContentId>{arrival_cue_objectives},
                {},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    arrival_attempt_rules
                },
            },
            {
                cue_mooring_choice,
                alpha_beat,
                source_bit(QuestUiProjectionSource::choice_available),
                std::span<const ContentId>{mooring_choice_cue_objectives},
                {},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    mooring_choice_attempt_rules
                },
            },
            {
                cue_mooring_load,
                alpha_beat,
                source_bit(QuestUiProjectionSource::interaction_feedback),
                std::span<const ContentId>{mooring_load_cue_objectives},
                std::span<const contracts::QuestUiResultSelectorDefinition>{mooring_selectors},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    mooring_load_attempt_rules
                },
            },
            {
                cue_bell,
                alpha_beat,
                source_bit(QuestUiProjectionSource::interaction_feedback),
                std::span<const ContentId>{bell_cue_objectives},
                {},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    bell_attempt_rules
                },
            },
            {
                cue_training_choice,
                beta_beat,
                source_bit(QuestUiProjectionSource::choice_available),
                std::span<const ContentId>{training_choice_cue_objectives},
                {},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    training_choice_attempt_rules
                },
            },
            {
                cue_training_phase,
                beta_beat,
                source_bit(QuestUiProjectionSource::objective_state),
                std::span<const ContentId>{phase_cue_objectives},
                {},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    phase_attempt_rules
                },
            },
            {
                cue_action_proof,
                beta_beat,
                source_bit(QuestUiProjectionSource::combat_feedback),
                std::span<const ContentId>{action_cue_objectives},
                std::span<const contracts::QuestUiResultSelectorDefinition>{action_selectors},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    action_attempt_rules
                },
            },
            {
                cue_recovery,
                beta_beat,
                static_cast<std::uint16_t>(
                    source_bit(QuestUiProjectionSource::recovery_offer) |
                    source_bit(QuestUiProjectionSource::recovery_resume)
                ),
                {},
                {},
                std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                    recovery_attempt_rules
                },
            },
        }};
    }

    [[nodiscard]] contracts::VerticalSliceDefinition definition() const noexcept {
        contracts::VerticalSliceDefinition definition{};
        definition.id = slice_id;
        definition.player.actor = 1;
        definition.beats = std::span<const contracts::VerticalSliceBeatDefinition>{beats};
        definition.safe_points =
            std::span<const contracts::VerticalSliceSafePointDefinition>{safe_points};
        definition.quest_interactions =
            std::span<const contracts::QuestInteractionDefinition>{interactions};
        definition.quest_combat_triggers =
            std::span<const contracts::QuestCombatTriggerDefinition>{triggers};
        definition.quest_combat_outcomes =
            std::span<const contracts::QuestCombatOutcomeDefinition>{outcomes};
        definition.quest_encounter_activations =
            std::span<const contracts::QuestEncounterActivationDefinition>{activations};
        definition.quest_ui_cues = std::span<const contracts::QuestUiCueDefinition>{cues};
        return definition;
    }
};

class StubQuestRuntime final : public gameplay::IQuestRuntime {
  public:
    struct ObjectiveEntry final {
        contracts::StableContentKey objective{};
        QuestObjectiveState state{QuestObjectiveState::locked};
        contracts::StableContentKey selection{};
    };

    void configure(
        const contracts::VerticalSliceDefinition& definition,
        contracts::StableContentKey stage,
        std::uint64_t checksum,
        contracts::TickIndex tick = 10
    ) {
        entries_.clear();
        for (const auto& beat : definition.beats) {
            for (const auto& objective : beat.objectives) {
                entries_.push_back({objective.key, QuestObjectiveState::locked, 0});
            }
        }
        snapshot_ = {};
        snapshot_.tick = tick;
        snapshot_.quest = definition.id.key;
        snapshot_.stage = stage;
        snapshot_.checksum = checksum;
    }

    void set_state(contracts::StableContentKey objective, QuestObjectiveState state) {
        for (auto& entry : entries_) {
            if (entry.objective == objective) {
                entry.state = state;
                return;
            }
        }
    }

    void set_selection(
        contracts::StableContentKey objective,
        contracts::StableContentKey selection
    ) {
        for (auto& entry : entries_) {
            if (entry.objective == objective) {
                entry.selection = selection;
                return;
            }
        }
    }

    void set_stage(contracts::StableContentKey stage) noexcept {
        snapshot_.stage = stage;
    }

    void set_quest(contracts::StableContentKey quest) noexcept {
        snapshot_.quest = quest;
    }

    void set_checksum(std::uint64_t checksum) noexcept {
        snapshot_.checksum = checksum;
    }

    [[nodiscard]] gameplay::QuestError initialize(
        const contracts::VerticalSliceDefinition&,
        contracts::StableActorKey
    ) noexcept override {
        return gameplay::QuestError::none;
    }

    [[nodiscard]] gameplay::QuestError start() noexcept override {
        return gameplay::QuestError::none;
    }

    [[nodiscard]] gameplay::QuestError pause() noexcept override {
        return gameplay::QuestError::none;
    }

    [[nodiscard]] gameplay::QuestError resume() noexcept override {
        return gameplay::QuestError::none;
    }

    [[nodiscard]] gameplay::QuestError destroy() noexcept override {
        return gameplay::QuestError::none;
    }

    [[nodiscard]] gameplay::QuestApplyResult apply(
        const contracts::QuestCommand&,
        gameplay::IQuestEventSink&
    ) noexcept override {
        return {};
    }

    [[nodiscard]] QuestObjectiveState objective_state(
        contracts::StableContentKey objective
    ) const noexcept override {
        for (const auto& entry : entries_) {
            if (entry.objective == objective) {
                return entry.state;
            }
        }
        return QuestObjectiveState::unknown;
    }

    [[nodiscard]] contracts::StableContentKey selected_option(
        contracts::StableContentKey objective
    ) const noexcept override {
        for (const auto& entry : entries_) {
            if (entry.objective == objective) {
                return entry.selection;
            }
        }
        return 0;
    }

    [[nodiscard]] const contracts::QuestSnapshot& snapshot() const noexcept override {
        return snapshot_;
    }

  private:
    std::vector<ObjectiveEntry> entries_{};
    contracts::QuestSnapshot snapshot_{};
};

[[nodiscard]] constexpr QuestUiResultSlot result(
    ContentId id,
    ContentId objective,
    QuestUiResultStatus status,
    QuestUiRejectionReason reason = QuestUiRejectionReason::none
) noexcept {
    return {id.key, objective.key, status, reason};
}

[[nodiscard]] constexpr QuestUiProjectionSignal signal(
    QuestUiProjectionSource source,
    ContentId objective,
    QuestUiResultSlot primary = {},
    QuestUiResultSlot secondary = {}
) noexcept {
    return {
        source,
        objective.key,
        primary,
        secondary,
    };
}

[[nodiscard]] contracts::CombatActorSnapshot actor(
    contracts::StableActorKey key,
    bool active,
    bool defeated
) noexcept {
    contracts::CombatActorSnapshot snapshot{};
    snapshot.actor = key;
    snapshot.faction = contracts::CombatFaction::hostile;
    snapshot.active = active;
    snapshot.defeated = defeated;
    return snapshot;
}

struct StateChange final {
    ContentId objective{};
    QuestObjectiveState state{QuestObjectiveState::locked};
};

struct SelectionChange final {
    ContentId objective{};
    ContentId selection{};
};

struct ProjectionCase final {
    std::string_view name{};
    ContentId beat{};
    ContentId safe_point{};
    QuestUiProjectionSignal projection_signal{};
    std::vector<StateChange> states{};
    std::vector<SelectionChange> selections{};
    std::vector<contracts::CombatActorSnapshot> actors{};
    QuestUiAttemptTimeClassification expected_attempt{
        QuestUiAttemptTimeClassification::unspecified
    };
    ContentId expected_cue{};
    QuestUiPolarity expected_polarity{QuestUiPolarity::positive};
    std::uint8_t expected_choice_count{};
};

void test_authoritative_projection_catalog() {
    ProjectionFixture fixture;
    const auto definition = fixture.definition();
    const std::vector<ProjectionCase> cases{
        {
            "arrival choice",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::choice_available,
                arrival_choice
            ),
            {{arrival_choice, QuestObjectiveState::active}},
            {},
            {},
            QuestUiAttemptTimeClassification::qualifying_first_visit,
            cue_arrival,
            QuestUiPolarity::positive,
            3,
        },
        {
            "arrival repeat",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::interaction_feedback,
                arrival_choice,
                result(
                    arrival_low_interaction,
                    arrival_choice,
                    QuestUiResultStatus::ignored_repeat,
                    QuestUiRejectionReason::selection_already_committed
                )
            ),
            {
                {arrival_choice, QuestObjectiveState::completed},
                {mooring_choice, QuestObjectiveState::active},
            },
            {{arrival_choice, arrival_high}},
            {},
            QuestUiAttemptTimeClassification::repeat_no_progress,
            cue_arrival,
            QuestUiPolarity::negative,
            0,
        },
        {
            "mooring choice",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::choice_available,
                mooring_choice
            ),
            {{mooring_choice, QuestObjectiveState::active}},
            {},
            {},
            QuestUiAttemptTimeClassification::qualifying_craft_decision,
            cue_mooring_choice,
            QuestUiPolarity::positive,
            2,
        },
        {
            "cross belay stable",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::interaction_feedback,
                secure_mooring,
                result(
                    lock_cross_interaction,
                    secure_mooring,
                    QuestUiResultStatus::accepted
                )
            ),
            {
                {mooring_choice, QuestObjectiveState::completed},
                {secure_mooring, QuestObjectiveState::completed},
                {bell_code, QuestObjectiveState::active},
            },
            {{mooring_choice, mooring_cross}},
            {},
            QuestUiAttemptTimeClassification::qualifying_craft_decision,
            cue_mooring_load,
            QuestUiPolarity::positive,
            0,
        },
        {
            "quick hitch overload",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::interaction_feedback,
                secure_mooring,
                result(
                    mooring_quick_interaction,
                    mooring_choice,
                    QuestUiResultStatus::accepted
                )
            ),
            {
                {mooring_choice, QuestObjectiveState::completed},
                {secure_mooring, QuestObjectiveState::active},
            },
            {{mooring_choice, mooring_quick}},
            {},
            QuestUiAttemptTimeClassification::qualifying_error_feedback,
            cue_mooring_load,
            QuestUiPolarity::negative,
            0,
        },
        {
            "bell rejected",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::interaction_feedback,
                sound_bell,
                result(
                    bell_interaction,
                    sound_bell,
                    QuestUiResultStatus::rejected,
                    QuestUiRejectionReason::prerequisite_incomplete
                )
            ),
            {
                {bell_code, QuestObjectiveState::active},
                {sound_bell, QuestObjectiveState::locked},
            },
            {},
            {},
            QuestUiAttemptTimeClassification::qualifying_wrong_order_feedback,
            cue_bell,
            QuestUiPolarity::negative,
            0,
        },
        {
            "bell accepted",
            alpha_beat,
            alpha_safe,
            signal(
                QuestUiProjectionSource::interaction_feedback,
                sound_bell,
                result(bell_interaction, sound_bell, QuestUiResultStatus::accepted)
            ),
            {
                {sound_bell, QuestObjectiveState::completed},
                {alpha_finish, QuestObjectiveState::active},
            },
            {},
            {},
            QuestUiAttemptTimeClassification::qualifying_craft_confirmation,
            cue_bell,
            QuestUiPolarity::positive,
            0,
        },
        {
            "training choice",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::choice_available,
                training_choice
            ),
            {{training_choice, QuestObjectiveState::active}},
            {},
            {},
            QuestUiAttemptTimeClassification::qualifying_dialogue_decision,
            cue_training_choice,
            QuestUiPolarity::positive,
            2,
        },
        {
            "guard phase",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::objective_state,
                guard_counter
            ),
            {
                {training_choice, QuestObjectiveState::completed},
                {guard_counter, QuestObjectiveState::active},
            },
            {{training_choice, training_windward}},
            {actor(101, true, false)},
            QuestUiAttemptTimeClassification::qualifying_training_risk,
            cue_training_phase,
            QuestUiPolarity::positive,
            0,
        },
        {
            "flower phase",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::objective_state,
                flower_counter
            ),
            {{flower_counter, QuestObjectiveState::active}},
            {},
            {actor(102, true, false)},
            QuestUiAttemptTimeClassification::qualifying_training_risk,
            cue_training_phase,
            QuestUiPolarity::positive,
            0,
        },
        {
            "guard proof accepted",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::combat_feedback,
                guard_counter,
                result(guard_trigger, guard_counter, QuestUiResultStatus::accepted)
            ),
            {
                {guard_counter, QuestObjectiveState::completed},
                {flower_heavy, QuestObjectiveState::active},
            },
            {},
            {actor(101, true, false)},
            QuestUiAttemptTimeClassification::qualifying_combat_proof,
            cue_action_proof,
            QuestUiPolarity::positive,
            0,
        },
        {
            "trigger accepted outcome rejected",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::combat_feedback,
                break_target,
                result(
                    flower_heavy_trigger,
                    flower_heavy,
                    QuestUiResultStatus::accepted
                ),
                result(
                    break_target_outcome,
                    break_target,
                    QuestUiResultStatus::rejected,
                    QuestUiRejectionReason::wrong_target
                )
            ),
            {
                {flower_heavy, QuestObjectiveState::completed},
                {break_target, QuestObjectiveState::active},
            },
            {},
            {actor(103, true, false)},
            QuestUiAttemptTimeClassification::qualifying_combat_feedback,
            cue_action_proof,
            QuestUiPolarity::negative,
            0,
        },
        {
            "recovery offer",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::recovery_offer,
                guard_counter
            ),
            {{guard_counter, QuestObjectiveState::active}},
            {},
            {actor(101, true, false)},
            QuestUiAttemptTimeClassification::failure_retry_excluded,
            cue_recovery,
            QuestUiPolarity::recovery,
            0,
        },
        {
            "recovery resume",
            beta_beat,
            beta_safe,
            signal(
                QuestUiProjectionSource::recovery_resume,
                flower_counter
            ),
            {{flower_counter, QuestObjectiveState::active}},
            {},
            {actor(102, true, false), actor(103, false, true)},
            QuestUiAttemptTimeClassification::resume_no_duplicate_progress,
            cue_recovery,
            QuestUiPolarity::recovery,
            0,
        },
    };

    std::uint64_t checksum = 1'000;
    for (const auto& projection_case : cases) {
        StubQuestRuntime quest;
        quest.configure(definition, projection_case.beat.key, checksum++);
        for (const auto& state : projection_case.states) {
            quest.set_state(state.objective.key, state.state);
        }
        for (const auto& selection : projection_case.selections) {
            quest.set_selection(selection.objective.key, selection.selection.key);
        }
        DeterministicQuestUiProjectionProducer producer;
        expect(
            producer.initialize(definition) == QuestUiProjectionError::none,
            projection_case.name
        );
        const auto projected = producer.project(
            projection_case.projection_signal,
            quest,
            projection_case.safe_point.key,
            projection_case.actors
        );
        expect(projected.error == QuestUiProjectionError::none, projection_case.name);
        if (projected.error != QuestUiProjectionError::none) {
            continue;
        }
        expect(projected.projection.sequence == 1, projection_case.name);
        expect(projected.projection.cue == projection_case.expected_cue.key, projection_case.name);
        expect(
            projected.projection.polarity == projection_case.expected_polarity,
            projection_case.name
        );
        expect(
            projected.projection.choice_option_count ==
                projection_case.expected_choice_count,
            projection_case.name
        );
        expect(
            projected.projection.attempt_time_classification ==
                projection_case.expected_attempt,
            projection_case.name
        );
        expect(projected.projection.quest_checksum == quest.snapshot().checksum, projection_case.name);
        expect(projected.projection.checksum != 0, projection_case.name);
        expect(producer.snapshot() == projected.projection, projection_case.name);
    }
}

void test_choice_order_intent_and_sequence() {
    ProjectionFixture fixture;
    const auto definition = fixture.definition();
    StubQuestRuntime quest;
    quest.configure(definition, alpha_beat.key, 8'001, 20);
    quest.set_state(arrival_choice.key, QuestObjectiveState::active);
    DeterministicQuestUiProjectionProducer producer;
    expect(producer.initialize(definition) == QuestUiProjectionError::none, "choice init");
    const auto choice_signal = signal(
        QuestUiProjectionSource::choice_available,
        arrival_choice
    );
    expect(
        producer.has_authored_cue(
            alpha_beat.key,
            arrival_choice.key,
            QuestUiProjectionSource::choice_available
        ),
        "an authored choice cue requires projection and Action intent"
    );
    expect(
        !producer.has_authored_cue(
            beta_beat.key,
            arrival_choice.key,
            QuestUiProjectionSource::choice_available
        ) &&
            !producer.has_authored_cue(
                alpha_beat.key,
                secure_mooring.key,
                QuestUiProjectionSource::choice_available
            ),
        "a cross-beat or unprojected world interaction does not enter choice UI mode"
    );

    auto legacy_definition = definition;
    legacy_definition.quest_ui_cues = definition.quest_ui_cues.subspan(1);
    DeterministicQuestUiProjectionProducer legacy_producer;
    expect(
        legacy_producer.initialize(legacy_definition) == QuestUiProjectionError::none &&
            !legacy_producer.has_authored_cue(
                alpha_beat.key,
                arrival_choice.key,
                QuestUiProjectionSource::choice_available
            ),
        "an authored world choose interaction without a UI cue stays on the explicit world path"
    );
    const auto first = producer.project(choice_signal, quest, alpha_safe.key, {});
    expect(first.error == QuestUiProjectionError::none, "choice first projection");
    expect(first.projection.choice_option_count == 3, "choice exact option count");
    expect(
        first.projection.choice_options[0].interaction == arrival_high_interaction.key &&
            first.projection.choice_options[0].selection == arrival_high.key &&
            first.projection.choice_options[1].interaction == arrival_low_interaction.key &&
            first.projection.choice_options[1].selection == arrival_low.key &&
            first.projection.choice_options[2].interaction == arrival_direct_interaction.key &&
            first.projection.choice_options[2].selection == arrival_direct.key,
        "choice authored order"
    );

    auto native_projection = first.projection;
    native_projection.choice_options[0].selection =
        contracts::stable_content_key("f1_choice_arrival_high_water_tags");
    native_projection.choice_options[1].selection =
        contracts::stable_content_key("f1_choice_arrival_drowned_manifest");
    native_projection.choice_options[2].selection =
        contracts::stable_content_key("f1_choice_arrival_follow_bell");
    F1QuestUiChoiceState native_choice;
    expect(
        native_choice.begin(native_projection, false) == F1QuestUiChoiceError::none &&
            native_choice.pending() && native_choice.native_pending() &&
            native_choice.option_count() == 3,
        "native choice enters an explicit input-gated mode"
    );
    expect(
        native_choice.option_label(0) == "High-water marks" &&
            native_choice.option_label(1) == "Drowned manifest" &&
            native_choice.option_label(2) == "Follow the bell",
        "native choice Presentation preserves authored projection order"
    );
    const auto out_of_range = native_choice.native_intent(3);
    expect(
        out_of_range.error == F1QuestUiChoiceError::option_out_of_range &&
            native_choice.pending() && native_choice.option_count() == 3,
        "an out-of-range native key leaves the active choice unchanged"
    );
    const auto native_second = native_choice.native_intent(1);
    expect(
        native_second.error == F1QuestUiChoiceError::none &&
            native_second.intent.projection_sequence == native_projection.sequence &&
            native_second.intent.projection_checksum == native_projection.checksum &&
            native_second.intent.interaction ==
                native_projection.choice_options[1].interaction &&
            native_second.intent.selection == native_projection.choice_options[1].selection &&
            native_choice.matches(native_second.intent),
        "native numeric selection submits the option displayed at the same index"
    );
    auto old_native_intent = native_second.intent;
    --old_native_intent.projection_sequence;
    expect(
        !native_choice.matches(old_native_intent) && native_choice.pending(),
        "an old native intent cannot exit the active choice mode"
    );
    native_choice.finish();
    expect(
        !native_choice.pending() &&
            native_choice.native_intent(1).error ==
                F1QuestUiChoiceError::unavailable_in_mode &&
            !native_choice.matches(native_second.intent),
        "accepted native selection exits choice mode and repeat input cannot replay it"
    );
    auto unknown_native_projection = native_projection;
    unknown_native_projection.choice_options[1].selection =
        contracts::stable_content_key("unknown_native_presentation");
    expect(
        native_choice.begin(unknown_native_projection, false) ==
                F1QuestUiChoiceError::missing_presentation &&
            !native_choice.pending(),
        "native choice fails closed instead of displaying an internal Stable ID"
    );

    const contracts::QuestUiSelectionIntent valid_intent{
        first.projection.sequence,
        first.projection.checksum,
        arrival_choice.key,
        arrival_low_interaction.key,
        arrival_low.key,
    };
    expect(
        producer.validate_choice_intent(valid_intent, quest) ==
            QuestUiSelectionIntentError::none,
        "choice intent valid"
    );
    F1QuestUiChoiceState external_choice;
    expect(
        external_choice.begin(first.projection, true) == F1QuestUiChoiceError::none &&
            external_choice.mode() == F1QuestUiChoiceMode::external &&
            external_choice.matches(valid_intent) &&
            external_choice.native_intent(0).error ==
                F1QuestUiChoiceError::unavailable_in_mode,
        "external consumer and Native fallback share the exact projection intent identity"
    );
    const auto stable = producer.snapshot();
    const auto expect_intent_failure = [&](
                                           const contracts::QuestUiSelectionIntent& intent,
                                           const StubQuestRuntime& runtime,
                                           QuestUiSelectionIntentError error,
                                           std::string_view label) {
        expect(producer.validate_choice_intent(intent, runtime) == error, label);
        expect(producer.snapshot() == stable, label);
    };
    auto stale_sequence = valid_intent;
    --stale_sequence.projection_sequence;
    expect_intent_failure(
        stale_sequence,
        quest,
        QuestUiSelectionIntentError::stale_projection,
        "intent stale sequence"
    );
    auto stale_checksum = valid_intent;
    stale_checksum.projection_checksum ^= 1U;
    expect_intent_failure(
        stale_checksum,
        quest,
        QuestUiSelectionIntentError::stale_projection,
        "intent stale projection checksum"
    );
    auto wrong_objective = valid_intent;
    wrong_objective.objective = mooring_choice.key;
    expect_intent_failure(
        wrong_objective,
        quest,
        QuestUiSelectionIntentError::objective_mismatch,
        "intent cross objective"
    );
    auto wrong_interaction = valid_intent;
    wrong_interaction.interaction = arrival_high_interaction.key;
    expect_intent_failure(
        wrong_interaction,
        quest,
        QuestUiSelectionIntentError::selection_not_authored,
        "intent interaction selection pair"
    );
    auto zero_interaction = valid_intent;
    zero_interaction.interaction = 0;
    expect_intent_failure(
        zero_interaction,
        quest,
        QuestUiSelectionIntentError::selection_not_authored,
        "intent zero interaction"
    );
    auto unknown_selection = valid_intent;
    unknown_selection.selection = contracts::stable_content_key("unknown_selection");
    expect_intent_failure(
        unknown_selection,
        quest,
        QuestUiSelectionIntentError::selection_not_authored,
        "intent unknown selection"
    );

    StubQuestRuntime changed_checksum;
    changed_checksum.configure(definition, alpha_beat.key, 8'002);
    changed_checksum.set_state(arrival_choice.key, QuestObjectiveState::active);
    expect_intent_failure(
        valid_intent,
        changed_checksum,
        QuestUiSelectionIntentError::quest_context_changed,
        "intent quest checksum changed"
    );
    StubQuestRuntime changed_stage;
    changed_stage.configure(definition, beta_beat.key, 8'001);
    changed_stage.set_state(training_choice.key, QuestObjectiveState::active);
    expect_intent_failure(
        valid_intent,
        changed_stage,
        QuestUiSelectionIntentError::quest_context_changed,
        "intent stage changed"
    );
    StubQuestRuntime changed_quest;
    changed_quest.configure(definition, alpha_beat.key, 8'001);
    changed_quest.set_state(arrival_choice.key, QuestObjectiveState::active);
    changed_quest.set_quest(contracts::stable_content_key("another_slice"));
    expect_intent_failure(
        valid_intent,
        changed_quest,
        QuestUiSelectionIntentError::quest_context_changed,
        "intent definition changed"
    );
    StubQuestRuntime committed;
    committed.configure(definition, alpha_beat.key, 8'001);
    committed.set_state(arrival_choice.key, QuestObjectiveState::active);
    committed.set_selection(arrival_choice.key, arrival_low.key);
    expect_intent_failure(
        valid_intent,
        committed,
        QuestUiSelectionIntentError::selection_already_committed,
        "intent selection committed"
    );
    StubQuestRuntime locked;
    locked.configure(definition, alpha_beat.key, 8'001);
    locked.set_state(arrival_choice.key, QuestObjectiveState::locked);
    locked.set_state(alpha_inspect.key, QuestObjectiveState::active);
    expect_intent_failure(
        valid_intent,
        locked,
        QuestUiSelectionIntentError::objective_not_active,
        "intent objective no longer active"
    );

    quest.set_checksum(8'002);
    const auto second = producer.project(choice_signal, quest, alpha_safe.key, {});
    expect(second.error == QuestUiProjectionError::none, "sequence second projection");
    expect(second.projection.sequence == 2, "sequence strictly monotonic");
    expect(second.projection.checksum != first.projection.checksum, "sequence enters checksum");
}

void test_same_beat_stage_active_focus() {
    ProjectionFixture fixture;
    const auto definition = fixture.definition();

    gameplay::DeterministicQuestRuntime real_quest;
    expect(
        real_quest.initialize(definition, definition.player.actor) == gameplay::QuestError::none &&
            real_quest.start() == gameplay::QuestError::none,
        "real Quest runtime starts with stage-active objectives"
    );
    expect(
        real_quest.objective_state(alpha_inspect.key) == QuestObjectiveState::active &&
            real_quest.objective_state(arrival_choice.key) == QuestObjectiveState::active &&
            real_quest.objective_state(alpha_finish.key) == QuestObjectiveState::active &&
            real_quest.objective_state(training_choice.key) == QuestObjectiveState::locked,
        "real Quest runtime exposes all current Beat objectives as active"
    );

    DeterministicQuestUiProjectionProducer choice_producer;
    expect(
        choice_producer.initialize(definition) == QuestUiProjectionError::none,
        "stage-active choice producer init"
    );
    const auto choice = choice_producer.project(
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        real_quest,
        alpha_safe.key,
        {}
    );
    expect(choice.error == QuestUiProjectionError::none, "stage-active choice projection");
    expect(
        choice.projection.pending_objective == arrival_choice.key &&
            choice.projection.objective == arrival_choice.key &&
            choice.projection.choice_option_count == 3,
        "choice projection keeps one signal focus without flattening peer active objectives"
    );
    expect(
        choice_producer.validate_choice_intent(
            {
                choice.projection.sequence,
                choice.projection.checksum,
                choice.projection.objective,
                choice.projection.choice_options[0].interaction,
                choice.projection.choice_options[0].selection,
            },
            real_quest
        ) == QuestUiSelectionIntentError::none,
        "choice intent validates against a real multi-active Quest stage"
    );

    StubQuestRuntime gameplay_quest;
    gameplay_quest.configure(definition, beta_beat.key, 8'101);
    gameplay_quest.set_state(guard_counter.key, QuestObjectiveState::active);
    gameplay_quest.set_state(flower_counter.key, QuestObjectiveState::active);
    gameplay_quest.set_state(beta_finish.key, QuestObjectiveState::active);
    DeterministicQuestUiProjectionProducer gameplay_producer;
    expect(
        gameplay_producer.initialize(definition) == QuestUiProjectionError::none,
        "stage-active gameplay producer init"
    );
    const auto objective = gameplay_producer.project(
        signal(
            QuestUiProjectionSource::objective_state,
            guard_counter
        ),
        gameplay_quest,
        beta_safe.key,
        {}
    );
    expect(
        objective.error == QuestUiProjectionError::none &&
            objective.projection.pending_objective == guard_counter.key,
        "objective projection accepts three active objectives in the current Beat"
    );

    gameplay_quest.set_checksum(8'102);
    const auto combat = gameplay_producer.project(
        signal(
            QuestUiProjectionSource::combat_feedback,
            guard_counter,
            result(guard_trigger, guard_counter, QuestUiResultStatus::accepted)
        ),
        gameplay_quest,
        beta_safe.key,
        {}
    );
    expect(
        combat.error == QuestUiProjectionError::none &&
            combat.projection.pending_objective == guard_counter.key &&
            combat.projection.sequence == objective.projection.sequence + 1,
        "combat projection preserves signal focus with peer stage-active objectives"
    );

    StubQuestRuntime completed_focus;
    completed_focus.configure(definition, alpha_beat.key, 8'103);
    completed_focus.set_state(sound_bell.key, QuestObjectiveState::completed);
    completed_focus.set_state(alpha_finish.key, QuestObjectiveState::active);
    DeterministicQuestUiProjectionProducer completed_producer;
    expect(
        completed_producer.initialize(definition) == QuestUiProjectionError::none,
        "completed focus producer init"
    );
    const auto completed = completed_producer.project(
        signal(
            QuestUiProjectionSource::interaction_feedback,
            sound_bell,
            result(bell_interaction, sound_bell, QuestUiResultStatus::accepted)
        ),
        completed_focus,
        alpha_safe.key,
        {}
    );
    expect(
        completed.error == QuestUiProjectionError::none &&
            completed.projection.pending_objective == 0 &&
            completed.projection.objective == sound_bell.key,
        "completed result focus does not invent a pending objective"
    );
}

void expect_initialize_invalid(ProjectionFixture& fixture, std::string_view label) {
    DeterministicQuestUiProjectionProducer producer;
    expect(
        producer.initialize(fixture.definition()) == QuestUiProjectionError::invalid_definition,
        label
    );
    expect(!producer.initialized() && !producer.has_projection(), label);
}

void test_definition_fail_closed() {
    {
        ProjectionFixture fixture;
        fixture.cues[0].source_mask = 0;
        expect_initialize_invalid(fixture, "definition zero source mask");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].source_mask = static_cast<std::uint16_t>(1U << 12U);
        expect_initialize_invalid(fixture, "definition unknown source bit");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].cue_id = {};
        expect_initialize_invalid(fixture, "definition empty cue id");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].beat_id = {};
        expect_initialize_invalid(fixture, "definition empty cue beat");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].beat_id = contracts::content_id("unknown_beat");
        expect_initialize_invalid(fixture, "definition unknown cue beat");
    }
    {
        ProjectionFixture fixture;
        fixture.arrival_cue_objectives[0] = {};
        expect_initialize_invalid(fixture, "definition empty cue objective");
    }
    {
        ProjectionFixture fixture;
        fixture.arrival_cue_objectives[0] = contracts::content_id("unknown_objective");
        expect_initialize_invalid(fixture, "definition unknown cue objective");
    }
    {
        ProjectionFixture fixture;
        fixture.arrival_cue_objectives[0] = guard_counter;
        expect_initialize_invalid(fixture, "definition cross beat cue objective");
    }
    {
        ProjectionFixture fixture;
        std::array<ContentId, 2> duplicate{{arrival_choice, arrival_choice}};
        fixture.cues[0].objective_ids = std::span<const ContentId>{duplicate};
        expect_initialize_invalid(fixture, "definition duplicate cue objective");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].objective_ids = {};
        expect_initialize_invalid(fixture, "definition wildcard overlap");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[1].objective_ids =
            std::span<const ContentId>{fixture.arrival_cue_objectives};
        expect_initialize_invalid(fixture, "definition same source domain ambiguity");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[1].cue_id = fixture.cues[0].cue_id;
        expect_initialize_invalid(fixture, "definition duplicate cue id");
    }
    {
        ProjectionFixture fixture;
        fixture.interactions[4].selection_id = fixture.interactions[3].selection_id;
        expect_initialize_invalid(fixture, "definition duplicate choice selection");
    }
    {
        ProjectionFixture fixture;
        fixture.mooring_selectors[0].primary_result_id =
            contracts::content_id("unknown_interaction");
        expect_initialize_invalid(fixture, "definition unknown selector result");
    }
    {
        ProjectionFixture fixture;
        fixture.mooring_selectors[0].polarity_override =
            static_cast<contracts::QuestUiPolarityOverride>(255);
        expect_initialize_invalid(fixture, "definition unknown selector polarity override");
    }
    {
        ProjectionFixture fixture;
        std::array<contracts::QuestUiResultSelectorDefinition, 2> duplicate{{
            fixture.mooring_selectors[0],
            fixture.mooring_selectors[0],
        }};
        fixture.cues[2].result_selectors =
            std::span<const contracts::QuestUiResultSelectorDefinition>{duplicate};
        expect_initialize_invalid(fixture, "definition duplicate result selector");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].attempt_evidence_rules = {};
        expect_initialize_invalid(fixture, "definition empty attempt evidence rules");
    }
    {
        ProjectionFixture fixture;
        fixture.cues[0].attempt_evidence_rules =
            std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                &fixture.arrival_attempt_rules[0],
                1
            };
        expect_initialize_invalid(fixture, "definition attempt evidence misses cue source");
    }
    {
        ProjectionFixture fixture;
        std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, 2> duplicate{{
            fixture.mooring_choice_attempt_rules[0],
            fixture.mooring_choice_attempt_rules[0],
        }};
        duplicate[1].classification =
            QuestUiAttemptTimeClassification::qualifying_first_visit;
        fixture.cues[1].attempt_evidence_rules =
            std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{duplicate};
        expect_initialize_invalid(fixture, "definition ambiguous attempt evidence selector");
    }
    {
        ProjectionFixture fixture;
        fixture.mooring_load_attempt_rules[0].primary_result.result_id =
            contracts::content_id("unknown_attempt_result");
        expect_initialize_invalid(fixture, "definition unknown attempt evidence result");
    }
    {
        ProjectionFixture fixture;
        fixture.arrival_attempt_rules[0].primary_result.result_id =
            arrival_high_interaction;
        expect_initialize_invalid(fixture, "definition nonempty attempt evidence sentinel");
    }
    {
        ProjectionFixture fixture;
        fixture.arrival_attempt_rules[0].classification =
            static_cast<QuestUiAttemptTimeClassification>(255);
        expect_initialize_invalid(fixture, "definition unknown attempt evidence classification");
    }
    {
        ProjectionFixture fixture;
        fixture.arrival_attempt_rules[0].classification =
            QuestUiAttemptTimeClassification::qualifying_combat_proof;
        expect_initialize_invalid(fixture, "definition source classification mismatch");
    }
    {
        ProjectionFixture fixture;
        const auto legacy = fixture.definition();
        auto pre_1_5 = legacy;
        pre_1_5.quest_ui_cues = {};
        DeterministicQuestUiProjectionProducer producer;
        expect(pre_1_5.quest_ui_cues.empty(), "pre-1.5 empty span compiles");
        expect(
            producer.initialize(pre_1_5) == QuestUiProjectionError::invalid_definition,
            "pre-1.5 projection producer fails closed"
        );
    }
}

void expect_projection_failure_unchanged(
    DeterministicQuestUiProjectionProducer& producer,
    const QuestUiProjectionSignal& projection_signal,
    const StubQuestRuntime& quest,
    contracts::StableContentKey safe_point,
    std::span<const contracts::CombatActorSnapshot> actors,
    QuestUiProjectionError expected_error,
    std::string_view label
) {
    const auto before = producer.snapshot();
    const auto projected = producer.project(projection_signal, quest, safe_point, actors);
    expect(projected.error == expected_error, label);
    expect(producer.snapshot() == before, label);
    expect(producer.snapshot().sequence == before.sequence, label);
    expect(producer.snapshot().checksum == before.checksum, label);
}

void test_missing_attempt_evidence_fails_without_state_change() {
    ProjectionFixture fixture;
    fixture.cues[3].attempt_evidence_rules =
        std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
            &fixture.bell_attempt_rules[0],
            1
        };
    const auto definition = fixture.definition();
    DeterministicQuestUiProjectionProducer producer;
    expect(
        producer.initialize(definition) == QuestUiProjectionError::none,
        "missing attempt evidence producer init"
    );

    StubQuestRuntime baseline_quest;
    baseline_quest.configure(definition, alpha_beat.key, 8'900);
    baseline_quest.set_state(arrival_choice.key, QuestObjectiveState::active);
    const auto baseline = producer.project(
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        {}
    );
    expect(baseline.error == QuestUiProjectionError::none, "missing evidence baseline");

    StubQuestRuntime bell_quest;
    bell_quest.configure(definition, alpha_beat.key, 8'901);
    bell_quest.set_state(sound_bell.key, QuestObjectiveState::completed);
    bell_quest.set_state(alpha_finish.key, QuestObjectiveState::active);
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::interaction_feedback,
            sound_bell,
            result(bell_interaction, sound_bell, QuestUiResultStatus::accepted)
        ),
        bell_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::missing_attempt_evidence,
        "missing attempt evidence"
    );
}

void test_runtime_fail_closed_and_snapshot_invariance() {
    ProjectionFixture fixture;
    const auto definition = fixture.definition();
    StubQuestRuntime baseline_quest;
    baseline_quest.configure(definition, alpha_beat.key, 9'001);
    baseline_quest.set_state(arrival_choice.key, QuestObjectiveState::active);
    DeterministicQuestUiProjectionProducer producer;
    expect(producer.initialize(definition) == QuestUiProjectionError::none, "failure init");
    const auto baseline = producer.project(
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        {}
    );
    expect(baseline.error == QuestUiProjectionError::none, "failure baseline");

    StubQuestRuntime secure_quest;
    secure_quest.configure(definition, alpha_beat.key, 9'002);
    secure_quest.set_state(mooring_choice.key, QuestObjectiveState::completed);
    secure_quest.set_selection(mooring_choice.key, mooring_quick.key);
    secure_quest.set_state(secure_mooring.key, QuestObjectiveState::active);
    const auto quick_signal = signal(
        QuestUiProjectionSource::interaction_feedback,
        secure_mooring,
        result(
            mooring_quick_interaction,
            mooring_choice,
            QuestUiResultStatus::accepted
        )
    );

    auto unknown_interaction = quick_signal;
    unknown_interaction.primary_result.id = contracts::stable_content_key("unknown_interaction");
    expect_projection_failure_unchanged(
        producer,
        unknown_interaction,
        secure_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "unknown interaction result"
    );
    auto wrong_interaction_type = quick_signal;
    wrong_interaction_type.primary_result.id = break_target_outcome.key;
    expect_projection_failure_unchanged(
        producer,
        wrong_interaction_type,
        secure_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "interaction result wrong type"
    );
    auto cross_beat_interaction = quick_signal;
    cross_beat_interaction.primary_result.id = training_windward_interaction.key;
    cross_beat_interaction.primary_result.objective = training_choice.key;
    expect_projection_failure_unchanged(
        producer,
        cross_beat_interaction,
        secure_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "interaction result cross beat"
    );
    auto cross_objective_rejected = quick_signal;
    cross_objective_rejected.primary_result.status = QuestUiResultStatus::rejected;
    cross_objective_rejected.primary_result.rejection_reason =
        QuestUiRejectionReason::wrong_target;
    expect_projection_failure_unchanged(
        producer,
        cross_objective_rejected,
        secure_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "cross objective interaction rejects non-accepted transition"
    );
    StubQuestRuntime uncommitted_origin;
    uncommitted_origin.configure(definition, alpha_beat.key, 9'003);
    uncommitted_origin.set_state(mooring_choice.key, QuestObjectiveState::completed);
    uncommitted_origin.set_state(secure_mooring.key, QuestObjectiveState::active);
    expect_projection_failure_unchanged(
        producer,
        quick_signal,
        uncommitted_origin,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "cross objective interaction requires committed selection"
    );

    StubQuestRuntime combat_quest;
    combat_quest.configure(definition, beta_beat.key, 9'004);
    combat_quest.set_state(flower_heavy.key, QuestObjectiveState::completed);
    combat_quest.set_state(break_target.key, QuestObjectiveState::active);
    const auto combat_signal = signal(
        QuestUiProjectionSource::combat_feedback,
        break_target,
        result(flower_heavy_trigger, flower_heavy, QuestUiResultStatus::accepted),
        result(
            break_target_outcome,
            break_target,
            QuestUiResultStatus::rejected,
            QuestUiRejectionReason::wrong_target
        )
    );
    auto unknown_trigger = combat_signal;
    unknown_trigger.primary_result.id = contracts::stable_content_key("unknown_trigger");
    expect_projection_failure_unchanged(
        producer,
        unknown_trigger,
        combat_quest,
        beta_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "unknown combat trigger"
    );
    auto unknown_outcome = combat_signal;
    unknown_outcome.secondary_result.id = contracts::stable_content_key("unknown_outcome");
    expect_projection_failure_unchanged(
        producer,
        unknown_outcome,
        combat_quest,
        beta_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "unknown combat outcome"
    );
    auto wrong_trigger_type = combat_signal;
    wrong_trigger_type.primary_result.id = break_target_outcome.key;
    expect_projection_failure_unchanged(
        producer,
        wrong_trigger_type,
        combat_quest,
        beta_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "combat trigger wrong type"
    );
    auto wrong_outcome_type = combat_signal;
    wrong_outcome_type.secondary_result.id = flower_heavy_trigger.key;
    expect_projection_failure_unchanged(
        producer,
        wrong_outcome_type,
        combat_quest,
        beta_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "combat outcome wrong type"
    );
    auto wrong_outcome_owner = combat_signal;
    wrong_outcome_owner.secondary_result.objective = guard_counter.key;
    expect_projection_failure_unchanged(
        producer,
        wrong_outcome_owner,
        combat_quest,
        beta_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "combat outcome objective mismatch"
    );
    auto rejected_trigger_with_outcome = combat_signal;
    rejected_trigger_with_outcome.primary_result.status = QuestUiResultStatus::rejected;
    rejected_trigger_with_outcome.primary_result.rejection_reason =
        QuestUiRejectionReason::wrong_target;
    expect_projection_failure_unchanged(
        producer,
        rejected_trigger_with_outcome,
        combat_quest,
        beta_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "combat rejected trigger cannot carry outcome"
    );

    StubQuestRuntime cross_beat_active;
    cross_beat_active.configure(definition, alpha_beat.key, 9'005);
    cross_beat_active.set_state(arrival_choice.key, QuestObjectiveState::active);
    cross_beat_active.set_state(training_choice.key, QuestObjectiveState::active);
    expect_projection_failure_unchanged(
        producer,
        baseline.projection.source == QuestUiProjectionSource::choice_available
            ? signal(
                  QuestUiProjectionSource::choice_available,
                  arrival_choice
              )
            : QuestUiProjectionSignal{},
        cross_beat_active,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_snapshot,
        "active objective outside current Beat"
    );
    StubQuestRuntime locked_choice_focus;
    locked_choice_focus.configure(definition, alpha_beat.key, 9'006);
    locked_choice_focus.set_state(alpha_inspect.key, QuestObjectiveState::active);
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        locked_choice_focus,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_snapshot,
        "choice focus must remain active"
    );
    const std::array<contracts::CombatActorSnapshot, 1> unknown_actor{{
        actor(999, true, false),
    }};
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        unknown_actor,
        QuestUiProjectionError::invalid_snapshot,
        "unknown hostile actor"
    );
    const std::array<contracts::CombatActorSnapshot, 2> duplicate_actor{{
        actor(101, true, false),
        actor(101, true, false),
    }};
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        duplicate_actor,
        QuestUiProjectionError::invalid_snapshot,
        "duplicate actor snapshot"
    );
    const std::array<contracts::CombatActorSnapshot, 1> impossible_actor{{
        actor(101, true, true),
    }};
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        impossible_actor,
        QuestUiProjectionError::invalid_snapshot,
        "active defeated actor"
    );
    const std::array<contracts::CombatActorSnapshot, 1> player_as_hostile{{
        actor(1, true, false),
    }};
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        player_as_hostile,
        QuestUiProjectionError::invalid_snapshot,
        "player actor cannot be reported as hostile"
    );
    auto hostile_as_player_snapshot = actor(101, true, false);
    hostile_as_player_snapshot.faction = contracts::CombatFaction::player;
    const std::array<contracts::CombatActorSnapshot, 1> hostile_as_player{{
        hostile_as_player_snapshot,
    }};
    expect_projection_failure_unchanged(
        producer,
        signal(
            QuestUiProjectionSource::choice_available,
            arrival_choice
        ),
        baseline_quest,
        alpha_safe.key,
        hostile_as_player,
        QuestUiProjectionError::invalid_snapshot,
        "hostile actor cannot be reported as player"
    );
    auto wrong_repeat_attempt = signal(
        QuestUiProjectionSource::interaction_feedback,
        arrival_choice,
        result(
            arrival_low_interaction,
            arrival_choice,
            QuestUiResultStatus::ignored_repeat,
            QuestUiRejectionReason::selection_already_committed
        )
    );
    expect_projection_failure_unchanged(
        producer,
        wrong_repeat_attempt,
        baseline_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "ignored repeat requires committed choice state"
    );
    StubQuestRuntime repeat_quest;
    repeat_quest.configure(definition, alpha_beat.key, 9'006);
    repeat_quest.set_state(arrival_choice.key, QuestObjectiveState::completed);
    repeat_quest.set_selection(arrival_choice.key, arrival_high.key);
    repeat_quest.set_state(mooring_choice.key, QuestObjectiveState::active);
    auto wrong_repeat_reason = signal(
        QuestUiProjectionSource::interaction_feedback,
        arrival_choice,
        result(
            arrival_low_interaction,
            arrival_choice,
            QuestUiResultStatus::ignored_repeat,
            QuestUiRejectionReason::wrong_target
        )
    );
    expect_projection_failure_unchanged(
        producer,
        wrong_repeat_reason,
        repeat_quest,
        alpha_safe.key,
        {},
        QuestUiProjectionError::invalid_signal,
        "ignored repeat requires selection already committed reason"
    );
}

void test_selector_status_polarity_and_missing_selector() {
    ProjectionFixture fixture;
    const auto definition = fixture.definition();
    StubQuestRuntime quest;
    quest.configure(definition, beta_beat.key, 10'001);
    quest.set_state(flower_heavy.key, QuestObjectiveState::completed);
    quest.set_state(break_target.key, QuestObjectiveState::active);
    DeterministicQuestUiProjectionProducer producer;
    expect(producer.initialize(definition) == QuestUiProjectionError::none, "selector init");
    const auto accepted = producer.project(
        signal(
            QuestUiProjectionSource::combat_feedback,
            break_target,
            result(flower_heavy_trigger, flower_heavy, QuestUiResultStatus::accepted),
            result(break_target_outcome, break_target, QuestUiResultStatus::accepted)
        ),
        quest,
        beta_safe.key,
        {}
    );
    expect(accepted.error == QuestUiProjectionError::none, "selector accepted outcome");
    expect(accepted.projection.polarity == QuestUiPolarity::positive, "accepted outcome positive");
    expect(
        accepted.projection.attempt_time_classification ==
            QuestUiAttemptTimeClassification::qualifying_combat_feedback,
        "accepted outcome derives combat feedback evidence"
    );
    quest.set_checksum(10'002);
    const auto rejected = producer.project(
        signal(
            QuestUiProjectionSource::combat_feedback,
            break_target,
            result(flower_heavy_trigger, flower_heavy, QuestUiResultStatus::accepted),
            result(
                break_target_outcome,
                break_target,
                QuestUiResultStatus::rejected,
                QuestUiRejectionReason::wrong_target
            )
        ),
        quest,
        beta_safe.key,
        {}
    );
    expect(rejected.error == QuestUiProjectionError::none, "selector rejected outcome");
    expect(rejected.projection.polarity == QuestUiPolarity::negative, "rejected outcome negative");
    expect(
        rejected.projection.attempt_time_classification ==
            QuestUiAttemptTimeClassification::qualifying_combat_feedback,
        "rejected outcome derives combat feedback evidence"
    );
    expect(
        rejected.projection.primary_result.status == QuestUiResultStatus::accepted &&
            rejected.projection.secondary_result.status == QuestUiResultStatus::rejected &&
            rejected.projection.secondary_result.rejection_reason ==
                QuestUiRejectionReason::wrong_target,
        "independent combat result slots"
    );

    ProjectionFixture missing_selector_fixture;
    missing_selector_fixture.cues[2].result_selectors = {};
    missing_selector_fixture.cues[2].attempt_evidence_rules =
        std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
            &missing_selector_fixture.mooring_load_attempt_rules[0],
            1
        };
    const auto missing_definition = missing_selector_fixture.definition();
    StubQuestRuntime quick_quest;
    quick_quest.configure(missing_definition, alpha_beat.key, 10'003);
    quick_quest.set_state(mooring_choice.key, QuestObjectiveState::completed);
    quick_quest.set_selection(mooring_choice.key, mooring_quick.key);
    quick_quest.set_state(secure_mooring.key, QuestObjectiveState::active);
    DeterministicQuestUiProjectionProducer missing_selector_producer;
    expect(
        missing_selector_producer.initialize(missing_definition) == QuestUiProjectionError::none,
        "missing selector definition remains structurally valid"
    );
    const auto direct = missing_selector_producer.project(
        signal(
            QuestUiProjectionSource::interaction_feedback,
            secure_mooring,
            result(lock_cross_interaction, secure_mooring, QuestUiResultStatus::accepted)
        ),
        quick_quest,
        alpha_safe.key,
        {}
    );
    expect(direct.error == QuestUiProjectionError::none, "missing selector baseline direct result");
    const auto stable = missing_selector_producer.snapshot();
    const auto undefined_transition = missing_selector_producer.project(
        signal(
            QuestUiProjectionSource::interaction_feedback,
            secure_mooring,
            result(
                mooring_quick_interaction,
                mooring_choice,
                QuestUiResultStatus::accepted
            )
        ),
        quick_quest,
        alpha_safe.key,
        {}
    );
    expect(
        undefined_transition.error == QuestUiProjectionError::invalid_signal,
        "undefined accepted-negative transition rejected"
    );
    expect(missing_selector_producer.snapshot() == stable, "missing selector preserves snapshot");
}

class DynamicFixture final {
  public:
    DynamicFixture(
        std::size_t objective_count,
        std::size_t interaction_count,
        bool choices_share_objective,
        QuestUiProjectionSource cue_source
    ) {
        objective_names_.reserve(objective_count);
        selection_names_.reserve(interaction_count);
        interaction_names_.reserve(interaction_count);
        objectives_.reserve(objective_count);
        interactions_.reserve(interaction_count);
        for (std::size_t index = 0; index < objective_count; ++index) {
            objective_names_.push_back("test_dynamic_objective_" + std::to_string(index));
            objectives_.push_back(contracts::content_id(objective_names_.back()));
        }
        for (std::size_t index = 0; index < interaction_count; ++index) {
            selection_names_.push_back("test_dynamic_selection_" + std::to_string(index));
            interaction_names_.push_back("test_dynamic_interaction_" + std::to_string(index));
            const auto objective_index = choices_share_objective ? 0 : index;
            interactions_.push_back(make_choice(
                contracts::content_id(interaction_names_.back()),
                objectives_[objective_index],
                contracts::content_id(selection_names_.back())
            ));
        }
        beat_ = {
            contracts::content_id("test_dynamic_beat"),
            contracts::VerticalSliceBeatKind::training,
            1,
            cell_id,
            std::span<const ContentId>{objectives_},
        };
        safe_point_ = {
            contracts::content_id("test_dynamic_safe"),
            beat_.id,
            {},
        };
        cue_objectives_[0] = objectives_[choices_share_objective ? 0 : objective_count - 1];
        attempt_evidence_rule_ = attempt_rule(
            cue_source,
            cue_objectives_[0],
            QuestUiAttemptTimeClassification::qualifying_first_visit
        );
        cue_ = {
            contracts::content_id("ui.test.dynamic"),
            beat_.id,
            source_bit(cue_source),
            std::span<const ContentId>{cue_objectives_},
            {},
            std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{
                &attempt_evidence_rule_,
                1
            },
        };
    }

    [[nodiscard]] contracts::VerticalSliceDefinition definition() const noexcept {
        contracts::VerticalSliceDefinition definition{};
        definition.id = contracts::content_id("test_dynamic_slice");
        definition.player.actor = 1;
        definition.beats = std::span<const contracts::VerticalSliceBeatDefinition>{&beat_, 1};
        definition.safe_points =
            std::span<const contracts::VerticalSliceSafePointDefinition>{&safe_point_, 1};
        definition.quest_interactions =
            std::span<const contracts::QuestInteractionDefinition>{interactions_};
        definition.quest_ui_cues =
            std::span<const contracts::QuestUiCueDefinition>{&cue_, 1};
        return definition;
    }

    [[nodiscard]] const std::vector<ContentId>& objectives() const noexcept {
        return objectives_;
    }

    [[nodiscard]] const std::vector<contracts::QuestInteractionDefinition>& interactions()
        const noexcept {
        return interactions_;
    }

    [[nodiscard]] contracts::StableContentKey beat() const noexcept {
        return beat_.id.key;
    }

    [[nodiscard]] contracts::StableContentKey safe_point() const noexcept {
        return safe_point_.id.key;
    }

    void project_all_objectives() noexcept {
        cue_.objective_ids = std::span<const ContentId>{objectives_};
    }

  private:
    std::vector<std::string> objective_names_{};
    std::vector<std::string> selection_names_{};
    std::vector<std::string> interaction_names_{};
    std::vector<ContentId> objectives_{};
    std::vector<contracts::QuestInteractionDefinition> interactions_{};
    contracts::VerticalSliceBeatDefinition beat_{};
    contracts::VerticalSliceSafePointDefinition safe_point_{};
    std::array<ContentId, 1> cue_objectives_{};
    contracts::QuestUiAttemptEvidenceRuleDefinition attempt_evidence_rule_{};
    contracts::QuestUiCueDefinition cue_{};
};

void test_capacity_fail_closed() {
    {
        DynamicFixture fixture(9, 0, false, QuestUiProjectionSource::objective_state);
        fixture.project_all_objectives();
        DeterministicQuestUiProjectionProducer producer;
        expect(
            producer.initialize(fixture.definition()) == QuestUiProjectionError::invalid_definition,
            "cue objectives over eight fail definition"
        );
    }
    {
        ProjectionFixture fixture;
        for (auto& interaction : fixture.interactions) {
            interaction.kind = contracts::QuestInteractionKind::operate;
            interaction.objective_id = secure_mooring;
            interaction.selection_id = {};
        }
        std::array<contracts::QuestUiResultSelectorDefinition, 9> selectors{};
        for (std::size_t index = 0; index < selectors.size(); ++index) {
            selectors[index] = {
                QuestUiProjectionSource::interaction_feedback,
                secure_mooring,
                fixture.interactions[index].id,
                {},
                contracts::QuestUiPolarityOverride::none,
            };
        }
        fixture.cues[2].result_selectors =
            std::span<const contracts::QuestUiResultSelectorDefinition>{selectors};
        auto definition = fixture.definition();
        definition.quest_ui_cues =
            std::span<const contracts::QuestUiCueDefinition>{&fixture.cues[2], 1};
        DeterministicQuestUiProjectionProducer producer;
        expect(
            producer.initialize(definition) == QuestUiProjectionError::invalid_definition,
            "cue result selectors over eight fail definition"
        );
    }
    {
        ProjectionFixture fixture;
        std::array<
            contracts::QuestUiAttemptEvidenceRuleDefinition,
            contracts::quest_ui_attempt_evidence_rule_capacity + 1
        > rules{};
        rules.fill(fixture.mooring_choice_attempt_rules[0]);
        fixture.cues[1].attempt_evidence_rules =
            std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{rules};
        DeterministicQuestUiProjectionProducer producer;
        expect(
            producer.initialize(fixture.definition()) ==
                QuestUiProjectionError::invalid_definition,
            "cue attempt evidence rules over capacity fail definition"
        );
    }
    {
        DynamicFixture fixture(2, 9, true, QuestUiProjectionSource::choice_available);
        DeterministicQuestUiProjectionProducer producer;
        expect(
            producer.initialize(fixture.definition()) == QuestUiProjectionError::invalid_definition,
            "choice options over eight fail definition"
        );
    }
    {
        DynamicFixture fixture(18, 17, false, QuestUiProjectionSource::objective_state);
        const auto definition = fixture.definition();
        StubQuestRuntime quest;
        quest.configure(definition, fixture.beat(), 11'001);
        quest.set_state(fixture.objectives().back().key, QuestObjectiveState::active);
        DeterministicQuestUiProjectionProducer producer;
        expect(producer.initialize(definition) == QuestUiProjectionError::none, "selected capacity init");
        const auto base_signal = signal(
            QuestUiProjectionSource::objective_state,
            fixture.objectives().back()
        );
        const auto baseline = producer.project(base_signal, quest, fixture.safe_point(), {});
        expect(baseline.error == QuestUiProjectionError::none, "selected capacity baseline");
        for (std::size_t index = 0; index < 17; ++index) {
            quest.set_state(fixture.objectives()[index].key, QuestObjectiveState::completed);
            quest.set_selection(
                fixture.objectives()[index].key,
                fixture.interactions()[index].selection_id.key
            );
        }
        quest.set_checksum(11'002);
        expect_projection_failure_unchanged(
            producer,
            base_signal,
            quest,
            fixture.safe_point(),
            {},
            QuestUiProjectionError::capacity_exceeded,
            "selected options over sixteen"
        );
    }
    {
        DynamicFixture fixture(65, 0, false, QuestUiProjectionSource::objective_state);
        const auto definition = fixture.definition();
        StubQuestRuntime quest;
        quest.configure(definition, fixture.beat(), 12'001);
        quest.set_state(fixture.objectives().back().key, QuestObjectiveState::active);
        DeterministicQuestUiProjectionProducer producer;
        expect(producer.initialize(definition) == QuestUiProjectionError::none, "retained capacity init");
        const auto base_signal = signal(
            QuestUiProjectionSource::objective_state,
            fixture.objectives().back()
        );
        const auto baseline = producer.project(base_signal, quest, fixture.safe_point(), {});
        expect(baseline.error == QuestUiProjectionError::none, "retained capacity baseline");
        for (const auto& objective : fixture.objectives()) {
            quest.set_state(objective.key, QuestObjectiveState::completed);
        }
        quest.set_checksum(12'002);
        expect_projection_failure_unchanged(
            producer,
            base_signal,
            quest,
            fixture.safe_point(),
            {},
            QuestUiProjectionError::capacity_exceeded,
            "retained objectives over sixty four"
        );
    }
    {
        ProjectionFixture fixture;
        const auto definition = fixture.definition();
        StubQuestRuntime quest;
        quest.configure(definition, beta_beat.key, 13'001);
        quest.set_state(guard_counter.key, QuestObjectiveState::active);
        DeterministicQuestUiProjectionProducer producer;
        expect(producer.initialize(definition) == QuestUiProjectionError::none, "actor capacity init");
        const auto base_signal = signal(
            QuestUiProjectionSource::objective_state,
            guard_counter
        );
        const auto baseline = producer.project(base_signal, quest, beta_safe.key, {});
        expect(baseline.error == QuestUiProjectionError::none, "actor capacity baseline");
        std::array<contracts::CombatActorSnapshot, 17> actors{};
        for (std::size_t index = 0; index < actors.size(); ++index) {
            actors[index] = actor(fixture.actor_keys[index], true, false);
        }
        expect_projection_failure_unchanged(
            producer,
            base_signal,
            quest,
            beta_safe.key,
            actors,
            QuestUiProjectionError::capacity_exceeded,
            "active actors over sixteen"
        );
    }
}

}  // namespace

int main() {
    test_authoritative_projection_catalog();
    test_choice_order_intent_and_sequence();
    test_same_beat_stage_active_focus();
    test_definition_fail_closed();
    test_missing_attempt_evidence_fails_without_state_change();
    test_runtime_fail_closed_and_snapshot_invariance();
    test_selector_status_polarity_and_missing_selector();
    test_capacity_fail_closed();
    if (failures != 0) {
        std::cerr << failures << " quest UI projection checks failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "Quest UI projection checks passed.\n";
    return EXIT_SUCCESS;
}
