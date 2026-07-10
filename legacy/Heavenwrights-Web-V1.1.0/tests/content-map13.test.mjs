import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const contentRoot = join(root, "content");

function readJson(path) {
  return JSON.parse(readFileSync(join(contentRoot, path), "utf8"));
}

const localization = readJson("localization/zh-CN.json").strings;
const region = readJson("regions/jiangnan_rain_alley.json");
const subregions = readJson("subregions/jiangnan_rain_alley_subregions.json");
const buildings = readJson("buildings/jiangnan_rain_alley_buildings.json");
const interactions = readJson("interactions/jiangnan_rain_alley_interactions.json");
const enemies = readJson("enemies/rain_alley_m1_enemies.json");
const boss = readJson("bosses/rain_alley_boss.json");
const largeAreaPack = readJson("large_areas/jiangnan_large_areas.json");
const gatherNodePack = readJson("gather_nodes/jiangnan_gather_nodes.json");
const craftRecipePack = readJson("recipes/jiangnan_craft_recipes.json");
const encounterPack = readJson("encounters/jiangnan_encounters.json");
const editorTemplates = readJson("editors/jiangnan_editor_templates.json");

const pointIds = new Set(largeAreaPack.points.map((point) => point.id));
const largeAreaIds = new Set(largeAreaPack.largeAreaIds);
const routeIds = new Set(largeAreaPack.routeIds ?? []);
const buildingIds = new Set(buildings.buildings.map((building) => building.id));
const interactionIds = new Set(interactions.interactions.map((interaction) => interaction.id));
const enemyIds = new Set(enemies.enemies.map((enemy) => enemy.id));
const trapIds = new Set(enemies.traps.map((trap) => trap.id));
const gatherNodeIds = new Set(gatherNodePack.gatherNodes.map((node) => node.id));
const craftRecipeIds = new Set(craftRecipePack.recipes.map((recipe) => recipe.id));
const craftTraitIds = new Set(craftRecipePack.traits.map((trait) => trait.id));
const encounterIds = new Set(encounterPack.encounters.map((encounter) => encounter.id));

function assertFormalRef(ref) {
  if (ref.startsWith("gather.")) assert.ok(gatherNodeIds.has(ref), `${ref} missing from gather node library`);
  if (ref.startsWith("recipe.")) assert.ok(craftRecipeIds.has(ref), `${ref} missing from craft recipe library`);
  if (ref.startsWith("trait.")) assert.ok(craftTraitIds.has(ref), `${ref} missing from craft trait library`);
  if (ref.startsWith("encounter.")) assert.ok(encounterIds.has(ref), `${ref} missing from encounter library`);
}

