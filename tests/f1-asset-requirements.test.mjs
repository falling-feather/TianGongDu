import assert from "node:assert/strict";
import test from "node:test";

import { loadF1SliceContract } from "../tools/generate-f1-slice-contract.mjs";
import {
  applyAssetRequirementMutation,
  loadF1AssetRequirementNegativeFixtures,
  loadF1AssetRequirements,
  requiredStableAssetIds,
  validateF1AssetRequirement,
  validateF1AssetRequirements
} from "../tools/validate-f1-asset-requirements.mjs";

const [contract, requirements, negativeFixtures] = await Promise.all([
  loadF1SliceContract(),
  loadF1AssetRequirements(),
  loadF1AssetRequirementNegativeFixtures()
]);

function requirement(stableAssetId) {
  const found = requirements.requirements.find((entry) => entry.stableAssetId === stableAssetId);
  assert(found, `missing asset requirement ${stableAssetId}`);
  return found;
}

test("F1 first-two-beat asset requirements stay bound to the authoritative Definition", () => {
  assert.equal(validateF1AssetRequirements(requirements, contract), requirements);
  assert.equal(requirements.requirements.length, 10);
  assert.deepEqual(
    new Set(requirements.requirements.map((entry) => entry.stableAssetId)),
    requiredStableAssetIds
  );
  assert.equal(requirements.source.authoritative, false);
  assert.equal(
    requirements.source.authoritativeDefinition,
    "content/design/f1-vertical-slice.json"
  );
  for (const entry of requirements.requirements) {
    assert.equal(entry.presentationOnly, true);
    assert.equal(entry.greyboxFallback.distinctPerFunction, true);
    assert.equal(entry.greyboxFallback.stateLabelsRequired, true);
    assert.equal(entry.accessibility.shapeDistinct, true);
    assert.equal(entry.accessibility.textOrSymbolFallback, true);
    assert.equal(entry.accessibility.audioOnly, false);
    assert.equal(Object.hasOwn(entry, "resourcePath"), false);
    assert.equal(Object.hasOwn(entry, "assetPath"), false);
  }
});

test("F1 rain-ferry devices expose distinct anchors, states, and interaction bindings", () => {
  const mooring = requirement("asset_f1_rain_mooring_rig");
  assert.deepEqual(mooring.footpoints, ["foot_deck"]);
  assert.deepEqual(mooring.anchors, ["anchor_low", "anchor_high", "anchor_rope_load"]);
  assert.deepEqual(mooring.states, [
    "unsecured",
    "cross-belay-stable",
    "quick-hitch-overloaded",
    "quick-hitch-corrected"
  ]);
  assert.deepEqual(
    mooring.bindings.map((binding) => binding.interactionId),
    [
      "f1_interaction_choose_cross_belay",
      "f1_interaction_choose_quick_hitch",
      "f1_interaction_lock_cross_belay",
      "f1_interaction_correct_overloaded_quick_hitch"
    ]
  );

  const bell = requirement("asset_f1_rain_workshop_bell");
  assert.deepEqual(bell.states, ["idle", "wrong-pattern", "answered"]);
  assert.deepEqual(
    bell.bindings.map((binding) => binding.objectiveId),
    ["f1_objective_read_workshop_bell_code", "f1_objective_sound_workshop_bell"]
  );
});

test("F1 training assets distinguish ground/high forms and selected spatial lines", () => {
  const rig = requirement("asset_f1_training_safety_rig");
  assert.deepEqual(
    rig.bindings.map((binding) => ({
      actorKey: binding.actorKey,
      archetypeId: binding.archetypeId,
      formVariant: binding.formVariant
    })),
    [
      {
        actorKey: 104,
        archetypeId: "f1_training_umbrella_rig",
        formVariant: "eavesguard-ground"
      },
      {
        actorKey: 108,
        archetypeId: "f1_training_flower_turn_rig",
        formVariant: "flower-turn-high"
      }
    ]
  );

  const target = requirement("asset_f1_training_proof_target");
  assert.deepEqual(target.bindings.map((binding) => binding.actorKey), [107, 109]);
  assert.notEqual(target.bindings[0].formVariant, target.bindings[1].formVariant);

  const lines = requirement("asset_f1_training_lane_markers");
  assert.deepEqual(lines.anchors, [
    "anchor_windward",
    "anchor_leeward",
    "anchor_flower-cross"
  ]);
  assert.deepEqual(
    lines.bindings.map((binding) => binding.interactionId),
    [
      "f1_interaction_choose_training_windward_lane",
      "f1_interaction_choose_training_leeward_lane",
      "f1_interaction_take_windward_eavesguard_mark",
      "f1_interaction_take_leeward_eavesguard_mark",
      "f1_interaction_cross_flower_turn_line"
    ]
  );
});

test("F1 asset requirement negative fixtures fail closed", () => {
  assert.equal(negativeFixtures.contractVersion, "1.0.0");
  assert.equal(negativeFixtures.cases.length, 10);
  for (const negative of negativeFixtures.cases) {
    const mutated = applyAssetRequirementMutation(requirements, negative);
    assert.throws(
      () => validateF1AssetRequirement(mutated, contract),
      new RegExp(negative.expectedError),
      negative.id
    );
  }

  const duplicate = structuredClone(requirements);
  duplicate.requirements[9] = structuredClone(duplicate.requirements[8]);
  assert.throws(
    () => validateF1AssetRequirements(duplicate, contract),
    /duplicate stable asset ID/
  );
});
