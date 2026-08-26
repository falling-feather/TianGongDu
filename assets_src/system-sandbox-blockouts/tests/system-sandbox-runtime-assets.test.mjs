import assert from "node:assert/strict";
import { before, test } from "node:test";

import { ASSETS } from "../tools/system-sandbox-blockout-spec.mjs";
import {
  runtimeSvgFromFallback,
  stableContentKeyHex
} from "../tools/generate-system-sandbox-runtime-assets.mjs";
import {
  RuntimeImportValidationError,
  loadRuntimeImportPackage,
  resolveRuntimeArtifact,
  validateRuntimeImportPackage
} from "../tools/validate-system-sandbox-runtime-assets.mjs";

let baseline;

before(async () => {
  baseline = await loadRuntimeImportPackage();
});

function clonePackage() {
  return {
    manifest: structuredClone(baseline.manifest),
    artifacts: new Map([...baseline.artifacts].map(([path, bytes]) => [path, Buffer.from(bytes)])),
    sources: new Map(baseline.sources),
    manifestText: undefined
  };
}

function assertFailure(data, code) {
  assert.throws(
    () => validateRuntimeImportPackage(data),
    (error) => error instanceof RuntimeImportValidationError && (!code || error.code === code),
    `expected RuntimeImportValidationError${code ? `(${code})` : ""}`
  );
}

function negative(name, mutate, code) {
  test(`negative: ${name}`, () => {
    const data = clonePackage();
    mutate(data);
    assertFailure(data, code);
  });
}

test("positive: validates the dense 12-slot/24-artifact import package", () => {
  const result = validateRuntimeImportPackage(baseline);
  assert.deepEqual(result, {
    stableAssetSlots: 12,
    runtimeArtifacts: 24,
    standardTransferBytes: 111179,
    lowTransferBytes: 53361,
    standardDecodedBytes: 3145728,
    lowDecodedBytes: 786432,
    manifestBytes: 62656,
    actorSilhouettesDistinct: 4,
    maturity: "Blockout/not-integrated",
    previewReady: false
  });
  assert.equal(baseline.manifest.runtimeBuildStatus, "not-produced");
  assert.ok(baseline.manifest.entries.every((entry) => entry.runtimeBuildStatus === "not-produced"));
});

test("positive: resolves only exact Stable ID + content key + kind pairs", () => {
  for (const expected of ASSETS) {
    const resolved = resolveRuntimeArtifact(baseline, {
      stableAssetId: expected.stableAssetId,
      stableContentKeyHex: stableContentKeyHex(expected.stableAssetId),
      sandboxAssetKind: expected.sandboxAssetKind,
      quality: "low"
    });
    assert.equal(resolved.artifact.path, `low/${expected.slug}.png`);
    assert.equal(resolved.bytes.length, resolved.artifact.fileBytes);
  }
});

test("positive: runtime SVG extraction keeps only the dedicated shape group", () => {
  for (const expected of ASSETS) {
    const source = baseline.sources.get(`fallbacks/${expected.slug}.svg`);
    const runtimeSvg = runtimeSvgFromFallback(source, expected.stableAssetId, 128);
    assert.match(runtimeSvg, new RegExp(`data-runtime-stable-asset-id="${expected.stableAssetId.replaceAll(".", "\\.")}"`));
    assert.doesNotMatch(runtimeSvg, /<text\b|data-anchor-mark=|data-preview|data-frame/i);
    assert.equal((runtimeSvg.match(/data-stable-asset-id=/g) || []).length, 1);
  }
});

negative("partial table omits one Stable Asset ID", ({ manifest }) => {
  manifest.entries.pop();
}, "dense_table");

negative("obstacle cannot masquerade as ground_blocker", ({ manifest }) => {
  manifest.entries[4].sandboxAssetKind = "ground_blocker";
}, "asset_kind_mismatch");

negative("Stable Asset ID/content key mismatch", ({ manifest }) => {
  manifest.entries[0].stableContentKeyHex = "0000000000000000";
}, "identity_mismatch");

negative("missing generated artifact fails closed", ({ manifest, artifacts }) => {
  artifacts.delete(manifest.entries[0].artifacts.standard.path);
}, "artifact_missing");

negative("unexpected generated artifact prevents a partial/expanded table", ({ artifacts }) => {
  artifacts.set("low/unlisted.png", Buffer.from([0]));
}, "dense_table");

negative("artifact byte mutation invalidates SHA-256", ({ manifest, artifacts }) => {
  const path = manifest.entries[0].artifacts.standard.path;
  const bytes = Buffer.from(artifacts.get(path));
  bytes[bytes.length - 1] ^= 1;
  artifacts.set(path, bytes);
}, "artifact_hash_mismatch");

negative("declared PNG dimensions are frozen", ({ manifest }) => {
  manifest.entries[0].artifacts.low.width = 127;
}, "artifact_dimensions");

negative("alpha evidence cannot be made opaque by declaration", ({ manifest }) => {
  manifest.entries[0].artifacts.low.transparentPixels = 0;
}, "alpha_invalid");

negative("RGBA8/sRGB import format cannot drift", ({ manifest }) => {
  manifest.entries[0].artifacts.standard.colorSpace = "Display-P3";
}, "artifact_format");

