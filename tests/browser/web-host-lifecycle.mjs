import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { createReadStream } from "node:fs";
import { mkdir, stat, writeFile } from "node:fs/promises";
import { createServer } from "node:http";
import { extname, relative, resolve, sep } from "node:path";

import { chromium, firefox } from "playwright";
import pngjs from "pngjs";

const { PNG } = pngjs;
const root = resolve(import.meta.dirname, "../..");

function argument(name) {
  const prefix = `--${name}=`;
  return process.argv.find((entry) => entry.startsWith(prefix))?.slice(prefix.length);
}

function commaList(value) {
  return value.split(",").map((entry) => entry.trim()).filter(Boolean);
}

const targets = commaList(
  argument("targets") ??
    process.env.TGD_BROWSER_TARGETS ??
    (process.platform === "win32" ? "chrome,edge,firefox" : "chromium,firefox")
);
const distDirectory = resolve(
  root,
  argument("dist") ??
    process.env.TGD_WEB_DIST ??
    "build/web-release-single/dist/web"
);
const reportDirectory = resolve(
  root,
  argument("report-dir") ??
    process.env.TGD_BROWSER_REPORT_DIR ??
    "build/browser-qa"
);
const expectedCommit = process.env.TGD_EXPECT_COMMIT?.trim() || null;
const forceSoftwareGraphics =
  process.env.TGD_FORCE_SOFTWARE_WEBGL === "1" || process.env.CI === "true";

const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
  [".json", "application/json; charset=utf-8"],
  [".png", "image/png"]
]);

function projectPath(path) {
  return relative(root, path).split(sep).join("/");
}

async function assertWebArtifacts() {
  for (const fileName of [
    "tiangongdu-f1.html",
    "tiangongdu-f1.js",
    "tiangongdu-f1.wasm",
    "service-worker.js",
    "tgd-replay-probe.html",
    "tgd-replay-probe.js",
    "tgd-replay-probe.wasm"
  ]) {
    const artifact = resolve(distDirectory, fileName);
    const metadata = await stat(artifact);
    assert(metadata.isFile(), `Web artifact is not a file: ${projectPath(artifact)}`);
    assert(metadata.size > 0, `Web artifact is empty: ${projectPath(artifact)}`);
  }
}

async function startStaticServer() {
  const directoryPrefix = `${distDirectory}${sep}`;
  const server = createServer(async (request, response) => {
    try {
      const url = new URL(request.url ?? "/", "http://127.0.0.1");
      if (url.pathname === "/favicon.ico") {
        response.writeHead(204, {
          "Cache-Control": "no-store",
          "Content-Length": 0
        }).end();
        return;
      }
      const pathname = decodeURIComponent(
        url.pathname === "/" ? "/tiangongdu-f1.html" : url.pathname
      );
      const requestedFile = resolve(distDirectory, `.${pathname}`);
      if (requestedFile !== distDirectory && !requestedFile.startsWith(directoryPrefix)) {
        response.writeHead(403).end("Forbidden");
        return;
      }

      const metadata = await stat(requestedFile);
      if (!metadata.isFile()) {
        response.writeHead(404).end("Not found");
        return;
      }

      response.writeHead(200, {
        "Cache-Control": "no-store",
        "Content-Length": metadata.size,
        "Content-Type": contentTypes.get(extname(requestedFile)) ?? "application/octet-stream",
        "X-Content-Type-Options": "nosniff"
      });
      createReadStream(requestedFile).pipe(response);
    } catch {
      response.writeHead(404).end("Not found");
    }
  });

  await new Promise((resolveListen, rejectListen) => {
    server.once("error", rejectListen);
    server.listen(0, "127.0.0.1", resolveListen);
  });
  const address = server.address();
  assert(address && typeof address === "object", "Static server did not expose an address.");
  return {
    origin: `http://127.0.0.1:${address.port}`,
    close: () => new Promise((resolveClose, rejectClose) => {
      server.close((error) => error ? rejectClose(error) : resolveClose());
    })
  };
}

