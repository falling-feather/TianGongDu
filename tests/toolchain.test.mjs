import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { resolve } from "node:path";

import { validateToolchainLock } from "../tools/verify-toolchain.mjs";
import { buildToolchainEnvironment } from "../tools/run-toolchain.mjs";

const root = resolve(import.meta.dirname, "..");
const lock = JSON.parse(await readFile(resolve(root, "toolchains/toolchain-lock.json"), "utf8"));
const baseline = JSON.parse(await readFile(resolve(root, "content/design/technical-baseline.json"), "utf8"));

test("candidate toolchain lock is exact and synchronized with the technical baseline", () => {
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
