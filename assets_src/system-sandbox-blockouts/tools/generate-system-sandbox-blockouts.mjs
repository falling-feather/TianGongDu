import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
  ASSETS,
  AUTHORING_BASE,
  AUTHORITATIVE_ROOT,
  INTEGRATION_STATUS,
  LICENSE_STATUS,
  LINEAGE,
  NEW_SOURCE_ASSETS,
  PACKAGE_ID,
  STATUS,
  TASK_ID,
  VIEWPORTS
} from "./system-sandbox-blockout-spec.mjs";

export const PACKAGE_ROOT = fileURLToPath(new URL("../", import.meta.url));

const palette = Object.freeze({
  background: "#f1eee5",
  panel: "#dedbd2",
  ink: "#101820",
  mid: "#687272",
  light: "#d7d0bb",
  paper: "#f7f3e8",
  guide: "#8b928d"
});

function sha256(value) {
  return createHash("sha256").update(value, "utf8").digest("hex");
}

function bytes(value) {
  return Buffer.byteLength(value, "utf8");
}

function xml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function json(value) {
  return JSON.stringify(value, null, 2) + "\n";
}

function svgOpen(width, height, extra) {
  return [
    '<svg xmlns="http://www.w3.org/2000/svg" width="', width,
    '" height="', height,
    '" viewBox="0 0 ', width, " ", height,
    '" role="img" ', extra || "", ">\n",
    "<style>",
    ".outline{stroke:", palette.ink, ";stroke-width:7;stroke-linejoin:round;stroke-linecap:round}",
    ".thin{stroke:", palette.ink, ";stroke-width:4;stroke-linejoin:round;stroke-linecap:round}",
    ".guide{stroke:", palette.guide, ";stroke-width:3;fill:none;stroke-dasharray:9 7}",
    ".label{font-family:Arial,sans-serif;font-size:18px;fill:", palette.ink, "}",
    ".small{font-family:Arial,sans-serif;font-size:13px;fill:", palette.ink, "}",
    "</style>\n"
  ].join("");
}

function anchorMark(x, y) {
  return [
    '<g data-anchor-mark="true" transform="translate(', x, " ", y, ')">',
    '<circle r="9" fill="none" class="thin"/>',
    '<path d="M-14 0H14M0-14V14" class="thin" fill="none"/>',
    "</g>"
  ].join("");
}

