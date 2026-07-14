import assert from "node:assert/strict";
import test from "node:test";

import { loadF1SliceContract } from "../tools/generate-f1-slice-contract.mjs";
import {
  applyFixtureMutation,
  loadF1UiContentFixtures,
  loadF1UiContentNegativeFixtures,
  requiredCuePolarities,
  validateF1UiContentEvent,
  validateF1UiContentFixtures
} from "../tools/validate-f1-ui-content-fixtures.mjs";

const [contract, fixtures, negativeFixtures] = await Promise.all([
  loadF1SliceContract(),
  loadF1UiContentFixtures(),
  loadF1UiContentNegativeFixtures()
]);

function event(fixtureId) {
  const found = fixtures.events.find((entry) => entry.fixtureId === fixtureId);
  assert(found, `missing fixture ${fixtureId}`);
  return found;
}

function collectStrings(value, output = []) {
  if (typeof value === "string") {
    output.push(value);
  } else if (Array.isArray(value)) {
    for (const entry of value) collectStrings(entry, output);
  } else if (value && typeof value === "object") {
    for (const entry of Object.values(value)) collectStrings(entry, output);
  }
  return output;
}

test("F1 first-two-beat UI fixtures stay synchronized with the authoritative Definition", () => {
  assert.equal(validateF1UiContentFixtures(fixtures, contract), fixtures);
  assert.equal(fixtures.events.length, 16);
  assert.deepEqual(
    new Set(fixtures.events.map((entry) => entry.panel.model.cueId)),
    new Set(requiredCuePolarities.keys())
  );

  const mooring = event("f1_rain_mooring_choice_available");
  assert.deepEqual(
    mooring.panel.model.presentation.options.map((option) => option.selectionId),
    ["f1_choice_mooring_cross_belay", "f1_choice_mooring_quick_hitch"]
  );
  assert.equal(mooring.panel.model.authority.objectiveId, "f1_objective_choose_mooring_method");
  assert.equal(fixtures.source.authoritative, false);
  assert.equal(fixtures.source.authoritativeDefinition, "content/design/f1-vertical-slice.json");
  assert.equal(Object.hasOwn(fixtures, "flow"), false);
  assert.equal(Object.hasOwn(fixtures, "autoAdvance"), false);

  const eventKeys = [
    "contractVersion",
    "eventType",
    "fixtureId",
    "panel",
    "sequence",
    "sessionGeneration"
  ];
  const panelKeys = [
    "announcements",
    "focus",
    "inputContext",
    "mode",
    "model",
    "surface",
    "title"
  ];
  for (const entry of fixtures.events) {
    assert.deepEqual(Object.keys(entry).sort(), eventKeys);
    assert.deepEqual(Object.keys(entry.panel).sort(), panelKeys);
  }
});

test("F1 UI projections express branch choice, recoverable craft error, and bell ordering", () => {
  const arrival = event("f1_rain_arrival_clue_available");
  assert.deepEqual(
    arrival.panel.model.presentation.options.map((option) => ({
      selectionId: option.selectionId,
      explicitSkip: option.explicitSkip,
      rewardDelta: option.rewardDelta,
      convergesToObjectiveId: option.convergesToObjectiveId
    })),
    [
      {
        selectionId: "f1_choice_arrival_high_water_tags",
        explicitSkip: false,
        rewardDelta: false,
        convergesToObjectiveId: "f1_objective_read_ferry_condition"
      },
      {
        selectionId: "f1_choice_arrival_drowned_manifest",
        explicitSkip: false,
        rewardDelta: false,
        convergesToObjectiveId: "f1_objective_read_ferry_condition"
      },
      {
        selectionId: "f1_choice_arrival_follow_bell",
        explicitSkip: true,
        rewardDelta: false,
        convergesToObjectiveId: "f1_objective_read_ferry_condition"
      }
    ]
  );

  const repeat = event("f1_rain_arrival_clue_repeat_locked");
  assert.equal(repeat.panel.model.authority.interactionResult.status, "ignored_repeat");
  assert.equal(repeat.panel.model.presentation.repeatEffect, "no_progress_no_reward");

  const overload = event("f1_rain_mooring_quick_hitch_overloaded");
  assert.equal(overload.panel.model.authority.objectiveState, "active");
  assert.equal(
    overload.panel.model.presentation.correctionInteractionId,
    "f1_interaction_correct_overloaded_quick_hitch"
  );
  assert.equal(overload.panel.model.presentation.deviceState, "quick-hitch-overloaded");

  const bellRejected = event("f1_rain_bell_rejected_unread_code");
  assert.equal(bellRejected.panel.model.authority.objectiveState, "locked");
  assert.equal(
    bellRejected.panel.model.authority.pendingObjectiveId,
    "f1_objective_read_workshop_bell_code"
  );
  assert.equal(
    bellRejected.panel.model.authority.interactionResult.rejectionReason,
    "prerequisite_incomplete"
  );
});

