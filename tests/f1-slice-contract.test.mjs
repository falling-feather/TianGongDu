import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import test from "node:test";

import {
  fnv1a64,
  loadF1SliceContract,
  renderF1SliceContract,
  validateF1SliceContract
} from "../tools/generate-f1-slice-contract.mjs";

const root = resolve(import.meta.dirname, "..");
const generatedPath = resolve(
  root,
  "src/content-core/include/tgd/content/f1_vertical_slice.generated.hpp"
);
const catalog = JSON.parse(
  await readFile(resolve(root, "content/design/v1-content-catalog.json"), "utf8")
);

test("F1 一小时纵切机器合同与生成的 C++ 定义保持同步", async () => {
  const contract = await loadF1SliceContract();
  const generated = await readFile(generatedPath, "utf8");
  assert.equal(generated, renderF1SliceContract(contract));
  assert.equal(contract.beats.reduce((sum, beat) => sum + beat.targetMinutes, 0), 60);
  assert.equal(contract.timing.endToEndTestBudgetMinutes, 70);
  assert.equal(contract.view.primaryGuidance, "douzhanshen");
  assert.equal(contract.beats.length, 7);
  assert.equal(contract.ports.filter((port) => port.status === "reserved").length, 8);
});

test("纵切稳定 ID 的 64 位键无碰撞", async () => {
  const contract = await loadF1SliceContract();
  const ids = [
    contract.id,
    ...contract.cellIds,
    ...contract.beats.flatMap((beat) => [beat.id, ...beat.objectiveIds])
  ];
  assert.equal(new Set(ids.map((id) => fnv1a64(id))).size, ids.length);
});

test("纵切不能用计时自动推进或缩水到一小时以下", async () => {
  const contract = structuredClone(await loadF1SliceContract());
  contract.beats[0].autoAdvance = true;
  assert.throws(() => validateF1SliceContract(contract, catalog), /must never advance/);

  const shortContract = structuredClone(await loadF1SliceContract());
  shortContract.beats[0].targetMinutes -= 1;
  assert.throws(() => validateF1SliceContract(shortContract, catalog), /expected 60/);
});