function shapeMarkup(asset, x, y, scale, phase = 1) {
  const open = [
    '<g data-stable-asset-id="', xml(asset.stableAssetId),
    '" data-shape-token="', xml(asset.shapeToken),
    '" transform="translate(', x, " ", y, ") scale(", scale, ')">'
  ].join("");
  let body = "";

  switch (asset.slug) {
    case "player":
      body = [
        '<path d="M-28-132H24L44-14H-42Z" fill="', palette.light, '" class="outline"/>',
        '<circle cx="-2" cy="-154" r="20" fill="', palette.paper, '" class="outline"/>',
        '<path d="M-54-112L58-22" class="outline" fill="none"/>',
        '<path d="M-30-82L24-55" class="thin" fill="none"/>',
        '<circle cx="-49" cy="-76" r="9" fill="', palette.ink, '"/>',
        '<path d="M-22-14L-30 0M22-14L30 0" class="outline" fill="none"/>'
      ].join("");
      break;
    case "enemy-pressure":
      body = [
        '<path d="M-78-102L-14-142L78-96L48-42L-62-38Z" fill="', palette.light, '" class="outline"/>',
        '<path d="M-17-135L8-105L-10-82L20-52" class="thin" fill="none"/>',
        '<path d="M-55-43L-82 0M-18-38L-30 3M18-39L29 3M50-43L82 0" class="outline" fill="none"/>',
        '<path d="M-58-82L45-70" class="thin" fill="none"/>'
      ].join("");
      break;
    case "enemy-flanker":
      body = [
        '<ellipse cx="0" cy="0" rx="62" ry="17" fill="none" class="guide"/>',
        '<path d="M-18-54L-25-122L2-150L24-112L17-56Z" fill="', palette.mid, '" class="outline"/>',
        '<path d="M-16-128L-104-94L-55-58L-8-83M18-116L116-82L48-42L10-77" fill="', palette.light, '" class="outline"/>',
        '<path d="M-95-91L-55-78L-76-62M108-80L62-66L82-48" class="thin" fill="none"/>',
        '<path d="M0-54V-8" class="thin" fill="none"/>'
      ].join("");
      break;
    case "enemy-elite":
      body = [
        '<path d="M-92-150L-12-183L-3-125L-82-102Z" fill="', palette.light, '" class="outline"/>',
        '<path d="M12-183L94-149L82-101L3-125Z" fill="', palette.light, '" class="outline"/>',
        '<rect x="-20" y="-126" width="40" height="102" fill="', palette.mid, '" class="outline"/>',
        '<path d="M-66-100L-48-24M66-100L48-24M0-25V0" class="outline" fill="none"/>',
        '<rect x="-70" y="-55" width="24" height="34" fill="', palette.ink, '"/>',
        '<rect x="-12" y="-48" width="24" height="40" fill="', palette.ink, '"/>',
        '<rect x="46" y="-55" width="24" height="34" fill="', palette.ink, '"/>',
        '<path d="M-38-24L-58 0M38-24L58 0" class="outline" fill="none"/>'
      ].join("");
      break;
    case "obstacle-tension-gate":
      body = [
        '<rect x="-42" y="-170" width="84" height="170" fill="', palette.mid, '" class="outline"/>',
        '<path d="M-35-155L35-18M35-155L-35-18" class="outline" fill="none"/>',
        '<rect x="-55" y="-15" width="110" height="15" fill="', palette.ink, '"/>',
        '<path d="M-58-126H58M-58-58H58" class="thin" fill="none"/>'
      ].join("");
      break;
    case "interaction-console":
      body = [
        '<path d="M-60-105L-38-132H58L72-112V0H-60Z" fill="', palette.light, '" class="outline"/>',
        '<rect x="-28" y="-94" width="56" height="28" rx="6" fill="', palette.ink, '"/>',
        '<path d="M-18-48H20M28-55L45-85" class="outline" fill="none"/>',
        '<circle cx="45" cy="-85" r="8" fill="', palette.paper, '" class="thin"/>'
      ].join("");
      break;
    case "mechanism-gate": {
      const offset = phase === 0 ? 18 : phase === 2 ? -22 : 0;
      body = [
        '<rect x="-65" y="-155" width="28" height="155" fill="', palette.mid, '" class="outline"/>',
        '<rect x="38" y="-130" width="24" height="130" fill="', palette.mid, '" class="outline"/>',
        '<path d="M-38-125L38-52" class="outline" fill="none"/>',
        '<path d="M-20-142L48-100" class="thin" fill="none"/>',
        '<rect x="24" y="', -82 + offset, '" width="40" height="44" fill="', palette.ink, '"/>',
        '<circle cx="-38" cy="-126" r="12" fill="', palette.paper, '" class="thin"/>'
      ].join("");
      break;
    }
    case "safe-point-lamp-shelter":
      body = [
        '<ellipse cx="0" cy="0" rx="82" ry="28" fill="none" class="outline"/>',
        '<ellipse cx="0" cy="0" rx="58" ry="19" fill="none" class="thin"/>',
        '<path d="M-60-4L-32-118M60-4L32-118M0-15V-135" class="outline" fill="none"/>',
        '<path d="M-42-116Q0-154 42-116" fill="', palette.light, '" class="outline"/>',
        '<path d="M0-152L14-132L0-112L-14-132Z" fill="', palette.paper, '" class="outline"/>',
        '<path d="M0-24V-2M-12-12L0-2L12-12" class="thin" fill="none"/>'
      ].join("");
      break;
    case "skill-eavesguard-telegraph":
      body = [
        '<path d="M0 0L-95-65L-72-104L0-42L72-104L95-65Z" fill="', palette.light, '" class="outline"/>',
        '<path d="M0-6L-62-62M0-6V-72M0-6L62-62" class="thin" fill="none"/>',
        '<path d="M-83-82L-65-76M83-82L65-76" class="guide"/>'
      ].join("");
      break;
    case "skill-eavesguard-hit":
      body = [
        '<rect x="-74" y="-112" width="148" height="24" fill="', palette.ink, '"/>',
        '<rect x="-58" y="-78" width="116" height="22" fill="', palette.mid, '" class="thin"/>',
        '<rect x="-40" y="-45" width="80" height="20" fill="', palette.light, '" class="thin"/>',
        '<path d="M0-126V-4M-18-20L0-4L18-20" class="outline" fill="none"/>'
      ].join("");
      break;
    case "skill-flower-turn-telegraph":
      body = [
        '<path d="M-100-18Q0-124 100-18" class="outline" fill="none" stroke-dasharray="28 14"/>',
        '<path d="M-78-4Q0-86 78-4" class="thin" fill="none" stroke-dasharray="18 12"/>',
        '<path d="M-72-92L75-5" class="outline" fill="none"/>',
        '<path d="M-88-30L-102-12M82-36L103-20" class="thin" fill="none"/>'
      ].join("");
      break;
    case "skill-flower-turn-hit":
      body = [
        '<path d="M-92-108L-12-58L-50-8L-108-54Z" fill="', palette.mid, '" class="outline"/>',
        '<path d="M92-108L12-58L50-8L108-54Z" fill="', palette.light, '" class="outline"/>',
        '<path d="M-18-64L0-86L18-64L0-42Z" fill="', palette.paper, '" class="thin"/>',
        '<path d="M-106-20L-52-42M106-20L52-42" class="guide"/>'
      ].join("");
      break;
    default:
      throw new Error("Unknown shape slug " + asset.slug);
  }

  return open + body + "</g>";
}