function launchDefinition(target) {
  const chromiumSoftwareArguments = forceSoftwareGraphics
    ? ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"]
    : [];
  const firefoxSoftwarePreferences = forceSoftwareGraphics
    ? {
        "layers.d3d11.force-warp": true,
        "webgl.angle.force-d3d11": true,
        "webgl.angle.force-warp": true,
        "webgl.disable-fail-if-major-performance-caveat": true,
        "webgl.enable-webgl2": true,
        "webgl.forbid-software": false,
        "webgl.force-enabled": true
      }
    : undefined;
  switch (target) {
    case "chrome":
      return {
        browserType: chromium,
        launchOptions: { channel: "chrome", headless: true, args: chromiumSoftwareArguments }
      };
    case "edge":
      return {
        browserType: chromium,
        launchOptions: { channel: "msedge", headless: true, args: chromiumSoftwareArguments }
      };
    case "chromium":
      return {
        browserType: chromium,
        launchOptions: { headless: true, args: chromiumSoftwareArguments }
      };
    case "firefox":
      return {
        browserType: firefox,
        launchOptions: {
          headless: true,
          ...(firefoxSoftwarePreferences
            ? { firefoxUserPrefs: firefoxSoftwarePreferences }
            : {})
        }
      };
    default:
      throw new Error(`Unsupported browser target: ${target}`);
  }
}

async function waitForText(page, locator, expected, timeoutMs = 15_000) {
  const deadline = Date.now() + timeoutMs;
  let lastText = "";
  while (Date.now() < deadline) {
    try {
      lastText = (await locator.innerText({ timeout: 1_000 })).trim();
      if (lastText.includes(expected)) {
        return lastText;
      }
    } catch {
      // The document may still be replacing its startup text.
    }
    await page.waitForTimeout(100);
  }
  throw new Error(`Timed out waiting for "${expected}"; last text was "${lastText}".`);
}

async function waitForChangedText(page, locator, previous, expected, timeoutMs = 15_000) {
  const deadline = Date.now() + timeoutMs;
  let lastText = previous;
  while (Date.now() < deadline) {
    try {
      lastText = (await locator.innerText({ timeout: 1_000 })).trim();
      if (lastText !== previous && lastText.includes(expected)) {
        return lastText;
      }
    } catch {
      // The document may still be processing the browser event.
    }
    await page.waitForTimeout(100);
  }
  throw new Error(
    `Timed out waiting for a new "${expected}" event; last text was "${lastText}".`
  );
}

function analyzePng(buffer) {
  const png = PNG.sync.read(buffer);
  const uniqueColors = new Set();
  let goldPixels = 0;
  let opaquePixels = 0;
  for (let offset = 0; offset < png.data.length; offset += 4) {
    const red = png.data[offset];
    const green = png.data[offset + 1];
    const blue = png.data[offset + 2];
    const alpha = png.data[offset + 3];
    if (alpha < 250) continue;
    opaquePixels += 1;
    if (uniqueColors.size < 1024) uniqueColors.add((red << 16) | (green << 8) | blue);
    if (red >= 150 && green >= 80 && green <= 190 && blue <= 130) goldPixels += 1;
  }
  return {
    width: png.width,
    height: png.height,
    opaquePixels,
    uniqueColors: uniqueColors.size,
    goldPixelRatio: opaquePixels === 0 ? 0 : goldPixels / opaquePixels,
    sha256: createHash("sha256").update(buffer).digest("hex"),
    pixels: png.data
  };
}

function compareFrames(before, after) {
  assert.equal(after.width, before.width, "Restored canvas width changed.");
  assert.equal(after.height, before.height, "Restored canvas height changed.");
  let changedPixels = 0;
  const pixelCount = before.width * before.height;
  for (let offset = 0; offset < before.pixels.length; offset += 4) {
    const maximumDifference = Math.max(
      Math.abs(before.pixels[offset] - after.pixels[offset]),
      Math.abs(before.pixels[offset + 1] - after.pixels[offset + 1]),
      Math.abs(before.pixels[offset + 2] - after.pixels[offset + 2]),
      Math.abs(before.pixels[offset + 3] - after.pixels[offset + 3])
    );
    if (maximumDifference > 10) changedPixels += 1;
  }
  return {
    changedPixels,
    changedPixelRatio: changedPixels / pixelCount
  };
}

function publicFrame(frame) {
  const { pixels: _pixels, ...summary } = frame;
  return summary;
}

