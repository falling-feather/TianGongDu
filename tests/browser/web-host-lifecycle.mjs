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

async function holdMovementKeys(page, keys, durationMs) {
  for (const key of keys) await page.keyboard.down(key);
  await page.waitForTimeout(durationMs);
  for (const key of [...keys].reverse()) await page.keyboard.up(key);
  await page.waitForTimeout(25);
}

async function moveF1PlayerTo(page, targetX, targetY, tolerance = 180) {
  const deadline = Date.now() + 10_000;
  while (Date.now() < deadline) {
    const state = await page.evaluate(() => window.__tgdTest.getF1State());
    const deltaX = targetX - state.playerPoseX;
    const deltaY = targetY - state.playerPoseY;
    if (Math.abs(deltaX) <= tolerance && Math.abs(deltaY) <= tolerance) return state;

    const moveX = Math.abs(deltaX) > tolerance;
    const delta = moveX ? deltaX : deltaY;
    const keys = moveX
      ? delta > 0 ? ["w", "d"] : ["s", "a"]
      : delta > 0 ? ["w", "a"] : ["s", "d"];
    const durationMs = Math.min(
      120,
      Math.max(35, Math.floor(Math.abs(delta) / 3_600 * 750))
    );
    await holdMovementKeys(page, keys, durationMs);
  }
  const finalState = await page.evaluate(() => window.__tgdTest.getF1State());
  throw new Error(
    `F1 player did not reach (${targetX}, ${targetY}): ${JSON.stringify(finalState)}`
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
  let contextLossProbeActive = false;
  let pendingEmptyContextDiagnostics = 0;

  try {
    browser = await definition.browserType.launch(definition.launchOptions);
    context = await browser.newContext({ viewport: { width: 1280, height: 960 } });
    const page = await context.newPage();
    page.on("console", (message) => {
      const text = message.text();
      const type = message.type();
      if (type === "warning" || type === "error") {
        const diagnostic = { type, text };
        const expectedContextError =
          contextLossProbeActive &&
          type === "error" &&
          /OpenGL error 0x(?:9242|0500).*CommandBufferGL\.cpp beginRenderPass/.test(text);
        const expectedEmptyContextError =
          contextLossProbeActive &&
          type === "error" &&
          text === "" &&
          pendingEmptyContextDiagnostics > 0;
        if (expectedContextError) pendingEmptyContextDiagnostics += 1;
        if (expectedEmptyContextError) pendingEmptyContextDiagnostics -= 1;
        if (
          isExpectedConsoleDiagnostic(target, type, text) ||
          expectedContextError ||
          expectedEmptyContextError
        ) {
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
    const quotaPage = await context.newPage();
    quotaPage.on("console", (message) => {
      if (message.type() === "warning" || message.type() === "error") {
        consoleProblems.push({
          page: "quota",
          type: message.type(),
          text: message.text()
        });
      }
    });
    quotaPage.on("pageerror", (error) => pageErrors.push(`quota: ${error.message}`));
    quotaPage.on("requestfailed", (request) => {
      if (request.url().startsWith(origin)) {
        requestFailures.push({
          page: "quota",
          url: request.url(),
          error: request.failure()?.errorText ?? "unknown"
        });
      }
    });
    const quotaResponse = await quotaPage.goto(quotaUrl, {
      waitUntil: "domcontentloaded",
      timeout: 45_000
    });
    assert(quotaResponse?.ok(), `${target} failed to load quota fixture.`);
    const quotaStatus = quotaPage.locator("#status");
    const quotaState = quotaPage.getByTestId("qa-state");
    const quotaProfileState = quotaPage.getByTestId("profile-state");
    const quotaSaveProfile = quotaPage.getByTestId("save-profile");
    await waitForText(quotaPage, quotaStatus, "宿主已就绪", 45_000);
    await waitForText(quotaPage, quotaState, "presentation: running");
    await quotaPage.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    assert.equal(
      (await quotaPage.evaluate(() => window.__tgdProfile.getState())).logicalSequence,
      "1"
    );
    await quotaSaveProfile.click();
    await quotaPage.waitForFunction(
      () => window.__tgdProfile?.getState()?.errorName === "storage_quota",
      undefined,
      { timeout: 15_000 }
    );
    const quotaProfile = await quotaPage.evaluate(() => window.__tgdProfile.getState());
    assert.equal(quotaProfile.stateName, "save_failed");
    assert.equal(quotaProfile.hasPendingSave, true);
    assert.equal(quotaProfile.committedSaveCount, "0");
    assert.equal(quotaProfile.logicalSequence, "1");
    assert.equal((await quotaProfileState.getAttribute("data-save-state")), "retryable");
    assert.doesNotMatch(await quotaProfileState.innerText(), /已保存/);
    assert.match(await quotaSaveProfile.innerText(), /重试保存/);
    assert.equal(await quotaSaveProfile.isEnabled(), true);

    await quotaSaveProfile.click();
    await quotaPage.waitForFunction(
      () => {
        const profile = window.__tgdProfile?.getState();
        return profile?.stateName === "ready" && profile.committedSaveCount === "1";
      },
      undefined,
      { timeout: 15_000 }
    );
    const retriedProfile = await quotaPage.evaluate(() => window.__tgdProfile.getState());
    assert.equal(retriedProfile.hasPendingSave, false);
    assert.equal(retriedProfile.logicalSequence, "2");
    await waitForText(quotaPage, quotaProfileState, "已保存");

    await quotaPage.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(quotaPage, quotaStatus, "宿主已就绪", 45_000);
    await waitForText(quotaPage, quotaState, "presentation: running");
    await quotaPage.waitForFunction(
      () => window.__tgdProfile?.getState()?.stateName === "ready",
      undefined,
      { timeout: 45_000 }
    );
    const retryRestoredProfile = await quotaPage.evaluate(() => window.__tgdProfile.getState());
    assert.equal(retryRestoredProfile.logicalSequence, "2");
    assert.equal(retryRestoredProfile.snapshotId, retriedProfile.snapshotId);
    await waitForText(quotaPage, quotaProfileState, "已恢复");
    await quotaPage.close();

    await page.bringToFront();
    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
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

    try {
      await page.waitForFunction(
        () => window.__tgdServiceWorker?.ready === true,
        undefined,
        { timeout: 45_000 }
      );
    } catch (error) {
      const serviceWorker = await page.evaluate(async () => ({
        state: window.__tgdServiceWorker ?? null,
        controller: navigator.serviceWorker.controller?.scriptURL ?? null,
        registration: (await navigator.serviceWorker.getRegistration())?.active?.scriptURL ?? null,
        webTrace: window.__tgdLifecycleTrace?.slice(-10) ?? []
      }));
      throw new Error(
        `${target} Service Worker did not become ready: ${JSON.stringify(serviceWorker)}`,
        { cause: error }
      );
    }
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

    await canvas.focus();
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questRequiredObjectives === 6,
      undefined,
      { timeout: 5_000 }
    );
    const openingQuestState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(openingQuestState.questBeatIndex, 0);
    assert.equal(openingQuestState.questCompletedObjectives, 0);
    assert.equal(openingQuestState.questRequiredObjectives, 6);
    assert.equal(openingQuestState.activeHostiles, 0);
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-quest-interaction-ready.png`),
      `${target} opening quest interaction`
    );
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questCompletedObjectives === 1,
      undefined,
      { timeout: 5_000 }
    );
    const stationaryFrame = analyzePng(await canvas.screenshot({ type: "png" }));
    await page.keyboard.down("w");
    await page.waitForTimeout(600);
    await page.keyboard.up("w");
    await page.waitForTimeout(150);
    const movedFrame = analyzePng(await canvas.screenshot({ type: "png" }));
    const movementComparison = compareFrames(stationaryFrame, movedFrame);
    assert(
      movementComparison.changedPixelRatio >= 0.001 &&
        movementComparison.changedPixelRatio <= 0.03,
      `${target} oblique movement changed ${(
        movementComparison.changedPixelRatio * 100
      ).toFixed(2)}% of the frame.`
    );
    const rainFerryReadiness = [
      { x: -11_700, y: -1_300, completed: 2 },
      { x: -11_400, y: -1_000, completed: 3 },
      { x: -11_100, y: -700, completed: 4 },
      { x: -10_800, y: -400, completed: 5 }
    ];
    for (const step of rainFerryReadiness) {
      await moveF1PlayerTo(page, step.x, step.y);
      await page.keyboard.press("f");
      await page.waitForFunction(
        (completed) =>
          window.__tgdTest?.getF1State()?.questCompletedObjectives === completed,
        step.completed,
        { timeout: 5_000 }
      );
    }
    await moveF1PlayerTo(page, -10_450, -100);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questBeatIndex === 1,
      undefined,
      { timeout: 5_000 }
    );
    const advancedQuestState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(advancedQuestState.questCompletedObjectives, 0);
    assert.equal(advancedQuestState.questRequiredObjectives, 6);
    assert.equal(advancedQuestState.activeHostiles, 1);
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-quest-stage-advanced.png`),
      `${target} quest stage advanced`
    );
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questCompletedObjectives === 1,
      undefined,
      { timeout: 5_000 }
    );

    await page.keyboard.down("w");
    await page.keyboard.down("d");
    await page.waitForTimeout(1_800);
    await page.keyboard.up("d");
    await page.keyboard.up("w");
    await page.waitForTimeout(150);
    const combatReadyPath = resolve(reportDirectory, `${target}-combat-ready.png`);
    const combatActionPath = resolve(reportDirectory, `${target}-combat-action.png`);
    const combatHitPath = resolve(reportDirectory, `${target}-combat-hit.png`);
    const combatFlowerLightPath = resolve(reportDirectory, `${target}-combat-flower-light.png`);
    const combatGuardPath = resolve(reportDirectory, `${target}-combat-guard.png`);
    const combatDefeatedPath = resolve(reportDirectory, `${target}-combat-defeated.png`);
    const combatRetriedPath = resolve(reportDirectory, `${target}-combat-retried.png`);
    const combatReadyFrame = await captureRenderedFrame(
      page,
      canvas,
      combatReadyPath,
      `${target} combat ready`
    );
    await page.keyboard.press("k");
    await page.waitForTimeout(50);
    const combatActionFrame = await captureRenderedFrame(
      page,
      canvas,
      combatActionPath,
      `${target} heavy attack startup`
    );
    const combatActionComparison = compareFrames(combatReadyFrame, combatActionFrame);
    assert(
      combatActionComparison.changedPixelRatio >= 0.0002 &&
        combatActionComparison.changedPixelRatio <= 0.04,
      `${target} heavy startup feedback changed ${(
        combatActionComparison.changedPixelRatio * 100
      ).toFixed(2)}% of the frame.`
    );
    await page.waitForTimeout(320);
    const combatHitFrame = await captureRenderedFrame(
      page,
      canvas,
      combatHitPath,
      `${target} heavy attack hit`
    );
    const combatHitComparison = compareFrames(combatReadyFrame, combatHitFrame);
    assert(
      combatHitComparison.changedPixelRatio >= 0.0002 &&
        combatHitComparison.changedPixelRatio <= 0.04,
      `${target} hit feedback changed ${(
        combatHitComparison.changedPixelRatio * 100
      ).toFixed(2)}% of the frame.`
    );
    assert.notEqual(
      combatActionFrame.sha256,
      combatHitFrame.sha256,
      `${target} startup and hit frames must be visually distinct.`
    );

    await page.waitForTimeout(450);
    await page.keyboard.down("Shift");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.questBeatIndex === 1 && state.questCompletedObjectives >= 3;
      },
      undefined,
      { timeout: 15_000 }
    );
    await captureRenderedFrame(
      page,
      canvas,
      combatGuardPath,
      `${target} guard completed and flower-turn rig released`
    );
    const flowerRigState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(flowerRigState.questCompletedObjectives, 3);
    assert.equal(flowerRigState.activeHostiles, 1);
    await page.keyboard.up("Shift");
    await page.waitForTimeout(120);
    await page.keyboard.press("2");
    await page.waitForTimeout(60);
    await page.keyboard.press("j");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.questBeatIndex === 1 && state.questCompletedObjectives >= 5;
      },
      undefined,
      { timeout: 5_000 }
    );
    let trainingState = await page.evaluate(() => window.__tgdTest.getF1State());
    const trainingDeadline = Date.now() + 20_000;
    while (trainingState.questBeatIndex === 1 && Date.now() < trainingDeadline) {
      assert.equal(trainingState.playerActive, true, `${target} player fell during training.`);
      if (
        trainingState.incomingAttackTicks > 0 &&
        trainingState.incomingAttackTicks <= 10 &&
        !trainingState.playerBusy
      ) {
        await page.keyboard.press("c");
      }
      await page.waitForTimeout(40);
      trainingState = await page.evaluate(() => window.__tgdTest.getF1State());
    }
    assert.equal(
      trainingState.questBeatIndex,
      2,
      `${target} flower-turn evade never completed the training beat.`
    );
    assert.equal(
      trainingState.activeHostiles,
      2,
      `${target} umbrella-lane doll wave did not replace the training rigs.`
    );
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-training-complete.png`),
      `${target} combat training complete`
    );
    await page.waitForTimeout(850);
    await page.keyboard.press("j");
    await page.waitForTimeout(50);
    await captureRenderedFrame(
      page,
      canvas,
      combatFlowerLightPath,
      `${target} flower-turn light attack`
    );
    await page.waitForTimeout(850);
    await page.keyboard.press("c");
    await page.waitForTimeout(120);
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.playerActive === false,
      undefined,
      { timeout: 45_000 }
    );
    const defeatedState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(defeatedState.playerHealth, 0, `${target} defeat did not reach zero health.`);
    assert(
      defeatedState.activeHostiles > 0,
      `${target} defeat did not come from an active encounter.`
    );
    const combatDefeatedFrame = await captureRenderedFrame(
      page,
      canvas,
      combatDefeatedPath,
      `${target} player defeated`
    );
    await page.keyboard.press("r");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.playerActive === true && state.retryCount === 1;
      },
      undefined,
      { timeout: 5_000 }
    );
    const retryState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(retryState.playerHealth, 120, `${target} retry did not restore player health.`);
    assert.equal(retryState.activeHostiles, 2, `${target} retry did not restore the active wave.`);
    assert.equal(retryState.safePointPoseX, -5_600);
    assert.equal(retryState.safePointPoseY, -1_200);
    assert.equal(retryState.playerPoseX, retryState.safePointPoseX);
    assert.equal(retryState.playerPoseY, retryState.safePointPoseY);
    assert(
      retryState.failureRetryTicks > defeatedState.failureRetryTicks,
      `${target} failed attempt was not moved into the excluded retry bucket.`
    );
    assert.equal(retryState.playableTargetMet, false);
    assert.equal(
      retryState.questBeatIndex,
      defeatedState.questBeatIndex,
      `${target} retry discarded quest stage progress.`
    );
    assert.equal(
      retryState.questCompletedObjectives,
      defeatedState.questCompletedObjectives,
      `${target} retry discarded completed quest objectives.`
    );
    assert.equal(retryState.questRequiredObjectives, defeatedState.questRequiredObjectives);
    const combatRetriedFrame = await captureRenderedFrame(
      page,
      canvas,
      combatRetriedPath,
      `${target} encounter retried`
    );
    const retryFrameComparison = compareFrames(combatDefeatedFrame, combatRetriedFrame);
    assert(
      retryFrameComparison.changedPixelRatio >= 0.002 &&
        retryFrameComparison.changedPixelRatio <= 0.08,
      `${target} retry feedback changed ${(
        retryFrameComparison.changedPixelRatio * 100
      ).toFixed(2)}% of the frame.`
    );

    await page.keyboard.down("w");
    await page.waitForTimeout(600);
    await page.keyboard.up("w");
    await page.keyboard.down("w");
    await page.keyboard.down("d");
    await page.waitForTimeout(1_800);
    await page.keyboard.up("d");
    await page.keyboard.up("w");
    await page.waitForTimeout(150);
    await page.keyboard.press("2");
    await page.waitForTimeout(100);
    let victoryState = await page.evaluate(() => window.__tgdTest.getF1State());
    let readyToAttack = false;
    let sawTelegraph = false;
    const victoryDeadline = Date.now() + 45_000;
    while (victoryState.activeHostiles > 0 && Date.now() < victoryDeadline) {
      assert.equal(
        victoryState.playerActive,
        true,
        `${target} player fell during telegraph-driven victory route: ${JSON.stringify(victoryState)}`
      );
      if (victoryState.incomingAttackTicks > 0) {
        sawTelegraph = true;
        if (victoryState.incomingAttackTicks <= 10 && !victoryState.playerBusy) {
          await page.keyboard.press("c");
          readyToAttack = true;
        }
      } else if (readyToAttack && !victoryState.playerBusy) {
        await page.keyboard.press("j");
        readyToAttack = false;
      }
      await page.waitForTimeout(40);
      victoryState = await page.evaluate(() => window.__tgdTest.getF1State());
    }
    assert.equal(sawTelegraph, true, `${target} never exposed a hostile telegraph.`);
    assert.equal(victoryState.activeHostiles, 0, `${target} did not clear the leaking-doll wave.`);
    assert.equal(victoryState.questBeatIndex, 2);
    assert.equal(victoryState.questCompletedObjectives, 1);
    assert.equal(victoryState.questRequiredObjectives, 6);

    const rainworksSteps = [
      { x: -3_600, y: -1_700, completed: 2 },
      { x: -2_700, y: -700, completed: 3 },
      { x: -1_800, y: 500, completed: 4 }
    ];
    for (const step of rainworksSteps) {
      await moveF1PlayerTo(page, step.x, step.y);
      await page.keyboard.press("f");
      await page.waitForFunction(
        (completed) => window.__tgdTest?.getF1State()?.questCompletedObjectives === completed,
        step.completed,
        { timeout: 5_000 }
      );
      victoryState = await page.evaluate(() => window.__tgdTest.getF1State());
      assert.equal(victoryState.questBeatIndex, 2);
      assert.equal(victoryState.questRequiredObjectives, 6);
      if (step.completed < 4) {
        assert.equal(victoryState.activeHostiles, 0);
      }
      if (step.completed === 3) {
        await captureRenderedFrame(
          page,
          canvas,
          resolve(reportDirectory, `${target}-umbrella-lane-rainworks.png`),
          `${target} umbrella-lane rainworks interlude`
        );
      }
    }
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.activeHostiles === 1,
      undefined,
      { timeout: 5_000 }
    );
    victoryState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(victoryState.questCompletedObjectives, 4);
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-umbrella-lane-paper-egret-wave.png`),
      `${target} umbrella-lane paper-egret wave`
    );

    await page.keyboard.press("2");
    await page.waitForTimeout(100);
    readyToAttack = false;
    let sawPaperEgretTelegraph = false;
    const paperEgretDeadline = Date.now() + 30_000;
    while (victoryState.activeHostiles > 0 && Date.now() < paperEgretDeadline) {
      assert.equal(
        victoryState.playerActive,
        true,
        `${target} player fell during the paper-egret wave: ${JSON.stringify(victoryState)}`
      );
      if (victoryState.incomingAttackTicks > 0) {
        sawPaperEgretTelegraph = true;
        if (victoryState.incomingAttackTicks <= 10 && !victoryState.playerBusy) {
          await page.keyboard.press("c");
          readyToAttack = true;
        } else if (readyToAttack && !victoryState.playerBusy) {
          await page.keyboard.press("j");
          readyToAttack = false;
        }
      } else if (sawPaperEgretTelegraph && !victoryState.playerBusy) {
        await page.keyboard.press("j");
        readyToAttack = false;
      }
      await page.waitForTimeout(40);
      victoryState = await page.evaluate(() => window.__tgdTest.getF1State());
    }
    assert.equal(
      sawPaperEgretTelegraph,
      true,
      `${target} paper-egret wave exposed no hostile telegraph.`
    );
    assert.equal(victoryState.activeHostiles, 0, `${target} did not clear the paper-egret wave.`);
    assert.equal(victoryState.questBeatIndex, 2);
    assert.equal(victoryState.questCompletedObjectives, 5);
    assert.equal(victoryState.questRequiredObjectives, 6);

    await moveF1PlayerTo(page, -3_900, -100);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questBeatIndex === 3,
      undefined,
      { timeout: 5_000 }
    );
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-umbrella-lane-complete.png`),
      `${target} umbrella-lane beat complete`
    );
    let workbenchState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(workbenchState.questCompletedObjectives, 0);
    assert.equal(workbenchState.questRequiredObjectives, 4);
    assert.equal(workbenchState.questSelectedChoices, 1);

    await moveF1PlayerTo(page, -3_900, -100);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questCompletedObjectives === 1,
      undefined,
      { timeout: 5_000 }
    );
    await moveF1PlayerTo(page, -3_100, -100);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questCompletedObjectives === 2,
      undefined,
      { timeout: 5_000 }
    );
    await moveF1PlayerTo(page, -2_300, -100);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questCompletedObjectives === 3,
      undefined,
      { timeout: 5_000 }
    );
    workbenchState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(workbenchState.questSelectedChoices, 1);

    await moveF1PlayerTo(page, -1_500, 400);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.questBeatIndex === 4 && state.questSelectedChoices === 2;
      },
      undefined,
      { timeout: 5_000 }
    );
    workbenchState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(workbenchState.questCompletedObjectives, 0);
    assert.equal(workbenchState.questRequiredObjectives, 3);
    assert.equal(workbenchState.activeHostiles, 3);
    assert.equal(workbenchState.playerHealth, 120);
    assert.equal(workbenchState.safePointPoseX, -4_300);
    assert.equal(workbenchState.safePointPoseY, -100);
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-return-formation-ready.png`),
      `${target} authored return formation ready`
    );
    await moveF1PlayerTo(page, -3_500, -900);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.questCompletedObjectives === 1 && state.activeHostiles === 4;
      },
      undefined,
      { timeout: 5_000 }
    );
    const reinforcementState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(reinforcementState.questBeatIndex, 4);
    assert.equal(reinforcementState.questRequiredObjectives, 3);
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-return-reinforcement-ready.png`),
      `${target} additive return reinforcement ready`
    );
    await page.keyboard.press("2");
    await page.waitForTimeout(100);

    let returnState = reinforcementState;
    let returnReadyToAttack = false;
    let returnSawTelegraph = false;
    let returnRetryAttempts = 0;
    let returnDeadline = Date.now() + 60_000;
    while (returnState.activeHostiles > 0 && Date.now() < returnDeadline) {
      if (!returnState.playerActive) {
        assert.equal(
          returnRetryAttempts,
          0,
          `${target} player fell twice during calibration return encounter: ${JSON.stringify(returnState)}`
        );
        const previousRetryCount = returnState.retryCount;
        await page.keyboard.press("r");
        await page.waitForFunction(
          (retryCount) => {
            const state = window.__tgdTest?.getF1State();
            return state?.playerActive === true && state.retryCount === retryCount + 1;
          },
          previousRetryCount,
          { timeout: 5_000 }
        );
        returnState = await page.evaluate(() => window.__tgdTest.getF1State());
        assert.equal(returnState.questBeatIndex, 4);
        assert.equal(returnState.questCompletedObjectives, 1);
        assert.equal(returnState.activeHostiles, 4);
        await page.keyboard.press("2");
        await page.waitForTimeout(100);
        returnReadyToAttack = false;
        returnRetryAttempts += 1;
        returnDeadline = Date.now() + 60_000;
        continue;
      }
      if (returnState.incomingAttackTicks > 0) {
        returnSawTelegraph = true;
        if (returnState.incomingAttackTicks <= 10 && !returnState.playerBusy) {
          await page.keyboard.press("c");
          returnReadyToAttack = true;
        }
      } else if (returnReadyToAttack && !returnState.playerBusy) {
        await page.keyboard.press("j");
        returnReadyToAttack = false;
      }
      await page.waitForTimeout(40);
      returnState = await page.evaluate(() => window.__tgdTest.getF1State());
    }
    assert.equal(returnSawTelegraph, true, `${target} return encounter exposed no telegraph.`);
    assert.equal(
      returnState.activeHostiles,
      0,
      `${target} did not clear the return encounter: ${JSON.stringify(returnState)}`
    );
    assert.equal(returnState.questBeatIndex, 4);
    assert.equal(returnState.questCompletedObjectives, 2);
    assert.equal(returnState.questRequiredObjectives, 3);
    assert.equal(returnState.questSelectedChoices, 2);

    await moveF1PlayerTo(page, -800, 400, 300);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => window.__tgdTest?.getF1State()?.questBeatIndex === 5,
      undefined,
      { timeout: 5_000 }
    );
    returnState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(returnState.questCompletedObjectives, 0);
    assert.equal(returnState.questRequiredObjectives, 4);
    assert.equal(returnState.activeHostiles, 1);
    assert.equal(returnState.safePointPoseX, 2_200);
    assert.equal(returnState.safePointPoseY, 800);
    const bossSpringPath = resolve(reportDirectory, `${target}-boss-spring-phase.png`);
    const bossWinterPath = resolve(reportDirectory, `${target}-boss-winter-phase.png`);
    const bossCompletePath = resolve(reportDirectory, `${target}-four-seasons-wraith-complete.png`);
    const resolutionChoicePath = resolve(reportDirectory, `${target}-resolution-choice.png`);
    const resolutionCompletePath = resolve(
      reportDirectory,
      `${target}-resolution-return-complete.png`
    );
    await captureRenderedFrame(
      page,
      canvas,
      resolve(reportDirectory, `${target}-canopy-return-complete.png`),
      `${target} canopy return beat complete`
    );
    await captureRenderedFrame(
      page,
      canvas,
      bossSpringPath,
      `${target} four-seasons wraith spring phase`
    );
    await page.keyboard.press("2");
    await page.waitForTimeout(100);

    let bossState = returnState;
    let bossReadyToAttack = false;
    let bossSawTelegraph = false;
    let bossWinterCaptured = false;
    let bossMaxCompletedObjectives = 0;
    const bossDeadline = Date.now() + 90_000;
    while (bossState.questBeatIndex === 5 && Date.now() < bossDeadline) {
      assert.equal(
        bossState.playerActive,
        true,
        `${target} player fell during the four-seasons wraith: ${JSON.stringify(bossState)}`
      );
      bossMaxCompletedObjectives = Math.max(
        bossMaxCompletedObjectives,
        bossState.questCompletedObjectives
      );
      if (bossState.questCompletedObjectives >= 3 && !bossWinterCaptured) {
        await captureRenderedFrame(
          page,
          canvas,
          bossWinterPath,
          `${target} four-seasons wraith winter phase`
        );
        bossWinterCaptured = true;
      }
      if (bossState.incomingAttackTicks > 0) {
        bossSawTelegraph = true;
        if (bossState.incomingAttackTicks <= 10 && !bossState.playerBusy) {
          await page.keyboard.press("c");
          bossReadyToAttack = true;
        }
      } else if (bossReadyToAttack && !bossState.playerBusy) {
        await page.keyboard.press("j");
        bossReadyToAttack = false;
      }
      await page.waitForTimeout(40);
      bossState = await page.evaluate(() => window.__tgdTest.getF1State());
    }
    assert.equal(bossSawTelegraph, true, `${target} boss exposed no readable telegraph.`);
    assert.equal(
      bossWinterCaptured,
      true,
      `${target} boss never exposed the authored winter phase.`
    );
    assert.equal(
      bossMaxCompletedObjectives,
      3,
      `${target} boss did not progress through spring, summer, and autumn in order.`
    );
    assert.equal(bossState.questBeatIndex, 6, `${target} boss did not unlock resolution.`);
    assert.equal(bossState.questCompletedObjectives, 0);
    assert.equal(bossState.questRequiredObjectives, 2);
    assert.equal(bossState.activeHostiles, 0);
    assert.equal(bossState.safePointPoseX, 3_000);
    assert.equal(bossState.safePointPoseY, 800);
    await captureRenderedFrame(
      page,
      canvas,
      bossCompletePath,
      `${target} four-seasons wraith complete`
    );
    await moveF1PlayerTo(page, 4_200, 2_300);
    await captureRenderedFrame(
      page,
      canvas,
      resolutionChoicePath,
      `${target} restore-shared-mark resolution choice`
    );
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.questBeatIndex === 6 && state.questCompletedObjectives === 1 &&
          state.questSelectedChoices === 3;
      },
      undefined,
      { timeout: 5_000 }
    );
    await moveF1PlayerTo(page, -10_500, -600);
    await page.keyboard.press("f");
    await page.waitForFunction(
      () => {
        const state = window.__tgdTest?.getF1State();
        return state?.questResolved === true && state.resolutionRewardReady === true;
      },
      undefined,
      { timeout: 5_000 }
    );
    const resolutionState = await page.evaluate(() => window.__tgdTest.getF1State());
    assert.equal(resolutionState.questBeatIndex, 6);
    assert.equal(resolutionState.questCompletedObjectives, 2);
    assert.equal(resolutionState.questRequiredObjectives, 2);
    assert.equal(resolutionState.questSelectedChoices, 3);
    assert.equal(resolutionState.activeHostiles, 0);
    assert.equal(resolutionState.questResolved, true);
    assert.equal(resolutionState.resolutionRewardReady, true);
    assert.equal(resolutionState.safePointPoseX, 3_000);
    assert.equal(resolutionState.safePointPoseY, 800);
    assert(resolutionState.eligiblePlayTicks > 0);
    assert(resolutionState.failureRetryTicks > 0);
    assert.equal(resolutionState.beatTargetsMet, 0);
    assert.equal(
      resolutionState.playableTargetMet,
      false,
      `${target} fast functional route must not masquerade as a one-hour playtest.`
    );
    await captureRenderedFrame(
      page,
      canvas,
      resolutionCompletePath,
      `${target} resolution returned to Shen Yan`
    );
    await page.reload({ waitUntil: "domcontentloaded", timeout: 45_000 });
    await waitForText(page, status, "宿主已就绪", 45_000);
    await canvas.focus();
    await page.waitForTimeout(150);

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
    contextLossProbeActive = true;
    await clickAndExpect("qa-context-lost", "context_lost", "webgl.context_lost");
    await clickAndExpect("qa-hide", "context_lost", "document.hidden");
    await clickAndExpect("qa-context-restored", "suspended", "webgl.context_restored");
    await clickAndExpect("qa-show", "running", "document.visible");

    // Axmol recreates buffers/programs asynchronously after the browser event.
    await page.waitForTimeout(4_000);
    contextLossProbeActive = false;
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
      movementComparison,
      combatActionComparison,
      combatHitComparison,
      retryState,
      workbenchState,
      reinforcementState,
      returnState,
      returnRetryAttempts,
      bossState,
      resolutionState,
      bossMaxCompletedObjectives,
      retryFrameComparison,
      frameComparison,
      screenshots: [
        projectPath(resolve(reportDirectory, `${target}-quest-interaction-ready.png`)),
        projectPath(resolve(reportDirectory, `${target}-quest-stage-advanced.png`)),
        projectPath(resolve(reportDirectory, `${target}-training-complete.png`)),
        projectPath(resolve(reportDirectory, `${target}-umbrella-lane-rainworks.png`)),
        projectPath(
          resolve(reportDirectory, `${target}-umbrella-lane-paper-egret-wave.png`)
        ),
        projectPath(resolve(reportDirectory, `${target}-umbrella-lane-complete.png`)),
        projectPath(
          resolve(reportDirectory, `${target}-return-formation-ready.png`)
        ),
        projectPath(
          resolve(reportDirectory, `${target}-return-reinforcement-ready.png`)
        ),
        projectPath(resolve(reportDirectory, `${target}-canopy-return-complete.png`)),
        projectPath(bossSpringPath),
        projectPath(bossWinterPath),
        projectPath(bossCompletePath),
        projectPath(resolutionChoicePath),
        projectPath(resolutionCompletePath),
        projectPath(combatReadyPath),
        projectPath(combatActionPath),
        projectPath(combatHitPath),
        projectPath(combatFlowerLightPath),
        projectPath(combatGuardPath),
        projectPath(combatDefeatedPath),
        projectPath(combatRetriedPath),
        projectPath(beforePath),
        projectPath(afterPath)
      ]
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
