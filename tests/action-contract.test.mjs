import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { resolve } from "node:path";

import {
  fnv1a32,
  loadActionRegistry,
  renderActionContract
} from "../tools/generate-action-contract.mjs";

const root = resolve(import.meta.dirname, "..");
const generatedPath = resolve(
  root,
  "src/contracts/include/tgd/contracts/action_registry.generated.hpp"
);

test("Action Registry 生成的 C++ 合同保持同步", async () => {
  const registry = await loadActionRegistry();
  const actual = await readFile(generatedPath, "utf8");
  assert.equal(actual, renderActionContract(registry));
  assert.equal(registry.contexts.length, 5);
  assert.equal(registry.actions.length, 26);
});

test("Action 与 Context 的稳定哈希在各自命名空间内无碰撞", async () => {
  const registry = await loadActionRegistry();
  const actionHashes = registry.actions.map((action) => fnv1a32(action.id));
  const contextHashes = registry.contexts.map((context) => fnv1a32(context.id));
  assert.equal(new Set(actionHashes).size, actionHashes.length);
  assert.equal(new Set(contextHashes).size, contextHashes.length);
});
