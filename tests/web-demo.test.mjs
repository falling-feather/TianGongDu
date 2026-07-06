import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));

describe("web demo static surface", () => {
  const html = readFileSync(join(root, "prototype/web-demo/index.html"), "utf8");
  const js = readFileSync(join(root, "prototype/web-demo/src/main-v2.js"), "utf8");
  const css = readFileSync(join(root, "prototype/web-demo/src/styles.css"), "utf8");
  const devServer = readFileSync(join(root, "tools/dev-server.mjs"), "utf8");
  const assetPack = readFileSync(join(root, "content/assets/jiangnan_rain_alley_assets.json"), "utf8");

  it("renders the demo identity and core UI labels", () => {
    for (const label of ["天工渡", "江南雨巷", "纸伞门径", "生机", "气力", "风息", "归位进度", "工匠四目", "行脚记录", "重置试炼", "背包", "人物", "大地图", "编辑器", "江南M1", "推进 M1 流程"]) {
      assert.match(html, new RegExp(label));
    }
  });

  it("wires expected controls to interactions", () => {
    for (const key of ["gatherWind", "borrowWind", "interact", "resetDemo", "getContextTarget", "cooldowns", "keydown", "requestAnimationFrame", "INVENTORY_DEFS", "MAP_NODES", "NPC_DEFS", "advanceTime", "loadContentPack", "advanceM1Flow", "startBossEncounter", "damageBoss", "restoreBoss"]) {
      assert.match(js, new RegExp(key));
    }
  });

  it("keeps UI and canvas performance safeguards in the runtime loop", () => {
    for (const key of ["MAX_RENDER_DPR", "UI_SYNC_MAX_HZ", "canvasRenderDpr", "markUiDirty", "shouldSyncUi", "buildUiSignature", "renderVisiblePanel"]) {
      assert.match(js, new RegExp(key));
    }
    assert.match(js, /world\.ui/);
    assert.match(js, /activePanelRendered/);
    assert.match(js, /lastStateSignature/);
    assert.match(js, /update\(dt,\s*now\)/);
    assert.match(js, /renderVisiblePanel\(\)/);
    assert.match(js, /performance:\s*\{/);
  });

  it("surfaces the 13 large-map and editor content tools", () => {
    for (const key of ["largeMapPanel", "largeMapBoard", "largeMapDetails", "editorPanel", "editorBoard", "exportEditorButton", "resetEditorButton"]) {
      assert.match(html, new RegExp(key));
    }
    for (const key of ["largeAreas", "largeAreaRoutes", "gatherNodes", "craftRecipes", "craftTraits", "encounters", "editors", "renderLargeMapPanel", "createLargeMapRouteLayer", "createLargeAreaNode", "createRouteRows", "focusLargeArea", "renderLargeMapDetails", "resolveContentRef", "createContentRefChip", "createCoverageRows", "renderEditorPanel", "selectEditorTemplate", "buildEditableContentModel", "exportEditorDraft", "resetEditorDraft", "updateEditorDraftFromControl", "validateEditorDraft", "createEditorFieldControl", "createEditorValidationList"]) {
      assert.match(js, new RegExp(key));
    }
    for (const key of ["draftsByTemplateId", "validationByTemplateId", "exportedTextByTemplateId", "data-editor-field", "data-editor-preview", "__heavenwrightsDemo", "getDemoQaSnapshot"]) {
      assert.match(js, new RegExp(key));
    }
    assert.match(js, /\/content\/large_areas\/jiangnan_large_areas\.json/);
    assert.match(js, /\/content\/gather_nodes\/jiangnan_gather_nodes\.json/);
    assert.match(js, /\/content\/recipes\/jiangnan_craft_recipes\.json/);
    assert.match(js, /\/content\/encounters\/jiangnan_encounters\.json/);
    assert.match(js, /\/content\/editors\/jiangnan_editor_templates\.json/);
    assert.match(css, /\.large-map-atlas/);
    assert.match(css, /\.large-area-node/);
    assert.match(css, /\.large-map-route/);
    assert.match(css, /\.route-row/);
    assert.match(css, /\.content-ref-chip/);
    assert.match(css, /\.coverage-row/);
    assert.match(css, /\.editor-template-list/);
    assert.match(css, /\.editor-form/);
    assert.match(css, /\.editor-validation-error/);
  });

  it("surfaces the M1 production interface in the demo", () => {
    assert.match(html, /productionBoard/);
    for (const key of ["renderProductionBoard", "emitProductionCue", "getBossProductionState", "getAudioProductionCount", "PRODUCTION_WHITEBOX_REFS", "PRODUCTION_AUDIO_CUE_REFS"]) {
      assert.match(js, new RegExp(key));
    }
    assert.match(js, /asset_group\.jiangnan\.boss_sprites/);
    assert.match(js, /sfx\.boss\.core_restore/);
    assert.match(assetPack, /building\.jiangnan_rain_alley\.outer_dike_sluice/);
    assert.match(assetPack, /npc\.outer_dike_ahu/);
  });

  it("wires the dye court craft whitebox to gameplay and assets", () => {
    assert.match(html, /craftBoard/);
    for (const key of ["CRAFT_RECIPES", "CRAFT_INTERACTION_HOTSPOT", "getCraftInteractionTarget", "openCraftInteraction", "applyCraftRecipe", "renderCraftBoard", "drawDyePaperCourt", "wind_hungry_surface", "stable_blue_dye", "quick_order_finish"]) {
      assert.match(js, new RegExp(key));
    }
    assert.match(js, /trait\.umbrella\.wind_hungry_surface/);
    assert.match(js, /interaction\.jiangnan_rain_alley\.craft_paper_oil/);
    assert.match(js, /asset\.variant\.umbrella\.blue_lantern/);
    assert.match(assetPack, /tile\.dye_paper_court/);
    assert.match(css, /\.craft-board/);
    assert.match(css, /\.craft-board\.is-highlighted/);
    assert.match(css, /\.craft-options button\.active/);
  });

  it("serves content JSON to the web demo", () => {
    assert.match(devServer, /workspaceRoot/);
    assert.match(devServer, /contentRoot/);
    assert.match(devServer, /pathname\.startsWith\("\/content\/"\)/);
  });

  it("keeps the playfield full viewport and responsive", () => {
    assert.match(css, /width:\s*100vw/);
    assert.match(css, /height:\s*100vh/);
    assert.match(css, /@media\s*\(max-width:\s*900px\)/);
    assert.match(css, /\.boss-readout/);
    assert.match(css, /\.production-board/);
    assert.match(css, /\.production-row\.whitebox/);
  });
});
