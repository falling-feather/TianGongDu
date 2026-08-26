import assert from "node:assert/strict";
import test from "node:test";

import { loadF1SliceContract } from "../tools/generate-f1-slice-contract.mjs";
import {
  allowedExclusionReasons,
  allowedQualifiedActivities,
  createSyntheticF1PlaytestEvidence,
  f1DefinitionSha256,
  loadF1PlaytestEvidenceTemplate,
  maximumNoveltyGapMs,
  validateF1PlaytestEvidence,
  validateF1PlaytestTemplate
} from "../tools/validate-f1-playtest-evidence.mjs";

const [contract, template] = await Promise.all([
  loadF1SliceContract(),
  loadF1PlaytestEvidenceTemplate()
]);

function syntheticEvidence() {
  return createSyntheticF1PlaytestEvidence(contract);
}

test("F1 first-two-beat playtest template stays pending and cannot claim observations", () => {
  assert.equal(validateF1PlaytestTemplate(template, contract), template);
  assert.equal(template.evidenceClass, "human_observation");
  assert.equal(template.evidenceState, "pending");
  assert.equal(template.definitionSha256, null);
  assert.equal(template.build, null);
  assert.deepEqual(template.participants, []);

  const fabricatedPending = structuredClone(template);
  fabricatedPending.definitionSha256 = f1DefinitionSha256(contract);
  assert.throws(
    () => validateF1PlaytestTemplate(fabricatedPending, contract),
    /pending template must not claim a Definition digest/
  );
});

test("synthetic evidence proves the calculator contract without becoming release evidence", () => {
  const evidence = syntheticEvidence();
  const result = validateF1PlaytestEvidence(evidence, contract);

  assert.equal(result.contractValid, true);
  assert.equal(result.blindPlaytestGateMet, false);
  assert.equal(result.gateChecks.actualHumanEvidence, false);
  assert.equal(result.gateChecks.integratedPlayerBuild, true);
  assert.equal(result.gateChecks.allCompleted, true);
  assert.equal(result.gateChecks.allNoGuidance, true);
  assert.equal(result.gateChecks.allWithinCadence, true);
  assert.equal(result.gateChecks.medianInWindow, true);
  assert.equal(result.gateChecks.routeCoverage, true);
  assert.equal(result.medianQualifiedMs, 960_000);
  assert.deepEqual(
    result.participantSummaries.map((participant) => participant.qualifiedMs),
    [900_000, 930_000, 960_000, 990_000, 1_020_000]
  );
  assert.deepEqual(result.routeCoverage, {
    arrival: true,
    mooring: true,
    training: true
  });
  for (const participant of result.participantSummaries) {
    assert(participant.maximumNoveltyGapMs <= maximumNoveltyGapMs);
    assert.equal(participant.rainQualifiedMs + participant.trainingQualifiedMs, participant.qualifiedMs);
    assert.equal(
      Object.values(participant.qualifiedByActivity).reduce((sum, value) => sum + value, 0),
      participant.qualifiedMs
    );
    assert.deepEqual(
      new Set(Object.keys(participant.qualifiedByActivity)),
      allowedQualifiedActivities
    );
  }
});

test("every forbidden time category remains excluded from qualifying minutes", () => {
  const evidence = syntheticEvidence();
  const result = validateF1PlaytestEvidence(evidence, contract);
  assert.deepEqual(
    result.participantSummaries.map((participant) => participant.excludedMs),
    [0, 30_000, 45_000, 40_000, 30_000]
  );

  for (const reason of allowedExclusionReasons) {
    const isolated = syntheticEvidence();
    const participant = isolated.participants[0];
    const lastObjectiveId = contract.beats[1].objectiveIds.at(-1);
    const originalEnd = participant.sessionEndedMs;
    const exclusionDuration = 120_000;
    participant.exclusions.push({
      startMs: originalEnd,
      endMs: originalEnd + exclusionDuration,
      reason,
      note: `synthetic excluded ${reason}`
    });
    participant.sessionEndedMs += exclusionDuration;
    participant.trainingBeatCompletedMs += exclusionDuration;
    participant.objectiveEvents[lastObjectiveId].atMs += exclusionDuration;
    const excluded = validateF1PlaytestEvidence(isolated, contract);
    assert.equal(excluded.participantSummaries[0].qualifiedMs, 900_000, reason);
    assert.equal(excluded.participantSummaries[0].excludedMs, exclusionDuration, reason);
  }
});

