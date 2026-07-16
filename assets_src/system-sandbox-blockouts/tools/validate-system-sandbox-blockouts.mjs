import { createHash } from "node:crypto";
import { execFile } from "node:child_process";
import { readFile, readdir, stat } from "node:fs/promises";
import { relative, resolve, sep } from "node:path";
import { promisify } from "node:util";
import { fileURLToPath } from "node:url";

import {
  ASSETS,
  AUTHORITATIVE_ROOT,
  EXPECTED_STABLE_ASSET_KINDS,
  INTEGRATION_STATUS,
  LICENSE_STATUS,
  NEW_SOURCE_ASSETS,
  PACKAGE_ID,
  STABLE_ID_PATTERN,
  STATUS,
  VIEWPORTS
} from "./system-sandbox-blockout-spec.mjs";
import {
  buildGeneratedOutputs,
  PACKAGE_ROOT
} from "./generate-system-sandbox-blockouts.mjs";

const execFileAsync = promisify(execFile);

const EXPECTED_IDS = Object.freeze([...EXPECTED_STABLE_ASSET_KINDS.keys()]);
const EXPECTED_NEW_SOURCE_IDS = Object.freeze(NEW_SOURCE_ASSETS.map((entry) => entry.stableAssetId));
const REQUIRED_METADATA_GROUPS = Object.freeze(["source", "import", "license", "runtime", "preview", "budget"]);
const EXPECTED_MANUAL_PATHS = Object.freeze([
  "README.md",
  "tests/system-sandbox-blockout-contract.test.mjs",
  "tools/generate-system-sandbox-blockouts.mjs",
  "tools/system-sandbox-blockout-spec.mjs",
  "tools/validate-system-sandbox-blockouts.mjs"
]);
const FORBIDDEN_NORMALIZED_KEYS = new Set([
  "damage",
  "tick",
  "range",
  "cooldown",
  "faction",
  "duty",
  "objective",
  "wave",
  "retry",
  "collisionwriter",
  "taskprogression",
  "progressionwriter"
]);
const FORBIDDEN_BINARY_EXTENSIONS = new Set([
  ".blend", ".fbx", ".glb", ".gltf", ".png", ".jpg", ".jpeg", ".webp", ".tga", ".psd", ".kra", ".wav", ".ogg", ".mp3"
]);

export class ContractValidationError extends Error {
  constructor(code, message) {
    super(code + ": " + message);
    this.name = "ContractValidationError";
    this.code = code;
  }
}

function fail(code, message) {
  throw new ContractValidationError(code, message);
}

function unique(values) {
  return new Set(values).size === values.length;
}

function normalizedKey(value) {
  return String(value).toLowerCase().replaceAll(/[^a-z0-9]/g, "");
}

function assertNoForbiddenKeys(value, path = "metadata") {
  if (Array.isArray(value)) {
    value.forEach((entry, index) => assertNoForbiddenKeys(entry, path + "[" + index + "]"));
    return;
  }
  if (!value || typeof value !== "object") return;
  for (const [key, nested] of Object.entries(value)) {
    if (FORBIDDEN_NORMALIZED_KEYS.has(normalizedKey(key))) {
      fail("forbidden_gameplay_field", path + "." + key + " is Gameplay-authoritative and forbidden");
    }
    assertNoForbiddenKeys(nested, path + "." + key);
  }
}

function assertNoF1ContentId(value, path = "value") {
  if (Array.isArray(value)) {
    value.forEach((entry, index) => assertNoF1ContentId(entry, path + "[" + index + "]"));
    return;
  }
  if (value && typeof value === "object") {
    for (const [key, nested] of Object.entries(value)) {
      assertNoF1ContentId(nested, path + "." + key);
    }
    return;
  }
  if (typeof value !== "string") return;
  const looksLikeF1Id = /(?:^|\b)(?:asset|actor|prop|effect|interaction|mechanism|jn)[._-]f1(?:[._-]|\b)/i.test(value)
    || /(?:^|\b)jn_[a-z0-9_]*f1(?:_|\b)/i.test(value);
  if (looksLikeF1Id) fail("invalid_stable_id", path + " contains an F1 content identifier");
}