negative("two slots cannot share one fallback artifact path", ({ manifest }) => {
  manifest.entries[1].artifacts.low.path = manifest.entries[0].artifacts.low.path;
}, "universal_fallback");

negative("two slots cannot share one artifact identity", ({ manifest }) => {
  manifest.entries[1].artifacts.low.artifactId = manifest.entries[0].artifacts.low.artifactId;
}, "universal_fallback");

negative("release-cleared license upgrade is forbidden", ({ manifest }) => {
  manifest.license = "release-cleared";
}, "maturity_overclaim");

negative("per-item Web transfer budget is bounded", ({ manifest }) => {
  manifest.entries[0].artifacts.standard.fileBytes = 48 * 1024 + 1;
}, "budget_exceeded");

negative("measured totals cannot under-report capacity", ({ manifest }) => {
  manifest.measured.standardTransferBytes -= 1;
}, "budget_exceeded");

negative("root manifest rejects unknown fields", ({ manifest }) => {
  manifest.unexpected = true;
}, "closed_shape");

negative("entry manifest rejects a missing required field", ({ manifest }) => {
  delete manifest.entries[0].accessibility.nonColorChannels;
}, "closed_shape");

negative("Windows/Web mapping cannot point to a neighbor", ({ manifest }) => {
  manifest.entries[0].platformQuality.web.low = manifest.entries[1].artifacts.low.artifactId;
}, "lookup_failed");

negative("color-only accessibility claim is rejected", ({ manifest }) => {
  manifest.entries[0].accessibility.colorOnly = true;
}, "color_only_distinction");

negative("fallback SVG group SHA cannot drift", ({ manifest }) => {
  manifest.entries[0].source.groupSha256 = "0".repeat(64);
}, "source_drift");

negative("missing fallback SVG source fails closed", ({ manifest, sources }) => {
  sources.delete(manifest.entries[0].source.path);
}, "source_drift");

negative("Preview-ready overclaim is rejected", ({ manifest }) => {
  manifest.entries[0].previewReady = true;
}, "maturity_overclaim");

negative("platform Preview pass overclaim is rejected", ({ manifest }) => {
  manifest.platformPreviewStatus.web = "passed";
}, "maturity_overclaim");

negative("path traversal is rejected before file access", ({ manifest }) => {
  manifest.entries[0].artifacts.low.path = "low/../player.png";
}, "unsafe_path");

negative("F1 content IDs cannot enter the system Demo package", ({ manifest }) => {
  manifest.entries[0].dependencies[0] = "asset.f1.borrowed";
}, "invalid_stable_id");

negative("Gameplay-authoritative fields are forbidden", ({ manifest }) => {
  manifest.entries[0].damage = 10;
}, "forbidden_gameplay_field");

negative("maximum UTF-8 string capacity is bounded", ({ manifest }) => {
  manifest.entries[0].reuseLineage = "x".repeat(257);
}, "capacity_exceeded");

negative("generator browser executable identity is locked", ({ manifest }) => {
  manifest.generatorIdentity.browserExecutableSha256 = "0".repeat(64);
}, "generator_identity");

test("negative: unknown Stable Asset ID lookup exposes no partial table", () => {
  assert.throws(
    () => resolveRuntimeArtifact(baseline, {
      stableAssetId: "asset.system_demo.unknown",
      stableContentKeyHex: stableContentKeyHex("asset.system_demo.unknown"),
      sandboxAssetKind: "actor",
      quality: "low"
    }),
    (error) => error instanceof RuntimeImportValidationError && error.code === "lookup_failed"
  );
});

test("negative: lookup name/key mismatch fails closed", () => {
  assert.throws(
    () => resolveRuntimeArtifact(baseline, {
      stableAssetId: ASSETS[0].stableAssetId,
      stableContentKeyHex: stableContentKeyHex(ASSETS[1].stableAssetId),
      sandboxAssetKind: ASSETS[0].sandboxAssetKind,
      quality: "standard"
    }),
    (error) => error instanceof RuntimeImportValidationError && error.code === "lookup_failed"
  );
});

test("negative: lookup wrong SandboxAssetKind fails closed", () => {
  assert.throws(
    () => resolveRuntimeArtifact(baseline, {
      stableAssetId: ASSETS[4].stableAssetId,
      stableContentKeyHex: stableContentKeyHex(ASSETS[4].stableAssetId),
      sandboxAssetKind: "ground_blocker",
      quality: "standard"
    }),
    (error) => error instanceof RuntimeImportValidationError && error.code === "asset_kind_mismatch"
  );
});

test("negative: lookup refuses an unknown quality instead of guessing a path", () => {
  assert.throws(
    () => resolveRuntimeArtifact(baseline, {
      stableAssetId: ASSETS[0].stableAssetId,
      stableContentKeyHex: stableContentKeyHex(ASSETS[0].stableAssetId),
      sandboxAssetKind: ASSETS[0].sandboxAssetKind,
      quality: "automatic"
    }),
    (error) => error instanceof RuntimeImportValidationError && error.code === "lookup_failed"
  );
});
