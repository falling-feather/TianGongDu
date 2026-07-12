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
  const flowerLight = contract.combatBootstrap.abilities.find(
    (ability) => ability.id === "ability_flower_light"
  );
  assert.equal(flowerLight.healthDamage, 30);
  assert.equal(
    flowerLight.windupTicks + flowerLight.activeTicks + flowerLight.recoveryTicks,
    18
  );
  assert.equal(contract.questInteractions.length, 4);
  assert.deepEqual(
    new Set(
      contract.questInteractions
        .filter((interaction) => contract.beats[0].objectiveIds.includes(interaction.objectiveId))
        .map((interaction) => interaction.objectiveId)
    ),
    new Set(contract.beats[0].objectiveIds)
  );
  assert.deepEqual(
    new Set(contract.questCombatTriggers.map((trigger) => trigger.objectiveId)),
    new Set(contract.beats[1].objectiveIds.slice(1))
  );
  assert.deepEqual(
    new Set(contract.questCombatOutcomes.map((outcome) => outcome.objectiveId)),
    new Set(contract.beats[2].objectiveIds.slice(0, 2))
  );
  assert.deepEqual(
    contract.questInteractions.find(
      (interaction) => interaction.objectiveId === "f1_objective_choose_lane_route"
    ).prerequisiteObjectiveIds,
    contract.beats[2].objectiveIds.slice(0, 2)
  );
  assert.equal(
    contract.questInteractions.find(
      (interaction) => interaction.objectiveId === "f1_objective_choose_lane_route"
    ).selectionId,
    "f1_choice_lane_canopy"
  );
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
    ...contract.beats.flatMap((beat) => [beat.id, ...beat.objectiveIds]),
    ...contract.questInteractions.map((interaction) => interaction.id),
    ...contract.questCombatTriggers.map((trigger) => trigger.id),
    ...contract.questCombatOutcomes.map((outcome) => outcome.id),
    ...contract.questInteractions.map((interaction) => interaction.selectionId).filter(Boolean)
  ];
  assert.equal(new Set(ids.map((id) => fnv1a64(id))).size, ids.length);
});

test("F1 opening objectives require valid content-driven scene interactions", async () => {
  const unknownObjective = structuredClone(await loadF1SliceContract());
  unknownObjective.questInteractions[0].objectiveId = "f1_objective_missing";
  assert.throws(
    () => validateF1SliceContract(unknownObjective, catalog),
    /unknown objective/
  );

  const duplicateObjective = structuredClone(await loadF1SliceContract());
  duplicateObjective.questInteractions[1].objectiveId =
    duplicateObjective.questInteractions[0].objectiveId;
  assert.throws(
    () => validateF1SliceContract(duplicateObjective, catalog),
    /duplicate non-choice quest interaction objective/
  );

  const futurePrerequisite = structuredClone(await loadF1SliceContract());
  futurePrerequisite.questInteractions[0].prerequisiteObjectiveIds = [
    "f1_objective_choose_resolution"
  ];
  assert.throws(
    () => validateF1SliceContract(futurePrerequisite, catalog),
    /invalid prerequisite objective/
  );

  const missingSelection = structuredClone(await loadF1SliceContract());
  missingSelection.questInteractions.at(-1).selectionId = null;
  assert.throws(
    () => validateF1SliceContract(missingSelection, catalog),
    /requires a selection id/
  );
});

test("F1 training counters require valid content-driven combat triggers", async () => {
  const unknownStance = structuredClone(await loadF1SliceContract());
  unknownStance.questCombatTriggers[0].requiredStanceId = "stance_missing";
  assert.throws(
    () => validateF1SliceContract(unknownStance, catalog),
    /unknown required stance/
  );

  const duplicateObjective = structuredClone(await loadF1SliceContract());
  duplicateObjective.questCombatTriggers[1].objectiveId =
    duplicateObjective.questCombatTriggers[0].objectiveId;
  assert.throws(
    () => validateF1SliceContract(duplicateObjective, catalog),
    /duplicate quest combat trigger objective/
  );
});

test("F1 umbrella-lane outcomes require reachable hostile groups", async () => {
  const impossibleCount = structuredClone(await loadF1SliceContract());
  impossibleCount.questCombatOutcomes[0].requiredCount = 3;
  assert.throws(
    () => validateF1SliceContract(impossibleCount, catalog),
    /cannot reach its required hostile count/
  );

  const duplicateObjective = structuredClone(await loadF1SliceContract());
  duplicateObjective.questCombatOutcomes[1].objectiveId =
    duplicateObjective.questCombatOutcomes[0].objectiveId;
  assert.throws(
    () => validateF1SliceContract(duplicateObjective, catalog),
    /duplicate quest combat outcome objective/
  );
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
