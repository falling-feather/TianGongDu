import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { createReadStream } from "node:fs";
import { mkdir, readFile, stat, writeFile } from "node:fs/promises";
import { relative, resolve, sep } from "node:path";

const root = resolve(import.meta.dirname, "..");
const browserReportPath = resolve(
  root,
  process.env.TGD_BROWSER_REPORT ?? "build/browser-qa/report.json"
);

function projectPath(path) {
  return relative(root, path).split(sep).join("/");
}

async function hashFile(path) {
  const hash = createHash("sha256");
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  return hash.digest("hex");
}

async function artifact(path) {
  const metadata = await stat(path);
  assert(metadata.isFile(), `Evidence artifact is not a file: ${projectPath(path)}`);
  assert(metadata.size > 0, `Evidence artifact is empty: ${projectPath(path)}`);
  return {
    path: projectPath(path),
    sizeBytes: metadata.size,
    sha256: await hashFile(path)
  };
}

const browserReport = JSON.parse(await readFile(browserReportPath, "utf8"));
const requiredBrowsers = ["chrome", "edge", "firefox"];
assert.deepEqual(
  [...browserReport.targets].sort(),
  [...requiredBrowsers].sort(),
  "G1 browser evidence must contain Chrome, Edge, and Firefox in one report."
);
assert.equal(browserReport.results.length, requiredBrowsers.length);
for (const result of browserReport.results) {
  assert.equal(result.status, "passed", `${result.target} browser evidence did not pass.`);
  assert.deepEqual(result.consoleProblems, [], `${result.target} had console problems.`);
  assert.deepEqual(result.pageErrors, [], `${result.target} had page errors.`);
  assert.deepEqual(result.requestFailures, [], `${result.target} had request failures.`);
  assert.equal(result.checkpoints.length, 8, `${result.target} lacks lifecycle checkpoints.`);
  assert(
    result.frameComparison.changedPixelRatio <= 0.02,
    `${result.target} did not restore the bootstrap frame.`
  );
}

const embeddedCommits = new Set(
  browserReport.results.map((result) => result.embeddedIdentity.commit)
);
assert.equal(embeddedCommits.size, 1, "Browser variants embedded different Git commits.");
const embeddedCommit = [...embeddedCommits][0];
const expectedCommit =
  process.env.TGD_EXPECT_COMMIT?.trim() ||
  process.env.GITHUB_SHA?.trim() ||
  embeddedCommit;
assert.equal(embeddedCommit, expectedCommit, "Browser evidence is not bound to the expected commit.");

const buildId =
  process.env.TGD_BUILD_ID?.trim() ||
  `local-g1-${embeddedCommit.slice(0, 12)}`;
assert(
  /^[A-Za-z0-9._-]+$/.test(buildId),
  "TGD_BUILD_ID may contain only letters, numbers, dot, underscore, and hyphen."
);

const artifactPaths = [
  "build/web-debug/dist/web/tiangongdu-f1.html",
  "build/web-debug/dist/web/tiangongdu-f1.js",
  "build/web-debug/dist/web/tiangongdu-f1.wasm",
  "build/web-release-single/dist/web/tiangongdu-f1.html",
  "build/web-release-single/dist/web/tiangongdu-f1.js",
  "build/web-release-single/dist/web/tiangongdu-f1.wasm",
  "build/windows-msvc-debug/tests/native/Debug/tgd_native_bootstrap_smoke.exe",
  "build/windows-msvc-debug/tests/native/Release/tgd_native_bootstrap_smoke.exe",
  "build/windows-clang-debug/tests/native/tgd_native_bootstrap_smoke.exe",
  "build/windows-clang-release/tests/native/tgd_native_bootstrap_smoke.exe"
].map((path) => resolve(root, path));

const toolchainLockPath = resolve(root, "toolchains/toolchain-lock.json");
const packageLockPath = resolve(root, "package-lock.json");
const manifest = {
  schemaVersion: "1.0.0",
  buildId,
  result: "passed",
  commit: embeddedCommit,
  version: browserReport.results[0].embeddedIdentity.version,
  channel: browserReport.results[0].embeddedIdentity.channel,
  createdAt: new Date().toISOString(),
  source: {
    repository: process.env.GITHUB_REPOSITORY ?? null,
    ref: process.env.GITHUB_REF ?? null,
    workflowRunId: process.env.GITHUB_RUN_ID ?? null,
    workflowRunAttempt: process.env.GITHUB_RUN_ATTEMPT ?? null,
    workflowUrl:
      process.env.GITHUB_SERVER_URL &&
      process.env.GITHUB_REPOSITORY &&
      process.env.GITHUB_RUN_ID
        ? `${process.env.GITHUB_SERVER_URL}/${process.env.GITHUB_REPOSITORY}/actions/runs/${process.env.GITHUB_RUN_ID}`
        : null
  },
  environment: {
    ci: process.env.CI === "true",
    os: process.platform,
    architecture: process.arch,
    node: process.version,
    runnerName: process.env.RUNNER_NAME ?? null,
    runnerImage: process.env.ImageOS ?? null,
    runnerImageVersion: process.env.ImageVersion ?? null
  },
  locks: {
    toolchain: await artifact(toolchainLockPath),
    npm: await artifact(packageLockPath)
  },
  browserMatrix: browserReport.results.map((result) => ({
    target: result.target,
    browserVersion: result.browserVersion,
    checkpoints: result.checkpoints,
    expectedConsoleDiagnostics: result.expectedConsoleDiagnostics,
    frameComparison: result.frameComparison,
    beforeFrame: result.beforeFrame,
    afterFrame: result.afterFrame,
    screenshots: result.screenshots
  })),
  browserReport: await artifact(browserReportPath),
  artifacts: await Promise.all(artifactPaths.map(artifact)),
  verifiedCommands: [
    "npm test",
    "npm run validate:toolchain:cache",
    "npm run test:cpp:msvc",
    "npm run test:cpp:msvc:release",
    "npm run test:cpp:clang",
    "npm run test:cpp:clang:release",
    "npm run build:web:debug",
    "npm run build:web:release",
    "npm run test:browser -- --targets=chrome,edge,firefox"
  ],
  limits: [
    "The geometric scene is a host/lifecycle probe, not the final ADR-0009 2.5D art-direction slice.",
    "Web Single is the required baseline; Pthreads remains a conditional G3 spike.",
    "This G1 matrix covers desktop Chrome, Edge, and Firefox on the recorded Windows runner."
  ]
};

const evidenceDirectory = resolve(root, "build/evidence", buildId);
await mkdir(evidenceDirectory, { recursive: true });
const manifestPath = resolve(evidenceDirectory, "manifest.json");
await writeFile(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
console.log(`G1 evidence manifest: ${projectPath(manifestPath)}`);
