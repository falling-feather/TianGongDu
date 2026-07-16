import { createHash } from "node:crypto";
import { createReadStream } from "node:fs";
import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { dirname, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

import {
  ASSETS,
  PACKAGE_ID
} from "./system-sandbox-blockout-spec.mjs";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const { PNG } = require("pngjs");
const playwrightPackage = require("playwright/package.json");
const pngjsPackage = require("pngjs/package.json");

export const TASK_ID = "ART-003";
export const GENERATOR_VERSION = "1.0.0";
export const ART_SOURCE_COMMIT = "b3dfd9e20f6651fc60abc22ef417878b5a9d70da";
export const ROOT_INTEGRATION_COMMIT = "24eb25df1b67ea210e09473e319dff6cd99580c1";
export const AUTHORITATIVE_ROOT = "cb7d5ac347e6581d187db1bfb2ff05e8997b97fd";
export const LOCKED_NODE_VERSION = "20.18.0";
export const LOCKED_PLAYWRIGHT_VERSION = "1.61.1";
export const LOCKED_PNGJS_VERSION = "7.0.0";
export const LOCKED_BROWSER = Object.freeze({
  name: "chromium",
  revision: "1223",
  version: "148.0.7778.96",
  executableSha256: "290fa7018fda22c52ada5eddb0113baf3ebc41fd0fde6085eddb19793606c635"
});
export const RUNTIME_SCHEMA_VERSION = "system-sandbox-runtime-import-1.0";
export const ROOT_ANCHOR_UV = Object.freeze({ x: 0.5, y: 0.125 });
export const RUNTIME_VIEW_BOX = Object.freeze({ x: 32, y: 26, width: 256, height: 256 });
export const QUALITY_TIERS = Object.freeze({
  standard: Object.freeze({ width: 256, height: 256, rootAnchorPx: Object.freeze({ x: 128, y: 32 }) }),
  low: Object.freeze({ width: 128, height: 128, rootAnchorPx: Object.freeze({ x: 64, y: 16 }) })
});

export const PACKAGE_ROOT = fileURLToPath(new URL("../", import.meta.url));
export const REPOSITORY_ROOT = resolve(PACKAGE_ROOT, "../..");
export const OUTPUT_ROOT = resolve(REPOSITORY_ROOT, "build/generated-assets/system-demo-blockouts");
export const MANIFEST_PATH = resolve(PACKAGE_ROOT, "runtime/runtime-import-manifest.json");

const STANDARD_TOTAL_LIMIT_BYTES = 512 * 1024;
const LOW_TOTAL_LIMIT_BYTES = 256 * 1024;
const MANIFEST_LIMIT_BYTES = 64 * 1024;
const STANDARD_ITEM_LIMIT_BYTES = 48 * 1024;
const LOW_ITEM_LIMIT_BYTES = 24 * 1024;
const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const RASTER_FLAGS = Object.freeze([
  "--disable-gpu",
  "--disable-lcd-text",
  "--disable-font-subpixel-positioning",
  "--force-color-profile=srgb",
  "--hide-scrollbars"
]);

function invariant(condition, message) {
  if (!condition) throw new Error(message);
}

export function sha256(value) {
  return createHash("sha256").update(value).digest("hex");
}

async function sha256File(path) {
  const hash = createHash("sha256");
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  return hash.digest("hex");
}

async function sha256PortableTextFile(path) {
  const source = await readFile(path, "utf8");
  return sha256(source.replaceAll("\r\n", "\n"));
}

export function stableContentKeyHex(name) {
  let hash = 14_695_981_039_346_656_037n;
  for (const byte of Buffer.from(name, "utf8")) {
    hash ^= BigInt(byte);
    hash = BigInt.asUintN(64, hash * 1_099_511_628_211n);
  }
  return hash.toString(16).padStart(16, "0");
}

function json(value) {
  return JSON.stringify(value, null, 2) + "\n";
}

function countOccurrences(value, token) {
  let count = 0;
  let offset = 0;
  while ((offset = value.indexOf(token, offset)) >= 0) {
    count += 1;
    offset += token.length;
  }
  return count;
}

export function extractStableAssetGroup(sourceSvg, stableAssetId) {
  const identity = `data-stable-asset-id="${stableAssetId}"`;
  invariant(countOccurrences(sourceSvg, identity) === 1, `${stableAssetId} must occur in exactly one SVG group`);
  const identityOffset = sourceSvg.indexOf(identity);
  const start = sourceSvg.lastIndexOf("<g ", identityOffset);
  invariant(start >= 0, `${stableAssetId} group opening tag is missing`);

  const tagPattern = /<\/?g\b[^>]*>/g;
  tagPattern.lastIndex = start;
  let depth = 0;
  let end = -1;
  for (let match = tagPattern.exec(sourceSvg); match; match = tagPattern.exec(sourceSvg)) {
    if (match.index === start || !match[0].startsWith("</")) depth += 1;
    else depth -= 1;
    if (depth === 0) {
      end = tagPattern.lastIndex;
      break;
    }
  }
  invariant(end > start, `${stableAssetId} group is not balanced`);
  const group = sourceSvg.slice(start, end);
  invariant(group.startsWith(`<g ${identity}`), `${stableAssetId} group identity must be the first attribute`);
  invariant(!/<text\b|data-anchor-mark=|<image\b|(?:xlink:)?href\s*=/i.test(group), `${stableAssetId} runtime group contains review or external content`);
  return group;
}

function extractStyle(sourceSvg) {
  const matches = [...sourceSvg.matchAll(/<style>[\s\S]*?<\/style>/g)];
  invariant(matches.length === 1, "fallback SVG must contain exactly one inline style block");
  return matches[0][0];
}

export function runtimeSvgFromFallback(sourceSvg, stableAssetId, size) {
  invariant(size === 256 || size === 128, `unsupported runtime raster size ${size}`);
  const group = extractStableAssetGroup(sourceSvg, stableAssetId);
  const style = extractStyle(sourceSvg);
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="${RUNTIME_VIEW_BOX.x} ${RUNTIME_VIEW_BOX.y} ${RUNTIME_VIEW_BOX.width} ${RUNTIME_VIEW_BOX.height}"`,
    ` data-runtime-stable-asset-id="${stableAssetId}" data-color-space="sRGB">`,
    style,
    group,
    "</svg>"
  ].join("");
}

const crcTable = (() => {
  const table = new Uint32Array(256);
  for (let index = 0; index < 256; index += 1) {
    let value = index;
    for (let bit = 0; bit < 8; bit += 1) {
      value = (value & 1) !== 0 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
    }
    table[index] = value >>> 0;
  }
  return table;
})();

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type, data) {
  const typeBytes = Buffer.from(type, "ascii");
  const output = Buffer.allocUnsafe(12 + data.length);
  output.writeUInt32BE(data.length, 0);
  typeBytes.copy(output, 4);
  data.copy(output, 8);
  output.writeUInt32BE(crc32(Buffer.concat([typeBytes, data])), 8 + data.length);
  return output;
}

function addSrgbChunk(png) {
  invariant(png.subarray(0, 8).equals(PNG_SIGNATURE), "canonical PNG signature is invalid");
  const ihdrLength = png.readUInt32BE(8);
  invariant(ihdrLength === 13 && png.toString("ascii", 12, 16) === "IHDR", "canonical PNG lacks IHDR");
  invariant(!png.includes(Buffer.from("sRGB", "ascii")), "canonical PNG unexpectedly already contains sRGB");
  const insertion = 8 + 12 + ihdrLength;
  return Buffer.concat([png.subarray(0, insertion), pngChunk("sRGB", Buffer.from([0])), png.subarray(insertion)]);
}

export function canonicalizePng(screenshot, width, height) {
  const decoded = PNG.sync.read(screenshot, { skipRescale: true });
  invariant(decoded.width === width && decoded.height === height, `browser raster was ${decoded.width}x${decoded.height}, expected ${width}x${height}`);
  const encoded = PNG.sync.write(
    { width, height, data: decoded.data },
    {
      bitDepth: 8,
      colorType: 6,
      inputColorType: 6,
      inputHasAlpha: true,
      deflateChunkSize: 32 * 1024,
      deflateLevel: 9,
      deflateStrategy: 3,
      filterType: 4
    }
  );
  return addSrgbChunk(encoded);
}

export function inspectRgbaPng(png) {
  const decoded = PNG.sync.read(png, { skipRescale: true });
  let transparentPixels = 0;
  let visiblePixels = 0;
  let chromaticPixels = 0;
  const silhouette = Buffer.allocUnsafe(decoded.width * decoded.height);
  for (let offset = 0, pixel = 0; offset < decoded.data.length; offset += 4, pixel += 1) {
    const red = decoded.data[offset];
    const green = decoded.data[offset + 1];
    const blue = decoded.data[offset + 2];
    const alpha = decoded.data[offset + 3];
    silhouette[pixel] = alpha;
    if (alpha === 0) transparentPixels += 1;
    else {
      visiblePixels += 1;
      if (Math.max(red, green, blue) - Math.min(red, green, blue) > 32) chromaticPixels += 1;
    }
  }
  return {
    width: decoded.width,
    height: decoded.height,
    transparentPixels,
    visiblePixels,
    chromaticPixels,
    silhouetteSha256: sha256(silhouette)
  };
}

async function rasterize(page, sourceSvg, stableAssetId, size) {
  await page.setViewportSize({ width: size + 32, height: size + 32 });
  await page.setContent(runtimeSvgFromFallback(sourceSvg, stableAssetId, size), { waitUntil: "load" });
  const svg = page.locator("svg");
  const screenshot = await svg.screenshot({
    animations: "disabled",
    caret: "hide",
    omitBackground: true,
    scale: "css",
    type: "png"
  });
  return canonicalizePng(screenshot, size, size);
}

function pinnedBrowserExecutablePath() {
  const override = process.env.SYSTEM_SANDBOX_CHROMIUM_EXECUTABLE;
  if (override) return resolve(override);
  invariant(process.platform === "win32", "the ART-003 pinned rasterizer currently requires Windows or an explicit executable override");
  invariant(process.env.LOCALAPPDATA, "LOCALAPPDATA is required to locate the pinned Playwright browser cache");
  return resolve(
    process.env.LOCALAPPDATA,
    "ms-playwright",
    `chromium-${LOCKED_BROWSER.revision}`,
    "chrome-win64",
    "chrome.exe"
  );
}

async function renderPass(executablePath, executableSha256, generatorSourceSha256) {
  const browser = await chromium.launch({ executablePath, headless: true, args: [...RASTER_FLAGS] });
  try {
    const context = await browser.newContext({
      viewport: { width: 320, height: 320 },
      deviceScaleFactor: 1,
      colorScheme: "light",
      reducedMotion: "reduce",
      locale: "en-US"
    });
    const page = await context.newPage();
    const artifacts = new Map();
    const sources = new Map();

    for (const asset of ASSETS) {
      const sourcePath = `fallbacks/${asset.slug}.svg`;
      const sourceSvg = await readFile(resolve(PACKAGE_ROOT, sourcePath), "utf8");
      const group = extractStableAssetGroup(sourceSvg, asset.stableAssetId);
      sources.set(asset.slug, {
        path: sourcePath,
        sha256: sha256(sourceSvg),
        bytes: Buffer.byteLength(sourceSvg, "utf8"),
        groupSelector: `g[data-stable-asset-id="${asset.stableAssetId}"]`,
        groupSha256: sha256(group)
      });
      for (const [quality, dimensions] of Object.entries(QUALITY_TIERS)) {
        const png = await rasterize(page, sourceSvg, asset.stableAssetId, dimensions.width);
        const inspection = inspectRgbaPng(png);
        artifacts.set(`${asset.slug}/${quality}`, { png, inspection });
      }
    }
    await context.close();
    return {
      artifacts,
      sources,
      generatorIdentity: {
        generatorVersion: GENERATOR_VERSION,
        generatorSourceSha256,
        nodeVersion: process.version.slice(1),
        playwrightVersion: playwrightPackage.version,
        pngjsVersion: pngjsPackage.version,
        browserName: LOCKED_BROWSER.name,
        browserRevision: LOCKED_BROWSER.revision,
        browserVersion: browser.version(),
        browserExecutableSha256: executableSha256,
        deviceScaleFactor: 1,
        rasterFlags: [...RASTER_FLAGS],
        canonicalPng: "pngjs-rgba8-deflate9-filter4-srgb-intent0"
      }
    };
  } finally {
    await browser.close();
  }
}

function comparePasses(first, second) {
  invariant(JSON.stringify(first.generatorIdentity) === JSON.stringify(second.generatorIdentity), "generator identity drifted between clean passes");
  invariant(first.artifacts.size === 24 && second.artifacts.size === 24, "each clean pass must produce 24 artifacts");
  for (const [key, firstArtifact] of first.artifacts) {
    const secondArtifact = second.artifacts.get(key);
    invariant(secondArtifact, `second clean pass omitted ${key}`);
    invariant(firstArtifact.png.equals(secondArtifact.png), `${key} bytes drifted between clean passes`);
  }
}

function artifactRecord(asset, quality, generated) {
  const dimensions = QUALITY_TIERS[quality];
  const { png, inspection } = generated;
  return {
    artifactId: `artifact.system_demo.${asset.slug}.${quality}`,
    path: `${quality}/${asset.slug}.png`,
    format: "PNG",
    pixelFormat: "RGBA8",
    colorSpace: "sRGB",
    alphaMode: "straight-source-premultiply-on-upload",
    filter: "linear",
    wrap: "clamp",
    mipmaps: false,
    width: dimensions.width,
    height: dimensions.height,
    rootAnchorPx: dimensions.rootAnchorPx,
    sha256: sha256(png),
    fileBytes: png.length,
    decodedBytes: dimensions.width * dimensions.height * 4,
    transparentPixels: inspection.transparentPixels,
    visiblePixels: inspection.visiblePixels,
    grayscaleSilhouetteSha256: inspection.silhouetteSha256
  };
}

export function buildRuntimeManifest(pass) {
  const entries = ASSETS.map((asset) => {
    const standard = artifactRecord(asset, "standard", pass.artifacts.get(`${asset.slug}/standard`));
    const low = artifactRecord(asset, "low", pass.artifacts.get(`${asset.slug}/low`));
    return {
      stableAssetId: asset.stableAssetId,
      stableContentKeyHex: stableContentKeyHex(asset.stableAssetId),
      sandboxAssetKind: asset.sandboxAssetKind,
      slug: asset.slug,
      owner: "ART",
      package: PACKAGE_ID,
      status: "Blockout",
      integrationStatus: "not-integrated",
      previewReady: false,
      releaseAllowed: false,
      allowedChannel: "internal-preview",
      license: "review-recorded-not-release-cleared",
      runtimeBuildStatus: "not-produced",
      platformPreviewStatus: { windows: "not-validated", web: "not-validated" },
      sourceMode: asset.sourceMode,
      reuseLineage: asset.reuseLineage ?? null,
      source: pass.sources.get(asset.slug),
      rootAnchorUv: ROOT_ANCHOR_UV,
      axes: asset.axes,
      unit: asset.unit,
      facingConvention: asset.facingConvention,
      visualBoundsMm: asset.visualBoundsMm ?? null,
      nominalPresentationExtentMm: asset.nominalPresentationExtentMm ?? null,
      anchors: asset.anchors,
      artifacts: { standard, low },
      platformQuality: {
        windows: { standard: standard.artifactId, low: low.artifactId },
        web: { standard: standard.artifactId, low: low.artifactId }
      },
      accessibility: {
        colorOnly: false,
        audioOnly: false,
        shapeToken: asset.shapeToken,
        nonColorChannels: asset.nonColorChannels
      },
      budget: {
        standardMaxFileBytes: STANDARD_ITEM_LIMIT_BYTES,
        lowMaxFileBytes: LOW_ITEM_LIMIT_BYTES,
        targetTransportKiB: asset.budget.targetTransportKiB,
        targetGpuTextureMiB: asset.budget.targetGpuTextureMiB,
        targetDrawCalls: asset.budget.targetDrawCalls,
        peakTransparentCoveragePercent: asset.budget.peakTransparentCoveragePercent,
        peakLayerCount: asset.budget.peakLayerCount ?? 1
      },
      dependencies: asset.dependencies
    };
  });

  const standardTransferBytes = entries.reduce((sum, entry) => sum + entry.artifacts.standard.fileBytes, 0);
  const lowTransferBytes = entries.reduce((sum, entry) => sum + entry.artifacts.low.fileBytes, 0);
  const standardDecodedBytes = entries.reduce((sum, entry) => sum + entry.artifacts.standard.decodedBytes, 0);
  const lowDecodedBytes = entries.reduce((sum, entry) => sum + entry.artifacts.low.decodedBytes, 0);

  return {
    schemaVersion: RUNTIME_SCHEMA_VERSION,
    taskId: TASK_ID,
    artSourceCommit: ART_SOURCE_COMMIT,
    rootIntegrationCommit: ROOT_INTEGRATION_COMMIT,
    authoritativeRoot: AUTHORITATIVE_ROOT,
    package: PACKAGE_ID,
    status: "Blockout",
    integrationStatus: "not-integrated",
    previewReady: false,
    releaseAllowed: false,
    allowedChannel: "internal-preview",
    license: "review-recorded-not-release-cleared",
    runtimeBuildStatus: "not-produced",
    platformPreviewStatus: { windows: "not-validated", web: "not-validated" },
    sourcePackageRoot: "assets_src/system-sandbox-blockouts",
    generatedArtifactRoot: "build/generated-assets/system-demo-blockouts",
    generatedArtifactsTrackedByGit: false,
    lookupContract: {
      key: ["stableAssetId", "sandboxAssetKind"],
      verifyStableContentKey: true,
      pathInferenceAllowed: false,
      unknownId: "fail-closed",
      wrongKind: "fail-closed",
      missingArtifact: "fail-closed",
      partialTableExposure: false,
      standardFallback: "same-entry-explicit-low-only"
    },
    generatorIdentity: pass.generatorIdentity,
    limits: {
      entries: 12,
      artifacts: 24,
      maximumUtf8StringBytes: 256,
      standardTransferBytes: STANDARD_TOTAL_LIMIT_BYTES,
      lowTransferBytes: LOW_TOTAL_LIMIT_BYTES,
      manifestBytes: MANIFEST_LIMIT_BYTES,
      standardDecodedBytes: 3 * 1024 * 1024,
      lowDecodedBytes: 768 * 1024
    },
    measured: {
      entries: entries.length,
      artifacts: entries.length * 2,
      standardTransferBytes,
      lowTransferBytes,
      standardDecodedBytes,
      lowDecodedBytes
    },
    entries
  };
}

function outputMap(pass, manifest) {
  const outputs = new Map();
  for (const entry of manifest.entries) {
    for (const quality of ["standard", "low"]) {
      outputs.set(entry.artifacts[quality].path, pass.artifacts.get(`${entry.slug}/${quality}`).png);
    }
  }
  return outputs;
}

function assertEnvironment() {
  invariant(process.version === `v${LOCKED_NODE_VERSION}`, `Node ${process.version.slice(1)} is not locked ${LOCKED_NODE_VERSION}`);
  invariant(playwrightPackage.version === LOCKED_PLAYWRIGHT_VERSION, `Playwright ${playwrightPackage.version} is not locked ${LOCKED_PLAYWRIGHT_VERSION}`);
  invariant(pngjsPackage.version === LOCKED_PNGJS_VERSION, `pngjs ${pngjsPackage.version} is not locked ${LOCKED_PNGJS_VERSION}`);
}

function assertSafeOutputRoot() {
  const normalized = relative(REPOSITORY_ROOT, OUTPUT_ROOT).split(sep).join("/");
  invariant(normalized === "build/generated-assets/system-demo-blockouts", `unsafe generated output root ${normalized}`);
}

async function writePackage(pass, manifest) {
  assertSafeOutputRoot();
  await rm(OUTPUT_ROOT, { recursive: true, force: true });
  for (const [path, content] of outputMap(pass, manifest)) {
    const absolute = resolve(OUTPUT_ROOT, path);
    const normalized = relative(OUTPUT_ROOT, absolute).split(sep).join("/");
    invariant(!normalized.startsWith("../") && normalized === path, `unsafe artifact path ${path}`);
    await mkdir(dirname(absolute), { recursive: true });
    await writeFile(absolute, content);
  }
  await mkdir(dirname(MANIFEST_PATH), { recursive: true });
  await writeFile(MANIFEST_PATH, json(manifest), "utf8");
}

async function checkPackage(pass, manifest) {
  const expectedManifest = json(manifest);
  const actualManifest = await readFile(MANIFEST_PATH, "utf8").catch(() => null);
  invariant(actualManifest === expectedManifest, "runtime import manifest is missing or stale");
  for (const [path, expected] of outputMap(pass, manifest)) {
    const actual = await readFile(resolve(OUTPUT_ROOT, path)).catch(() => null);
    invariant(actual && actual.equals(expected), `${path} is missing or stale`);
  }
}

export async function generateRuntimeImportPackage({ verifyDeterminism = false, checkOnly = false } = {}) {
  assertEnvironment();
  const executablePath = pinnedBrowserExecutablePath();
  invariant(executablePath && !executablePath.includes("\n"), "pinned Chromium executable is unavailable");
  const executableSha256 = await sha256File(executablePath).catch(() => null);
  invariant(executableSha256 === LOCKED_BROWSER.executableSha256, "pinned Chromium executable is missing or its SHA-256 drifted");
  const generatorSourceSha256 = await sha256PortableTextFile(fileURLToPath(import.meta.url));
  const first = await renderPass(executablePath, executableSha256, generatorSourceSha256);
  invariant(first.generatorIdentity.browserRevision === LOCKED_BROWSER.revision, "pinned Chromium revision drifted");
  invariant(first.generatorIdentity.browserVersion === LOCKED_BROWSER.version, "pinned Chromium version drifted");
  let selected = first;
  if (verifyDeterminism || checkOnly) {
    const second = await renderPass(executablePath, executableSha256, generatorSourceSha256);
    comparePasses(first, second);
    selected = second;
  }
  const manifest = buildRuntimeManifest(selected);
  const manifestBytes = Buffer.byteLength(json(manifest), "utf8");
  invariant(manifest.measured.standardTransferBytes <= STANDARD_TOTAL_LIMIT_BYTES, "Standard transfer budget exceeded");
  invariant(manifest.measured.lowTransferBytes <= LOW_TOTAL_LIMIT_BYTES, "Low transfer budget exceeded");
  invariant(manifest.measured.standardDecodedBytes <= manifest.limits.standardDecodedBytes, "Standard decoded budget exceeded");
  invariant(manifest.measured.lowDecodedBytes <= manifest.limits.lowDecodedBytes, "Low decoded budget exceeded");
  invariant(manifestBytes <= MANIFEST_LIMIT_BYTES, "manifest budget exceeded");
  if (checkOnly) await checkPackage(selected, manifest);
  else await writePackage(selected, manifest);
  return {
    entries: manifest.entries.length,
    artifacts: manifest.measured.artifacts,
    standardTransferBytes: manifest.measured.standardTransferBytes,
    lowTransferBytes: manifest.measured.lowTransferBytes,
    standardDecodedBytes: manifest.measured.standardDecodedBytes,
    lowDecodedBytes: manifest.measured.lowDecodedBytes,
    manifestBytes,
    generatorIdentity: manifest.generatorIdentity,
    deterministicPasses: verifyDeterminism || checkOnly ? 2 : 1
  };
}

const invokedPath = process.argv[1] ? resolve(process.argv[1]) : "";
if (invokedPath === fileURLToPath(import.meta.url)) {
  const checkOnly = process.argv.includes("--check");
  const verifyDeterminism = process.argv.includes("--verify-determinism");
  generateRuntimeImportPackage({ verifyDeterminism, checkOnly })
    .then((summary) => process.stdout.write(JSON.stringify({ task: TASK_ID, status: checkOnly ? "CHECKED" : "GENERATED", ...summary }, null, 2) + "\n"))
    .catch((error) => {
      process.stderr.write(String(error.stack || error) + "\n");
      process.exitCode = 1;
    });
}