test("F1 UI projections distinguish phase, accepted action, wrong target, retry, and return", () => {
  const laneChoice = event("f1_training_lane_choice_available");
  assert.deepEqual(
    laneChoice.panel.model.presentation.options.map((option) => option.selectionId),
    ["f1_choice_training_windward_lane", "f1_choice_training_leeward_lane"]
  );
  assert.equal(laneChoice.panel.model.authority.objectiveId, "f1_objective_choose_training_lane");

  assert.deepEqual(
    event("f1_training_guard_phase_windward").panel.model.authority.activeActorKeys,
    [104]
  );
  assert.deepEqual(
    event("f1_training_flower_phase").panel.model.authority.activeActorKeys,
    [108]
  );

  const accepted = event("f1_training_eavesguard_proof_accepted");
  assert.equal(accepted.panel.model.authority.combatResult.triggerStatus, "accepted");
  assert.equal(accepted.panel.model.authority.objectiveState, "completed");

  const wrongTarget = event("f1_training_flower_proof_wrong_target");
  assert.equal(wrongTarget.panel.model.authority.combatResult.triggerStatus, "accepted");
  assert.equal(wrongTarget.panel.model.authority.combatResult.outcomeStatus, "rejected");
  assert.equal(wrongTarget.panel.model.authority.combatResult.rejectionReason, "wrong_target");
  assert.equal(wrongTarget.panel.model.authority.objectiveState, "active");

  const retry = event("f1_training_failure_retry_offer");
  assert.equal(retry.panel.model.authority.safePointId, "f1_safe_point_shen_yan_training");
  assert.equal(retry.panel.model.authority.attemptTimeClassification, "failure_retry_excluded");
  assert.deepEqual(retry.panel.model.authority.activeActorKeys, [104]);

  const returned = event("f1_training_return_resume_selected_lane");
  assert.deepEqual(returned.panel.model.authority.activeActorKeys, [108]);
  assert.deepEqual(returned.panel.model.authority.defeatedActorKeys, [107]);
  assert.equal(returned.panel.model.presentation.shapeToken, "selected_lane_rebuilt");

  const eavesguardResume = event("f1_training_retry_resume_eavesguard");
  assert.equal(
    eavesguardResume.panel.model.authority.objectiveId,
    "f1_objective_eavesguard_counter"
  );
  assert.equal(
    eavesguardResume.panel.model.authority.attemptTimeClassification,
    "resume_no_duplicate_progress"
  );
  assert.deepEqual(eavesguardResume.panel.model.authority.activeActorKeys, [104]);

  const flowerOffer = event("f1_training_failure_retry_offer_flower");
  assert.equal(
    flowerOffer.panel.model.authority.objectiveId,
    "f1_objective_flower_turn_counter"
  );
  assert.equal(
    flowerOffer.panel.model.authority.attemptTimeClassification,
    "failure_retry_excluded"
  );
  assert.deepEqual(flowerOffer.panel.model.authority.activeActorKeys, [108]);
});

test("F1 player-facing copy never exposes qualifying-time audit terminology", () => {
  for (const entry of fixtures.events) {
    const playerFacingStrings = collectStrings({
      title: entry.panel.title,
      announcements: entry.panel.announcements,
      presentation: entry.panel.model.presentation
    });
    assert.doesNotMatch(
      playerFacingStrings.join("\n"),
      /合格玩法时长|失败重试排除|计入[^\n]*时长/,
      `${entry.fixtureId} leaks QA time-accounting language into player-facing copy`
    );
  }

  assert.equal(
    event("f1_training_failure_retry_offer").panel.announcements.assertive,
    "可以立即再试或离开；失败次数不会推进训练。"
  );
  assert.equal(
    event("f1_training_failure_retry_offer_flower").panel.announcements.assertive,
    "可以立即再试或离开；失败次数不会推进训练。"
  );
});

