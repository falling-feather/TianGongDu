import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const contentRoot = join(root, "content");

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function findJsonFiles(dir) {
  return readdirSync(dir)
    .flatMap((entry) => {
      const fullPath = join(dir, entry);
      if (statSync(fullPath).isDirectory()) {
        return findJsonFiles(fullPath);
      }
      return fullPath.endsWith(".json") ? [fullPath] : [];
    })
    .sort();
}

function walk(value, visit) {
  if (Array.isArray(value)) {
    for (const item of value) walk(item, visit);
    return;
  }
  if (!value || typeof value !== "object") return;
  visit(value);
  for (const child of Object.values(value)) walk(child, visit);
}

const localization = readJson(join(contentRoot, "localization", "zh-CN.json")).strings;
const region = readJson(join(contentRoot, "regions", "jiangnan_rain_alley.json"));
const subregions = readJson(join(contentRoot, "subregions", "jiangnan_rain_alley_subregions.json"));
const npcs = readJson(join(contentRoot, "npcs", "rain_alley_m1_npcs.json"));
const quests = readJson(join(contentRoot, "quests", "jiangnan_rain_alley_quests.json"));
const buildings = readJson(join(contentRoot, "buildings", "jiangnan_rain_alley_buildings.json"));
const interactions = readJson(join(contentRoot, "interactions", "jiangnan_rain_alley_interactions.json"));
const enemies = readJson(join(contentRoot, "enemies", "rain_alley_m1_enemies.json"));
const boss = readJson(join(contentRoot, "bosses", "rain_alley_boss.json"));
const assets = readJson(join(contentRoot, "assets", "jiangnan_rain_alley_assets.json"));
const audio = readJson(join(contentRoot, "audio", "jiangnan_rain_alley_audio.json"));

