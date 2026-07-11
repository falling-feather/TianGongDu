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
  const interactions = contract.questInteractions;
  if (!Array.isArray(interactions) || interactions.length < 2 || interactions.length > 64) {
    fail("quest interactions must contain 2..64 definitions");
  }
  const interactionKinds = new Set(["inspect", "operate", "talk", "choose"]);
  for (const interaction of interactions) {
    if (!interactionKinds.has(interaction.kind)) {
      fail(`${interaction.id} has an unsupported interaction kind`);
    }
    if (!contract.cellIds.includes(interaction.cellId)) {
      fail(`${interaction.id} references an unknown cell`);
    }
    if (!objectiveIds.includes(interaction.objectiveId)) {
      fail(`${interaction.id} references an unknown objective`);
    }
    if (!Number.isInteger(interaction.radiusMm) || interaction.radiusMm <= 0 ||
        interaction.radiusMm > 10000) {
      fail(`${interaction.id} has an invalid interaction radius`);
    }
    const pose = interaction.poseMm;
    if (!pose || ![pose.x, pose.y, pose.height, pose.floorLayer].every(Number.isInteger)) {
      fail(`${interaction.id} has an invalid interaction pose`);
    }
  }
  assertUnique(interactions.map((interaction) => interaction.id), "quest interaction id");
  assertUnique(
    interactions.map((interaction) => interaction.objectiveId),
    "quest interaction objective"
  );
  const firstBeatObjectives = new Set(contract.beats[0].objectiveIds);
  const firstBeatInteractionObjectives = interactions
    .filter((interaction) => firstBeatObjectives.has(interaction.objectiveId))
    .map((interaction) => interaction.objectiveId);
  if (
    firstBeatInteractionObjectives.length !== firstBeatObjectives.size ||
    firstBeatInteractionObjectives.some((objective) => !firstBeatObjectives.has(objective))
  ) {
    fail("the first playable beat must be fully covered by scene interactions");
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
  const implementedPorts = new Set([
    "IContentDefinitionProvider",
    "ICombatResolver",
    "ICombatEventSink",
    "IEncounterDirector",
    "IQuestRuntime"
  ]);
  for (const port of contract.ports) {
    const expected = implementedPorts.has(port.name) ? "bootstrap_implemented" : "reserved";
    if (port.status !== expected) fail(`${port.name} must remain ${expected}`);
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

  const combat = contract.combatBootstrap;
  if (combat?.id !== "f1_encounter_umbrella_lane_bootstrap") {
    fail("combat bootstrap id drifted");
  }
  if (!Array.isArray(combat.actors) || combat.actors.length < 2 || combat.actors.length > 16) {
    fail("combat bootstrap requires 2..16 actors");
  }
  if (!Array.isArray(combat.abilities) || combat.abilities.length === 0 || combat.abilities.length > 32) {
    fail("combat bootstrap requires 1..32 abilities");
  }
  const director = combat.director;
  if (
    director?.playerActorKey !== seed.actorKey ||
    !Number.isInteger(director.aggroRangeMm) ||
    director.aggroRangeMm <= 0 ||
    !Number.isInteger(director.leashRangeMm) ||
    director.leashRangeMm < director.aggroRangeMm ||
    !Number.isInteger(director.chaseSpeedMmPerSecond) ||
    director.chaseSpeedMmPerSecond <= 0 ||
    director.chaseSpeedMmPerSecond % 60 !== 0 ||
    !Number.isInteger(director.formationRadiusMm) ||
    director.formationRadiusMm <= 0 ||
    director.formationRadiusMm >= director.aggroRangeMm ||
    !Number.isInteger(director.decisionIntervalTicks) ||
    director.decisionIntervalTicks <= 0 ||
    !Number.isInteger(director.postAttackCooldownTicks) ||
    director.postAttackCooldownTicks < 0 ||
    !Number.isInteger(director.maxSimultaneousAttackers) ||
    director.maxSimultaneousAttackers <= 0 ||
    director.maxSimultaneousAttackers > 15
  ) {
    fail("combat encounter director definition is invalid");
  }
  assertUnique(combat.actors.map((actor) => actor.actorKey), "combat actor key");
  const stanceIds = new Set();
  const resourcePairs = [
    ["health", "healthMax", true],
    ["stamina", "staminaMax", true],
    ["poise", "poiseMax", true],
    ["lantern", "lanternMax", false]
  ];
  for (const actor of combat.actors) {
    if (!Number.isSafeInteger(actor.actorKey) || actor.actorKey <= 0) {
      fail("invalid combat actor key");
    }
    if (!["player", "hostile", "neutral"].includes(actor.faction)) {
      fail(`${actor.actorKey} has an invalid faction`);
    }
    if (!Array.isArray(actor.stanceIds) || actor.stanceIds.length === 0 || actor.stanceIds.length > 3) {
      fail(`${actor.actorKey} must declare 1..3 stances`);
    }
    assertUnique(actor.stanceIds, `${actor.actorKey} stance`);
    if (!actor.stanceIds.includes(actor.initialStanceId)) {
      fail(`${actor.actorKey} initial stance is not allowed`);
    }
    for (const stance of actor.stanceIds) stanceIds.add(stance);
    for (const [valueField, maxField, positiveMax] of resourcePairs) {
      const value = actor.resources?.[valueField];
      const maximum = actor.resources?.[maxField];
      if (
        !Number.isInteger(value) ||
        !Number.isInteger(maximum) ||
        value < 0 ||
        maximum < (positiveMax ? 1 : 0) ||
        value > maximum
      ) {
        fail(`${actor.actorKey} has invalid ${valueField}/${maxField}`);
      }
    }
    if (!Number.isInteger(actor.resources.evidence) || actor.resources.evidence < 0) {
      fail(`${actor.actorKey} has invalid evidence`);
    }
    for (const [field, maximum] of [
      ["staminaDelayTicks", 3600],
      ["staminaIntervalTicks", 600],
      ["staminaPerInterval", 100000],
      ["poiseDelayTicks", 3600],
      ["poiseIntervalTicks", 600],
      ["poisePerInterval", 100000]
    ]) {
      if (
        !Number.isInteger(actor.recovery?.[field]) ||
        actor.recovery[field] <= 0 ||
        actor.recovery[field] > maximum
      ) {
        fail(`${actor.actorKey} has invalid recovery field ${field}`);
      }
    }
  }
  const playerCombatSeed = combat.actors.find((actor) => actor.actorKey === seed.actorKey);
  if (
    playerCombatSeed?.faction !== "player" ||
    !sameValues(playerCombatSeed.poseMm, seed.initialPoseMm)
  ) {
    fail("combat player seed must match the movement player seed");
  }
  const hostileCount = combat.actors.filter((actor) => actor.faction === "hostile").length;
  if (director.maxSimultaneousAttackers > hostileCount) {
    fail("combat attack token count exceeds hostile actors");
  }

  assertUnique(combat.abilities.map((ability) => ability.id), "combat ability id");
  const triggerKeys = [];
  const coveredStanceTriggers = new Set();
  let evadeCount = 0;
  const feedbackTags = new Set(["light", "heavy", "guard", "poise_break", "evade"]);
  for (const ability of combat.abilities) {
    if (!["light_attack", "heavy_attack", "evade"].includes(ability.trigger)) {
      fail(`${ability.id} has an invalid trigger`);
    }
    if (ability.trigger === "evade") {
      if (ability.requiredStanceId !== undefined) fail(`${ability.id} evade must be stance-neutral`);
      evadeCount += 1;
    } else if (!stanceIds.has(ability.requiredStanceId)) {
      fail(`${ability.id} references an unknown stance`);
    }
    const triggerKey = `${ability.trigger}:${ability.requiredStanceId ?? "*"}`;
    triggerKeys.push(triggerKey);
    coveredStanceTriggers.add(triggerKey);
    for (const field of [
      "staminaCost",
      "windupTicks",
      "recoveryTicks",
      "rangeMm",
      "heightToleranceMm",
      "healthDamage",
      "poiseDamage"
    ]) {
      if (!Number.isInteger(ability[field]) || ability[field] < 0 || ability[field] > 100000) {
        fail(`${ability.id} has invalid ${field}`);
      }
    }
    if (!Number.isInteger(ability.activeTicks) || ability.activeTicks <= 0 || ability.activeTicks > 600) {
      fail(`${ability.id} has invalid activeTicks`);
    }
    if (!Array.isArray(ability.feedbackTags) || ability.feedbackTags.length === 0) {
      fail(`${ability.id} has no feedback tags`);
    }
    assertUnique(ability.feedbackTags, `${ability.id} feedback tag`);
    if (ability.feedbackTags.some((tag) => !feedbackTags.has(tag))) {
      fail(`${ability.id} has an unknown feedback tag`);
    }
  }
  assertUnique(triggerKeys, "combat trigger and stance");
  if (evadeCount !== 1) fail("combat bootstrap requires one shared evade ability");
  for (const stance of stanceIds) {
    for (const trigger of ["light_attack", "heavy_attack"]) {
      if (!coveredStanceTriggers.has(`${trigger}:${stance}`)) {
        fail(`${stance} is missing ${trigger}`);
      }
    }
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
    ...objectiveIds,
    ...interactions.map((interaction) => interaction.id),
    combat.id,
    ...combat.actors.map((actor) => actor.archetypeId),
    ...stanceIds,
    ...combat.abilities.map((ability) => ability.id)
  ];
  assertHashUnique([...new Set(allStableIds)], "stable content id");
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

function stableKey(value) {
  return value ? `contracts::stable_content_key("${value}")` : "0";
}

function combatActorRow(actor) {
  const pose = actor.poseMm;
  const resources = actor.resources;
  const recovery = actor.recovery;
  const stances = [...actor.stanceIds.map(stableKey), ...Array(3 - actor.stanceIds.length).fill("0")];
  return `    {${actor.actorKey}ULL, ${contentId(actor.archetypeId)}, contracts::CombatFaction::${actor.faction}, {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}}, {${resources.health}, ${resources.healthMax}, ${resources.stamina}, ${resources.staminaMax}, ${resources.poise}, ${resources.poiseMax}, ${resources.lantern}, ${resources.lanternMax}, ${resources.evidence}}, {${stances.join(", ")}}, ${actor.stanceIds.length}U, ${stableKey(actor.initialStanceId)}, {${recovery.staminaDelayTicks}, ${recovery.staminaIntervalTicks}, ${recovery.staminaPerInterval}, ${recovery.poiseDelayTicks}, ${recovery.poiseIntervalTicks}, ${recovery.poisePerInterval}}},`;
}

function combatAbilityRow(ability) {
  const feedback = ability.feedbackTags
    .map((tag) => `contracts::feedback_${tag}`)
    .join(" | ");
  return `    {${contentId(ability.id)}, contracts::CombatCommandType::${ability.trigger}, ${stableKey(ability.requiredStanceId)}, ${ability.staminaCost}, ${ability.windupTicks}, ${ability.activeTicks}, ${ability.recoveryTicks}, ${ability.rangeMm}, ${ability.heightToleranceMm}, ${ability.healthDamage}, ${ability.poiseDamage}, ${feedback}},`;
}

function questInteractionRow(interaction) {
  const pose = interaction.poseMm;
  return `    {${contentId(interaction.id)}, contracts::QuestInteractionKind::${interaction.kind}, ${contentId(interaction.cellId)}, ${contentId(interaction.objectiveId)}, {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}}, ${interaction.radiusMm}},`;
}

export function renderF1SliceContract(contract) {
  const refs = contract.catalogReferences;
  const seed = contract.playerSeed;
  const combat = contract.combatBootstrap;
  const director = combat.director;
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
  const combatActorRows = combat.actors.map(combatActorRow).join("\n");
  const combatAbilityRows = combat.abilities.map(combatAbilityRow).join("\n");
  const questInteractionRows = contract.questInteractions.map(questInteractionRow).join("\n");

  return `// Generated from content/design/f1-vertical-slice.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/combat_types.hpp>
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

inline constexpr std::array<contracts::QuestInteractionDefinition, ${contract.questInteractions.length}> f1_quest_interactions{{
${questInteractionRows}
}};

inline constexpr std::array<contracts::CombatActorConfig, ${combat.actors.length}> f1_combat_actors{{
${combatActorRows}
}};

inline constexpr std::array<contracts::AbilityDefinition, ${combat.abilities.length}> f1_combat_abilities{{
${combatAbilityRows}
}};

inline constexpr contracts::CombatEncounterDefinition f1_combat_encounter_definition{
    ${contentId(combat.id)},
    std::span<const contracts::CombatActorConfig>{f1_combat_actors},
    std::span<const contracts::AbilityDefinition>{f1_combat_abilities},
    {${director.playerActorKey}ULL, ${director.aggroRangeMm}, ${director.leashRangeMm}, ${director.chaseSpeedMmPerSecond}, ${director.formationRadiusMm}, ${director.decisionIntervalTicks}, ${director.postAttackCooldownTicks}, ${director.maxSimultaneousAttackers}U},
};

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
    std::span<const contracts::QuestInteractionDefinition>{f1_quest_interactions},
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
