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

  it("renders the demo identity and core UI labels", () => {
    for (const label of ["天工渡", "江南雨巷", "纸伞门径", "生机", "气力", "风息", "归位进度", "工匠四目", "行脚记录", "重置试炼", "背包", "人物"]) {
      assert.match(html, new RegExp(label));
    }
  });

  it("wires expected controls to interactions", () => {
    for (const key of ["gatherWind", "borrowWind", "interact", "resetDemo", "getContextTarget", "cooldowns", "keydown", "requestAnimationFrame", "INVENTORY_DEFS", "MAP_NODES", "NPC_DEFS", "advanceTime"]) {
      assert.match(js, new RegExp(key));
    }
  });

  it("keeps the playfield full viewport and responsive", () => {
    assert.match(css, /width:\s*100vw/);
    assert.match(css, /height:\s*100vh/);
    assert.match(css, /@media\s*\(max-width:\s*900px\)/);
  });
});
