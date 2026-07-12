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
  assert.equal(contract.timing.activityGraceTicks, 180);
  assert.equal(contract.view.primaryGuidance, "douzhanshen");
  assert.equal(contract.beats.length, 7);
  assert.equal(contract.safePoints.length, 7);
  assert.equal(contract.combatBootstrap.actors.length, 8);
  const springReturnUmbrella = contract.combatBootstrap.actors.find(
    (actor) => actor.actorKey === 106
  );
  assert.equal(springReturnUmbrella.archetypeId, "jn_enemy_leaking_umbrella_doll");
  assert.equal(springReturnUmbrella.resources.health, 70);
  assert.equal(springReturnUmbrella.initiallyActive, false);
  assert.equal(contract.combatBootstrap.abilities.length, 17);
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
  assert.equal(contract.questInteractions.length, 23);
  assert.deepEqual(
    contract.beats[0].objectiveIds,
    [
      "f1_objective_inspect_travel_writ",
      "f1_objective_read_flood_marks",
      "f1_objective_secure_ferry_mooring",
      "f1_objective_raise_wayfinding_lantern",
      "f1_objective_sound_workshop_bell",
      "f1_objective_reach_ferry_gate"
    ]
  );
  assert.deepEqual(
    new Set(
      contract.questInteractions
        .filter((interaction) => contract.beats[0].objectiveIds.includes(interaction.objectiveId))
        .map((interaction) => interaction.objectiveId)
    ),
    new Set(contract.beats[0].objectiveIds)
  );
  assert.deepEqual(
    new Set(
      contract.questCombatTriggers
        .filter((trigger) => contract.beats[1].objectiveIds.includes(trigger.objectiveId))
        .map((trigger) => trigger.objectiveId)
    ),
    new Set(contract.beats[1].objectiveIds.slice(1))
  );
  assert.equal(contract.questCombatTriggers.length, 7);
  assert.equal(
    contract.questCombatTriggers[0].requiredAbilityId,
    "ability_eavesguard_heavy"
  );
  assert.equal(
    contract.questCombatTriggers[3].requiredAbilityId,
    "ability_flower_light"
  );
  assert.deepEqual(
    contract.questCombatTriggers.slice(5).map((trigger) => ({
      objectiveId: trigger.objectiveId,
      requiredAbilityId: trigger.requiredAbilityId,
      requiredSelectionId: trigger.requiredSelectionId
    })),
    [
      {
        objectiveId: contract.beats[4].objectiveIds[1],
        requiredAbilityId: "ability_eavesguard_heavy",
        requiredSelectionId: "f1_choice_rib_spring_calibration"
      },
      {
        objectiveId: contract.beats[4].objectiveIds[1],
        requiredAbilityId: "ability_flower_light",
        requiredSelectionId: "f1_choice_rib_winter_calibration"
      }
    ]
  );
  assert.deepEqual(
    new Set(
      contract.questCombatOutcomes
        .filter((outcome) => contract.beats[2].objectiveIds.includes(outcome.objectiveId))
        .map((outcome) => outcome.objectiveId)
    ),
    new Set([contract.beats[2].objectiveIds[0], contract.beats[2].objectiveIds[4]])
  );
  const laneRoutes = contract.questInteractions.filter(
    (interaction) => interaction.objectiveId === "f1_objective_choose_lane_route"
  );
  assert.deepEqual(
    laneRoutes.map((interaction) => interaction.selectionId),
    ["f1_choice_lane_canopy", "f1_choice_lane_drain"]
  );
  assert(
    laneRoutes.every(
      (interaction) =>
        JSON.stringify(interaction.prerequisiteObjectiveIds) ===
        JSON.stringify(contract.beats[2].objectiveIds.slice(0, 5))
    )
  );
  assert.deepEqual(
    contract.questInteractions
      .filter((interaction) => interaction.objectiveId === contract.beats[3].objectiveIds[0])
      .map((interaction) => ({
        requiredSelectionObjectiveId: interaction.requiredSelectionObjectiveId,
        requiredSelectionId: interaction.requiredSelectionId
      })),
    [
      {
        requiredSelectionObjectiveId: contract.beats[2].objectiveIds[5],
        requiredSelectionId: "f1_choice_lane_canopy"
      },
      {
        requiredSelectionObjectiveId: contract.beats[2].objectiveIds[5],
        requiredSelectionId: "f1_choice_lane_drain"
      }
    ]
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
    ...contract.safePoints.map((safePoint) => safePoint.id),
    ...contract.questInteractions.map((interaction) => interaction.id),
    ...contract.questCombatTriggers.map((trigger) => trigger.id),
    ...contract.questCombatOutcomes.map((outcome) => outcome.id),
    ...contract.questEncounterActivations.map((activation) => activation.id),
    ...contract.questBossPhases.map((phase) => phase.id),
    ...contract.questResolutionRewards.flatMap((reward) => [
      reward.id,
      reward.rewardId,
      reward.rewardDedupKey
    ]),
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
  duplicateObjective.questInteractions.find(
    (interaction) => interaction.id === "f1_interaction_ferry_gate"
  ).objectiveId = duplicateObjective.questInteractions[0].objectiveId;
  assert.throws(
    () => validateF1SliceContract(duplicateObjective, catalog),
    /duplicate non-choice quest interaction objective/
  );

  const brokenReadinessChain = structuredClone(await loadF1SliceContract());
  brokenReadinessChain.questInteractions.find(
    (interaction) => interaction.id === "f1_interaction_ferry_gate"
  ).prerequisiteObjectiveIds = [];
  assert.throws(
    () => validateF1SliceContract(brokenReadinessChain, catalog),
    /Rain Ferry readiness chain drifted/
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
  missingSelection.questInteractions.find(
    (interaction) => interaction.kind === "choose"
  ).selectionId = null;
  assert.throws(
    () => validateF1SliceContract(missingSelection, catalog),
    /requires a selection id/
  );
});

