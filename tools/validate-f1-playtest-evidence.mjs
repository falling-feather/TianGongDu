import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

import { loadF1SliceContract } from "./generate-f1-slice-contract.mjs";

const root = resolve(import.meta.dirname, "..");
const templatePath = resolve(
  root,
  "tests/fixtures/f1-first-two-beats-playtest-evidence.template.json"
);

export const requiredParticipantIds = Object.freeze(["P01", "P02", "P03", "P04", "P05"]);
export const minimumMedianQualifiedMs = 15 * 60 * 1000;
export const maximumMedianQualifiedMs = 17 * 60 * 1000;
export const maximumNoveltyGapMs = 90 * 1000;

export const allowedQualifiedActivities = new Set([
  "exploration",
  "choice",
  "craft_judgment",
  "dialogue",
  "combat"
]);
export const allowedNoveltyKinds = new Set([
  "judgment",
  "spatial_change",
  "risk",
  "feedback"
]);
export const allowedExclusionReasons = new Set([
  "loading",
  "pause_or_settings",
  "idle",
  "failure_retry",
  "repeat_grind",
  "long_empty_traversal",
  "forced_slowdown",
  "waiting"
]);
export const allowedRecoveryKinds = new Set([
  "out_of_order",
  "explicit_skip",
  "repeat_interaction",
  "failure_retry",
  "leave_and_return"
]);

const arrivalSelections = Object.freeze([
  "f1_choice_arrival_high_water_tags",
  "f1_choice_arrival_drowned_manifest",
  "f1_choice_arrival_follow_bell"
]);
const mooringSelections = Object.freeze([
  "f1_choice_mooring_cross_belay",
  "f1_choice_mooring_quick_hitch"
]);
const trainingSelections = Object.freeze([
  "f1_choice_training_windward_lane",
  "f1_choice_training_leeward_lane"
]);

function fail(message) {
  throw new Error(`F1 playtest evidence: ${message}`);
}

function assert(condition, message) {
  if (!condition) fail(message);
}

function assertString(value, message) {
  assert(typeof value === "string" && value.length > 0, message);
}

function assertInteger(value, message, minimum = 0) {
  assert(Number.isSafeInteger(value) && value >= minimum, message);
}

function sameValues(left, right) {
  return left.length === right.length && left.every((value, index) => value === right[index]);
}

function assertExactAcceptance(acceptance) {
  assert(acceptance && typeof acceptance === "object", "acceptance policy is missing");
  assert(acceptance.requiredParticipants === 5, "required participant count drifted");
  assert(acceptance.minimumMedianQualifiedMs === minimumMedianQualifiedMs, "minimum median drifted");
  assert(acceptance.maximumMedianQualifiedMs === maximumMedianQualifiedMs, "maximum median drifted");
  assert(acceptance.maximumNoveltyGapMs === maximumNoveltyGapMs, "novelty gap limit drifted");
  assert(acceptance.allCompleteWithoutOralGuidance === true, "no-guidance gate drifted");
  assert(acceptance.excludeNonQualifyingTime === true, "time exclusion gate drifted");
  assert(
    Array.isArray(acceptance.excludedReasons) &&
      sameValues(acceptance.excludedReasons, [...allowedExclusionReasons]),
    "excluded reason policy drifted"
  );
}

export function f1DefinitionSha256(contract) {
  return createHash("sha256").update(JSON.stringify(contract)).digest("hex");
}

function buildContractIndexes(contract) {
  assert(Array.isArray(contract?.beats) && contract.beats.length >= 2, "authoritative beats are missing");
  const firstTwoBeats = contract.beats.slice(0, 2);
  assert(
    firstTwoBeats[0].id === "f1_beat_rain_ferry_arrival" &&
      firstTwoBeats[1].id === "f1_beat_shen_yan_training",
    "first-two-beat boundary drifted"
  );
  const objectiveOrder = firstTwoBeats.flatMap((beat) => beat.objectiveIds);
  const objectiveToBeat = new Map();
  for (const beat of firstTwoBeats) {
    for (const objectiveId of beat.objectiveIds) objectiveToBeat.set(objectiveId, beat.id);
  }
  const selections = new Set(
    contract.questInteractions
      .map((interaction) => interaction.selectionId)
      .filter((selectionId) => typeof selectionId === "string" && selectionId.length > 0)
  );
  for (const selectionId of [...arrivalSelections, ...mooringSelections, ...trainingSelections]) {
    assert(selections.has(selectionId), `authoritative selection ${selectionId} is missing`);
  }
  return {
    firstTwoBeats,
    objectiveOrder,
    objectiveToBeat,
    objectiveIds: new Set(objectiveOrder)
  };
}