function assertFiniteMap(value, path, allowZero) {
  if (!value || typeof value !== "object" || Array.isArray(value) || Object.keys(value).length === 0) {
    fail("missing_asset_metadata", path + " must be a non-empty numeric map");
  }
  for (const [key, number] of Object.entries(value)) {
    if (!Number.isFinite(number) || (allowZero ? number < 0 : number <= 0)) {
      fail("missing_asset_metadata", path + "." + key + " must be " + (allowZero ? "non-negative" : "positive"));
    }
  }
}

function assertPlatforms(entry) {
  const platforms = entry.platformPlan;
  if (!platforms || !platforms.web || !platforms.windows) {
    fail("missing_platform_variant", entry.stableAssetId + " needs Web and Windows plans");
  }
  if (!unique(Object.keys(platforms)) || Object.keys(platforms).sort().join(",") !== "web,windows") {
    fail("missing_platform_variant", entry.stableAssetId + " has unexpected platform keys");
  }
  for (const platform of ["web", "windows"]) {
    const plan = platforms[platform];
    if (plan.variantState !== "not-built" || plan.integrationState !== INTEGRATION_STATUS) {
      fail("missing_platform_variant", entry.stableAssetId + " " + platform + " must remain not-built/not-integrated");
    }
  }
}

function assertAnchors(entry) {
  if (!Array.isArray(entry.anchors) || entry.anchors.length === 0) {
    fail("missing_asset_anchor", entry.stableAssetId + " needs at least one presentation anchor");
  }
  if (!unique(entry.anchors.map((anchor) => anchor.name))) {
    fail("missing_asset_anchor", entry.stableAssetId + " has duplicate anchor names");
  }
  for (const anchor of entry.anchors) {
    if (!anchor.name || !anchor.role || !anchor.localMm) {
      fail("missing_asset_anchor", entry.stableAssetId + " has an incomplete anchor");
    }
    for (const axis of ["x", "y", "height"]) {
      if (!Number.isFinite(anchor.localMm[axis])) {
        fail("missing_asset_anchor", entry.stableAssetId + " anchor " + anchor.name + " lacks " + axis);
      }
    }
  }
}

function assertAccessibility(entry) {
  if (!entry.shapeToken || !Array.isArray(entry.nonColorChannels) || entry.nonColorChannels.length < 2) {
    fail("color_only_distinction", entry.stableAssetId + " needs shape plus at least two non-color channels");
  }
  if (!unique(entry.nonColorChannels)) {
    fail("color_only_distinction", entry.stableAssetId + " repeats a non-color channel");
  }
}

function assertBudget(entry) {
  const budget = entry.budget;
  if (!budget || !Number.isFinite(budget.targetTransportKiB) || budget.targetTransportKiB <= 0 || budget.targetTransportKiB > 1024) {
    fail("web_budget_exceeded", entry.stableAssetId + " target transport must be 1..1024 KiB");
  }
  if (!Number.isFinite(budget.targetGpuTextureMiB) || budget.targetGpuTextureMiB <= 0 || budget.targetGpuTextureMiB > 8) {
    fail("web_budget_exceeded", entry.stableAssetId + " GPU texture target must be >0 and <=8 MiB");
  }
  if (!Number.isInteger(budget.targetDrawCalls) || budget.targetDrawCalls < 1 || budget.targetDrawCalls > 3) {
    fail("web_budget_exceeded", entry.stableAssetId + " draw-call target must be 1..3");
  }
  if (!Number.isFinite(budget.peakTransparentCoveragePercent)
      || budget.peakTransparentCoveragePercent < 0
      || budget.peakTransparentCoveragePercent > 10) {
    fail("web_budget_exceeded", entry.stableAssetId + " transparent coverage must be 0..10 percent");
  }
  if (budget.peakLayerCount !== undefined && (!Number.isInteger(budget.peakLayerCount) || budget.peakLayerCount > 2)) {
    fail("web_budget_exceeded", entry.stableAssetId + " transparent layer target must be <=2");
  }
}