test("F1 route owns one ordered content-driven safe point per beat", async () => {
  const contract = await loadF1SliceContract();
  assert.deepEqual(
    contract.safePoints.map((safePoint) => ({
      id: safePoint.id,
      beatId: safePoint.beatId,
      x: safePoint.poseMm.x,
      y: safePoint.poseMm.y
    })),
    [
      ["f1_safe_point_rain_ferry_arrival", -12000, -1600],
      ["f1_safe_point_shen_yan_training", -10500, -600],
      ["f1_safe_point_umbrella_lane", -5600, -1200],
      ["f1_safe_point_shared_workbench", -4300, -100],
      ["f1_safe_point_canopy_return", -4300, -100],
      ["f1_safe_point_four_seasons_court", 2200, 800],
      ["f1_safe_point_resolution_return", 3000, 800]
    ].map(([id, x, y], index) => ({
      id,
      beatId: contract.beats[index].id,
      x,
      y
    }))
  );

  const missing = structuredClone(contract);
  missing.safePoints.pop();
  assert.throws(
    () => validateF1SliceContract(missing, catalog),
    /every playable beat must own exactly one safe point/
  );

  const outOfOrder = structuredClone(contract);
  [outOfOrder.safePoints[1], outOfOrder.safePoints[2]] =
    [outOfOrder.safePoints[2], outOfOrder.safePoints[1]];
  assert.throws(
    () => validateF1SliceContract(outOfOrder, catalog),
    /not valid for its authored beat/
  );
});

