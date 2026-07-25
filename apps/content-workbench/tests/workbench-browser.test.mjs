import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { copyFile, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

import { chromium } from "playwright";

import { startWorkbenchServer } from "../src/workbench-server.mjs";

const fixtureUrl = new URL(
  "../../../content/design/system-demo.sandbox.json",
  import.meta.url
);
const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));

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

function browserIdentity(generation) {
  return Object.freeze({
    generation,
    checksum: Object.freeze(Array(32).fill(generation))
  });
}

function browserCompilerResult({
  outcome,
  generation,
  diagnostics = [],
  packageBytes = outcome === 1 ? new Uint8Array([84, 71, 68, generation]) : null
}) {
  return Object.freeze({
    complete: true,
    outcome,
    compileStatus: outcome === 1 ? 1 : 2,
    packageError: outcome === 1 ? 0 : 17,
    identity: browserIdentity(generation),
    diagnostics: Object.freeze(diagnostics),
    bindingValidation: Object.freeze({
      code: 1,
      domain: 255,
      field: 65535,
      recordIndex: 0,
      subjectId: null,
      relatedId: null
    }),
    packageBytes
  });
}

function createBrowserCompilerService() {
  let identity = browserIdentity(0);
  let requests = 0;
  let closed = 0;
  return {
    get requests() {
      return requests;
    },
    get closed() {
      return closed;
    },
    identity() {
      return browserIdentity(identity.generation);
    },
    compileAndPublish(runtime, expectedIdentity) {
      requests += 1;
      assert.deepEqual(expectedIdentity, identity);
      assert.equal(Object.hasOwn(runtime, "editor"), false);
      if (runtime.player.pose.x === 909) {
        throw new Error("injected compiler transport detail");
      }
      if (runtime.actors[0].regionId === "region.missing") {
        return browserCompilerResult({
          outcome: 2,
          generation: identity.generation,
          diagnostics: [
            Object.freeze({
              code: 21,
              severity: 1,
              section: 6,
              field: 10,
              recordIndex: 0,
              subjectId: runtime.actors[0].id,
              relatedId: null
            })
          ]
        });
      }
      identity = browserIdentity(identity.generation + 1);
      return browserCompilerResult({
        outcome: 1,
        generation: identity.generation
      });
    },
    close() {
      closed += 1;
    }
  };
}

