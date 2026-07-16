import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import { inspectSource, validateArchitecture } from "../tools/check-architecture.mjs";

test("当前 C++ 源码遵守层级 include 边界", async () => {
  assert.deepEqual(await validateArchitecture(), []);
});

test("架构 lint 会拒绝 Gameplay 私自 include Axmol", async () => {
  const fixturePath = new URL("./fixtures/architecture/gameplay-includes-axmol.cpp", import.meta.url);
  const fixture = await readFile(fixturePath, "utf8");
  const errors = inspectSource("gameplay", fixture, "gameplay-includes-axmol.cpp");
  assert.equal(errors.length, 1);
  assert.match(errors[0], /cannot expose\/include external dependency: axmol\.h/);
});

test("Sandbox integration 只组合 Contracts、Content 与 Gameplay", () => {
  const source = `
#include <tgd/contracts/sandbox_pack.hpp>
#include <tgd/content/sandbox_package.hpp>
#include <tgd/gameplay/sandbox_session.hpp>
#include <tgd/integration/sandbox_session_adapter.hpp>
`;
  assert.deepEqual(
    inspectSource("sandbox_integration", source, "src/sandbox-integration/src/allowed.cpp"),
    [],
  );
});

test("Sandbox integration 拒绝 Runtime、Platform、Presentation 与 Sync 直连", () => {
  const source = `
#include <tgd/runtime/game_session.hpp>
#include <tgd/platform/web_platform_bridge.hpp>
#include <tgd/presentation/presentation_lifecycle.hpp>
#include <tgd/sync/save_sync.hpp>
`;
  const errors = inspectSource(
    "sandbox_integration",
    source,
    "src/sandbox-integration/src/forbidden.cpp",
  );
  assert.equal(errors.length, 4);
  assert.match(errors[0], /cannot include project layer runtime/);
  assert.match(errors[1], /cannot include project layer platform/);
  assert.match(errors[2], /cannot include project layer presentation/);
  assert.match(errors[3], /cannot include project layer sync/);
});