function fallbackSvg(asset) {
  return [
    svgOpen(320, 320, 'data-artifact-kind="stable-id-specific-fallback"'),
    '<rect width="320" height="320" fill="', palette.background, '"/>',
    '<rect x="12" y="12" width="296" height="296" rx="12" fill="none" class="thin"/>',
    '<text x="160" y="34" text-anchor="middle" class="small">', xml(asset.displayName), "</text>",
    shapeMarkup(asset, 160, 250, 1, 1),
    anchorMark(160, 250),
    '<text x="160" y="300" text-anchor="middle" class="small">', xml(asset.fallbackId), "</text>",
    "</svg>\n"
  ].join("");
}

function sourceSvg(asset) {
  const dimension = asset.visualBoundsMm
    ? JSON.stringify(asset.visualBoundsMm)
    : JSON.stringify(asset.nominalPresentationExtentMm);
  const panels = [0, 1, 2].map((phase) => {
    const x = 180 + phase * 300;
    const state = asset.states[Math.min(phase, asset.states.length - 1)];
    return [
      '<rect x="', x - 125, '" y="120" width="250" height="390" rx="10" fill="', palette.panel, '" class="thin"/>',
      '<text x="', x, '" y="150" text-anchor="middle" class="label">', xml(state), "</text>",
      shapeMarkup(asset, x, 430, 1.35, phase),
      anchorMark(x, 430)
    ].join("");
  }).join("");
  const anchorNames = asset.anchors.map((entry) => entry.name).join(" / ");
  return [
    svgOpen(960, 640, 'data-artifact-kind="deterministic-review-source"'),
    '<rect width="960" height="640" fill="', palette.background, '"/>',
    '<text x="48" y="52" class="label">', xml(asset.displayName), "</text>",
    '<text x="48" y="82" class="small">', xml(asset.stableAssetId), "</text>",
    panels,
    '<text x="48" y="550" class="small">nominal visual dimensions: ', xml(dimension), " mm</text>",
    '<text x="48" y="576" class="small">anchors: ', xml(anchorNames), "</text>",
    '<text x="48" y="602" class="small">review source only / Blockout / not-integrated</text>',
    "</svg>\n"
  ].join("");
}

