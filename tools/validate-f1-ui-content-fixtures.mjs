import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

import { loadF1SliceContract } from "./generate-f1-slice-contract.mjs";

const root = resolve(import.meta.dirname, "..");
const fixturePath = resolve(root, "tests/fixtures/f1-first-two-beats-ui-events.json");
const negativeFixturePath = resolve(
  root,
  "tests/fixtures/f1-first-two-beats-ui-events.negative.json"
);

export const requiredCuePolarities = new Map([
  ["ui.f1.rain.choice.arrival-clue", new Set(["positive", "negative"])],
  ["ui.f1.rain.choice.mooring-method", new Set(["positive"])],
  ["ui.f1.rain.mooring-load", new Set(["positive", "negative"])],
  ["ui.f1.rain.bell-feedback", new Set(["positive", "negative"])],
  ["ui.f1.training.choice.lane", new Set(["positive"])],
  ["ui.f1.training.phase", new Set(["positive"])],
  ["ui.f1.training.action-proof", new Set(["positive", "negative"])],
  ["ui.f1.training.recovery", new Set(["recovery"])]
]);

const allowedSurfaces = new Set([
  "gameplay",
  "dialogue",
  "tutorial",
  "failure",
  "choice"
]);
const allowedInputContexts = new Set(["gameplay", "dialogue", "menu", "system"]);
const allowedObjectiveStates = new Set(["locked", "active", "completed"]);
const allowedPolarities = new Set(["positive", "negative", "recovery"]);
const allowedInteractionStatuses = new Set([
  "accepted",
  "rejected",
  "ignored_repeat"
]);
const allowedCombatStatuses = new Set(["accepted", "rejected", "pending", "not_applicable"]);
const allowedRejectionReasons = new Set([
  "prerequisite_incomplete",
  "selection_already_committed",
  "wrong_target"
]);
const allowedAttemptTimeClassifications = new Set([
  "qualifying_first_visit",
  "repeat_no_progress",
  "qualifying_craft_decision",
  "qualifying_error_feedback",
  "qualifying_wrong_order_feedback",
  "qualifying_craft_confirmation",
  "qualifying_dialogue_decision",
  "qualifying_training_risk",
  "qualifying_combat_proof",
  "qualifying_combat_feedback",
  "failure_retry_excluded",
  "resume_no_duplicate_progress"
]);
const forbiddenUiAuthorityFields = new Set([
  "advanceObjective",
  "animationComplete",
  "grantReward",
  "localSelected",
  "persistState",
  "reward",
  "rewardDedupKey",
  "rewardId",
  "wallClockMs",
  "waitMilliseconds"
]);

function fail(message) {
  throw new Error(`F1 UI content fixture: ${message}`);
}

function assert(condition, message) {
  if (!condition) fail(message);
}

function assertString(value, message) {
  assert(typeof value === "string" && value.length > 0, message);
}

function collectForbiddenFields(value, path = "panel.model", found = []) {
  if (Array.isArray(value)) {
    value.forEach((entry, index) => collectForbiddenFields(entry, `${path}[${index}]`, found));
    return found;
  }
  if (!value || typeof value !== "object") return found;
  for (const [key, child] of Object.entries(value)) {
    if (forbiddenUiAuthorityFields.has(key)) found.push(`${path}.${key}`);
    collectForbiddenFields(child, `${path}.${key}`, found);
  }
  return found;
}

function buildIndexes(contract) {
  const objectiveToBeat = new Map();
  for (const beat of contract.beats) {
    for (const objectiveId of beat.objectiveIds) objectiveToBeat.set(objectiveId, beat.id);
  }
  const interactionById = new Map(
    contract.questInteractions.map((interaction) => [interaction.id, interaction])
  );
  const selectionToObjective = new Map();
  const selectionsByObjective = new Map();
  for (const interaction of contract.questInteractions) {
    if (!interaction.selectionId) continue;
    selectionToObjective.set(interaction.selectionId, interaction.objectiveId);
    const selections = selectionsByObjective.get(interaction.objectiveId) ?? new Set();
    selections.add(interaction.selectionId);
    selectionsByObjective.set(interaction.objectiveId, selections);
  }
  return {
    firstTwoBeatIds: new Set(contract.beats.slice(0, 2).map((beat) => beat.id)),
    objectiveToBeat,
    interactionById,
    selectionToObjective,
    selectionsByObjective,
    triggerById: new Map(contract.questCombatTriggers.map((trigger) => [trigger.id, trigger])),
    outcomeById: new Map(contract.questCombatOutcomes.map((outcome) => [outcome.id, outcome])),
    cueById: new Map(contract.questUiCues.map((cue) => [cue.id, cue])),
    safePointById: new Map(contract.safePoints.map((safePoint) => [safePoint.id, safePoint])),
    hostileActorKeys: new Set(
      contract.combatBootstrap.actors
        .filter((actor) => actor.faction === "hostile")
        .map((actor) => actor.actorKey)
    )
  };
}