function validateBuild(build) {
  assert(build && typeof build === "object", "build binding is missing");
  assert(/^[0-9a-f]{40}$/.test(build.commit ?? ""), "build commit must be a full hexadecimal SHA");
  assert(
    new Set(["synthetic", "web-single", "windows-msvc", "windows-clang"]).has(build.platform),
    `unsupported playtest platform ${build.platform}`
  );
  assert(typeof build.contentBridgeIntegrated === "boolean", "content bridge state is missing");
  assert(typeof build.playerFacingUi === "boolean", "player-facing UI state is missing");
  assert(typeof build.assetRequirementsIntegrated === "boolean", "asset integration state is missing");
  assert(typeof build.developerOverlayVisible === "boolean", "developer overlay state is missing");
}

function validateRouteSelection(selectionId, allowed, knownSelections, label) {
  if (selectionId === null) return;
  assertString(selectionId, `${label} selection is invalid`);
  assert(knownSelections.has(selectionId), `${label} selection ${selectionId} is not authoritative`);
  assert(allowed.includes(selectionId), `${label} selection ${selectionId} is outside the route contract`);
}

function validateExclusions(exclusions, sessionEndedMs) {
  assert(Array.isArray(exclusions), "exclusions must be an array");
  let priorEnd = 0;
  for (const exclusion of exclusions) {
    assert(exclusion && typeof exclusion === "object", "exclusion must be an object");
    assertInteger(exclusion.startMs, "exclusion startMs is invalid");
    assertInteger(exclusion.endMs, "exclusion endMs is invalid", 1);
    assert(exclusion.startMs < exclusion.endMs, "exclusion interval is empty or reversed");
    assert(exclusion.startMs >= priorEnd, "exclusion intervals overlap or are unsorted");
    assert(exclusion.endMs <= sessionEndedMs, "exclusion exceeds the observed session");
    assert(
      allowedExclusionReasons.has(exclusion.reason),
      `unsupported exclusion reason ${exclusion.reason}`
    );
    assertString(exclusion.note, "exclusion note is missing");
    priorEnd = exclusion.endMs;
  }
}

function overlapMs(startMs, endMs, exclusions) {
  let total = 0;
  for (const exclusion of exclusions) {
    const overlapStart = Math.max(startMs, exclusion.startMs);
    const overlapEnd = Math.min(endMs, exclusion.endMs);
    if (overlapEnd > overlapStart) total += overlapEnd - overlapStart;
  }
  return total;
}

function isInsideExclusion(atMs, exclusions) {
  return exclusions.some((exclusion) => atMs > exclusion.startMs && atMs < exclusion.endMs);
}

function validateStuckMoments(participant, indexes) {
  assert(Array.isArray(participant.stuckMoments), "stuckMoments must be an array");
  assert(
    typeof participant.noStuckMomentObserved === "boolean",
    "noStuckMomentObserved must be explicit"
  );
  assert(
    participant.noStuckMomentObserved === (participant.stuckMoments.length === 0),
    "stuck-moment empty state is inconsistent"
  );
  for (const moment of participant.stuckMoments) {
    assertInteger(moment.atMs, "stuck moment atMs is invalid");
    assert(moment.atMs <= participant.sessionEndedMs, "stuck moment exceeds the session");
    assert(indexes.objectiveIds.has(moment.objectiveId), `unknown stuck objective ${moment.objectiveId}`);
    assertInteger(moment.durationMs, "stuck moment durationMs is invalid", 1);
    assert(
      typeof moment.resolvedWithoutGuidance === "boolean",
      "stuck moment guidance resolution is missing"
    );
    assertString(moment.note, "stuck moment note is missing");
  }
}