async function inspectWebgl(page, target) {
  return page.evaluate((useDebugRendererInfo) => {
    const canvas = document.querySelector("canvas");
    const context = canvas?.getContext("webgl2") ?? null;
    if (!context) return { available: false };
    const debugRendererInfo = useDebugRendererInfo
      ? context.getExtension("WEBGL_debug_renderer_info")
      : null;
    const contextLossExtension = context.getExtension("WEBGL_lose_context");
    return {
      available: true,
      contextLost: context.isContextLost(),
      contextLossExtension: Boolean(contextLossExtension),
      version: String(context.getParameter(context.VERSION)),
      shadingLanguageVersion: String(context.getParameter(context.SHADING_LANGUAGE_VERSION)),
      vendor: String(
        context.getParameter(debugRendererInfo?.UNMASKED_VENDOR_WEBGL ?? context.VENDOR)
      ),
      renderer: String(
        context.getParameter(debugRendererInfo?.UNMASKED_RENDERER_WEBGL ?? context.RENDERER)
      ),
      maxTextureSize: Number(context.getParameter(context.MAX_TEXTURE_SIZE))
    };
  }, target !== "firefox");
}

async function captureRenderedFrame(page, canvas, path, label, timeoutMs = 20_000) {
  const deadline = Date.now() + timeoutMs;
  let buffer;
  let frame;
  while (Date.now() < deadline) {
    buffer = await canvas.screenshot();
    frame = analyzePng(buffer);
    if (frame.uniqueColors >= 4 && frame.goldPixelRatio >= 0.005) {
      await writeFile(path, buffer);
      return frame;
    }
    await page.waitForTimeout(250);
  }
  if (buffer) await writeFile(path, buffer);
  throw new Error(
    `${label} did not render the bootstrap probe: ${frame?.uniqueColors ?? 0} colors, ${(
      (frame?.goldPixelRatio ?? 0) * 100
    ).toFixed(3)}% gold pixels.`
  );
}

function parseLifecycle(message) {
  const prefix = "[tgd.lifecycle] ";
  if (!message.startsWith(prefix)) return null;
  try {
    return JSON.parse(message.slice(prefix.length));
  } catch {
    return null;
  }
}

function parseReplayProbe(message) {
  const prefix = "[tgd.replay] ";
  if (!message.startsWith(prefix)) return null;
  try {
    return JSON.parse(message.slice(prefix.length));
  } catch {
    return null;
  }
}

async function waitForRecord(page, records, label, timeoutMs = 45_000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (records.length > 0) return records.at(-1);
    await page.waitForTimeout(100);
  }
  throw new Error(`Timed out waiting for ${label}.`);
}

function validateReplayProbe(record, target) {
  assert.equal(record.schemaVersion, "1.0.0", `${target} replay schema changed.`);
  assert.equal(record.status, "passed", `${target} WASM replay probe failed.`);
  assert(record.build, `${target} replay omitted its build identity.`);
  assert(record.build.version, `${target} replay omitted its semantic version.`);
  assert(record.build.commit, `${target} replay omitted its Git commit.`);
  assert(record.build.channel, `${target} replay omitted its build channel.`);
  assert.equal(record.formatMajor, 1, `${target} replay major changed.`);
  assert.equal(record.formatMinor, 0, `${target} replay minor changed.`);
  assert.equal(record.finalTick, 10_000, `${target} replay did not target 10,000 ticks.`);
  assert.match(
    record.contentFingerprint,
    /^[0-9a-f]{16}$/,
    `${target} replay content fingerprint is not canonical.`
  );
  assert.match(
    record.expectedChecksum,
    /^[0-9a-f]{16}$/,
    `${target} replay checksum is not canonical.`
  );
  assert(record.canonicalBytes > 0, `${target} replay emitted no canonical bytes.`);
  for (const field of [
    "validationError",
    "encodeError",
    "decodeError",
    "reencodeError"
  ]) {
    assert.equal(record[field], 0, `${target} replay ${field} was non-zero.`);
  }
  assert.deepEqual(
    record.cadences.map((cadence) => cadence.fps),
    [30, 60, 144],
    `${target} replay cadence matrix changed.`
  );
  const expectedPose = record.cadences[0]?.pose;
  assert(expectedPose, `${target} replay omitted its quantized pose.`);
  assert.notEqual(expectedPose.x, 0, `${target} replay did not exercise world x.`);
  assert.notEqual(expectedPose.y, 0, `${target} replay did not exercise world y.`);
  assert(expectedPose.height > 0, `${target} replay did not exercise independent height.`);
  assert.notEqual(
    expectedPose.floorLayer,
    0,
    `${target} replay did not exercise an independent floor layer.`
  );
  for (const cadence of record.cadences) {
    assert.equal(cadence.error, 0, `${target} ${cadence.fps} Hz replay failed.`);
    assert.equal(cadence.tick, record.finalTick, `${target} ${cadence.fps} Hz tick drifted.`);
    assert.equal(
      cadence.checksum,
      record.expectedChecksum,
      `${target} ${cadence.fps} Hz checksum drifted.`
    );
    assert(cadence.frames > 0, `${target} ${cadence.fps} Hz emitted no render frames.`);
    assert.deepEqual(
      cadence.pose,
      expectedPose,
      `${target} ${cadence.fps} Hz quantized pose drifted.`
    );
  }
  return record;
}