function projectionSource(event) {
  const { panel } = event;
  const authority = panel.model.authority;
  if (panel.surface === "choice") return "choice_available";
  if (panel.surface === "failure") return "recovery_offer";
  if (panel.model.polarity === "recovery") return "recovery_resume";
  if (authority.interactionResult !== null) return "interaction_feedback";
  if (authority.combatResult !== null) return "combat_feedback";
  return "objective_state";
}

function resultSelectorFor(cue, source, authority) {
  const primaryResultId = source === "interaction_feedback"
    ? authority.interactionResult?.interactionId ?? null
    : authority.combatResult?.triggerId ?? null;
  const secondaryResultId = source === "combat_feedback"
    ? authority.combatResult?.outcomeId ?? null
    : null;
  return cue.resultSelectors.find(
    (selector) =>
      selector.source === source &&
      selector.objectiveId === authority.objectiveId &&
      selector.primaryResultId === primaryResultId &&
      selector.secondaryResultId === secondaryResultId
  ) ?? null;
}

function notApplicableAttemptResult() {
  return {
    resultId: null,
    status: "not_applicable",
    rejectionReason: "none"
  };
}

function attemptEvidenceResults(source, authority) {
  if (source === "interaction_feedback") {
    const result = authority.interactionResult;
    return {
      primaryResult: {
        resultId: result.interactionId,
        status: result.status,
        rejectionReason: result.rejectionReason ?? "none"
      },
      secondaryResult: notApplicableAttemptResult()
    };
  }
  if (source === "combat_feedback") {
    const result = authority.combatResult;
    return {
      primaryResult: {
        resultId: result.triggerId,
        status: result.triggerStatus,
        rejectionReason: result.triggerStatus === "rejected"
          ? result.rejectionReason
          : "none"
      },
      secondaryResult: result.outcomeId === null
        ? notApplicableAttemptResult()
        : {
            resultId: result.outcomeId,
            status: result.outcomeStatus,
            rejectionReason: result.outcomeStatus === "rejected"
              ? result.rejectionReason
              : "none"
          }
    };
  }
  return {
    primaryResult: notApplicableAttemptResult(),
    secondaryResult: notApplicableAttemptResult()
  };
}

function sameAttemptResult(actual, expected) {
  return actual.resultId === expected.resultId &&
    actual.status === expected.status &&
    actual.rejectionReason === expected.rejectionReason;
}

function validateAttemptEvidenceProjection(event, indexes) {
  const model = event.panel.model;
  const authority = model.authority;
  const source = projectionSource(event);
  const cue = indexes.cueById.get(model.cueId);
  const expected = attemptEvidenceResults(source, authority);
  const matches = cue.attemptEvidenceRules.filter(
    (rule) => rule.source === source &&
      rule.objectiveId === authority.objectiveId &&
      sameAttemptResult(rule.primaryResult, expected.primaryResult) &&
      sameAttemptResult(rule.secondaryResult, expected.secondaryResult)
  );
  assert(
    matches.length > 0,
    `cue ${model.cueId} has no exact Definition attempt evidence rule`
  );
  assert(
    matches.length === 1,
    `cue ${model.cueId} has ambiguous Definition attempt evidence rules`
  );
  assert(
    authority.attemptTimeClassification === matches[0].classification,
    `attemptTimeClassification does not match the exact Definition rule`
  );
}

