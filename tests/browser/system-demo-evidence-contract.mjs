import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

export const systemDemoEvidenceContractVersion = "1.0.1";

const sha256Pattern = /^[0-9a-f]{64}$/;
const sourceCommitPattern = /^[0-9a-f]{40}$/;
const checksumPattern = /^[0-9a-f]{16}$/;
const diagnosticCodePattern = /^[A-Z][A-Z0-9_]{2,63}$/;
const maximumIdentityStringBytes = 256;
const maximumDiagnostics = 64;
const contractOnlyCanonicalSha256 =
  "5a52f08ee5d5cf9ea78231570b74830e3d98762d14c8bc1e954da68f82f3ac2c";
const failurePhases = ["authoring", "compiler", "decode", "session", "reload"];
const browserOrder = ["chrome", "edge", "firefox"];
const lastValidKeys = [
  "lastValidDocument",
  "validatedOwningPackage",
  "runningPreviewSession"
];

export const systemDemoFailurePreservation = Object.freeze({
  authoring: Object.freeze([...lastValidKeys]),
  compiler: Object.freeze(["validatedOwningPackage", "runningPreviewSession"]),
  decode: Object.freeze(["validatedOwningPackage", "runningPreviewSession"]),
  session: Object.freeze(["runningPreviewSession"]),
  reload: Object.freeze([...lastValidKeys])
});

class EvidenceContractError extends Error {
  constructor(code, path, message) {
    super(`${path}: ${message}`);
    this.name = "EvidenceContractError";
    this.code = code;
    this.path = path;
  }
}

function fail(code, path, message) {
  throw new EvidenceContractError(code, path, message);
}

function assertExactRecord(value, path, expectedKeys) {
  if (
    value === null ||
    typeof value !== "object" ||
    Array.isArray(value) ||
    ![Object.prototype, null].includes(Object.getPrototypeOf(value))
  ) {
    fail("invalid_record", path, "must be a plain object");
  }

  const ownKeys = Reflect.ownKeys(value);
  const symbolKeys = ownKeys.filter((key) => typeof key === "symbol");
  if (symbolKeys.length > 0) {
    fail("unknown_field", path, "symbol keys are not serializable evidence fields");
  }

  const stringKeys = ownKeys.map(String);
  for (const key of [...stringKeys].sort()) {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor?.enumerable || !("value" in descriptor)) {
      fail(
        "invalid_property_descriptor",
        `${path}.${key}`,
        "must be an enumerable data property"
      );
    }
  }

  for (const key of expectedKeys) {
    if (!Object.hasOwn(value, key)) {
      fail("missing_field", `${path}.${key}`, "is required");
    }
  }

  const unknownKeys = stringKeys
    .filter((key) => !expectedKeys.includes(key))
    .sort();
  if (unknownKeys.length > 0) {
    fail("unknown_field", `${path}.${unknownKeys[0]}`, "is not part of the closed contract");
  }
}

function assertDenseArray(value, path, expectedLength = null) {
  if (!Array.isArray(value) || Object.getPrototypeOf(value) !== Array.prototype) {
    fail("invalid_array", path, "must be a plain array");
  }
  if (expectedLength !== null && value.length !== expectedLength) {
    fail("invalid_array_length", path, `must contain exactly ${expectedLength} entries`);
  }

  const indexKeys = Array.from({ length: value.length }, (_, index) => String(index));
  for (const key of indexKeys) {
    if (!Object.hasOwn(value, key)) fail("missing_array_entry", `${path}[${key}]`, "is required");
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor?.enumerable || !("value" in descriptor)) {
      fail(
        "invalid_property_descriptor",
        `${path}[${key}]`,
        "must be an enumerable data property"
      );
    }
  }

  const allowedKeys = new Set(["length", ...indexKeys]);
  const unknownKeys = Reflect.ownKeys(value)
    .filter((key) => !allowedKeys.has(key))
    .sort((left, right) => String(left).localeCompare(String(right)));
  if (unknownKeys.length > 0) {
    fail("unknown_field", path, "array own properties are not part of the closed contract");
  }
}

function assertNonEmptyString(value, path) {
  if (typeof value !== "string") {
    fail("invalid_string", path, "must be a non-empty trimmed string");
  }
  const byteLength = Buffer.byteLength(value, "utf8");
  if (byteLength === 0 || value.trim() !== value) {
    fail("invalid_string", path, "must be a non-empty trimmed string");
  }
  if (byteLength > maximumIdentityStringBytes) {
    fail(
      "identity_string_too_long",
      path,
      `must contain at most ${maximumIdentityStringBytes} UTF-8 bytes`
    );
  }
}

function assertLiteral(value, expected, path, code = "invalid_literal") {
  if (value !== expected) fail(code, path, `must equal ${JSON.stringify(expected)}`);
}

