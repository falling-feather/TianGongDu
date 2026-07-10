import assert from "node:assert/strict";
import test from "node:test";

import { validateProject } from "../tools/validate-design.mjs";

test("1.0 设计目录、内容预算与引用关系完整", async () => {
  const result = await validateProject();
  assert.deepEqual(result.errors, []);
  assert.equal(result.valid, true);
});

test("1.0 内容规模达到承诺下限", async () => {
  const { summary } = await validateProject();
  assert.equal(summary.combatSystems, 2);
  assert.equal(summary.weapons, 2);
  assert.equal(summary.regions, 3);
  assert.equal(summary.subregions, 18);
  assert.equal(summary.bosses, 14);
  assert.equal(summary.npcs, 24);
  assert.equal(summary.templates, 8);
  assert.equal(summary.coreLanguage, "C++20");
  assert.equal(summary.engine, "Axmol");
  assert.equal(summary.webTarget, "wasm32");
  assert.equal(summary.cloudSync, true);
  assert.equal(summary.activeArchitectureDecisions, 4);
  assert.equal(summary.platformTargets, 4);
  assert.equal(summary.architectureModules, 11);
  assert.equal(summary.mainMinutes, 945);
});
