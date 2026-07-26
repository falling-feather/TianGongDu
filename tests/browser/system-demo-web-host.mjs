import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { createServer } from "node:http";
import { mkdir, readFile, stat, writeFile } from "node:fs/promises";
import { extname, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

import { chromium } from "playwright";

const repositoryRoot = fileURLToPath(new URL("../../", import.meta.url));
const buildDirectory = resolve(
  repositoryRoot,
  process.env.TGD_SYSTEM_DEMO_WEB_BUILD_DIRECTORY ?? "build/web-release-single"
);
const webRoot = resolve(buildDirectory, "dist/web");
const evidenceDirectory = resolve(
  repositoryRoot,
  process.env.TGD_SYSTEM_DEMO_EVIDENCE_DIRECTORY ??
    "build/evidence/demo-081-system-demo-web"
);

const requiredFiles = [
  "tiangongdu-system-demo.html",
  "tiangongdu-system-demo.js",
  "tiangongdu-system-demo.wasm",
  "system-demo.tgdsbx"
];

const mimeTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
  [".data", "application/octet-stream"],
  [".tgdsbx", "application/octet-stream"],
  [".json", "application/json; charset=utf-8"],
  [".png", "image/png"]
]);

const ensureArtifacts = async () => {
  for (const relative of requiredFiles) {
    const file = resolve(webRoot, relative);
    const info = await stat(file);
    assert.equal(info.isFile(), true, `missing system Demo Web artifact: ${relative}`);
    assert.ok(info.size > 0, `empty system Demo Web artifact: ${relative}`);
  }
};

const startServer = async () => {
  const requestErrors = [];
  const server = createServer(async (request, response) => {
    try {
      const url = new URL(request.url ?? "/", "http://127.0.0.1");
      const pathname =
        url.pathname === "/" ? "/tiangongdu-system-demo.html" : url.pathname;
      const decoded = decodeURIComponent(pathname);
      const file = resolve(webRoot, `.${decoded}`);
      const allowedPrefix = `${webRoot}${sep}`;
      if (file !== webRoot && !file.startsWith(allowedPrefix)) {
        requestErrors.push(`blocked path escape: ${pathname}`);
        response.writeHead(403).end("forbidden");
        return;
      }
      const bytes = await readFile(file);
      response.writeHead(200, {
        "Cache-Control": "no-store",
        "Content-Type": mimeTypes.get(extname(file)) ?? "application/octet-stream",
        "Cross-Origin-Resource-Policy": "same-origin"
      });
      response.end(bytes);
    } catch (error) {
      requestErrors.push(`${request.url ?? "/"}: ${String(error)}`);
      response.writeHead(404).end("not found");
    }
  });
  await new Promise((resolveReady, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolveReady);
  });
  const address = server.address();
  assert.ok(address && typeof address === "object");
  return {
    origin: `http://127.0.0.1:${address.port}`,
    requestErrors,
    close: () => new Promise((resolveClosed, reject) => {
      server.close((error) => error ? reject(error) : resolveClosed());
    })
  };
};

const launchBrowser = async () => {
  const requested = process.env.TGD_SYSTEM_DEMO_BROWSER ?? "edge";
  if (requested === "edge") {
    try {
      return {
        name: "edge",
        browser: await chromium.launch({ channel: "msedge", headless: true })
      };
    } catch (edgeError) {
      process.stdout.write(
        `[demo-081] Edge channel unavailable; using locked Playwright Chromium: ${String(edgeError)}\n`
      );
    }
  }
  return {
    name: "chromium",
    browser: await chromium.launch({ headless: true })
  };
};

const state = (page) => page.evaluate(() => window.__tgdSystemDemo.getState());

const hold = async (page, key, milliseconds) => {
  await page.keyboard.down(key);
  await page.waitForTimeout(milliseconds);
  await page.keyboard.up(key);
  await page.waitForTimeout(120);
};

await ensureArtifacts();
await mkdir(evidenceDirectory, { recursive: true });

const server = await startServer();
const launched = await launchBrowser();
const context = await launched.browser.newContext({
  viewport: { width: 1440, height: 900 },
  deviceScaleFactor: 1
});
const page = await context.newPage();
const consoleErrors = [];
const pageErrors = [];
const requestErrors = server.requestErrors;