function validateGuidance(participant) {
  assert(Array.isArray(participant.oralGuidance), "oralGuidance must be an array");
  for (const intervention of participant.oralGuidance) {
    assertInteger(intervention.atMs, "oral guidance atMs is invalid");
    assert(intervention.atMs <= participant.sessionEndedMs, "oral guidance exceeds the session");
    assertString(intervention.note, "oral guidance note is missing");
  }
}

function validateRepetition(participant, indexes) {
  const repetition = participant.repetition;
  assert(repetition && typeof repetition === "object", "repetition observation is missing");
  assertInteger(repetition.rating1To5, "repetition rating is invalid", 1);
  assert(repetition.rating1To5 <= 5, "repetition rating exceeds 5");
  assertString(repetition.summary, "repetition summary is missing");
  assert(Array.isArray(repetition.repeatedObjectiveIds), "repeatedObjectiveIds must be an array");
  assert(
    new Set(repetition.repeatedObjectiveIds).size === repetition.repeatedObjectiveIds.length,
    "repeatedObjectiveIds contains duplicates"
  );
  for (const objectiveId of repetition.repeatedObjectiveIds) {
    assert(indexes.objectiveIds.has(objectiveId), `unknown repeated objective ${objectiveId}`);
  }
}

function validateRecoveryEvents(participant) {
  assert(Array.isArray(participant.recoveryEvents), "recoveryEvents must be an array");
  for (const recovery of participant.recoveryEvents) {
    assertInteger(recovery.atMs, "recovery event atMs is invalid");
    assert(recovery.atMs <= participant.sessionEndedMs, "recovery event exceeds the session");
    assert(allowedRecoveryKinds.has(recovery.kind), `unsupported recovery kind ${recovery.kind}`);
    assert(typeof recovery.deadlocked === "boolean", "recovery deadlock result is missing");
    assertString(recovery.note, "recovery event note is missing");
  }
}