function validateCueProjection(event, indexes) {
  const model = event.panel.model;
  const authority = model.authority;
  const cue = indexes.cueById.get(model.cueId);
  assert(cue, `unknown cue ${model.cueId}`);
  const source = projectionSource(event);
  assert(cue.beatId === authority.beatId, `cue ${model.cueId} belongs to another beat`);
  assert(cue.sources.includes(source), `cue ${model.cueId} does not accept source ${source}`);
  assert(
    cue.objectiveIds.includes(authority.objectiveId),
    `cue ${model.cueId} does not own objective ${authority.objectiveId}`
  );

  const selector = resultSelectorFor(cue, source, authority);
  if (source === "interaction_feedback") {
    const result = authority.interactionResult;
    if (result.objectiveId !== authority.objectiveId) {
      assert(selector, `cue ${model.cueId} lacks an interaction transition selector`);
    }
  }
  if (source === "combat_feedback") {
    const result = authority.combatResult;
    if (result.triggerObjectiveId !== authority.objectiveId ||
        (result.outcomeId !== null && result.outcomeObjectiveId !== result.triggerObjectiveId)) {
      assert(selector, `cue ${model.cueId} lacks a combat transition selector`);
    }
  }

  let expectedPolarity = "positive";
  if (source === "recovery_offer" || source === "recovery_resume") {
    expectedPolarity = "recovery";
  } else {
    const effectiveStatus = source === "interaction_feedback"
      ? authority.interactionResult.status
      : source === "combat_feedback"
        ? authority.combatResult.outcomeStatus === "not_applicable"
          ? authority.combatResult.triggerStatus
          : authority.combatResult.outcomeStatus
        : null;
    if (["rejected", "ignored_repeat"].includes(effectiveStatus) ||
        selector?.polarityOverride === "negative") {
      expectedPolarity = "negative";
    }
  }
  assert(
    model.polarity === expectedPolarity,
    `cue ${model.cueId} expected ${expectedPolarity} polarity`
  );
}

function validateObjective(objectiveId, indexes, label) {
  assertString(objectiveId, `${label} is missing`);
  assert(indexes.objectiveToBeat.has(objectiveId), `unknown objective ${objectiveId}`);
}

function validateObjectiveInBeat(objectiveId, beatId, indexes, label) {
  validateObjective(objectiveId, indexes, label);
  assert(
    indexes.objectiveToBeat.get(objectiveId) === beatId,
    `${label} ${objectiveId} does not belong to beat ${beatId}`
  );
}

function validateSelection(selection, indexes) {
  assert(selection && typeof selection === "object", "selected option must be an object");
  validateObjective(selection.objectiveId, indexes, "selected option objective");
  assertString(selection.selectionId, "selected option selectionId is missing");
  const owner = indexes.selectionToObjective.get(selection.selectionId);
  assert(owner, `unknown selection ${selection.selectionId}`);
  assert(
    owner === selection.objectiveId,
    `selection ${selection.selectionId} does not belong to objective ${selection.objectiveId}`
  );
}

function validateInteractionResult(result, indexes) {
  if (result === null) return;
  assert(result && typeof result === "object", "interaction result must be null or an object");
  const interaction = indexes.interactionById.get(result.interactionId);
  assert(interaction, `unknown interaction ${result.interactionId}`);
  validateObjective(result.objectiveId, indexes, "interaction result objective");
  assert(
    interaction.objectiveId === result.objectiveId,
    `interaction ${result.interactionId} does not belong to objective ${result.objectiveId}`
  );
  assert(
    allowedInteractionStatuses.has(result.status),
    `invalid interaction status ${result.status}`
  );
  if (result.status === "accepted") {
    assert(result.rejectionReason === null, "accepted interaction cannot have a rejection reason");
  } else {
    assert(
      allowedRejectionReasons.has(result.rejectionReason),
      "rejected or repeated interaction requires a known rejection reason"
    );
  }
  if (result.status === "ignored_repeat") {
    assert(
      result.rejectionReason === "selection_already_committed",
      "ignored repeat must report selection_already_committed"
    );
  }
}

