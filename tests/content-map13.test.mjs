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
const largeAreaPack = readJson("large_areas/jiangnan_large_areas.json");
const editorTemplates = readJson("editors/jiangnan_editor_templates.json");

describe("jiangnan chapter large-map content", () => {
  it("keeps 07 M1 subregions while indexing the 13 large-map layer", () => {
    assert.equal(region.id, "region.jiangnan_rain_alley");
    assert.deepEqual(region.recommendedDurationMinutes, [45, 60]);
    assert.deepEqual(region.chapterTargetDurationMinutes, [300, 600]);
    assert.equal(region.subregionIds.length, 11);
    assert.equal(region.largeAreaIds.length, 6);
    assert.equal(region.pointIds.length, 36);
    assert.equal(region.corePointIds.length, 7);
    assert.ok(region.largeAreaSetIds.includes(largeAreaPack.id));
    assert.ok(region.editorProfileIds.includes(editorTemplates.id));
  });

  it("defines six large areas with point coverage, duration, and localized labels", () => {
    assert.equal(largeAreaPack.largeAreas.length, 6);
    assert.deepEqual(largeAreaPack.largeAreaIds, largeAreaPack.largeAreas.map((area) => area.id));
    for (const area of largeAreaPack.largeAreas) {
      assert.ok(region.largeAreaIds.includes(area.id), `${area.id} missing from region index`);
      assert.ok(localization[area.displayNameKey], `${area.displayNameKey} missing localization`);
      assert.ok(area.primaryDimensions.length >= 2, `${area.id} needs gameplay dimensions`);
      assert.equal(area.pointIds.length, 6, `${area.id} should expose six first-pass points`);
      assert.equal(area.durationMinutes.length, 2, `${area.id} needs duration range`);
      assert.ok(area.coverage.npcIds.length >= 2, `${area.id} needs NPC coverage`);
      assert.ok(area.coverage.gatherNodeIds.length >= 1, `${area.id} needs gathering hooks`);
      assert.ok(area.coverage.encounterIds.length >= 1, `${area.id} needs encounter hooks`);
      assert.ok(area.resourceLoop.inputs.length >= 1, `${area.id} needs self-supply inputs`);
      assert.ok(area.resourceLoop.outputs.length >= 1, `${area.id} needs self-supply outputs`);
    }
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
      assert.ok(Array.isArray(point.gates), `${point.id} gates must be an array`);
      assert.ok(Array.isArray(point.revisitStates), `${point.id} revisitStates must be an array`);
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

  it("describes seven standard editor templates with content output targets", () => {
    assert.equal(editorTemplates.templates.length, 7);
    assert.deepEqual(editorTemplates.templateIds, editorTemplates.templates.map((template) => template.id));
    for (const template of editorTemplates.templates) {
      assert.ok(localization[template.displayNameKey], `${template.displayNameKey} missing localization`);
      assert.ok(template.targetObjects.length >= 1, `${template.id} needs target objects`);
      assert.ok(template.outputTargets.every((target) => target.startsWith("content/")), `${template.id} must output into content/`);
      assert.ok(template.requiredFields.includes("id"), `${template.id} needs id field`);
      assert.ok(template.validationRules.length >= 2, `${template.id} needs validation rules`);
      assert.ok(template.demoPreview.surface, `${template.id} needs demo preview surface`);
    }
  });
});