function validateParticipant(participant, contract, indexes) {
  assert(participant && typeof participant === "object", "participant record must be an object");
  assert(requiredParticipantIds.includes(participant.id), `participant ID ${participant.id} is invalid`);
  assert(
    typeof participant.previouslyViewedDeveloperUi === "boolean",
    "developer UI exposure must be explicit"
  );
  assert(typeof participant.completed === "boolean", "completion result must be explicit");
  assertString(participant.recordingRef, "recordingRef is missing");
  assert(/^[0-9a-f]{64}$/.test(participant.recordingSha256 ?? ""), "recordingSha256 is invalid");
  assert(participant.controlGrantedMs === 0, "controlGrantedMs must be the zero boundary");
  assertInteger(participant.sessionEndedMs, "sessionEndedMs is invalid", 1);

  const knownSelections = new Set(
    contract.questInteractions
      .map((interaction) => interaction.selectionId)
      .filter((selectionId) => typeof selectionId === "string" && selectionId.length > 0)
  );
  assert(participant.route && typeof participant.route === "object", "route record is missing");
  validateRouteSelection(
    participant.route.arrivalSelectionId,
    arrivalSelections,
    knownSelections,
    "arrival"
  );
  validateRouteSelection(
    participant.route.mooringSelectionId,
    mooringSelections,
    knownSelections,
    "mooring"
  );
  validateRouteSelection(
    participant.route.trainingSelectionId,
    trainingSelections,
    knownSelections,
    "training"
  );

  validateExclusions(participant.exclusions, participant.sessionEndedMs);
  const events = participant.objectiveEvents;
  assert(events && typeof events === "object" && !Array.isArray(events), "objectiveEvents is missing");
  const observedObjectiveIds = Object.keys(events);
  for (const objectiveId of observedObjectiveIds) {
    assert(indexes.objectiveIds.has(objectiveId), `unknown observed objective ${objectiveId}`);
  }
  const expectedPrefix = indexes.objectiveOrder.slice(0, observedObjectiveIds.length);
  assert(
    expectedPrefix.every((objectiveId) => Object.hasOwn(events, objectiveId)),
    "observed objectives must form an unbroken authoritative prefix"
  );
  assert(
    observedObjectiveIds.length <= indexes.objectiveOrder.length,
    "too many observed objective events"
  );

  let priorAtMs = participant.controlGrantedMs;
  let maximumGap = 0;
  const qualifiedByActivity = Object.fromEntries(
    [...allowedQualifiedActivities].map((activity) => [activity, 0])
  );
  for (const objectiveId of expectedPrefix) {
    const event = events[objectiveId];
    assert(event && typeof event === "object", `event ${objectiveId} is invalid`);
    assertInteger(event.atMs, `event ${objectiveId} atMs is invalid`, 1);
    assert(event.atMs > priorAtMs, `event ${objectiveId} is not monotonic`);
    assert(event.atMs <= participant.sessionEndedMs, `event ${objectiveId} exceeds the session`);
    assert(
      !isInsideExclusion(event.atMs, participant.exclusions),
      `event ${objectiveId} occurs inside excluded time`
    );
    assert(
      allowedQualifiedActivities.has(event.activitySincePrevious),
      `event ${objectiveId} has invalid activity ${event.activitySincePrevious}`
    );
    assert(
      allowedNoveltyKinds.has(event.noveltyKind),
      `event ${objectiveId} has invalid novelty ${event.noveltyKind}`
    );
    assertString(event.note, `event ${objectiveId} note is missing`);
    const qualifyingGap =
      event.atMs - priorAtMs - overlapMs(priorAtMs, event.atMs, participant.exclusions);
    assert(qualifyingGap >= 0, `event ${objectiveId} has negative qualifying time`);
    maximumGap = Math.max(maximumGap, qualifyingGap);
    qualifiedByActivity[event.activitySincePrevious] += qualifyingGap;
    priorAtMs = event.atMs;
  }

  const trailingQualifyingGap =
    participant.sessionEndedMs -
    priorAtMs -
    overlapMs(priorAtMs, participant.sessionEndedMs, participant.exclusions);
  assert(trailingQualifyingGap >= 0, "trailing qualifying time is negative");
  maximumGap = Math.max(maximumGap, trailingQualifyingGap);

  const rainObjectives = indexes.firstTwoBeats[0].objectiveIds;
  const trainingObjectives = indexes.firstTwoBeats[1].objectiveIds;
  const rainLast = rainObjectives.at(-1);
  const trainingLast = trainingObjectives.at(-1);
  if (Object.hasOwn(events, rainLast)) {
    assert(
      participant.rainBeatCompletedMs === events[rainLast].atMs,
      "rain beat completion boundary does not match its final Objective"
    );
  } else {
    assert(participant.rainBeatCompletedMs === null, "incomplete rain beat must not claim completion");
  }
  if (participant.completed) {
    assert(
      observedObjectiveIds.length === indexes.objectiveOrder.length,
      "completed participant is missing Objective evidence"
    );
    assert(
      participant.trainingBeatCompletedMs === events[trainingLast].atMs &&
        participant.trainingBeatCompletedMs === participant.sessionEndedMs,
      "training completion boundary does not match the final Objective"
    );
  } else {
    assert(
      participant.trainingBeatCompletedMs === null,
      "incomplete participant must not claim training completion"
    );
  }

  validateGuidance(participant);
  validateStuckMoments(participant, indexes);
  validateRepetition(participant, indexes);
  validateRecoveryEvents(participant);

  const excludedMs = participant.exclusions.reduce(
    (sum, exclusion) => sum + exclusion.endMs - exclusion.startMs,
    0
  );
  const qualifiedMs = participant.sessionEndedMs - excludedMs;
  const rainBoundary = participant.rainBeatCompletedMs ?? participant.sessionEndedMs;
  const rainQualifiedMs =
    rainBoundary - overlapMs(0, rainBoundary, participant.exclusions);
  const trainingQualifiedMs = participant.rainBeatCompletedMs === null
    ? 0
    : participant.sessionEndedMs -
      participant.rainBeatCompletedMs -
      overlapMs(participant.rainBeatCompletedMs, participant.sessionEndedMs, participant.exclusions);
  return {
    id: participant.id,
    completed: participant.completed,
    noGuidance: participant.oralGuidance.length === 0,
    noDeveloperUiExposure: participant.previouslyViewedDeveloperUi === false,
    noDeadlock: participant.recoveryEvents.every((event) => event.deadlocked === false),
    qualifiedMs,
    rainQualifiedMs,
    trainingQualifiedMs,
    excludedMs,
    maximumNoveltyGapMs: maximumGap,
    qualifiedByActivity,
    route: structuredClone(participant.route),
    recordingRef: participant.recordingRef,
    recordingSha256: participant.recordingSha256
  };
}