describe("jiangnan rain alley M1 content pack", () => {
  it("indexes the M1 package from RegionDef", () => {
    assert.equal(region.id, "region.jiangnan_rain_alley");
    assert.deepEqual(region.recommendedDurationMinutes, [45, 60]);
    assert.equal(region.entrySubregionId, "subregion.jiangnan_rain_alley.water_entry");
    assert.equal(region.subregionIds.length, 11);
    assert.equal(region.coreSubregionIds.length, 7);
    assert.ok(region.questIds.includes("quest.jiangnan_rain_alley.mainline"));
    assert.ok(region.npcIds.includes("npc.master_shen_yu"));
    assert.ok(region.bossIds.includes("boss.rain_alley_broken_umbrella_obsession"));
    assert.ok(region.assetPackIds.includes("asset_pack.jiangnan_rain_alley_m1"));
    assert.ok(region.audioPackIds.includes("audio_pack.jiangnan_rain_alley_m1"));
  });

  it("defines 11 subregions with 7 M1 core nodes", () => {
    assert.equal(subregions.subregions.length, 11);
    assert.equal(subregions.coreSubregionIds.length, 7);
    for (const id of subregions.coreSubregionIds) {
      assert.ok(subregions.subregions.some((item) => item.id === id), `${id} missing from subregions`);
      assert.ok(region.coreSubregionIds.includes(id), `${id} missing from region core index`);
    }
    for (const item of subregions.subregions) {
      assert.equal(item.regionId, region.id);
      assert.match(item.id, /^subregion\.jiangnan_rain_alley\.[a-z0-9_]+$/);
      assert.ok(item.type);
      assert.ok(item.layer);
      assert.ok(item.purpose);
      assert.ok(Array.isArray(item.connections));
      assert.ok(localization[item.displayNameKey], `${item.displayNameKey} missing localization`);
    }
  });

  it("defines the mainline, side quests, and return interactions", () => {
    assert.equal(quests.mainlineQuest.id, "quest.jiangnan_rain_alley.mainline");
    assert.equal(quests.mainlineQuest.stepIds.length, 10);
    assert.equal(quests.steps.length, 10);
    assert.equal(quests.sideQuests.length, 8);
    assert.ok(quests.mainlineQuest.choiceIds.includes("choice.material_compromise"));
    assert.ok(quests.sideQuests.some((quest) => quest.id === "quest.jiangnan_rain_alley.side.bamboo_after_wind"));
    assert.ok(quests.repeatableInteractions.length >= 6);
  });

  it("defines NPCs with functional roles, emotional roles, schedules, and four-eye lean", () => {
    assert.equal(npcs.npcs.length, 15);
    const requiredNpcIds = [
      "npc.master_shen_yu",
      "npc.market_runner",
      "npc.alley_resident",
      "npc.zhou_bamboo",
      "npc.apprentice_aqing",
      "npc.ye_hao",
      "npc.mei_bridge_keeper",
      "npc.du_repairer"
    ];
    for (const id of requiredNpcIds) {
      assert.ok(npcs.npcs.some((npc) => npc.id === id), `${id} missing`);
      assert.ok(region.npcIds.includes(id), `${id} missing from region index`);
    }
    for (const npc of npcs.npcs) {
      assert.ok(npc.roles.length >= 2, `${npc.id} needs at least two roles`);
      assert.ok(npc.emotionalRole, `${npc.id} missing emotional role`);
      assert.ok(npc.routine.day, `${npc.id} missing day routine`);
      assert.ok(npc.routine.evening, `${npc.id} missing evening routine`);
      assert.ok(npc.routine.night, `${npc.id} missing night routine`);
      assert.ok(npc.fourEyeLean.length >= 1, `${npc.id} missing four-eye lean`);
    }
  });

  it("keeps buildings and interactions separated by responsibility", () => {
    assert.equal(buildings.buildings.length, 10);
    assert.ok(buildings.buildings.every((building) => building.regionId === region.id));
    const interactionTypes = new Set(interactions.interactions.map((interaction) => interaction.type));
    for (const type of interactions.interactionTypes) {
      assert.ok(interactionTypes.has(type), `${type} interaction missing`);
    }
    const marketChoice = interactions.interactions.find((item) => item.id === "interaction.jiangnan_rain_alley.market_choice");
    assert.ok(marketChoice);
    assert.equal(marketChoice.effects.feedbackTargets.length, 3);
    assert.deepEqual(Object.keys(marketChoice.effects.fourEyeDeltasByChoice).sort(), [
      "choice.full_process",
      "choice.material_compromise",
      "choice.rush_order"
    ]);
  });

  it("defines M1 enemies, traps, boss phases, and restoration outcomes", () => {
    assert.equal(enemies.enemies.length, 9);
    assert.equal(enemies.traps.length, 6);
    for (const enemy of enemies.enemies) {
      assert.ok(enemy.weakTags.length >= 1, `${enemy.id} missing weak tags`);
      assert.ok(enemy.subregionIds.length >= 1, `${enemy.id} missing subregion use`);
    }
    assert.equal(boss.phaseIds.length, 4);
    assert.equal(boss.phases.length, 4);
    assert.ok(boss.phaseIds.includes("bossphase.rain_alley_broken_umbrella_obsession.restoration"));
    assert.equal(boss.restorationOutcomes.length, 3);
    assert.ok(boss.restorationChoiceIds.includes("choice.material_compromise"));
  });

  it("defines art and audio production interfaces", () => {
    assert.equal(assets.assetGroups.length, 9);
    assert.ok(assets.assetGroups.some((group) => group.id === "asset_group.jiangnan.subregion_concepts" && group.requiredCount === 11));
    assert.ok(assets.assetGroups.some((group) => group.id === "asset_group.jiangnan.npc_portraits" && group.requiredCount === 15));
    assert.equal(audio.tracks.length, 4);
    assert.equal(audio.ambient.length, 3);
    assert.equal(audio.sfx.length, 9);
    assert.ok(audio.mixRules.length >= 2);
  });

  it("has localization strings for every content displayNameKey", () => {
    const missing = [];
    for (const file of findJsonFiles(contentRoot)) {
      const data = readJson(file);
      walk(data, (item) => {
        if (typeof item.displayNameKey === "string" && !localization[item.displayNameKey]) {
          missing.push(`${item.displayNameKey} in ${file}`);
        }
      });
    }
    assert.deepEqual(missing, []);
  });
});
