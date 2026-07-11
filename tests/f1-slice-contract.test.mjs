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

test("F1 one-hour contract and generated C++ stay synchronized", async () => {
  const contract = await loadF1SliceContract();
  const generated = await readFile(generatedPath, "utf8");
  assert.equal(generated, renderF1SliceContract(contract));
  assert.equal(contract.beats.reduce((sum, beat) => sum + beat.targetMinutes, 0), 60);
  assert.equal(contract.timing.endToEndTestBudgetMinutes, 70);
  assert.equal(contract.view.primaryGuidance, "douzhanshen");
  assert.equal(contract.beats.length, 7);
  assert.equal(contract.combatBootstrap.actors.length, 4);
  assert.equal(contract.combatBootstrap.abilities.length, 9);
  assert.equal(contract.combatBootstrap.director.maxSimultaneousAttackers, 1);
  assert.equal(contract.combatBootstrap.director.formationRadiusMm, 1500);
  assert(
    contract.combatBootstrap.actors.every((actor) =>
      Object.values(actor.recovery).every((value) => Number.isInteger(value) && value > 0)
    )
  );
  assert.equal(contract.ports.filter((port) => port.status === "reserved").length, 4);
  assert.equal(contract.ports.filter((port) => port.status === "bootstrap_implemented").length, 5);
});

test("F1 stable content IDs have unique 64-bit keys", async () => {
  const contract = await loadF1SliceContract();
  const ids = [
    contract.id,
    ...contract.cellIds,
    ...contract.beats.flatMap((beat) => [beat.id, ...beat.objectiveIds])
  ];
  assert.equal(new Set(ids.map((id) => fnv1a64(id))).size, ids.length);
});

test("F1 slice cannot auto-advance or shrink below one playable hour", async () => {
  const contract = structuredClone(await loadF1SliceContract());
  contract.beats[0].autoAdvance = true;
  assert.throws(() => validateF1SliceContract(contract, catalog), /must never advance/);

  const shortContract = structuredClone(await loadF1SliceContract());
  shortContract.beats[0].targetMinutes -= 1;
  assert.throws(() => validateF1SliceContract(shortContract, catalog), /expected 60/);
});

test("F1 combat contract requires stance abilities and stance-neutral evade", async () => {
  const missingAbility = structuredClone(await loadF1SliceContract());
  missingAbility.combatBootstrap.abilities = missingAbility.combatBootstrap.abilities.filter(
    (ability) => ability.id !== "ability_flower_heavy"
  );
  assert.throws(() => validateF1SliceContract(missingAbility, catalog), /missing heavy_attack/);

  const stanceBoundEvade = structuredClone(await loadF1SliceContract());
  stanceBoundEvade.combatBootstrap.abilities.at(-1).requiredStanceId = "stance_eavesguard";
  assert.throws(() => validateF1SliceContract(stanceBoundEvade, catalog), /stance-neutral/);

  const fractionalChase = structuredClone(await loadF1SliceContract());
  fractionalChase.combatBootstrap.director.chaseSpeedMmPerSecond = 1801;
  assert.throws(() => validateF1SliceContract(fractionalChase, catalog), /director definition/);

  const zeroRecoveryInterval = structuredClone(await loadF1SliceContract());
  zeroRecoveryInterval.combatBootstrap.actors[0].recovery.staminaIntervalTicks = 0;
  assert.throws(
    () => validateF1SliceContract(zeroRecoveryInterval, catalog),
    /invalid recovery field/
  );
});