function median(values) {
  if (values.length === 0) return null;
  const sorted = [...values].sort((left, right) => left - right);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 1
    ? sorted[middle]
    : Math.floor((sorted[middle - 1] + sorted[middle]) / 2);
}

function includesAll(observed, required) {
  const values = new Set(observed.filter((value) => value !== null));
  return required.every((value) => values.has(value));
}

function isSyntheticRecording(summary) {
  return summary.recordingRef.startsWith("synthetic:") || /^0+$/.test(summary.recordingSha256);
}

export function validateF1PlaytestEvidence(document, contract) {
  assert(document && typeof document === "object", "evidence document must be an object");
  assert(document.contractVersion === "1.0.0", "contract version drifted");
  assert(document.evidenceState === "observed", "observed evidenceState is required");
  assert(
    new Set(["synthetic_contract_fixture", "human_observation"]).has(document.evidenceClass),
    `unsupported evidence class ${document.evidenceClass}`
  );
  assert(document.taskId === "F1-GAME-01-CONTENT-A", "task ID drifted");
  assert(document.sliceId === contract.id, "slice ID drifted");
  assert(
    document.authoritativeDefinition === "content/design/f1-vertical-slice.json",
    "authoritative Definition path drifted"
  );
  assert(document.definitionSha256 === f1DefinitionSha256(contract), "Definition digest drifted");
  assertString(document.capturedAtUtc, "capturedAtUtc is missing");
  assert(!Number.isNaN(Date.parse(document.capturedAtUtc)), "capturedAtUtc is invalid");
  assertString(document.observerId, "observerId is missing");
  validateBuild(document.build);
  assertExactAcceptance(document.acceptance);
  assert(
    Array.isArray(document.participants) && document.participants.length === 5,
    "exactly five participant records are required"
  );

  const indexes = buildContractIndexes(contract);
  const ids = document.participants.map((participant) => participant.id);
  assert(new Set(ids).size === ids.length, "participant IDs contain duplicates");
  assert(
    requiredParticipantIds.every((participantId) => ids.includes(participantId)),
    "participant IDs must cover P01 through P05"
  );
  const participantSummaries = document.participants.map((participant) =>
    validateParticipant(participant, contract, indexes)
  );
  participantSummaries.sort(
    (left, right) => requiredParticipantIds.indexOf(left.id) - requiredParticipantIds.indexOf(right.id)
  );

  const completedQualified = participantSummaries
    .filter((participant) => participant.completed)
    .map((participant) => participant.qualifiedMs);
  const medianQualifiedMs =
    completedQualified.length === requiredParticipantIds.length ? median(completedQualified) : null;
  const allCompleted = participantSummaries.every((participant) => participant.completed);
  const allNoGuidance = participantSummaries.every((participant) => participant.noGuidance);
  const allUnexposed = participantSummaries.every(
    (participant) => participant.noDeveloperUiExposure
  );
  const allWithinCadence = participantSummaries.every(
    (participant) => participant.maximumNoveltyGapMs <= maximumNoveltyGapMs
  );
  const allNoDeadlock = participantSummaries.every((participant) => participant.noDeadlock);
  const routeCoverage = {
    arrival: includesAll(
      participantSummaries.map((participant) => participant.route.arrivalSelectionId),
      arrivalSelections
    ),
    mooring: includesAll(
      participantSummaries.map((participant) => participant.route.mooringSelectionId),
      mooringSelections
    ),
    training: includesAll(
      participantSummaries.map((participant) => participant.route.trainingSelectionId),
      trainingSelections
    )
  };
  const actualHumanEvidence =
    document.evidenceClass === "human_observation" &&
    document.build.platform !== "synthetic" &&
    !/^0+$/.test(document.build.commit) &&
    participantSummaries.every((participant) => !isSyntheticRecording(participant));
  const integratedPlayerBuild =
    document.build.contentBridgeIntegrated === true &&
    document.build.playerFacingUi === true &&
    document.build.assetRequirementsIntegrated === true &&
    document.build.developerOverlayVisible === false;
  const medianInWindow =
    medianQualifiedMs !== null &&
    medianQualifiedMs >= minimumMedianQualifiedMs &&
    medianQualifiedMs <= maximumMedianQualifiedMs;

  const gateChecks = {
    actualHumanEvidence,
    integratedPlayerBuild,
    allCompleted,
    allNoGuidance,
    allUnexposed,
    allWithinCadence,
    allNoDeadlock,
    medianInWindow,
    routeCoverage: Object.values(routeCoverage).every(Boolean)
  };
  return {
    contractValid: true,
    blindPlaytestGateMet: Object.values(gateChecks).every(Boolean),
    gateChecks,
    routeCoverage,
    participantSummaries,
    completedParticipantCount: completedQualified.length,
    medianQualifiedMs
  };
}

