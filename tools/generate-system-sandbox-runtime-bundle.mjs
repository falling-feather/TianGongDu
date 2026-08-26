import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
  loadRuntimeImportPackage,
  validateRuntimePackage
} from "../assets_src/system-sandbox-blockouts/tools/validate-system-sandbox-runtime-assets.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const KIND = new Map([
  ["player", "player"],
  ["actor", "actor"],
  ["obstacle", "obstacle"],
  ["interaction", "interaction"],
  ["mechanism", "mechanism"],
  ["safe_point", "safe_point"],
  ["effect", "effect"]
]);
const FORMAT = new Map([["PNG", "png"]]);
const PIXEL_FORMAT = new Map([["RGBA8", "rgba8"]]);
const COLOR_SPACE = new Map([["sRGB", "srgb"]]);
const ALPHA_MODE = new Map([
  [
    "straight-source-premultiply-on-upload",
    "straight_source_premultiply_on_upload"
  ]
]);
const FILTER = new Map([["linear", "linear"]]);
const WRAP = new Map([["clamp", "clamp"]]);
const METRICS = new Map([
  ["width", ["width", "width_mm"]],
  ["depth", ["depth", "depth_mm"]],
  ["height", ["height", "height_mm"]],
  ["bodyHeight", ["body_height", "body_height_mm"]],
  ["bodyRootHeight", ["body_root_height", "body_root_height_mm"]],
  ["groundRingDiameter", ["ground_ring_diameter", "ground_ring_diameter_mm"]],
  ["forward", ["forward", "forward_mm"]],
  ["lateral", ["lateral", "lateral_mm"]],
  ["sweep", ["sweep", "sweep_mm"]]
]);

function argument(name) {
  const index = process.argv.indexOf(name);
  return index < 0 ? undefined : process.argv[index + 1];
}

function cppString(value) {
  return JSON.stringify(value);
}

function mappedEnum(mapping, value, field) {
  const result = mapping.get(value);
  if (!result) {
    throw new Error(`unsupported ${field} ${JSON.stringify(value)}`);
  }
  return result;
}

function hexBytes(bytes, indent = "    ") {
  const rows = [];
  for (let offset = 0; offset < bytes.length; offset += 16) {
    rows.push(`${indent}${[...bytes.subarray(offset, offset + 16)]
      .map((value) => `0x${value.toString(16).padStart(2, "0")}`)
      .join(", ")}`);
  }
  return rows.join(",\n");
}

function digestInitializer(hex) {
  return `{${[...Buffer.from(hex, "hex")]
    .map((value) => `0x${value.toString(16).padStart(2, "0")}`)
    .join(", ")}}`;
}

function metricInitializer(entry) {
  const source = entry.visualBoundsMm ?? entry.nominalPresentationExtentMm ?? {};
  const shape = entry.visualBoundsMm ? "visual_bounds" : "nominal_extent";
  const flags = [];
  const values = new Map();
  for (const [jsonName, [flag, member]] of METRICS) {
    if (Object.hasOwn(source, jsonName)) {
      flags.push(`metric(Metric::${flag})`);
      values.set(member, source[jsonName]);
    }
  }
  const ordered = [
    "width_mm",
    "depth_mm",
    "height_mm",
    "body_height_mm",
    "body_root_height_mm",
    "ground_ring_diameter_mm",
    "forward_mm",
    "lateral_mm",
    "sweep_mm"
  ];
  return `{SandboxAssetMetricShape::${shape}, static_cast<std::uint16_t>(${flags.length ? flags.join(" | ") : "0U"}), ${ordered
    .map((name) => values.get(name) ?? 0)
    .join(", ")}}`;
}

function artifactSource(entry, quality, index) {
  const artifact = entry.artifacts[quality];
  const expectedArtifactId = artifact.artifactId;
  for (const platform of ["windows", "web"]) {
    if (entry.platformQuality?.[platform]?.[quality] !== expectedArtifactId) {
      throw new Error(
        `${entry.stableAssetId} ${platform}/${quality} does not name its own artifact`
      );
    }
  }
  if (artifact.mipmaps !== false) {
    throw new Error(
      `${entry.stableAssetId} ${quality} mipmaps must be explicit false`
    );
  }
  const bytesName = `bytes_${index}_${quality}`;
  return {
    declaration:
      `constexpr std::array<std::uint8_t, ${artifact.fileBytes}> ${bytesName}{\n` +
      `${hexBytes(entry.__bytes[quality])}\n};`,
    initializer:
      `{${cppString(artifact.artifactId)}, Quality::${quality}, ` +
      `SandboxAssetFormat::${mappedEnum(FORMAT, artifact.format, "format")}, ` +
      `SandboxAssetPixelFormat::${mappedEnum(
        PIXEL_FORMAT,
        artifact.pixelFormat,
        "pixelFormat"
      )}, ` +
      `SandboxAssetColorSpace::${mappedEnum(
        COLOR_SPACE,
        artifact.colorSpace,
        "colorSpace"
      )}, ` +
      `SandboxAssetAlphaMode::${mappedEnum(
        ALPHA_MODE,
        artifact.alphaMode,
        "alphaMode"
      )}, ` +
      `SandboxAssetFilter::${mappedEnum(
        FILTER,
        artifact.filter,
        "filter"
      )}, ` +
      `SandboxAssetWrap::${mappedEnum(WRAP, artifact.wrap, "wrap")}, ` +
      `${artifact.mipmaps}, ` +
      `${digestInitializer(artifact.sha256)}, ${artifact.width}U, ` +
      `${artifact.height}U, ${artifact.rootAnchorPx.x}U, ` +
      `${artifact.rootAnchorPx.y}U, ` +
      `${Math.round(entry.rootAnchorUv.x * 1_000_000)}U, ` +
      `${Math.round(entry.rootAnchorUv.y * 1_000_000)}U, ` +
      `${artifact.fileBytes}U, ` +
      `${artifact.decodedBytes}U, ${bytesName}}`
  };
}