page.on("console", (message) => {
  if (message.type() === "error") {
    consoleErrors.push(message.text());
  }
});
page.on("pageerror", (error) => pageErrors.push(String(error)));
page.on("requestfailed", (request) => {
  requestErrors.push(
    `${request.method()} ${request.url()}: ${request.failure()?.errorText ?? "failed"}`
  );
});
page.on("response", (response) => {
  if (response.status() >= 400) {
    requestErrors.push(`${response.status()} ${response.url()}`);
  }
});

let routeEvidence;
try {
  await page.goto(`${server.origin}/tiangongdu-system-demo.html?qa=1`, {
    waitUntil: "domcontentloaded",
    timeout: 60_000
  });
  await page.waitForFunction(
    () => window.__tgdSystemDemo?.getState().ready === true,
    undefined,
    { timeout: 60_000 }
  );
  await page.locator("#canvas").click();

  const initial = await state(page);
  assert.equal(initial.packageByteCount, 2712, "canonical package byte count drifted");
  assert.equal(initial.assetCount, 12, "resolved Stable Asset count drifted");
  assert.equal(initial.playerX, 0, "authored player spawn X drifted");
  assert.equal(initial.playerY, -3250, "authored player spawn Y drifted");
  assert.equal(initial.gateOpen, false, "gate must start closed");
  assert.equal(initial.retryCount, 0);
  await page.screenshot({
    path: resolve(evidenceDirectory, "01-initial-closed-gate.png"),
    fullPage: true
  });

  await hold(page, "w", 1_450);
  const blocked = await state(page);
  assert.equal(blocked.gateOpen, false);
  assert.ok(blocked.blockedMoveCount > 0, "closed gate never blocked movement");
  assert.ok(blocked.playerY <= 400, `player crossed closed gate at Y=${blocked.playerY}`);

  await hold(page, "a", 520);
  await page.keyboard.press("f");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().gateOpen === true,
    undefined,
    { timeout: 5_000 }
  );
  const opened = await state(page);
  assert.equal(opened.gateOpen, true, "operate did not open the gate");

  await hold(page, "w", 520);
  const crossed = await state(page);
  assert.ok(crossed.playerY > 1100, `player did not cross opened gate: Y=${crossed.playerY}`);
  await page.screenshot({
    path: resolve(evidenceDirectory, "02-opened-and-crossed.png"),
    fullPage: true
  });

  await page.keyboard.press("r");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().retryCount === 1,
    undefined,
    { timeout: 5_000 }
  );
  const retried = await state(page);
  assert.equal(retried.playerX, -1250, "retry did not restore authored safe point X");
  assert.equal(retried.playerY, -3000, "retry did not restore authored safe point Y");
  assert.equal(retried.gateOpen, false, "retry did not restore the closed gate");
  assert.equal(retried.retryCount, 1);
  await page.screenshot({
    path: resolve(evidenceDirectory, "03-retried-closed-gate.png"),
    fullPage: true
  });

  await page.waitForTimeout(500);
  const shellPageErrors = await page.evaluate(
    () => window.__tgdSystemDemo.getPageErrors()
  );
  pageErrors.push(...shellPageErrors);
  assert.deepEqual(consoleErrors, [], "browser console errors were emitted");
  assert.deepEqual(pageErrors, [], "browser page errors were emitted");
  assert.deepEqual(requestErrors, [], "browser request errors were emitted");

  routeEvidence = {
    taskId: "DEMO-081",
    productVersion: "Demo 0.8.1",
    commit: execFileSync("git", ["rev-parse", "HEAD"], {
      cwd: repositoryRoot,
      encoding: "utf8"
    }).trim(),
    browser: {
      requested: process.env.TGD_SYSTEM_DEMO_BROWSER ?? "edge",
      used: launched.name,
      version: launched.browser.version()
    },
    url: `${server.origin}/tiangongdu-system-demo.html?qa=1`,
    initial,
    blocked,
    opened,
    crossed,
    retried,
    consoleErrors,
    pageErrors,
    requestErrors,
    screenshots: [
      "01-initial-closed-gate.png",
      "02-opened-and-crossed.png",
      "03-retried-closed-gate.png"
    ]
  };
  await writeFile(
    resolve(evidenceDirectory, "evidence.json"),
    `${JSON.stringify(routeEvidence, null, 2)}\n`,
    "utf8"
  );
  process.stdout.write(
    `[demo-081] real Web route passed ${JSON.stringify({
      browser: routeEvidence.browser,
      blockedMoveCount: blocked.blockedMoveCount,
      crossedY: crossed.playerY,
      retryPose: [retried.playerX, retried.playerY],
      evidenceDirectory
    })}\n`
  );
} finally {
  await context.close();
  await launched.browser.close();
  await server.close();
}