export function validateF1PlaytestTemplate(document, contract) {
  assert(document && typeof document === "object", "template document must be an object");
  assert(document.contractVersion === "1.0.0", "template contract version drifted");
  assert(document.evidenceClass === "human_observation", "template evidence class drifted");
  assert(document.evidenceState === "pending", "template must remain pending");
  assert(document.taskId === "F1-GAME-01-CONTENT-A", "template task ID drifted");
  assert(document.sliceId === contract.id, "template slice ID drifted");
  assert(
    document.authoritativeDefinition === "content/design/f1-vertical-slice.json",
    "template authoritative path drifted"
  );
  assert(document.definitionSha256 === null, "pending template must not claim a Definition digest");
  assert(document.capturedAtUtc === null, "pending template must not claim capture time");
  assert(document.observerId === null, "pending template must not claim an observer");
  assert(document.build === null, "pending template must not claim a build");
  assertExactAcceptance(document.acceptance);
  assert(Array.isArray(document.participants) && document.participants.length === 0, "pending template must not contain samples");
  return document;
}

function activityForObjective(objectiveId) {
  if (/(meet|review|finish)/.test(objectiveId)) return "dialogue";
  if (/choose/.test(objectiveId)) return "choice";
  if (/(counter|commit|break|enter_flower)/.test(objectiveId)) return "combat";
  if (/(secure|release|raise|sound|take)/.test(objectiveId)) return "craft_judgment";
  return "exploration";
}