function validateCombatResult(result, indexes) {
  if (result === null) return;
  assert(result && typeof result === "object", "combat result must be null or an object");
  assert(allowedCombatStatuses.has(result.triggerStatus), "invalid combat trigger status");
  assert(allowedCombatStatuses.has(result.outcomeStatus), "invalid combat outcome status");
  assert(result.triggerId !== null, "combat feedback requires a trigger result");
  assert(result.triggerStatus !== "not_applicable", "combat trigger cannot be not_applicable");
  const trigger = indexes.triggerById.get(result.triggerId);
  assert(trigger, `unknown combat trigger ${result.triggerId}`);
  validateObjective(result.triggerObjectiveId, indexes, "combat trigger objective");
  assert(
    trigger.objectiveId === result.triggerObjectiveId,
    `combat trigger ${result.triggerId} does not belong to objective ${result.triggerObjectiveId}`
  );
  if (result.outcomeId === null) {
    assert(result.outcomeObjectiveId === null, "missing outcome cannot have an objective");
    assert(result.outcomeStatus === "not_applicable", "missing outcome must be not_applicable");
  } else {
    const outcome = indexes.outcomeById.get(result.outcomeId);
    assert(outcome, `unknown combat outcome ${result.outcomeId}`);
    validateObjective(result.outcomeObjectiveId, indexes, "combat outcome objective");
    assert(
      outcome.objectiveId === result.outcomeObjectiveId,
      `combat outcome ${result.outcomeId} does not belong to objective ${result.outcomeObjectiveId}`
    );
    assert(result.outcomeStatus !== "not_applicable", "authored outcome needs a result status");
    assert(result.triggerStatus === "accepted", "combat outcome requires an accepted trigger");
  }
  const effectiveStatus = result.outcomeId === null
    ? result.triggerStatus
    : result.outcomeStatus;
  if (effectiveStatus === "rejected") {
    assert(
      allowedRejectionReasons.has(result.rejectionReason),
      "rejected combat result requires a known rejection reason"
    );
  } else {
    assert(result.rejectionReason === null, "non-rejected combat result cannot have a rejection reason");
  }
}

function validateActorKeys(keys, indexes, label) {
  assert(Array.isArray(keys), `${label} must be an array`);
  assert(new Set(keys).size === keys.length, `${label} contains duplicate actor keys`);
  for (const actorKey of keys) {
    assert(Number.isInteger(actorKey) && actorKey > 0, `${label} contains an invalid actor key`);
    assert(
      indexes.hostileActorKeys.has(actorKey),
      `unknown actor ${actorKey} for hostile projection`
    );
  }
}

function validatePresentation(presentation, indexes, objectiveId, surface) {
  assert(presentation && typeof presentation === "object", "presentation is missing");
  assertString(presentation.primaryText, "presentation primaryText is missing");
  assertString(presentation.shapeToken, "presentation shapeToken is missing");
  if (surface === "choice") {
    assert(Array.isArray(presentation.options), "choice presentation options must be an array");
  }
  if (presentation.options !== undefined) {
    assert(Array.isArray(presentation.options), "presentation options must be an array");
    const seenSelections = new Set();
    for (const option of presentation.options) {
      assertString(option.selectionId, "presentation option selectionId is missing");
      assert(!seenSelections.has(option.selectionId), `duplicate presentation option ${option.selectionId}`);
      seenSelections.add(option.selectionId);
      const owner = indexes.selectionToObjective.get(option.selectionId);
      assert(owner, `unknown selection ${option.selectionId}`);
      assert(
        owner === objectiveId,
        `presentation selection ${option.selectionId} does not belong to objective ${objectiveId}`
      );
      assertString(option.label, "presentation option label is missing");
      if (option.convergesToObjectiveId !== undefined) {
        validateObjective(option.convergesToObjectiveId, indexes, "option convergence objective");
      }
    }
    if (surface === "choice") {
      const authoredSelections = indexes.selectionsByObjective.get(objectiveId) ?? new Set();
      const exactlyCoversObjective =
        authoredSelections.size > 0 &&
        authoredSelections.size === seenSelections.size &&
        [...authoredSelections].every((selectionId) => seenSelections.has(selectionId));
      assert(
        exactlyCoversObjective,
        `choice options do not exactly cover objective ${objectiveId}`
      );
    }
  }
  if (presentation.correctionInteractionId !== undefined) {
    assert(
      indexes.interactionById.has(presentation.correctionInteractionId),
      `unknown correction interaction ${presentation.correctionInteractionId}`
    );
  }
}

