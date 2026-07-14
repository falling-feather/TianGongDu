import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

import { loadF1SliceContract } from "./generate-f1-slice-contract.mjs";

const root = resolve(import.meta.dirname, "..");
const requirementPath = resolve(
  root,
  "tests/fixtures/f1-first-two-beats-asset-requirements.json"
);
const negativeRequirementPath = resolve(
  root,
  "tests/fixtures/f1-first-two-beats-asset-requirements.negative.json"
);

export const requiredStableAssetIds = new Set([
  "asset_f1_rain_travel_writ",
  "asset_f1_rain_high_water_tags",
  "asset_f1_rain_drowned_manifest",
  "asset_f1_rain_mooring_rig",
  "asset_f1_rain_bilge_counterweight",
  "asset_f1_rain_wayfinding_lantern",
  "asset_f1_rain_workshop_bell",
  "asset_f1_training_safety_rig",
  "asset_f1_training_proof_target",
  "asset_f1_training_lane_markers"
]);

const allowedCategories = new Set(["prop", "device", "actor", "marker"]);
const allowedAuthorityOwners = new Set(["quest_definition", "gameplay_combat"]);
const allowedCollisionOwners = new Set(["content_definition", "gameplay_combat"]);
const forbiddenResourceFields = new Set([
  "assetPath",
  "packagePath",
  "resourcePath",
  "runtimePath",
  "sourcePath"
]);

function fail(message) {
  throw new Error(`F1 asset requirement: ${message}`);
}

function assert(condition, message) {
  if (!condition) fail(message);
}

function assertString(value, message) {
  assert(typeof value === "string" && value.length > 0, message);
}

function assertUniqueStrings(values, label, minimum = 0) {
  assert(Array.isArray(values) && values.length >= minimum, `${label} is incomplete`);
  values.forEach((value) => assertString(value, `${label} contains an invalid token`));
  assert(new Set(values).size === values.length, `${label} contains duplicate tokens`);
}

function collectForbiddenFields(value, path = "requirement", found = []) {
  if (Array.isArray(value)) {
    value.forEach((entry, index) => collectForbiddenFields(entry, `${path}[${index}]`, found));
    return found;
  }
  if (!value || typeof value !== "object") return found;
  for (const [key, child] of Object.entries(value)) {
    if (forbiddenResourceFields.has(key)) found.push(`${path}.${key}`);
    collectForbiddenFields(child, `${path}.${key}`, found);
  }
  return found;
}

function buildIndexes(contract) {
  return {
    interactionById: new Map(
      contract.questInteractions.map((interaction) => [interaction.id, interaction])
    ),
    actorByKey: new Map(contract.combatBootstrap.actors.map((actor) => [actor.actorKey, actor]))
  };
}

function validateInteractionBinding(binding, indexes) {
  const interaction = indexes.interactionById.get(binding.interactionId);
  assert(interaction, `unknown interaction ${binding.interactionId}`);
  assertString(binding.objectiveId, "interaction binding objectiveId is missing");
  assert(
    interaction.objectiveId === binding.objectiveId,
    `interaction ${binding.interactionId} does not belong to objective ${binding.objectiveId}`
  );
}

function validateActorBinding(binding, requirement, indexes) {
  assert(Number.isInteger(binding.actorKey) && binding.actorKey > 0, "actor binding key is invalid");
  const actor = indexes.actorByKey.get(binding.actorKey);
  assert(actor, `unknown actor ${binding.actorKey}`);
  assertString(binding.archetypeId, "actor binding archetypeId is missing");
  assert(
    actor.archetypeId === binding.archetypeId,
    `actor ${binding.actorKey} archetype ${binding.archetypeId} does not match ${actor.archetypeId}`
  );
  assertString(binding.formVariant, "actor binding formVariant is missing");
  assert(
    requirement.formVariants.includes(binding.formVariant),
    `actor binding formVariant ${binding.formVariant} is not declared`
  );
}

