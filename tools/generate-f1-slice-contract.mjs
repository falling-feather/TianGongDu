import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const root = resolve(import.meta.dirname, "..");
const inputPath = resolve(root, "content/design/f1-vertical-slice.json");
const catalogPath = resolve(root, "content/design/v1-content-catalog.json");
const outputPath = resolve(
  root,
  "src/content-core/include/tgd/content/f1_vertical_slice.generated.hpp"
);
const encounterFormationSlotCapacity = 15;

function fail(message) {
  throw new Error(`F1 vertical slice contract: ${message}`);
}

export function fnv1a64(value) {
  let hash = 0xcbf29ce484222325n;
  for (const byte of Buffer.from(value, "utf8")) {
    hash ^= BigInt(byte);
    hash = BigInt.asUintN(64, hash * 0x100000001b3n);
  }
  return hash;
}

function assertUnique(values, label) {
  const seen = new Set();
  for (const value of values) {
    if (seen.has(value)) fail(`duplicate ${label}: ${value}`);
    seen.add(value);
  }
}

function assertHashUnique(values, label) {
  const seen = new Map();
  for (const value of values) {
    const hash = fnv1a64(value).toString(16);
    if (seen.has(hash)) fail(`${label} hash collision: ${seen.get(hash)} and ${value}`);
    seen.set(hash, value);
  }
}