test(
  "real Chromium covers the six-object editor, buffered input, focus, and conflict",
  { timeout: 90_000 },
  async (t) => {
    const root = await mkdtemp(path.join(tmpdir(), "tgd-browser-workbench-"));
    await copyFile(fixtureUrl, path.join(root, "demo.json"));
    await copyFile(fixtureUrl, path.join(root, "other.json"));
    await writeFile(path.join(root, "malformed.json"), "{", "utf8");
    let workspaceFaultInjector = null;
    const running = await startWorkbenchServer({
      workspaceRoot: root,
      async faultInjector(name) {
        await workspaceFaultInjector?.(name);
      }
    });
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
    const expectedStaleSaveConsole = [];
    const expectedInvalidLoadConsole = [];
    const pageErrors = [];
    const requestErrors = [];
    const unexpectedResponses = [];
    let externalConflictResponses = 0;
    let ignoredStaleSaveResponses = 0;
    let staleSaveFailureActive = false;
    page.on("console", (message) => {
      if (message.type() === "error") {
        if (message.text().includes("status of 409 (Conflict)")) {
          if (staleSaveFailureActive) {
            expectedStaleSaveConsole.push(message.text());
          } else {
            expectedConflictConsole.push(message.text());
          }
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
        if (staleSaveFailureActive) {
          ignoredStaleSaveResponses += 1;
        } else {
          externalConflictResponses += 1;
        }
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
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "共享内容检查当前不可用。作者草稿保持不变。"
    );
    assert.equal(await page.locator("#content-check-button").isDisabled(), true);

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
    assert.match(
      await page.locator("#selection-summary").textContent(),
      /actors \/ actor\.system_demo\.entry\.slot_a/
    );
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
    assert.equal(await page.locator('input[name="x"]').inputValue(), "-1250");
    assert.equal(await page.locator("#save-button").isDisabled(), false);

    let interceptedApply;
    let releaseApply;
    let duplicateApplyRequests = 0;
    let applyPendingSaveRequests = 0;
    const saveButton = page.locator("#save-button");
    const countApplyPendingSaves = (request) => {
      if (request.url().endsWith("/api/save")) {
        applyPendingSaveRequests += 1;
      }
    };
    page.on("request", countApplyPendingSaves);
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
    await page.evaluate(() => document.querySelector("#save-button").click());
    releaseApply();
    await doubleApply;
    await waitForRevision(page, 1);
    await page.unroute("**/api/update", holdApplyRoute);
    page.off("request", countApplyPendingSaves);
    assert.equal(duplicateApplyRequests, 1);
    assert.equal(applyPendingSaveRequests, 0);
    assert.equal(await page.locator("#revision-value").textContent(), "1");
    assert.equal(await page.locator("#dirty-value").textContent(), "是");
    assert.equal(await page.locator("#error-live").textContent(), "");
    assert.equal(await applyButton.getAttribute("aria-busy"), "false");
    assert.equal(await applyButton.getAttribute("aria-disabled"), "false");
    assert.equal(await focusLabel(page), "apply-button");

    let delayedSaveSuccessSeen;
    let releaseDelayedSaveSuccess;
    let delayedSaveSuccessRequests = 0;
    let delayedSaveSuccessUpdates = 0;
    const delayedSaveSuccessReady = new Promise((resolve) => {
      delayedSaveSuccessSeen = resolve;
    });
    const delayedSaveSuccessReleased = new Promise((resolve) => {
      releaseDelayedSaveSuccess = resolve;
    });
    const countDelayedSaveSuccessUpdates = (request) => {
      if (request.url().endsWith("/api/update")) {
        delayedSaveSuccessUpdates += 1;
      }
    };
    const holdSuccessfulSaveResponse = async (route) => {
      delayedSaveSuccessRequests += 1;
      const response = await route.fetch();
      delayedSaveSuccessSeen();
      await delayedSaveSuccessReleased;
      await route.fulfill({ response });
    };
    page.on("request", countDelayedSaveSuccessUpdates);
    await page.route("**/api/save", holdSuccessfulSaveResponse);
    await saveButton.click();
    await delayedSaveSuccessReady;
    assert.equal(running.controller.editorState.revision, 1);
    assert.equal(running.controller.editorState.savedRevision, 1);
    assert.equal(running.controller.editorState.dirty, false);
    assert.equal(
      running.controller.editorState.document.runtime.player.pose.x,
      101
    );
    assert.equal(
      JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
      101
    );
    await page.locator('input[name="x"]').fill("102");
    await applyButton.click();
    await waitForRevision(page, 2);
    assert.equal(await focusLabel(page), "apply-button");
    releaseDelayedSaveSuccess();
    await page.waitForFunction(
      () => document.querySelector("#save-button")?.getAttribute("aria-busy") === "false"
    );
    await page.unroute("**/api/save", holdSuccessfulSaveResponse);
    page.off("request", countDelayedSaveSuccessUpdates);
    assert.equal(delayedSaveSuccessRequests, 1);
    assert.equal(delayedSaveSuccessUpdates, 1);
    assert.equal(await page.locator("#revision-value").textContent(), "2");
    assert.equal(await page.locator("#saved-revision-value").textContent(), "1");
    assert.equal(await page.locator("#dirty-value").textContent(), "是");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "102");
    assert.equal(await focusLabel(page), "apply-button");
    assert.equal(await page.locator("#conflict-banner").isHidden(), true);
    assert.equal(await page.locator("#error-live").textContent(), "");
    assert.equal(running.controller.editorState.revision, 2);
    assert.equal(running.controller.editorState.savedRevision, 1);
    assert.equal(running.controller.editorState.dirty, true);
    assert.equal(
      running.controller.editorState.document.runtime.player.pose.x,
      102
    );
    assert.equal(
      running.controller.editorState.lastValidDocument.runtime.player.pose.x,
      102
    );
    assert.equal(
      JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
      101
    );

    let staleSaveFailureSeen;
    let releaseStaleSaveFailure;
    let staleSaveFailureRequests = 0;
    let staleSaveFailureUpdates = 0;
    const staleSaveFailureReady = new Promise((resolve) => {
      staleSaveFailureSeen = resolve;
    });
    const staleSaveFailureReleased = new Promise((resolve) => {
      releaseStaleSaveFailure = resolve;
    });
    const countStaleSaveFailureUpdates = (request) => {
      if (request.url().endsWith("/api/update")) {
        staleSaveFailureUpdates += 1;
      }
    };
    const holdStaleSaveRequest = async (route) => {
      staleSaveFailureRequests += 1;
      staleSaveFailureSeen();
      await staleSaveFailureReleased;
      await route.continue();
    };
    page.on("request", countStaleSaveFailureUpdates);
    await page.route("**/api/save", holdStaleSaveRequest);
    staleSaveFailureActive = true;
    await saveButton.click();
    await staleSaveFailureReady;
    await page.locator('input[name="x"]').fill("103");
    await applyButton.click();
    await waitForRevision(page, 3);
    releaseStaleSaveFailure();
    await page.waitForFunction(
      () => document.querySelector("#save-button")?.getAttribute("aria-busy") === "false"
    );
    staleSaveFailureActive = false;
    await page.unroute("**/api/save", holdStaleSaveRequest);
    page.off("request", countStaleSaveFailureUpdates);
    assert.equal(staleSaveFailureRequests, 1);
    assert.equal(staleSaveFailureUpdates, 1);
    assert.equal(ignoredStaleSaveResponses, 1);
    assert.equal(await page.locator("#revision-value").textContent(), "3");
    assert.equal(await page.locator("#saved-revision-value").textContent(), "1");
    assert.equal(await page.locator("#dirty-value").textContent(), "是");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "103");
    assert.equal(await focusLabel(page), "apply-button");
    assert.equal(await page.locator("#conflict-banner").isHidden(), true);
    assert.equal(await page.locator("#error-live").textContent(), "");

    let bufferedSaveSeen;
    let releaseBufferedSave;
    let bufferedSaveRequests = 0;
    let bufferedSaveUpdates = 0;
    const bufferedSaveReady = new Promise((resolve) => {
      bufferedSaveSeen = resolve;
    });
    const bufferedSaveReleased = new Promise((resolve) => {
      releaseBufferedSave = resolve;
    });
    const countBufferedSaveUpdates = (request) => {
      if (request.url().endsWith("/api/update")) {
        bufferedSaveUpdates += 1;
      }
    };
    const holdBufferedSaveResponse = async (route) => {
      bufferedSaveRequests += 1;
      const response = await route.fetch();
      bufferedSaveSeen();
      await bufferedSaveReleased;
      await route.fulfill({ response });
    };
    page.on("request", countBufferedSaveUpdates);
    await page.route("**/api/save", holdBufferedSaveResponse);
    await saveButton.click();
    await bufferedSaveReady;
    const bufferedX = page.locator('input[name="x"]');
    await bufferedX.fill("104");
    await page.locator('input[name="y"]').focus();
    assert.equal(await focusLabel(page), "y");
    releaseBufferedSave();
    await page.waitForFunction(
      () => document.querySelector("#save-button")?.getAttribute("aria-busy") === "false"
    );
    await page.unroute("**/api/save", holdBufferedSaveResponse);
    page.off("request", countBufferedSaveUpdates);
    assert.equal(bufferedSaveRequests, 1);
    assert.equal(bufferedSaveUpdates, 0);
    assert.equal(await page.locator("#revision-value").textContent(), "3");
    assert.equal(await page.locator("#saved-revision-value").textContent(), "3");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await bufferedX.inputValue(), "104");
    assert.equal(await focusLabel(page), "y");
    assert.equal(running.controller.editorState.revision, 3);
    assert.equal(running.controller.editorState.savedRevision, 3);
    assert.equal(running.controller.editorState.dirty, false);
    assert.equal(
      running.controller.editorState.document.runtime.player.pose.x,
      103
    );
    assert.equal(
      running.controller.editorState.lastValidDocument.runtime.player.pose.x,
      103
    );
    assert.equal(
      JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
      103
    );
    await bufferedX.focus();
    await bufferedX.press("Escape");
    assert.equal(await bufferedX.inputValue(), "103");

    await page.locator('input[name="x"]').fill("105");
    await applyButton.click();
    await waitForRevision(page, 4);
    let serverReplaceSeen;
    let releaseServerReplace;
    let delayServerReplace = true;
    let serverDelayedSaveRequests = 0;
    let serverDelayedSaveUpdates = 0;
    const serverReplaceReady = new Promise((resolve) => {
      serverReplaceSeen = resolve;
    });
    const serverReplaceReleased = new Promise((resolve) => {
      releaseServerReplace = resolve;
    });
    workspaceFaultInjector = async (name) => {
      if (delayServerReplace && name === "replace") {
        delayServerReplace = false;
        serverReplaceSeen();
        await serverReplaceReleased;
      }
    };
    const countServerDelayedRequests = (request) => {
      if (request.url().endsWith("/api/save")) {
        serverDelayedSaveRequests += 1;
      }
      if (request.url().endsWith("/api/update")) {
        serverDelayedSaveUpdates += 1;
      }
    };
    page.on("request", countServerDelayedRequests);
    await saveButton.click();
    await serverReplaceReady;
    await page.locator('input[name="x"]').fill("106");
    await applyButton.click();
    await waitForRevision(page, 5);
    assert.equal(await focusLabel(page), "apply-button");
    releaseServerReplace();
    await page.waitForFunction(
      () => document.querySelector("#save-button")?.getAttribute("aria-busy") === "false"
    );
    workspaceFaultInjector = null;
    assert.equal(await page.locator("#revision-value").textContent(), "5");
    assert.equal(await page.locator("#saved-revision-value").textContent(), "3");
    assert.equal(await page.locator("#dirty-value").textContent(), "是");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "106");
    assert.equal(await focusLabel(page), "apply-button");
    assert.equal(running.controller.editorState.revision, 5);
    assert.equal(running.controller.editorState.savedRevision, 3);
    assert.equal(running.controller.editorState.dirty, true);
    assert.equal(
      running.controller.editorState.document.runtime.player.pose.x,
      106
    );
    assert.equal(
      running.controller.editorState.lastValidDocument.runtime.player.pose.x,
      106
    );
    assert.equal(
      JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
      105
    );
    assert.equal(externalConflictResponses, 0);
    await saveButton.click();
    await page.waitForFunction(
      () => document.querySelector("#dirty-value")?.textContent === "否"
    );
    page.off("request", countServerDelayedRequests);
    assert.equal(serverDelayedSaveRequests, 2);
    assert.equal(serverDelayedSaveUpdates, 1);
    assert.equal(externalConflictResponses, 0);
    assert.equal(
      JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
      106
    );

    await page.locator('input[name="x"]').fill("107");
    await applyButton.click();
    await waitForRevision(page, 6);
    let lateOpenSaveSeen;
    let releaseLateOpenSave;
    let lateOpenSaveRequests = 0;
    const lateOpenSaveReady = new Promise((resolve) => {
      lateOpenSaveSeen = resolve;
    });
    const lateOpenSaveReleased = new Promise((resolve) => {
      releaseLateOpenSave = resolve;
    });
    const holdSaveResponseAcrossOpen = async (route) => {
      lateOpenSaveRequests += 1;
      const response = await route.fetch();
      lateOpenSaveSeen();
      await lateOpenSaveReleased;
      await route.fulfill({ response });
    };
    await page.route("**/api/save", holdSaveResponseAcrossOpen);
    await saveButton.click();
    await lateOpenSaveReady;
    page.once("dialog", async (dialog) => {
      assert.equal(dialog.type(), "confirm");
      await dialog.accept();
    });
    await page.locator("#path-input").fill("other.json");
    await page.locator("#path-input").press("Enter");
    await page.waitForFunction(
      () => document.querySelector("#opened-path")?.textContent === "other.json"
    );
    assert.equal(await focusLabel(page), "object-tree");
    releaseLateOpenSave();
    await page.waitForFunction(
      () => document.querySelector("#save-button")?.getAttribute("aria-busy") === "false"
    );
    await page.unroute("**/api/save", holdSaveResponseAcrossOpen);
    assert.equal(lateOpenSaveRequests, 1);
    assert.equal(await page.locator("#opened-path").textContent(), "other.json");
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "0");
    assert.equal(await focusLabel(page), "object-tree");
    assert.equal(running.controller.view().relativePath, "other.json");
    assert.equal(running.controller.view().conflict, false);

    await page.locator('input[name="x"]').fill("201");
    await applyButton.click();
    await waitForRevision(page, 1);
    let lateReloadSaveSeen;
    let releaseLateReloadSave;
    let lateReloadSaveRequests = 0;
    const lateReloadSaveReady = new Promise((resolve) => {
      lateReloadSaveSeen = resolve;
    });
    const lateReloadSaveReleased = new Promise((resolve) => {
      releaseLateReloadSave = resolve;
    });
    const holdSaveResponseAcrossReload = async (route) => {
      lateReloadSaveRequests += 1;
      const response = await route.fetch();
      lateReloadSaveSeen();
      await lateReloadSaveReleased;
      await route.fulfill({ response });
    };
    await page.route("**/api/save", holdSaveResponseAcrossReload);
    await saveButton.click();
    await lateReloadSaveReady;
    page.once("dialog", async (dialog) => {
      assert.equal(dialog.type(), "confirm");
      await dialog.accept();
    });
    await page.locator("#reload-button").click();
    await page.waitForFunction(
      () =>
        document.querySelector("#revision-value")?.textContent === "0" &&
        document.querySelector('input[name="x"]')?.value === "201"
    );
    assert.equal(await focusLabel(page), "object-tree");
    releaseLateReloadSave();
    await page.waitForFunction(
      () => document.querySelector("#save-button")?.getAttribute("aria-busy") === "false"
    );
    await page.unroute("**/api/save", holdSaveResponseAcrossReload);
    assert.equal(lateReloadSaveRequests, 1);
    assert.equal(await page.locator("#opened-path").textContent(), "other.json");
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "201");
    assert.equal(await focusLabel(page), "object-tree");
    assert.equal(running.controller.view().relativePath, "other.json");
    assert.equal(running.controller.view().conflict, false);

    await page.locator('input[name="x"]').fill("202");
    await applyButton.click();
    await waitForRevision(page, 1);
    let reloadErrorSaveSeen;
    let releaseReloadErrorSave;
    let reloadErrorResponseSeen;
    let releaseReloadErrorResponse;
    let reloadErrorSaveRequests = 0;
    const reloadErrorSaveReady = new Promise((resolve) => {
      reloadErrorSaveSeen = resolve;
    });
    const reloadErrorSaveReleased = new Promise((resolve) => {
      releaseReloadErrorSave = resolve;
    });
    const reloadErrorResponseReady = new Promise((resolve) => {
      reloadErrorResponseSeen = resolve;
    });
    const reloadErrorResponseReleased = new Promise((resolve) => {
      releaseReloadErrorResponse = resolve;
    });
    const holdSaveErrorAcrossReload = async (route) => {
      reloadErrorSaveRequests += 1;
      const response = await route.fetch();
      const savedState = (await response.json()).state;
      reloadErrorSaveSeen();
      await reloadErrorSaveReleased;
      await route.fulfill({
        status: 409,
        contentType: "application/json",
        body: JSON.stringify({
          error: {
            code: "stale_revision",
            message: "injected late save error"
          },
          state: savedState
        })
      });
    };
    const holdReloadAfterSaveError = async (route) => {
      const response = await route.fetch();
      reloadErrorResponseSeen();
      await reloadErrorResponseReleased;
      await route.fulfill({ response });
    };
    await page.route("**/api/save", holdSaveErrorAcrossReload);
    await page.route("**/api/reload", holdReloadAfterSaveError);
    await saveButton.click();
    await reloadErrorSaveReady;
    page.once("dialog", async (dialog) => {
      assert.equal(dialog.type(), "confirm");
      await dialog.accept();
    });
    await page.locator("#reload-button").click();
    await reloadErrorResponseReady;
    staleSaveFailureActive = true;
    releaseReloadErrorSave();
    await page.waitForFunction(
      () => document.querySelector("#error-live")?.textContent.trim().length > 0
    );
    staleSaveFailureActive = false;
    releaseReloadErrorResponse();
    await page.waitForFunction(
      () =>
        document.querySelector("#revision-value")?.textContent === "0" &&
        document.querySelector('input[name="x"]')?.value === "202" &&
        document.querySelector("#error-live")?.textContent === ""
    );
    await page.unroute("**/api/save", holdSaveErrorAcrossReload);
    await page.unroute("**/api/reload", holdReloadAfterSaveError);
    assert.equal(reloadErrorSaveRequests, 1);
    assert.equal(await page.locator("#opened-path").textContent(), "other.json");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await focusLabel(page), "object-tree");
    assert.equal(running.controller.view().relativePath, "other.json");
    assert.equal(running.controller.view().conflict, false);

    await page.locator('input[name="x"]').fill("203");
    await applyButton.click();
    await waitForRevision(page, 1);
    let openErrorSaveSeen;
    let releaseOpenErrorSave;
    let openErrorResponseSeen;
    let releaseOpenErrorResponse;
    let openErrorSaveRequests = 0;
    const openErrorSaveReady = new Promise((resolve) => {
      openErrorSaveSeen = resolve;
    });
    const openErrorSaveReleased = new Promise((resolve) => {
      releaseOpenErrorSave = resolve;
    });
    const openErrorResponseReady = new Promise((resolve) => {
      openErrorResponseSeen = resolve;
    });
    const openErrorResponseReleased = new Promise((resolve) => {
      releaseOpenErrorResponse = resolve;
    });
    const holdSaveErrorAcrossOpen = async (route) => {
      openErrorSaveRequests += 1;
      const response = await route.fetch();
      const savedState = (await response.json()).state;
      openErrorSaveSeen();
      await openErrorSaveReleased;
      await route.fulfill({
        status: 409,
        contentType: "application/json",
        body: JSON.stringify({
          error: {
            code: "stale_revision",
            message: "injected late save error"
          },
          state: savedState
        })
      });
    };
    const holdOpenAfterSaveError = async (route) => {
      const response = await route.fetch();
      openErrorResponseSeen();
      await openErrorResponseReleased;
      await route.fulfill({ response });
    };
    await page.route("**/api/save", holdSaveErrorAcrossOpen);
    await page.route("**/api/open", holdOpenAfterSaveError);
    await saveButton.click();
    await openErrorSaveReady;
    page.once("dialog", async (dialog) => {
      assert.equal(dialog.type(), "confirm");
      await dialog.accept();
    });
    await page.locator("#path-input").fill("demo.json");
    await page.locator("#path-input").press("Enter");
    await openErrorResponseReady;
    staleSaveFailureActive = true;
    releaseOpenErrorSave();
    await page.waitForFunction(
      () => document.querySelector("#error-live")?.textContent.trim().length > 0
    );
    staleSaveFailureActive = false;
    releaseOpenErrorResponse();
    await page.waitForFunction(
      () =>
        document.querySelector("#opened-path")?.textContent === "demo.json" &&
        document.querySelector("#error-live")?.textContent === ""
    );
    await page.unroute("**/api/save", holdSaveErrorAcrossOpen);
    await page.unroute("**/api/open", holdOpenAfterSaveError);
    assert.equal(openErrorSaveRequests, 1);
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(await page.locator("#dirty-value").textContent(), "否");
    assert.equal(await focusLabel(page), "object-tree");
    assert.equal(running.controller.view().relativePath, "demo.json");
    assert.equal(running.controller.view().conflict, false);

    await selectObject(page, "player");
    assert.equal(await page.locator('input[name="x"]').inputValue(), "107");

    const edits = [
      ["actors", "x", "2201"],
      ["groundBlockers", "minX", "999"],
      ["safePoints", "x", "-999"],
      ["interactions", "x", "501"],
      ["mechanisms", "x", "1301"]
    ];
    let revision = 0;
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
    assert.equal(await page.locator('input[name="x"]').inputValue(), "107");

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
    await waitForRevision(page, 6);
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
    assert.equal(await page.locator("#save-button").isDisabled(), false);
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
    t.diagnostic(
      "stale-save success requests/update requests: " +
        delayedSaveSuccessRequests +
        "/" +
        delayedSaveSuccessUpdates
    );
    t.diagnostic(
      "stale-save failure requests/update requests: " +
        staleSaveFailureRequests +
        "/" +
        staleSaveFailureUpdates
    );
    t.diagnostic(
      "buffered-save requests/update requests: " +
        bufferedSaveRequests +
        "/" +
        bufferedSaveUpdates
    );
    t.diagnostic(
      "server-delayed save requests/update requests: " +
        serverDelayedSaveRequests +
        "/" +
        serverDelayedSaveUpdates
    );
    t.diagnostic(
      "document-epoch late save requests: open=" +
        lateOpenSaveRequests +
        ", reload=" +
        lateReloadSaveRequests
    );
    t.diagnostic(
      "document-switch late save errors: open=" +
        openErrorSaveRequests +
        ", reload=" +
        reloadErrorSaveRequests
    );
    t.diagnostic("external-conflict 409 responses: " + externalConflictResponses);
    t.diagnostic(
      "expected conflict console entries: " + expectedConflictConsole.length
    );
    t.diagnostic(
      "expected stale-save console entries: " + expectedStaleSaveConsole.length
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
    assert.equal(expectedStaleSaveConsole.length, 3);
    assert.equal(expectedInvalidLoadConsole.length, 1);
    assert.deepEqual(pageErrors, []);
    assert.deepEqual(requestErrors, []);
    assert.deepEqual(unexpectedResponses, []);
  }
);