export function validateF1AssetRequirement(
  requirement,
  contract,
  indexes = buildIndexes(contract)
) {
  assert(requirement && typeof requirement === "object", "requirement must be an object");
  assert(
    /^asset_f1_[a-z0-9_]+$/.test(requirement.stableAssetId ?? ""),
    `stable asset ID ${requirement.stableAssetId} is invalid`
  );
  assert(
    requiredStableAssetIds.has(requirement.stableAssetId),
    `unexpected stable asset ID ${requirement.stableAssetId}`
  );
  assertString(requirement.displayName, "displayName is missing");
  assert(allowedCategories.has(requirement.category), `invalid category ${requirement.category}`);
  assert(requirement.presentationOnly === true, "asset requirement must remain presentation-only");
  assert(
    allowedAuthorityOwners.has(requirement.authorityOwner),
    `invalid authority owner ${requirement.authorityOwner}`
  );
  assert(
    allowedCollisionOwners.has(requirement.collisionOwner),
    `invalid collision owner ${requirement.collisionOwner}`
  );
  const forbidden = collectForbiddenFields(requirement);
  assert(forbidden.length === 0, `content request cannot assign resource paths (${forbidden[0]})`);

  assertUniqueStrings(requirement.footpoints, "footpoints");
  assertUniqueStrings(requirement.anchors, "anchors");
  assert(
    requirement.footpoints.length + requirement.anchors.length > 0,
    "at least one footpoint or anchor is required"
  );
  assertUniqueStrings(requirement.states, "states", 2);
  assertUniqueStrings(requirement.formVariants, "formVariants");

  assert(Array.isArray(requirement.bindings) && requirement.bindings.length > 0, "bindings are missing");
  const bindingKeys = new Set();
  for (const binding of requirement.bindings) {
    assert(binding && typeof binding === "object", "binding must be an object");
    if (binding.kind === "interaction") {
      assert(requirement.category !== "actor", "actor assets cannot bind scene interactions");
      validateInteractionBinding(binding, indexes);
      const key = `interaction:${binding.interactionId}`;
      assert(!bindingKeys.has(key), `duplicate binding ${key}`);
      bindingKeys.add(key);
    } else if (binding.kind === "actor") {
      assert(requirement.category === "actor", "non-actor assets cannot bind combat actors");
      validateActorBinding(binding, requirement, indexes);
      const key = `actor:${binding.actorKey}`;
      assert(!bindingKeys.has(key), `duplicate binding ${key}`);
      bindingKeys.add(key);
    } else {
      fail(`invalid binding kind ${binding.kind}`);
    }
  }
  if (requirement.category === "actor") {
    assert(requirement.formVariants.length > 0, "actor asset formVariants are missing");
  }

  const fallback = requirement.greyboxFallback;
  assert(fallback && typeof fallback === "object", "greyboxFallback is missing");
  assert(fallback.distinctPerFunction === true, "greybox fallback must stay distinct per function");
  assert(fallback.stateLabelsRequired === true, "greybox fallback requires state labels");
  assertString(fallback.label, "greybox fallback label is missing");

  const accessibility = requirement.accessibility;
  assert(accessibility?.shapeDistinct === true, "shape distinction is required");
  assert(accessibility?.textOrSymbolFallback === true, "text or symbol fallback is required");
  assert(accessibility?.audioOnly === false, "asset feedback cannot be audio-only");
  return requirement;
}

export function validateF1AssetRequirements(document, contract) {
  assert(document?.contractVersion === "1.0.0", "contract version drifted");
  assert(document.source?.taskId === "F1-GAME-01-CONTENT-A", "task ID drifted");
  assert(document.source?.sliceId === contract.id, "slice ID drifted");
  assert(document.source?.authoritative === false, "asset requests must remain non-authoritative");
  assert(
    document.source?.authoritativeDefinition === "content/design/f1-vertical-slice.json",
    "authoritative Definition path drifted"
  );
  assert(Array.isArray(document.requirements), "requirements are missing");
  assert(
    document.requirements.length === requiredStableAssetIds.size,
    `expected ${requiredStableAssetIds.size} stable asset requirements`
  );

  const indexes = buildIndexes(contract);
  const observed = new Set();
  for (const requirement of document.requirements) {
    validateF1AssetRequirement(requirement, contract, indexes);
    assert(!observed.has(requirement.stableAssetId), `duplicate stable asset ID ${requirement.stableAssetId}`);
    observed.add(requirement.stableAssetId);
  }
  for (const stableAssetId of requiredStableAssetIds) {
    assert(observed.has(stableAssetId), `missing stable asset ID ${stableAssetId}`);
  }
  return document;
}

function pathSegment(segment) {
  return /^\d+$/.test(segment) ? Number(segment) : segment;
}

export function applyAssetRequirementMutation(document, mutation) {
  const base = document.requirements.find(
    (requirement) => requirement.stableAssetId === mutation.baseStableAssetId
  );
  assert(base, `negative fixture base ${mutation.baseStableAssetId} is missing`);
  const requirement = structuredClone(base);
  const segments = mutation.path.split(".").map(pathSegment);
  assert(segments.length > 0, "negative fixture mutation path is empty");
  let target = requirement;
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
  return requirement;
}

export async function loadF1AssetRequirements() {
  return JSON.parse(await readFile(requirementPath, "utf8"));
}

export async function loadF1AssetRequirementNegativeFixtures() {
  return JSON.parse(await readFile(negativeRequirementPath, "utf8"));
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(import.meta.filename)) {
  const [contract, requirements, negativeFixtures] = await Promise.all([
    loadF1SliceContract(),
    loadF1AssetRequirements(),
    loadF1AssetRequirementNegativeFixtures()
  ]);
  validateF1AssetRequirements(requirements, contract);
  for (const negative of negativeFixtures.cases) {
    const requirement = applyAssetRequirementMutation(requirements, negative);
    try {
      validateF1AssetRequirement(requirement, contract);
      fail(`negative fixture ${negative.id} unexpectedly passed`);
    } catch (error) {
      if (!new RegExp(negative.expectedError).test(error.message)) throw error;
    }
  }
  console.log(
    `F1 asset requirements valid: ${requirements.requirements.length} assets, ${negativeFixtures.cases.length} negative cases`
  );
}
