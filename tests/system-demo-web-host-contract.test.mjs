import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const read = async (path) => readFile(new URL(`../${path}`, import.meta.url), "utf8");

test("Demo 0.8.1 Web Host stays isolated from the historical F1 player path", async () => {
  const [rootCmake, appCmake, layerHeader, layer, shell, packageJson] = await Promise.all([
    read("CMakeLists.txt"),
    read("apps/system-demo-web/CMakeLists.txt"),
    read("apps/system-demo-web/Source/SystemDemoLayer.hpp"),
    read("apps/system-demo-web/Source/SystemDemoLayer.cpp"),
    read("apps/system-demo-web/shell.html"),
    read("package.json")
  ]);
  const hostSource = `${layerHeader}\n${layer}`;

  assert.match(rootCmake, /TGD_BUILD_SYSTEM_DEMO_HOST/);
  assert.match(rootCmake, /add_subdirectory\(apps\/system-demo-web\)/);
  assert.match(appCmake, /OUTPUT_NAME "tiangongdu-system-demo"/);
  assert.match(appCmake, /tgd_prepare_system_demo_package/);
  assert.match(appCmake, /system-demo\.tgdsbx@\/system-demo\.tgdsbx/);
  assert.match(appCmake, /LINK_DEPENDS[\s\S]*shell\.html/);
  assert.match(hostSource, /SandboxRuntimeCoordinator/);
  assert.match(hostSource, /SandboxAssetResolver/);
  assert.match(layer, /system_sandbox_asset_registry/);
  assert.match(layer, /advance_player/);
  assert.match(layer, /submit_operate/);
  assert.match(layer, /retry_standalone/);
  assert.match(shell, /Internal Blockout/i);
  assert.match(shell, /__tgdSystemDemo/);
  assert.match(shell, /hostRuntimeReady/);
  assert.match(shell, /workbenchPreview/);
  assert.match(shell, /\/api\/preview-package/);
  assert.match(shell, /tgd-system-demo-preview-ready/);
  assert.match(layer, /workbench-preview\.tgdsbx/);
  assert.doesNotMatch(shell, /\blet runtimeInitialized\b/);
  assert.match(packageJson, /test:system-demo-web/);
  assert.match(packageJson, /test:system-demo-workbench/);

  const forbidden = [
    "F1GrayboxLayer",
    "F1RewardClaim",
    "F1QuestUiProjection",
    "ProfileStorage",
    "reward_claim"
  ];
  for (const token of forbidden) {
    assert.equal(
      appCmake.includes(token) || hostSource.includes(token) || shell.includes(token),
      false,
      `isolated system Demo Host unexpectedly references ${token}`
    );
  }
});

test("Demo 0.8.1 browser route exposes package, blocker, operate, and retry evidence", async () => {
  const browserRoute = await read("tests/browser/system-demo-web-host.mjs");
  for (const token of [
    "packageByteCount",
    "assetCount",
    "blockedMoveCount",
    "gateOpen",
    "retryCount",
    "consoleErrors",
    "pageErrors",
    "requestErrors"
  ]) {
    assert.match(browserRoute, new RegExp(token));
  }
});

test("Demo 0.8.2 Workbench route proves CRUD, atomic Preview swap, and last-valid retention", async () => {
  const [browserRoute, server, controller, workbench] = await Promise.all([
    read("tests/browser/system-demo-workbench.mjs"),
    read("apps/content-workbench/src/workbench-server.mjs"),
    read("apps/content-workbench/src/workbench-controller.mjs"),
    read("apps/content-workbench/public/workbench.mjs")
  ]);
  for (const token of [
    "/api/object-create",
    "/api/object-delete",
    "/api/preview-publish",
    "/api/preview-package",
    "preview-candidate",
    "preview-live",
    "oldFrameStayedVisibleUntilReady",
    "keptLiveFrame"
  ]) {
    assert.match(
      `${browserRoute}\n${server}\n${controller}\n${workbench}`,
      new RegExp(token.replaceAll("/", "\\/"))
    );
  }
});