function previewSvg(viewport) {
  const cols = 4;
  const rows = 3;
  const top = Math.round(viewport.height * 0.10);
  const cellWidth = viewport.width / cols;
  const cellHeight = (viewport.height - top) / rows;
  const scale = Math.min(viewport.width / 1280, viewport.height / 720);
  const groups = ASSETS.map((asset, index) => {
    const col = index % cols;
    const row = Math.floor(index / cols);
    const x = cellWidth * col + cellWidth / 2;
    const y = top + cellHeight * (row + 1) - 34 * scale;
    const shapeScale = row === 0 ? 0.76 * scale : 0.66 * scale;
    return [
      '<g data-preview-slot="', xml(asset.stableAssetId), '">',
      '<rect x="', Math.round(cellWidth * col + 8 * scale), '" y="', Math.round(top + cellHeight * row + 6 * scale),
      '" width="', Math.round(cellWidth - 16 * scale), '" height="', Math.round(cellHeight - 12 * scale),
      '" rx="', Math.round(8 * scale), '" fill="', row % 2 === 0 ? palette.panel : palette.paper, '" class="thin"/>',
      shapeMarkup(asset, x, y, shapeScale, 1),
      anchorMark(x, y),
      '<text x="', x, '" y="', Math.round(top + cellHeight * row + 28 * scale),
      '" text-anchor="middle" class="small">', xml(asset.displayName), "</text>",
      "</g>"
    ].join("");
  }).join("");
  return [
    svgOpen(viewport.width, viewport.height, 'data-artifact-kind="grayscale-readability-board" data-thumbnail-height="128"'),
    '<rect width="', viewport.width, '" height="', viewport.height, '" fill="', palette.background, '"/>',
    '<text x="24" y="32" class="label">System Sandbox Blockouts / ', viewport.id, " / grayscale</text>",
    '<text x="24" y="58" class="small">128px actor thumbnail comparison: player / pressure / flanker / elite</text>',
    groups,
    "</svg>\n"
  ].join("");
}

function actorThumbnailSvg() {
  const actors = ASSETS.slice(0, 4);
  const groups = actors.map((asset, index) => {
    const x = index * 128 + 64;
    return [
      '<g data-thumbnail-slot="', xml(asset.stableAssetId), '">',
      '<rect x="', index * 128, '" width="128" height="128" fill="',
      index % 2 === 0 ? palette.panel : palette.paper, '"/>',
      shapeMarkup(asset, x, 112, 0.48, 1),
      anchorMark(x, 112),
      '</g>'
    ].join("");
  }).join("");
  return [
    svgOpen(512, 128, 'data-artifact-kind="grayscale-actor-thumbnail-strip" data-thumbnail-height="128"'),
    groups,
    "</svg>\n"
  ].join("");
}

