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
  for (const fileName of ["tiangongdu-f1.html", "tiangongdu-f1.js", "tiangongdu-f1.wasm"]) {
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
  switch (target) {
    case "chrome":
      return { browserType: chromium, launchOptions: { channel: "chrome", headless: true } };
    case "edge":
      return { browserType: chromium, launchOptions: { channel: "msedge", headless: true } };
    case "chromium":
      return { browserType: chromium, launchOptions: { headless: true } };
    case "firefox":
      return { browserType: firefox, launchOptions: { headless: true } };
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

function parseLifecycle(message) {
  const prefix = "[tgd.lifecycle] ";
  if (!message.startsWith(prefix)) return null;
  try {
    return JSON.parse(message.slice(prefix.length));
  } catch {
    return null;
  }
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
  const checkpoints = [];
  let browser;

  try {
    browser = await definition.browserType.launch(definition.launchOptions);
    const page = await browser.newPage({ viewport: { width: 1280, height: 960 } });
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
    const lastEvent = page.locator("#last-event");
    const canvas = page.locator("canvas");
    await waitForText(page, status, "宿主已就绪", 45_000);
    await waitForText(page, state, "presentation: running");
    await canvas.waitFor({ state: "visible" });
    assert.equal(await canvas.count(), 1, "Exactly one game canvas is required.");

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
    const beforeBuffer = await canvas.screenshot({ path: beforePath });
    const beforeFrame = analyzePng(beforeBuffer);
    assert(beforeFrame.uniqueColors >= 4, `${target} initial canvas is visually blank.`);
    assert(
      beforeFrame.goldPixelRatio >= 0.005,
      `${target} initial canvas is missing the gold bridge probe.`
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
    const afterBuffer = await canvas.screenshot({ path: afterPath });
    const afterFrame = analyzePng(afterBuffer);
    const frameComparison = compareFrames(beforeFrame, afterFrame);
    assert(afterFrame.uniqueColors >= 4, `${target} restored canvas is visually blank.`);
    assert(
      afterFrame.goldPixelRatio >= 0.005,
      `${target} restored canvas is missing the gold bridge probe.`
    );
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

    return {
      target,
      status: "passed",
      browserVersion: browser.version(),
      embeddedIdentity: {
        version: identity.version,
        commit: identity.commit,
        channel: identity.channel
      },
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
