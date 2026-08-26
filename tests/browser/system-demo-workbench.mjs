import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import {
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  stat,
  writeFile
} from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { chromium } from "playwright";

import { startWorkbenchServer } from "../../apps/content-workbench/src/workbench-server.mjs";

const repositoryRoot = fileURLToPath(new URL("../../", import.meta.url));
const buildDirectoryArgument =
  process.env.TGD_SYSTEM_DEMO_WEB_BUILD_DIRECTORY ??
  "build/web-release-single";
assert.equal(
  path.isAbsolute(buildDirectoryArgument),
  false,
  "Web build directory must be repository-relative"
);
const buildDirectory = path.resolve(repositoryRoot, buildDirectoryArgument);
const evidenceDirectory = path.resolve(
  repositoryRoot,
  process.env.TGD_DEMO_085_EVIDENCE_DIRECTORY ??
    process.env.TGD_DEMO_082_EVIDENCE_DIRECTORY ??
    "build/evidence/demo-085-workbench"
);
const sourceDocument = path.resolve(
  repositoryRoot,
  "content/design/system-demo.sandbox.json"
);
const actorId = "actor.system_demo.entry.demo082";
const sourceActorId = "actor.system_demo.entry.slot_a";
const interactionId = "interaction.system_demo.console.demo082";
const invalidInteractionId =
  "interaction.system_demo.console.demo082.invalid";
const sourceInteractionId = "interaction.system_demo.console";
const craftPanels = [
  {
    kind: "craftMaterials",
    id: "material.jiangnan.umbrella.flexible_bark_paper_patch",
    fields: ["editorLabel"]
  },
  {
    kind: "craftWorkstations",
    id: "workstation.jiangnan.umbrella_tuning",
    fields: ["editorLabel", "regionId", "assetId", "x", "y"]
  },
  {
    kind: "craftProcesses",
    id: "craft_process.jiangnan.umbrella_canopy_tuning",
    fields: [
      "editorLabel",
      "workstationId",
      "needId",
      "outputItemId",
      "trialStepId",
      "materialOutcome_0",
      "materialOutcome_1"
    ]
  },
  {
    kind: "craftSteps",
    id: "craft_step.jiangnan.umbrella.rain_trial",
    fields: [
      "editorLabel",
      "processId",
      "predecessorStepId",
      "actionId",
      "kind"
    ]
  }
];
const workshopPanels = [
  {
    kind: "workshops",
    id: "workshop.jiangnan.umbrella_lane",
    fields: [
      "editorLabel",
      "workstationId",
      "initialFunds",
      "stockMaterialId_0",
      "stockUnitCost_0",
      "stockInitialQuantity_0",
      "stockBaseQuality_0",
      "stockReworkQualityGain_0",
      "stockMaterialId_1",
      "stockUnitCost_1",
      "stockInitialQuantity_1",
      "stockBaseQuality_1",
      "stockReworkQualityGain_1"
    ]
  },
  {
    kind: "workshopOrders",
    id: "order.jiangnan.umbrella_roof_shortcut",
    fields: [
      "editorLabel",
      "workshopId",
      "processId",
      "requiredQuantity",
      "minimumQuality",
      "rewardFunds",
      "consequenceKind",
      "consequenceTargetId"
    ]
  }
];

const requiredArtifacts = [
  "dist/web/tiangongdu-system-demo.html",
  "dist/web/tiangongdu-system-demo.js",
  "dist/web/tiangongdu-system-demo.wasm",
  "dist/web/tgd-sandbox-package-service-abi.mjs",
  "dist/web/tgd-sandbox-package-service-abi.wasm"
];

for (const relativePath of requiredArtifacts) {
  const information = await stat(path.resolve(buildDirectory, relativePath));
  assert.equal(information.isFile(), true, `missing ${relativePath}`);
  assert.ok(information.size > 0, `empty ${relativePath}`);
}

const workspaceRoot = await mkdtemp(
  path.join(tmpdir(), "tgd-demo-085-workbench-")
);
const workspaceDocument = path.join(
  workspaceRoot,
  "system-demo.sandbox.json"
);
await copyFile(sourceDocument, workspaceDocument);
await mkdir(evidenceDirectory, { recursive: true });