test("F1 workbench investigation gates two stable calibration choices", async () => {
  const contract = await loadF1SliceContract();
  const workbench = contract.beats[3];
  const calibration = contract.questInteractions.filter(
    (interaction) => interaction.objectiveId === workbench.objectiveIds[3]
  );
  assert.deepEqual(
    calibration.map((interaction) => interaction.selectionId).sort(),
    ["f1_choice_rib_spring_calibration", "f1_choice_rib_winter_calibration"]
  );

  const missingEvidence = structuredClone(contract);
  const missingEvidenceCalibration = missingEvidence.questInteractions.find(
    (interaction) => interaction.id === "f1_interaction_calibrate_rib_spring"
  );
  missingEvidenceCalibration.prerequisiteObjectiveIds.pop();
  assert.throws(
    () => validateF1SliceContract(missingEvidence, catalog),
    /two stable choices after all workbench evidence/
  );

  const halfSelectionGate = structuredClone(contract);
  halfSelectionGate.questInteractions.find(
    (interaction) => interaction.id === "f1_interaction_reveal_spring_trace"
  ).requiredSelectionId = null;
  assert.throws(
    () => validateF1SliceContract(halfSelectionGate, catalog),
    /invalid interaction selection gate/
  );

  const futureSelectionGate = structuredClone(contract);
  const futureInteraction = futureSelectionGate.questInteractions.find(
    (interaction) => interaction.id === "f1_interaction_reveal_spring_trace"
  );
  futureInteraction.requiredSelectionObjectiveId = futureSelectionGate.beats[6].objectiveIds[0];
  futureInteraction.requiredSelectionId = "f1_choice_resolution_subdue";
  assert.throws(
    () => validateF1SliceContract(futureSelectionGate, catalog),
    /missing or future interaction selection gate/
  );

  const duplicateSelectionGate = structuredClone(contract);
  duplicateSelectionGate.questInteractions.find(
    (interaction) => interaction.id === "f1_interaction_reveal_spring_trace_from_drain"
  ).requiredSelectionId = "f1_choice_lane_canopy";
  assert.throws(
    () => validateF1SliceContract(duplicateSelectionGate, catalog),
    /duplicate .* interaction selection gate/
  );

  const incompleteSelectionCoverage = structuredClone(contract);
  incompleteSelectionCoverage.questInteractions = incompleteSelectionCoverage.questInteractions.filter(
    (interaction) => interaction.id !== "f1_interaction_reveal_spring_trace_from_drain"
  );
  assert.throws(
    () => validateF1SliceContract(incompleteSelectionCoverage, catalog),
    /interactions do not cover every authored selection option/
  );
});

