import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const root = resolve(import.meta.dirname, "..");
const inputPath = resolve(root, "content/design/f1-vertical-slice.json");
const catalogPath = resolve(root, "content/design/v1-content-catalog.json");
const outputPath = resolve(
  root,
  "src/content-core/include/tgd/content/f1_vertical_slice.generated.hpp"
);

function fail(message) {
  throw new Error(`F1 vertical slice contract: ${message}`);
}

export function fnv1a64(value) {
  let hash = 0xcbf29ce484222325n;
  for (const byte of Buffer.from(value, "utf8")) {
    hash ^= BigInt(byte);
    hash = BigInt.asUintN(64, hash * 0x100000001b3n);
  }
  return hash;
}

function assertUnique(values, label) {
  const seen = new Set();
  for (const value of values) {
    if (seen.has(value)) fail(`duplicate ${label}: ${value}`);
    seen.add(value);
  }
}

function assertHashUnique(values, label) {
  const seen = new Map();
  for (const value of values) {
    const hash = fnv1a64(value).toString(16);
    if (seen.has(hash)) fail(`${label} hash collision: ${seen.get(hash)} and ${value}`);
    seen.set(hash, value);
  }
}

function sameValues(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

export function validateF1SliceContract(contract, catalog) {
  if (contract.schemaVersion !== "1.0.0") fail("unsupported schemaVersion");
  const catalogSlice = catalog.f1VerticalSlice;
  if (contract.id !== catalogSlice.id) fail("slice id drifted from the 1.0 catalog");
  const expectedView = {
    model: "2.5d-oblique-panoramic",
    primaryGuidance: "douzhanshen",
    secondaryReference: "warm-snow-combat-readability",
    cameraMode: "author-controlled-oblique"
  };
  if (!sameValues(contract.view, expectedView)) fail("view guidance drifted");
  const timing = contract.timing;
  if (
    timing?.playableTargetMinutes !== 60 ||
    timing?.endToEndTestBudgetMinutes !== 70 ||
    timing?.startupMinutes !== 3 ||
    timing?.postResolutionPersistenceMinutes !== 7
  ) {
    fail("60-minute playable and 70-minute E2E budgets are frozen");
  }
  const expectedExclusions = [
    "title_loading",
    "failure_retry",
    "idle",
    "post_resolution_persistence"
  ];
  if (!sameValues(timing.excludedFromPlayable, expectedExclusions)) {
    fail("playable-time exclusions drifted");
  }

  const refs = contract.catalogReferences;
  for (const key of ["startFixtureId", "chapterId", "bossId"]) {
    const catalogKey = key === "startFixtureId" ? "startFixtureId" : key;
    if (refs?.[key] !== catalogSlice[catalogKey]) fail(`${key} drifted from the catalog`);
  }
  for (const key of ["subregionIds", "npcIds", "enemyFamilyIds"]) {
    if (!sameValues(refs?.[key], catalogSlice[key])) fail(`${key} drifted from the catalog`);
  }

  if (!Array.isArray(contract.cellIds) || contract.cellIds.length < 5) {
    fail("at least five route cells are required");
  }
  assertUnique(contract.cellIds, "cell id");
  if (!Array.isArray(contract.beats) || contract.beats.length !== 7) {
    fail("the initial vertical slice must contain exactly seven playable beats");
  }
  const allowedKinds = new Set([
    "exploration",
    "training",
    "combat",
    "investigation",
    "boss",
    "resolution"
  ]);
  const beatIds = [];
  const objectiveIds = [];
  let playableMinutes = 0;
  for (const beat of contract.beats) {
    if (!allowedKinds.has(beat.kind)) fail(`unsupported beat kind: ${beat.kind}`);
    if (!Number.isInteger(beat.targetMinutes) || beat.targetMinutes <= 0) {
      fail(`${beat.id} has an invalid targetMinutes`);
    }
    if (!contract.cellIds.includes(beat.cellId)) fail(`${beat.id} references an unknown cell`);
    if (!Array.isArray(beat.objectiveIds) || beat.objectiveIds.length === 0) {
      fail(`${beat.id} has no objective path`);
    }
    if (beat.autoAdvance !== false) fail(`${beat.id} must never advance from a timer`);
    assertUnique(beat.objectiveIds, `${beat.id} objective`);
    beatIds.push(beat.id);
    objectiveIds.push(...beat.objectiveIds);
    playableMinutes += beat.targetMinutes;
  }
  assertUnique(beatIds, "beat id");
  assertUnique(objectiveIds, "objective id");
  if (playableMinutes !== timing.playableTargetMinutes) {
    fail(`beat budgets total ${playableMinutes}, expected ${timing.playableTargetMinutes}`);
  }
  if (
    contract.paddingPolicy?.repeatableCombatCountsTowardTarget !== false ||
    contract.paddingPolicy?.forcedWaitingCountsTowardTarget !== false ||
    contract.paddingPolicy?.failureRetryCountsTowardTarget !== false
  ) {
    fail("padding policy must reject combat repetition, waiting, and failure retries");
  }

  const requiredPorts = [
    "IContentDefinitionProvider",
    "IWorldCellSource",
    "ICombatResolver",
    "ICombatEventSink",
    "IEncounterDirector",
    "IQuestRuntime",
    "ISnapshotContributor",
    "IPresentationEventSink",
    "IAssetResolver"
  ];
  if (!sameValues(contract.ports?.map((port) => port.name), requiredPorts)) {
    fail("reserved interface list drifted");
  }
  if (contract.ports[0].status !== "bootstrap_implemented") {
    fail("the built-in definition provider must be marked as bootstrap implemented");
  }
  if (contract.ports.slice(1).some((port) => port.status !== "reserved")) {
    fail("unimplemented ports must remain explicitly reserved");
  }

  const seed = contract.playerSeed;
  if (!Number.isSafeInteger(seed?.actorKey) || seed.actorKey <= 0) fail("invalid player actor key");
  for (const field of [
    "moveSpeedMmPerSecond",
    "jumpSpeedMmPerSecond",
    "gravityMmPerSecondSquared",
    "collisionRadiusMm",
    "collisionHeightMm"
  ]) {
    if (!Number.isInteger(seed[field]) || seed[field] <= 0 || seed[field] > 100000) {
      fail(`invalid playerSeed.${field}`);
    }
  }
  const basis = seed.cameraBasisQ15;
  const right = basis?.screenRightWorld;
  const forward = basis?.screenForwardWorld;
  const target = 32767 * 32767;
  const tolerance = Math.floor(target / 100);
  const rightLength = right.x * right.x + right.y * right.y;
  const forwardLength = forward.x * forward.x + forward.y * forward.y;
  const dot = right.x * forward.x + right.y * forward.y;
  const determinant = right.x * forward.y - right.y * forward.x;
  if (
    basis.revision <= 1 ||
    Math.abs(rightLength - target) > tolerance ||
    Math.abs(forwardLength - target) > tolerance ||
    Math.abs(dot) > tolerance ||
    Math.abs(determinant - target) > tolerance
  ) {
    fail("camera basis must be a right-handed unit oblique basis with a new revision");
  }

  const allStableIds = [
    contract.id,
    refs.startFixtureId,
    refs.chapterId,
    ...refs.subregionIds,
    ...refs.npcIds,
    ...refs.enemyFamilyIds,
    refs.bossId,
    ...contract.cellIds,
    ...beatIds,
    ...objectiveIds
  ];
  assertHashUnique(allStableIds, "stable content id");
  return contract;
}

function contentId(value) {
  return `contracts::content_id("${value}")`;
}

function arrayRows(values) {
  return values.map((value) => `    ${contentId(value)},`).join("\n");
}

function beatKind(value) {
  return `contracts::VerticalSliceBeatKind::${value}`;
}

export function renderF1SliceContract(contract) {
  const refs = contract.catalogReferences;
  const seed = contract.playerSeed;
  const objectiveArrays = contract.beats
    .map(
      (beat, index) => `inline constexpr std::array<contracts::ContentId, ${beat.objectiveIds.length}> beat_${index}_objectives{{
${arrayRows(beat.objectiveIds)}
}};`
    )
    .join("\n\n");
  const beatRows = contract.beats
    .map(
      (beat, index) =>
        `    {${contentId(beat.id)}, ${beatKind(beat.kind)}, ${beat.targetMinutes}, ${contentId(beat.cellId)}, std::span<const contracts::ContentId>{beat_${index}_objectives}},`
    )
    .join("\n");
  const pose = seed.initialPoseMm;
  const basis = seed.cameraBasisQ15;

  return `// Generated from content/design/f1-vertical-slice.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <array>
#include <span>

namespace tgd::content::generated {

${objectiveArrays}

inline constexpr std::array<contracts::VerticalSliceBeatDefinition, ${contract.beats.length}> f1_beats{{
${beatRows}
}};

inline constexpr std::array<contracts::ContentId, ${refs.subregionIds.length}> f1_subregions{{
${arrayRows(refs.subregionIds)}
}};

inline constexpr std::array<contracts::ContentId, ${refs.npcIds.length}> f1_npcs{{
${arrayRows(refs.npcIds)}
}};

inline constexpr std::array<contracts::ContentId, ${refs.enemyFamilyIds.length}> f1_enemy_families{{
${arrayRows(refs.enemyFamilyIds)}
}};

inline constexpr std::array<contracts::ContentId, ${contract.cellIds.length}> f1_cells{{
${arrayRows(contract.cellIds)}
}};

inline constexpr contracts::VerticalSliceDefinition f1_vertical_slice_definition{
    ${contentId(contract.id)},
    "${contract.view.model}",
    "${contract.view.primaryGuidance}",
    "${contract.view.secondaryReference}",
    "${contract.view.cameraMode}",
    ${contract.timing.playableTargetMinutes},
    ${contract.timing.endToEndTestBudgetMinutes},
    ${contentId(refs.startFixtureId)},
    ${contentId(refs.chapterId)},
    ${contentId(refs.bossId)},
    {
        ${seed.actorKey}ULL,
        {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}},
        ${seed.moveSpeedMmPerSecond},
        ${seed.jumpSpeedMmPerSecond},
        ${seed.gravityMmPerSecondSquared},
        ${seed.collisionRadiusMm},
        ${seed.collisionHeightMm},
        {
            {${basis.screenRightWorld.x}, ${basis.screenRightWorld.y}},
            {${basis.screenForwardWorld.x}, ${basis.screenForwardWorld.y}},
            ${basis.revision}U,
        },
    },
    std::span<const contracts::ContentId>{f1_subregions},
    std::span<const contracts::ContentId>{f1_npcs},
    std::span<const contracts::ContentId>{f1_enemy_families},
    std::span<const contracts::ContentId>{f1_cells},
    std::span<const contracts::VerticalSliceBeatDefinition>{f1_beats},
};

}  // namespace tgd::content::generated
`;
}

export async function loadF1SliceContract() {
  const [contract, catalog] = await Promise.all([
    readFile(inputPath, "utf8").then(JSON.parse),
    readFile(catalogPath, "utf8").then(JSON.parse)
  ]);
  return validateF1SliceContract(contract, catalog);
}

async function main() {
  const contract = await loadF1SliceContract();
  const expected = renderF1SliceContract(contract);
  if (process.argv.includes("--check")) {
    const actual = await readFile(outputPath, "utf8").catch(() => "");
    if (actual !== expected) {
      fail("generated C++ contract is stale; run npm run generate:f1-slice-contract");
    }
    console.log("Generated F1 vertical slice contract is current.");
    return;
  }
  await mkdir(dirname(outputPath), { recursive: true });
  await writeFile(outputPath, expected, "utf8");
  console.log(`Generated ${outputPath.slice(root.length + 1).replaceAll("\\", "/")}`);
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  await main();
}