function buildSyntheticParticipant(contract, indexes, definition) {
  const objectiveCount = indexes.objectiveOrder.length;
  const targetMinuteTotal = indexes.firstTwoBeats.reduce(
    (sum, beat) => sum + beat.targetMinutes,
    0
  );
  const rainQualifiedMs = Math.round(
    definition.qualifiedMs * indexes.firstTwoBeats[0].targetMinutes / targetMinuteTotal
  );
  const perBeatQualified = [rainQualifiedMs, definition.qualifiedMs - rainQualifiedMs];
  const qualifyingTimes = [];
  let qualifyingCursor = 0;
  indexes.firstTwoBeats.forEach((beat, beatIndex) => {
    const baseDuration = Math.floor(perBeatQualified[beatIndex] / beat.objectiveIds.length);
    let remainder = perBeatQualified[beatIndex] - baseDuration * beat.objectiveIds.length;
    for (let index = 0; index < beat.objectiveIds.length; ++index) {
      const duration = baseDuration + (remainder > 0 ? 1 : 0);
      if (remainder > 0) --remainder;
      qualifyingCursor += duration;
      qualifyingTimes.push(qualifyingCursor);
    }
  });

  const exclusions = [];
  for (const authored of definition.exclusions) {
    const qualifyingStart = qualifyingTimes[authored.afterObjectiveIndex];
    const earlierExcluded = exclusions.reduce(
      (sum, exclusion) => sum + exclusion.endMs - exclusion.startMs,
      0
    );
    const startMs = qualifyingStart + earlierExcluded;
    exclusions.push({
      startMs,
      endMs: startMs + authored.durationMs,
      reason: authored.reason,
      note: `synthetic contract exclusion: ${authored.reason}`
    });
  }

  const objectiveEvents = {};
  for (let index = 0; index < objectiveCount; ++index) {
    const addedExcluded = definition.exclusions
      .filter((exclusion) => exclusion.afterObjectiveIndex < index)
      .reduce((sum, exclusion) => sum + exclusion.durationMs, 0);
    const objectiveId = indexes.objectiveOrder[index];
    objectiveEvents[objectiveId] = {
      atMs: qualifyingTimes[index] + addedExcluded,
      activitySincePrevious: activityForObjective(objectiveId),
      noveltyKind: [...allowedNoveltyKinds][index % allowedNoveltyKinds.size],
      note: `synthetic contract event ${index + 1}; not human evidence`
    };
  }
  const rainLast = indexes.firstTwoBeats[0].objectiveIds.at(-1);
  const trainingLast = indexes.firstTwoBeats[1].objectiveIds.at(-1);
  const failure = exclusions.find((exclusion) => exclusion.reason === "failure_retry");
  return {
    id: definition.id,
    previouslyViewedDeveloperUi: false,
    completed: true,
    recordingRef: `synthetic:${definition.id}`,
    recordingSha256: "0".repeat(64),
    controlGrantedMs: 0,
    sessionEndedMs: objectiveEvents[trainingLast].atMs,
    rainBeatCompletedMs: objectiveEvents[rainLast].atMs,
    trainingBeatCompletedMs: objectiveEvents[trainingLast].atMs,
    route: structuredClone(definition.route),
    objectiveEvents,
    exclusions,
    oralGuidance: [],
    noStuckMomentObserved: failure === undefined,
    stuckMoments: failure === undefined
      ? []
      : [{
          atMs: failure.startMs,
          objectiveId: indexes.firstTwoBeats[1].objectiveIds[3],
          durationMs: failure.endMs - failure.startMs,
          resolvedWithoutGuidance: true,
          note: "synthetic failure-recovery observation; not a player result"
        }],
    repetition: {
      rating1To5: 2,
      summary: "synthetic contract wording; no player quote",
      repeatedObjectiveIds: []
    },
    recoveryEvents: failure === undefined
      ? []
      : [{
          atMs: failure.endMs,
          kind: "failure_retry",
          deadlocked: false,
          note: "synthetic recovery path; not a player result"
        }]
  };
}

