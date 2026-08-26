import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { resolve } from "node:path";

import { validateToolchainLock } from "../tools/verify-toolchain.mjs";
import { buildToolchainEnvironment, lockedMsvcToolsetVersion } from "../tools/run-toolchain.mjs";

const root = resolve(import.meta.dirname, "..");
const lock = JSON.parse(await readFile(resolve(root, "toolchains/toolchain-lock.json"), "utf8"));
const baseline = JSON.parse(await readFile(resolve(root, "content/design/technical-baseline.json"), "utf8"));

test("validated toolchain lock is exact and synchronized with the technical baseline", () => {
  assert.deepEqual(validateToolchainLock(lock, baseline), []);
});

test("toolchain lock rejects floating versions", () => {
  const floating = structuredClone(lock);
  floating.tools.cmake.version = "latest";
  assert.match(validateToolchainLock(floating, baseline).join("\n"), /禁止使用 latest/);
});

test("verified artifacts require a SHA-256", () => {
  const unverified = structuredClone(lock);
  unverified.tools.clang.integrity = "pending:sha256-after-download";
  unverified.tools.clang.integrityScope = "pending";
  unverified.tools.clang.artifactVerified = true;
  assert.match(validateToolchainLock(unverified, baseline).join("\n"), /没有 SHA-256/);
});

test("installed executable variants remain an exact SHA-256 allowlist", () => {
  assert.deepEqual(lock.tools.msvc.integrityAlternates, [
    "sha256:9beec04038c74406e4c055593edc07ddda7b166272d77cbf85507d5a6be29ff0"
  ]);
  const malformed = structuredClone(lock);
  malformed.tools.msvc.integrityAlternates = ["sha256:unknown"];
  assert.match(validateToolchainLock(malformed, baseline).join("\n"), /无效 SHA-256/);
});

test("ART-003 clean runners lock both the Chromium archive and installed executable", async () => {
  const chromium = lock.supportArtifacts.art003Chromium;
  assert.deepEqual(
    {
      version: chromium.version,
      archive: chromium.integrity,
      bytes: chromium.provenance.expectedSizeBytes,
      installedPath: chromium.installedPath,
      executable: chromium.installedIntegrity
    },
    {
      version: "148.0.7778.96-revision-1223-win64",
      archive: "sha256:6cd8c5f6eeff7b6515dbcd35d050f696da3f7eac4b4a209c06e3d4354c2704a6",
      bytes: 190733461,
      installedPath: "chrome-win64/chrome.exe",
      executable: "sha256:290fa7018fda22c52ada5eddb0113baf3ebc41fd0fde6085eddb19793606c635"
    }
  );

  const missingPair = structuredClone(lock);
  delete missingPair.supportArtifacts.art003Chromium.installedIntegrity;
  assert.match(validateToolchainLock(missingPair, baseline).join("\n"), /必须成对出现/);

  const workflow = await readFile(resolve(root, ".github/workflows/f1-clean-build.yml"), "utf8");
  assert.match(workflow, /SYSTEM_SANDBOX_CHROMIUM_EXECUTABLE:.*chromium-1223.*chrome\.exe/);
  assert.match(workflow, /Check out the exact revision[\s\S]*?fetch-depth:\s*0/);
});

test("toolchain runner folds duplicate Windows PATH keys", () => {
  const environment = buildToolchainEnvironment(root, lock, {
    Path: "C:\\first",
    PATH: "C:\\duplicate",
    ComSpec: "C:\\Windows\\System32\\cmd.exe"
  });
  const pathKeys = Object.keys(environment).filter((name) => name.toLowerCase() === "path");
  assert.deepEqual(pathKeys, ["PATH"]);
  assert.match(environment.PATH, /cmake-4\.3\.1-windows-x86_64/);
  assert.equal(environment.EMSDK_KEEP_DOWNLOADS, "1");
});

test("toolchain runner selects the exact locked MSVC directory", () => {
  assert.equal(lockedMsvcToolsetVersion(lock.tools.msvc), "14.43.34808");
  assert.equal(
    lock.tools.msvc.componentId,
    "Microsoft.VisualStudio.Component.VC.14.43.17.13.x86.x64"
  );
  assert.throws(
    () => lockedMsvcToolsetVersion({ executable: "vswhere:VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe" }),
    /exact toolset directory/
  );
});

test("toolchain bootstrap hashes without PowerShell module autoloading", async () => {
  const bootstrap = await readFile(resolve(root, "tools/bootstrap-toolchain.ps1"), "utf8");
  assert.match(bootstrap, /System\.Security\.Cryptography\.SHA256/);
  assert.doesNotMatch(bootstrap, /Get-FileHash/);
  assert.match(bootstrap, /function ConvertTo-GpgPath/);
  assert.match(bootstrap, /return "\/\$drive\/\$tail"/);
  assert.match(bootstrap, /supportArtifacts\.art003Chromium/);
  assert.match(bootstrap, /verified installed artifact/);
});