function metadata(asset, fallbackContent, sourceContent) {
  const fallbackPath = "fallbacks/" + asset.slug + ".svg";
  const sourcePath = sourceContent ? "assets/" + asset.slug + "/source.svg" : null;
  const source = sourceContent
    ? {
        mode: "new-source",
        file: sourcePath,
        kind: "deterministic-project-svg",
        generationMethod: "project-local deterministic vector generation with no external image input",
        externalInputs: [],
        aiAssisted: true,
        sha256: sha256(sourceContent),
        bytes: bytes(sourceContent)
      }
    : {
        mode: "project-grammar-reuse",
        file: null,
        kind: "self-contained-new-fallback-from-project-grammar",
        generationMethod: asset.reuseLineage,
        externalInputs: [],
        aiAssisted: true,
        lineage: LINEAGE
      };

  return {
    schemaVersion: "system-sandbox-blockout-metadata-1.0",
    taskId: TASK_ID,
    authoringBase: AUTHORING_BASE,
    authoritativeRoot: AUTHORITATIVE_ROOT,
    stableAssetId: asset.stableAssetId,
    sandboxAssetKind: asset.sandboxAssetKind,
    displayName: asset.displayName,
    owner: asset.owner,
    package: asset.package,
    status: asset.status,
    integrationStatus: asset.integrationStatus,
    fallbackIdentity: {
      id: asset.fallbackId,
      exportName: asset.slug,
      artifactPath: fallbackPath,
      independentPerStableAsset: true,
      sha256: sha256(fallbackContent),
      bytes: bytes(fallbackContent)
    },
    source,
    import: {
      status: "planned",
      artifact: "not-produced",
      validation: "not-validated",
      unit: asset.unit,
      axes: asset.axes,
      facingConvention: asset.facingConvention,
      canonicalRoot: "root_visual",
      anchors: asset.anchors,
      visualBoundsMm: asset.visualBoundsMm || null,
      nominalPresentationExtentMm: asset.nominalPresentationExtentMm || null,
      trimPolicy: "preserve root_visual, every named anchor, silhouette breaks, and fallback export identity"
    },
    license: {
      status: LICENSE_STATUS,
      rightsHolder: "TianGongDu project",
      externalAssetLicenses: [],
      aiGenerationDisclosure: "Codex-assisted deterministic SVG using project-owned visual grammar; no external image input",
      permittedUse: "internal system Demo evaluation and production planning",
      releaseGate: "rights-holder sign-off, similarity review, runtime artifact report, and package policy confirmation"
    },
    runtime: {
      status: "planned",
      artifact: "not-produced",
      validation: "not-validated",
      integrationStatus: INTEGRATION_STATUS,
      authorityBoundary: "Presentation-only output; all gameplay state and spatial truth remain external",
      stableAssetIdChangesByPlatform: false,
      platformVariants: asset.platformPlan,
      fallbackWhenMissing: asset.fallbackId
    },
    preview: {
      status: "planned",
      runtimeCapture: "not-produced",
      validation: "not-validated",
      artifactType: "review-board-only",
      reviewBoards: VIEWPORTS.map((entry) => "previews/readability-" + entry.id + ".svg"),
      actorThumbnailHeightPx: ["player", "enemy-pressure", "enemy-flanker", "enemy-elite"].includes(asset.slug) ? 128 : null,
      requiredChecks: ["grayscale silhouette", "anchor marker", "non-color distinction", "no platform integration claim"]
    },
    budget: {
      runtimeTargetsAreEstimates: true,
      measuredSourceBytes: sourceContent ? bytes(sourceContent) : 0,
      measuredFallbackBytes: bytes(fallbackContent),
      commitItemLimitBytes: 32 * 1024,
      packageCommitLimitBytes: 1_500_000,
      ...asset.budget
    },
    states: asset.states,
    accessibility: {
      colorOnly: false,
      audioOnly: false,
      shapeToken: asset.shapeToken,
      nonColorChannels: asset.nonColorChannels
    },
    dependencies: asset.dependencies
  };
}