test(
  "real Edge presents shared diagnostics without disturbing authoring focus",
  { timeout: 90_000 },
  async (t) => {
    const root = await mkdtemp(path.join(tmpdir(), "tgd-browser-diagnostics-"));
    await copyFile(fixtureUrl, path.join(root, "demo.json"));
    await copyFile(fixtureUrl, path.join(root, "other.json"));
    const compilerService = createBrowserCompilerService();
    const running = await startWorkbenchServer({
      workspaceRoot: root,
      sandboxService: compilerService
    });
    let browser = null;
    t.after(async () => {
      if (browser) {
        await browser.close();
      }
      await running.close();
      await rm(root, { recursive: true, force: true });
    });
    browser = await chromium.launch({ headless: true, channel: "msedge" });
    const page = await browser.newPage({ viewport: { width: 1100, height: 820 } });
    const consoleErrors = [];
    const pageErrors = [];
    const requestErrors = [];
    const unexpectedHttp = [];
    const expectedConflictHttp = [];
    const expectedConflictConsole = [];
    const expectedStaleCheckHttp = [];
    const expectedStaleCheckConsole = [];
    let conflictRequestExpected = false;
    let staleCheckResponseExpected = false;
    let contentCheckHttpRequests = 0;
    page.on("console", (message) => {
      if (message.type() === "error") {
        if (
          staleCheckResponseExpected &&
          message.text().includes("409 (Conflict)")
        ) {
          expectedStaleCheckConsole.push(message.text());
        } else if (
          conflictRequestExpected &&
          message.text().includes("409 (Conflict)")
        ) {
          expectedConflictConsole.push(message.text());
        } else {
          consoleErrors.push(message.text());
        }
      }
    });
    page.on("pageerror", (error) => pageErrors.push(error.message));
    page.on("requestfailed", (request) =>
      requestErrors.push(request.url() + ": " + request.failure()?.errorText)
    );
    page.on("request", (request) => {
      if (request.url().endsWith("/api/content-check")) {
        contentCheckHttpRequests += 1;
      }
    });
    page.on("response", (response) => {
      if (response.status() >= 400) {
        const entry = response.status() + " " + response.url();
        if (
          response.status() === 409 &&
          response.url().endsWith("/api/save")
        ) {
          expectedConflictHttp.push(entry);
        } else if (
          response.status() === 409 &&
          response.url().endsWith("/api/content-check")
        ) {
          expectedStaleCheckHttp.push(entry);
        } else {
          unexpectedHttp.push(entry);
        }
      }
    });

    await page.goto(running.url, { waitUntil: "domcontentloaded" });
    await page.locator("#path-input").fill("demo.json");
    await page.locator("#path-input").press("Enter");
    await page.waitForFunction(
      () => document.querySelector("#opened-path")?.textContent === "demo.json"
    );
    const checkButton = page.locator("#content-check-button");
    assert.equal(await checkButton.isDisabled(), false);
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "尚未执行共享内容检查。"
    );
    await page.evaluate(() => {
      window.__tgdDiagnosticAnnouncements = [];
      const target = document.querySelector("#diagnostic-count-live");
      new MutationObserver(() => {
        window.__tgdDiagnosticAnnouncements.push(target.textContent);
      }).observe(target, {
        childList: true,
        characterData: true,
        subtree: true
      });
    });

    await selectObject(page, "actors");
    const noPackageActorRegion = page.locator('input[name="regionId"]');
    const noPackageActorOriginal = await noPackageActorRegion.inputValue();
    await noPackageActorRegion.fill("region.missing");
    await page.locator("#apply-button").click();
    await waitForRevision(page, 1);
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "共享内容检查未通过；尚无已准备包。"
    );
    assert.match(
      await page.locator("#diagnostic-count-live").textContent(),
      /^共享内容检查发现 \d+ 个问题。$/
    );
    assert.equal(running.controller.validatedPackageEvidence(), null);
    await noPackageActorRegion.fill(noPackageActorOriginal);
    await page.locator("#apply-button").click();
    await waitForRevision(page, 2);
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；尚无已准备包。"
    );
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    await page.locator("#save-button").click();
    await page.waitForFunction(
      () => document.querySelector("#dirty-value")?.textContent === "否"
    );
    await page.locator("#reload-button").click();
    await waitForRevision(page, 0);
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    await selectObject(page, "player");
    await page.evaluate(() => {
      window.__tgdDiagnosticAnnouncements = [];
    });
    const baselineContentCheckRequests = contentCheckHttpRequests;
    const baselineServiceRequests = compilerService.requests;

    const bufferedX = page.locator('input[name="x"]');
    const initialX = await bufferedX.inputValue();
    const preExistingBufferValue = initialX === "40" ? "42" : "40";
    await bufferedX.fill(preExistingBufferValue);
    assert.equal(await checkButton.getAttribute("disabled"), null);
    assert.equal(await checkButton.getAttribute("aria-disabled"), "true");
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；尚无已准备包。"
    );
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    await checkButton.focus();
    const bufferedGuardBox = await checkButton.boundingBox();
    assert.ok(bufferedGuardBox);
    await page.mouse.click(
      bufferedGuardBox.x + bufferedGuardBox.width / 2,
      bufferedGuardBox.y + bufferedGuardBox.height / 2
    );
    await page.keyboard.press("Enter");
    await page.waitForTimeout(50);
    assert.equal(contentCheckHttpRequests, baselineContentCheckRequests);
    assert.equal(compilerService.requests, baselineServiceRequests);
    assert.equal(await focusLabel(page), "content-check-button");
    await bufferedX.focus();
    await bufferedX.press("Escape");
    assert.equal(await bufferedX.inputValue(), initialX);
    assert.equal(await checkButton.getAttribute("aria-disabled"), "false");
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；尚无已准备包。"
    );
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");

    let releaseReady;
    let readyRequestSeen;
    let checkRequests = 0;
    const readyRelease = new Promise((resolve) => {
      releaseReady = resolve;
    });
    const readySeen = new Promise((resolve) => {
      readyRequestSeen = resolve;
    });
    const holdReadyResponse = async (route) => {
      checkRequests += 1;
      const response = await route.fetch();
      readyRequestSeen();
      await readyRelease;
      await route.fulfill({ response });
    };
    await page.route("**/api/content-check", holdReadyResponse);
    await checkButton.scrollIntoViewIfNeeded();
    const checkBox = await checkButton.boundingBox();
    assert.ok(checkBox);
    const duplicateCheck = page.mouse.dblclick(
      checkBox.x + checkBox.width / 2,
      checkBox.y + checkBox.height / 2
    );
    await readySeen;
    assert.equal(await checkButton.getAttribute("aria-busy"), "true");
    assert.equal(await checkButton.getAttribute("aria-disabled"), "true");
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "正在执行共享内容检查。"
    );
    const inFlightBufferValue = initialX === "41" ? "43" : "41";
    await bufferedX.fill(inFlightBufferValue);
    await page.locator('input[name="y"]').focus();
    assert.equal(await focusLabel(page), "y");
    assert.equal(await checkButton.getAttribute("aria-disabled"), "true");
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；尚无已准备包。"
    );
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    releaseReady();
    await duplicateCheck;
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-button")?.getAttribute(
          "aria-busy"
        ) === "false"
    );
    await page.unroute("**/api/content-check", holdReadyResponse);
    assert.equal(checkRequests, 1);
    assert.equal(compilerService.requests, baselineServiceRequests + 1);
    assert.equal(
      contentCheckHttpRequests,
      baselineContentCheckRequests + 1
    );
    assert.equal(await checkButton.getAttribute("aria-busy"), "false");
    assert.equal(await bufferedX.inputValue(), inFlightBufferValue);
    assert.equal(await focusLabel(page), "y");
    assert.equal(await page.locator("#revision-value").textContent(), "0");
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；尚无已准备包。"
    );
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    assert.equal(
      await page.evaluate(() =>
        window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
      ).then((values) => values.length),
      0
    );
    await bufferedX.focus();
    await bufferedX.press("Escape");
    assert.equal(await bufferedX.inputValue(), initialX);
    assert.equal(await checkButton.getAttribute("aria-disabled"), "false");
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；尚无已准备包。"
    );
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "包已准备；尚未导出，也未启动 Preview 或试玩。"
    );
    assert.equal(
      contentCheckHttpRequests,
      baselineContentCheckRequests + 2
    );
    assert.equal(compilerService.requests, baselineServiceRequests + 2);
    assert.equal(await focusLabel(page), "content-check-button");
    assert.equal(
      await page.evaluate(() =>
        window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
      ).then((values) => values.length),
      1
    );

    let releaseReloadLease;
    let reloadLeaseRequestSeen;
    let reloadLeaseRequests = 0;
    const reloadLeaseRelease = new Promise((resolve) => {
      releaseReloadLease = resolve;
    });
    const reloadLeaseSeen = new Promise((resolve) => {
      reloadLeaseRequestSeen = resolve;
    });
    const holdReloadLeaseBeforeServer = async (route) => {
      reloadLeaseRequests += 1;
      assert.equal(
        typeof route.request().postDataJSON().expectedDocumentLease,
        "string"
      );
      reloadLeaseRequestSeen();
      await reloadLeaseRelease;
      await route.continue();
    };
    await page.route(
      "**/api/content-check",
      holdReloadLeaseBeforeServer
    );
    const serviceBeforeReloadLease = compilerService.requests;
    const packageBeforeReloadLease =
      running.controller.validatedPackageEvidence();
    await checkButton.click();
    await reloadLeaseSeen;
    await page.locator("#reload-button").click();
    await page.waitForFunction(
      () => document.querySelector("#status-live")?.textContent.includes("重新加载")
    );
    const reloadedDocument = running.controller.editorState.document;
    const reloadedLastValid =
      running.controller.editorState.lastValidDocument;
    const reloadBufferedX = page.locator('input[name="x"]');
    const reloadAuthorX = await reloadBufferedX.inputValue();
    const reloadBufferValue = reloadAuthorX === "501" ? "502" : "501";
    await reloadBufferedX.fill(reloadBufferValue);
    await page.locator('input[name="y"]').focus();
    const announcementsBeforeReloadRelease = await page.evaluate(
      () =>
        window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
          .length
    );
    staleCheckResponseExpected = true;
    releaseReloadLease();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-button")?.getAttribute(
          "aria-busy"
        ) === "false"
    );
    staleCheckResponseExpected = false;
    await page.unroute(
      "**/api/content-check",
      holdReloadLeaseBeforeServer
    );
    assert.equal(reloadLeaseRequests, 1);
    assert.equal(compilerService.requests, serviceBeforeReloadLease);
    assert.deepEqual(
      running.controller.validatedPackageEvidence(),
      packageBeforeReloadLease
    );
    assert.strictEqual(running.controller.editorState.document, reloadedDocument);
    assert.strictEqual(
      running.controller.editorState.lastValidDocument,
      reloadedLastValid
    );
    assert.equal(await reloadBufferedX.inputValue(), reloadBufferValue);
    assert.equal(await focusLabel(page), "y");
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    assert.equal(
      await page.evaluate(
        () =>
          window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
            .length
      ),
      announcementsBeforeReloadRelease
    );
    await reloadBufferedX.focus();
    await reloadBufferedX.press("Escape");
    assert.equal(await reloadBufferedX.inputValue(), reloadAuthorX);
    assert.equal(await checkButton.getAttribute("aria-disabled"), "false");
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "包已准备；尚未导出，也未启动 Preview 或试玩。"
    );
    assert.equal(compilerService.requests, serviceBeforeReloadLease + 1);
    assert.equal(
      await page.evaluate(
        () =>
          window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
            .length
      ),
      2
    );

    let releaseOpenLease;
    let openLeaseRequestSeen;
    let openLeaseRequests = 0;
    const openLeaseRelease = new Promise((resolve) => {
      releaseOpenLease = resolve;
    });
    const openLeaseSeen = new Promise((resolve) => {
      openLeaseRequestSeen = resolve;
    });
    const holdOpenLeaseBeforeServer = async (route) => {
      openLeaseRequests += 1;
      openLeaseRequestSeen();
      await openLeaseRelease;
      await route.continue();
    };
    await page.route("**/api/content-check", holdOpenLeaseBeforeServer);
    const serviceBeforeOpenLease = compilerService.requests;
    const packageBeforeOpenLease =
      running.controller.validatedPackageEvidence();
    await checkButton.click();
    await openLeaseSeen;
    await page.locator("#path-input").fill("other.json");
    await page.locator("#path-input").press("Enter");
    await page.waitForFunction(
      () => document.querySelector("#opened-path")?.textContent === "other.json"
    );
    const openedDocument = running.controller.editorState.document;
    const openedLastValid = running.controller.editorState.lastValidDocument;
    assert.equal(await focusLabel(page), "object-tree");
    staleCheckResponseExpected = true;
    releaseOpenLease();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-button")?.getAttribute(
          "aria-busy"
        ) === "false"
    );
    staleCheckResponseExpected = false;
    await page.unroute("**/api/content-check", holdOpenLeaseBeforeServer);
    assert.equal(openLeaseRequests, 1);
    assert.equal(compilerService.requests, serviceBeforeOpenLease);
    assert.deepEqual(
      running.controller.validatedPackageEvidence(),
      packageBeforeOpenLease
    );
    assert.strictEqual(running.controller.editorState.document, openedDocument);
    assert.strictEqual(
      running.controller.editorState.lastValidDocument,
      openedLastValid
    );
    assert.equal(await focusLabel(page), "object-tree");
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "包已准备；尚未导出，也未启动 Preview 或试玩。"
    );
    assert.equal(compilerService.requests, serviceBeforeOpenLease + 1);
    assert.equal(
      await page.evaluate(
        () =>
          window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
            .length
      ),
      3
    );

    await selectObject(page, "actors");
    const actorRegion = page.locator('input[name="regionId"]');
    const originalActorRegion = await actorRegion.inputValue();
    await actorRegion.fill("region.missing");
    await page.locator("#apply-button").click();
    await waitForRevision(page, 1);
    await selectObject(page, "mechanisms");
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "共享内容检查未通过；上一份已准备包保持不变。"
    );
    assert.match(
      await page.locator("#selection-summary").textContent(),
      /^mechanisms \//
    );
    assert.equal(await focusLabel(page), "content-check-button");
    assert.equal(await page.locator("#diagnostic-list li").count(), 1);
    assert.equal(
      await page.locator("#diagnostic-list").getAttribute("aria-live"),
      null
    );
    assert.equal(
      await page.locator("#diagnostic-count-live").textContent(),
      "共享内容检查发现 1 个问题。"
    );
    assert.equal(
      await page.evaluate(() =>
        window.__tgdDiagnosticAnnouncements.filter((value) => value.trim())
      ).then((values) => values.length),
      4
    );
    await page.locator(".diagnostic-locator").click();
    assert.match(
      await page.locator("#selection-summary").textContent(),
      /^actors \//
    );
    assert.equal(await focusLabel(page), "regionId");

    const cdp = await page.context().newCDPSession(page);
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 2 });
    await page.waitForFunction(() => window.visualViewport?.scale >= 1.99);
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
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
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 2 });
    await page.waitForFunction(() => window.visualViewport?.scale >= 1.99);
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
    );
    const screenshotHash = createHash("sha256")
      .update(await page.screenshot())
      .digest("hex");
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 1 });
    await page.setViewportSize({ width: 1100, height: 820 });

    await actorRegion.fill(originalActorRegion);
    await page.locator("#apply-button").click();
    await waitForRevision(page, 2);
    let releaseStale;
    let staleRequestSeen;
    let staleRequests = 0;
    const staleRelease = new Promise((resolve) => {
      releaseStale = resolve;
    });
    const staleSeen = new Promise((resolve) => {
      staleRequestSeen = resolve;
    });
    const holdStaleResponse = async (route) => {
      staleRequests += 1;
      const response = await route.fetch();
      staleRequestSeen();
      await staleRelease;
      await route.fulfill({ response });
    };
    await page.route("**/api/content-check", holdStaleResponse);
    await checkButton.click();
    await staleSeen;
    await page.locator('input[name="x"]').fill("2301");
    await page.locator("#apply-button").click();
    await waitForRevision(page, 3);
    assert.equal(await focusLabel(page), "apply-button");
    releaseStale();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-button")?.getAttribute(
          "aria-busy"
        ) === "false"
    );
    await page.unroute("**/api/content-check", holdStaleResponse);
    assert.equal(staleRequests, 1);
    assert.equal(
      await page.locator("#content-check-summary").textContent(),
      "当前草稿的共享内容检查结果已过期；上一份已准备包保持不变。"
    );
    assert.equal(await focusLabel(page), "apply-button");
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");

    await selectObject(page, "player");
    await page.locator('input[name="x"]').fill("909");
    await page.locator("#apply-button").click();
    await waitForRevision(page, 4);
    const evidenceBeforeBridgeFailure =
      running.controller.validatedPackageEvidence();
    const documentBeforeBridgeFailure = running.controller.editorState.document;
    const lastValidBeforeBridgeFailure =
      running.controller.editorState.lastValidDocument;
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "共享内容检查未完成；作者草稿和已准备包状态未改变。"
    );
    assert.equal(await focusLabel(page), "content-check-button");
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    assert.deepEqual(
      running.controller.validatedPackageEvidence(),
      evidenceBeforeBridgeFailure
    );
    assert.strictEqual(
      running.controller.editorState.document,
      documentBeforeBridgeFailure
    );
    assert.strictEqual(
      running.controller.editorState.lastValidDocument,
      lastValidBeforeBridgeFailure
    );

    let releaseConflictLease;
    let conflictLeaseRequestSeen;
    let conflictLeaseRequests = 0;
    const conflictLeaseRelease = new Promise((resolve) => {
      releaseConflictLease = resolve;
    });
    const conflictLeaseSeen = new Promise((resolve) => {
      conflictLeaseRequestSeen = resolve;
    });
    const holdConflictLeaseBeforeServer = async (route) => {
      conflictLeaseRequests += 1;
      conflictLeaseRequestSeen();
      await conflictLeaseRelease;
      await route.continue();
    };
    await page.route(
      "**/api/content-check",
      holdConflictLeaseBeforeServer
    );
    const serviceBeforeConflictLease = compilerService.requests;
    const packageBeforeConflictLease =
      running.controller.validatedPackageEvidence();
    const documentBeforeConflictLease =
      running.controller.editorState.document;
    const lastValidBeforeConflictLease =
      running.controller.editorState.lastValidDocument;
    await checkButton.click();
    await conflictLeaseSeen;
    const external = JSON.parse(
      await readFile(path.join(root, "other.json"), "utf8")
    );
    external.editor.items[0].label = "诊断冲突外部修改";
    await writeFile(
      path.join(root, "other.json"),
      JSON.stringify(external, null, 2) + "\n",
      "utf8"
    );
    conflictRequestExpected = true;
    await page.locator("#save-button").click();
    await page.waitForFunction(
      () => document.querySelector("#conflict-banner")?.hidden === false
    );
    releaseConflictLease();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-button")?.getAttribute(
          "aria-busy"
        ) === "false"
    );
    conflictRequestExpected = false;
    await page.unroute(
      "**/api/content-check",
      holdConflictLeaseBeforeServer
    );
    assert.equal(conflictLeaseRequests, 1);
    assert.equal(compilerService.requests, serviceBeforeConflictLease);
    assert.deepEqual(
      running.controller.validatedPackageEvidence(),
      packageBeforeConflictLease
    );
    assert.strictEqual(
      running.controller.editorState.document,
      documentBeforeConflictLease
    );
    assert.strictEqual(
      running.controller.editorState.lastValidDocument,
      lastValidBeforeConflictLease
    );
    assert.equal(await checkButton.getAttribute("disabled"), null);
    assert.equal(await checkButton.getAttribute("aria-disabled"), "true");
    assert.equal(await page.locator("#diagnostic-count-live").textContent(), "");
    const requestsBeforeConflictGuard = contentCheckHttpRequests;
    const serviceRequestsBeforeConflictGuard = compilerService.requests;
    await checkButton.focus();
    const conflictGuardBox = await checkButton.boundingBox();
    assert.ok(conflictGuardBox);
    await page.mouse.click(
      conflictGuardBox.x + conflictGuardBox.width / 2,
      conflictGuardBox.y + conflictGuardBox.height / 2
    );
    await page.keyboard.press("Enter");
    await page.waitForTimeout(50);
    assert.equal(contentCheckHttpRequests, requestsBeforeConflictGuard);
    assert.equal(
      compilerService.requests,
      serviceRequestsBeforeConflictGuard
    );
    assert.equal(await focusLabel(page), "content-check-button");
    await page.locator("#resolve-conflict-button").click();
    await page.waitForFunction(
      () => document.querySelector("#conflict-dialog")?.open === true
    );
    await page.locator("#load-disk-button").click();
    await page.waitForFunction(
      () =>
        document.querySelector("#conflict-banner")?.hidden === true &&
        document.querySelector("#content-check-button")?.getAttribute(
          "aria-disabled"
        ) === "false"
    );
    await page.waitForFunction(
      () => document.activeElement?.id === "object-tree"
    );
    const serviceBeforeConflictRecovery = compilerService.requests;
    await checkButton.click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "包已准备；尚未导出，也未启动 Preview 或试玩。"
    );
    assert.equal(
      compilerService.requests,
      serviceBeforeConflictRecovery + 1
    );

    const bodyText = await page.locator("body").innerText();
    assert.equal(
      /\bgeneration\b|\bchecksum\b|\bCAS\b|Stable key|documentLease|expectedDocumentLease|lastError|JSONPath|\$\.|exception|stack/i.test(
        bodyText
      ),
      false
    );
    assert.equal(
      (await page.content()).includes(
        running.controller.view().documentLease
      ),
      false
    );
    const forbiddenControls = await page
      .locator("button, input[type=button], input[type=submit]")
      .allTextContents();
    assert.equal(
      forbiddenControls.some((label) =>
        /^(export|preview|run|导出|试玩|运行)$/i.test(label.trim())
      ),
      false
    );

    t.diagnostic("diagnostics browser: Edge " + browser.version());
    t.diagnostic(
      "content-check requests: ready=" +
        checkRequests +
        ", stale=" +
        staleRequests +
        ", total-http=" +
        contentCheckHttpRequests +
        ", total-service=" +
        compilerService.requests
    );
    t.diagnostic(
      "diagnostics focus: guarded=content-check-button, buffered=y, stale-result=y, explicit-ready=content-check-button, locator=regionId, stale-apply=apply-button, conflict=content-check-button, resolved=object-tree"
    );
    t.diagnostic("diagnostics screenshot sha256: " + screenshotHash);
    t.diagnostic("diagnostics console errors: " + consoleErrors.length);
    t.diagnostic(
      "diagnostics expected conflict console: " +
        expectedConflictConsole.length
    );
    t.diagnostic(
      "diagnostics expected stale-check console: " +
        expectedStaleCheckConsole.length
    );
    t.diagnostic("diagnostics page errors: " + pageErrors.length);
    t.diagnostic("diagnostics request errors: " + requestErrors.length);
    t.diagnostic(
      "diagnostics expected conflict HTTP: " + expectedConflictHttp.length
    );
    t.diagnostic(
      "diagnostics expected stale-check HTTP: " +
        expectedStaleCheckHttp.length
    );
    t.diagnostic("diagnostics unexpected HTTP: " + unexpectedHttp.length);
    assert.deepEqual(consoleErrors, []);
    assert.equal(expectedConflictConsole.length, 2);
    assert.equal(expectedStaleCheckConsole.length, 2);
    assert.deepEqual(pageErrors, []);
    assert.deepEqual(requestErrors, []);
    assert.equal(expectedConflictHttp.length, 1);
    assert.equal(expectedStaleCheckHttp.length, 3);
    assert.deepEqual(unexpectedHttp, []);
  }
);

