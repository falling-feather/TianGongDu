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
  "ignored_repeat",
  "required"
]);
const allowedCombatStatuses = new Set(["accepted", "rejected", "pending", "not_applicable"]);
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
    safePointById: new Map(contract.safePoints.map((safePoint) => [safePoint.id, safePoint])),
    actorKeys: new Set(contract.combatBootstrap.actors.map((actor) => actor.actorKey))
  };
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
  if (result.rejectionReason !== null) assertString(result.rejectionReason, "invalid rejection reason");
}

function validateCombatResult(result, indexes) {
  if (result === null) return;
  assert(result && typeof result === "object", "combat result must be null or an object");
  assert(allowedCombatStatuses.has(result.triggerStatus), "invalid combat trigger status");
  assert(allowedCombatStatuses.has(result.outcomeStatus), "invalid combat outcome status");
  if (result.triggerId !== null) {
    const trigger = indexes.triggerById.get(result.triggerId);
    assert(trigger, `unknown combat trigger ${result.triggerId}`);
    validateObjective(result.triggerObjectiveId, indexes, "combat trigger objective");
    assert(
      trigger.objectiveId === result.triggerObjectiveId,
      `combat trigger ${result.triggerId} does not belong to objective ${result.triggerObjectiveId}`
    );
  }
  if (result.outcomeId !== null) {
    const outcome = indexes.outcomeById.get(result.outcomeId);
    assert(outcome, `unknown combat outcome ${result.outcomeId}`);
    validateObjective(result.outcomeObjectiveId, indexes, "combat outcome objective");
    assert(
      outcome.objectiveId === result.outcomeObjectiveId,
      `combat outcome ${result.outcomeId} does not belong to objective ${result.outcomeObjectiveId}`
    );
  }
  if (result.rejectionReason !== null) assertString(result.rejectionReason, "invalid combat rejection reason");
}

function validateActorKeys(keys, indexes, label) {
  assert(Array.isArray(keys), `${label} must be an array`);
  assert(new Set(keys).size === keys.length, `${label} contains duplicate actor keys`);
  for (const actorKey of keys) {
    assert(Number.isInteger(actorKey) && actorKey > 0, `${label} contains an invalid actor key`);
    assert(indexes.actorKeys.has(actorKey), `unknown actor ${actorKey}`);
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
  assertString(authority.attemptTimeClassification, "attemptTimeClassification is missing");
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