export function validateContractEntries(entries) {
  if (!Array.isArray(entries) || entries.length !== EXPECTED_IDS.length) {
    fail("missing_asset_metadata", "expected exactly 12 Stable Asset slots");
  }

  const byId = new Map(entries.map((entry) => [entry.stableAssetId, entry]));
  if (byId.size !== entries.length) fail("invalid_stable_id", "Stable Asset IDs must be unique");

  for (const expectedId of EXPECTED_IDS) {
    const entry = byId.get(expectedId);
    if (!entry) fail("invalid_stable_id", "missing exact Stable Asset ID " + expectedId);
    const expectedKind = EXPECTED_STABLE_ASSET_KINDS.get(expectedId);
    if (entry.sandboxAssetKind !== expectedKind) {
      fail("asset_kind_mismatch", expectedId + " must be " + expectedKind + ", not " + entry.sandboxAssetKind);
    }
  }

  const fallbackIds = entries.map((entry) => entry.fallbackId);
  const shapeTokens = entries.map((entry) => entry.shapeToken);
  if (!unique(fallbackIds) || !unique(shapeTokens)) {
    fail("universal_placeholder_conflict", "every slot needs a distinct fallback identity and silhouette token");
  }

  const sourceIds = entries.filter((entry) => entry.sourceMode === "new-source").map((entry) => entry.stableAssetId);
  if (sourceIds.length !== 8 || sourceIds.sort().join("\n") !== [...EXPECTED_NEW_SOURCE_IDS].sort().join("\n")) {
    fail("missing_asset_metadata", "exactly the frozen eight slots must own new source SVGs");
  }

  for (const entry of entries) {
    if (!STABLE_ID_PATTERN.test(entry.stableAssetId) || !entry.stableAssetId.startsWith("asset.system_demo.")) {
      fail("invalid_stable_id", entry.stableAssetId + " is not a system Demo Stable Asset ID");
    }
    assertNoF1ContentId(entry, entry.stableAssetId);
    assertNoForbiddenKeys(entry, entry.stableAssetId);
    if (entry.owner !== "ART" || entry.package !== PACKAGE_ID) {
      fail("missing_asset_metadata", entry.stableAssetId + " owner/package mismatch");
    }
    if (entry.status !== STATUS || entry.integrationStatus !== INTEGRATION_STATUS) {
      fail("missing_asset_metadata", entry.stableAssetId + " must remain Blockout/not-integrated");
    }
    if (!entry.fallbackId || !entry.fallbackId.startsWith("fallback.system_demo.")) {
      fail("missing_asset_metadata", entry.stableAssetId + " lacks its independent fallback identity");
    }
    if (entry.sourceMode !== "new-source" && entry.sourceMode !== "project-grammar-reuse") {
      fail("missing_asset_metadata", entry.stableAssetId + " has an invalid source mode");
    }
    if (entry.sourceMode === "project-grammar-reuse" && !entry.reuseLineage) {
      fail("missing_asset_metadata", entry.stableAssetId + " lacks project-grammar lineage");
    }
    if (entry.unit !== "millimeter" || entry.axes?.right !== "+x" || entry.axes?.forward !== "+y" || entry.axes?.up !== "+height") {
      fail("missing_asset_metadata", entry.stableAssetId + " axis/unit contract mismatch");
    }
    if (!entry.facingConvention || !Array.isArray(entry.states) || entry.states.length === 0) {
      fail("missing_asset_metadata", entry.stableAssetId + " lacks facing or visual-state metadata");
    }
    assertAnchors(entry);
    assertPlatforms(entry);
    assertAccessibility(entry);
    assertBudget(entry);
    if (!Array.isArray(entry.dependencies) || !entry.dependencies.some((value) => value.startsWith("external "))
        || !entry.dependencies.includes("future presentation resolver")) {
      fail("missing_asset_metadata", entry.stableAssetId + " must declare external authority and future resolver dependencies");
    }
    if (entry.sandboxAssetKind === "effect") {
      if (entry.visualBoundsMm) fail("forbidden_gameplay_field", entry.stableAssetId + " effect may only declare nominal presentation extent");
      assertFiniteMap(entry.nominalPresentationExtentMm, entry.stableAssetId + ".nominalPresentationExtentMm", true);
      if (!Object.values(entry.nominalPresentationExtentMm).some((value) => value > 0)) {
        fail("missing_asset_metadata", entry.stableAssetId + " nominal extent cannot be all zero");
      }
    } else {
      if (entry.nominalPresentationExtentMm) fail("missing_asset_metadata", entry.stableAssetId + " must use visual bounds");
      assertFiniteMap(entry.visualBoundsMm, entry.stableAssetId + ".visualBoundsMm", false);
    }
  }

  return {
    stableAssetSlots: entries.length,
    newSourceAssets: sourceIds.length,
    independentFallbacks: new Set(fallbackIds).size,
    distinctShapeTokens: new Set(shapeTokens).size
  };
}