test("failed or guided human samples remain valid records but fail the release gate", () => {
  const guided = syntheticEvidence();
  guided.participants[0].oralGuidance.push({
    atMs: 100_000,
    note: "observer pointed out the mooring prompt"
  });
  const guidedResult = validateF1PlaytestEvidence(guided, contract);
  assert.equal(guidedResult.contractValid, true);
  assert.equal(guidedResult.gateChecks.allNoGuidance, false);
  assert.equal(guidedResult.blindPlaytestGateMet, false);

  const incomplete = syntheticEvidence();
  const participant = incomplete.participants[0];
  const objectiveOrder = contract.beats.slice(0, 2).flatMap((beat) => beat.objectiveIds);
  const retained = objectiveOrder.slice(0, 15);
  participant.objectiveEvents = Object.fromEntries(
    retained.map((objectiveId) => [objectiveId, participant.objectiveEvents[objectiveId]])
  );
  participant.completed = false;
  participant.trainingBeatCompletedMs = null;
  participant.sessionEndedMs = participant.objectiveEvents[retained.at(-1)].atMs + 120_000;
  participant.noStuckMomentObserved = false;
  participant.stuckMoments = [{
    atMs: participant.objectiveEvents[retained.at(-1)].atMs,
    objectiveId: retained.at(-1),
    durationMs: 120_000,
    resolvedWithoutGuidance: false,
    note: "participant stopped before resolving the training step"
  }];
  const incompleteResult = validateF1PlaytestEvidence(incomplete, contract);
  assert.equal(incompleteResult.contractValid, true);
  assert.equal(incompleteResult.gateChecks.allCompleted, false);
  assert.equal(incompleteResult.completedParticipantCount, 4);
  assert.equal(incompleteResult.medianQualifiedMs, null);
  assert.equal(incompleteResult.blindPlaytestGateMet, false);
});

test("a human-class report needs an integrated build and non-synthetic recording bindings", () => {
  const evidence = syntheticEvidence();
  evidence.evidenceClass = "human_observation";
  evidence.capturedAtUtc = "2026-07-14T04:00:00.000Z";
  evidence.observerId = "qa-observer-anonymized";
  evidence.build.commit = "1".repeat(40);
  evidence.build.platform = "web-single";
  evidence.participants.forEach((participant, index) => {
    participant.recordingRef = `evidence/f1-first-two-beats/${participant.id}.webm`;
    participant.recordingSha256 = (index + 1).toString(16).repeat(64);
  });

  const result = validateF1PlaytestEvidence(evidence, contract);
  assert.equal(result.blindPlaytestGateMet, true);
  assert.equal(result.gateChecks.actualHumanEvidence, true);

  evidence.build.contentBridgeIntegrated = false;
  const unintegrated = validateF1PlaytestEvidence(evidence, contract);
  assert.equal(unintegrated.contractValid, true);
  assert.equal(unintegrated.gateChecks.integratedPlayerBuild, false);
  assert.equal(unintegrated.blindPlaytestGateMet, false);
});

test("playtest evidence fails closed on drift, overlap, unknown routes, and broken Objective prefixes", () => {
  const cases = [
    {
      name: "definition digest drift",
      mutate(evidence) {
        evidence.definitionSha256 = "f".repeat(64);
      },
      expected: /Definition digest drifted/
    },
    {
      name: "participant duplication",
      mutate(evidence) {
        evidence.participants[4].id = "P04";
      },
      expected: /participant IDs contain duplicates/
    },
    {
      name: "unknown route",
      mutate(evidence) {
        evidence.participants[0].route.arrivalSelectionId = "f1_choice_arrival_nonexistent";
      },
      expected: /is not authoritative/
    },
    {
      name: "overlapping exclusions",
      mutate(evidence) {
        evidence.participants[1].exclusions.push({
          startMs: evidence.participants[1].exclusions[0].startMs + 1,
          endMs: evidence.participants[1].exclusions[0].endMs + 1,
          reason: "idle",
          note: "invalid overlap"
        });
      },
      expected: /overlap or are unsorted/
    },
    {
      name: "event inside exclusion",
      mutate(evidence) {
        const participant = evidence.participants[1];
        const objectiveId = contract.beats[0].objectiveIds[8];
        participant.objectiveEvents[objectiveId].atMs = participant.exclusions[0].startMs + 1;
      },
      expected: /occurs inside excluded time/
    },
    {
      name: "broken Objective prefix",
      mutate(evidence) {
        delete evidence.participants[0].objectiveEvents[contract.beats[0].objectiveIds[0]];
      },
      expected: /unbroken authoritative prefix/
    },
    {
      name: "acceptance drift",
      mutate(evidence) {
        evidence.acceptance.maximumMedianQualifiedMs += 1;
      },
      expected: /maximum median drifted/
    }
  ];

  for (const fixture of cases) {
    const evidence = syntheticEvidence();
    fixture.mutate(evidence);
    assert.throws(
      () => validateF1PlaytestEvidence(evidence, contract),
      fixture.expected,
      fixture.name
    );
  }
});