function assertOneOf(value, allowed, path) {
  if (!allowed.includes(value)) {
    fail("invalid_enum", path, `must be one of ${allowed.join(", ")}`);
  }
}

function assertSha256(value, path) {
  if (typeof value !== "string" || !sha256Pattern.test(value)) {
    fail("invalid_sha256", path, "must be 64 lowercase hexadecimal characters");
  }
}

function assertSourceCommit(value, path) {
  if (typeof value !== "string" || !sourceCommitPattern.test(value)) {
    fail("invalid_source_commit", path, "must be a full 40-character lowercase Git SHA");
  }
}

function assertChecksum(value, path) {
  if (typeof value !== "string" || !checksumPattern.test(value)) {
    fail("invalid_checksum", path, "must be 16 lowercase hexadecimal characters");
  }
}

function assertSafeInteger(value, path, minimum = 0) {
  if (!Number.isSafeInteger(value) || value < minimum) {
    fail("invalid_integer", path, `must be a safe integer >= ${minimum}`);
  }
}

function canonicalJson(value) {
  if (value === null || typeof value === "boolean" || typeof value === "string") {
    return JSON.stringify(value);
  }
  if (typeof value === "number") return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(",")}]`;

  return `{${Object.keys(value)
    .sort()
    .map((key) => `${JSON.stringify(key)}:${canonicalJson(value[key])}`)
    .join(",")}}`;
}

function recordsEqual(left, right) {
  return canonicalJson(left) === canonicalJson(right);
}

function assertCompilerIdentity(value, path) {
  assertExactRecord(value, path, ["name", "version", "toolchainSha256", "artifactSha256"]);
  assertNonEmptyString(value.name, `${path}.name`);
  assertNonEmptyString(value.version, `${path}.version`);
  assertSha256(value.toolchainSha256, `${path}.toolchainSha256`);
  assertSha256(value.artifactSha256, `${path}.artifactSha256`);
}

function assertEvidenceArtifacts(value, path) {
  assertExactRecord(value, path, ["reportSha256", "screenshotSha256", "videoSha256"]);
  assertSha256(value.reportSha256, `${path}.reportSha256`);
  assertSha256(value.screenshotSha256, `${path}.screenshotSha256`);
  assertSha256(value.videoSha256, `${path}.videoSha256`);
}

function assertDocumentIdentity(value, path) {
  assertExactRecord(value, path, [
    "layer",
    "schemaVersion",
    "canonicalJsonSha256",
    "runtimeProjectionSha256",
    "revision",
    "savedRevision"
  ]);
  assertLiteral(value.layer, "authoring-document", `${path}.layer`);
  assertNonEmptyString(value.schemaVersion, `${path}.schemaVersion`);
  assertSha256(value.canonicalJsonSha256, `${path}.canonicalJsonSha256`);
  assertSha256(value.runtimeProjectionSha256, `${path}.runtimeProjectionSha256`);
  assertSafeInteger(value.revision, `${path}.revision`);
  assertSafeInteger(value.savedRevision, `${path}.savedRevision`);
  if (value.savedRevision > value.revision) {
    fail("invalid_revision", `${path}.savedRevision`, "cannot exceed revision");
  }
}

function assertPackageIdentity(value, path) {
  assertExactRecord(value, path, [
    "layer",
    "formatVersion",
    "sourceCanonicalJsonSha256",
    "runtimeProjectionSha256",
    "fingerprintSha256",
    "packageSha256",
    "byteLength",
    "compiler"
  ]);
  assertLiteral(value.layer, "validated-owning-package", `${path}.layer`);
  assertNonEmptyString(value.formatVersion, `${path}.formatVersion`);
  assertSha256(value.sourceCanonicalJsonSha256, `${path}.sourceCanonicalJsonSha256`);
  assertSha256(value.runtimeProjectionSha256, `${path}.runtimeProjectionSha256`);
  assertSha256(value.fingerprintSha256, `${path}.fingerprintSha256`);
  assertSha256(value.packageSha256, `${path}.packageSha256`);
  assertSafeInteger(value.byteLength, `${path}.byteLength`, 1);
  assertCompilerIdentity(value.compiler, `${path}.compiler`);
}

function assertSessionIdentity(value, path) {
  assertExactRecord(value, path, [
    "layer",
    "state",
    "packageSha256",
    "generation",
    "checksum",
    "commandSequence",
    "eventSequence"
  ]);
  assertLiteral(value.layer, "running-preview-session", `${path}.layer`);
  assertLiteral(value.state, "running", `${path}.state`);
  assertSha256(value.packageSha256, `${path}.packageSha256`);
  assertSafeInteger(value.generation, `${path}.generation`, 1);
  assertChecksum(value.checksum, `${path}.checksum`);
  assertSafeInteger(value.commandSequence, `${path}.commandSequence`);
  assertSafeInteger(value.eventSequence, `${path}.eventSequence`);
}

function assertLastValid(value, path) {
  assertExactRecord(value, path, lastValidKeys);
  assertDocumentIdentity(value.lastValidDocument, `${path}.lastValidDocument`);
  assertPackageIdentity(value.validatedOwningPackage, `${path}.validatedOwningPackage`);
  assertSessionIdentity(value.runningPreviewSession, `${path}.runningPreviewSession`);
}

function assertDocumentPackageBinding(lastValid, path) {
  const document = lastValid.lastValidDocument;
  const owningPackage = lastValid.validatedOwningPackage;
  if (owningPackage.sourceCanonicalJsonSha256 !== document.canonicalJsonSha256) {
    fail(
      "identity_layer_confusion",
      `${path}.validatedOwningPackage.sourceCanonicalJsonSha256`,
      "must identify the recorded last-valid authoring document"
    );
  }
  if (owningPackage.runtimeProjectionSha256 !== document.runtimeProjectionSha256) {
    fail(
      "identity_layer_confusion",
      `${path}.validatedOwningPackage.runtimeProjectionSha256`,
      "must identify the recorded runtime projection"
    );
  }
}

function assertPackageSessionBinding(lastValid, path) {
  if (
    lastValid.runningPreviewSession.packageSha256 !==
    lastValid.validatedOwningPackage.packageSha256
  ) {
    fail(
      "identity_layer_confusion",
      `${path}.runningPreviewSession.packageSha256`,
      "must identify the recorded validated owning package"
    );
  }
}

function assertCoherentLastValid(lastValid, path) {
  assertDocumentPackageBinding(lastValid, path);
  assertPackageSessionBinding(lastValid, path);
}

function assertGraphicsIdentity(value, path, keys) {
  assertExactRecord(value, path, keys);
  for (const key of keys) assertNonEmptyString(value[key], `${path}.${key}`);
}

function assertRuntimeSessionMatches(value, runningPreviewSession, path) {
  assertSessionIdentity(value, path);
  if (!recordsEqual(value, runningPreviewSession)) {
    fail(
      "runtime_last_valid_mismatch",
      path,
      "must identify the independently recorded running Preview/Session"
    );
  }
}

function assertPlatforms(value, runningPreviewSession, path) {
  assertExactRecord(value, path, ["windows", "web"]);

  const windowsPath = `${path}.windows`;
  assertExactRecord(value.windows, windowsPath, [
    "surface",
    "osVersion",
    "compiler",
    "executableSha256",
    "graphics",
    "session",
    "evidence"
  ]);
  assertLiteral(
    value.windows.surface,
    "visible-window-preview",
    `${windowsPath}.surface`,
    "invalid_visible_preview"
  );
  assertNonEmptyString(value.windows.osVersion, `${windowsPath}.osVersion`);
  assertCompilerIdentity(value.windows.compiler, `${windowsPath}.compiler`);
  assertSha256(value.windows.executableSha256, `${windowsPath}.executableSha256`);
  assertGraphicsIdentity(value.windows.graphics, `${windowsPath}.graphics`, [
    "api",
    "adapter",
    "driverVersion"
  ]);
  assertRuntimeSessionMatches(
    value.windows.session,
    runningPreviewSession,
    `${windowsPath}.session`
  );
  assertEvidenceArtifacts(value.windows.evidence, `${windowsPath}.evidence`);

  const webPath = `${path}.web`;
  assertExactRecord(value.web, webPath, [
    "surface",
    "compiler",
    "distArtifactSha256",
    "browsers"
  ]);
  assertLiteral(value.web.surface, "web-single-preview", `${webPath}.surface`);
  assertCompilerIdentity(value.web.compiler, `${webPath}.compiler`);
  assertSha256(value.web.distArtifactSha256, `${webPath}.distArtifactSha256`);
  assertDenseArray(value.web.browsers, `${webPath}.browsers`, browserOrder.length);

  browserOrder.forEach((expectedName, index) => {
    const browser = value.web.browsers[index];
    const browserPath = `${webPath}.browsers[${index}]`;
    assertExactRecord(browser, browserPath, [
      "name",
      "version",
      "webgl",
      "graphics",
      "session",
      "evidence"
    ]);
    assertLiteral(browser.name, expectedName, `${browserPath}.name`, "invalid_browser_matrix");
    assertNonEmptyString(browser.version, `${browserPath}.version`);
    assertExactRecord(browser.webgl, `${browserPath}.webgl`, ["version", "vendor", "renderer"]);
    assertNonEmptyString(browser.webgl.version, `${browserPath}.webgl.version`);
    if (!/WebGL\s*2/i.test(browser.webgl.version)) {
      fail("invalid_webgl", `${browserPath}.webgl.version`, "must identify WebGL 2");
    }
    assertNonEmptyString(browser.webgl.vendor, `${browserPath}.webgl.vendor`);
    assertNonEmptyString(browser.webgl.renderer, `${browserPath}.webgl.renderer`);
    assertGraphicsIdentity(browser.graphics, `${browserPath}.graphics`, [
      "mode",
      "adapter",
      "driverVersion"
    ]);
    assertRuntimeSessionMatches(
      browser.session,
      runningPreviewSession,
      `${browserPath}.session`
    );
    assertEvidenceArtifacts(browser.evidence, `${browserPath}.evidence`);
  });
}

function assertOutcome(value, path) {
  assertExactRecord(value, path, [
    "status",
    "phase",
    "attemptedIdentitySha256",
    "previousLastValid",
    "diagnostics"
  ]);
  assertOneOf(value.status, ["passed", "failed"], `${path}.status`);
  assertOneOf(value.phase, ["complete", ...failurePhases], `${path}.phase`);
  assertSha256(value.attemptedIdentitySha256, `${path}.attemptedIdentitySha256`);
  assertLastValid(value.previousLastValid, `${path}.previousLastValid`);

  if (Array.isArray(value.diagnostics) && value.diagnostics.length > maximumDiagnostics) {
    fail(
      "diagnostics_capacity_exceeded",
      `${path}.diagnostics`,
      `must contain at most ${maximumDiagnostics} entries`
    );
  }
  assertDenseArray(value.diagnostics, `${path}.diagnostics`);
  value.diagnostics.forEach((diagnostic, index) => {
    const diagnosticPath = `${path}.diagnostics[${index}]`;
    assertExactRecord(diagnostic, diagnosticPath, [
      "phase",
      "code",
      "severity",
      "messageSha256"
    ]);
    assertOneOf(diagnostic.phase, failurePhases, `${diagnosticPath}.phase`);
    if (
      typeof diagnostic.code !== "string" ||
      !diagnosticCodePattern.test(diagnostic.code)
    ) {
      fail(
        "invalid_diagnostic_code",
        `${diagnosticPath}.code`,
        "must be a stable uppercase diagnostic code"
      );
    }
    assertLiteral(diagnostic.severity, "error", `${diagnosticPath}.severity`);
    assertSha256(diagnostic.messageSha256, `${diagnosticPath}.messageSha256`);
  });

  if (value.status === "passed") {
    assertLiteral(value.phase, "complete", `${path}.phase`);
    if (value.diagnostics.length !== 0) {
      fail("unexpected_diagnostic", `${path}.diagnostics`, "must be empty for a passed run");
    }
    return;
  }

  if (value.phase === "complete") {
    fail("invalid_failure_phase", `${path}.phase`, "a failed run must name its failing phase");
  }
  if (
    !value.diagnostics.some(
      (diagnostic) => diagnostic.phase === value.phase && diagnostic.severity === "error"
    )
  ) {
    fail(
      "missing_failure_diagnostic",
      `${path}.diagnostics`,
      "must contain an error diagnostic for the failing phase"
    );
  }
}

function assertFailureClosure(lastValid, outcome) {
  if (outcome.status !== "failed") return;
  for (const key of systemDemoFailurePreservation[outcome.phase]) {
    if (!recordsEqual(lastValid[key], outcome.previousLastValid[key])) {
      fail(
        "failed_stage_overwrote_last_valid",
        `$.lastValid.${key}`,
        `${outcome.phase} failure must preserve the prior ${key}`
      );
    }
  }
}

function assertCurrentLayerBindings(lastValid, outcome) {
  if (outcome.status === "passed") {
    assertCoherentLastValid(lastValid, "$.lastValid");
    return;
  }
  if (outcome.phase === "session") {
    assertDocumentPackageBinding(lastValid, "$.lastValid");
  }
}

function assertSystemDemoEvidenceManifest(manifest) {
  assertExactRecord(manifest, "$", [
    "contractVersion",
    "sourceCommit",
    "lastValid",
    "platforms",
    "outcome"
  ]);
  assertLiteral(manifest.contractVersion, systemDemoEvidenceContractVersion, "$.contractVersion");
  assertSourceCommit(manifest.sourceCommit, "$.sourceCommit");
  assertLastValid(manifest.lastValid, "$.lastValid");
  assertOutcome(manifest.outcome, "$.outcome");
  assertFailureClosure(manifest.lastValid, manifest.outcome);
  assertCurrentLayerBindings(manifest.lastValid, manifest.outcome);
  assertPlatforms(
    manifest.platforms,
    manifest.lastValid.runningPreviewSession,
    "$.platforms"
  );
}

export function serializeSystemDemoEvidenceManifest(manifest) {
  assertSystemDemoEvidenceManifest(manifest);
  return canonicalJson(manifest);
}

export function hashSystemDemoEvidenceManifest(manifest) {
  return createHash("sha256")
    .update(serializeSystemDemoEvidenceManifest(manifest), "utf8")
    .digest("hex");
}

export function validateSystemDemoEvidenceManifest(manifest) {
  try {
    const serialized = serializeSystemDemoEvidenceManifest(manifest);
    return {
      ok: true,
      diagnostics: [],
      serialized,
      sha256: createHash("sha256").update(serialized, "utf8").digest("hex")
    };
  } catch (error) {
    if (!(error instanceof EvidenceContractError)) throw error;
    return {
      ok: false,
      diagnostics: [{ code: error.code, path: error.path, message: error.message }],
      serialized: null,
      sha256: null
    };
  }
}

function sha256Label(label) {
  return createHash("sha256").update(`contract-only:${label}`, "utf8").digest("hex");
}

function compilerIdentity(label, name, version) {
  return {
    name,
    version,
    toolchainSha256: sha256Label(`${label}:toolchain`),
    artifactSha256: sha256Label(`${label}:artifact`)
  };
}

function evidenceArtifacts(label) {
  return {
    reportSha256: sha256Label(`${label}:report`),
    screenshotSha256: sha256Label(`${label}:screenshot`),
    videoSha256: sha256Label(`${label}:video`)
  };
}

function createContractOnlySample() {
  const document = {
    layer: "authoring-document",
    schemaVersion: "1.1.0",
    canonicalJsonSha256: sha256Label("authoring-json"),
    runtimeProjectionSha256: sha256Label("runtime-projection"),
    revision: 7,
    savedRevision: 7
  };
  const owningPackage = {
    layer: "validated-owning-package",
    formatVersion: "1.1",
    sourceCanonicalJsonSha256: document.canonicalJsonSha256,
    runtimeProjectionSha256: document.runtimeProjectionSha256,
    fingerprintSha256: sha256Label("package-fingerprint"),
    packageSha256: sha256Label("package-bytes"),
    byteLength: 4096,
    compiler: compilerIdentity("package-compiler", "tgd-sandbox-compiler", "1.0.0")
  };
  const runningPreviewSession = {
    layer: "running-preview-session",
    state: "running",
    packageSha256: owningPackage.packageSha256,
    generation: 3,
    checksum: "0123456789abcdef",
    commandSequence: 21,
    eventSequence: 34
  };
  const lastValid = {
    lastValidDocument: document,
    validatedOwningPackage: owningPackage,
    runningPreviewSession
  };

  return {
    contractVersion: systemDemoEvidenceContractVersion,
    sourceCommit: "1".repeat(40),
    lastValid,
    platforms: {
      windows: {
        surface: "visible-window-preview",
        osVersion: "Windows contract identity",
        compiler: compilerIdentity("windows", "MSVC", "contract-version"),
        executableSha256: sha256Label("windows-executable"),
        graphics: {
          api: "contract-graphics-api",
          adapter: "contract-graphics-adapter",
          driverVersion: "contract-driver-version"
        },
        session: structuredClone(runningPreviewSession),
        evidence: evidenceArtifacts("windows")
      },
      web: {
        surface: "web-single-preview",
        compiler: compilerIdentity("web", "Emscripten", "contract-version"),
        distArtifactSha256: sha256Label("web-dist"),
        browsers: browserOrder.map((name) => ({
          name,
          version: "contract-version",
          webgl: {
            version: "WebGL 2 contract identity",
            vendor: "contract-vendor",
            renderer: "contract-renderer"
          },
          graphics: {
            mode: "contract-mode",
            adapter: "contract-adapter",
            driverVersion: "contract-driver-version"
          },
          session: structuredClone(runningPreviewSession),
          evidence: evidenceArtifacts(`web:${name}`)
        }))
      }
    },
    outcome: {
      status: "passed",
      phase: "complete",
      attemptedIdentitySha256: sha256Label("complete-attempt"),
      previousLastValid: structuredClone(lastValid),
      diagnostics: []
    }
  };
}

function bindPlatformsToRunningSession(manifest) {
  const runningPreviewSession = manifest.lastValid.runningPreviewSession;
  manifest.platforms.windows.session = structuredClone(runningPreviewSession);
  for (const browser of manifest.platforms.web.browsers) {
    browser.session = structuredClone(runningPreviewSession);
  }
}

function advanceDocument(lastValid, label) {
  const document = lastValid.lastValidDocument;
  document.canonicalJsonSha256 = sha256Label(`${label}:authoring-json`);
  document.runtimeProjectionSha256 = sha256Label(`${label}:runtime-projection`);
  document.revision += 1;
  document.savedRevision = document.revision;
}

function bindPackageToDocument(lastValid, label) {
  const document = lastValid.lastValidDocument;
  const owningPackage = lastValid.validatedOwningPackage;
  owningPackage.sourceCanonicalJsonSha256 = document.canonicalJsonSha256;
  owningPackage.runtimeProjectionSha256 = document.runtimeProjectionSha256;
  owningPackage.fingerprintSha256 = sha256Label(`${label}:package-fingerprint`);
  owningPackage.packageSha256 = sha256Label(`${label}:package-bytes`);
  owningPackage.byteLength += 1;
  owningPackage.compiler.artifactSha256 = sha256Label(`${label}:compiler-artifact`);
}

function bindSessionToPackage(lastValid, label) {
  const session = lastValid.runningPreviewSession;
  session.packageSha256 = lastValid.validatedOwningPackage.packageSha256;
  session.generation += 1;
  session.checksum = sha256Label(`${label}:session-checksum`).slice(0, 16);
  session.commandSequence += 1;
  session.eventSequence += 1;
}

function createFailedContractOnlySample(phase, priorLastValid = null, label = phase) {
  const manifest = createContractOnlySample();
  const previousLastValid = structuredClone(priorLastValid ?? manifest.lastValid);
  manifest.lastValid = structuredClone(previousLastValid);
  manifest.outcome = {
    status: "failed",
    phase,
    attemptedIdentitySha256: sha256Label(`${label}:attempt`),
    previousLastValid,
    diagnostics: [
      {
        phase,
        code: `SYSTEM_DEMO_${phase.toUpperCase()}_FAILED`,
        severity: "error",
        messageSha256: sha256Label(`${label}:diagnostic`)
      }
    ]
  };

  if (["compiler", "decode", "session"].includes(phase)) {
    advanceDocument(manifest.lastValid, label);
  }
  if (phase === "session") {
    bindPackageToDocument(manifest.lastValid, label);
  }
  bindPlatformsToRunningSession(manifest);
  return manifest;
}

function createPassedContractOnlySample(priorLastValid, label) {
  const manifest = createContractOnlySample();
  manifest.lastValid = structuredClone(priorLastValid);
  bindPackageToDocument(manifest.lastValid, label);
  bindSessionToPackage(manifest.lastValid, label);
  bindPlatformsToRunningSession(manifest);
  manifest.outcome = {
    status: "passed",
    phase: "complete",
    attemptedIdentitySha256: sha256Label(`${label}:attempt`),
    previousLastValid: structuredClone(priorLastValid),
    diagnostics: []
  };
  return manifest;
}

function reverseRecordOrder(value) {
  if (Array.isArray(value)) return value.map(reverseRecordOrder);
  if (value === null || typeof value !== "object") return value;
  return Object.fromEntries(
    Object.keys(value)
      .reverse()
      .map((key) => [key, reverseRecordOrder(value[key])])
  );
}

function expectInvalid(manifest, expectedCode, label) {
  const result = validateSystemDemoEvidenceManifest(manifest);
  assert.equal(result.ok, false, `${label} unexpectedly passed`);
  assert.equal(result.diagnostics[0]?.code, expectedCode, label);
}

export function runSystemDemoEvidenceContractSelfTest() {
  const valid = createContractOnlySample();
  const validResult = validateSystemDemoEvidenceManifest(valid);
  assert.equal(validResult.ok, true, JSON.stringify(validResult.diagnostics));
  assert.match(validResult.sha256, sha256Pattern);
  assert.equal(
    validResult.sha256,
    contractOnlyCanonicalSha256,
    "contract-only canonical manifest identity changed without an explicit update"
  );

  const reordered = reverseRecordOrder(valid);
  assert.equal(
    serializeSystemDemoEvidenceManifest(reordered),
    validResult.serialized,
    "canonical serialization changed with insertion order"
  );
  assert.equal(hashSystemDemoEvidenceManifest(reordered), validResult.sha256);

  for (const phase of failurePhases) {
    const failed = validateSystemDemoEvidenceManifest(createFailedContractOnlySample(phase));
    assert.equal(failed.ok, true, `${phase}: ${JSON.stringify(failed.diagnostics)}`);
  }

  const sessionFailureBBA = createFailedContractOnlySample(
    "session",
    valid.lastValid,
    "sequence:b"
  );
  assert.equal(validateSystemDemoEvidenceManifest(sessionFailureBBA).ok, true);
  assert.equal(
    sessionFailureBBA.lastValid.validatedOwningPackage.sourceCanonicalJsonSha256,
    sessionFailureBBA.lastValid.lastValidDocument.canonicalJsonSha256
  );
  assert.equal(
    recordsEqual(
      sessionFailureBBA.lastValid.runningPreviewSession,
      valid.lastValid.runningPreviewSession
    ),
    true,
    "session failure must retain the A running Session while document/package advance to B"
  );

  const compilerFailureCBA = createFailedContractOnlySample(
    "compiler",
    sessionFailureBBA.lastValid,
    "sequence:c:compiler"
  );
  assert.equal(validateSystemDemoEvidenceManifest(compilerFailureCBA).ok, true);
  assert.equal(
    recordsEqual(
      compilerFailureCBA.lastValid.validatedOwningPackage,
      sessionFailureBBA.lastValid.validatedOwningPackage
    ),
    true,
    "compiler failure must retain the B package"
  );
  assert.equal(
    recordsEqual(
      compilerFailureCBA.lastValid.runningPreviewSession,
      valid.lastValid.runningPreviewSession
    ),
    true,
    "compiler failure must retain the A running Session"
  );

  const decodeFailureCBA = createFailedContractOnlySample(
    "decode",
    sessionFailureBBA.lastValid,
    "sequence:c:decode"
  );
  assert.equal(validateSystemDemoEvidenceManifest(decodeFailureCBA).ok, true);
  assert.equal(
    recordsEqual(
      decodeFailureCBA.lastValid.validatedOwningPackage,
      sessionFailureBBA.lastValid.validatedOwningPackage
    ),
    true,
    "decode failure must retain the B package"
  );
  assert.equal(
    recordsEqual(
      decodeFailureCBA.lastValid.runningPreviewSession,
      valid.lastValid.runningPreviewSession
    ),
    true,
    "decode failure must retain the A running Session"
  );

  const reloadFailureCBA = createFailedContractOnlySample(
    "reload",
    compilerFailureCBA.lastValid,
    "sequence:c:reload"
  );
  assert.equal(validateSystemDemoEvidenceManifest(reloadFailureCBA).ok, true);
  assert.equal(recordsEqual(reloadFailureCBA.lastValid, compilerFailureCBA.lastValid), true);

  const authoringFailureCBA = createFailedContractOnlySample(
    "authoring",
    compilerFailureCBA.lastValid,
    "sequence:c:authoring"
  );
  assert.equal(validateSystemDemoEvidenceManifest(authoringFailureCBA).ok, true);
  assert.equal(recordsEqual(authoringFailureCBA.lastValid, compilerFailureCBA.lastValid), true);

  const passedCCC = createPassedContractOnlySample(
    authoringFailureCBA.lastValid,
    "sequence:c:complete"
  );
  assert.equal(validateSystemDemoEvidenceManifest(passedCCC).ok, true);
  assert.equal(
    passedCCC.lastValid.validatedOwningPackage.sourceCanonicalJsonSha256,
    passedCCC.lastValid.lastValidDocument.canonicalJsonSha256
  );
  assert.equal(
    passedCCC.lastValid.runningPreviewSession.packageSha256,
    passedCCC.lastValid.validatedOwningPackage.packageSha256
  );

  const missingField = createContractOnlySample();
  delete missingField.platforms.web.browsers[0].evidence.videoSha256;
  expectInvalid(missingField, "missing_field", "missing evidence field");

  const malformedHash = createContractOnlySample();
  malformedHash.lastValid.validatedOwningPackage.packageSha256 = "ABC";
  expectInvalid(malformedHash, "invalid_sha256", "malformed hash");

  const documentReplacedByPackage = createContractOnlySample();
  documentReplacedByPackage.lastValid.lastValidDocument = structuredClone(
    documentReplacedByPackage.lastValid.validatedOwningPackage
  );
  expectInvalid(documentReplacedByPackage, "missing_field", "package substituted for document");

  const packageReplacedDocumentIdentity = createContractOnlySample();
  packageReplacedDocumentIdentity.lastValid.validatedOwningPackage.sourceCanonicalJsonSha256 =
    sha256Label("wrong-authoring-document");
  expectInvalid(
    packageReplacedDocumentIdentity,
    "identity_layer_confusion",
    "package/document identity confusion"
  );

  const sessionReplacedPackageIdentity = createContractOnlySample();
  sessionReplacedPackageIdentity.lastValid.runningPreviewSession.packageSha256 =
    sessionReplacedPackageIdentity.lastValid.lastValidDocument.canonicalJsonSha256;
  expectInvalid(
    sessionReplacedPackageIdentity,
    "identity_layer_confusion",
    "session/package identity confusion"
  );

  const sessionFailureWithMismatchedPackage = createFailedContractOnlySample("session");
  sessionFailureWithMismatchedPackage.lastValid.validatedOwningPackage.sourceCanonicalJsonSha256 =
    sha256Label("session:mismatched-document");
  expectInvalid(
    sessionFailureWithMismatchedPackage,
    "identity_layer_confusion",
    "session failure accepted a package for another document"
  );

  const failureOverwriteMutation = {
    authoring: (manifest) => {
      manifest.lastValid.lastValidDocument.revision += 1;
    },
    compiler: (manifest) => {
      manifest.lastValid.validatedOwningPackage.byteLength += 1;
    },
    decode: (manifest) => {
      manifest.lastValid.validatedOwningPackage.byteLength += 1;
    },
    session: (manifest) => {
      manifest.lastValid.runningPreviewSession.commandSequence += 1;
    },
    reload: (manifest) => {
      manifest.lastValid.lastValidDocument.revision += 1;
    }
  };
  for (const phase of failurePhases) {
    const failedOverwrite = createFailedContractOnlySample(phase);
    failureOverwriteMutation[phase](failedOverwrite);
    expectInvalid(
      failedOverwrite,
      "failed_stage_overwrote_last_valid",
      `failed ${phase} overwrote a preserved last-valid layer`
    );
  }

  const missingFailureDiagnostic = createFailedContractOnlySample("compiler");
  missingFailureDiagnostic.outcome.diagnostics = [];
  expectInvalid(
    missingFailureDiagnostic,
    "missing_failure_diagnostic",
    "failure without phase diagnostic"
  );

  const coreCTestAsPreview = createContractOnlySample();
  coreCTestAsPreview.platforms.windows.surface = "core-ctest";
  expectInvalid(coreCTestAsPreview, "invalid_visible_preview", "Core CTest as visible Preview");

  const failedPlatformSessionMismatch = createFailedContractOnlySample("session");
  failedPlatformSessionMismatch.platforms.windows.session.commandSequence += 1;
  expectInvalid(
    failedPlatformSessionMismatch,
    "runtime_last_valid_mismatch",
    "platform evidence bound to another running Session"
  );

  const browserArrayWithOwnField = createContractOnlySample();
  browserArrayWithOwnField.platforms.web.browsers.contractBypass = true;
  expectInvalid(browserArrayWithOwnField, "unknown_field", "browser array own-field bypass");

  const hiddenManifestField = createContractOnlySample();
  Object.defineProperty(hiddenManifestField.platforms.windows, "hiddenBypass", {
    value: true,
    enumerable: false
  });
  expectInvalid(
    hiddenManifestField,
    "invalid_property_descriptor",
    "non-enumerable manifest bypass"
  );

  const maximumIdentity = createContractOnlySample();
  maximumIdentity.platforms.web.browsers[0].version = `${"a".repeat(254)}é`;
  assert.equal(Buffer.byteLength(maximumIdentity.platforms.web.browsers[0].version), 256);
  assert.equal(validateSystemDemoEvidenceManifest(maximumIdentity).ok, true);

  const overlongIdentity = createContractOnlySample();
  overlongIdentity.platforms.web.browsers[0].version = `${"a".repeat(255)}é`;
  assert.equal(Buffer.byteLength(overlongIdentity.platforms.web.browsers[0].version), 257);
  expectInvalid(overlongIdentity, "identity_string_too_long", "257-byte identity string");

  const maximumDiagnosticsManifest = createFailedContractOnlySample("compiler");
  maximumDiagnosticsManifest.outcome.diagnostics = Array.from({ length: 64 }, (_, index) => ({
    phase: "compiler",
    code: "SYSTEM_DEMO_COMPILER_FAILED",
    severity: "error",
    messageSha256: sha256Label(`compiler:maximum-diagnostic:${index}`)
  }));
  assert.equal(validateSystemDemoEvidenceManifest(maximumDiagnosticsManifest).ok, true);

  const tooManyDiagnostics = createFailedContractOnlySample("compiler");
  tooManyDiagnostics.outcome.diagnostics = Array.from({ length: 65 }, (_, index) => ({
    phase: "compiler",
    code: "SYSTEM_DEMO_COMPILER_FAILED",
    severity: "error",
    messageSha256: sha256Label(`compiler:diagnostic:${index}`)
  }));
  expectInvalid(
    tooManyDiagnostics,
    "diagnostics_capacity_exceeded",
    "65 failure diagnostics"
  );

  return {
    contractVersion: systemDemoEvidenceContractVersion,
    positiveManifests: 9 + failurePhases.length,
    negativeCases: 18,
    browserOrder,
    canonicalSha256: validResult.sha256
  };
}

const isEntrypoint =
  process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url;
if (isEntrypoint) {
  if (!process.argv.includes("--self-test")) {
    throw new Error("Run with --self-test; this contract does not capture browser evidence.");
  }
  console.log(
    `[qa-004] system Demo evidence contract: passed ${JSON.stringify(
      runSystemDemoEvidenceContractSelfTest()
    )}`
  );
}