export function buildGeneratedOutputs() {
  const outputs = new Map();
  const fallbackBySlug = new Map();
  const sourceBySlug = new Map();

  for (const asset of ASSETS) {
    const content = fallbackSvg(asset);
    fallbackBySlug.set(asset.slug, content);
    outputs.set("fallbacks/" + asset.slug + ".svg", content);
  }

  for (const asset of NEW_SOURCE_ASSETS) {
    const content = sourceSvg(asset);
    sourceBySlug.set(asset.slug, content);
    outputs.set("assets/" + asset.slug + "/source.svg", content);
  }

  const metadataBySlug = new Map();
  for (const asset of ASSETS) {
    const document = metadata(asset, fallbackBySlug.get(asset.slug), sourceBySlug.get(asset.slug));
    metadataBySlug.set(asset.slug, document);
    outputs.set("assets/" + asset.slug + "/asset-metadata.json", json(document));
  }

  const previewDocuments = new Map();
  for (const viewport of VIEWPORTS) {
    const content = previewSvg(viewport);
    const path = "previews/readability-" + viewport.id + ".svg";
    previewDocuments.set(viewport.id, { viewport, path, content });
    outputs.set(path, content);
  }

  const actorThumbnailPath = "previews/actor-thumbnails-128.svg";
  const actorThumbnailContent = actorThumbnailSvg();
  outputs.set(actorThumbnailPath, actorThumbnailContent);

  const index = {
    schemaVersion: "system-sandbox-blockout-index-1.0",
    taskId: TASK_ID,
    authoringBase: AUTHORING_BASE,
    authoritativeRoot: AUTHORITATIVE_ROOT,
    lineage: LINEAGE,
    package: PACKAGE_ID,
    status: STATUS,
    integrationStatus: INTEGRATION_STATUS,
    previewReady: false,
    slots: ASSETS.map((asset) => ({
      stableAssetId: asset.stableAssetId,
      sandboxAssetKind: asset.sandboxAssetKind,
      status: asset.status,
      integrationStatus: asset.integrationStatus,
      sourceMode: asset.sourceMode,
      source: asset.sourceMode === "new-source" ? "assets/" + asset.slug + "/source.svg" : null,
      metadata: "assets/" + asset.slug + "/asset-metadata.json",
      fallbackArtifact: "fallbacks/" + asset.slug + ".svg",
      fallbackId: asset.fallbackId,
      shapeToken: asset.shapeToken
    })),
    summary: {
      stableAssetSlots: ASSETS.length,
      newSourceAssets: NEW_SOURCE_ASSETS.length,
      independentFallbackArtifacts: ASSETS.length,
      allSlotsTargetTransportKiB: ASSETS.reduce((sum, entry) => sum + entry.budget.targetTransportKiB, 0),
      newSourcesTargetTransportKiB: NEW_SOURCE_ASSETS.reduce((sum, entry) => sum + entry.budget.targetTransportKiB, 0)
    }
  };
  outputs.set("index.json", json(index));

  const previewEvidence = {
    schemaVersion: "system-sandbox-blockout-preview-evidence-1.0",
    taskId: TASK_ID,
    status: STATUS,
    integrationStatus: INTEGRATION_STATUS,
    runtimeCapture: "not-produced",
    platformValidation: "not-validated",
    actualHumanEvidence: false,
    actorThumbnailCheck: {
      heightPx: 128,
      grayscale: true,
      path: actorThumbnailPath,
      sha256: sha256(actorThumbnailContent),
      bytes: bytes(actorThumbnailContent),
      stableAssetIds: ASSETS.slice(0, 4).map((entry) => entry.stableAssetId)
    },
    viewports: [...previewDocuments.values()].map(({ viewport, path, content }) => ({
      id: viewport.id,
      width: viewport.width,
      height: viewport.height,
      path,
      sha256: sha256(content),
      bytes: bytes(content)
    })),
    checks: [
      "twelve distinct shape tokens",
      "twelve stable-ID-specific fallback artifacts",
      "anchors visible",
      "grayscale and low-motion semantics remain readable",
      "review boards are not Web or Windows runtime captures"
    ]
  };
  outputs.set("previews/preview-evidence.json", json(previewEvidence));

  return outputs;
}

async function generate(checkOnly) {
  const outputs = buildGeneratedOutputs();
  const mismatches = [];
  for (const [relativePath, content] of outputs) {
    const absolutePath = resolve(PACKAGE_ROOT, relativePath);
    if (checkOnly) {
      let actual = null;
      try {
        actual = await readFile(absolutePath, "utf8");
      } catch {
        mismatches.push(relativePath + " (missing)");
        continue;
      }
      if (actual !== content) mismatches.push(relativePath + " (stale)");
      continue;
    }
    await mkdir(dirname(absolutePath), { recursive: true });
    await writeFile(absolutePath, content, "utf8");
  }
  if (mismatches.length > 0) {
    throw new Error("Generated outputs do not match:\n" + mismatches.join("\n"));
  }
  return outputs.size;
}

const invokedPath = process.argv[1] ? resolve(process.argv[1]) : "";
if (invokedPath === fileURLToPath(import.meta.url)) {
  const checkOnly = process.argv.includes("--check");
  generate(checkOnly)
    .then((count) => process.stdout.write("system sandbox blockouts " + (checkOnly ? "checked" : "generated") + ": " + count + " files\n"))
    .catch((error) => {
      process.stderr.write(String(error.stack || error) + "\n");
      process.exitCode = 1;
    });
}
