import { execFile } from "node:child_process";
import { readFileSync } from "node:fs";
import { readFile } from "node:fs/promises";
import { isDeepStrictEqual, promisify } from "node:util";
import { relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

import {
  ASSETS,
  EXPECTED_STABLE_ASSET_KINDS,
  PACKAGE_ID
} from "./system-sandbox-blockout-spec.mjs";
import {
  ART_SOURCE_COMMIT,
  AUTHORITATIVE_ROOT,
  GENERATOR_VERSION,
  LOCKED_BROWSER,
  LOCKED_NODE_VERSION,
  LOCKED_PLAYWRIGHT_VERSION,
  LOCKED_PNGJS_VERSION,
  MANIFEST_PATH,
  OUTPUT_ROOT,
  PACKAGE_ROOT,
  QUALITY_TIERS,
  ROOT_ANCHOR_UV,
  ROOT_INTEGRATION_COMMIT,
  RUNTIME_SCHEMA_VERSION,
  extractStableAssetGroup,
  inspectRgbaPng,
  sha256,
  stableContentKeyHex
} from "./generate-system-sandbox-runtime-assets.mjs";

const execFileAsync = promisify(execFile);
const EXPECTED_IDS = Object.freeze(ASSETS.map((asset) => asset.stableAssetId));
const FORBIDDEN_NORMALIZED_KEYS = new Set([
  "damage", "tick", "range", "cooldown", "faction", "duty", "objective", "wave", "retry",
  "collisionwriter", "taskprogression", "progressionwriter"
]);
const ROOT_KEYS = Object.freeze([
  "schemaVersion", "taskId", "artSourceCommit", "rootIntegrationCommit", "authoritativeRoot", "package",
  "status", "integrationStatus", "previewReady", "releaseAllowed", "allowedChannel", "license",
  "runtimeBuildStatus", "platformPreviewStatus", "sourcePackageRoot", "generatedArtifactRoot",
  "generatedArtifactsTrackedByGit", "lookupContract", "generatorIdentity", "limits", "measured", "entries"
]);
const ENTRY_KEYS = Object.freeze([
  "stableAssetId", "stableContentKeyHex", "sandboxAssetKind", "slug", "owner", "package", "status",
  "integrationStatus", "previewReady", "releaseAllowed", "allowedChannel", "license", "runtimeBuildStatus",
  "platformPreviewStatus", "sourceMode", "reuseLineage", "source", "rootAnchorUv", "axes", "unit",
  "facingConvention", "visualBoundsMm", "nominalPresentationExtentMm", "anchors", "artifacts",
  "platformQuality", "accessibility", "budget", "dependencies"
]);
const ARTIFACT_KEYS = Object.freeze([
  "artifactId", "path", "format", "pixelFormat", "colorSpace", "alphaMode", "filter", "wrap", "mipmaps",
  "width", "height", "rootAnchorPx", "sha256", "fileBytes", "decodedBytes", "transparentPixels",
  "visiblePixels", "grayscaleSilhouetteSha256"
]);
const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const GENERATOR_SOURCE_SHA256 = sha256(
  readFileSync(fileURLToPath(new URL("./generate-system-sandbox-runtime-assets.mjs", import.meta.url)), "utf8")
    .replaceAll("\r\n", "\n")
);

export class RuntimeImportValidationError extends Error {
  constructor(code, message) {
    super(`${code}: ${message}`);
    this.name = "RuntimeImportValidationError";
    this.code = code;
  }
}

function fail(code, message) {
  throw new RuntimeImportValidationError(code, message);
}

function exactKeys(value, keys, path) {
  if (!value || typeof value !== "object" || Array.isArray(value)) fail("closed_shape", `${path} must be an object`);
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  if (!isDeepStrictEqual(actual, expected)) {
    fail("closed_shape", `${path} keys differ; actual=${actual.join(",")}; expected=${expected.join(",")}`);
  }
}

function exact(value, expected, code, path) {
  if (!isDeepStrictEqual(value, expected)) fail(code, `${path} differs from its frozen value`);
}

function unique(values, code, path) {
  if (new Set(values).size !== values.length) fail(code, `${path} must be unique`);
}

function normalizedKey(value) {
  return String(value).toLowerCase().replaceAll(/[^a-z0-9]/g, "");
}

function assertNoForbiddenKeys(value, path = "manifest") {
  if (Array.isArray(value)) {
    value.forEach((entry, index) => assertNoForbiddenKeys(entry, `${path}[${index}]`));
    return;
  }
  if (!value || typeof value !== "object") return;
  for (const [key, nested] of Object.entries(value)) {
    if (FORBIDDEN_NORMALIZED_KEYS.has(normalizedKey(key))) {
      fail("forbidden_gameplay_field", `${path}.${key} is Gameplay-authoritative`);
    }
    assertNoForbiddenKeys(nested, `${path}.${key}`);
  }
}

function assertPortableStrings(value, maximumBytes, path = "manifest") {
  if (typeof value === "string") {
    if (Buffer.byteLength(value, "utf8") > maximumBytes) fail("capacity_exceeded", `${path} exceeds ${maximumBytes} UTF-8 bytes`);
    if (value.includes("\0") || value.includes("\n") || /^[a-z]:[\\/]|^\\\\|^file:\/\//i.test(value)) {
      fail("unsafe_path", `${path} contains a non-portable or absolute value`);
    }
    if (/(?:^|\b)(?:asset|actor|prop|effect|interaction|mechanism|jn)[._-]f1(?:[._-]|\b)/i.test(value)) {
      fail("invalid_stable_id", `${path} contains an F1 content identifier`);
    }
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((entry, index) => assertPortableStrings(entry, maximumBytes, `${path}[${index}]`));
    return;
  }
  if (value && typeof value === "object") {
    for (const [key, nested] of Object.entries(value)) assertPortableStrings(nested, maximumBytes, `${path}.${key}`);
  }
}

function safeRelativePath(value, prefix, suffix, path) {
  if (typeof value !== "string" || value.includes("\\") || value.startsWith("/") || value.split("/").some((part) => !part || part === "." || part === "..")) {
    fail("unsafe_path", `${path} is not a normalized relative path`);
  }
  if (!value.startsWith(`${prefix}/`) || !value.endsWith(suffix)) fail("unsafe_path", `${path} is outside ${prefix}/*${suffix}`);
}

function jsonBytes(manifest) {
  return Buffer.byteLength(`${JSON.stringify(manifest, null, 2)}\n`, "utf8");
}

function pngChunks(buffer, path) {
  if (!Buffer.isBuffer(buffer) || buffer.length < 45 || !buffer.subarray(0, 8).equals(PNG_SIGNATURE)) {
    fail("artifact_format", `${path} lacks a PNG signature`);
  }
  const chunks = [];
  let offset = 8;
  while (offset < buffer.length) {
    if (offset + 12 > buffer.length) fail("artifact_format", `${path} has a truncated PNG chunk`);
    const length = buffer.readUInt32BE(offset);
    const type = buffer.toString("ascii", offset + 4, offset + 8);
    const end = offset + 12 + length;
    if (end > buffer.length) fail("artifact_format", `${path} has an oversized ${type} chunk`);
    chunks.push({ type, data: buffer.subarray(offset + 8, offset + 8 + length) });
    offset = end;
    if (type === "IEND") break;
  }
  if (offset !== buffer.length || chunks.at(-1)?.type !== "IEND") fail("artifact_format", `${path} has trailing data or no IEND`);
  return chunks;
}

function validatePng(buffer, artifact, quality, stableAssetId) {
  const path = `${stableAssetId}.${quality}`;
  if (sha256(buffer) !== artifact.sha256 || buffer.length !== artifact.fileBytes) {
    fail("artifact_hash_mismatch", `${path} bytes/hash do not match the manifest`);
  }
  const chunks = pngChunks(buffer, path);
  const ihdr = chunks[0];
  if (ihdr?.type !== "IHDR" || ihdr.data.length !== 13 || ihdr.data[8] !== 8 || ihdr.data[9] !== 6) {
    fail("artifact_format", `${path} must be RGBA8 PNG`);
  }
  if (ihdr.data.readUInt32BE(0) !== artifact.width || ihdr.data.readUInt32BE(4) !== artifact.height) {
    fail("artifact_dimensions", `${path} IHDR dimensions differ from the manifest`);
  }
  const srgb = chunks.filter((chunk) => chunk.type === "sRGB");
  if (srgb.length !== 1 || srgb[0].data.length !== 1 || srgb[0].data[0] !== 0) {
    fail("artifact_format", `${path} needs one sRGB perceptual-intent chunk`);
  }
  let inspection;
  try {
    inspection = inspectRgbaPng(buffer);
  } catch (error) {
    fail("artifact_format", `${path} cannot be decoded: ${error.message}`);
  }
  if (inspection.width !== artifact.width || inspection.height !== artifact.height) fail("artifact_dimensions", `${path} decoded dimensions differ`);
  if (inspection.transparentPixels <= 0 || inspection.visiblePixels <= 0
      || inspection.transparentPixels + inspection.visiblePixels !== artifact.width * artifact.height) {
    fail("alpha_invalid", `${path} must retain both transparent and visible pixels`);
  }
  if (inspection.transparentPixels !== artifact.transparentPixels || inspection.visiblePixels !== artifact.visiblePixels) {
    fail("alpha_invalid", `${path} alpha evidence differs from the manifest`);
  }
  if (inspection.silhouetteSha256 !== artifact.grayscaleSilhouetteSha256) {
    fail("silhouette_mismatch", `${path} grayscale alpha silhouette differs`);
  }
  return inspection;
}

function validateHeader(manifest) {
  exactKeys(manifest, ROOT_KEYS, "manifest");
  exact(manifest.schemaVersion, RUNTIME_SCHEMA_VERSION, "identity_mismatch", "schemaVersion");
  exact(manifest.taskId, "ART-003", "identity_mismatch", "taskId");
  exact(manifest.artSourceCommit, ART_SOURCE_COMMIT, "identity_mismatch", "artSourceCommit");
  exact(manifest.rootIntegrationCommit, ROOT_INTEGRATION_COMMIT, "identity_mismatch", "rootIntegrationCommit");
  exact(manifest.authoritativeRoot, AUTHORITATIVE_ROOT, "identity_mismatch", "authoritativeRoot");
  exact(manifest.package, PACKAGE_ID, "identity_mismatch", "package");
  exact([
    manifest.status, manifest.integrationStatus, manifest.previewReady, manifest.releaseAllowed,
    manifest.allowedChannel, manifest.license, manifest.runtimeBuildStatus
  ], [
    "Blockout", "not-integrated", false, false, "internal-preview",
    "review-recorded-not-release-cleared", "not-produced"
  ], "maturity_overclaim", "package maturity/license gate");
  exact(manifest.platformPreviewStatus, { windows: "not-validated", web: "not-validated" }, "maturity_overclaim", "platformPreviewStatus");
  exact(manifest.sourcePackageRoot, "assets_src/system-sandbox-blockouts", "identity_mismatch", "sourcePackageRoot");
  exact(manifest.generatedArtifactRoot, "build/generated-assets/system-demo-blockouts", "identity_mismatch", "generatedArtifactRoot");
  exact(manifest.generatedArtifactsTrackedByGit, false, "maturity_overclaim", "generatedArtifactsTrackedByGit");

  exactKeys(manifest.lookupContract, [
    "key", "verifyStableContentKey", "pathInferenceAllowed", "unknownId", "wrongKind", "missingArtifact",
    "partialTableExposure", "standardFallback"
  ], "lookupContract");
  exact(manifest.lookupContract, {
    key: ["stableAssetId", "sandboxAssetKind"],
    verifyStableContentKey: true,
    pathInferenceAllowed: false,
    unknownId: "fail-closed",
    wrongKind: "fail-closed",
    missingArtifact: "fail-closed",
    partialTableExposure: false,
    standardFallback: "same-entry-explicit-low-only"
  }, "lookup_failed", "lookupContract");

  exactKeys(manifest.generatorIdentity, [
    "generatorVersion", "generatorSourceSha256", "nodeVersion", "playwrightVersion", "pngjsVersion", "browserName",
    "browserRevision", "browserVersion", "browserExecutableSha256", "deviceScaleFactor", "rasterFlags", "canonicalPng"
  ], "generatorIdentity");
  exact(manifest.generatorIdentity, {
    generatorVersion: GENERATOR_VERSION,
    generatorSourceSha256: GENERATOR_SOURCE_SHA256,
    nodeVersion: LOCKED_NODE_VERSION,
    playwrightVersion: LOCKED_PLAYWRIGHT_VERSION,
    pngjsVersion: LOCKED_PNGJS_VERSION,
    browserName: LOCKED_BROWSER.name,
    browserRevision: LOCKED_BROWSER.revision,
    browserVersion: LOCKED_BROWSER.version,
    browserExecutableSha256: LOCKED_BROWSER.executableSha256,
    deviceScaleFactor: 1,
    rasterFlags: [
      "--disable-gpu", "--disable-lcd-text", "--disable-font-subpixel-positioning",
      "--force-color-profile=srgb", "--hide-scrollbars"
    ],
    canonicalPng: "pngjs-rgba8-deflate9-filter4-srgb-intent0"
  }, "generator_identity", "generatorIdentity");

  exactKeys(manifest.limits, [
    "entries", "artifacts", "maximumUtf8StringBytes", "standardTransferBytes", "lowTransferBytes",
    "manifestBytes", "standardDecodedBytes", "lowDecodedBytes"
  ], "limits");
  exact(manifest.limits, {
    entries: 12,
    artifacts: 24,
    maximumUtf8StringBytes: 256,
    standardTransferBytes: 512 * 1024,
    lowTransferBytes: 256 * 1024,
    manifestBytes: 64 * 1024,
    standardDecodedBytes: 3 * 1024 * 1024,
    lowDecodedBytes: 768 * 1024
  }, "capacity_exceeded", "limits");
  exactKeys(manifest.measured, [
    "entries", "artifacts", "standardTransferBytes", "lowTransferBytes", "standardDecodedBytes", "lowDecodedBytes"
  ], "measured");
}

function validateSource(entry, expected, sources) {
  exactKeys(entry.source, ["path", "sha256", "bytes", "groupSelector", "groupSha256"], `${entry.stableAssetId}.source`);
  exact(entry.source.path, `fallbacks/${expected.slug}.svg`, "source_drift", `${entry.stableAssetId}.source.path`);
  safeRelativePath(entry.source.path, "fallbacks", ".svg", `${entry.stableAssetId}.source.path`);
  exact(entry.source.groupSelector, `g[data-stable-asset-id="${entry.stableAssetId}"]`, "source_drift", `${entry.stableAssetId}.source.groupSelector`);
  const source = sources.get(entry.source.path);
  if (typeof source !== "string") fail("source_drift", `${entry.source.path} is missing`);
  let group;
  try {
    group = extractStableAssetGroup(source, entry.stableAssetId);
  } catch (error) {
    fail("source_drift", error.message);
  }
  if (sha256(source) !== entry.source.sha256 || Buffer.byteLength(source, "utf8") !== entry.source.bytes
      || sha256(group) !== entry.source.groupSha256) {
    fail("source_drift", `${entry.stableAssetId} source/group identity drifted`);
  }
}

function validateArtifact(entry, expected, quality, artifact, artifacts) {
  exactKeys(artifact, ARTIFACT_KEYS, `${entry.stableAssetId}.artifacts.${quality}`);
  const tier = QUALITY_TIERS[quality];
  exact(artifact.artifactId, `artifact.system_demo.${expected.slug}.${quality}`, "identity_mismatch", `${entry.stableAssetId}.${quality}.artifactId`);
  exact(artifact.path, `${quality}/${expected.slug}.png`, "identity_mismatch", `${entry.stableAssetId}.${quality}.path`);
  safeRelativePath(artifact.path, quality, ".png", `${entry.stableAssetId}.${quality}.path`);
  exact([
    artifact.format, artifact.pixelFormat, artifact.colorSpace, artifact.alphaMode,
    artifact.filter, artifact.wrap, artifact.mipmaps
  ], [
    "PNG", "RGBA8", "sRGB", "straight-source-premultiply-on-upload",
    "linear", "clamp", false
  ], "artifact_format", `${entry.stableAssetId}.${quality} import contract`);
  exact([artifact.width, artifact.height, artifact.rootAnchorPx], [tier.width, tier.height, tier.rootAnchorPx], "artifact_dimensions", `${entry.stableAssetId}.${quality} dimensions/anchor`);
  exact(artifact.decodedBytes, tier.width * tier.height * 4, "budget_exceeded", `${entry.stableAssetId}.${quality}.decodedBytes`);
  const perItemLimit = quality === "standard" ? entry.budget.standardMaxFileBytes : entry.budget.lowMaxFileBytes;
  if (!Number.isInteger(artifact.fileBytes) || artifact.fileBytes <= 0 || artifact.fileBytes > perItemLimit) {
    fail("budget_exceeded", `${entry.stableAssetId}.${quality} file budget exceeded`);
  }
  const bytes = artifacts.get(artifact.path);
  if (!Buffer.isBuffer(bytes)) fail("artifact_missing", `${artifact.path} is unavailable`);
  return validatePng(bytes, artifact, quality, entry.stableAssetId);
}

function validateEntry(entry, expected, artifacts, sources) {
  exactKeys(entry, ENTRY_KEYS, expected.stableAssetId);
  if (entry.stableAssetId === expected.stableAssetId && entry.sandboxAssetKind !== expected.sandboxAssetKind) {
    fail("asset_kind_mismatch", `${entry.stableAssetId} must be ${expected.sandboxAssetKind}, not ${entry.sandboxAssetKind}`);
  }
  exact([
    entry.stableAssetId, entry.stableContentKeyHex, entry.sandboxAssetKind, entry.slug, entry.owner, entry.package
  ], [
    expected.stableAssetId, stableContentKeyHex(expected.stableAssetId), expected.sandboxAssetKind,
    expected.slug, "ART", PACKAGE_ID
  ], "identity_mismatch", `${expected.stableAssetId} identity`);
  if (EXPECTED_STABLE_ASSET_KINDS.get(entry.stableAssetId) !== entry.sandboxAssetKind) {
    fail("asset_kind_mismatch", `${entry.stableAssetId} has the wrong SandboxAssetKind`);
  }
  exact([
    entry.status, entry.integrationStatus, entry.previewReady, entry.releaseAllowed, entry.allowedChannel,
    entry.license, entry.runtimeBuildStatus
  ], [
    "Blockout", "not-integrated", false, false, "internal-preview",
    "review-recorded-not-release-cleared", "not-produced"
  ], "maturity_overclaim", `${entry.stableAssetId} maturity/license gate`);
  exact(entry.platformPreviewStatus, { windows: "not-validated", web: "not-validated" }, "maturity_overclaim", `${entry.stableAssetId}.platformPreviewStatus`);
  exact([entry.sourceMode, entry.reuseLineage], [expected.sourceMode, expected.reuseLineage ?? null], "source_drift", `${entry.stableAssetId} source lineage`);
  validateSource(entry, expected, sources);
  exact(entry.rootAnchorUv, ROOT_ANCHOR_UV, "artifact_dimensions", `${entry.stableAssetId}.rootAnchorUv`);
  exact(entry.axes, expected.axes, "identity_mismatch", `${entry.stableAssetId}.axes`);
  exact(entry.unit, "millimeter", "identity_mismatch", `${entry.stableAssetId}.unit`);
  exact(entry.facingConvention, expected.facingConvention, "identity_mismatch", `${entry.stableAssetId}.facingConvention`);
  exact(entry.visualBoundsMm, expected.visualBoundsMm ?? null, "identity_mismatch", `${entry.stableAssetId}.visualBoundsMm`);
  exact(entry.nominalPresentationExtentMm, expected.nominalPresentationExtentMm ?? null, "identity_mismatch", `${entry.stableAssetId}.nominalPresentationExtentMm`);
  if ((entry.visualBoundsMm === null) === (entry.nominalPresentationExtentMm === null)) {
    fail("identity_mismatch", `${entry.stableAssetId} needs exactly one visual bounds/nominal extent declaration`);
  }
  exact(entry.anchors, expected.anchors, "identity_mismatch", `${entry.stableAssetId}.anchors`);
  exactKeys(entry.artifacts, ["standard", "low"], `${entry.stableAssetId}.artifacts`);
  const standardInspection = validateArtifact(entry, expected, "standard", entry.artifacts.standard, artifacts);
  const lowInspection = validateArtifact(entry, expected, "low", entry.artifacts.low, artifacts);
  exactKeys(entry.platformQuality, ["windows", "web"], `${entry.stableAssetId}.platformQuality`);
  const platformBinding = {
    standard: entry.artifacts.standard.artifactId,
    low: entry.artifacts.low.artifactId
  };
  exact(entry.platformQuality, { windows: platformBinding, web: platformBinding }, "lookup_failed", `${entry.stableAssetId}.platformQuality`);
  exactKeys(entry.accessibility, ["colorOnly", "audioOnly", "shapeToken", "nonColorChannels"], `${entry.stableAssetId}.accessibility`);
  exact(entry.accessibility, {
    colorOnly: false,
    audioOnly: false,
    shapeToken: expected.shapeToken,
    nonColorChannels: expected.nonColorChannels
  }, "color_only_distinction", `${entry.stableAssetId}.accessibility`);
  if (entry.accessibility.nonColorChannels.length < 2) fail("color_only_distinction", `${entry.stableAssetId} needs at least two non-color cues`);
  exactKeys(entry.budget, [
    "standardMaxFileBytes", "lowMaxFileBytes", "targetTransportKiB", "targetGpuTextureMiB",
    "targetDrawCalls", "peakTransparentCoveragePercent", "peakLayerCount"
  ], `${entry.stableAssetId}.budget`);
  exact(entry.budget, {
    standardMaxFileBytes: 48 * 1024,
    lowMaxFileBytes: 24 * 1024,
    targetTransportKiB: expected.budget.targetTransportKiB,
    targetGpuTextureMiB: expected.budget.targetGpuTextureMiB,
    targetDrawCalls: expected.budget.targetDrawCalls,
    peakTransparentCoveragePercent: expected.budget.peakTransparentCoveragePercent,
    peakLayerCount: expected.budget.peakLayerCount ?? 1
  }, "budget_exceeded", `${entry.stableAssetId}.budget`);
  exact(entry.dependencies, expected.dependencies, "identity_mismatch", `${entry.stableAssetId}.dependencies`);
  return { standardInspection, lowInspection };
}

export function validateRuntimeImportPackage({ manifest, artifacts, sources, manifestText } = {}) {
  assertNoForbiddenKeys(manifest);
  validateHeader(manifest);
  assertPortableStrings(manifest, manifest.limits.maximumUtf8StringBytes);
  if (!Array.isArray(manifest.entries) || manifest.entries.length !== 12) fail("dense_table", "manifest must contain exactly 12 entries");
  if (!(artifacts instanceof Map) || !(sources instanceof Map)) fail("dense_table", "artifact and source tables must be Maps");

  const ids = manifest.entries.map((entry) => entry?.stableAssetId);
  exact(ids, EXPECTED_IDS, "dense_table", "entry order/Stable IDs");
  unique(ids, "dense_table", "Stable Asset IDs");
  preflightManifestPaths(manifest);
  unique(manifest.entries.flatMap((entry) => [entry.artifacts.standard.artifactId, entry.artifacts.low.artifactId]), "universal_fallback", "preflight artifact IDs");
  unique(manifest.entries.map((entry) => entry.source.groupSelector), "universal_fallback", "preflight group selectors");
  unique(manifest.entries.map((entry) => entry.accessibility?.shapeToken), "color_only_distinction", "preflight shape tokens");
  const artifactPaths = [];
  const artifactIds = [];
  const sourcePaths = [];
  const groupSelectors = [];
  const groupHashes = [];
  const shapeTokens = [];
  const lowSilhouettes = [];
  let standardTransferBytes = 0;
  let lowTransferBytes = 0;
  let standardDecodedBytes = 0;
  let lowDecodedBytes = 0;

  manifest.entries.forEach((entry, index) => {
    const result = validateEntry(entry, ASSETS[index], artifacts, sources);
    sourcePaths.push(entry.source.path);
    groupSelectors.push(entry.source.groupSelector);
    groupHashes.push(entry.source.groupSha256);
    shapeTokens.push(entry.accessibility.shapeToken);
    for (const quality of ["standard", "low"]) {
      const artifact = entry.artifacts[quality];
      artifactPaths.push(artifact.path);
      artifactIds.push(artifact.artifactId);
    }
    lowSilhouettes.push(result.lowInspection.silhouetteSha256);
    standardTransferBytes += entry.artifacts.standard.fileBytes;
    lowTransferBytes += entry.artifacts.low.fileBytes;
    standardDecodedBytes += entry.artifacts.standard.decodedBytes;
    lowDecodedBytes += entry.artifacts.low.decodedBytes;
  });

  unique(artifactPaths, "universal_fallback", "artifact paths");
  unique(artifactIds, "universal_fallback", "artifact IDs");
  unique(sourcePaths, "universal_fallback", "fallback source paths");
  unique(groupSelectors, "universal_fallback", "fallback group selectors");
  unique(groupHashes, "universal_fallback", "fallback group identities");
  unique(shapeTokens, "color_only_distinction", "shape tokens");
  unique(lowSilhouettes, "silhouette_mismatch", "128px grayscale silhouettes");
  unique(lowSilhouettes.slice(0, 4), "silhouette_mismatch", "player/pressure/flanker/elite 128px silhouettes");
  if (artifacts.size !== 24 || [...artifacts.keys()].some((path) => !artifactPaths.includes(path))) {
    fail("dense_table", "artifact table must expose exactly the 24 declared paths and no extras");
  }
  if (sources.size !== 12 || [...sources.keys()].some((path) => !sourcePaths.includes(path))) {
    fail("dense_table", "source table must expose exactly the 12 declared fallback SVGs and no extras");
  }

  const measured = {
    entries: 12,
    artifacts: 24,
    standardTransferBytes,
    lowTransferBytes,
    standardDecodedBytes,
    lowDecodedBytes
  };
  exact(manifest.measured, measured, "budget_exceeded", "measured totals");
  if (standardTransferBytes > manifest.limits.standardTransferBytes
      || lowTransferBytes > manifest.limits.lowTransferBytes
      || standardDecodedBytes > manifest.limits.standardDecodedBytes
      || lowDecodedBytes > manifest.limits.lowDecodedBytes
      || (manifestText ? Buffer.byteLength(manifestText, "utf8") : jsonBytes(manifest)) > manifest.limits.manifestBytes) {
    fail("budget_exceeded", "package transfer/decoded/manifest capacity exceeded");
  }
  return {
    stableAssetSlots: 12,
    runtimeArtifacts: 24,
    standardTransferBytes,
    lowTransferBytes,
    standardDecodedBytes,
    lowDecodedBytes,
    manifestBytes: manifestText ? Buffer.byteLength(manifestText, "utf8") : jsonBytes(manifest),
    actorSilhouettesDistinct: 4,
    maturity: "Blockout/not-integrated",
    previewReady: false
  };
}

function preflightManifestPaths(manifest) {
  if (!manifest || !Array.isArray(manifest.entries) || manifest.entries.length !== 12) fail("dense_table", "cannot load a partial entry table");
  const artifacts = [];
  const sources = [];
  for (const entry of manifest.entries) {
    if (!entry?.source?.path || !entry?.artifacts?.standard?.path || !entry?.artifacts?.low?.path) {
      fail("dense_table", "cannot load incomplete source/artifact declarations");
    }
    safeRelativePath(entry.source.path, "fallbacks", ".svg", "source.path");
    sources.push(entry.source.path);
    for (const quality of ["standard", "low"]) {
      const path = entry.artifacts[quality].path;
      safeRelativePath(path, quality, ".png", `${quality}.path`);
      artifacts.push(path);
    }
  }
  unique(artifacts, "universal_fallback", "preflight artifact paths");
  unique(sources, "universal_fallback", "preflight source paths");
  return { artifacts, sources };
}

export async function loadRuntimeImportPackage() {
  const manifestText = await readFile(MANIFEST_PATH, "utf8").catch((error) => fail("artifact_missing", `manifest unavailable: ${error.message}`));
  let manifest;
  try {
    manifest = JSON.parse(manifestText);
  } catch (error) {
    fail("closed_shape", `manifest JSON is invalid: ${error.message}`);
  }
  const declared = preflightManifestPaths(manifest);
  const artifacts = new Map();
  for (const path of declared.artifacts) {
    const absolute = resolve(OUTPUT_ROOT, path);
    const normalized = relative(OUTPUT_ROOT, absolute).split(sep).join("/");
    if (normalized !== path) fail("unsafe_path", `${path} escapes the generated artifact root`);
    const bytes = await readFile(absolute).catch((error) => fail("artifact_missing", `${path}: ${error.message}`));
    artifacts.set(path, bytes);
  }
  const sources = new Map();
  for (const path of declared.sources) {
    const absolute = resolve(PACKAGE_ROOT, path);
    const normalized = relative(PACKAGE_ROOT, absolute).split(sep).join("/");
    if (normalized !== path) fail("unsafe_path", `${path} escapes the source package root`);
    const content = await readFile(absolute, "utf8").catch((error) => fail("source_drift", `${path}: ${error.message}`));
    sources.set(path, content);
  }
  const packageData = { manifest, manifestText, artifacts, sources };
  validateRuntimeImportPackage(packageData);
  return packageData;
}

export function resolveRuntimeArtifact(packageData, request) {
  validateRuntimeImportPackage(packageData);
  exactKeys(request, ["stableAssetId", "stableContentKeyHex", "sandboxAssetKind", "quality"], "lookup request");
  if (request.quality !== "standard" && request.quality !== "low") fail("lookup_failed", "quality must be standard or low");
  const entry = packageData.manifest.entries.find((candidate) => candidate.stableAssetId === request.stableAssetId);
  if (!entry) fail("lookup_failed", `unknown Stable Asset ID ${request.stableAssetId}`);
  if (entry.stableContentKeyHex !== request.stableContentKeyHex) fail("lookup_failed", "Stable ID/content key mismatch");
  if (entry.sandboxAssetKind !== request.sandboxAssetKind) fail("asset_kind_mismatch", `${request.stableAssetId} kind mismatch`);
  const artifact = entry.artifacts[request.quality];
  const bytes = packageData.artifacts.get(artifact.path);
  if (!bytes) fail("artifact_missing", `${artifact.path} is unavailable`);
  return { entry, artifact, bytes };
}

async function git(args) {
  try {
    const result = await execFileAsync("git", args, { cwd: PACKAGE_ROOT, encoding: "utf8", maxBuffer: 2 * 1024 * 1024 });
    return result.stdout;
  } catch (error) {
    fail("authoritative_root_unavailable", error.stderr || error.message);
  }
}

async function validateAuthority(authoritativeRoot) {
  if (authoritativeRoot !== AUTHORITATIVE_ROOT) fail("authoritative_root_mismatch", `expected ${AUTHORITATIVE_ROOT}, received ${authoritativeRoot}`);
  await git(["cat-file", "-e", `${authoritativeRoot}^{commit}`]);
  await git(["cat-file", "-e", `${ROOT_INTEGRATION_COMMIT}^{commit}`]);
  const definition = await git(["show", `${authoritativeRoot}:src/contracts/include/tgd/contracts/sandbox_definition.hpp`]);
  for (const fragment of ["player = 1", "actor = 2", "obstacle = 3", "interaction = 4", "mechanism = 5", "safe_point = 6", "effect = 7"]) {
    if (!definition.includes(fragment)) fail("authoritative_root_mismatch", `SandboxAssetKind lacks ${fragment}`);
  }
  const indexText = await git(["show", `${ROOT_INTEGRATION_COMMIT}:assets_src/system-sandbox-blockouts/index.json`]);
  const index = JSON.parse(indexText);
  const rootBindings = index.slots.map((slot) => [slot.stableAssetId, slot.sandboxAssetKind]);
  exact(rootBindings, ASSETS.map((asset) => [asset.stableAssetId, asset.sandboxAssetKind]), "authoritative_root_mismatch", "ART-002 root bindings");
  return { authoritativeRoot, rootIntegrationCommit: ROOT_INTEGRATION_COMMIT, sandboxAssetKinds: 7 };
}

export async function validateRuntimePackage({ authoritativeRoot }) {
  const packageData = await loadRuntimeImportPackage();
  const runtime = validateRuntimeImportPackage(packageData);
  const authority = await validateAuthority(authoritativeRoot);
  return { runtime, authority };
}

function argument(name) {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : undefined;
}

const invokedPath = process.argv[1] ? resolve(process.argv[1]) : "";
if (invokedPath === fileURLToPath(import.meta.url)) {
  validateRuntimePackage({ authoritativeRoot: argument("--authoritative-root") })
    .then(({ runtime, authority }) => process.stdout.write(`${JSON.stringify({
      task: "ART-003",
      status: "PASS",
      ...runtime,
      authoritativeRoot: authority.authoritativeRoot,
      rootIntegrationCommit: authority.rootIntegrationCommit,
      license: "review-recorded-not-release-cleared",
      runtimeBuild: "not-produced",
      platformPreview: "not-validated"
    }, null, 2)}\n`))
    .catch((error) => {
      process.stderr.write(`${String(error.stack || error)}\n`);
      process.exitCode = 1;
    });
}
