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