function sameValues(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

export function validateF1SliceContract(contract, catalog) {
  if (contract.schemaVersion !== "1.6.0") fail("unsupported schemaVersion");
  const catalogSlice = catalog.f1VerticalSlice;
  if (contract.id !== catalogSlice.id) fail("slice id drifted from the 1.0 catalog");
  const expectedView = {
    model: "2.5d-oblique-panoramic",
    primaryGuidance: "douzhanshen",
    secondaryReference: "warm-snow-combat-readability",
    cameraMode: "author-controlled-oblique"
  };
  if (!sameValues(contract.view, expectedView)) fail("view guidance drifted");
  const timing = contract.timing;
  if (
    timing?.playableTargetMinutes !== 60 ||
    timing?.endToEndTestBudgetMinutes !== 70 ||
    timing?.startupMinutes !== 3 ||
    timing?.postResolutionPersistenceMinutes !== 7 ||
    timing?.activityGraceTicks !== 180
  ) {
    fail("60-minute playable and 70-minute E2E budgets are frozen");
  }
  const expectedExclusions = [
    "title_loading",
    "failure_retry",
    "idle",
    "post_resolution_persistence"
  ];
  if (!sameValues(timing.excludedFromPlayable, expectedExclusions)) {
    fail("playable-time exclusions drifted");
  }

  const refs = contract.catalogReferences;
  for (const key of ["startFixtureId", "chapterId", "bossId"]) {
    const catalogKey = key === "startFixtureId" ? "startFixtureId" : key;
    if (refs?.[key] !== catalogSlice[catalogKey]) fail(`${key} drifted from the catalog`);
  }
  for (const key of ["subregionIds", "npcIds", "enemyFamilyIds"]) {
    if (!sameValues(refs?.[key], catalogSlice[key])) fail(`${key} drifted from the catalog`);
  }

  if (!Array.isArray(contract.cellIds) || contract.cellIds.length < 5) {
    fail("at least five route cells are required");
  }
  assertUnique(contract.cellIds, "cell id");
  if (!Array.isArray(contract.beats) || contract.beats.length !== 7) {
    fail("the initial vertical slice must contain exactly seven playable beats");
  }
  const allowedKinds = new Set([
    "exploration",
    "training",
    "combat",
    "investigation",
    "boss",
    "resolution"
  ]);
  const beatIds = [];
  const objectiveIds = [];
  const objectiveStages = new Map();
  let playableMinutes = 0;
  for (const [beatIndex, beat] of contract.beats.entries()) {
    if (!allowedKinds.has(beat.kind)) fail(`unsupported beat kind: ${beat.kind}`);
    if (!Number.isInteger(beat.targetMinutes) || beat.targetMinutes <= 0) {
      fail(`${beat.id} has an invalid targetMinutes`);
    }
    if (!contract.cellIds.includes(beat.cellId)) fail(`${beat.id} references an unknown cell`);
    if (!Array.isArray(beat.objectiveIds) || beat.objectiveIds.length === 0) {
      fail(`${beat.id} has no objective path`);
    }
    if (beat.autoAdvance !== false) fail(`${beat.id} must never advance from a timer`);
    assertUnique(beat.objectiveIds, `${beat.id} objective`);
    beatIds.push(beat.id);
    objectiveIds.push(...beat.objectiveIds);
    for (const objective of beat.objectiveIds) objectiveStages.set(objective, beatIndex);
    playableMinutes += beat.targetMinutes;
  }
  assertUnique(beatIds, "beat id");
  assertUnique(objectiveIds, "objective id");
  if (playableMinutes !== timing.playableTargetMinutes) {
    fail(`beat budgets total ${playableMinutes}, expected ${timing.playableTargetMinutes}`);
  }
  const safePoints = contract.safePoints;
  if (!Array.isArray(safePoints) || safePoints.length !== contract.beats.length) {
    fail("every playable beat must own exactly one safe point");
  }
  for (const [index, safePoint] of safePoints.entries()) {
    const pose = safePoint.poseMm;
    if (
      typeof safePoint.id !== "string" || safePoint.id.length === 0 ||
      safePoint.beatId !== contract.beats[index].id ||
      !pose || ![pose.x, pose.y, pose.height, pose.floorLayer].every(Number.isInteger) ||
      pose.height < contract.playerSeed.initialPoseMm.height
    ) {
      fail(`${safePoint.id ?? "safe point"} is not valid for its authored beat`);
    }
  }
  assertUnique(safePoints.map((safePoint) => safePoint.id), "safe point id");
  assertUnique(safePoints.map((safePoint) => safePoint.beatId), "safe point beat");
  const expectedSafePoints = [
    ["f1_safe_point_rain_ferry_arrival", -12000, -1600],
    ["f1_safe_point_shen_yan_training", -6500, -500],
    ["f1_safe_point_umbrella_lane", -5600, -1200],
    ["f1_safe_point_shared_workbench", -4300, -100],
    ["f1_safe_point_canopy_return", -4300, -100],
    ["f1_safe_point_four_seasons_court", 2200, 800],
    ["f1_safe_point_resolution_return", 3000, 800]
  ].map(([id, x, y], index) => ({
    id,
    beatId: contract.beats[index].id,
    poseMm: {x, y, height: 0, floorLayer: 0}
  }));
  if (!sameValues(safePoints, expectedSafePoints)) {
    fail("F1 safe-point route or authored poses drifted");
  }
  const interactions = contract.questInteractions;
  if (!Array.isArray(interactions) || interactions.length < 40 || interactions.length > 64) {
    fail("the F1 route must contain 40..64 quest interaction definitions");
  }
  const interactionKinds = new Set(["inspect", "operate", "talk", "choose"]);
  const interactionsByObjective = new Map();
  for (const interaction of interactions) {
    if (!interactionKinds.has(interaction.kind)) {
      fail(`${interaction.id} has an unsupported interaction kind`);
    }
    if (!contract.cellIds.includes(interaction.cellId)) {
      fail(`${interaction.id} references an unknown cell`);
    }
    if (!objectiveIds.includes(interaction.objectiveId)) {
      fail(`${interaction.id} references an unknown objective`);
    }
    if (interaction.kind === "choose") {
      if (typeof interaction.selectionId !== "string" || interaction.selectionId.length === 0) {
        fail(`${interaction.id} choice interaction requires a selection id`);
      }
    } else if (interaction.selectionId !== null) {
      fail(`${interaction.id} non-choice interaction cannot declare a selection id`);
    }
    const hasSelectionObjective = interaction.requiredSelectionObjectiveId !== null;
    const hasSelection = interaction.requiredSelectionId !== null;
    if (hasSelectionObjective !== hasSelection) {
      fail(`${interaction.id} has an invalid interaction selection gate`);
    }
    if (hasSelection) {
      const selectionInteraction = interactions.find(
        (candidate) => candidate.kind === "choose" &&
          candidate.objectiveId === interaction.requiredSelectionObjectiveId &&
          candidate.selectionId === interaction.requiredSelectionId
      );
      const selectionStageIndex = objectiveStages.get(
        interaction.requiredSelectionObjectiveId
      );
      const interactionStageIndex = objectiveStages.get(interaction.objectiveId);
      const selectionObjectiveIndex = selectionStageIndex === undefined
        ? -1
        : contract.beats[selectionStageIndex].objectiveIds.indexOf(
            interaction.requiredSelectionObjectiveId
          );
      const interactionObjectiveIndex = interactionStageIndex === undefined
        ? -1
        : contract.beats[interactionStageIndex].objectiveIds.indexOf(
            interaction.objectiveId
          );
      const selectionPrecedesInteraction =
        selectionStageIndex < interactionStageIndex ||
        (selectionStageIndex === interactionStageIndex &&
         selectionObjectiveIndex < interactionObjectiveIndex);
      if (!selectionInteraction || !selectionPrecedesInteraction) {
        fail(`${interaction.id} has a missing or future interaction selection gate`);
      }
    }
    const objectiveInteractions = interactionsByObjective.get(interaction.objectiveId) ?? [];
    objectiveInteractions.push(interaction);
    interactionsByObjective.set(interaction.objectiveId, objectiveInteractions);
    if (!Array.isArray(interaction.prerequisiteObjectiveIds) ||
        interaction.prerequisiteObjectiveIds.length > 8) {
      fail(`${interaction.id} has invalid prerequisite objectives`);
    }
    assertUnique(interaction.prerequisiteObjectiveIds, `${interaction.id} prerequisite objective`);
    for (const prerequisite of interaction.prerequisiteObjectiveIds) {
      if (!objectiveIds.includes(prerequisite) || prerequisite === interaction.objectiveId ||
          objectiveStages.get(prerequisite) > objectiveStages.get(interaction.objectiveId)) {
        fail(`${interaction.id} has an invalid prerequisite objective: ${prerequisite}`);
      }
    }
    if (!Number.isInteger(interaction.radiusMm) || interaction.radiusMm <= 0 ||
        interaction.radiusMm > 10000) {
      fail(`${interaction.id} has an invalid interaction radius`);
    }
    const pose = interaction.poseMm;
    if (!pose || ![pose.x, pose.y, pose.height, pose.floorLayer].every(Number.isInteger)) {
      fail(`${interaction.id} has an invalid interaction pose`);
    }
  }
  assertUnique(interactions.map((interaction) => interaction.id), "quest interaction id");
  for (const [objective, objectiveInteractions] of interactionsByObjective) {
    const gatedVariants = objectiveInteractions.filter(
      (interaction) => interaction.requiredSelectionId !== null
    );
    if (gatedVariants.length === 0) {
      if (objectiveInteractions.length === 1) continue;
      if (objectiveInteractions.some((interaction) => interaction.kind !== "choose")) {
        fail(`duplicate non-choice quest interaction objective: ${objective}`);
      }
      assertUnique(
        objectiveInteractions.map((interaction) => interaction.selectionId),
        `${objective} quest choice selection`
      );
      continue;
    }
    const selectionObjectiveId = gatedVariants[0].requiredSelectionObjectiveId;
    const chooseTarget = gatedVariants[0].kind === "choose";
    if (gatedVariants.length !== objectiveInteractions.length ||
        gatedVariants.some(
          (candidate) =>
            candidate.requiredSelectionObjectiveId !== selectionObjectiveId ||
            (candidate.kind === "choose") !== chooseTarget
        )) {
      fail(`${objective} mixes incompatible interaction selection gates`);
    }
    assertUnique(
      gatedVariants.map((candidate) => candidate.requiredSelectionId),
      `${objective} interaction selection gate`
    );
    if (chooseTarget) {
      assertUnique(
        gatedVariants.map((candidate) => candidate.selectionId),
        `${objective} gated quest choice selection`
      );
    }
    const selectionOptions = interactions
      .filter(
        (candidate) => candidate.kind === "choose" &&
          candidate.objectiveId === selectionObjectiveId &&
          candidate.selectionId !== null
      )
      .map((candidate) => candidate.selectionId);
    if (selectionOptions.length !== gatedVariants.length ||
        selectionOptions.some(
          (selection) => !gatedVariants.some(
            (candidate) => candidate.requiredSelectionId === selection
          )
        )) {
      fail(`${objective} interactions do not cover every authored selection option`);
    }
  }
  const firstBeatObjectives = new Set(contract.beats[0].objectiveIds);
  const firstBeatInteractionObjectives = new Set(interactions
    .filter((interaction) => firstBeatObjectives.has(interaction.objectiveId))
    .map((interaction) => interaction.objectiveId));
  if (
    firstBeatInteractionObjectives.size !== firstBeatObjectives.size ||
    [...firstBeatInteractionObjectives].some(
      (objective) => !firstBeatObjectives.has(objective)
    )
  ) {
    fail("the first playable beat must be fully covered by scene interactions");
  }
  const expectedRainFerryObjectives = [
    "f1_objective_inspect_travel_writ",
    "f1_objective_choose_arrival_clue",
    "f1_objective_read_ferry_condition",
    "f1_objective_choose_mooring_method",
    "f1_objective_secure_ferry_mooring",
    "f1_objective_inspect_bilge_counterweight",
    "f1_objective_release_ferry_bilge",
    "f1_objective_raise_wayfinding_lantern",
    "f1_objective_read_workshop_bell_code",
    "f1_objective_sound_workshop_bell",
    "f1_objective_reach_ferry_gate"
  ];
  const expectedRainFerryInteractions = [
    ["f1_interaction_travel_writ", "inspect", expectedRainFerryObjectives[0], null, null, null, -12000, -1600, 800, []],
    ["f1_interaction_arrival_clue_high_water_tags", "choose", expectedRainFerryObjectives[1], "f1_choice_arrival_high_water_tags", null, null, -12200, 900, 600, [expectedRainFerryObjectives[0]]],
    ["f1_interaction_arrival_clue_drowned_manifest", "choose", expectedRainFerryObjectives[1], "f1_choice_arrival_drowned_manifest", null, null, -11400, -3100, 600, [expectedRainFerryObjectives[0]]],
    ["f1_interaction_arrival_clue_follow_bell", "choose", expectedRainFerryObjectives[1], "f1_choice_arrival_follow_bell", null, null, -10900, -1000, 700, [expectedRainFerryObjectives[0]]],
    ["f1_interaction_read_high_water_repairs", "inspect", expectedRainFerryObjectives[2], null, expectedRainFerryObjectives[1], "f1_choice_arrival_high_water_tags", -11100, 1700, 550, [expectedRainFerryObjectives[1]]],
    ["f1_interaction_read_manifest_waterline", "inspect", expectedRainFerryObjectives[2], null, expectedRainFerryObjectives[1], "f1_choice_arrival_drowned_manifest", -10600, -2800, 550, [expectedRainFerryObjectives[1]]],
    ["f1_interaction_read_main_flood_gauge", "inspect", expectedRainFerryObjectives[2], null, expectedRainFerryObjectives[1], "f1_choice_arrival_follow_bell", -10700, -900, 650, [expectedRainFerryObjectives[1]]],
    ["f1_interaction_choose_cross_belay", "choose", expectedRainFerryObjectives[3], "f1_choice_mooring_cross_belay", null, null, -10100, 1200, 600, [expectedRainFerryObjectives[2]]],
    ["f1_interaction_choose_quick_hitch", "choose", expectedRainFerryObjectives[3], "f1_choice_mooring_quick_hitch", null, null, -10000, -1700, 600, [expectedRainFerryObjectives[2]]],
    ["f1_interaction_lock_cross_belay", "operate", expectedRainFerryObjectives[4], null, expectedRainFerryObjectives[3], "f1_choice_mooring_cross_belay", -9400, 1900, 550, [expectedRainFerryObjectives[3]]],
    ["f1_interaction_correct_overloaded_quick_hitch", "operate", expectedRainFerryObjectives[4], null, expectedRainFerryObjectives[3], "f1_choice_mooring_quick_hitch", -9200, -2300, 550, [expectedRainFerryObjectives[3]]],
    ["f1_interaction_inspect_bilge_counterweight", "inspect", expectedRainFerryObjectives[5], null, null, null, -8700, -800, 550, [expectedRainFerryObjectives[4]]],
    ["f1_interaction_release_ferry_bilge", "operate", expectedRainFerryObjectives[6], null, null, null, -8400, 1100, 550, [expectedRainFerryObjectives[5]]],
    ["f1_interaction_raise_wayfinding_lantern", "operate", expectedRainFerryObjectives[7], null, null, null, -7900, -300, 600, [expectedRainFerryObjectives[6]]],
    ["f1_interaction_read_workshop_bell_code", "inspect", expectedRainFerryObjectives[8], null, null, null, -7500, 1300, 550, [expectedRainFerryObjectives[7]]],
    ["f1_interaction_sound_workshop_bell", "operate", expectedRainFerryObjectives[9], null, null, null, -7100, 300, 600, [expectedRainFerryObjectives[8]]],
    ["f1_interaction_ferry_gate", "operate", expectedRainFerryObjectives[10], null, null, null, -6700, -600, 800, [expectedRainFerryObjectives[9]]]
  ].map(([
    id,
    kind,
    objectiveId,
    selectionId,
    requiredSelectionObjectiveId,
    requiredSelectionId,
    x,
    y,
    radiusMm,
    prerequisiteObjectiveIds
  ]) => ({
    id,
    kind,
    cellId: contract.beats[0].cellId,
    objectiveId,
    selectionId,
    requiredSelectionObjectiveId,
    requiredSelectionId,
    poseMm: {x, y, height: 0, floorLayer: 0},
    radiusMm,
    prerequisiteObjectiveIds
  }));
  const rainFerryChain = interactions.filter((interaction) =>
    firstBeatObjectives.has(interaction.objectiveId)
  );
  if (
    !sameValues(
      contract.beats[0].objectiveIds,
      expectedRainFerryObjectives
    ) ||
    !sameValues(rainFerryChain, expectedRainFerryInteractions)
  ) {
    fail("the Rain Ferry clue, mooring correction, and readiness route drifted");
  }
  const expectedTrainingObjectives = [
    "f1_objective_meet_shen_yan",
    "f1_objective_choose_training_lane",
    "f1_objective_take_eavesguard_mark",
    "f1_objective_eavesguard_counter",
    "f1_objective_commit_eavesguard_heavy",
    "f1_objective_break_eavesguard_target",
    "f1_objective_review_eavesguard_with_shen_yan",
    "f1_objective_enter_flower_turn",
    "f1_objective_cross_flower_turn_line",
    "f1_objective_flower_turn_counter",
    "f1_objective_commit_flower_turn_light",
    "f1_objective_commit_flower_turn_heavy",
    "f1_objective_break_flower_turn_target",
    "f1_objective_finish_shen_yan_training"
  ];
  const expectedTrainingInteractions = [
    ["f1_interaction_meet_shen_yan", "talk", expectedTrainingObjectives[0], null, null, null, -6500, -500, 800, []],
    ["f1_interaction_choose_training_windward_lane", "choose", expectedTrainingObjectives[1], "f1_choice_training_windward_lane", null, null, -6100, 1600, 600, [expectedTrainingObjectives[0]]],
    ["f1_interaction_choose_training_leeward_lane", "choose", expectedTrainingObjectives[1], "f1_choice_training_leeward_lane", null, null, -6000, -2200, 600, [expectedTrainingObjectives[0]]],
    ["f1_interaction_take_windward_eavesguard_mark", "operate", expectedTrainingObjectives[2], null, expectedTrainingObjectives[1], "f1_choice_training_windward_lane", -5400, 1900, 550, [expectedTrainingObjectives[1]]],
    ["f1_interaction_take_leeward_eavesguard_mark", "operate", expectedTrainingObjectives[2], null, expectedTrainingObjectives[1], "f1_choice_training_leeward_lane", -5200, -2400, 550, [expectedTrainingObjectives[1]]],
    ["f1_interaction_review_eavesguard_with_shen_yan", "talk", expectedTrainingObjectives[6], null, null, null, -5000, -200, 800, [expectedTrainingObjectives[5]]],
    ["f1_interaction_cross_flower_turn_line", "operate", expectedTrainingObjectives[8], null, null, null, -3400, -1700, 550, [expectedTrainingObjectives[7]]],
    ["f1_interaction_finish_shen_yan_training", "talk", expectedTrainingObjectives[13], null, null, null, -5000, -200, 800, [expectedTrainingObjectives[12]]]
  ].map(([
    id,
    kind,
    objectiveId,
    selectionId,
    requiredSelectionObjectiveId,
    requiredSelectionId,
    x,
    y,
    radiusMm,
    prerequisiteObjectiveIds
  ]) => ({
    id,
    kind,
    cellId: contract.beats[1].cellId,
    objectiveId,
    selectionId,
    requiredSelectionObjectiveId,
    requiredSelectionId,
    poseMm: {x, y, height: 0, floorLayer: 0},
    radiusMm,
    prerequisiteObjectiveIds
  }));
  const trainingObjectiveSet = new Set(expectedTrainingObjectives);
  const authoredTrainingInteractions = interactions.filter((interaction) =>
    trainingObjectiveSet.has(interaction.objectiveId)
  );
  if (
    !sameValues(contract.beats[1].objectiveIds, expectedTrainingObjectives) ||
    !sameValues(authoredTrainingInteractions, expectedTrainingInteractions)
  ) {
    fail("the Shen Yan dialogue, practice-lane, and spatial training route drifted");
  }
  const umbrellaLaneBeat = contract.beats[2];
  const expectedUmbrellaLaneRainworks = [
    [
      "f1_interaction_inspect_torn_canopy_seam",
      "inspect",
      "f1_objective_inspect_torn_canopy_seam",
      -3600,
      -1700,
      650,
      ["f1_objective_defeat_leaking_dolls"]
    ],
    [
      "f1_interaction_release_flooded_gutter",
      "operate",
      "f1_objective_release_flooded_gutter",
      -2700,
      -700,
      650,
      ["f1_objective_inspect_torn_canopy_seam"]
    ],
    [
      "f1_interaction_raise_paper_egret_lure",
      "operate",
      "f1_objective_raise_paper_egret_lure",
      -1800,
      500,
      700,
      ["f1_objective_release_flooded_gutter"]
    ]
  ].map(([id, kind, objectiveId, x, y, radiusMm, prerequisiteObjectiveIds]) => ({
    id,
    kind,
    cellId: umbrellaLaneBeat.cellId,
    objectiveId,
    selectionId: null,
    requiredSelectionObjectiveId: null,
    requiredSelectionId: null,
    poseMm: {x, y, height: 0, floorLayer: 0},
    radiusMm,
    prerequisiteObjectiveIds
  }));
  const umbrellaLaneRainworks = interactions.filter((interaction) =>
    umbrellaLaneBeat.objectiveIds.slice(1, 4).includes(interaction.objectiveId)
  );
  if (
    !sameValues(
      umbrellaLaneBeat.objectiveIds.slice(1, 4),
      expectedUmbrellaLaneRainworks.map((interaction) => interaction.objectiveId)
    ) ||
    !sameValues(umbrellaLaneRainworks, expectedUmbrellaLaneRainworks)
  ) {
    fail("the umbrella-lane rainworks chain drifted");
  }
  const laneRouteInteractions = interactions.filter(
    (interaction) => interaction.objectiveId === umbrellaLaneBeat.objectiveIds[5]
  );
  if (
    laneRouteInteractions.length !== 2 ||
    laneRouteInteractions.some(
      (interaction) => interaction.kind !== "choose" ||
        interaction.requiredSelectionId !== null ||
        !sameValues(
          interaction.prerequisiteObjectiveIds,
          umbrellaLaneBeat.objectiveIds.slice(0, 5)
        )
    ) ||
    !sameValues(
      laneRouteInteractions.map((interaction) => interaction.selectionId),
      ["f1_choice_lane_canopy", "f1_choice_lane_drain"]
    )
  ) {
    fail("the umbrella-lane route choice must offer two routes after combat and rainworks");
  }
  const workbenchBeat = contract.beats[3];
  const workbenchEvidence = workbenchBeat.objectiveIds.slice(0, 3);
  const workbenchChoice = workbenchBeat.objectiveIds[3];
  const routeEvidenceInteractions = interactions.filter(
    (interaction) => interaction.objectiveId === workbenchEvidence[0]
  );
  const independentEvidenceInteractions = interactions.filter((interaction) =>
    workbenchEvidence.slice(1).includes(interaction.objectiveId)
  );
  const calibrationInteractions = interactions.filter(
    (interaction) => interaction.objectiveId === workbenchChoice
  );
  if (
    routeEvidenceInteractions.length !== 2 ||
    routeEvidenceInteractions.some(
      (interaction) =>
        interaction.kind !== "inspect" ||
        interaction.cellId !== workbenchBeat.cellId ||
        interaction.prerequisiteObjectiveIds.length !== 0 ||
        interaction.requiredSelectionObjectiveId !== umbrellaLaneBeat.objectiveIds[5]
    ) ||
    !sameValues(
      routeEvidenceInteractions.map((interaction) => interaction.requiredSelectionId),
      ["f1_choice_lane_canopy", "f1_choice_lane_drain"]
    ) ||
    independentEvidenceInteractions.length !== 2 ||
    independentEvidenceInteractions.some(
      (interaction) => interaction.kind !== "inspect" ||
        interaction.cellId !== workbenchBeat.cellId ||
        interaction.prerequisiteObjectiveIds.length !== 0 ||
        interaction.requiredSelectionId !== null
    )
  ) {
    fail("the workbench investigation must expose one route-gated and two independent evidence inspections");
  }
  if (
    calibrationInteractions.length !== 2 ||
    calibrationInteractions.some(
      (interaction) =>
        interaction.kind !== "choose" ||
        interaction.cellId !== workbenchBeat.cellId ||
        !sameValues(interaction.prerequisiteObjectiveIds, workbenchEvidence)
    ) ||
    !sameValues(
      calibrationInteractions.map((interaction) => interaction.selectionId),
      ["f1_choice_rib_spring_calibration", "f1_choice_rib_winter_calibration"]
    )
  ) {
    fail("rib calibration must offer two stable choices after all workbench evidence");
  }
  const returnBeat = contract.beats[4];
  const returnPrimer = interactions.find(
    (interaction) => interaction.objectiveId === returnBeat.objectiveIds[0]
  );
  if (
    returnPrimer?.id !== "f1_interaction_prime_return_calibration" ||
    returnPrimer.kind !== "operate" ||
    returnPrimer.cellId !== returnBeat.cellId ||
    !sameValues(returnPrimer.poseMm, {x: -3500, y: -900, height: 0, floorLayer: 0}) ||
    !sameValues(returnPrimer.prerequisiteObjectiveIds, [])
  ) {
    fail("the return calibration primer must be an authored combat-space operation");
  }
  const returnShortcut = interactions.find(
    (interaction) => interaction.objectiveId === returnBeat.objectiveIds[3]
  );
  if (
    returnShortcut?.kind !== "operate" ||
    returnShortcut.cellId !== returnBeat.cellId ||
    !sameValues(returnShortcut.prerequisiteObjectiveIds, [returnBeat.objectiveIds[2]])
  ) {
    fail("the return shortcut must wait for calibration combat validation");
  }
  const encounterActivations = contract.questEncounterActivations;
  if (!Array.isArray(encounterActivations) ||
      encounterActivations.length < 1 || encounterActivations.length > 16) {
    fail("quest encounter activations must contain 1..16 definitions");
  }
  for (const activation of encounterActivations) {
    const activationBeat = contract.beats.find((beat) => beat.id === activation.beatId);
    if (!activationBeat) {
      fail(`${activation.id} references an unknown beat`);
    }
    if (activation.triggerObjectiveId !== null &&
        !activationBeat.objectiveIds.includes(activation.triggerObjectiveId)) {
      fail(`${activation.id} references an objective outside its beat`);
    }
    if (!["replace", "reinforce"].includes(activation.mode) ||
        (activation.mode === "reinforce" && activation.triggerObjectiveId === null)) {
      fail(`${activation.id} has an invalid activation mode or stage-entry reinforcement`);
    }
    const hasSelectionObjective = activation.requiredSelectionObjectiveId !== null;
    const hasSelection = activation.requiredSelectionId !== null;
    if (hasSelectionObjective !== hasSelection ||
        (hasSelection && activation.triggerObjectiveId === null)) {
      fail(`${activation.id} has an invalid selection gate`);
    }
    if (hasSelection) {
      const selectionInteraction = interactions.find(
        (interaction) =>
          interaction.kind === "choose" &&
          interaction.objectiveId === activation.requiredSelectionObjectiveId &&
          interaction.selectionId === activation.requiredSelectionId
      );
      const selectionBeatIndex = contract.beats.findIndex(
        (beat) => beat.objectiveIds.includes(activation.requiredSelectionObjectiveId)
      );
      const activationBeatIndex = contract.beats.findIndex(
        (beat) => beat.id === activation.beatId
      );
      const selectionObjectiveIndex = selectionBeatIndex < 0
        ? -1
        : contract.beats[selectionBeatIndex].objectiveIds.indexOf(
            activation.requiredSelectionObjectiveId
          );
      const triggerObjectiveIndex = activationBeat.objectiveIds.indexOf(
        activation.triggerObjectiveId
      );
      if (!selectionInteraction || selectionBeatIndex < 0 ||
          selectionBeatIndex > activationBeatIndex ||
          (selectionBeatIndex === activationBeatIndex &&
           selectionObjectiveIndex >= triggerObjectiveIndex)) {
        fail(`${activation.id} references a missing or future selection gate`);
      }
    }
    if (activation.encounterId !== contract.combatBootstrap?.id) {
      fail(`${activation.id} references an unknown encounter`);
    }
    const hostileActorKeys = new Set(
      contract.combatBootstrap.actors
        .filter((actor) => actor.faction === "hostile")
        .map((actor) => actor.actorKey)
    );
    if (!Array.isArray(activation.actorKeys) || activation.actorKeys.length === 0 ||
        activation.actorKeys.length > 15 ||
        activation.actorKeys.some((actor) => !hostileActorKeys.has(actor))) {
      fail(`${activation.id} references an invalid hostile actor group`);
    }
    assertUnique(activation.actorKeys, `${activation.id} actor key`);
    if (!Array.isArray(activation.actorPlacements) ||
        activation.actorPlacements.length !== activation.actorKeys.length) {
      fail(`${activation.id} must place every hostile actor exactly once`);
    }
    for (let index = 0; index < activation.actorPlacements.length; index += 1) {
      const placement = activation.actorPlacements[index];
      const pose = placement?.poseMm;
      if (placement?.actorKey !== activation.actorKeys[index] ||
          !Number.isInteger(placement?.formationSlot) || placement.formationSlot < 0 ||
          placement.formationSlot >= encounterFormationSlotCapacity ||
          !pose || ![pose.x, pose.y, pose.height, pose.floorLayer].every(Number.isInteger)) {
        fail(`${activation.id} has an invalid ordered actor placement`);
      }
    }
    assertUnique(
      activation.actorPlacements.map((placement) => placement.formationSlot),
      `${activation.id} formation slot`
    );
  }
  assertUnique(encounterActivations.map((activation) => activation.id), "encounter activation id");
  const visitedActivationBoundaries = new Set();
  for (const activation of encounterActivations) {
    const boundary = `${activation.beatId}:${activation.triggerObjectiveId ?? "stage_entered"}`;
    if (visitedActivationBoundaries.has(boundary)) continue;
    visitedActivationBoundaries.add(boundary);
    const variants = encounterActivations.filter(
      (candidate) => candidate.beatId === activation.beatId &&
        candidate.triggerObjectiveId === activation.triggerObjectiveId
    );
    const gatedVariants = variants.filter(
      (candidate) => candidate.requiredSelectionId !== null
    );
    if (gatedVariants.length === 0) {
      if (variants.length !== 1) fail(`${boundary} has duplicate unconditional activations`);
      continue;
    }
    const selectionObjectiveId = gatedVariants[0].requiredSelectionObjectiveId;
    if (gatedVariants.length !== variants.length ||
        gatedVariants.some(
          (candidate) => candidate.requiredSelectionObjectiveId !== selectionObjectiveId
        )) {
      fail(`${boundary} mixes incompatible selection gates`);
    }
    assertUnique(
      gatedVariants.map((candidate) => candidate.requiredSelectionId),
      `${boundary} selection gate`
    );
    const selectionOptions = interactions
      .filter(
        (interaction) => interaction.objectiveId === selectionObjectiveId &&
          interaction.selectionId !== null
      )
      .map((interaction) => interaction.selectionId);
    if (selectionOptions.length !== gatedVariants.length ||
        selectionOptions.some(
          (selection) => !gatedVariants.some(
            (candidate) => candidate.requiredSelectionId === selection
          )
        )) {
      fail(`${boundary} does not cover every authored selection option`);
    }
  }
  const trainingBeat = contract.beats[1];
  const laneBeat = contract.beats[2];
  const bossBeat = contract.beats[5];
  const activationSignatures = encounterActivations.map((activation) => [
    activation.id,
    activation.beatId,
    activation.triggerObjectiveId,
    activation.requiredSelectionObjectiveId,
    activation.requiredSelectionId,
    activation.mode,
    activation.actorKeys,
    activation.actorPlacements.map((placement) => [
      placement.actorKey,
      placement.poseMm.x,
      placement.poseMm.y,
      placement.poseMm.height,
      placement.poseMm.floorLayer,
      placement.formationSlot
    ])
  ]);
  const expectedActivationSignatures = [
    ["f1_activation_training_windward_guard_rig", trainingBeat.id, expectedTrainingObjectives[2], expectedTrainingObjectives[1], "f1_choice_training_windward_lane", "replace", [104], [[104, -4100, 2300, 0, 0, 0]]],
    ["f1_activation_training_leeward_guard_rig", trainingBeat.id, expectedTrainingObjectives[2], expectedTrainingObjectives[1], "f1_choice_training_leeward_lane", "replace", [104], [[104, -3900, -2500, 0, 0, 0]]],
    ["f1_activation_training_windward_eavesguard_target", trainingBeat.id, expectedTrainingObjectives[4], expectedTrainingObjectives[1], "f1_choice_training_windward_lane", "replace", [107], [[107, -4000, 2000, 0, 0, 0]]],
    ["f1_activation_training_leeward_eavesguard_target", trainingBeat.id, expectedTrainingObjectives[4], expectedTrainingObjectives[1], "f1_choice_training_leeward_lane", "replace", [107], [[107, -3800, -2100, 0, 0, 0]]],
    ["f1_activation_training_flower_turn_rig", trainingBeat.id, expectedTrainingObjectives[6], null, null, "replace", [108], [[108, -3300, 1200, 700, 0, 2]]],
    ["f1_activation_training_flower_turn_target", trainingBeat.id, expectedTrainingObjectives[11], null, null, "replace", [109], [[109, -3000, -800, 700, 0, 2]]],
    ["f1_activation_umbrella_lane_first_encounter", laneBeat.id, null, null, null, "replace", [101, 102], [[101, -4000, -2600, 0, 0, 1], [102, -3000, -400, 0, 0, 5]]],
    ["f1_activation_umbrella_lane_paper_egret", laneBeat.id, laneBeat.objectiveIds[3], null, null, "replace", [103], [[103, -1500, 900, 700, 0, 2]]],
    ["f1_activation_canopy_return_encounter", returnBeat.id, null, null, null, "replace", [101, 102, 103], [[101, -2500, -1800, 0, 0, 0], [102, -900, -300, 0, 0, 3], [103, -500, 1700, 700, 0, 6]]],
    ["f1_activation_canopy_return_spring_reinforcement", returnBeat.id, returnBeat.objectiveIds[0], workbenchBeat.objectiveIds[3], "f1_choice_rib_spring_calibration", "reinforce", [106], [[106, 500, 1400, 0, 0, 5]]],
    ["f1_activation_canopy_return_winter_reinforcement", returnBeat.id, returnBeat.objectiveIds[0], workbenchBeat.objectiveIds[3], "f1_choice_rib_winter_calibration", "reinforce", [105], [[105, 500, 1400, 700, 0, 5]]],
    ["f1_activation_four_seasons_wraith", bossBeat.id, null, null, null, "replace", [201], [[201, 4000, 1900, 0, 0, 4]]]
  ];
  const returnSafePoint = safePoints[4].poseMm;
  const returnAggroRangeMm = contract.combatBootstrap?.director?.aggroRangeMm;
  const returnPlacements = [
    ...encounterActivations[8].actorPlacements,
    ...encounterActivations[9].actorPlacements,
    ...encounterActivations[10].actorPlacements
  ];
  if (!Number.isInteger(returnAggroRangeMm) || returnPlacements.some((placement) => {
    const deltaX = placement.poseMm.x - returnSafePoint.x;
    const deltaY = placement.poseMm.y - returnSafePoint.y;
    return deltaX * deltaX + deltaY * deltaY > returnAggroRangeMm * returnAggroRangeMm;
  })) {
    fail("return encounter placements must engage from the authored safe point");
  }
  if (!sameValues(activationSignatures, expectedActivationSignatures)) {
    fail("training lane variants, proof targets, later waves, and boss activations drifted");
  }
  const bossPhases = contract.questBossPhases;
  if (!Array.isArray(bossPhases) || bossPhases.length !== 4) {
    fail("the four-seasons wraith must contain exactly four authored phases");
  }
  for (const phase of bossPhases) {
    if (!bossBeat.objectiveIds.includes(phase.objectiveId) || phase.actorKey !== 201 ||
        !Number.isInteger(phase.healthPercent) || phase.healthPercent < 0 ||
        phase.healthPercent > 100 ||
        (phase.healthPercent === 0 ? phase.nextStanceId !== null :
          typeof phase.nextStanceId !== "string" || phase.nextStanceId.length === 0)) {
      fail(`${phase.id} is not a valid four-seasons boss phase`);
    }
  }
  assertUnique(bossPhases.map((phase) => phase.id), "boss phase id");
  assertUnique(bossPhases.map((phase) => phase.objectiveId), "boss phase objective");
  if (!sameValues(bossPhases.map((phase) => phase.objectiveId), bossBeat.objectiveIds) ||
      !sameValues(bossPhases.map((phase) => phase.healthPercent), [75, 50, 25, 0]) ||
      !sameValues(bossPhases.map((phase) => phase.nextStanceId), [
        "stance_wraith_summer",
        "stance_wraith_autumn",
        "stance_wraith_winter",
        null
  ])) {
    fail("the four-seasons boss phase order or thresholds drifted");
  }
  const resolutionBeat = contract.beats[6];
  const resolutionChoiceObjective = resolutionBeat.objectiveIds[0];
  const resolutionReturnObjective = resolutionBeat.objectiveIds[1];
  const resolutionChoices = interactions.filter(
    (interaction) => interaction.objectiveId === resolutionChoiceObjective
  );
  const resolutionReturn = interactions.filter(
    (interaction) => interaction.objectiveId === resolutionReturnObjective
  );
  const expectedResolutionSelections = [
    "f1_choice_resolution_subdue",
    "f1_choice_resolution_restore_shared_mark"
  ];
  if (
    resolutionChoices.length !== 2 ||
    resolutionChoices.some(
      (interaction) =>
        interaction.kind !== "choose" ||
        interaction.cellId !== resolutionBeat.cellId ||
        interaction.prerequisiteObjectiveIds.length !== 0
    ) ||
    !sameValues(
      resolutionChoices.map((interaction) => interaction.selectionId),
      expectedResolutionSelections
    )
  ) {
    fail("the resolution beat must expose two stable authored choices");
  }
  if (
    resolutionReturn.length !== 1 ||
    resolutionReturn[0].kind !== "talk" ||
    resolutionReturn[0].cellId !== resolutionBeat.cellId ||
    !sameValues(resolutionReturn[0].prerequisiteObjectiveIds, [resolutionChoiceObjective])
  ) {
    fail("returning to Shen Yan must wait for one committed resolution choice");
  }
  const resolutionRewards = contract.questResolutionRewards;
  if (!Array.isArray(resolutionRewards) || resolutionRewards.length !== 2) {
    fail("the resolution beat must contain exactly two reward receipts");
  }
  for (const reward of resolutionRewards) {
    if (
      [
        reward.id,
        reward.objectiveId,
        reward.selectionId,
        reward.rewardId,
        reward.rewardDedupKey
      ].some((value) => typeof value !== "string" || value.length === 0) ||
      reward.objectiveId !== resolutionChoiceObjective ||
      !expectedResolutionSelections.includes(reward.selectionId)
    ) {
      fail(`${reward.id ?? "resolution reward"} is not a valid resolution reward receipt`);
    }
  }
  for (const field of ["id", "selectionId", "rewardId", "rewardDedupKey"]) {
    assertUnique(
      resolutionRewards.map((reward) => reward[field]),
      `resolution reward ${field}`
    );
  }
  const expectedResolutionRewards = [
    {
      id: "f1_resolution_reward_subdue",
      objectiveId: resolutionChoiceObjective,
      selectionId: expectedResolutionSelections[0],
      rewardId: "f1_reward_sealed_mixed_umbrella",
      rewardDedupKey: "f1_claim_resolution_subdue"
    },
    {
      id: "f1_resolution_reward_restore_shared_mark",
      objectiveId: resolutionChoiceObjective,
      selectionId: expectedResolutionSelections[1],
      rewardId: "f1_reward_joint_workshop_formula",
      rewardDedupKey: "f1_claim_resolution_restore_shared_mark"
    }
  ];
  if (!sameValues(resolutionRewards, expectedResolutionRewards)) {
    fail("resolution reward receipt mappings drifted");
  }
  const combatTriggers = contract.questCombatTriggers;
  if (!Array.isArray(combatTriggers) || combatTriggers.length < 2 || combatTriggers.length > 64) {
    fail("quest combat triggers must contain 2..64 definitions");
  }
  const combatTriggerKinds = new Set([
    "player_ability_started",
    "player_stance_changed",
    "player_hit_guarded",
    "player_hit_evaded"
  ]);
  const authoredCombatStances = new Set(
    contract.combatBootstrap?.actors?.flatMap((actor) => actor.stanceIds ?? []) ?? []
  );
  const authoredCombatAbilities = contract.combatBootstrap?.abilities ?? [];
  for (const trigger of combatTriggers) {
    if (!combatTriggerKinds.has(trigger.kind)) {
      fail(`${trigger.id} has an unsupported quest combat trigger kind`);
    }
    if (!objectiveIds.includes(trigger.objectiveId)) {
      fail(`${trigger.id} references an unknown objective`);
    }
    if (typeof trigger.requiredStanceId !== "string" || trigger.requiredStanceId.length === 0) {
      fail(`${trigger.id} has an invalid required stance`);
    }
    const abilityStarted = trigger.kind === "player_ability_started";
    if (
      (abilityStarted &&
        (typeof trigger.requiredAbilityId !== "string" ||
          trigger.requiredAbilityId.length === 0)) ||
      (!abilityStarted && trigger.requiredAbilityId !== null)
    ) {
      fail(`${trigger.id} has an invalid required ability`);
    }
    if (!authoredCombatStances.has(trigger.requiredStanceId)) {
      fail(`${trigger.id} references an unknown required stance`);
    }
    if (abilityStarted) {
      const ability = authoredCombatAbilities.find(
        (candidate) => candidate.id === trigger.requiredAbilityId
      );
      if (ability?.requiredStanceId !== trigger.requiredStanceId) {
        fail(`${trigger.id} references an incompatible required ability`);
      }
    }
    const hasSelectionObjective = trigger.requiredSelectionObjectiveId !== null;
    const hasSelection = trigger.requiredSelectionId !== null;
    if (hasSelectionObjective !== hasSelection) {
      fail(`${trigger.id} has an invalid combat trigger selection gate`);
    }
    if (hasSelection) {
      const selectionInteraction = interactions.find(
        (interaction) =>
          interaction.kind === "choose" &&
          interaction.objectiveId === trigger.requiredSelectionObjectiveId &&
          interaction.selectionId === trigger.requiredSelectionId
      );
      const selectionBeatIndex = objectiveStages.get(trigger.requiredSelectionObjectiveId);
      const triggerBeatIndex = objectiveStages.get(trigger.objectiveId);
      const selectionObjectiveIndex = selectionBeatIndex === undefined
        ? -1
        : contract.beats[selectionBeatIndex].objectiveIds.indexOf(
            trigger.requiredSelectionObjectiveId
          );
      const triggerObjectiveIndex = triggerBeatIndex === undefined
        ? -1
        : contract.beats[triggerBeatIndex].objectiveIds.indexOf(trigger.objectiveId);
      if (!selectionInteraction || selectionBeatIndex === undefined ||
          triggerBeatIndex === undefined || selectionBeatIndex > triggerBeatIndex ||
          (selectionBeatIndex === triggerBeatIndex &&
           selectionObjectiveIndex >= triggerObjectiveIndex)) {
        fail(`${trigger.id} references a missing or future combat trigger selection gate`);
      }
    }
    if (!Array.isArray(trigger.prerequisiteObjectiveIds) ||
        trigger.prerequisiteObjectiveIds.length === 0 ||
        trigger.prerequisiteObjectiveIds.length > 8) {
      fail(`${trigger.id} has invalid prerequisite objectives`);
    }
    assertUnique(
      trigger.prerequisiteObjectiveIds,
      `${trigger.id} prerequisite objective`
    );
    for (const prerequisite of trigger.prerequisiteObjectiveIds) {
      if (!objectiveIds.includes(prerequisite) || prerequisite === trigger.objectiveId ||
          objectiveStages.get(prerequisite) > objectiveStages.get(trigger.objectiveId)) {
        fail(`${trigger.id} has an invalid prerequisite objective: ${prerequisite}`);
      }
    }
  }
  assertUnique(combatTriggers.map((trigger) => trigger.id), "quest combat trigger id");
  const visitedCombatTriggerObjectives = new Set();
  for (const trigger of combatTriggers) {
    if (visitedCombatTriggerObjectives.has(trigger.objectiveId)) continue;
    visitedCombatTriggerObjectives.add(trigger.objectiveId);
    const variants = combatTriggers.filter(
      (candidate) => candidate.objectiveId === trigger.objectiveId
    );
    const gatedVariants = variants.filter(
      (candidate) => candidate.requiredSelectionId !== null
    );
    if (gatedVariants.length === 0) {
      if (variants.length !== 1) {
        fail(`${trigger.objectiveId} has duplicate unconditional combat triggers`);
      }
      continue;
    }
    const selectionObjectiveId = gatedVariants[0].requiredSelectionObjectiveId;
    if (gatedVariants.length !== variants.length ||
        gatedVariants.some(
          (candidate) => candidate.requiredSelectionObjectiveId !== selectionObjectiveId
        )) {
      fail(`${trigger.objectiveId} mixes incompatible combat trigger selection gates`);
    }
    assertUnique(
      gatedVariants.map((candidate) => candidate.requiredSelectionId),
      `${trigger.objectiveId} combat trigger selection gate`
    );
    const selectionOptions = interactions
      .filter(
        (interaction) => interaction.kind === "choose" &&
          interaction.objectiveId === selectionObjectiveId &&
          interaction.selectionId !== null
      )
      .map((interaction) => interaction.selectionId);
    if (selectionOptions.length !== gatedVariants.length ||
        selectionOptions.some(
          (selection) => !gatedVariants.some(
            (candidate) => candidate.requiredSelectionId === selection
          )
        )) {
      fail(`${trigger.objectiveId} combat triggers do not cover every authored selection option`);
    }
  }
  const trainingCombatObjectives = new Set([
    expectedTrainingObjectives[3],
    expectedTrainingObjectives[4],
    expectedTrainingObjectives[7],
    expectedTrainingObjectives[9],
    expectedTrainingObjectives[10],
    expectedTrainingObjectives[11]
  ]);
  const coveredTrainingObjectives = combatTriggers
    .filter((trigger) => trainingCombatObjectives.has(trigger.objectiveId))
    .map((trigger) => trigger.objectiveId);
  if (
    coveredTrainingObjectives.length !== trainingCombatObjectives.size ||
    coveredTrainingObjectives.some((objective) => !trainingCombatObjectives.has(objective))
  ) {
    fail("the training beat combat objectives must be fully covered by combat triggers");
  }
  const expectedTrainingTriggers = [
    {
      id: "f1_trigger_eavesguard_counter",
      kind: "player_hit_guarded",
      objectiveId: expectedTrainingObjectives[3],
      requiredStanceId: "stance_eavesguard",
      requiredAbilityId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      prerequisiteObjectiveIds: [expectedTrainingObjectives[2]]
    },
    {
      id: "f1_trigger_eavesguard_heavy",
      kind: "player_ability_started",
      objectiveId: expectedTrainingObjectives[4],
      requiredStanceId: "stance_eavesguard",
      requiredAbilityId: "ability_eavesguard_heavy",
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      prerequisiteObjectiveIds: [expectedTrainingObjectives[3]]
    },
    {
      id: "f1_trigger_enter_flower_turn",
      kind: "player_stance_changed",
      objectiveId: expectedTrainingObjectives[7],
      requiredStanceId: "stance_flower_turn",
      requiredAbilityId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      prerequisiteObjectiveIds: [expectedTrainingObjectives[6]]
    },
    {
      id: "f1_trigger_flower_turn_counter",
      kind: "player_hit_evaded",
      objectiveId: expectedTrainingObjectives[9],
      requiredStanceId: "stance_flower_turn",
      requiredAbilityId: null,
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      prerequisiteObjectiveIds: [expectedTrainingObjectives[8]]
    },
    {
      id: "f1_trigger_flower_turn_light",
      kind: "player_ability_started",
      objectiveId: expectedTrainingObjectives[10],
      requiredStanceId: "stance_flower_turn",
      requiredAbilityId: "ability_flower_light",
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      prerequisiteObjectiveIds: [expectedTrainingObjectives[9]]
    },
    {
      id: "f1_trigger_flower_turn_heavy",
      kind: "player_ability_started",
      objectiveId: expectedTrainingObjectives[11],
      requiredStanceId: "stance_flower_turn",
      requiredAbilityId: "ability_flower_heavy",
      requiredSelectionObjectiveId: null,
      requiredSelectionId: null,
      prerequisiteObjectiveIds: [expectedTrainingObjectives[10]]
    }
  ];
  const authoredTrainingTriggers = combatTriggers.filter(
    (trigger) => trainingCombatObjectives.has(trigger.objectiveId)
  );
  if (!sameValues(authoredTrainingTriggers, expectedTrainingTriggers)) {
    fail("the authored Shen Yan training sequence drifted");
  }
  const expectedReturnCalibrationTriggers = [
    {
      id: "f1_trigger_return_spring_calibration_heavy",
      kind: "player_ability_started",
      objectiveId: returnBeat.objectiveIds[1],
      requiredStanceId: "stance_eavesguard",
      requiredAbilityId: "ability_eavesguard_heavy",
      requiredSelectionObjectiveId: workbenchChoice,
      requiredSelectionId: "f1_choice_rib_spring_calibration",
      prerequisiteObjectiveIds: [returnBeat.objectiveIds[0]]
    },
    {
      id: "f1_trigger_return_winter_calibration_light",
      kind: "player_ability_started",
      objectiveId: returnBeat.objectiveIds[1],
      requiredStanceId: "stance_flower_turn",
      requiredAbilityId: "ability_flower_light",
      requiredSelectionObjectiveId: workbenchChoice,
      requiredSelectionId: "f1_choice_rib_winter_calibration",
      prerequisiteObjectiveIds: [returnBeat.objectiveIds[0]]
    }
  ];
  const authoredReturnCalibrationTriggers = combatTriggers.filter(
    (trigger) => trigger.objectiveId === returnBeat.objectiveIds[1]
  );
  if (!sameValues(authoredReturnCalibrationTriggers, expectedReturnCalibrationTriggers)) {
    fail("the return calibration must require the combat action selected at the workbench");
  }
  const combatOutcomes = contract.questCombatOutcomes;
  if (!Array.isArray(combatOutcomes) || combatOutcomes.length < 2 || combatOutcomes.length > 64) {
    fail("quest combat outcomes must contain 2..64 definitions");
  }
  const combatOutcomeKinds = new Set([
    "hostile_archetype_defeated",
    "all_hostiles_defeated"
  ]);
  for (const outcome of combatOutcomes) {
    if (!combatOutcomeKinds.has(outcome.kind)) {
      fail(`${outcome.id} has an unsupported quest combat outcome kind`);
    }
    if (!objectiveIds.includes(outcome.objectiveId)) {
      fail(`${outcome.id} references an unknown objective`);
    }
    const archetypeGroup = outcome.kind === "hostile_archetype_defeated" &&
      typeof outcome.archetypeId === "string" && outcome.archetypeId.length > 0 &&
      Number.isInteger(outcome.requiredCount) && outcome.requiredCount > 0 &&
      outcome.requiredCount <= 16;
    const allHostiles = outcome.kind === "all_hostiles_defeated" &&
      outcome.archetypeId === null && outcome.requiredCount === 0;
    if (!archetypeGroup && !allHostiles) {
      fail(`${outcome.id} has an invalid required count`);
    }
  }
  assertUnique(combatOutcomes.map((outcome) => outcome.id), "quest combat outcome id");
  assertUnique(
    combatOutcomes.map((outcome) => outcome.objectiveId),
    "quest combat outcome objective"
  );
  const expectedTrainingOutcomes = [
    {
      id: "f1_outcome_break_eavesguard_target",
      kind: "hostile_archetype_defeated",
      objectiveId: expectedTrainingObjectives[5],
      archetypeId: "f1_training_eavesguard_target",
      requiredCount: 1
    },
    {
      id: "f1_outcome_break_flower_turn_target",
      kind: "hostile_archetype_defeated",
      objectiveId: expectedTrainingObjectives[12],
      archetypeId: "f1_training_flower_turn_target",
      requiredCount: 1
    }
  ];
  const authoredTrainingOutcomes = combatOutcomes.filter((outcome) =>
    trainingObjectiveSet.has(outcome.objectiveId)
  );
  if (!sameValues(authoredTrainingOutcomes, expectedTrainingOutcomes)) {
    fail("both Shen Yan stance lessons must end in a real target defeat");
  }
  const coveredTrainingObjectiveIds = new Set([
    ...authoredTrainingInteractions.map((interaction) => interaction.objectiveId),
    ...authoredTrainingTriggers.map((trigger) => trigger.objectiveId),
    ...authoredTrainingOutcomes.map((outcome) => outcome.objectiveId)
  ]);
  if (
    coveredTrainingObjectiveIds.size !== expectedTrainingObjectives.length ||
    expectedTrainingObjectives.some(
      (objective) => !coveredTrainingObjectiveIds.has(objective)
    )
  ) {
    fail("every Shen Yan training objective needs one authored interaction, signal, or outcome");
  }
  const laneCombatObjectives = new Set([
    contract.beats[2].objectiveIds[0],
    contract.beats[2].objectiveIds[4]
  ]);
  const coveredLaneCombatObjectives = combatOutcomes
    .filter((outcome) => laneCombatObjectives.has(outcome.objectiveId))
    .map((outcome) => outcome.objectiveId);
  if (
    coveredLaneCombatObjectives.length !== laneCombatObjectives.size ||
    coveredLaneCombatObjectives.some((objective) => !laneCombatObjectives.has(objective))
  ) {
    fail("the umbrella-lane combat objectives must be fully covered by combat outcomes");
  }
  const returnCombatOutcome = combatOutcomes.find(
    (outcome) => outcome.objectiveId === returnBeat.objectiveIds[2]
  );
  if (returnCombatOutcome?.kind !== "all_hostiles_defeated") {
    fail("the canopy return validation must require all active hostiles defeated");
  }

  const uiCues = contract.questUiCues;
  const allowedUiSources = new Set([
    "choice_available",
    "interaction_feedback",
    "objective_state",
    "combat_feedback",
    "recovery_offer",
    "recovery_resume"
  ]);
  const allowedAttemptClassificationsBySource = new Map([
    ["choice_available", new Set([
      "qualifying_first_visit",
      "qualifying_craft_decision",
      "qualifying_dialogue_decision"
    ])],
    ["interaction_feedback", new Set([
      "repeat_no_progress",
      "qualifying_craft_decision",
      "qualifying_error_feedback",
      "qualifying_wrong_order_feedback",
      "qualifying_craft_confirmation"
    ])],
    ["objective_state", new Set([
      "qualifying_first_visit",
      "qualifying_training_risk"
    ])],
    ["combat_feedback", new Set([
      "qualifying_combat_proof",
      "qualifying_combat_feedback"
    ])],
    ["recovery_offer", new Set(["failure_retry_excluded"])],
    ["recovery_resume", new Set(["resume_no_duplicate_progress"])]
  ]);
  const allowedAttemptStatuses = new Set([
    "not_applicable",
    "accepted",
    "rejected",
    "ignored_repeat",
    "pending"
  ]);
  const allowedAttemptRejectionReasons = new Set([
    "none",
    "prerequisite_incomplete",
    "selection_already_committed",
    "wrong_target"
  ]);
  const attemptResultIsNotApplicable = (result) =>
    result?.resultId === null && result?.status === "not_applicable" &&
    result?.rejectionReason === "none";
  const attemptResultShapeValid = (result) => {
    if (!result || typeof result !== "object" ||
        !allowedAttemptStatuses.has(result.status) ||
        !allowedAttemptRejectionReasons.has(result.rejectionReason)) {
      return false;
    }
    if (result.status === "not_applicable") return attemptResultIsNotApplicable(result);
    if (typeof result.resultId !== "string" || result.resultId.length === 0) return false;
    if (["accepted", "pending"].includes(result.status)) {
      return result.rejectionReason === "none";
    }
    if (result.status === "rejected") return result.rejectionReason !== "none";
    return result.status === "ignored_repeat" &&
      result.rejectionReason === "selection_already_committed";
  };
  if (!Array.isArray(uiCues) || uiCues.length !== 8) {
    fail("the first-two-beat UI projection contract requires exactly eight cues");
  }
  assertUnique(uiCues.map((cue) => cue.id), "quest UI cue id");
  const occupiedCueDomains = new Set();
  for (const cue of uiCues) {
    const beatIndex = contract.beats.findIndex((beat) => beat.id === cue.beatId);
    if (typeof cue.id !== "string" || cue.id.length === 0 ||
        beatIndex < 0 || beatIndex > 1) {
      fail(`${cue.id ?? "quest UI cue"} must belong to one of the first two beats`);
    }
    if (!Array.isArray(cue.sources) || cue.sources.length === 0 || cue.sources.length > 6 ||
        cue.sources.some((source) => !allowedUiSources.has(source))) {
      fail(`${cue.id} has invalid projection sources`);
    }
    assertUnique(cue.sources, `${cue.id} projection source`);
    if (!Array.isArray(cue.objectiveIds) || cue.objectiveIds.length === 0 ||
        cue.objectiveIds.length > 8) {
      fail(`${cue.id} has invalid projection objectives`);
    }
    assertUnique(cue.objectiveIds, `${cue.id} projection objective`);
    for (const objectiveId of cue.objectiveIds) {
      if (objectiveStages.get(objectiveId) !== beatIndex) {
        fail(`${cue.id} references an objective outside its beat`);
      }
      for (const source of cue.sources) {
        const domain = `${cue.beatId}:${source}:${objectiveId}`;
        if (occupiedCueDomains.has(domain)) {
          fail(`${cue.id} overlaps another quest UI cue domain`);
        }
        occupiedCueDomains.add(domain);
      }
    }
    if (cue.sources.includes("choice_available")) {
      for (const objectiveId of cue.objectiveIds) {
        const choices = interactions.filter(
          (interaction) => interaction.kind === "choose" &&
            interaction.objectiveId === objectiveId && interaction.selectionId !== null
        );
        if (choices.length === 0 || choices.length > 8) {
          fail(`${cue.id} choice source has no bounded authored options`);
        }
      }
    }
    if (!Array.isArray(cue.resultSelectors) || cue.resultSelectors.length > 8) {
      fail(`${cue.id} has invalid result selectors`);
    }
    const selectorKeys = [];
    for (const selector of cue.resultSelectors) {
      if (!cue.sources.includes(selector.source) ||
          !["interaction_feedback", "combat_feedback"].includes(selector.source) ||
          !cue.objectiveIds.includes(selector.objectiveId) ||
          !["none", "negative"].includes(selector.polarityOverride)) {
        fail(`${cue.id} has an invalid result selector domain`);
      }
      if (selector.source === "interaction_feedback") {
        const interaction = interactions.find(
          (candidate) => candidate.id === selector.primaryResultId
        );
        if (!interaction || selector.secondaryResultId !== null ||
            objectiveStages.get(interaction.objectiveId) !== beatIndex) {
          fail(`${cue.id} interaction selector references an invalid result`);
        }
        if (interaction.objectiveId !== selector.objectiveId) {
          const objectives = contract.beats[beatIndex].objectiveIds;
          if (interaction.kind !== "choose" ||
              objectives.indexOf(interaction.objectiveId) + 1 !==
                objectives.indexOf(selector.objectiveId)) {
            fail(`${cue.id} interaction selector is not an authored next-objective transition`);
          }
        }
      } else {
        const trigger = combatTriggers.find(
          (candidate) => candidate.id === selector.primaryResultId
        );
        if (!trigger || objectiveStages.get(trigger.objectiveId) !== beatIndex) {
          fail(`${cue.id} combat selector references an invalid trigger`);
        }
        if (selector.secondaryResultId === null) {
          if (trigger.objectiveId !== selector.objectiveId) {
            fail(`${cue.id} trigger-only selector must retain its objective`);
          }
        } else {
          const outcome = combatOutcomes.find(
            (candidate) => candidate.id === selector.secondaryResultId
          );
          const objectives = contract.beats[beatIndex].objectiveIds;
          if (!outcome || outcome.objectiveId !== selector.objectiveId ||
              objectiveStages.get(outcome.objectiveId) !== beatIndex ||
              (trigger.objectiveId !== selector.objectiveId &&
               objectives.indexOf(trigger.objectiveId) + 1 !==
                 objectives.indexOf(selector.objectiveId))) {
            fail(`${cue.id} combat selector is not an authored trigger-to-outcome transition`);
          }
        }
      }
      selectorKeys.push([
        selector.source,
        selector.objectiveId,
        selector.primaryResultId,
        selector.secondaryResultId
      ].join(":"));
    }
    assertUnique(selectorKeys, `${cue.id} result selector`);

    if (!Array.isArray(cue.attemptEvidenceRules) ||
        cue.attemptEvidenceRules.length === 0 || cue.attemptEvidenceRules.length > 16) {
      fail(`${cue.id} has invalid attempt evidence rules`);
    }
    const attemptRuleKeys = [];
    const attemptRuleSources = new Set();
    for (const rule of cue.attemptEvidenceRules) {
      if (!cue.sources.includes(rule.source) ||
          !cue.objectiveIds.includes(rule.objectiveId) ||
          !allowedAttemptClassificationsBySource
            .get(rule.source)
            ?.has(rule.classification) ||
          !attemptResultShapeValid(rule.primaryResult) ||
          !attemptResultShapeValid(rule.secondaryResult)) {
        fail(`${cue.id} has an invalid attempt evidence rule domain`);
      }
      const repeat = rule.classification === "repeat_no_progress";
      if ((rule.primaryResult.status === "ignored_repeat") !== repeat) {
        fail(`${cue.id} repeat classification does not match its primary result`);
      }
      if ([
        "choice_available",
        "objective_state",
        "recovery_offer",
        "recovery_resume"
      ].includes(rule.source)) {
        if (!attemptResultIsNotApplicable(rule.primaryResult) ||
            !attemptResultIsNotApplicable(rule.secondaryResult)) {
          fail(`${cue.id} source requires exact not_applicable result sentinels`);
        }
      } else if (rule.source === "interaction_feedback") {
        if (["not_applicable", "pending"].includes(rule.primaryResult.status) ||
            !attemptResultIsNotApplicable(rule.secondaryResult)) {
          fail(`${cue.id} interaction attempt evidence has an invalid result shape`);
        }
        const interaction = interactions.find(
          (candidate) => candidate.id === rule.primaryResult.resultId
        );
        if (!interaction || objectiveStages.get(interaction.objectiveId) !== beatIndex ||
            (repeat && interaction.kind !== "choose")) {
          fail(`${cue.id} interaction attempt evidence references an invalid result`);
        }
        if (interaction.objectiveId !== rule.objectiveId) {
          const objectives = contract.beats[beatIndex].objectiveIds;
          const selector = cue.resultSelectors.find(
            (candidate) => candidate.source === rule.source &&
              candidate.objectiveId === rule.objectiveId &&
              candidate.primaryResultId === rule.primaryResult.resultId &&
              candidate.secondaryResultId === null
          );
          if (rule.primaryResult.status !== "accepted" || interaction.kind !== "choose" ||
              interaction.selectionId === null ||
              objectives.indexOf(interaction.objectiveId) + 1 !==
                objectives.indexOf(rule.objectiveId) || !selector) {
            fail(`${cue.id} interaction attempt evidence is not an authored transition`);
          }
        }
      } else {
        if (rule.primaryResult.status === "not_applicable" ||
            rule.primaryResult.status === "ignored_repeat" ||
            rule.secondaryResult.status === "ignored_repeat" ||
            (!attemptResultIsNotApplicable(rule.secondaryResult) &&
             rule.primaryResult.status !== "accepted")) {
          fail(`${cue.id} combat attempt evidence has an invalid result shape`);
        }
        const trigger = combatTriggers.find(
          (candidate) => candidate.id === rule.primaryResult.resultId
        );
        if (!trigger || objectiveStages.get(trigger.objectiveId) !== beatIndex) {
          fail(`${cue.id} combat attempt evidence references an invalid trigger`);
        }
        if (attemptResultIsNotApplicable(rule.secondaryResult)) {
          if (trigger.objectiveId !== rule.objectiveId) {
            fail(`${cue.id} trigger-only attempt evidence must retain its objective`);
          }
        } else {
          const outcome = combatOutcomes.find(
            (candidate) => candidate.id === rule.secondaryResult.resultId
          );
          const objectives = contract.beats[beatIndex].objectiveIds;
          const selector = cue.resultSelectors.find(
            (candidate) => candidate.source === rule.source &&
              candidate.objectiveId === rule.objectiveId &&
              candidate.primaryResultId === rule.primaryResult.resultId &&
              candidate.secondaryResultId === rule.secondaryResult.resultId
          );
          if (!outcome || outcome.objectiveId !== rule.objectiveId ||
              objectiveStages.get(outcome.objectiveId) !== beatIndex ||
              (trigger.objectiveId !== rule.objectiveId &&
               (objectives.indexOf(trigger.objectiveId) + 1 !==
                  objectives.indexOf(rule.objectiveId) || !selector))) {
            fail(`${cue.id} combat attempt evidence is not an authored transition`);
          }
        }
      }
      attemptRuleSources.add(rule.source);
      attemptRuleKeys.push(JSON.stringify([
        rule.source,
        rule.objectiveId,
        rule.primaryResult.resultId,
        rule.primaryResult.status,
        rule.primaryResult.rejectionReason,
        rule.secondaryResult.resultId,
        rule.secondaryResult.status,
        rule.secondaryResult.rejectionReason
      ]));
    }
    assertUnique(attemptRuleKeys, `${cue.id} attempt evidence selector`);
    if (cue.sources.some((source) => !attemptRuleSources.has(source))) {
      fail(`${cue.id} is missing attempt evidence for an authored source`);
    }
  }
  const expectedQuestUiCues = [
    {
      id: "ui.f1.rain.choice.arrival-clue",
      beatId: "f1_beat_rain_ferry_arrival",
      sources: ["choice_available", "interaction_feedback"],
      objectiveIds: ["f1_objective_choose_arrival_clue"],
      resultSelectors: []
    },
    {
      id: "ui.f1.rain.choice.mooring-method",
      beatId: "f1_beat_rain_ferry_arrival",
      sources: ["choice_available"],
      objectiveIds: ["f1_objective_choose_mooring_method"],
      resultSelectors: []
    },
    {
      id: "ui.f1.rain.mooring-load",
      beatId: "f1_beat_rain_ferry_arrival",
      sources: ["interaction_feedback"],
      objectiveIds: ["f1_objective_secure_ferry_mooring"],
      resultSelectors: [{
        source: "interaction_feedback",
        objectiveId: "f1_objective_secure_ferry_mooring",
        primaryResultId: "f1_interaction_choose_quick_hitch",
        secondaryResultId: null,
        polarityOverride: "negative"
      }]
    },
    {
      id: "ui.f1.rain.bell-feedback",
      beatId: "f1_beat_rain_ferry_arrival",
      sources: ["interaction_feedback"],
      objectiveIds: ["f1_objective_sound_workshop_bell"],
      resultSelectors: []
    },
    {
      id: "ui.f1.training.choice.lane",
      beatId: "f1_beat_shen_yan_training",
      sources: ["choice_available"],
      objectiveIds: ["f1_objective_choose_training_lane"],
      resultSelectors: []
    },
    {
      id: "ui.f1.training.phase",
      beatId: "f1_beat_shen_yan_training",
      sources: ["objective_state"],
      objectiveIds: [
        "f1_objective_eavesguard_counter",
        "f1_objective_flower_turn_counter"
      ],
      resultSelectors: []
    },
    {
      id: "ui.f1.training.action-proof",
      beatId: "f1_beat_shen_yan_training",
      sources: ["combat_feedback"],
      objectiveIds: [
        "f1_objective_eavesguard_counter",
        "f1_objective_commit_eavesguard_heavy",
        "f1_objective_break_eavesguard_target",
        "f1_objective_enter_flower_turn",
        "f1_objective_flower_turn_counter",
        "f1_objective_commit_flower_turn_light",
        "f1_objective_commit_flower_turn_heavy",
        "f1_objective_break_flower_turn_target"
      ],
      resultSelectors: [
        {
          source: "combat_feedback",
          objectiveId: "f1_objective_break_eavesguard_target",
          primaryResultId: "f1_trigger_eavesguard_heavy",
          secondaryResultId: "f1_outcome_break_eavesguard_target",
          polarityOverride: "none"
        },
        {
          source: "combat_feedback",
          objectiveId: "f1_objective_break_flower_turn_target",
          primaryResultId: "f1_trigger_flower_turn_heavy",
          secondaryResultId: "f1_outcome_break_flower_turn_target",
          polarityOverride: "none"
        }
      ]
    },
    {
      id: "ui.f1.training.recovery",
      beatId: "f1_beat_shen_yan_training",
      sources: ["recovery_offer", "recovery_resume"],
      objectiveIds: [
        "f1_objective_eavesguard_counter",
        "f1_objective_flower_turn_counter"
      ],
      resultSelectors: []
    }
  ];
  const questUiCueShape = uiCues.map(({ attemptEvidenceRules, ...cue }) => cue);
  if (!sameValues(questUiCueShape, expectedQuestUiCues)) {
    fail("the first-two-beat quest UI cue contract drifted");
  }
  const actualAttemptEvidence = uiCues.flatMap((cue) =>
    cue.attemptEvidenceRules.map((rule) => [
      cue.id,
      rule.source,
      rule.objectiveId,
      rule.primaryResult.resultId,
      rule.primaryResult.status,
      rule.primaryResult.rejectionReason,
      rule.secondaryResult.resultId,
      rule.secondaryResult.status,
      rule.secondaryResult.rejectionReason,
      rule.classification
    ])
  );
  const expectedAttemptEvidence = [
    ["ui.f1.rain.choice.arrival-clue", "choice_available", "f1_objective_choose_arrival_clue", null, "not_applicable", "none", null, "not_applicable", "none", "qualifying_first_visit"],
    ["ui.f1.rain.choice.arrival-clue", "interaction_feedback", "f1_objective_choose_arrival_clue", "f1_interaction_arrival_clue_drowned_manifest", "ignored_repeat", "selection_already_committed", null, "not_applicable", "none", "repeat_no_progress"],
    ["ui.f1.rain.choice.mooring-method", "choice_available", "f1_objective_choose_mooring_method", null, "not_applicable", "none", null, "not_applicable", "none", "qualifying_craft_decision"],
    ["ui.f1.rain.mooring-load", "interaction_feedback", "f1_objective_secure_ferry_mooring", "f1_interaction_lock_cross_belay", "accepted", "none", null, "not_applicable", "none", "qualifying_craft_decision"],
    ["ui.f1.rain.mooring-load", "interaction_feedback", "f1_objective_secure_ferry_mooring", "f1_interaction_choose_quick_hitch", "accepted", "none", null, "not_applicable", "none", "qualifying_error_feedback"],
    ["ui.f1.rain.bell-feedback", "interaction_feedback", "f1_objective_sound_workshop_bell", "f1_interaction_sound_workshop_bell", "rejected", "prerequisite_incomplete", null, "not_applicable", "none", "qualifying_wrong_order_feedback"],
    ["ui.f1.rain.bell-feedback", "interaction_feedback", "f1_objective_sound_workshop_bell", "f1_interaction_sound_workshop_bell", "accepted", "none", null, "not_applicable", "none", "qualifying_craft_confirmation"],
    ["ui.f1.training.choice.lane", "choice_available", "f1_objective_choose_training_lane", null, "not_applicable", "none", null, "not_applicable", "none", "qualifying_dialogue_decision"],
    ["ui.f1.training.phase", "objective_state", "f1_objective_eavesguard_counter", null, "not_applicable", "none", null, "not_applicable", "none", "qualifying_training_risk"],
    ["ui.f1.training.phase", "objective_state", "f1_objective_flower_turn_counter", null, "not_applicable", "none", null, "not_applicable", "none", "qualifying_training_risk"],
    ["ui.f1.training.action-proof", "combat_feedback", "f1_objective_eavesguard_counter", "f1_trigger_eavesguard_counter", "accepted", "none", null, "not_applicable", "none", "qualifying_combat_proof"],
    ["ui.f1.training.action-proof", "combat_feedback", "f1_objective_break_flower_turn_target", "f1_trigger_flower_turn_heavy", "accepted", "none", "f1_outcome_break_flower_turn_target", "rejected", "wrong_target", "qualifying_combat_feedback"],
    ["ui.f1.training.recovery", "recovery_offer", "f1_objective_eavesguard_counter", null, "not_applicable", "none", null, "not_applicable", "none", "failure_retry_excluded"],
    ["ui.f1.training.recovery", "recovery_resume", "f1_objective_eavesguard_counter", null, "not_applicable", "none", null, "not_applicable", "none", "resume_no_duplicate_progress"],
    ["ui.f1.training.recovery", "recovery_offer", "f1_objective_flower_turn_counter", null, "not_applicable", "none", null, "not_applicable", "none", "failure_retry_excluded"],
    ["ui.f1.training.recovery", "recovery_resume", "f1_objective_flower_turn_counter", null, "not_applicable", "none", null, "not_applicable", "none", "resume_no_duplicate_progress"]
  ];
  if (!sameValues(actualAttemptEvidence, expectedAttemptEvidence)) {
    fail("the first-two-beat attempt evidence contract drifted");
  }
  if (
    contract.paddingPolicy?.repeatableCombatCountsTowardTarget !== false ||
    contract.paddingPolicy?.forcedWaitingCountsTowardTarget !== false ||
    contract.paddingPolicy?.failureRetryCountsTowardTarget !== false
  ) {
    fail("padding policy must reject combat repetition, waiting, and failure retries");
  }

  const requiredPorts = [
    "IContentDefinitionProvider",
    "IWorldCellSource",
    "ICombatResolver",
    "ICombatEventSink",
    "IEncounterDirector",
    "IQuestRuntime",
    "ISnapshotContributor",
    "IPresentationEventSink",
    "IAssetResolver"
  ];
  if (!sameValues(contract.ports?.map((port) => port.name), requiredPorts)) {
    fail("reserved interface list drifted");
  }
  if (contract.ports[0].status !== "bootstrap_implemented") {
    fail("the built-in definition provider must be marked as bootstrap implemented");
  }
  const implementedPorts = new Set([
    "IContentDefinitionProvider",
    "ICombatResolver",
    "ICombatEventSink",
    "IEncounterDirector",
    "IQuestRuntime"
  ]);
  for (const port of contract.ports) {
    const expected = implementedPorts.has(port.name) ? "bootstrap_implemented" : "reserved";
    if (port.status !== expected) fail(`${port.name} must remain ${expected}`);
  }

  const seed = contract.playerSeed;
  if (!Number.isSafeInteger(seed?.actorKey) || seed.actorKey <= 0) fail("invalid player actor key");
  for (const field of [
    "moveSpeedMmPerSecond",
    "jumpSpeedMmPerSecond",
    "gravityMmPerSecondSquared",
    "collisionRadiusMm",
    "collisionHeightMm"
  ]) {
    if (!Number.isInteger(seed[field]) || seed[field] <= 0 || seed[field] > 100000) {
      fail(`invalid playerSeed.${field}`);
    }
  }
  const basis = seed.cameraBasisQ15;
  const right = basis?.screenRightWorld;
  const forward = basis?.screenForwardWorld;
  const target = 32767 * 32767;
  const tolerance = Math.floor(target / 100);
  const rightLength = right.x * right.x + right.y * right.y;
  const forwardLength = forward.x * forward.x + forward.y * forward.y;
  const dot = right.x * forward.x + right.y * forward.y;
  const determinant = right.x * forward.y - right.y * forward.x;
  if (
    basis.revision <= 1 ||
    Math.abs(rightLength - target) > tolerance ||
    Math.abs(forwardLength - target) > tolerance ||
    Math.abs(dot) > tolerance ||
    Math.abs(determinant - target) > tolerance
  ) {
    fail("camera basis must be a right-handed unit oblique basis with a new revision");
  }

  const combat = contract.combatBootstrap;
  if (combat?.id !== "f1_encounter_umbrella_lane_bootstrap") {
    fail("combat bootstrap id drifted");
  }
  if (!Array.isArray(combat.actors) || combat.actors.length < 2 || combat.actors.length > 16) {
    fail("combat bootstrap requires 2..16 actors");
  }
  if (!Array.isArray(combat.abilities) || combat.abilities.length === 0 || combat.abilities.length > 32) {
    fail("combat bootstrap requires 1..32 abilities");
  }
  const director = combat.director;
  if (
    director?.playerActorKey !== seed.actorKey ||
    !Number.isInteger(director.aggroRangeMm) ||
    director.aggroRangeMm <= 0 ||
    !Number.isInteger(director.leashRangeMm) ||
    director.leashRangeMm < director.aggroRangeMm ||
    !Number.isInteger(director.chaseSpeedMmPerSecond) ||
    director.chaseSpeedMmPerSecond <= 0 ||
    director.chaseSpeedMmPerSecond % 60 !== 0 ||
    !Number.isInteger(director.formationRadiusMm) ||
    director.formationRadiusMm <= 0 ||
    director.formationRadiusMm >= director.aggroRangeMm ||
    !Number.isInteger(director.decisionIntervalTicks) ||
    director.decisionIntervalTicks <= 0 ||
    !Number.isInteger(director.postAttackCooldownTicks) ||
    director.postAttackCooldownTicks < 0 ||
    !Number.isInteger(director.maxSimultaneousAttackers) ||
    director.maxSimultaneousAttackers <= 0 ||
    director.maxSimultaneousAttackers > 15
  ) {
    fail("combat encounter director definition is invalid");
  }
  assertUnique(combat.actors.map((actor) => actor.actorKey), "combat actor key");
  const stanceIds = new Set();
  const resourcePairs = [
    ["health", "healthMax", true],
    ["stamina", "staminaMax", true],
    ["poise", "poiseMax", true],
    ["lantern", "lanternMax", false]
  ];
  for (const actor of combat.actors) {
    if (!Number.isSafeInteger(actor.actorKey) || actor.actorKey <= 0) {
      fail("invalid combat actor key");
    }
    if (!["player", "hostile", "neutral"].includes(actor.faction)) {
      fail(`${actor.actorKey} has an invalid faction`);
    }
    if (!Array.isArray(actor.stanceIds) || actor.stanceIds.length === 0 || actor.stanceIds.length > 4) {
      fail(`${actor.actorKey} must declare 1..4 stances`);
    }
    assertUnique(actor.stanceIds, `${actor.actorKey} stance`);
    if (!actor.stanceIds.includes(actor.initialStanceId)) {
      fail(`${actor.actorKey} initial stance is not allowed`);
    }
    if (typeof actor.initiallyActive !== "boolean" ||
        (actor.faction === "player" && !actor.initiallyActive)) {
      fail(`${actor.actorKey} has an invalid initial active state`);
    }
    for (const stance of actor.stanceIds) stanceIds.add(stance);
    for (const [valueField, maxField, positiveMax] of resourcePairs) {
      const value = actor.resources?.[valueField];
      const maximum = actor.resources?.[maxField];
      if (
        !Number.isInteger(value) ||
        !Number.isInteger(maximum) ||
        value < 0 ||
        maximum < (positiveMax ? 1 : 0) ||
        value > maximum
      ) {
        fail(`${actor.actorKey} has invalid ${valueField}/${maxField}`);
      }
    }
    if (!Number.isInteger(actor.resources.evidence) || actor.resources.evidence < 0) {
      fail(`${actor.actorKey} has invalid evidence`);
    }
    for (const [field, maximum] of [
      ["staminaDelayTicks", 3600],
      ["staminaIntervalTicks", 600],
      ["staminaPerInterval", 100000],
      ["poiseDelayTicks", 3600],
      ["poiseIntervalTicks", 600],
      ["poisePerInterval", 100000]
    ]) {
      if (
        !Number.isInteger(actor.recovery?.[field]) ||
        actor.recovery[field] <= 0 ||
        actor.recovery[field] > maximum
      ) {
        fail(`${actor.actorKey} has invalid recovery field ${field}`);
      }
    }
  }
  for (const trigger of combatTriggers) {
    if (!stanceIds.has(trigger.requiredStanceId)) {
      fail(`${trigger.id} references an unknown required stance`);
    }
    if (trigger.kind === "player_ability_started") {
      const ability = combat.abilities.find(
        (candidate) => candidate.id === trigger.requiredAbilityId
      );
      if (ability?.requiredStanceId !== trigger.requiredStanceId) {
        fail(`${trigger.id} references an incompatible required ability`);
      }
    }
  }
  for (const outcome of combatOutcomes) {
    const outcomeBeat = contract.beats.find(
      (beat) => beat.objectiveIds.includes(outcome.objectiveId)
    );
    const reachableActorKeys = new Set(
      encounterActivations
        .filter((activation) => activation.beatId === outcomeBeat?.id)
        .flatMap((activation) => activation.actorKeys)
    );
    const matchingHostiles = combat.actors.filter(
      (actor) => actor.faction === "hostile" &&
        reachableActorKeys.has(actor.actorKey) &&
        actor.archetypeId === outcome.archetypeId
    ).length;
    if (matchingHostiles < outcome.requiredCount) {
      fail(`${outcome.id} cannot reach its required hostile count`);
    }
  }
  const playerCombatSeed = combat.actors.find((actor) => actor.actorKey === seed.actorKey);
  if (
    playerCombatSeed?.faction !== "player" ||
    !sameValues(playerCombatSeed.poseMm, seed.initialPoseMm)
  ) {
    fail("combat player seed must match the movement player seed");
  }
  const hostileCount = combat.actors.filter((actor) => actor.faction === "hostile").length;
  if (director.maxSimultaneousAttackers > hostileCount) {
    fail("combat attack token count exceeds hostile actors");
  }
  if (combat.actors.some((actor) => actor.faction === "hostile" && actor.initiallyActive)) {
    fail("beat-scoped hostile actors must start dormant");
  }
  const trainingUmbrella = combat.actors.find((actor) => actor.actorKey === 104);
  const trainingEavesguardTarget = combat.actors.find((actor) => actor.actorKey === 107);
  const trainingFlowerRig = combat.actors.find((actor) => actor.actorKey === 108);
  const trainingFlowerTarget = combat.actors.find((actor) => actor.actorKey === 109);
  if (trainingUmbrella?.archetypeId !== "f1_training_umbrella_rig" ||
      trainingUmbrella.initialStanceId !== "stance_umbrella_rust" ||
      trainingUmbrella.initiallyActive !== false ||
      trainingUmbrella.resources.health !== 999 ||
      !sameValues(trainingUmbrella.poseMm, { x: -4100, y: 2300, height: 0, floorLayer: 0 }) ||
      trainingEavesguardTarget?.archetypeId !== "f1_training_eavesguard_target" ||
      trainingEavesguardTarget.initialStanceId !== "stance_umbrella_rust" ||
      trainingEavesguardTarget.initiallyActive !== false ||
      trainingEavesguardTarget.resources.health !== 120 ||
      !sameValues(trainingEavesguardTarget.poseMm, { x: -4000, y: 2000, height: 0, floorLayer: 0 }) ||
      trainingFlowerRig?.archetypeId !== "f1_training_flower_turn_rig" ||
      trainingFlowerRig.initialStanceId !== "stance_paper_egret" ||
      trainingFlowerRig.initiallyActive !== false ||
      trainingFlowerRig.resources.health !== 999 ||
      !sameValues(trainingFlowerRig.poseMm, { x: -3300, y: 1200, height: 700, floorLayer: 0 }) ||
      trainingFlowerTarget?.archetypeId !== "f1_training_flower_turn_target" ||
      trainingFlowerTarget.initialStanceId !== "stance_paper_egret" ||
      trainingFlowerTarget.initiallyActive !== false ||
      trainingFlowerTarget.resources.health !== 120 ||
      !sameValues(trainingFlowerTarget.poseMm, { x: -3000, y: -800, height: 700, floorLayer: 0 })) {
    fail("the Shen Yan safety rigs and defeat targets drifted from their authored roles");
  }
  const springReturnUmbrella = combat.actors.find((actor) => actor.actorKey === 106);
  const referenceUmbrella = combat.actors.find((actor) => actor.actorKey === 101);
  if (springReturnUmbrella?.archetypeId !== referenceUmbrella?.archetypeId ||
      springReturnUmbrella.initialStanceId !== "stance_umbrella_rust" ||
      springReturnUmbrella.initiallyActive !== false ||
      springReturnUmbrella.resources.health !== 70 ||
      !sameValues(
        springReturnUmbrella.poseMm,
        { x: -4600, y: 1600, height: 0, floorLayer: 0 }
      )) {
    fail("the spring return umbrella variant drifted from its authored role");
  }
  const bossActor = combat.actors.find((actor) => actor.actorKey === 201);
  if (bossActor?.archetypeId !== refs.bossId || bossActor.faction !== "hostile" ||
      bossActor.initiallyActive !== false || !sameValues(bossActor.stanceIds, [
        "stance_wraith_spring",
        "stance_wraith_summer",
        "stance_wraith_autumn",
        "stance_wraith_winter"
      ]) || bossActor.initialStanceId !== "stance_wraith_spring" ||
      bossPhases.some((phase) => phase.actorKey !== bossActor.actorKey) ||
      bossPhases.some((phase) => phase.nextStanceId !== null &&
        !bossActor.stanceIds.includes(phase.nextStanceId))) {
    fail("the inactive four-seasons boss actor does not match its authored phases");
  }

  assertUnique(combat.abilities.map((ability) => ability.id), "combat ability id");
  const triggerKeys = [];
  const coveredStanceTriggers = new Set();
  let evadeCount = 0;
  const feedbackTags = new Set(["light", "heavy", "guard", "poise_break", "evade"]);
  for (const ability of combat.abilities) {
    if (!["light_attack", "heavy_attack", "evade"].includes(ability.trigger)) {
      fail(`${ability.id} has an invalid trigger`);
    }
    if (ability.trigger === "evade") {
      if (ability.requiredStanceId !== undefined) fail(`${ability.id} evade must be stance-neutral`);
      evadeCount += 1;
    } else if (!stanceIds.has(ability.requiredStanceId)) {
      fail(`${ability.id} references an unknown stance`);
    }
    const triggerKey = `${ability.trigger}:${ability.requiredStanceId ?? "*"}`;
    triggerKeys.push(triggerKey);
    coveredStanceTriggers.add(triggerKey);
    for (const field of [
      "staminaCost",
      "windupTicks",
      "recoveryTicks",
      "rangeMm",
      "heightToleranceMm",
      "healthDamage",
      "poiseDamage"
    ]) {
      if (!Number.isInteger(ability[field]) || ability[field] < 0 || ability[field] > 100000) {
        fail(`${ability.id} has invalid ${field}`);
      }
    }
    if (!Number.isInteger(ability.activeTicks) || ability.activeTicks <= 0 || ability.activeTicks > 600) {
      fail(`${ability.id} has invalid activeTicks`);
    }
    if (!Array.isArray(ability.feedbackTags) || ability.feedbackTags.length === 0) {
      fail(`${ability.id} has no feedback tags`);
    }
    assertUnique(ability.feedbackTags, `${ability.id} feedback tag`);
    if (ability.feedbackTags.some((tag) => !feedbackTags.has(tag))) {
      fail(`${ability.id} has an unknown feedback tag`);
    }
  }
  assertUnique(triggerKeys, "combat trigger and stance");
  if (evadeCount !== 1) fail("combat bootstrap requires one shared evade ability");
  for (const stance of stanceIds) {
    for (const trigger of ["light_attack", "heavy_attack"]) {
      if (!coveredStanceTriggers.has(`${trigger}:${stance}`)) {
        fail(`${stance} is missing ${trigger}`);
      }
    }
  }

  const allStableIds = [
    contract.id,
    refs.startFixtureId,
    refs.chapterId,
    ...refs.subregionIds,
    ...refs.npcIds,
    ...refs.enemyFamilyIds,
    refs.bossId,
    ...contract.cellIds,
    ...beatIds,
    ...objectiveIds,
    ...safePoints.map((safePoint) => safePoint.id),
    ...interactions.map((interaction) => interaction.id),
    ...interactions.map((interaction) => interaction.selectionId).filter(Boolean),
    ...encounterActivations.map((activation) => activation.id),
    ...bossPhases.map((phase) => phase.id),
    ...resolutionRewards.flatMap((reward) => [
      reward.id,
      reward.rewardId,
      reward.rewardDedupKey
    ]),
    ...combatTriggers.map((trigger) => trigger.id),
    ...combatOutcomes.map((outcome) => outcome.id),
    ...uiCues.map((cue) => cue.id),
    combat.id,
    ...combat.actors.map((actor) => actor.archetypeId),
    ...stanceIds,
    ...combat.abilities.map((ability) => ability.id)
  ];
  assertHashUnique([...new Set(allStableIds)], "stable content id");
  return contract;
}

function contentId(value) {
  return `contracts::content_id("${value}")`;
}

function arrayRows(values) {
  return values.map((value) => `    ${contentId(value)},`).join("\n");
}

function beatKind(value) {
  return `contracts::VerticalSliceBeatKind::${value}`;
}

function safePointRow(safePoint) {
  const pose = safePoint.poseMm;
  return `    {${contentId(safePoint.id)}, ${contentId(safePoint.beatId)}, {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}}},`;
}

function stableKey(value) {
  return value ? `contracts::stable_content_key("${value}")` : "0";
}

function combatActorRow(actor) {
  const pose = actor.poseMm;
  const resources = actor.resources;
  const recovery = actor.recovery;
  const stances = [...actor.stanceIds.map(stableKey), ...Array(4 - actor.stanceIds.length).fill("0")];
  return `    {${actor.actorKey}ULL, ${contentId(actor.archetypeId)}, contracts::CombatFaction::${actor.faction}, {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}}, {${resources.health}, ${resources.healthMax}, ${resources.stamina}, ${resources.staminaMax}, ${resources.poise}, ${resources.poiseMax}, ${resources.lantern}, ${resources.lanternMax}, ${resources.evidence}}, {${stances.join(", ")}}, ${actor.stanceIds.length}U, ${stableKey(actor.initialStanceId)}, {${recovery.staminaDelayTicks}, ${recovery.staminaIntervalTicks}, ${recovery.staminaPerInterval}, ${recovery.poiseDelayTicks}, ${recovery.poiseIntervalTicks}, ${recovery.poisePerInterval}}, ${actor.initiallyActive}},`;
}

function combatAbilityRow(ability) {
  const feedback = ability.feedbackTags
    .map((tag) => `contracts::feedback_${tag}`)
    .join(" | ");
  return `    {${contentId(ability.id)}, contracts::CombatCommandType::${ability.trigger}, ${stableKey(ability.requiredStanceId)}, ${ability.staminaCost}, ${ability.windupTicks}, ${ability.activeTicks}, ${ability.recoveryTicks}, ${ability.rangeMm}, ${ability.heightToleranceMm}, ${ability.healthDamage}, ${ability.poiseDamage}, ${feedback}},`;
}

function questInteractionRow(interaction, index) {
  const pose = interaction.poseMm;
  const prerequisites = interaction.prerequisiteObjectiveIds.length === 0
    ? "std::span<const contracts::ContentId>{}"
    : `std::span<const contracts::ContentId>{interaction_${index}_prerequisites}`;
  const selection = interaction.selectionId === null ? "contracts::ContentId{}" : contentId(interaction.selectionId);
  const requiredSelectionObjective = interaction.requiredSelectionObjectiveId === null
    ? "contracts::ContentId{}"
    : contentId(interaction.requiredSelectionObjectiveId);
  const requiredSelection = interaction.requiredSelectionId === null
    ? "contracts::ContentId{}"
    : contentId(interaction.requiredSelectionId);
  return `    {${contentId(interaction.id)}, contracts::QuestInteractionKind::${interaction.kind}, ${contentId(interaction.cellId)}, ${contentId(interaction.objectiveId)}, ${selection}, ${requiredSelectionObjective}, ${requiredSelection}, {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}}, ${interaction.radiusMm}, ${prerequisites}},`;
}

function questCombatTriggerRow(trigger, index) {
  const requiredSelectionObjective = trigger.requiredSelectionObjectiveId === null
    ? "contracts::ContentId{}"
    : contentId(trigger.requiredSelectionObjectiveId);
  const requiredSelection = trigger.requiredSelectionId === null
    ? "contracts::ContentId{}"
    : contentId(trigger.requiredSelectionId);
  return `    {${contentId(trigger.id)}, contracts::QuestCombatTriggerKind::${trigger.kind}, ${contentId(trigger.objectiveId)}, ${stableKey(trigger.requiredStanceId)}, ${stableKey(trigger.requiredAbilityId)}, ${requiredSelectionObjective}, ${requiredSelection}, std::span<const contracts::ContentId>{combat_trigger_${index}_prerequisites}},`;
}

function questCombatOutcomeRow(outcome) {
  const archetype = outcome.archetypeId === null
    ? "contracts::ContentId{}"
    : contentId(outcome.archetypeId);
  return `    {${contentId(outcome.id)}, contracts::QuestCombatOutcomeKind::${outcome.kind}, ${contentId(outcome.objectiveId)}, ${archetype}, ${outcome.requiredCount}U},`;
}

function questEncounterActivationRow(activation, index) {
  const triggerObjective = activation.triggerObjectiveId === null
    ? "contracts::ContentId{}"
    : contentId(activation.triggerObjectiveId);
  const requiredSelectionObjective = activation.requiredSelectionObjectiveId === null
    ? "contracts::ContentId{}"
    : contentId(activation.requiredSelectionObjectiveId);
  const requiredSelection = activation.requiredSelectionId === null
    ? "contracts::ContentId{}"
    : contentId(activation.requiredSelectionId);
  return `    {${contentId(activation.id)}, ${contentId(activation.beatId)}, ${triggerObjective}, ${requiredSelectionObjective}, ${requiredSelection}, contracts::EncounterActivationMode::${activation.mode}, ${contentId(activation.encounterId)}, std::span<const contracts::StableActorKey>{encounter_activation_${index}_actors}, std::span<const contracts::EncounterActorPlacementDefinition>{encounter_activation_${index}_placements}},`;
}

function questBossPhaseRow(phase) {
  return `    {${contentId(phase.id)}, ${contentId(phase.objectiveId)}, ${phase.actorKey}ULL, ${phase.healthPercent}U, ${stableKey(phase.nextStanceId)}},`;
}

function questResolutionRewardRow(reward) {
  return `    {${contentId(reward.id)}, ${contentId(reward.objectiveId)}, ${contentId(reward.selectionId)}, ${contentId(reward.rewardId)}, ${contentId(reward.rewardDedupKey)}},`;
}

function questUiSourceMask(sources) {
  return sources
    .map(
      (source) =>
        `contracts::quest_ui_projection_source_bit(contracts::QuestUiProjectionSource::${source})`
    )
    .join(" | ");
}

function questUiResultSelectorRow(selector) {
  const secondary = selector.secondaryResultId === null
    ? "contracts::ContentId{}"
    : contentId(selector.secondaryResultId);
  return `    {contracts::QuestUiProjectionSource::${selector.source}, ${contentId(selector.objectiveId)}, ${contentId(selector.primaryResultId)}, ${secondary}, contracts::QuestUiPolarityOverride::${selector.polarityOverride}},`;
}

function questUiAttemptEvidenceResultRow(result) {
  const resultId = result.resultId === null
    ? "contracts::ContentId{}"
    : contentId(result.resultId);
  return `{${resultId}, contracts::QuestUiResultStatus::${result.status}, contracts::QuestUiRejectionReason::${result.rejectionReason}}`;
}

function questUiAttemptEvidenceRuleRow(rule) {
  return `    {contracts::QuestUiProjectionSource::${rule.source}, ${contentId(rule.objectiveId)}, ${questUiAttemptEvidenceResultRow(rule.primaryResult)}, ${questUiAttemptEvidenceResultRow(rule.secondaryResult)}, contracts::QuestUiAttemptTimeClassification::${rule.classification}},`;
}

function questUiCueRow(cue, index) {
  const selectors = cue.resultSelectors.length === 0
    ? "std::span<const contracts::QuestUiResultSelectorDefinition>{}"
    : `std::span<const contracts::QuestUiResultSelectorDefinition>{quest_ui_cue_${index}_selectors}`;
  const attemptEvidence =
    `std::span<const contracts::QuestUiAttemptEvidenceRuleDefinition>{quest_ui_cue_${index}_attempt_evidence}`;
  return `    {${contentId(cue.id)}, ${contentId(cue.beatId)}, ${questUiSourceMask(cue.sources)}, std::span<const contracts::ContentId>{quest_ui_cue_${index}_objectives}, ${selectors}, ${attemptEvidence}},`;
}

export function renderF1SliceContract(contract) {
  const refs = contract.catalogReferences;
  const seed = contract.playerSeed;
  const combat = contract.combatBootstrap;
  const director = combat.director;
  const objectiveArrays = contract.beats
    .map(
      (beat, index) => `inline constexpr std::array<contracts::ContentId, ${beat.objectiveIds.length}> beat_${index}_objectives{{
${arrayRows(beat.objectiveIds)}
}};`
    )
    .join("\n\n");
  const beatRows = contract.beats
    .map(
      (beat, index) =>
        `    {${contentId(beat.id)}, ${beatKind(beat.kind)}, ${beat.targetMinutes}, ${contentId(beat.cellId)}, std::span<const contracts::ContentId>{beat_${index}_objectives}},`
    )
    .join("\n");
  const safePointRows = contract.safePoints.map(safePointRow).join("\n");
  const pose = seed.initialPoseMm;
  const basis = seed.cameraBasisQ15;
  const combatActorRows = combat.actors.map(combatActorRow).join("\n");
  const combatAbilityRows = combat.abilities.map(combatAbilityRow).join("\n");
  const interactionPrerequisiteArrays = contract.questInteractions
    .map((interaction, index) => ({ interaction, index }))
    .filter(({ interaction }) => interaction.prerequisiteObjectiveIds.length !== 0)
    .map(
      ({ interaction, index }) => `inline constexpr std::array<contracts::ContentId, ${interaction.prerequisiteObjectiveIds.length}> interaction_${index}_prerequisites{{
${arrayRows(interaction.prerequisiteObjectiveIds)}
}};`
    )
    .join("\n\n");
  const combatTriggerPrerequisiteArrays = contract.questCombatTriggers
    .map(
      (trigger, index) => `inline constexpr std::array<contracts::ContentId, ${trigger.prerequisiteObjectiveIds.length}> combat_trigger_${index}_prerequisites{{
${arrayRows(trigger.prerequisiteObjectiveIds)}
}};`
    )
    .join("\n\n");
  const questInteractionRows = contract.questInteractions.map(questInteractionRow).join("\n");
  const questCombatTriggerRows = contract.questCombatTriggers
    .map(questCombatTriggerRow)
    .join("\n");
  const questCombatOutcomeRows = contract.questCombatOutcomes
    .map(questCombatOutcomeRow)
    .join("\n");
  const questEncounterActivationRows = contract.questEncounterActivations
    .map(questEncounterActivationRow)
    .join("\n");
  const questEncounterActivationActorArrays = contract.questEncounterActivations
    .map(
      (activation, index) => `inline constexpr std::array<contracts::StableActorKey, ${activation.actorKeys.length}> encounter_activation_${index}_actors{{${activation.actorKeys.map((actor) => `\n    ${actor}ULL,`).join("")}\n}};`
    )
    .join("\n\n");
  const questEncounterActivationPlacementArrays = contract.questEncounterActivations
    .map(
      (activation, index) => `inline constexpr std::array<contracts::EncounterActorPlacementDefinition, ${activation.actorPlacements.length}> encounter_activation_${index}_placements{{${activation.actorPlacements.map((placement) => `\n    {${placement.actorKey}ULL, {${placement.poseMm.x}, ${placement.poseMm.y}, ${placement.poseMm.height}, ${placement.poseMm.floorLayer}}, ${placement.formationSlot}U},`).join("")}\n}};`
    )
    .join("\n\n");
  const questBossPhaseRows = contract.questBossPhases.map(questBossPhaseRow).join("\n");
  const questResolutionRewardRows = contract.questResolutionRewards
    .map(questResolutionRewardRow)
    .join("\n");
  const questUiCueObjectiveArrays = contract.questUiCues
    .map(
      (cue, index) => `inline constexpr std::array<contracts::ContentId, ${cue.objectiveIds.length}> quest_ui_cue_${index}_objectives{{
${arrayRows(cue.objectiveIds)}
}};`
    )
    .join("\n\n");
  const questUiCueSelectorArrays = contract.questUiCues
    .map((cue, index) => ({ cue, index }))
    .filter(({ cue }) => cue.resultSelectors.length !== 0)
    .map(
      ({ cue, index }) => `inline constexpr std::array<contracts::QuestUiResultSelectorDefinition, ${cue.resultSelectors.length}> quest_ui_cue_${index}_selectors{{
${cue.resultSelectors.map(questUiResultSelectorRow).join("\n")}
}};`
    )
    .join("\n\n");
  const questUiCueAttemptEvidenceArrays = contract.questUiCues
    .map(
      (cue, index) => `inline constexpr std::array<contracts::QuestUiAttemptEvidenceRuleDefinition, ${cue.attemptEvidenceRules.length}> quest_ui_cue_${index}_attempt_evidence{{
${cue.attemptEvidenceRules.map(questUiAttemptEvidenceRuleRow).join("\n")}
}};`
    )
    .join("\n\n");
  const questUiCueRows = contract.questUiCues.map(questUiCueRow).join("\n");

  return `// Generated from content/design/f1-vertical-slice.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/combat_types.hpp>
#include <tgd/contracts/content_definition.hpp>

#include <array>
#include <span>

namespace tgd::content::generated {

${objectiveArrays}

${interactionPrerequisiteArrays}

${combatTriggerPrerequisiteArrays}

${questEncounterActivationActorArrays}

${questEncounterActivationPlacementArrays}

${questUiCueObjectiveArrays}

${questUiCueSelectorArrays}

${questUiCueAttemptEvidenceArrays}

inline constexpr std::array<contracts::VerticalSliceBeatDefinition, ${contract.beats.length}> f1_beats{{
${beatRows}
}};

inline constexpr std::array<contracts::VerticalSliceSafePointDefinition, ${contract.safePoints.length}> f1_safe_points{{
${safePointRows}
}};

inline constexpr std::array<contracts::ContentId, ${refs.subregionIds.length}> f1_subregions{{
${arrayRows(refs.subregionIds)}
}};

inline constexpr std::array<contracts::ContentId, ${refs.npcIds.length}> f1_npcs{{
${arrayRows(refs.npcIds)}
}};

inline constexpr std::array<contracts::ContentId, ${refs.enemyFamilyIds.length}> f1_enemy_families{{
${arrayRows(refs.enemyFamilyIds)}
}};

inline constexpr std::array<contracts::ContentId, ${contract.cellIds.length}> f1_cells{{
${arrayRows(contract.cellIds)}
}};

inline constexpr std::array<contracts::QuestInteractionDefinition, ${contract.questInteractions.length}> f1_quest_interactions{{
${questInteractionRows}
}};

inline constexpr std::array<contracts::QuestCombatTriggerDefinition, ${contract.questCombatTriggers.length}> f1_quest_combat_triggers{{
${questCombatTriggerRows}
}};

inline constexpr std::array<contracts::QuestCombatOutcomeDefinition, ${contract.questCombatOutcomes.length}> f1_quest_combat_outcomes{{
${questCombatOutcomeRows}
}};

inline constexpr std::array<contracts::QuestEncounterActivationDefinition, ${contract.questEncounterActivations.length}> f1_quest_encounter_activations{{
${questEncounterActivationRows}
}};

inline constexpr std::array<contracts::QuestBossPhaseDefinition, ${contract.questBossPhases.length}> f1_quest_boss_phases{{
${questBossPhaseRows}
}};

inline constexpr std::array<contracts::QuestResolutionRewardDefinition, ${contract.questResolutionRewards.length}> f1_quest_resolution_rewards{{
${questResolutionRewardRows}
}};

inline constexpr std::array<contracts::QuestUiCueDefinition, ${contract.questUiCues.length}> f1_quest_ui_cues{{
${questUiCueRows}
}};

inline constexpr std::array<contracts::CombatActorConfig, ${combat.actors.length}> f1_combat_actors{{
${combatActorRows}
}};

inline constexpr std::array<contracts::AbilityDefinition, ${combat.abilities.length}> f1_combat_abilities{{
${combatAbilityRows}
}};

inline constexpr contracts::CombatEncounterDefinition f1_combat_encounter_definition{
    ${contentId(combat.id)},
    std::span<const contracts::CombatActorConfig>{f1_combat_actors},
    std::span<const contracts::AbilityDefinition>{f1_combat_abilities},
    {${director.playerActorKey}ULL, ${director.aggroRangeMm}, ${director.leashRangeMm}, ${director.chaseSpeedMmPerSecond}, ${director.formationRadiusMm}, ${director.decisionIntervalTicks}, ${director.postAttackCooldownTicks}, ${director.maxSimultaneousAttackers}U},
};

inline constexpr contracts::VerticalSliceDefinition f1_vertical_slice_definition{
    ${contentId(contract.id)},
    "${contract.view.model}",
    "${contract.view.primaryGuidance}",
    "${contract.view.secondaryReference}",
    "${contract.view.cameraMode}",
    ${contract.timing.playableTargetMinutes},
    ${contract.timing.endToEndTestBudgetMinutes},
    ${contract.timing.activityGraceTicks},
    ${contentId(refs.startFixtureId)},
    ${contentId(refs.chapterId)},
    ${contentId(refs.bossId)},
    {
        ${seed.actorKey}ULL,
        {${pose.x}, ${pose.y}, ${pose.height}, ${pose.floorLayer}},
        ${seed.moveSpeedMmPerSecond},
        ${seed.jumpSpeedMmPerSecond},
        ${seed.gravityMmPerSecondSquared},
        ${seed.collisionRadiusMm},
        ${seed.collisionHeightMm},
        {
            {${basis.screenRightWorld.x}, ${basis.screenRightWorld.y}},
            {${basis.screenForwardWorld.x}, ${basis.screenForwardWorld.y}},
            ${basis.revision}U,
        },
    },
    std::span<const contracts::ContentId>{f1_subregions},
    std::span<const contracts::ContentId>{f1_npcs},
    std::span<const contracts::ContentId>{f1_enemy_families},
    std::span<const contracts::ContentId>{f1_cells},
    std::span<const contracts::VerticalSliceBeatDefinition>{f1_beats},
    std::span<const contracts::VerticalSliceSafePointDefinition>{f1_safe_points},
    std::span<const contracts::QuestInteractionDefinition>{f1_quest_interactions},
    std::span<const contracts::QuestCombatTriggerDefinition>{f1_quest_combat_triggers},
    std::span<const contracts::QuestCombatOutcomeDefinition>{f1_quest_combat_outcomes},
    std::span<const contracts::QuestEncounterActivationDefinition>{f1_quest_encounter_activations},
    std::span<const contracts::QuestBossPhaseDefinition>{f1_quest_boss_phases},
    std::span<const contracts::QuestResolutionRewardDefinition>{f1_quest_resolution_rewards},
    std::span<const contracts::QuestUiCueDefinition>{f1_quest_ui_cues},
};

}  // namespace tgd::content::generated
`;
}

export async function loadF1SliceContract() {
  const [contract, catalog] = await Promise.all([
    readFile(inputPath, "utf8").then(JSON.parse),
    readFile(catalogPath, "utf8").then(JSON.parse)
  ]);
  return validateF1SliceContract(contract, catalog);
}

async function main() {
  const contract = await loadF1SliceContract();
  const expected = renderF1SliceContract(contract);
  if (process.argv.includes("--check")) {
    const actual = await readFile(outputPath, "utf8").catch(() => "");
    if (actual !== expected) {
      fail("generated C++ contract is stale; run npm run generate:f1-slice-contract");
    }
    console.log("Generated F1 vertical slice contract is current.");
    return;
  }
  await mkdir(dirname(outputPath), { recursive: true });
  await writeFile(outputPath, expected, "utf8");
  console.log(`Generated ${outputPath.slice(root.length + 1).replaceAll("\\", "/")}`);
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  await main();
}