describe("jiangnan chapter large-map content", () => {
  it("keeps 07 M1 subregions while indexing the 13 large-map layer", () => {
    assert.equal(region.id, "region.jiangnan_rain_alley");
    assert.deepEqual(region.recommendedDurationMinutes, [45, 60]);
    assert.deepEqual(region.chapterTargetDurationMinutes, [300, 600]);
    assert.equal(region.subregionIds.length, 11);
    assert.equal(region.largeAreaIds.length, 6);
    assert.equal(region.pointIds.length, 48);
    assert.deepEqual(region.routeIds, largeAreaPack.routeIds);
    assert.equal(region.corePointIds.length, 8);
    assert.ok(region.corePointIds.includes("point.jiangnan.lake_field_outer_dike.water_lock"));
    assert.ok(region.pointIds.includes("point.jiangnan.lake_field_outer_dike.watch_hut"));
    assert.ok(region.questIds.includes("quest.jiangnan.outer_dike.sluice_supply"));
    assert.ok(region.npcIds.includes("npc.outer_dike_ahu"));
    assert.ok(region.largeAreaSetIds.includes(largeAreaPack.id));
    assert.ok(region.gatherNodeSetIds.includes(gatherNodePack.id));
    assert.ok(region.craftRecipeSetIds.includes(craftRecipePack.id));
    assert.ok(region.encounterSetIds.includes(encounterPack.id));
    assert.ok(region.editorProfileIds.includes(editorTemplates.id));
  });

  it("defines six large areas with point coverage, duration, and localized labels", () => {
    assert.equal(largeAreaPack.largeAreas.length, 6);
    assert.deepEqual(largeAreaPack.largeAreaIds, largeAreaPack.largeAreas.map((area) => area.id));
    for (const area of largeAreaPack.largeAreas) {
      assert.ok(region.largeAreaIds.includes(area.id), `${area.id} missing from region index`);
      assert.ok(localization[area.displayNameKey], `${area.displayNameKey} missing localization`);
      assert.ok(area.primaryDimensions.length >= 2, `${area.id} needs gameplay dimensions`);
      assert.equal(area.pointIds.length, 8, `${area.id} should expose eight first-pass points`);
      assert.equal(area.durationMinutes.length, 2, `${area.id} needs duration range`);
      assert.ok(area.coverage.npcIds.length >= 2, `${area.id} needs NPC coverage`);
      assert.ok(area.coverage.gatherNodeIds.length >= 1, `${area.id} needs gathering hooks`);
      assert.ok(area.coverage.encounterIds.length >= 1, `${area.id} needs encounter hooks`);
      assert.ok(area.coverage.gatherNodeIds.every((id) => gatherNodeIds.has(id)), `${area.id} has unresolved gather nodes`);
      assert.ok(area.coverage.craftRecipeIds.every((id) => id.startsWith("recipe.") && craftRecipeIds.has(id)), `${area.id} has unresolved craft recipes`);
      assert.ok(area.coverage.encounterIds.every((id) => encounterIds.has(id) || id === boss.id), `${area.id} has unresolved encounters`);
      assert.ok(area.resourceLoop.inputs.length >= 1, `${area.id} needs self-supply inputs`);
      assert.ok(area.resourceLoop.outputs.length >= 1, `${area.id} needs self-supply outputs`);
    }
    const outerDike = largeAreaPack.largeAreas.find((area) => area.id === "large_area.jiangnan.lake_field_outer_dike");
    assert.ok(outerDike.coverage.questIds.includes("quest.jiangnan.outer_dike.sluice_supply"));
    assert.ok(outerDike.coverage.npcIds.includes("npc.outer_dike_ahu"));
    assert.ok(outerDike.coverage.buildingIds.includes("building.jiangnan_rain_alley.outer_dike_sluice"));
    assert.ok(outerDike.coverage.gatherNodeIds.some((id) => gatherNodeIds.has(id)));
    assert.ok(outerDike.coverage.craftRecipeIds.some((id) => craftRecipeIds.has(id)));
    assert.ok(outerDike.coverage.encounterIds.some((id) => encounterIds.has(id)));
  });

  it("defines reusable cross-area routes with valid endpoints and localized labels", () => {
    assert.equal(largeAreaPack.routes.length, 8);
    assert.deepEqual(largeAreaPack.routeIds, largeAreaPack.routes.map((route) => route.id));
    for (const route of largeAreaPack.routes) {
      assert.ok(routeIds.has(route.id), `${route.id} missing from routeIds`);
      assert.ok(localization[route.displayNameKey], `${route.displayNameKey} missing localization`);
      assert.ok(largeAreaIds.has(route.fromLargeAreaId), `${route.id} has invalid fromLargeAreaId`);
      assert.ok(largeAreaIds.has(route.toLargeAreaId), `${route.id} has invalid toLargeAreaId`);
      assert.notEqual(route.fromLargeAreaId, route.toLargeAreaId, `${route.id} should connect two areas`);
      assert.ok(route.travelMode, `${route.id} needs a travelMode`);
      assert.ok(Array.isArray(route.gateIds), `${route.id} gateIds must be an array`);
      assert.ok(route.contentRefs.length >= 2, `${route.id} needs content refs`);
      for (const ref of route.contentRefs) {
        if (ref.startsWith("point.")) assert.ok(pointIds.has(ref), `${route.id} references unknown point ${ref}`);
        assertFormalRef(ref);
      }
    }
    assert.ok(largeAreaPack.routes.some((route) => route.toLargeAreaId === "large_area.jiangnan.lake_field_outer_dike"));
  });

  it("maps every 07 subregion into 13 point definitions without deleting the old baseline", () => {
    const migrated = new Set(largeAreaPack.migrationFrom07.map((item) => item.subregionId));
    assert.deepEqual([...migrated].sort(), [...subregions.subregions.map((item) => item.id)].sort());

    const pointsById = new Map(largeAreaPack.points.map((point) => [point.id, point]));
    for (const migration of largeAreaPack.migrationFrom07) {
      assert.ok(region.subregionIds.includes(migration.subregionId), `${migration.subregionId} not in RegionDef`);
      assert.ok(region.largeAreaIds.includes(migration.largeAreaId), `${migration.largeAreaId} not in RegionDef`);
      assert.ok(migration.pointIds.length >= 1, `${migration.subregionId} needs at least one target point`);
      for (const pointId of migration.pointIds) {
        const point = pointsById.get(pointId);
        assert.ok(point, `${pointId} missing from point table`);
        assert.ok(region.largeAreaIds.includes(point.largeAreaId), `${pointId} references unknown large area`);
        assert.ok(region.pointIds.includes(pointId), `${pointId} missing from RegionDef pointIds`);
      }
      assert.ok(
        migration.pointIds.some((pointId) => pointsById.get(pointId)?.largeAreaId === migration.largeAreaId),
        `${migration.subregionId} needs at least one point in its primary large area`
      );
    }
  });

  it("defines point records with coordinates, gates, content refs, and localization", () => {
    const largeAreaIds = new Set(largeAreaPack.largeAreaIds);
    for (const point of largeAreaPack.points) {
      assert.ok(region.pointIds.includes(point.id), `${point.id} missing from RegionDef`);
      assert.ok(largeAreaIds.has(point.largeAreaId), `${point.id} references unknown large area`);
      assert.ok(localization[point.displayNameKey], `${point.displayNameKey} missing localization`);
      assert.equal(typeof point.coordinates.x, "number");
      assert.equal(typeof point.coordinates.y, "number");
      assert.ok(point.type, `${point.id} missing type`);
      assert.ok(point.contentRefs.length >= 1, `${point.id} needs content refs`);
      for (const ref of point.contentRefs) assertFormalRef(ref);
      assert.ok(Array.isArray(point.gates), `${point.id} gates must be an array`);
      assert.ok(Array.isArray(point.revisitStates), `${point.id} revisitStates must be an array`);
    }
  });

  it("formalizes gather nodes, craft recipes, traits, and encounters for the large-map layer", () => {
    assert.deepEqual(gatherNodePack.gatherNodeIds, gatherNodePack.gatherNodes.map((node) => node.id));
    assert.deepEqual(craftRecipePack.recipeIds, craftRecipePack.recipes.map((recipe) => recipe.id));
    assert.deepEqual(craftRecipePack.traitIds, craftRecipePack.traits.map((trait) => trait.id));
    assert.deepEqual(encounterPack.encounterIds, encounterPack.encounters.map((encounter) => encounter.id));
    assert.equal(gatherNodePack.gatherNodes.length, 19);
    assert.equal(craftRecipePack.recipes.length, 11);
    assert.equal(craftRecipePack.traits.length, 5);
    assert.equal(encounterPack.encounters.length, 12);

    for (const node of gatherNodePack.gatherNodes) {
      assert.ok(largeAreaIds.has(node.largeAreaId), `${node.id} has invalid largeAreaId`);
      assert.ok(pointIds.has(node.pointId), `${node.id} has invalid pointId`);
      assert.ok(node.resourceIds.length >= 1, `${node.id} needs resources`);
      for (const ref of node.riskRefs ?? []) assertFormalRef(ref);
    }

    for (const recipe of craftRecipePack.recipes) {
      assert.ok(largeAreaIds.has(recipe.largeAreaId), `${recipe.id} has invalid largeAreaId`);
      assert.ok(pointIds.has(recipe.pointId), `${recipe.id} has invalid pointId`);
      assert.ok(buildingIds.has(recipe.stationId), `${recipe.id} has invalid stationId`);
      if (recipe.interactionId) assert.ok(interactionIds.has(recipe.interactionId), `${recipe.id} has invalid interactionId`);
      assert.ok(recipe.requiredItems.length >= 1, `${recipe.id} needs requiredItems`);
      assert.ok(recipe.craftFields.length >= 1, `${recipe.id} needs craftFields`);
      assert.ok(recipe.resultIds.length >= 1, `${recipe.id} needs resultIds`);
      for (const ref of recipe.resultIds) assertFormalRef(ref);
    }

    for (const trait of craftRecipePack.traits) {
      assert.ok(craftRecipeIds.has(trait.sourceRecipeId), `${trait.id} has invalid sourceRecipeId`);
      assert.ok(Object.keys(trait.effectFields).length >= 1, `${trait.id} needs effect fields`);
    }

    for (const encounter of encounterPack.encounters) {
      assert.ok(largeAreaIds.has(encounter.largeAreaId), `${encounter.id} has invalid largeAreaId`);
      assert.ok(pointIds.has(encounter.pointId), `${encounter.id} has invalid pointId`);
      assert.ok(encounter.enemyIds.every((id) => enemyIds.has(id)), `${encounter.id} has unresolved enemies`);
      assert.ok(encounter.trapIds.every((id) => trapIds.has(id)), `${encounter.id} has unresolved traps`);
      assert.ok(encounter.learningGoal, `${encounter.id} needs learningGoal`);
      assert.ok(encounter.failureHandling, `${encounter.id} needs failureHandling`);
      assert.ok(encounter.rewards.length >= 1, `${encounter.id} needs rewards`);
      assert.ok(encounter.repeatability, `${encounter.id} needs repeatability`);
    }
  });

  it("captures self-supply loops for craft, market, and bridge revisit", () => {
    assert.deepEqual(region.selfSupplyLoopIds.sort(), largeAreaPack.selfSupplyLoops.map((loop) => loop.id).sort());
    for (const loop of largeAreaPack.selfSupplyLoops) {
      assert.ok(localization[loop.displayNameKey], `${loop.displayNameKey} missing localization`);
      assert.ok(loop.largeAreaIds.every((id) => region.largeAreaIds.includes(id)));
      assert.ok(loop.steps.length >= 4, `${loop.id} should describe a loop, not a single action`);
      assert.ok(loop.outputs.length >= 2, `${loop.id} needs outputs`);
    }
  });

  it("describes eight standard editor templates with content output targets", () => {
    assert.equal(editorTemplates.templates.length, 8);
    assert.equal(editorTemplates.draftContract.storage, "in_memory_only");
    assert.equal(editorTemplates.draftContract.writePolicy, "export_text_then_review");
    assert.ok(editorTemplates.draftContract.validationLevels.includes("error"));
    assert.ok(editorTemplates.draftContract.fieldControlSources.includes("gatherNodes"));
    assert.ok(editorTemplates.draftContract.fieldControlSources.includes("craftRecipes"));
    assert.ok(editorTemplates.draftContract.fieldControlSources.includes("encounters"));
    assert.deepEqual(editorTemplates.templateIds, editorTemplates.templates.map((template) => template.id));
    for (const template of editorTemplates.templates) {
      assert.ok(localization[template.displayNameKey], `${template.displayNameKey} missing localization`);
      assert.ok(template.targetObjects.length >= 1, `${template.id} needs target objects`);
      assert.ok(template.outputTargets.every((target) => target.startsWith("content/")), `${template.id} must output into content/`);
      assert.ok(template.requiredFields.includes("id"), `${template.id} needs id field`);
      assert.ok(template.draftFields.includes("id"), `${template.id} needs editable id field`);
      assert.ok(template.draftFields.includes("displayNameKey"), `${template.id} needs editable displayNameKey field`);
      assert.ok(template.draftSeed?.id, `${template.id} needs a seed draft id`);
      assert.ok(template.draftSeed?.displayNameKey?.endsWith(".name"), `${template.id} seed needs localization key shape`);
      assert.ok(template.validationRules.length >= 2, `${template.id} needs validation rules`);
      assert.ok(template.demoPreview.surface, `${template.id} needs demo preview surface`);
    }
    const gatherTemplate = editorTemplates.templates.find((template) => template.id === "editor_template.jiangnan.gather_node");
    const craftTemplate = editorTemplates.templates.find((template) => template.id === "editor_template.jiangnan.craft_recipe");
    const encounterTemplate = editorTemplates.templates.find((template) => template.id === "editor_template.jiangnan.encounter");
    assert.ok(gatherTemplate.outputTargets.includes("content/gather_nodes/jiangnan_gather_nodes.json"));
    assert.ok(craftTemplate.outputTargets.includes("content/recipes/jiangnan_craft_recipes.json"));
    assert.ok(encounterTemplate.outputTargets.includes("content/encounters/jiangnan_encounters.json"));
    assert.ok(craftTemplate.requiredFields.includes("resultIds"));
  });
});