const generatedBuildDirectory =
  process.env.TGD_SANDBOX_SERVICE_BUILD_DIRECTORY;
test(
  "real Edge loads the trusted generated module and prepares the unique source",
  {
    timeout: 90_000,
    skip: generatedBuildDirectory
      ? false
      : "set TGD_SANDBOX_SERVICE_BUILD_DIRECTORY for the real module browser gate"
  },
  async (t) => {
    const root = await mkdtemp(path.join(tmpdir(), "tgd-browser-real-module-"));
    await copyFile(fixtureUrl, path.join(root, "demo.json"));
    const running = await startWorkbenchServer({
      workspaceRoot: root,
      sandboxBuildDirectory: generatedBuildDirectory
    });
    let browser = null;
    t.after(async () => {
      if (browser) {
        await browser.close();
      }
      await running.close();
      await rm(root, { recursive: true, force: true });
    });
    browser = await chromium.launch({ headless: true, channel: "msedge" });
    const page = await browser.newPage({ viewport: { width: 1100, height: 820 } });
    const consoleErrors = [];
    const pageErrors = [];
    const requestErrors = [];
    const unexpectedHttp = [];
    let checkRequests = 0;
    page.on("console", (message) => {
      if (message.type() === "error") consoleErrors.push(message.text());
    });
    page.on("pageerror", (error) => pageErrors.push(error.message));
    page.on("requestfailed", (request) =>
      requestErrors.push(request.url() + ": " + request.failure()?.errorText)
    );
    page.on("request", (request) => {
      if (request.url().endsWith("/api/content-check")) checkRequests += 1;
    });
    page.on("response", (response) => {
      if (response.status() >= 400) {
        unexpectedHttp.push(response.status() + " " + response.url());
      }
    });

    await page.goto(running.url, { waitUntil: "domcontentloaded" });
    await page.locator("#path-input").fill("demo.json");
    await page.locator("#path-input").press("Enter");
    await page.waitForFunction(
      () => document.querySelector("#opened-path")?.textContent === "demo.json"
    );
    await page.locator("#content-check-button").click();
    await page.waitForFunction(
      () =>
        document.querySelector("#content-check-summary")?.textContent ===
        "包已准备；尚未导出，也未启动 Preview 或试玩。"
    );
    const evidence = running.controller.validatedPackageEvidence();
    assert.ok(evidence);
    assert.equal(evidence.revision, 0);
    assert.equal(evidence.generation, 1);
    assert.equal(checkRequests, 1);
    assert.equal(await focusLabel(page), "content-check-button");
    assert.equal(
      /generation|checksum|packageBytes|\bCAS\b|Stable key/i.test(
        await page.locator("body").innerText()
      ),
      false
    );

    const cdp = await page.context().newCDPSession(page);
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 2 });
    await page.waitForFunction(() => window.visualViewport?.scale >= 1.99);
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
    );
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 1 });
    await page.setViewportSize({ width: 520, height: 800 });
    await cdp.send("Emulation.setPageScaleFactor", { pageScaleFactor: 2 });
    await page.waitForFunction(() => window.visualViewport?.scale >= 1.99);
    assert.equal(
      await page.evaluate(
        () =>
          document.documentElement.scrollWidth <=
          document.documentElement.clientWidth + 1
      ),
      true
    );
    const screenshotHash = createHash("sha256")
      .update(await page.screenshot())
      .digest("hex");

    const sourceBytes = await readFile(fixtureUrl);
    const artifactRoot = path.join(
      repositoryRoot,
      generatedBuildDirectory,
      "dist",
      "web"
    );
    const moduleBytes = await readFile(
      path.join(artifactRoot, "tgd-sandbox-package-service-abi.mjs")
    );
    const wasmBytes = await readFile(
      path.join(artifactRoot, "tgd-sandbox-package-service-abi.wasm")
    );
    const digest = (bytes) =>
      createHash("sha256").update(bytes).digest("hex");
    t.diagnostic("real module browser: Edge " + browser.version());
    t.diagnostic("source sha256: " + digest(sourceBytes));
    t.diagnostic("generated module sha256: " + digest(moduleBytes));
    t.diagnostic("generated wasm sha256: " + digest(wasmBytes));
    t.diagnostic(
      "package identity: generation=" +
        evidence.generation +
        ", checksum=" +
        Buffer.from(evidence.checksum).toString("hex") +
        ", projection=" +
        evidence.projectionSha256 +
        ", package=" +
        evidence.packageSha256 +
        ", bytes=" +
        evidence.packageBytes
    );
    t.diagnostic("real module screenshot sha256: " + screenshotHash);
    t.diagnostic("real module console/page/request/http: 0/0/0/0");
    assert.deepEqual(consoleErrors, []);
    assert.deepEqual(pageErrors, []);
    assert.deepEqual(requestErrors, []);
    assert.deepEqual(unexpectedHttp, []);
  }
);