const running = await startWorkbenchServer({
  workspaceRoot,
  sandboxBuildDirectory: buildDirectoryArgument
});
let browser;
let context;
const consoleErrors = [];
const pageErrors = [];
const requestErrors = [];

const waitForRevision = (page, revision) =>
  page.waitForFunction(
    (expected) =>
      Number(document.querySelector("#revision-value")?.textContent) >=
      expected,
    revision
  );

const selectTreeObject = async (page, kind, id) => {
  await page
    .locator(
      `.tree-object[data-object-kind="${kind}"][data-object-id="${id}"] .tree-row`
    )
    .click();
  await page.waitForFunction(
    ({ expectedKind, expectedId }) =>
      document
        .querySelector("#selection-summary")
        ?.textContent.startsWith(`${expectedKind} / ${expectedId}`),
    { expectedKind: kind, expectedId: id }
  );
};

const duplicateSelected = async (page, { id, label, revision }) => {
  await page.locator("#duplicate-object-button").click();
  await page.waitForFunction(
    () => document.querySelector("#object-dialog")?.open === true
  );
  await page.locator("#object-id-input").fill(id);
  await page.locator("#object-label-input").fill(label);
  await page.locator("#confirm-object-button").click();
  await waitForRevision(page, revision);
  await page.waitForFunction(
    (stableId) =>
      document.querySelector(
        `.tree-object[data-object-id="${CSS.escape(stableId)}"]`
      ) !== null,
    id
  );
};

const checkContent = async (page, expectedStatus) => {
  await page.locator("#content-check-button").click();
  await page.waitForFunction(
    () => {
      const status = document.querySelector(
        "#content-check-summary"
      )?.dataset.status;
      return (
      document.querySelector("#content-check-button")?.getAttribute(
        "aria-busy"
      ) === "false" &&
        status !== "compiling" &&
        status !== "publishing"
      );
    }
  );
  assert.equal(
    await page.locator("#content-check-summary").getAttribute("data-status"),
    expectedStatus,
    JSON.stringify(running.controller.view().contentCheck)
  );
};