function isExpectedConsoleDiagnostic(target, type, text) {
  return (
    target === "firefox" &&
    type === "warning" &&
    text.includes('[JavaScript Warning: "WebGL context was lost."')
  );
}

async function runBrowser(target, origin) {
  const definition = launchDefinition(target);
  const consoleProblems = [];
  const expectedConsoleDiagnostics = [];
  const pageErrors = [];
  const requestFailures = [];
  const lifecycle = [];
  const replayProbes = [];
  const checkpoints = [];
  let browser;
  let context;

  try {
    browser = await definition.browserType.launch(definition.launchOptions);
    context = await browser.newContext({ viewport: { width: 1280, height: 960 } });
    const page = await context.newPage();
    page.on("console", (message) => {
      const text = message.text();
      const type = message.type();
      if (type === "warning" || type === "error") {
        const diagnostic = { type, text };
        if (isExpectedConsoleDiagnostic(target, type, text)) {
          expectedConsoleDiagnostics.push(diagnostic);
        } else {
          consoleProblems.push(diagnostic);
        }
      }
      const record = parseLifecycle(text);
      if (record) lifecycle.push(record);
      const replayProbe = parseReplayProbe(text);
      if (replayProbe) replayProbes.push(replayProbe);
    });
    page.on("pageerror", (error) => pageErrors.push(error.message));
    page.on("requestfailed", (request) => {
      if (request.url().startsWith(origin)) {
        requestFailures.push({
          url: request.url(),
          error: request.failure()?.errorText ?? "unknown"
        });
      }
    });

    const url = `${origin}/tiangongdu-f1.html?qa=1`;
    const response = await page.goto(url, { waitUntil: "domcontentloaded", timeout: 45_000 });
    assert(response?.ok(), `${target} failed to load ${url}: HTTP ${response?.status()}`);
    assert.equal(await page.title(), "天工渡 · F1 Web Host");

    const status = page.locator("#status");
    const state = page.getByTestId("qa-state");
    const profileState = page.getByTestId("profile-state");
    const saveProfile = page.getByTestId("save-profile");
    const lastEvent = page.locator("#last-event");
    const canvas = page.locator("canvas");
    await waitForText(page, status, "宿主已就绪", 45_000);
    await waitForText(page, state, "presentation: running");
    await canvas.waitFor({ state: "visible" });
    assert.equal(await canvas.count(), 1, "Exactly one game canvas is required.");
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    const initialProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(initialProfile.hasSnapshot, false, `${target} Guest Profile was not fresh.`);
    assert.equal(initialProfile.committedSaveCount, "0");
    assert.equal(await saveProfile.isEnabled(), true, `${target} Guest save never became available.`);
    assert.deepEqual(
      await page.evaluate(() => window.__tgdProfile.inspectSchema()),
      {
        name: "tiangongdu.prototype_f1.internal.profile",
        version: 1,
        stores: [
          "device_settings",
          "migration_workspace",
          "operations",
          "profile_heads",
          "profile_meta",
          "snapshots"
        ]
      },
      `${target} IndexedDB v1 schema drifted.`
    );

    await saveProfile.click();
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.committedSaveCount === "1",
      undefined,
      { timeout: 15_000 }
    );
    await waitForText(page, profileState, "已保存");
    const savedProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(savedProfile.stateName, "ready");
    assert.equal(savedProfile.hasSnapshot, true);
    assert.equal(savedProfile.hasPendingSave, false);
    assert.equal(savedProfile.logicalSequence, "1");
    const saveTrace = await page.evaluate(() => window.__tgdLifecycleTrace
      .filter((record) => record.event === "profile.state")
      .map((record) => ({ ...record })));
    const savingIndex = saveTrace.findIndex((record) => record.stateName === "saving");
    const committedIndex = saveTrace.findIndex(
      (record) => record.stateName === "ready" && record.committedSaveCount === "1"
    );
    assert(savingIndex >= 0, `${target} omitted the saving state.`);
    assert(committedIndex > savingIndex, `${target} claimed commit before the saving state.`);
    assert.equal(saveTrace[savingIndex].committedSaveCount, "0");
    assert.doesNotMatch(saveTrace[savingIndex].displayText, /已保存/);
    assert.match(saveTrace[committedIndex].displayText, /已保存/);

    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await waitForText(page, state, "presentation: running");
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    await waitForText(page, profileState, "已恢复");
    const restoredProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(restoredProfile.hasSnapshot, true);
    assert.equal(restoredProfile.hasPendingSave, false);
    assert.equal(restoredProfile.committedSaveCount, "0");
    assert.equal(restoredProfile.logicalSequence, "1");
    assert.equal(restoredProfile.snapshotId, savedProfile.snapshotId);
    assert.doesNotMatch(await profileState.innerText(), /已保存/);

    const quotaUrl = `${origin}/tiangongdu-f1.html?qa=1&storageFault=quota_once`;
    const quotaResponse = await page.goto(quotaUrl, {
      waitUntil: "domcontentloaded",
      timeout: 45_000
    });
    assert(quotaResponse?.ok(), `${target} failed to load quota fixture.`);
    await waitForText(page, status, "宿主已就绪", 45_000);
    await waitForText(page, state, "presentation: running");
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    assert.equal((await page.evaluate(() => window.__tgdProfile.getState())).logicalSequence, "1");
    await saveProfile.click();
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.errorName === "storage_quota",
      undefined,
      { timeout: 15_000 }
    );
    const quotaProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(quotaProfile.stateName, "save_failed");
    assert.equal(quotaProfile.hasPendingSave, true);
    assert.equal(quotaProfile.committedSaveCount, "0");
    assert.equal(quotaProfile.logicalSequence, "1");
    assert.equal((await profileState.getAttribute("data-save-state")), "retryable");
    assert.doesNotMatch(await profileState.innerText(), /已保存/);
    assert.match(await saveProfile.innerText(), /重试保存/);
    assert.equal(await saveProfile.isEnabled(), true);

    await saveProfile.click();
    await page.waitForFunction(
      () => {
        const profile = window.__tgdProfile?.getState();
        return profile?.stateName === "ready" && profile.committedSaveCount === "1";
      },
      undefined,
      { timeout: 15_000 }
    );
    const retriedProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(retriedProfile.hasPendingSave, false);
    assert.equal(retriedProfile.logicalSequence, "2");
    await waitForText(page, profileState, "已保存");

    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await waitForText(page, state, "presentation: running");
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    const retryRestoredProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(retryRestoredProfile.logicalSequence, "2");
    assert.equal(retryRestoredProfile.snapshotId, retriedProfile.snapshotId);
    await waitForText(page, profileState, "已恢复");

    await page.goto(url, { waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    assert.equal((await page.evaluate(() => window.__tgdProfile.getState())).logicalSequence, "2");

    const contenderProblems = [];
    const contender = await page.context().newPage();
    contender.on("pageerror", (error) => contenderProblems.push(error.message));
    contender.on("console", (message) => {
      if (message.type() === "error") contenderProblems.push(message.text());
    });
    const contenderResponse = await contender.goto(url, {
      waitUntil: "domcontentloaded",
      timeout: 45_000
    });
    assert(contenderResponse?.ok(), `${target} failed to open the second Profile tab.`);
    await waitForText(contender, contender.getByTestId("profile-state"), "已恢复", 45_000);
    assert.equal(
      (await contender.evaluate(() => window.__tgdProfile.getState())).logicalSequence,
      "2"
    );

    await saveProfile.click();
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.committedSaveCount === "1",
      undefined,
      { timeout: 15_000 }
    );
    const winningProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(winningProfile.logicalSequence, "3");

    await contender.getByTestId("save-profile").click();
    await contender.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "conflict_read_only",
      undefined,
      { timeout: 15_000 }
    );
    const losingProfile = await contender.evaluate(() => window.__tgdProfile.getState());
    assert.equal(losingProfile.errorName, "storage_conflict");
    assert.equal(losingProfile.committedSaveCount, "0");
    assert.equal(losingProfile.logicalSequence, "2");
    assert.equal(await contender.getByTestId("save-profile").isDisabled(), true);
    assert.doesNotMatch(await contender.getByTestId("profile-state").innerText(), /已保存/);
    assert.deepEqual(contenderProblems, [], `${target} second tab emitted browser errors.`);
    await contender.close();

    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    const conflictRestoredProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(conflictRestoredProfile.logicalSequence, "3");
    assert.equal(conflictRestoredProfile.snapshotId, winningProfile.snapshotId);

    await page.waitForFunction(
      () => window.__tgdServiceWorker?.ready === true,
      undefined,
      { timeout: 45_000 }
    );
    let offlineSavedProfile;
    await page.context().setOffline(true);
    try {
      const offlineResponse = await page.reload({
        waitUntil: "domcontentloaded",
        timeout: 45_000
      });
      assert(offlineResponse?.ok(), `${target} offline shell reload did not return a cached response.`);
      assert.equal(
        offlineResponse.fromServiceWorker(),
        true,
        `${target} offline shell reload bypassed the Service Worker.`
      );
      await waitForText(page, status, "宿主已就绪", 45_000);
      await page.waitForFunction(
        () => window.__tgdProfile?.getState()?.stateName === "ready",
        undefined,
        { timeout: 45_000 }
      );
      assert.equal(
        (await page.evaluate(() => window.__tgdProfile.getState())).logicalSequence,
        "3"
      );
      await saveProfile.click();
      await page.waitForFunction(
        () => window.__tgdProfile?.getState()?.committedSaveCount === "1",
        undefined,
        { timeout: 15_000 }
      );
      offlineSavedProfile = await page.evaluate(() => window.__tgdProfile.getState());
      assert.equal(offlineSavedProfile.logicalSequence, "4");
      assert.equal(offlineSavedProfile.hasPendingSave, false);
    } finally {
      await page.context().setOffline(false);
    }

    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    const onlineRestoredProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(onlineRestoredProfile.logicalSequence, "4");
    assert.equal(onlineRestoredProfile.snapshotId, offlineSavedProfile.snapshotId);

    await page.evaluate(async () => {
      const profile = window.__tgdProfile.getState();
      const profileId = window.__tgdProfile.identity.profileId;
      const database = await new Promise((resolveOpen, rejectOpen) => {
        const request = indexedDB.open("tiangongdu.prototype_f1.internal.profile", 1);
        request.onsuccess = () => resolveOpen(request.result);
        request.onerror = () => rejectOpen(request.error);
      });
      const transaction = database.transaction("snapshots", "readwrite");
      const store = transaction.objectStore("snapshots");
      const record = await new Promise((resolveRecord, rejectRecord) => {
        const request = store.get([profileId, profile.snapshotId]);
        request.onsuccess = () => resolveRecord(request.result);
        request.onerror = () => rejectRecord(request.error);
      });
      const corrupted = new Uint8Array(record.bytes);
      corrupted[0] ^= 0xff;
      record.bytes = corrupted.buffer;
      store.put(record);
      await new Promise((resolveTransaction, rejectTransaction) => {
        transaction.oncomplete = resolveTransaction;
        transaction.onabort = () => rejectTransaction(transaction.error);
        transaction.onerror = () => rejectTransaction(transaction.error);
      });
      database.close();
    });
    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await page.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "recovery_required",
      undefined,
      { timeout: 45_000 }
    );
    const corruptProfile = await page.evaluate(() => window.__tgdProfile.getState());
    assert.equal(corruptProfile.errorName, "storage_corrupt");
    assert.equal(corruptProfile.committedSaveCount, "0");
    assert.equal(await saveProfile.isDisabled(), true);
    assert.doesNotMatch(await profileState.innerText(), /已保存/);
    assert.equal(await page.getByTestId("export-profile").isEnabled(), true);
    const recoveryExport = await page.evaluate(async () => {
      const exported = await window.__tgdProfile.export();
      return {
        expectedProfileId: window.__tgdProfile.identity.profileId,
        fileName: exported.fileName,
        mediaType: exported.mediaType,
        profileId: exported.profileId,
        snapshotId: exported.snapshotId,
        logicalSequence: exported.logicalSequence,
        envelopeHash: exported.envelopeHash,
        byteLength: exported.bytes.byteLength,
        firstByte: exported.bytes[0]
      };
    });
    assert.match(recoveryExport.fileName, /\.tgdprofile$/);
    assert.equal(recoveryExport.mediaType, "application/vnd.tiangongdu.profile+octet-stream");
    assert.equal(recoveryExport.profileId, recoveryExport.expectedProfileId);
    assert.equal(recoveryExport.snapshotId, offlineSavedProfile.snapshotId);
    assert.equal(recoveryExport.logicalSequence, "4");
    assert.match(recoveryExport.envelopeHash, /^[0-9a-f]{64}$/);
    assert(recoveryExport.byteLength > 176);
    assert.notEqual(recoveryExport.firstByte, 0x54);

    await page.evaluate(() => {
      const canvasElement = document.querySelector("canvas");
      if (canvasElement) canvasElement.style.boxShadow = "none";
    });
    const webgl = await inspectWebgl(page, target);
    assert(webgl.available, `${target} did not create a WebGL2 context.`);
    assert.equal(webgl.contextLost, false, `${target} started with a lost WebGL2 context.`);
    assert(webgl.contextLossExtension, `${target} lacks WEBGL_lose_context.`);
    assert.match(webgl.version, /WebGL 2/i, `${target} did not expose WebGL 2.`);
    assert(webgl.renderer, `${target} did not report a WebGL renderer.`);

    const button = (testId) => page.getByTestId(testId);
    for (const testId of [
      "qa-hide",
      "qa-show",
      "qa-blur",
      "qa-focus",
      "qa-context-lost",
      "qa-context-restored"
    ]) {
      assert.equal(await button(testId).count(), 1, `${target} is missing ${testId}.`);
    }

    const beforePath = resolve(reportDirectory, `${target}-before.png`);
    const afterPath = resolve(reportDirectory, `${target}-after-context-restore.png`);
    const beforeFrame = await captureRenderedFrame(
      page,
      canvas,
      beforePath,
      `${target} initial canvas`
    );

    const clickAndExpect = async (testId, expected, expectedEvent) => {
      const previousEvent = (await lastEvent.innerText()).trim();
      await button(testId).click();
      const lastEventText = await waitForChangedText(
        page,
        lastEvent,
        previousEvent,
        expectedEvent
      );
      const stateText = await waitForText(
        page,
        state,
        `${testId} · presentation: ${expected}`
      );
      const checkpoint = {
        action: testId,
        presentation: expected,
        stateText,
        lastEvent: lastEventText
      };
      checkpoints.push(checkpoint);
      return checkpoint;
    };

    await clickAndExpect("qa-hide", "suspended", "document.hidden");
    await clickAndExpect("qa-show", "running", "document.visible");
    await clickAndExpect("qa-blur", "suspended", "window.blur");
    await clickAndExpect("qa-focus", "running", "window.focus");
    await clickAndExpect("qa-context-lost", "context_lost", "webgl.context_lost");
    await clickAndExpect("qa-hide", "context_lost", "document.hidden");
    await clickAndExpect("qa-context-restored", "suspended", "webgl.context_restored");
    await clickAndExpect("qa-show", "running", "document.visible");

    // Axmol recreates buffers/programs asynchronously after the browser event.
    await page.waitForTimeout(4_000);
    const afterFrame = await captureRenderedFrame(
      page,
      canvas,
      afterPath,
      `${target} restored canvas`
    );
    const frameComparison = compareFrames(beforeFrame, afterFrame);
    assert(
      frameComparison.changedPixelRatio <= 0.02,
      `${target} restored canvas differs from the initial probe by ${(
        frameComparison.changedPixelRatio * 100
      ).toFixed(2)}%.`
    );

    assert.deepEqual(pageErrors, [], `${target} emitted page errors.`);
    assert.deepEqual(requestFailures, [], `${target} had failed local requests.`);
    assert.deepEqual(consoleProblems, [], `${target} emitted console warnings/errors.`);
    const identity = lifecycle.find((record) => record.event === "runtime.initialize");
    assert(identity, `${target} did not emit the embedded build identity.`);
    if (expectedCommit) {
      assert.equal(identity.commit, expectedCommit, `${target} embedded the wrong Git commit.`);
    }

    const replayUrl = `${origin}/tgd-replay-probe.html?qa=1`;
    const replayResponse = await page.goto(replayUrl, {
      waitUntil: "domcontentloaded",
      timeout: 45_000
    });
    assert(
      replayResponse?.ok(),
      `${target} failed to load ${replayUrl}: HTTP ${replayResponse?.status()}`
    );
    const replayProbe = validateReplayProbe(
      await waitForRecord(page, replayProbes, `${target} WASM replay result`),
      target
    );
    assert.deepEqual(
      replayProbe.build,
      {
        version: identity.version,
        commit: identity.commit,
        channel: identity.channel
      },
      `${target} host and replay probe came from different builds.`
    );

    assert.deepEqual(pageErrors, [], `${target} emitted page errors.`);
    assert.deepEqual(requestFailures, [], `${target} had failed local requests.`);
    assert.deepEqual(consoleProblems, [], `${target} emitted console warnings/errors.`);

    return {
      target,
      status: "passed",
      browserVersion: browser.version(),
      graphicsMode: forceSoftwareGraphics ? "forced-software" : "browser-default",
      webgl,
      embeddedIdentity: {
        version: identity.version,
        commit: identity.commit,
        channel: identity.channel
      },
      replayProbe,
      checkpoints,
      consoleProblems,
      expectedConsoleDiagnostics,
      pageErrors,
      requestFailures,
      beforeFrame: publicFrame(beforeFrame),
      afterFrame: publicFrame(afterFrame),
      frameComparison,
      screenshots: [projectPath(beforePath), projectPath(afterPath)]
    };
  } finally {
    await browser?.close();
  }
}

