import assert from "node:assert/strict";
import { copyFile, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import test from "node:test";

import { chromium } from "playwright";

import { startWorkbenchServer } from "../src/workbench-server.mjs";

const fixtureUrl = new URL(
  "./fixtures/system-demo-authoring.v1.valid.json",
  import.meta.url
);

async function waitForRevision(page, minimum) {
  await page.waitForFunction(
    (value) =>
      Number(document.querySelector("#revision-value")?.textContent) >= value,
    minimum
  );
}

async function focusLabel(page) {
  return page.evaluate(() => {
    const active = document.activeElement;
    return (
      active?.id ||
      active?.dataset?.treeKey ||
      active?.getAttribute?.("name") ||
      active?.tagName ||
      "none"
    );
  });
}

async function liveSnapshot(page, stage) {
  return {
    stage,
    status: await page.locator("#status-live").textContent(),
    error: await page.locator("#error-live").textContent(),
    conflict: await page.locator("#conflict-banner").isVisible()
  };
}

async function selectObject(page, kind) {
  const item = page.locator(
    '.tree-object[data-object-kind="' + kind + '"]'
  ).first();
  await item.click();
  assert.equal(await focusLabel(page), "object-tree");
}

test(
  "real Chromium covers the six-object editor, buffered input, focus, and conflict",
  { timeout: 90_000 },
  async (t) => {
    const root = await mkdtemp(path.join(tmpdir(), "tgd-browser-workbench-"));
    await copyFile(fixtureUrl, path.join(root, "demo.json"));
    await writeFile(path.join(root, "malformed.json"), "{", "utf8");
    const running = await startWorkbenchServer({ workspaceRoot: root });
    let browser = null;
    t.after(async () => {
      if (browser) {
        await browser.close();
      }
      await running.close();
      await rm(root, { recursive: true, force: true });
    });
    browser = await chromium.launch({ headless: true, channel: "msedge" });

    t.diagnostic("Chromium " + browser.version());
    t.diagnostic("Workbench origin " + new URL(running.url).origin);
    const page = await browser.newPage({ viewport: { width: 1100, height: 820 } });
    const focusTrace = [];
    const liveTrace = [];
    const consoleErrors = [];
    const expectedConflictConsole = [];
    const expectedInvalidLoadConsole = [];
    const pageErrors = [];
    const requestErrors = [];
    const unexpectedResponses = [];
    let externalConflictResponses = 0;
    page.on("console", (message) => {
      if (message.type() === "error") {
        if (message.text().includes("status of 409 (Conflict)")) {
          expectedConflictConsole.push(message.text());
        } else if (message.text().includes("status of 400 (Bad Request)")) {
          expectedInvalidLoadConsole.push(message.text());
        } else {
          consoleErrors.push(message.text());
        }
      }
    });
    page.on("pageerror", (error) => pageErrors.push(error.message));
    page.on("requestfailed", (request) =>
      requestErrors.push(request.url() + ": " + request.failure()?.errorText)
    );
    page.on("response", (response) => {
      if (
        response.status() === 409 &&
        response.url().endsWith("/api/save")
      ) {
        externalConflictResponses += 1;
      }
      if (
        response.status() >= 400 &&
        !(
          (response.status() === 409 && response.url().endsWith("/api/save")) ||
          (response.status() === 400 && response.url().endsWith("/api/open"))
        )
      ) {
        unexpectedResponses.push(response.status() + " " + response.url());
      }
    });

    await page.goto(running.url, { waitUntil: "domcontentloaded" });
    await page.waitForFunction(
      () => document.activeElement?.id === "open-button"
    );
    focusTrace.push("empty:" + (await focusLabel(page)));
    assert.equal(await focusLabel(page), "open-button");
    assert.equal(
      await page
        .getByText("字段格式可保存；尚未进行内容与玩法校验。", { exact: true })
        .count(),
      1
    );

    await page.locator("#path-input").fill("demo.json");
    await page.locator("#path-input").press("Enter");
    await page.waitForFunction(
      () => document.querySelector("#opened-path")?.textContent === "demo.json"
    );
    focusTrace.push("opened:" + (await focusLabel(page)));
    assert.equal(await focusLabel(page), "object-tree");
    liveTrace.push(await liveSnapshot(page, "opened"));

    const tree = page.locator("#object-tree");
    assert.equal(await tree.getAttribute("tabindex"), "0");
    assert.equal(await page.locator('#object-tree [tabindex="0"]').count(), 0);
    assert.equal(
      await page.locator('#object-tree [role="treeitem"]').count() >= 12,
      true
    );
    await tree.press("Home");
    assert.equal(
      await page.evaluate(() => {
        const id = document
          .querySelector("#object-tree")
          ?.getAttribute("aria-activedescendant");
        return id ? document.getElementById(id)?.dataset.treeKey : null;
      }),
      "group:player"
    );
    await tree.press("ArrowRight");
    await tree.press("Enter");
    assert.equal(await focusLabel(page), "object-tree");
    await tree.press("End");
    await tree.press("ArrowLeft");
    await tree.press("Home");
    focusTrace.push("tree-keyboard:" + (await focusLabel(page)));

    await selectObject(page, "actors");
    const editorBeforeInvalidOpen = running.controller.editorState;
    await page.locator("#path-input").fill("malformed.json");
    await page.locator("#path-input").press("Enter");
    await page.waitForFunction(
      () => document.querySelector("#error-live")?.textContent.includes("字段格式")
    );
    assert.strictEqual(running.controller.editorState, editorBeforeInvalidOpen);
    assert.equal(await page.locator("#opened-path").textContent(), "demo.json");
    assert.equal(await page.locator("#path-input").inputValue(), "demo.json");
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.match(await page.locator("#selection-summary").textContent(), /actors \/ actor\.demo/);
    assert.equal(
      await page.locator('[role="alert"]:visible').evaluateAll((nodes) =>
        nodes.filter((node) => node.textContent.trim().length > 0).length
      ),
      1
    );
    liveTrace.push(await liveSnapshot(page, "invalid-load"));

    await selectObject(page, "safePoints");
    const invalidInput = page.locator('input[name="x"]');
    const editorBeforeInvalidField = running.controller.editorState;
    const lastValidBeforeInvalidField =
      running.controller.editorState.lastValidDocument;
    let invalidFieldUpdateRequests = 0;
    const countInvalidFieldUpdates = (request) => {
      if (request.url().endsWith("/api/update")) {
        invalidFieldUpdateRequests += 1;
      }
    };
    page.on("request", countInvalidFieldUpdates);
    await invalidInput.fill("1.5");
    await page.locator("#apply-button").click();
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await invalidInput.getAttribute("aria-invalid"), "true");
    assert.equal(await focusLabel(page), "x");
    assert.equal(
      await page.locator("#apply-button").getAttribute("aria-busy"),
      "false"
    );
    assert.equal(
      await page.locator("#apply-button").getAttribute("aria-disabled"),
      "false"
    );
    await invalidInput.press("Enter");
    assert.equal(await focusLabel(page), "x");
    assert.equal(await invalidInput.getAttribute("aria-invalid"), "true");
    page.off("request", countInvalidFieldUpdates);
    assert.equal(invalidFieldUpdateRequests, 0);
    assert.strictEqual(running.controller.editorState, editorBeforeInvalidField);
    assert.strictEqual(
      running.controller.editorState.lastValidDocument,
      lastValidBeforeInvalidField
    );
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await page.locator("#save-button").isDisabled(), true);
    liveTrace.push(await liveSnapshot(page, "invalid-buffer"));

    await selectObject(page, "actors");
    await selectObject(page, "safePoints");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "1.5");
    assert.equal(
      await page.locator('input[name="x"]').getAttribute("aria-invalid"),
      "true"
    );

    page.once("dialog", async (dialog) => {
      assert.equal(dialog.type(), "confirm");
      await dialog.dismiss();
    });
    await page.locator("#reload-button").click();
    await page.waitForFunction(
      () => document.querySelector("#status-live")?.textContent.includes("已取消重新加载")
    );
    assert.equal(await page.locator('input[name="x"]').inputValue(), "1.5");
    await page.locator('input[name="x"]').press("Escape");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "-1000");
    assert.equal(await page.locator("#save-button").isDisabled(), false);

    let interceptedApply;
    let releaseApply;
    let duplicateApplyRequests = 0;
    const applyIntercepted = new Promise((resolve) => {
      interceptedApply = resolve;
    });
    const applyReleased = new Promise((resolve) => {
      releaseApply = resolve;
    });
    const holdApplyRoute = async (route) => {
      duplicateApplyRequests += 1;
      interceptedApply();
      await applyReleased;
      await route.continue();
    };
    await page.route("**/api/update", holdApplyRoute);
    await selectObject(page, "player");
    await page.locator('input[name="x"]').fill("101");
    const applyButton = page.locator("#apply-button");
    await applyButton.scrollIntoViewIfNeeded();
    const applyBox = await applyButton.boundingBox();
    assert.ok(applyBox);
    const doubleApply = page.mouse.dblclick(
      applyBox.x + applyBox.width / 2,
      applyBox.y + applyBox.height / 2
    );
    await applyIntercepted;
    assert.equal(await applyButton.getAttribute("aria-busy"), "true");
    assert.equal(await applyButton.getAttribute("aria-disabled"), "true");
    assert.equal(await focusLabel(page), "apply-button");
    releaseApply();
    await doubleApply;
    await waitForRevision(page, 1);
    await page.unroute("**/api/update", holdApplyRoute);
    assert.equal(duplicateApplyRequests, 1);
    assert.equal(await page.locator("#revision-value").textContent(), "1");
    assert.equal(await page.locator("#dirty-value").textContent(), "是");
    assert.equal(await page.locator("#error-live").textContent(), "");
    assert.equal(await applyButton.getAttribute("aria-busy"), "false");
    assert.equal(await applyButton.getAttribute("aria-disabled"), "false");
    assert.equal(await focusLabel(page), "apply-button");

    const edits = [
      ["actors", "x", "2201"],
      ["groundBlockers", "minX", "999"],
      ["safePoints", "x", "-999"],
      ["interactions", "x", "501"],
      ["mechanisms", "x", "1301"]
    ];
    let revision = 1;
    for (const [kind, field, value] of edits) {
      await selectObject(page, kind);
      const idInput = page.locator('input[name="id"]');
      assert.equal(await idInput.isEditable(), false);
      await page.locator('[name="' + field + '"]').fill(value);
      await page.locator("#apply-button").click();
      revision += 1;
      await waitForRevision(page, revision);
    }
    assert.equal(await page.locator("#dirty-value").textContent(), "是");

    let interceptedSave;
    let releaseSave;
    let duplicateSaveRequests = 0;
    const saveIntercepted = new Promise((resolve) => {
      interceptedSave = resolve;
    });
    const saveReleased = new Promise((resolve) => {
      releaseSave = resolve;
    });
    const holdSaveRoute = async (route) => {
      duplicateSaveRequests += 1;
      interceptedSave();
      await saveReleased;
      await route.continue();
    };
    await page.route("**/api/save", holdSaveRoute);
    const saveButton = page.locator("#save-button");
    await saveButton.scrollIntoViewIfNeeded();
    const saveBox = await saveButton.boundingBox();
    assert.ok(saveBox);
    const doubleClick = page.mouse.dblclick(
      saveBox.x + saveBox.width / 2,
      saveBox.y + saveBox.height / 2
    );
    await saveIntercepted;
    assert.equal(await saveButton.getAttribute("aria-busy"), "true");
    assert.equal(await saveButton.getAttribute("aria-disabled"), "true");
    assert.equal(await focusLabel(page), "save-button");
    await page.keyboard.press("Control+s");
    releaseSave();
    await doubleClick;
    await page.waitForFunction(
      () => document.querySelector("#dirty-value")?.textContent === "否"
    );
    await page.unroute("**/api/save", holdSaveRoute);
    assert.equal(duplicateSaveRequests, 1);
    assert.equal(
      await page.locator("#saved-revision-value").textContent(),
      await page.locator("#revision-value").textContent()
    );
    assert.equal(await page.locator("#conflict-banner").isHidden(), true);
    assert.equal(await saveButton.getAttribute("aria-busy"), "false");
    assert.equal(await saveButton.getAttribute("aria-disabled"), "false");
    focusTrace.push("saved:" + (await focusLabel(page)));
    assert.equal(await focusLabel(page), "save-button");
    liveTrace.push(await liveSnapshot(page, "saved"));

    await page.reload({ waitUntil: "domcontentloaded" });
    await page.waitForFunction(
      () => document.querySelector("#opened-path")?.textContent === "demo.json"
    );
    assert.equal(await focusLabel(page), "object-tree");
    await selectObject(page, "player");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "101");

    const cdp = await page.context().newCDPSession(page);
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 2 });
    await page.waitForFunction(() => window.visualViewport?.scale >= 1.99);
    const desktopZoom = await page.evaluate(() => window.visualViewport?.scale);
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
    );
    assert.equal(
      await page.evaluate(() => document.documentElement.style.zoom),
      ""
    );
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 1 });

    await page.setViewportSize({ width: 520, height: 800 });
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
    );
    assert.equal(
      await page.evaluate(
        () =>
          getComputedStyle(document.querySelector(".workbench-grid"))
            .gridTemplateColumns.split(" ").length
      ),
      1
    );
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 2 });
    await page.waitForFunction(() => window.visualViewport?.scale >= 1.99);
    const mobileZoom = await page.evaluate(() => window.visualViewport?.scale);
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
    );
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 1 });
    await page.setViewportSize({ width: 1100, height: 820 });
    t.diagnostic(
      "browser page scale: desktop=" + desktopZoom + ", 520x800=" + mobileZoom
    );

    await selectObject(page, "actors");
    await page.locator('input[name="x"]').fill("2202");
    await page.locator("#apply-button").click();
    await waitForRevision(page, 7);
    page.once("dialog", async (dialog) => {
      assert.equal(dialog.type(), "confirm");
      await dialog.dismiss();
    });
    await page.locator("#reload-button").click();
    assert.equal(await page.locator("#dirty-value").textContent(), "是");

    const lastValidBeforeExternalConflict =
      running.controller.editorState.lastValidDocument;
    const external = JSON.parse(
      await readFile(path.join(root, "demo.json"), "utf8")
    );
    external.editor.items[0].label = "外部修改";
    const externalBytes = JSON.stringify(external, null, 2) + "\n";
    await writeFile(path.join(root, "demo.json"), externalBytes, "utf8");
    await page.locator("#save-button").click();
    await page.waitForFunction(
      () => document.querySelector("#conflict-banner")?.hidden === false
    );
    focusTrace.push("conflict:" + (await focusLabel(page)));
    assert.equal(await focusLabel(page), "save-button");
    assert.equal(await page.locator("#conflict-dialog").getAttribute("open"), null);
    assert.equal(await readFile(path.join(root, "demo.json"), "utf8"), externalBytes);
    assert.equal(await page.locator("#save-button").isDisabled(), true);
    assert.equal(externalConflictResponses, 1);
    assert.strictEqual(
      running.controller.editorState.lastValidDocument,
      lastValidBeforeExternalConflict
    );
    assert.equal(await page.locator("#error-live").textContent(), "");
    assert.equal(
      await page.locator('[role="alert"]:visible').evaluateAll((nodes) =>
        nodes.filter((node) => node.textContent.trim().length > 0).length
      ),
      1
    );
    liveTrace.push(await liveSnapshot(page, "conflict"));

    await page.locator("#resolve-conflict-button").click();
    await page.waitForFunction(
      () => document.querySelector("#conflict-dialog")?.open === true
    );
    focusTrace.push("conflict-dialog:" + (await focusLabel(page)));
    assert.equal(await focusLabel(page), "continue-editing-button");
    await page.locator("#continue-editing-button").press("Escape");
    await page.waitForFunction(
      () => document.querySelector("#conflict-dialog")?.open === false
    );
    assert.equal(await focusLabel(page), "resolve-conflict-button");
    await page.locator("#resolve-conflict-button").click();
    assert.equal(await focusLabel(page), "continue-editing-button");
    await page.locator("#continue-editing-button").click();
    assert.equal(await focusLabel(page), "resolve-conflict-button");

    const forbidden = /^(validate|export|preview|run|校验|导出|试玩|运行)$/i;
    const controlLabels = await page
      .locator("button, input[type=button], input[type=submit]")
      .allTextContents();
    assert.equal(
      controlLabels.some((label) => forbidden.test(label.trim())),
      false
    );
    const visibleText = await page.locator("body").innerText();
    assert.equal(/\bCAS\b|sha256|JSONPath|Stable key|Stable hash/i.test(visibleText), false);
    assert.equal(/可导出|可试玩|DEV-valid/i.test(visibleText), false);

    t.diagnostic("focus trace: " + JSON.stringify(focusTrace));
    t.diagnostic("live-region trace: " + JSON.stringify(liveTrace));
    t.diagnostic("console errors: " + consoleErrors.length);
    t.diagnostic(
      "invalid-field /api/update requests: " + invalidFieldUpdateRequests
    );
    t.diagnostic("duplicate-apply requests: " + duplicateApplyRequests);
    t.diagnostic("duplicate-save requests: " + duplicateSaveRequests);
    t.diagnostic("external-conflict 409 responses: " + externalConflictResponses);
    t.diagnostic(
      "expected conflict console entries: " + expectedConflictConsole.length
    );
    t.diagnostic(
      "expected invalid-load console entries: " +
        expectedInvalidLoadConsole.length
    );
    t.diagnostic("page errors: " + pageErrors.length);
    t.diagnostic("request errors: " + requestErrors.length);
    t.diagnostic("unexpected HTTP errors: " + unexpectedResponses.length);
    assert.deepEqual(consoleErrors, []);
    assert.equal(expectedConflictConsole.length, 1);
    assert.equal(expectedInvalidLoadConsole.length, 1);
    assert.deepEqual(pageErrors, []);
    assert.deepEqual(requestErrors, []);
    assert.deepEqual(unexpectedResponses, []);
  }
);