try {
  assert.equal(running.controller.view().preview.available, true);
  browser = await chromium.launch({ channel: "msedge", headless: true });
  context = await browser.newContext({
    viewport: { width: 1600, height: 1100 },
    deviceScaleFactor: 1
  });
  const page = await context.newPage();
  page.on("console", (message) => {
    if (message.type() === "error") {
      consoleErrors.push(message.text());
    }
  });
  page.on("pageerror", (error) => pageErrors.push(String(error)));
  page.on("requestfailed", (request) => {
    requestErrors.push(
      `${request.method()} ${request.url()}: ${
        request.failure()?.errorText ?? "failed"
      }`
    );
  });
  page.on("response", (response) => {
    if (response.status() >= 400) {
      requestErrors.push(`${response.status()} ${response.url()}`);
    }
  });

  await page.goto(running.url, {
    waitUntil: "domcontentloaded",
    timeout: 60_000
  });
  await page.locator("#path-input").fill("system-demo.sandbox.json");
  await page.locator("#path-input").press("Enter");
  await page.waitForFunction(
    () =>
      document.querySelector("#opened-path")?.textContent ===
      "system-demo.sandbox.json"
  );

  for (const panel of [...craftPanels, ...workshopPanels]) {
    await selectTreeObject(page, panel.kind, panel.id);
    for (const field of panel.fields) {
      assert.equal(
        await page.locator(`#inspector-form [name="${field}"]`).count(),
        1,
        `${panel.kind} did not render ${field}; pageErrors=${JSON.stringify(pageErrors)}`
      );
    }
    assert.equal(
      await page.locator("#duplicate-object-button").getAttribute(
        "aria-disabled"
      ),
      "true"
    );
    assert.equal(
      await page.locator("#delete-object-button").getAttribute(
        "aria-disabled"
      ),
      "true"
    );
  }
  await page.screenshot({
    path: path.join(
      evidenceDirectory,
      "00-craft-workshop-update-only-panels.png"
    ),
    fullPage: true
  });

  await selectTreeObject(page, "actors", sourceActorId);
  await duplicateSelected(page, {
    id: actorId,
    label: "Demo 0.8.2 调试敌人",
    revision: 1
  });
  const actorBeforeCanvasMove = running.controller
    .view()
    .document.runtime.actors.find(({ id }) => id === actorId);
  await page.locator("#scene-canvas").focus();
  await page.keyboard.press("ArrowRight");
  await waitForRevision(page, 2);
  const actorAfterCanvasMove = running.controller
    .view()
    .document.runtime.actors.find(({ id }) => id === actorId);
  assert.equal(
    actorAfterCanvasMove.pose.x,
    actorBeforeCanvasMove.pose.x + 100
  );

  await selectTreeObject(page, "interactions", sourceInteractionId);
  await duplicateSelected(page, {
    id: interactionId,
    label: "Demo 0.8.2 新交互点",
    revision: 3
  });
  await checkContent(page, "validation_failed");
  assert.ok(
    running.controller.view().contentCheck.diagnostics.length > 0,
    "duplicated interaction should expose its shared-target diagnostic"
  );
  assert.equal(
    await page.locator("#diagnostic-list .diagnostic-locator").count(),
    1
  );
  await page.locator("#diagnostic-list .diagnostic-locator").click();
  assert.match(
    await page.locator("#selection-summary").textContent(),
    new RegExp(`^interactions / ${interactionId}`)
  );
  await page.screenshot({
    path: path.join(evidenceDirectory, "01-crud-diagnostic.png"),
    fullPage: true
  });

  await selectTreeObject(page, "interactions", sourceInteractionId);
  await page.locator("#delete-object-button").click();
  await page.waitForFunction(
    () => document.querySelector("#delete-dialog")?.open === true
  );
  await page.locator("#confirm-delete-button").click();
  await waitForRevision(page, 4);
  assert.equal(
    running.controller
      .view()
      .document.runtime.interactions.some(({ id }) => id === interactionId),
    true
  );
  assert.equal(
    running.controller
      .view()
      .document.runtime.interactions.some(
        ({ id }) => id === sourceInteractionId
      ),
    false
  );

  await page.locator("#save-button").click();
  await page.waitForFunction(
    () => document.querySelector("#dirty-value")?.textContent === "否"
  );
  await checkContent(page, "ready");
  assert.equal(
    await page.locator("#preview-launch-button").getAttribute("aria-disabled"),
    "false"
  );
  const savedRevision = running.controller.view().savedRevision;
  assert.equal(savedRevision, 4);

  const savedDocument = JSON.parse(await readFile(workspaceDocument, "utf8"));
  assert.ok(savedDocument.runtime.actors.some(({ id }) => id === actorId));
  assert.ok(
    savedDocument.runtime.waveSpawns.some(
      ({ actorId: spawnedActorId }) => spawnedActorId === actorId
    )
  );
  assert.ok(
    savedDocument.runtime.interactions.some(({ id }) => id === interactionId)
  );
  assert.equal(
    savedDocument.runtime.interactions.some(
      ({ id }) => id === sourceInteractionId
    ),
    false
  );

  await page.locator("#preview-launch-button").click();
  await page.waitForFunction(
    () =>
      document.querySelectorAll(".preview-frame.preview-live").length === 1 &&
      document.querySelectorAll(".preview-frame.preview-candidate").length ===
        0,
    undefined,
    { timeout: 60_000 }
  );
  const launchPublication = structuredClone(
    running.controller.view().preview.publication
  );
  const launchFrameUrl = await page
    .locator(".preview-frame.preview-live")
    .getAttribute("src");
  const launchFrame = page
    .frames()
    .find((frame) => frame.url().includes(launchPublication.token));
  assert.ok(launchFrame, "launched Preview frame was not attached");
  const launchedRuntime = await launchFrame.evaluate(() =>
    window.__tgdSystemDemo.getState()
  );
  assert.equal(launchedRuntime.ready, true);
  assert.equal(launchedRuntime.assetCount, 12);
  assert.equal(launchedRuntime.packageByteCount, launchPublication.packageBytes);
  assert.equal(launchedRuntime.craftStage, 1);
  assert.equal(launchedRuntime.craftCompleted, false);
  assert.equal(launchedRuntime.workshopFunds, 100);
  assert.equal(launchedRuntime.workshopStockFlexible, 1);
  assert.equal(launchedRuntime.workshopStockSalvage, 2);
  assert.equal(launchedRuntime.orderFulfilled, false);
  assert.equal(launchedRuntime.shortcutOpen, false);
  await page.screenshot({
    path: path.join(evidenceDirectory, "02-launch-live.png"),
    fullPage: true
  });

  await selectTreeObject(page, "actors", actorId);
  const actorBeforeReloadMove = running.controller
    .view()
    .document.runtime.actors.find(({ id }) => id === actorId);
  await page.locator("#scene-canvas").focus();
  await page.keyboard.press("ArrowRight");
  await waitForRevision(page, 5);
  const actorAfterReloadMove = running.controller
    .view()
    .document.runtime.actors.find(({ id }) => id === actorId);
  assert.equal(
    actorAfterReloadMove.pose.x,
    actorBeforeReloadMove.pose.x + 100
  );
  await page.locator("#save-button").click();
  await page.waitForFunction(
    () => document.querySelector("#dirty-value")?.textContent === "否"
  );
  await checkContent(page, "ready");

  let releaseReload;
  let announceReloadRequest;
  const reloadHold = new Promise((resolve) => {
    releaseReload = resolve;
  });
  const reloadRequestSeen = new Promise((resolve) => {
    announceReloadRequest = resolve;
  });
  let holdNextPreviewPackage = true;
  await page.route("**/api/preview-package?token=*", async (route) => {
    if (!holdNextPreviewPackage) {
      await route.continue();
      return;
    }
    holdNextPreviewPackage = false;
    announceReloadRequest();
    await reloadHold;
    await route.continue();
  });
  await page.locator("#preview-reload-button").click();
  await reloadRequestSeen;
  assert.equal(
    await page.locator(".preview-frame.preview-live").getAttribute("src"),
    launchFrameUrl
  );
  assert.equal(
    await page.locator(".preview-frame.preview-candidate").isHidden(),
    true
  );
  await page.screenshot({
    path: path.join(evidenceDirectory, "03-reload-keeps-live.png"),
    fullPage: true
  });
  releaseReload();
  await page.waitForFunction(
    (previousUrl) => {
      const live = document.querySelector(".preview-frame.preview-live");
      return (
        live !== null &&
        live.getAttribute("src") !== previousUrl &&
        document.querySelector(".preview-frame.preview-candidate") === null
      );
    },
    launchFrameUrl,
    { timeout: 60_000 }
  );
  await page.unroute("**/api/preview-package?token=*");
  const reloadPublication = structuredClone(
    running.controller.view().preview.publication
  );
  assert.equal(reloadPublication.generation, launchPublication.generation + 1);
  assert.notEqual(
    reloadPublication.packageSha256,
    launchPublication.packageSha256
  );
  const reloadFrameUrl = await page
    .locator(".preview-frame.preview-live")
    .getAttribute("src");
  const reloadFrame = page
    .frames()
    .find((frame) => frame.url().includes(reloadPublication.token));
  assert.ok(reloadFrame, "reloaded Preview frame was not attached");
  const reloadedRuntime = await reloadFrame.evaluate(() =>
    window.__tgdSystemDemo.getState()
  );
  assert.equal(reloadedRuntime.ready, true);
  assert.equal(reloadedRuntime.packageByteCount, reloadPublication.packageBytes);
  assert.equal(reloadedRuntime.workshopFunds, 100);
  assert.equal(reloadedRuntime.orderFulfilled, false);
  assert.equal(reloadedRuntime.shortcutOpen, false);
  await page.screenshot({
    path: path.join(evidenceDirectory, "04-reloaded-live.png"),
    fullPage: true
  });

  await selectTreeObject(page, "interactions", interactionId);
  await duplicateSelected(page, {
    id: invalidInteractionId,
    label: "Demo 0.8.2 无效候选交互",
    revision: 6
  });
  await checkContent(page, "validation_failed");
  assert.equal(running.controller.view().contentCheck.hasPreparedPackage, true);
  assert.deepEqual(
    running.controller.view().preview.publication,
    reloadPublication
  );
  assert.equal(
    await page.locator(".preview-frame.preview-live").getAttribute("src"),
    reloadFrameUrl
  );
  assert.equal(
    await page.locator("#preview-reload-button").getAttribute("aria-disabled"),
    "true"
  );
  await page.screenshot({
    path: path.join(evidenceDirectory, "05-invalid-keeps-live.png"),
    fullPage: true
  });

  await page.waitForTimeout(500);
  for (const frame of page.frames()) {
    const shellErrors = await frame
      .evaluate(() =>
        typeof window.__tgdSystemDemo?.getPageErrors === "function"
          ? window.__tgdSystemDemo.getPageErrors()
          : []
      )
      .catch(() => []);
    pageErrors.push(...shellErrors);
  }
  assert.deepEqual(consoleErrors, [], "browser console errors were emitted");
  assert.deepEqual(pageErrors, [], "browser page errors were emitted");
  assert.deepEqual(requestErrors, [], "browser request errors were emitted");

  const evidence = {
    taskId: "DEMO-085",
    productVersion: "Demo 0.8.5",
    commit: execFileSync("git", ["rev-parse", "HEAD"], {
      cwd: repositoryRoot,
      encoding: "utf8"
    }).trim(),
    browser: {
      requested: "edge",
      used: "edge",
      version: browser.version()
    },
    route: {
      url: running.url,
      regressionTaskId: "DEMO-082",
      craftPanels: craftPanels.map(({ kind, id }) => ({ kind, id })),
      workshopPanels: workshopPanels.map(({ kind, id }) => ({ kind, id })),
      createdActor: actorId,
      createdInteraction: interactionId,
      removedInteraction: sourceInteractionId,
      savedRevision
    },
    canvasMoves: {
      first: [
        actorBeforeCanvasMove.pose.x,
        actorAfterCanvasMove.pose.x
      ],
      reload: [
        actorBeforeReloadMove.pose.x,
        actorAfterReloadMove.pose.x
      ]
    },
    launch: {
      publication: launchPublication,
      runtime: launchedRuntime
    },
    reload: {
      publication: reloadPublication,
      runtime: reloadedRuntime,
      oldFrameStayedVisibleUntilReady: true
    },
    invalidDraft: {
      revision: running.controller.view().revision,
      rejectedInteraction: invalidInteractionId,
      diagnosticCount:
        running.controller.view().contentCheck.diagnostics.length,
      keptPublicationGeneration: reloadPublication.generation,
      keptLiveFrame: true
    },
    consoleErrors,
    pageErrors,
    requestErrors,
    screenshots: [
      "00-craft-workshop-update-only-panels.png",
      "01-crud-diagnostic.png",
      "02-launch-live.png",
      "03-reload-keeps-live.png",
      "04-reloaded-live.png",
      "05-invalid-keeps-live.png"
    ]
  };
  await writeFile(
    path.join(evidenceDirectory, "evidence.json"),
    `${JSON.stringify(evidence, null, 2)}\n`,
    "utf8"
  );
  process.stdout.write(
    `[demo-085] real Workbench route passed ${JSON.stringify({
      browser: evidence.browser,
      revisions: [4, 5, 6],
      publications: [
        launchPublication.generation,
        reloadPublication.generation
      ],
      packageChanged:
        launchPublication.packageSha256 !== reloadPublication.packageSha256,
      craftPanels: craftPanels.length,
      workshopPanels: workshopPanels.length,
      evidenceDirectory
    })}\n`
  );
} finally {
  await context?.close();
  await browser?.close();
  await running.close();
  await rm(workspaceRoot, { recursive: true, force: true });
}