export function validateF1UiContentEvent(event, contract, indexes = buildIndexes(contract)) {
  assert(event && typeof event === "object", "event must be an object");
  assert(event.contractVersion === "1.0.0-prototype", "event contract version drifted");
  assert(/^[a-z0-9_]+$/.test(event.fixtureId ?? ""), "fixtureId is invalid");
  assert(Number.isInteger(event.sequence) && event.sequence > 0, "event sequence is invalid");
  assertString(event.sessionGeneration, "sessionGeneration is missing");
  assert(event.eventType === "panel_replace", "eventType must be panel_replace");

  const panel = event.panel;
  assert(panel && typeof panel === "object", "panel is missing");
  assert(allowedSurfaces.has(panel.surface), `unsupported surface ${panel.surface}`);
  assertString(panel.mode, "panel mode is missing");
  assert(allowedInputContexts.has(panel.inputContext), "panel inputContext is invalid");
  assert(typeof panel.title === "string", "panel title must be a string");
  assert(panel.focus && typeof panel.focus === "object", "panel focus is missing");
  assert(
    panel.focus.entryTarget === null || typeof panel.focus.entryTarget === "string",
    "panel focus entryTarget is invalid"
  );
  assert(
    panel.focus.restoreTarget === null || typeof panel.focus.restoreTarget === "string",
    "panel focus restoreTarget is invalid"
  );
  assert(panel.announcements && typeof panel.announcements === "object", "announcements missing");
  assert(typeof panel.announcements.polite === "string", "polite announcement is invalid");
  assert(typeof panel.announcements.assertive === "string", "assertive announcement is invalid");

  const model = panel.model;
  assert(model && typeof model === "object", "panel model is missing");
  assert(requiredCuePolarities.has(model.cueId), `unknown cue ${model.cueId}`);
  assert(allowedPolarities.has(model.polarity), `invalid fixture polarity ${model.polarity}`);
  const forbidden = collectForbiddenFields(model);
  assert(forbidden.length === 0, `forbidden UI authority field ${forbidden[0]}`);

  const authority = model.authority;
  assert(authority && typeof authority === "object", "authority projection is missing");
  assert(indexes.firstTwoBeatIds.has(authority.beatId), `beat ${authority.beatId} is outside the first two beats`);
  validateObjectiveInBeat(authority.objectiveId, authority.beatId, indexes, "current objective");
  assert(allowedObjectiveStates.has(authority.objectiveState), "objective state is invalid");
  assert(Number.isInteger(authority.questSequence) && authority.questSequence > 0, "invalid quest sequence");
  assert(Array.isArray(authority.selectedOptions), "selectedOptions must be an array");
  authority.selectedOptions.forEach((selection) => validateSelection(selection, indexes));
  validateInteractionResult(authority.interactionResult, indexes);
  validateCombatResult(authority.combatResult, indexes);
  validateActorKeys(authority.activeActorKeys, indexes, "activeActorKeys");
  validateActorKeys(authority.defeatedActorKeys, indexes, "defeatedActorKeys");
  const overlap = authority.activeActorKeys.filter((actorKey) => authority.defeatedActorKeys.includes(actorKey));
  assert(overlap.length === 0, "an actor cannot be active and defeated in the same projection");

  assert(Array.isArray(authority.retainedObjectiveIds), "retainedObjectiveIds must be an array");
  for (const objectiveId of authority.retainedObjectiveIds) {
    validateObjective(objectiveId, indexes, "retained objective");
  }
  if (authority.pendingObjectiveId !== null) {
    validateObjectiveInBeat(
      authority.pendingObjectiveId,
      authority.beatId,
      indexes,
      "pending objective"
    );
  }
  if (authority.safePointId !== null) {
    const safePoint = indexes.safePointById.get(authority.safePointId);
    assert(safePoint, `unknown safe point ${authority.safePointId}`);
    assert(
      safePoint.beatId === authority.beatId,
      `safe point ${authority.safePointId} does not belong to beat ${authority.beatId}`
    );
  }
  assert(
    allowedAttemptTimeClassifications.has(authority.attemptTimeClassification),
    "attemptTimeClassification is invalid"
  );
  validateCueProjection(event, indexes);
  validateAttemptEvidenceProjection(event, indexes);
  validatePresentation(model.presentation, indexes, authority.objectiveId, panel.surface);
  return event;
}