test("F1 combat actions require valid content-driven and selection-gated triggers", async () => {
  const unknownStance = structuredClone(await loadF1SliceContract());
  unknownStance.questCombatTriggers[0].requiredStanceId = "stance_missing";
  assert.throws(
    () => validateF1SliceContract(unknownStance, catalog),
    /unknown required stance/
  );

  const duplicateObjective = structuredClone(await loadF1SliceContract());
  duplicateObjective.questCombatTriggers[1] = {
    ...duplicateObjective.questCombatTriggers[0],
    id: "f1_trigger_duplicate_eavesguard_heavy"
  };
  assert.throws(
    () => validateF1SliceContract(duplicateObjective, catalog),
    /duplicate unconditional combat triggers/
  );

  const incompatibleAbility = structuredClone(await loadF1SliceContract());
  incompatibleAbility.questCombatTriggers[0].requiredAbilityId = "ability_flower_light";
  assert.throws(
    () => validateF1SliceContract(incompatibleAbility, catalog),
    /incompatible required ability/
  );

  const missingPrerequisite = structuredClone(await loadF1SliceContract());
  missingPrerequisite.questCombatTriggers[0].prerequisiteObjectiveIds = [];
  assert.throws(
    () => validateF1SliceContract(missingPrerequisite, catalog),
    /invalid prerequisite objectives/
  );

  const halfSelectionGate = structuredClone(await loadF1SliceContract());
  halfSelectionGate.questCombatTriggers[5].requiredSelectionId = null;
  assert.throws(
    () => validateF1SliceContract(halfSelectionGate, catalog),
    /invalid combat trigger selection gate/
  );

  const futureSelectionGate = structuredClone(await loadF1SliceContract());
  futureSelectionGate.questCombatTriggers[5].requiredSelectionObjectiveId =
    futureSelectionGate.beats[6].objectiveIds[0];
  futureSelectionGate.questCombatTriggers[5].requiredSelectionId =
    "f1_choice_resolution_subdue";
  assert.throws(
    () => validateF1SliceContract(futureSelectionGate, catalog),
    /missing or future combat trigger selection gate/
  );

  const duplicateSelectionGate = structuredClone(await loadF1SliceContract());
  duplicateSelectionGate.questCombatTriggers[6].requiredSelectionId =
    "f1_choice_rib_spring_calibration";
  assert.throws(
    () => validateF1SliceContract(duplicateSelectionGate, catalog),
    /duplicate .* combat trigger selection gate/
  );

  const incompleteSelectionCoverage = structuredClone(await loadF1SliceContract());
  incompleteSelectionCoverage.questCombatTriggers.splice(6, 1);
  assert.throws(
    () => validateF1SliceContract(incompleteSelectionCoverage, catalog),
    /do not cover every authored selection option/
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

test("F1 encounters activate authored groups at beat and objective boundaries", async () => {
  const contract = await loadF1SliceContract();
  assert.deepEqual(contract.questEncounterActivations, [
    {
      id: "f1_activation_shen_yan_training_rigs",
      beatId: contract.beats[1].id,
      triggerObjectiveId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      mode: "replace",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [104],
      actorPlacements: [
        {
          actorKey: 104,
          poseMm: { x: -5900, y: 2300, height: 0, floorLayer: 0 },
          formationSlot: 0
        }
      ]
    },
    {
      id: "f1_activation_shen_yan_flower_turn_rig",
      beatId: contract.beats[1].id,
      triggerObjectiveId: contract.beats[1].objectiveIds[2],
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      mode: "replace",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [105],
      actorPlacements: [
        {
          actorKey: 105,
          poseMm: { x: -5200, y: -1600, height: 700, floorLayer: 0 },
          formationSlot: 2
        }
      ]
    },
    {
      id: "f1_activation_umbrella_lane_first_encounter",
      beatId: contract.beats[2].id,
      triggerObjectiveId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      mode: "replace",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [101, 102],
      actorPlacements: [
        {
          actorKey: 101,
          poseMm: { x: -4000, y: -2600, height: 0, floorLayer: 0 },
          formationSlot: 1
        },
        {
          actorKey: 102,
          poseMm: { x: -3000, y: -400, height: 0, floorLayer: 0 },
          formationSlot: 5
        }
      ]
    },
    {
      id: "f1_activation_umbrella_lane_paper_egret",
      beatId: contract.beats[2].id,
      triggerObjectiveId: contract.beats[2].objectiveIds[3],
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      mode: "replace",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [103],
      actorPlacements: [
        {
          actorKey: 103,
          poseMm: { x: -1500, y: 900, height: 700, floorLayer: 0 },
          formationSlot: 2
        }
      ]
    },
    {
      id: "f1_activation_canopy_return_encounter",
      beatId: contract.beats[4].id,
      triggerObjectiveId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      mode: "replace",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [101, 102, 103],
      actorPlacements: [
        {
          actorKey: 101,
          poseMm: { x: -2500, y: -1800, height: 0, floorLayer: 0 },
          formationSlot: 0
        },
        {
          actorKey: 102,
          poseMm: { x: -900, y: -300, height: 0, floorLayer: 0 },
          formationSlot: 3
        },
        {
          actorKey: 103,
          poseMm: { x: -500, y: 1700, height: 700, floorLayer: 0 },
          formationSlot: 6
        }
      ]
    },
    {
      id: "f1_activation_canopy_return_spring_reinforcement",
      beatId: contract.beats[4].id,
      triggerObjectiveId: contract.beats[4].objectiveIds[0],
      requiredSelectionObjectiveId: contract.beats[3].objectiveIds[3],
      requiredSelectionId: "f1_choice_rib_spring_calibration",
      mode: "reinforce",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [106],
      actorPlacements: [
        {
          actorKey: 106,
          poseMm: { x: 500, y: 1400, height: 0, floorLayer: 0 },
          formationSlot: 5
        }
      ]
    },
    {
      id: "f1_activation_canopy_return_winter_reinforcement",
      beatId: contract.beats[4].id,
      triggerObjectiveId: contract.beats[4].objectiveIds[0],
      requiredSelectionObjectiveId: contract.beats[3].objectiveIds[3],
      requiredSelectionId: "f1_choice_rib_winter_calibration",
      mode: "reinforce",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [105],
      actorPlacements: [
        {
          actorKey: 105,
          poseMm: { x: 500, y: 1400, height: 700, floorLayer: 0 },
          formationSlot: 5
        }
      ]
    },
    {
      id: "f1_activation_four_seasons_wraith",
      beatId: contract.beats[5].id,
      triggerObjectiveId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      mode: "replace",
      encounterId: contract.combatBootstrap.id,
      actorKeys: [201],
      actorPlacements: [
        {
          actorKey: 201,
          poseMm: { x: 4000, y: 1900, height: 0, floorLayer: 0 },
          formationSlot: 4
        }
      ]
    }
  ]);
  assert.equal(
    contract.questCombatOutcomes.find(
      (outcome) => outcome.objectiveId === contract.beats[4].objectiveIds[2]
    ).kind,
    "all_hostiles_defeated"
  );

  const wrongBeat = structuredClone(contract);
  wrongBeat.questEncounterActivations[0].beatId = contract.beats[3].id;
  assert.throws(
    () => validateF1SliceContract(wrongBeat, catalog),
    /training waves, lane waves, choice-driven return reinforcements, and boss beats must own/
  );

  const stageEntryReinforcement = structuredClone(contract);
  stageEntryReinforcement.questEncounterActivations[4].mode = "reinforce";
  assert.throws(
    () => validateF1SliceContract(stageEntryReinforcement, catalog),
    /invalid activation mode or stage-entry reinforcement/
  );

  const halfSelectionGate = structuredClone(contract);
  halfSelectionGate.questEncounterActivations[5].requiredSelectionId = null;
  assert.throws(
    () => validateF1SliceContract(halfSelectionGate, catalog),
    /invalid selection gate/
  );

  const futureSelectionGate = structuredClone(contract);
  futureSelectionGate.questEncounterActivations[5].requiredSelectionObjectiveId =
    contract.beats[6].objectiveIds[0];
  futureSelectionGate.questEncounterActivations[5].requiredSelectionId =
    "f1_choice_resolution_subdue";
  assert.throws(
    () => validateF1SliceContract(futureSelectionGate, catalog),
    /missing or future selection gate/
  );

  const duplicateSelectionGate = structuredClone(contract);
  duplicateSelectionGate.questEncounterActivations[6].requiredSelectionId =
    "f1_choice_rib_spring_calibration";
  assert.throws(
    () => validateF1SliceContract(duplicateSelectionGate, catalog),
    /duplicate .* selection gate/
  );

  const incompleteSelectionCoverage = structuredClone(contract);
  incompleteSelectionCoverage.questEncounterActivations.splice(6, 1);
  assert.throws(
    () => validateF1SliceContract(incompleteSelectionCoverage, catalog),
    /does not cover every authored selection option/
  );

  const crossBeatTrigger = structuredClone(contract);
  crossBeatTrigger.questEncounterActivations[1].triggerObjectiveId =
    contract.beats[2].objectiveIds[0];
  assert.throws(
    () => validateF1SliceContract(crossBeatTrigger, catalog),
    /references an objective outside its beat/
  );

  const misplacedActor = structuredClone(contract);
  misplacedActor.questEncounterActivations[2].actorPlacements[0].actorKey = 103;
  assert.throws(
    () => validateF1SliceContract(misplacedActor, catalog),
    /invalid ordered actor placement/
  );

  const duplicateFormationSlot = structuredClone(contract);
  duplicateFormationSlot.questEncounterActivations[2].actorPlacements[1].formationSlot =
    duplicateFormationSlot.questEncounterActivations[2].actorPlacements[0].formationSlot;
  assert.throws(
    () => validateF1SliceContract(duplicateFormationSlot, catalog),
    /duplicate .* formation slot/
  );

  const unreachableReturnReinforcement = structuredClone(contract);
  unreachableReturnReinforcement.questEncounterActivations[5].actorPlacements[0].poseMm = {
    x: 900,
    y: 2100,
    height: 700,
    floorLayer: 0
  };
  assert.throws(
    () => validateF1SliceContract(unreachableReturnReinforcement, catalog),
    /return encounter placements must engage from the authored safe point/
  );

  const leakedHostile = structuredClone(contract);
  leakedHostile.combatBootstrap.actors.find((actor) => actor.actorKey === 101).initiallyActive = true;
  assert.throws(
    () => validateF1SliceContract(leakedHostile, catalog),
    /beat-scoped hostile actors must start dormant/
  );
});

test("F1 four-seasons wraith is an inactive actor with ordered health phases", async () => {
  const contract = await loadF1SliceContract();
  const boss = contract.combatBootstrap.actors.find((actor) => actor.actorKey === 201);
  assert.equal(boss.archetypeId, contract.catalogReferences.bossId);
  assert.equal(boss.initiallyActive, false);
  assert.deepEqual(boss.stanceIds, [
    "stance_wraith_spring",
    "stance_wraith_summer",
    "stance_wraith_autumn",
    "stance_wraith_winter"
  ]);
  assert.deepEqual(
    contract.questBossPhases.map((phase) => ({
      objectiveId: phase.objectiveId,
      actorKey: phase.actorKey,
      healthPercent: phase.healthPercent,
      nextStanceId: phase.nextStanceId
    })),
    contract.beats[5].objectiveIds.map((objectiveId, index) => ({
      objectiveId,
      actorKey: 201,
      healthPercent: [75, 50, 25, 0][index],
      nextStanceId: [
        "stance_wraith_summer",
        "stance_wraith_autumn",
        "stance_wraith_winter",
        null
      ][index]
    }))
  );

  const outOfOrder = structuredClone(contract);
  outOfOrder.questBossPhases[1].healthPercent = 80;
  assert.throws(
    () => validateF1SliceContract(outOfOrder, catalog),
    /phase order or thresholds drifted/
  );
});

test("F1 resolution offers two choices, a gated return, and idempotent reward receipts", async () => {
  const contract = await loadF1SliceContract();
  const resolution = contract.beats[6];
  const choices = contract.questInteractions.filter(
    (interaction) => interaction.objectiveId === resolution.objectiveIds[0]
  );
  const returnToShenYan = contract.questInteractions.filter(
    (interaction) => interaction.objectiveId === resolution.objectiveIds[1]
  );
  assert.deepEqual(
    choices.map((interaction) => ({
      kind: interaction.kind,
      cellId: interaction.cellId,
      selectionId: interaction.selectionId,
      prerequisiteObjectiveIds: interaction.prerequisiteObjectiveIds
    })),
    [
      {
        kind: "choose",
        cellId: resolution.cellId,
        selectionId: "f1_choice_resolution_subdue",
        prerequisiteObjectiveIds: []
      },
      {
        kind: "choose",
        cellId: resolution.cellId,
        selectionId: "f1_choice_resolution_restore_shared_mark",
        prerequisiteObjectiveIds: []
      }
    ]
  );
  assert.equal(returnToShenYan.length, 1);
  assert.equal(returnToShenYan[0].kind, "talk");
  assert.equal(returnToShenYan[0].cellId, resolution.cellId);
  assert.deepEqual(returnToShenYan[0].prerequisiteObjectiveIds, [resolution.objectiveIds[0]]);
  assert.deepEqual(contract.questResolutionRewards, [
    {
      id: "f1_resolution_reward_subdue",
      objectiveId: resolution.objectiveIds[0],
      selectionId: "f1_choice_resolution_subdue",
      rewardId: "f1_reward_sealed_mixed_umbrella",
      rewardDedupKey: "f1_claim_resolution_subdue"
    },
    {
      id: "f1_resolution_reward_restore_shared_mark",
      objectiveId: resolution.objectiveIds[0],
      selectionId: "f1_choice_resolution_restore_shared_mark",
      rewardId: "f1_reward_joint_workshop_formula",
      rewardDedupKey: "f1_claim_resolution_restore_shared_mark"
    }
  ]);

  const duplicateDedup = structuredClone(contract);
  duplicateDedup.questResolutionRewards[1].rewardDedupKey =
    duplicateDedup.questResolutionRewards[0].rewardDedupKey;
  assert.throws(
    () => validateF1SliceContract(duplicateDedup, catalog),
    /duplicate resolution reward rewardDedupKey/
  );

  const driftedMapping = structuredClone(contract);
  driftedMapping.questResolutionRewards[0].rewardId = "f1_reward_wrong";
  assert.throws(
    () => validateF1SliceContract(driftedMapping, catalog),
    /reward receipt mappings drifted/
  );
});

test("F1 slice cannot auto-advance or shrink below one playable hour", async () => {
  const contract = structuredClone(await loadF1SliceContract());
  contract.beats[0].autoAdvance = true;
  assert.throws(() => validateF1SliceContract(contract, catalog), /must never advance/);

  const shortContract = structuredClone(await loadF1SliceContract());
  shortContract.beats[0].targetMinutes -= 1;
  assert.throws(() => validateF1SliceContract(shortContract, catalog), /expected 60/);

  const driftedGrace = structuredClone(await loadF1SliceContract());
  driftedGrace.timing.activityGraceTicks = 181;
  assert.throws(() => validateF1SliceContract(driftedGrace, catalog), /budgets are frozen/);
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