export function createSyntheticF1PlaytestEvidence(contract) {
  const indexes = buildContractIndexes(contract);
  const participantDefinitions = [
    {
      id: "P01",
      qualifiedMs: 900_000,
      route: {
        arrivalSelectionId: arrivalSelections[0],
        mooringSelectionId: mooringSelections[0],
        trainingSelectionId: trainingSelections[0]
      },
      exclusions: []
    },
    {
      id: "P02",
      qualifiedMs: 930_000,
      route: {
        arrivalSelectionId: arrivalSelections[1],
        mooringSelectionId: mooringSelections[1],
        trainingSelectionId: trainingSelections[1]
      },
      exclusions: [{ afterObjectiveIndex: 7, durationMs: 30_000, reason: "pause_or_settings" }]
    },
    {
      id: "P03",
      qualifiedMs: 960_000,
      route: {
        arrivalSelectionId: arrivalSelections[2],
        mooringSelectionId: mooringSelections[0],
        trainingSelectionId: trainingSelections[1]
      },
      exclusions: [{ afterObjectiveIndex: 15, durationMs: 45_000, reason: "failure_retry" }]
    },
    {
      id: "P04",
      qualifiedMs: 990_000,
      route: {
        arrivalSelectionId: arrivalSelections[0],
        mooringSelectionId: mooringSelections[1],
        trainingSelectionId: trainingSelections[0]
      },
      exclusions: [
        { afterObjectiveIndex: 5, durationMs: 20_000, reason: "idle" },
        { afterObjectiveIndex: 18, durationMs: 20_000, reason: "repeat_grind" }
      ]
    },
    {
      id: "P05",
      qualifiedMs: 1_020_000,
      route: {
        arrivalSelectionId: arrivalSelections[1],
        mooringSelectionId: mooringSelections[0],
        trainingSelectionId: trainingSelections[1]
      },
      exclusions: [{ afterObjectiveIndex: 10, durationMs: 30_000, reason: "waiting" }]
    }
  ];
  return {
    contractVersion: "1.0.0",
    evidenceClass: "synthetic_contract_fixture",
    evidenceState: "observed",
    taskId: "F1-GAME-01-CONTENT-A",
    sliceId: contract.id,
    authoritativeDefinition: "content/design/f1-vertical-slice.json",
    definitionSha256: f1DefinitionSha256(contract),
    capturedAtUtc: "2000-01-01T00:00:00.000Z",
    observerId: "synthetic-contract-fixture",
    build: {
      commit: "0".repeat(40),
      platform: "synthetic",
      contentBridgeIntegrated: true,
      playerFacingUi: true,
      assetRequirementsIntegrated: true,
      developerOverlayVisible: false
    },
    acceptance: {
      requiredParticipants: 5,
      minimumMedianQualifiedMs,
      maximumMedianQualifiedMs,
      maximumNoveltyGapMs,
      allCompleteWithoutOralGuidance: true,
      excludeNonQualifyingTime: true,
      excludedReasons: [...allowedExclusionReasons]
    },
    participants: participantDefinitions.map((definition) =>
      buildSyntheticParticipant(contract, indexes, definition)
    )
  };
}

export async function loadF1PlaytestEvidenceTemplate() {
  return JSON.parse(await readFile(templatePath, "utf8"));
}

function printableSummary(result) {
  return {
    contractValid: result.contractValid,
    blindPlaytestGateMet: result.blindPlaytestGateMet,
    completedParticipantCount: result.completedParticipantCount,
    medianQualifiedMinutes:
      result.medianQualifiedMs === null ? null : result.medianQualifiedMs / 60_000,
    gateChecks: result.gateChecks,
    routeCoverage: result.routeCoverage,
    participants: result.participantSummaries.map((participant) => ({
      id: participant.id,
      completed: participant.completed,
      rainQualifiedMinutes: participant.rainQualifiedMs / 60_000,
      trainingQualifiedMinutes: participant.trainingQualifiedMs / 60_000,
      totalQualifiedMinutes: participant.qualifiedMs / 60_000,
      excludedMinutes: participant.excludedMs / 60_000,
      maximumNoveltyGapSeconds: participant.maximumNoveltyGapMs / 1000,
      noGuidance: participant.noGuidance
    }))
  };
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(import.meta.filename)) {
  const args = process.argv.slice(2);
  const contract = await loadF1SliceContract();
  if (args.includes("--synthetic")) {
    const evidence = createSyntheticF1PlaytestEvidence(contract);
    const result = validateF1PlaytestEvidence(evidence, contract);
    if (args.includes("--json")) console.log(JSON.stringify(evidence, null, 2));
    else console.log(JSON.stringify(printableSummary(result), null, 2));
    if (args.includes("--require-blind-gate") && !result.blindPlaytestGateMet) process.exitCode = 1;
  } else {
    const fileArgument = args.find((argument) => !argument.startsWith("--"));
    const document = fileArgument
      ? JSON.parse(await readFile(resolve(fileArgument), "utf8"))
      : await loadF1PlaytestEvidenceTemplate();
    if (document.evidenceState === "pending") {
      validateF1PlaytestTemplate(document, contract);
      console.log("F1 first-two-beat playtest template valid: P01-P05 remain pending");
      if (args.includes("--require-blind-gate")) process.exitCode = 1;
    } else {
      const result = validateF1PlaytestEvidence(document, contract);
      console.log(JSON.stringify(printableSummary(result), null, 2));
      if (args.includes("--require-blind-gate") && !result.blindPlaytestGateMet) process.exitCode = 1;
    }
  }
}