function metadataByStableId(outputs) {
  const documents = new Map();
  for (const [path, content] of outputs) {
    if (!path.endsWith("/asset-metadata.json")) continue;
    const document = JSON.parse(content);
    documents.set(document.stableAssetId, document);
  }
  return documents;
}

export function validateMetadataDocuments(documents, entries = ASSETS) {
  const map = documents instanceof Map
    ? documents
    : new Map(documents.map((document) => [document.stableAssetId, document]));
  if (map.size !== entries.length) fail("missing_asset_metadata", "expected 12 metadata documents");

  const fallbackPaths = [];
  const sourcePaths = [];
  for (const entry of entries) {
    const document = map.get(entry.stableAssetId);
    if (!document) fail("missing_asset_metadata", "missing metadata for " + entry.stableAssetId);
    assertNoForbiddenKeys(document, entry.stableAssetId + ".metadata");
    assertNoF1ContentId(document, entry.stableAssetId + ".metadata");
    for (const group of REQUIRED_METADATA_GROUPS) {
      if (!document[group] || typeof document[group] !== "object") {
        fail("missing_asset_metadata", entry.stableAssetId + " missing metadata group " + group);
      }
    }
    if (document.stableAssetId !== entry.stableAssetId || document.sandboxAssetKind !== entry.sandboxAssetKind) {
      fail("asset_kind_mismatch", entry.stableAssetId + " metadata ID/kind mismatch");
    }
    if (document.owner !== "ART" || document.package !== PACKAGE_ID
        || document.status !== STATUS || document.integrationStatus !== INTEGRATION_STATUS) {
      fail("missing_asset_metadata", entry.stableAssetId + " metadata ownership or maturity mismatch");
    }
    if (document.fallbackIdentity?.id !== entry.fallbackId
        || document.fallbackIdentity?.exportName !== entry.slug
        || document.fallbackIdentity?.independentPerStableAsset !== true
        || !document.fallbackIdentity?.artifactPath) {
      fail("universal_placeholder_conflict", entry.stableAssetId + " fallback binding is not independent");
    }
    fallbackPaths.push(document.fallbackIdentity.artifactPath);
    if (entry.sourceMode === "new-source") {
      if (document.source.mode !== "new-source" || !document.source.file) {
        fail("missing_asset_metadata", entry.stableAssetId + " lacks its new source SVG binding");
      }
      sourcePaths.push(document.source.file);
    } else if (document.source.mode !== "project-grammar-reuse" || document.source.file !== null) {
      fail("missing_asset_metadata", entry.stableAssetId + " project-grammar reuse must not claim a source file");
    }
    if (document.import.status !== "planned" || document.import.artifact !== "not-produced"
        || document.import.validation !== "not-validated" || document.import.unit !== "millimeter"
        || !Array.isArray(document.import.anchors) || document.import.anchors.length === 0) {
      fail("missing_asset_metadata", entry.stableAssetId + " import plan or anchors are invalid");
    }
    if (document.license.status !== LICENSE_STATUS || document.license.externalAssetLicenses.length !== 0) {
      fail("license_blocked", entry.stableAssetId + " must remain review-recorded-not-release-cleared");
    }
    if (document.runtime.status !== "planned" || document.runtime.artifact !== "not-produced"
        || document.runtime.validation !== "not-validated" || document.runtime.integrationStatus !== INTEGRATION_STATUS
        || document.runtime.stableAssetIdChangesByPlatform !== false) {
      fail("missing_asset_metadata", entry.stableAssetId + " runtime plan overclaims integration or platform identity");
    }
    const runtimePlatforms = document.runtime.platformVariants;
    if (!runtimePlatforms?.web || !runtimePlatforms?.windows
        || runtimePlatforms.web.variantState !== "not-built"
        || runtimePlatforms.windows.variantState !== "not-built"
        || runtimePlatforms.web.integrationState !== INTEGRATION_STATUS
        || runtimePlatforms.windows.integrationState !== INTEGRATION_STATUS) {
      fail("missing_platform_variant", entry.stableAssetId + " runtime Web/Windows plans are incomplete");
    }
    if (document.preview.status !== "planned" || document.preview.runtimeCapture !== "not-produced"
        || document.preview.validation !== "not-validated" || document.preview.artifactType !== "review-board-only") {
      fail("missing_asset_metadata", entry.stableAssetId + " preview metadata overclaims readiness");
    }
    if (document.budget.packageCommitLimitBytes !== 1_500_000
        || document.budget.commitItemLimitBytes !== 32 * 1024
        || document.budget.targetTransportKiB > 1024
        || document.budget.targetGpuTextureMiB > 8
        || document.budget.targetDrawCalls > 3
        || document.budget.peakTransparentCoveragePercent > 10) {
      fail("web_budget_exceeded", entry.stableAssetId + " metadata exceeds the Web planning budget");
    }
    if (document.accessibility?.colorOnly !== false || document.accessibility?.audioOnly !== false
        || document.accessibility?.shapeToken !== entry.shapeToken
        || document.accessibility?.nonColorChannels?.length < 2) {
      fail("color_only_distinction", entry.stableAssetId + " accessibility metadata is incomplete");
    }
  }

  if (!unique(fallbackPaths) || fallbackPaths.length !== 12) {
    fail("universal_placeholder_conflict", "metadata must bind 12 distinct fallback paths");
  }
  if (!unique(sourcePaths) || sourcePaths.length !== 8) {
    fail("missing_asset_metadata", "metadata must bind exactly eight distinct source SVG paths");
  }
  return { metadataDocuments: map.size, metadataGroupsPerSlot: REQUIRED_METADATA_GROUPS.length };
}