await assertWebArtifacts();
await mkdir(reportDirectory, { recursive: true });
const server = await startStaticServer();
const startedAt = new Date().toISOString();
const results = [];

try {
  for (const target of targets) {
    const started = Date.now();
    try {
      const result = await runBrowser(target, server.origin);
      results.push({ ...result, durationMs: Date.now() - started });
      console.log(`[browser-qa] ${target}: passed (${result.browserVersion})`);
    } catch (error) {
      results.push({
        target,
        status: "failed",
        durationMs: Date.now() - started,
        error: error instanceof Error ? error.stack ?? error.message : String(error)
      });
      console.error(`[browser-qa] ${target}: failed`);
    }
  }
} finally {
  await server.close();
}

const report = {
  schemaVersion: "1.0.0",
  startedAt,
  finishedAt: new Date().toISOString(),
  platform: process.platform,
  architecture: process.arch,
  node: process.version,
  graphicsMode: forceSoftwareGraphics ? "forced-software" : "browser-default",
  distDirectory: projectPath(distDirectory),
  expectedCommit,
  targets,
  results
};
const reportPath = resolve(reportDirectory, "report.json");
await writeFile(reportPath, `${JSON.stringify(report, null, 2)}\n`, "utf8");
console.log(`[browser-qa] report: ${projectPath(reportPath)}`);

const failures = results.filter((result) => result.status !== "passed");
if (failures.length > 0) {
  throw new AggregateError(
    failures.map((failure) => new Error(`${failure.target}: ${failure.error}`)),
    `Browser lifecycle matrix failed for ${failures.map((failure) => failure.target).join(", ")}.`
  );
}