async function render() {
  const packageData = await loadRuntimeImportPackage();
  await validateRuntimePackage({
    authoritativeRoot: packageData.manifest.authoritativeRoot
  });
  const fingerprintHash = createHash("sha256").update(
    Buffer.from(packageData.manifestText, "utf8")
  );
  for (const entry of packageData.manifest.entries) {
    entry.__bytes = {};
    for (const quality of ["standard", "low"]) {
      const bytes = packageData.artifacts.get(entry.artifacts[quality].path);
      entry.__bytes[quality] = bytes;
      fingerprintHash.update(Buffer.from(entry.stableAssetId, "utf8"));
      fingerprintHash.update(bytes);
    }
  }
  const fingerprint = fingerprintHash.digest("hex");
  const declarations = [];
  const entries = [];
  packageData.manifest.entries.forEach((entry, index) => {
    if (!KIND.has(entry.sandboxAssetKind)) {
      throw new Error(`unsupported SandboxAssetKind ${entry.sandboxAssetKind}`);
    }
    const anchorsName = `anchors_${index}`;
    declarations.push(
      `constexpr std::array<Anchor, ${entry.anchors.length}> ${anchorsName}{{\n` +
      `${entry.anchors
        .map(
          (anchor) =>
            `    {${cppString(anchor.name)}, ${anchor.localMm.x}, ` +
            `${anchor.localMm.y}, ${anchor.localMm.height}, ` +
            `${cppString(anchor.role)}}`
        )
        .join(",\n")}\n}};`
    );
    const standard = artifactSource(entry, "standard", index);
    const low = artifactSource(entry, "low", index);
    declarations.push(standard.declaration, low.declaration);
    entries.push(
      `    {${cppString(entry.stableAssetId)}, ` +
      `0x${entry.stableContentKeyHex}ULL, ` +
      `Kind::${KIND.get(entry.sandboxAssetKind)}, ` +
      `${metricInitializer(entry)}, ${anchorsName}, ` +
      `${standard.initializer}, ${low.initializer}}`
    );
  });

  const manifest = packageData.manifest;
  const source =
`// Generated by tools/generate-system-sandbox-runtime-bundle.mjs. Do not edit.
#include <tgd/presentation/sandbox_asset_resolver.hpp>

#include <array>
#include <cstdint>

namespace tgd::presentation {
namespace {
using Anchor = SandboxAssetAnchorView;
using Entry = SandboxAssetRegistryEntryView;
using Kind = contracts::SandboxAssetKind;
using Metric = SandboxAssetMetric;
using Quality = SandboxAssetQuality;
constexpr std::uint16_t metric(Metric value) noexcept {
    return static_cast<std::uint16_t>(value);
}

${declarations.join("\n\n")}

constexpr std::array<Entry, ${manifest.entries.length}> entries{{
${entries.join(",\n")}
}};
}  // namespace

SandboxAssetRegistryView system_sandbox_asset_registry() noexcept {
    return {
        ${digestInitializer(fingerprint)},
        SandboxAssetMaturity::blockout,
        SandboxAssetChannel::internal_preview,
        SandboxAssetLicense::review_recorded_not_release_cleared,
        false,
        false,
        ${manifest.limits.standardTransferBytes}U,
        ${manifest.limits.lowTransferBytes}U,
        ${manifest.limits.standardDecodedBytes}U,
        ${manifest.limits.lowDecodedBytes}U,
        entries,
    };
}

}  // namespace tgd::presentation
`;
  return {
    source,
    fingerprint,
    entryCount: manifest.entries.length,
    artifactCount: manifest.entries.length * 2
  };
}

const output = argument("--output");
if (!output) {
  throw new Error("--output is required");
}
const absoluteOutput = resolve(ROOT, output);
const { source, fingerprint, entryCount, artifactCount } = await render();
if (process.argv.includes("--check")) {
  const existing = await readFile(absoluteOutput, "utf8").catch(() => null);
  if (existing !== source) {
    throw new Error(`generated registry is missing or stale: ${absoluteOutput}`);
  }
} else {
  await mkdir(dirname(absoluteOutput), { recursive: true });
  await writeFile(absoluteOutput, source, "utf8");
}
process.stdout.write(
  `${JSON.stringify({
    task: "DEV-003",
    entries: entryCount,
    artifacts: artifactCount,
    fingerprint,
    output: absoluteOutput
  })}\n`
);
