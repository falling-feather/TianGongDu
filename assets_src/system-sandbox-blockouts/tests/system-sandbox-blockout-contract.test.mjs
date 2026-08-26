import assert from "node:assert/strict";
import test from "node:test";

import {
  ASSETS,
  cloneAssetSpecs
} from "../tools/system-sandbox-blockout-spec.mjs";
import { buildGeneratedOutputs } from "../tools/generate-system-sandbox-blockouts.mjs";
import {
  validateContractEntries,
  validateMetadataDocuments
} from "../tools/validate-system-sandbox-blockouts.mjs";

function throwsCode(code) {
  return (error) => error?.code === code;
}

function generatedMetadata() {
  return [...buildGeneratedOutputs()]
    .filter(([path]) => path.endsWith("/asset-metadata.json"))
    .map(([, content]) => JSON.parse(content));
}

test("frozen ART-002 contract covers 12 exact IDs, 8 sources and 12 distinct fallbacks", () => {
  const result = validateContractEntries(cloneAssetSpecs());
  assert.deepEqual(result, {
    stableAssetSlots: 12,
    newSourceAssets: 8,
    independentFallbacks: 12,
    distinctShapeTokens: 12
  });
});

test("all generated metadata documents provide the six required groups", () => {
  const result = validateMetadataDocuments(generatedMetadata(), ASSETS);
  assert.deepEqual(result, { metadataDocuments: 12, metadataGroupsPerSlot: 6 });
});

test("ground_blocker is rejected for the obstacle presentation slot", () => {
  const candidate = cloneAssetSpecs();
  candidate.find((entry) => entry.stableAssetId === "asset.system_demo.obstacle.tension_gate").sandboxAssetKind = "ground_blocker";
  assert.throws(() => validateContractEntries(candidate), throwsCode("asset_kind_mismatch"));
});

test("a shared fallback or shared silhouette token is rejected as a universal placeholder", () => {
  const candidate = cloneAssetSpecs();
  candidate[1].fallbackId = candidate[0].fallbackId;
  candidate[1].shapeToken = candidate[0].shapeToken;
  assert.throws(() => validateContractEntries(candidate), throwsCode("universal_placeholder_conflict"));
});

test("Web and Windows plans are both mandatory and remain not-built/not-integrated", () => {
  const candidate = cloneAssetSpecs();
  delete candidate[0].platformPlan.web;
  assert.throws(() => validateContractEntries(candidate), throwsCode("missing_platform_variant"));
});

test("every slot must expose a numeric presentation anchor", () => {
  const candidate = cloneAssetSpecs();
  candidate[2].anchors = [];
  assert.throws(() => validateContractEntries(candidate), throwsCode("missing_asset_anchor"));
});

test("shape and multiple non-color channels are required", () => {
  const candidate = cloneAssetSpecs();
  candidate[3].nonColorChannels = ["different color"];
  assert.throws(() => validateContractEntries(candidate), throwsCode("color_only_distinction"));
});

test("F1 content IDs cannot enter the system Demo presentation package", () => {
  const candidate = cloneAssetSpecs();
  candidate[0].stableAssetId = "asset_f1_player";
  assert.throws(() => validateContractEntries(candidate), throwsCode("invalid_stable_id"));
});

test("target transport, GPU, draw and transparent coverage must stay inside Web planning limits", () => {
  const candidate = cloneAssetSpecs();
  candidate[0].budget.targetTransportKiB = 4096;
  assert.throws(() => validateContractEntries(candidate), throwsCode("web_budget_exceeded"));
});

test("license cannot be upgraded beyond review-recorded-not-release-cleared", () => {
  const documents = generatedMetadata();
  documents[0].license.status = "release-cleared";
  assert.throws(() => validateMetadataDocuments(documents, ASSETS), throwsCode("license_blocked"));
});

test("Gameplay truth and progression fields are forbidden in presentation metadata", () => {
  const documents = generatedMetadata();
  documents[0].duty = "pressure";
  assert.throws(() => validateMetadataDocuments(documents, ASSETS), throwsCode("forbidden_gameplay_field"));
});

test("runtime and preview metadata cannot claim produced or validated artifacts", () => {
  const documents = generatedMetadata();
  documents[0].runtime.artifact = "produced";
  documents[0].preview.validation = "validated";
  assert.throws(() => validateMetadataDocuments(documents, ASSETS), throwsCode("missing_asset_metadata"));
});

test("the frozen eight-source slice cannot silently shrink", () => {
  const candidate = cloneAssetSpecs();
  candidate.find((entry) => entry.stableAssetId === "asset.system_demo.enemy.flanker").sourceMode = "project-grammar-reuse";
  assert.throws(() => validateContractEntries(candidate), throwsCode("missing_asset_metadata"));
});