test("F1 UI negative fixture recipes fail closed for stale or UI-owned authority", () => {
  assert.equal(negativeFixtures.fixtureContractVersion, "1.0.0");
  assert.equal(negativeFixtures.cases.length, 14);
  for (const negative of negativeFixtures.cases) {
    const mutated = applyFixtureMutation(fixtures, negative);
    assert.throws(
      () => validateF1UiContentEvent(mutated, contract),
      new RegExp(negative.expectedError),
      negative.id
    );
  }

  const missingNegativeCoverage = structuredClone(fixtures);
  missingNegativeCoverage.events = missingNegativeCoverage.events.filter(
    (entry) =>
      entry.panel.model.cueId !== "ui.f1.rain.bell-feedback" ||
      entry.panel.model.polarity !== "negative"
  );
  assert.throws(
    () => validateF1UiContentFixtures(missingNegativeCoverage, contract),
    /cue ui\.f1\.rain\.bell-feedback is missing negative coverage/
  );

  const wrongKnownCue = structuredClone(
    event("f1_rain_mooring_quick_hitch_overloaded")
  );
  wrongKnownCue.panel.model.cueId = "ui.f1.rain.bell-feedback";
  assert.throws(
    () => validateF1UiContentEvent(wrongKnownCue, contract),
    /does not own objective/
  );

  const falsePositive = structuredClone(
    event("f1_rain_mooring_quick_hitch_overloaded")
  );
  falsePositive.panel.model.polarity = "positive";
  assert.throws(
    () => validateF1UiContentEvent(falsePositive, contract),
    /expected negative polarity/
  );

  const invalidTimeClass = structuredClone(event("f1_training_flower_phase"));
  invalidTimeClass.panel.model.authority.attemptTimeClassification =
    "qualifying_magic_wait";
  assert.throws(
    () => validateF1UiContentEvent(invalidTimeClass, contract),
    /attemptTimeClassification is invalid/
  );

  const wrongTimeSource = structuredClone(event("f1_training_flower_phase"));
  wrongTimeSource.panel.model.authority.attemptTimeClassification =
    "qualifying_combat_proof";
  assert.throws(
    () => validateF1UiContentEvent(wrongTimeSource, contract),
    /attemptTimeClassification does not match the exact Definition rule/
  );

  const acceptedWithRejection = structuredClone(event("f1_rain_bell_answered"));
  acceptedWithRejection.panel.model.authority.interactionResult.rejectionReason =
    "prerequisite_incomplete";
  assert.throws(
    () => validateF1UiContentEvent(acceptedWithRejection, contract),
    /accepted interaction cannot have a rejection reason/
  );

  const playerAsHostile = structuredClone(event("f1_training_guard_phase_windward"));
  playerAsHostile.panel.model.authority.activeActorKeys[0] = 1;
  assert.throws(
    () => validateF1UiContentEvent(playerAsHostile, contract),
    /unknown actor 1 for hostile projection/
  );

  const rejectedWithoutReason = structuredClone(
    event("f1_training_flower_proof_wrong_target")
  );
  rejectedWithoutReason.panel.model.authority.combatResult.rejectionReason = null;
  assert.throws(
    () => validateF1UiContentEvent(rejectedWithoutReason, contract),
    /rejected combat result requires a known rejection reason/
  );

  const missingTransitionSelector = structuredClone(contract);
  missingTransitionSelector.questUiCues.find(
    (cue) => cue.id === "ui.f1.rain.mooring-load"
  ).resultSelectors = [];
  assert.throws(
    () => validateF1UiContentEvent(
      event("f1_rain_mooring_quick_hitch_overloaded"),
      missingTransitionSelector
    ),
    /lacks an interaction transition selector/
  );

  const missingAttemptRule = structuredClone(contract);
  const phaseCue = missingAttemptRule.questUiCues.find(
    (cue) => cue.id === "ui.f1.training.phase"
  );
  phaseCue.attemptEvidenceRules = phaseCue.attemptEvidenceRules.filter(
    (rule) => rule.objectiveId !== "f1_objective_flower_turn_counter"
  );
  assert.throws(
    () => validateF1UiContentEvent(
      event("f1_training_flower_phase"),
      missingAttemptRule
    ),
    /has no exact Definition attempt evidence rule/
  );

  const ambiguousAttemptRule = structuredClone(contract);
  const recoveryCue = ambiguousAttemptRule.questUiCues.find(
    (cue) => cue.id === "ui.f1.training.recovery"
  );
  recoveryCue.attemptEvidenceRules.push(
    structuredClone(recoveryCue.attemptEvidenceRules[0])
  );
  assert.throws(
    () => validateF1UiContentEvent(
      event("f1_training_failure_retry_offer"),
      ambiguousAttemptRule
    ),
    /has ambiguous Definition attempt evidence rules/
  );

  const driftedRuleClassification = structuredClone(contract);
  driftedRuleClassification.questUiCues.find(
    (cue) => cue.id === "ui.f1.training.phase"
  ).attemptEvidenceRules.find(
    (rule) => rule.objectiveId === "f1_objective_flower_turn_counter"
  ).classification = "qualifying_first_visit";
  assert.throws(
    () => validateF1UiContentEvent(
      event("f1_training_flower_phase"),
      driftedRuleClassification
    ),
    /attemptTimeClassification does not match the exact Definition rule/
  );

  const driftedRuleSentinel = structuredClone(contract);
  driftedRuleSentinel.questUiCues.find(
    (cue) => cue.id === "ui.f1.training.choice.lane"
  ).attemptEvidenceRules[0].primaryResult.resultId = "f1_trigger_missing";
  assert.throws(
    () => validateF1UiContentEvent(
      event("f1_training_lane_choice_available"),
      driftedRuleSentinel
    ),
    /has no exact Definition attempt evidence rule/
  );
});