function sha256(value) {
  return createHash("sha256").update(value, "utf8").digest("hex");
}

function assertSvgStatic(path, content) {
  if (!content.startsWith("<svg ") || !content.endsWith("</svg>\n") || !content.includes('xmlns="http://www.w3.org/2000/svg"')) {
    fail("invalid_svg", path + " is not a complete SVG/XML document");
  }
  if (/<image\b|(?:xlink:)?href\s*=|url\s*\(/i.test(content)) {
    fail("invalid_svg", path + " contains an external or embedded asset dependency");
  }
  for (const match of content.matchAll(/#[0-9a-f]{6}/gi)) {
    const rgb = [match[0].slice(1, 3), match[0].slice(3, 5), match[0].slice(5, 7)].map((part) => Number.parseInt(part, 16));
    if (Math.max(...rgb) - Math.min(...rgb) > 32) {
      fail("color_only_distinction", path + " contains a chromatic color outside the near-grayscale board palette");
    }
  }
}

function assertExactViewport(path, content, width, height) {
  const opening = '<svg xmlns="http://www.w3.org/2000/svg" width="' + width + '" height="' + height
    + '" viewBox="0 0 ' + width + " " + height + '"';
  if (!content.startsWith(opening)) fail("invalid_preview_dimensions", path + " has the wrong SVG dimensions");
}

async function listFiles(directory) {
  const result = [];
  async function visit(current) {
    for (const entry of await readdir(current, { withFileTypes: true })) {
      const absolute = resolve(current, entry.name);
      if (entry.isDirectory()) await visit(absolute);
      else result.push(absolute);
    }
  }
  await visit(directory);
  return result;
}

async function validateGeneratedArtifacts() {
  const expected = buildGeneratedOutputs();
  const metadataResult = validateMetadataDocuments(metadataByStableId(expected));
  let generatedBytes = 0;

  for (const [path, content] of expected) {
    const actual = await readFile(resolve(PACKAGE_ROOT, path), "utf8").catch(() => null);
    if (actual === null) fail("missing_artifact", path + " is missing");
    if (actual !== content) fail("stale_generated_artifact", path + " is not deterministically generated");
    generatedBytes += Buffer.byteLength(actual, "utf8");
    if (path.endsWith(".svg")) assertSvgStatic(path, actual);
  }

  for (const entry of ASSETS) {
    const fallbackPath = "fallbacks/" + entry.slug + ".svg";
    const fallback = expected.get(fallbackPath);
    if (!fallback.includes('data-stable-asset-id="' + entry.stableAssetId + '"')
        || !fallback.includes('data-shape-token="' + entry.shapeToken + '"')) {
      fail("universal_placeholder_conflict", fallbackPath + " lacks its Stable ID-specific silhouette export");
    }
    if (Buffer.byteLength(fallback, "utf8") > 32 * 1024) {
      fail("web_budget_exceeded", fallbackPath + " exceeds 32 KiB");
    }
    if (entry.sourceMode === "new-source") {
      const sourcePath = "assets/" + entry.slug + "/source.svg";
      const source = expected.get(sourcePath);
      if (!source || !source.includes('data-stable-asset-id="' + entry.stableAssetId + '"')) {
        fail("missing_artifact", sourcePath + " does not expose its Stable ID");
      }
      if (Buffer.byteLength(source, "utf8") > 32 * 1024) fail("web_budget_exceeded", sourcePath + " exceeds 32 KiB");
    }
    const document = JSON.parse(expected.get("assets/" + entry.slug + "/asset-metadata.json"));
    const fallbackContent = expected.get(document.fallbackIdentity.artifactPath);
    if (document.fallbackIdentity.sha256 !== sha256(fallbackContent)
        || document.fallbackIdentity.bytes !== Buffer.byteLength(fallbackContent, "utf8")) {
      fail("stale_generated_artifact", entry.stableAssetId + " fallback checksum/size mismatch");
    }
    if (document.source.file) {
      const sourceContent = expected.get(document.source.file);
      if (document.source.sha256 !== sha256(sourceContent)
          || document.source.bytes !== Buffer.byteLength(sourceContent, "utf8")) {
        fail("stale_generated_artifact", entry.stableAssetId + " source checksum/size mismatch");
      }
    }
  }

  for (const viewport of VIEWPORTS) {
    const path = "previews/readability-" + viewport.id + ".svg";
    const content = expected.get(path);
    assertExactViewport(path, content, viewport.width, viewport.height);
    if (!content.includes('data-thumbnail-height="128"')) fail("invalid_preview_dimensions", path + " lacks 128px evidence declaration");
    for (const id of EXPECTED_IDS) {
      if (!content.includes('data-preview-slot="' + id + '"')) fail("missing_artifact", path + " lacks " + id);
    }
  }

  const thumbnailPath = "previews/actor-thumbnails-128.svg";
  const thumbnails = expected.get(thumbnailPath);
  assertExactViewport(thumbnailPath, thumbnails, 512, 128);
  if (!thumbnails.includes('data-thumbnail-height="128"')) fail("invalid_preview_dimensions", "actor thumbnail strip is not 128px high");
  for (const id of EXPECTED_IDS.slice(0, 4)) {
    if (!thumbnails.includes('data-thumbnail-slot="' + id + '"')) fail("missing_artifact", thumbnailPath + " lacks " + id);
  }

  const evidence = JSON.parse(expected.get("previews/preview-evidence.json"));
  if (evidence.viewports.length !== 3 || evidence.actualHumanEvidence !== false
      || evidence.runtimeCapture !== "not-produced" || evidence.platformValidation !== "not-validated"
      || evidence.actorThumbnailCheck.heightPx !== 128 || evidence.actorThumbnailCheck.path !== thumbnailPath) {
    fail("missing_asset_metadata", "preview evidence overclaims maturity or lacks required dimensions");
  }

  const index = JSON.parse(expected.get("index.json"));
  assertNoForbiddenKeys(index, "index");
  assertNoF1ContentId(index, "index");
  if (index.slots.length !== 12 || index.summary.newSourceAssets !== 8
      || index.summary.independentFallbackArtifacts !== 12 || index.previewReady !== false
      || index.status !== STATUS || index.integrationStatus !== INTEGRATION_STATUS) {
    fail("missing_asset_metadata", "index summary or maturity is invalid");
  }

  const files = await listFiles(PACKAGE_ROOT);
  const relativeFiles = files.map((path) => relative(PACKAGE_ROOT, path).split(sep).join("/")).sort();
  const expectedFiles = [...expected.keys(), ...EXPECTED_MANUAL_PATHS].sort();
  if (relativeFiles.join("\n") !== expectedFiles.join("\n")) {
    const extras = relativeFiles.filter((path) => !expectedFiles.includes(path));
    const missing = expectedFiles.filter((path) => !relativeFiles.includes(path));
    fail("unexpected_package_file", "file set mismatch; extras=" + extras.join(",") + "; missing=" + missing.join(","));
  }
  let packageBytes = 0;
  for (const path of files) {
    const extension = /\.[^.]+$/.exec(path)?.[0].toLowerCase() || "";
    if (FORBIDDEN_BINARY_EXTENSIONS.has(extension)) fail("unexpected_package_file", path + " is a forbidden binary/media type");
    packageBytes += (await stat(path)).size;
  }
  if (packageBytes >= 1_500_000) fail("web_budget_exceeded", "package is " + packageBytes + " bytes; must stay below 1.5 MiB token cap");

  return {
    ...metadataResult,
    generatedArtifacts: expected.size,
    generatedBytes,
    packageFiles: files.length,
    packageBytes,
    viewports: VIEWPORTS.length,
    actorThumbnailHeightPx: 128
  };
}

async function git(args) {
  try {
    const result = await execFileAsync("git", args, { cwd: PACKAGE_ROOT, encoding: "utf8", maxBuffer: 2 * 1024 * 1024 });
    return result.stdout;
  } catch (error) {
    fail("authoritative_root_unavailable", error.stderr || error.message);
  }
}

async function validateAuthoritativeRoot(root) {
  if (root !== AUTHORITATIVE_ROOT) {
    fail("authoritative_root_mismatch", "expected " + AUTHORITATIVE_ROOT + ", received " + root);
  }
  await git(["cat-file", "-e", root + "^{commit}"]);
  const definition = await git(["show", root + ":src/contracts/include/tgd/contracts/sandbox_definition.hpp"]);
  for (const fragment of [
    "player = 1", "actor = 2", "obstacle = 3", "interaction = 4",
    "mechanism = 5", "safe_point = 6", "effect = 7"
  ]) {
    if (!definition.includes(fragment)) fail("authoritative_root_mismatch", "SandboxAssetKind lacks " + fragment);
  }
  const pack = await git(["show", root + ":src/contracts/include/tgd/contracts/sandbox_pack.hpp"]);
  for (const diagnostic of [
    "missing_platform_variant", "missing_asset_metadata", "missing_asset_anchor",
    "color_only_distinction", "universal_placeholder_conflict", "license_blocked",
    "web_budget_exceeded", "invalid_stable_id"
  ]) {
    if (!pack.includes(diagnostic)) fail("authoritative_root_mismatch", "authoritative diagnostics lack " + diagnostic);
  }
  return { authoritativeRoot: root, sandboxAssetKinds: EXPECTED_STABLE_ASSET_KINDS.size, diagnosticsChecked: 8 };
}

export async function validatePackage({ authoritativeRoot }) {
  const contract = validateContractEntries(ASSETS);
  const artifacts = await validateGeneratedArtifacts();
  const authority = await validateAuthoritativeRoot(authoritativeRoot);
  return { contract, artifacts, authority };
}

function argument(name) {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : undefined;
}

const invokedPath = process.argv[1] ? resolve(process.argv[1]) : "";
if (invokedPath === fileURLToPath(import.meta.url)) {
  const authoritativeRoot = argument("--authoritative-root");
  validatePackage({ authoritativeRoot })
    .then((result) => {
      const summary = {
        task: "ART-002",
        status: "PASS",
        stableAssetSlots: result.contract.stableAssetSlots,
        exactKinds: result.authority.sandboxAssetKinds,
        newSourceAssets: result.contract.newSourceAssets,
        independentFallbacks: result.contract.independentFallbacks,
        metadataGroupsPerSlot: result.artifacts.metadataGroupsPerSlot,
        viewports: result.artifacts.viewports,
        actorThumbnailHeightPx: result.artifacts.actorThumbnailHeightPx,
        packageFiles: result.artifacts.packageFiles,
        packageBytes: result.artifacts.packageBytes,
        authoritativeRoot: result.authority.authoritativeRoot,
        maturity: "Blockout/not-integrated",
        previewReady: false
      };
      process.stdout.write(JSON.stringify(summary, null, 2) + "\n");
    })
    .catch((error) => {
      process.stderr.write(String(error.stack || error) + "\n");
      process.exitCode = 1;
    });
}