export function validateF1UiContentFixtures(document, contract) {
  assert(document?.fixtureContractVersion === "1.0.0", "fixture contract version drifted");
  assert(document.source?.taskId === "F1-GAME-01-CONTENT-A", "task ID drifted");
  assert(document.source?.sliceId === contract.id, "slice ID drifted");
  assert(document.source?.authoritative === false, "fixtures must remain non-authoritative");
  assert(
    document.source?.authoritativeDefinition === "content/design/f1-vertical-slice.json",
    "authoritative Definition path drifted"
  );
  assert(Array.isArray(document.events) && document.events.length > 0, "events are missing");
  assert(
    contract.questUiCues.length === requiredCuePolarities.size &&
      contract.questUiCues.every((cue) => requiredCuePolarities.has(cue.id)),
    "fixture cue catalog drifted from the authoritative Definition"
  );

  const indexes = buildIndexes(contract);
  const fixtureIds = new Set();
  const observedPolarities = new Map(
    [...requiredCuePolarities.keys()].map((cueId) => [cueId, new Set()])
  );
  let priorSequence = 0;
  let sessionGeneration = null;
  for (const event of document.events) {
    validateF1UiContentEvent(event, contract, indexes);
    assert(!fixtureIds.has(event.fixtureId), `duplicate fixtureId ${event.fixtureId}`);
    fixtureIds.add(event.fixtureId);
    assert(event.sequence > priorSequence, "event sequences must be strictly increasing");
    priorSequence = event.sequence;
    sessionGeneration ??= event.sessionGeneration;
    assert(event.sessionGeneration === sessionGeneration, "fixture sessionGeneration drifted");
    observedPolarities.get(event.panel.model.cueId).add(event.panel.model.polarity);
  }

  for (const [cueId, required] of requiredCuePolarities) {
    const observed = observedPolarities.get(cueId);
    for (const polarity of required) {
      assert(observed.has(polarity), `cue ${cueId} is missing ${polarity} coverage`);
    }
  }
  return document;
}

function pathSegment(segment) {
  return /^\d+$/.test(segment) ? Number(segment) : segment;
}

export function applyFixtureMutation(document, mutation) {
  const base = document.events.find((event) => event.fixtureId === mutation.baseFixtureId);
  assert(base, `negative fixture base ${mutation.baseFixtureId} is missing`);
  const event = structuredClone(base);
  const segments = mutation.path.split(".").map(pathSegment);
  assert(segments.length > 0, "negative fixture mutation path is empty");
  let target = event;
  for (const segment of segments.slice(0, -1)) {
    assert(target?.[segment] !== undefined, `negative fixture path ${mutation.path} is invalid`);
    target = target[segment];
  }
  const final = segments.at(-1);
  if (mutation.operation === "remove") {
    if (Array.isArray(target)) target.splice(final, 1);
    else delete target[final];
  } else {
    assert(
      mutation.operation === "replace" || mutation.operation === "add",
      `negative fixture operation ${mutation.operation} is invalid`
    );
    target[final] = structuredClone(mutation.value);
  }
  return event;
}

export async function loadF1UiContentFixtures() {
  return JSON.parse(await readFile(fixturePath, "utf8"));
}

export async function loadF1UiContentNegativeFixtures() {
  return JSON.parse(await readFile(negativeFixturePath, "utf8"));
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(import.meta.filename)) {
  const [contract, fixtures, negativeFixtures] = await Promise.all([
    loadF1SliceContract(),
    loadF1UiContentFixtures(),
    loadF1UiContentNegativeFixtures()
  ]);
  validateF1UiContentFixtures(fixtures, contract);
  for (const negative of negativeFixtures.cases) {
    const event = applyFixtureMutation(fixtures, negative);
    try {
      validateF1UiContentEvent(event, contract);
      fail(`negative fixture ${negative.id} unexpectedly passed`);
    } catch (error) {
      if (!new RegExp(negative.expectedError).test(error.message)) throw error;
    }
  }
  console.log(
    `F1 UI content fixtures valid: ${fixtures.events.length} events, ${negativeFixtures.cases.length} negative cases`
  );
}
