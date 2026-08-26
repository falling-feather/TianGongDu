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
    "build/evidence/demo-085-system-demo-web"
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

const fightToTerminal = async (page) => {
  await page.keyboard.down("w");
  try {
    for (let attempt = 0; attempt < 120; attempt += 1) {
      await page.keyboard.press(attempt % 3 === 0 ? "k" : "j");
      await page.waitForTimeout(300);
      const current = await state(page);
      assert.equal(
        current.playerHealth > 0,
        true,
        `player died during completion route at attempt ${attempt}`
      );
      if (current.terminalCompleted) return current;
    }
  } finally {
    await page.keyboard.up("w");
  }
  assert.fail("two-wave terminal objective did not complete within the combat budget");
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
  assert.equal(
    initial.packageByteCount,
    4_832,
    "canonical workshop package byte count drifted"
  );
  assert.equal(initial.assetCount, 12, "resolved Stable Asset count drifted");
  assert.equal(initial.playerX, 0, "authored player spawn X drifted");
  assert.equal(initial.playerY, -3250, "authored player spawn Y drifted");
  assert.equal(initial.gateOpen, false, "gate must start closed");
  assert.equal(initial.retryCount, 0);
  assert.equal(initial.playerHealth, 160);
  assert.equal(initial.activeHostileCount, 0);
  assert.equal(initial.completedWaveCount, 0);
  assert.equal(initial.completedObjectiveCount, 0);
  assert.equal(initial.terminalCompleted, false);
  assert.equal(initial.craftMode, false);
  assert.equal(initial.craftInRange, false);
  assert.equal(initial.craftStage, 1, "craft must await a material at boot");
  assert.equal(initial.craftSelectedMaterial, 0);
  assert.equal(initial.craftCompletedOperations, 0);
  assert.equal(initial.craftTrialCount, 0);
  assert.equal(initial.craftMistakeCount, 0);
  assert.equal(initial.craftReworkCount, 0);
  assert.equal(initial.craftCompleted, false);
  assert.equal(initial.workshopFunds, 100);
  assert.equal(initial.workshopSpentFunds, 0);
  assert.equal(initial.workshopStockFlexible, 1);
  assert.equal(initial.workshopStockSalvage, 2);
  assert.equal(initial.workshopQuality, 0);
  assert.equal(initial.workshopOccupied, false);
  assert.equal(initial.orderFulfilled, false);
  assert.equal(initial.shortcutOpen, false);
  await page.screenshot({
    path: resolve(evidenceDirectory, "01-initial-craft-bench-and-closed-gate.png"),
    fullPage: true
  });

  await hold(page, "d", 360);
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().craftInRange === true,
    undefined,
    { timeout: 5_000 }
  );
  await page.keyboard.press("c");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().craftMode === true,
    undefined,
    { timeout: 5_000 }
  );
  const craftOpened = await state(page);
  assert.equal(craftOpened.craftStage, 1);
  assert.equal(craftOpened.craftSelectedMaterial, 0);
  await page.screenshot({
    path: resolve(evidenceDirectory, "02-craft-need-and-material-choice.png"),
    fullPage: true
  });

  await page.keyboard.press("2");
  await page.keyboard.press("4");
  const wrongOrder = await state(page);
  assert.equal(wrongOrder.craftSelectedMaterial, 2);
  assert.equal(wrongOrder.craftStage, 2);
  assert.equal(wrongOrder.workshopFunds, 70);
  assert.equal(wrongOrder.workshopSpentFunds, 30);
  assert.equal(wrongOrder.workshopStockFlexible, 1);
  assert.equal(wrongOrder.workshopStockSalvage, 1);
  assert.equal(wrongOrder.workshopQuality, 6500);
  assert.equal(wrongOrder.workshopOccupied, true);
  assert.equal(
    wrongOrder.craftCompletedOperations,
    0,
    "out-of-order paste mutated craft progress"
  );

  await page.keyboard.press("3");
  await page.keyboard.press("4");
  const trialReady = await state(page);
  assert.equal(trialReady.craftCompletedOperations, 2);
  assert.equal(trialReady.craftStage, 3);

  await page.keyboard.press("t");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().craftStage === 4,
    undefined,
    { timeout: 5_000 }
  );
  const reworkRequired = await state(page);
  assert.equal(reworkRequired.craftTrialCount, 1);
  assert.equal(reworkRequired.craftMistakeCount, 1);
  assert.equal(reworkRequired.craftReworkCount, 0);
  assert.equal(reworkRequired.craftCompleted, false);
  await page.screenshot({
    path: resolve(evidenceDirectory, "03-rain-trial-rework-required.png"),
    fullPage: true
  });

  await page.keyboard.press("g");
  const reworked = await state(page);
  assert.equal(reworked.craftStage, 3);
  assert.equal(reworked.craftReworkCount, 1);
  assert.equal(reworked.workshopQuality, 8400);
  await page.keyboard.press("t");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().craftCompleted === true,
    undefined,
    { timeout: 5_000 }
  );
  const craftCompleted = await state(page);
  assert.equal(craftCompleted.craftStage, 5);
  assert.equal(craftCompleted.craftTrialCount, 2);
  assert.equal(craftCompleted.craftMistakeCount, 1);
  assert.equal(craftCompleted.craftReworkCount, 1);
  assert.equal(craftCompleted.workshopFunds, 70);
  assert.equal(craftCompleted.workshopQuality, 8400);
  assert.equal(craftCompleted.workshopOccupied, true);
  assert.equal(craftCompleted.orderFulfilled, false);
  assert.equal(craftCompleted.shortcutOpen, false);
  await page.screenshot({
    path: resolve(evidenceDirectory, "04-reworked-canopy-retrial-passed.png"),
    fullPage: true
  });

  await page.keyboard.press("y");
  await page.waitForFunction(
    () => {
      const current = window.__tgdSystemDemo.getState();
      return current.orderFulfilled === true && current.shortcutOpen === true;
    },
    undefined,
    { timeout: 5_000 }
  );
  const orderDelivered = await state(page);
  assert.equal(orderDelivered.craftMode, false);
  assert.equal(orderDelivered.workshopFunds, 95);
  assert.equal(orderDelivered.workshopSpentFunds, 30);
  assert.equal(orderDelivered.workshopStockFlexible, 1);
  assert.equal(orderDelivered.workshopStockSalvage, 1);
  assert.equal(orderDelivered.workshopQuality, 8400);
  assert.equal(orderDelivered.workshopOccupied, false);
  assert.equal(orderDelivered.orderFulfilled, true);
  assert.equal(orderDelivered.shortcutOpen, true);
  await page.screenshot({
    path: resolve(evidenceDirectory, "05-order-delivered-shortcut-open.png"),
    fullPage: true
  });

  await page.keyboard.press("c");
  await page.keyboard.press("y");
  const duplicateDelivery = await state(page);
  assert.equal(duplicateDelivery.workshopFunds, 95);
  assert.equal(duplicateDelivery.workshopStockSalvage, 1);
  assert.equal(duplicateDelivery.orderFulfilled, true);
  assert.equal(duplicateDelivery.shortcutOpen, true);
  await page.keyboard.press("c");

  await hold(page, "w", 1_450);
  const shortcutCrossed = await state(page);
  assert.equal(shortcutCrossed.gateOpen, false);
  assert.equal(shortcutCrossed.shortcutOpen, true);
  assert.ok(
    shortcutCrossed.playerY > 1_100,
    `player did not traverse the workshop shortcut: Y=${shortcutCrossed.playerY}`
  );
  await page.screenshot({
    path: resolve(evidenceDirectory, "06-shortcut-traversed-main-gate-closed.png"),
    fullPage: true
  });

  await hold(page, "s", 1_000);
  await hold(page, "a", 760);
  await hold(page, "w", 900);
  const blocked = await state(page);
  assert.equal(blocked.gateOpen, false);
  assert.equal(blocked.shortcutOpen, true);
  assert.ok(blocked.blockedMoveCount > 0, "closed main gate never blocked movement");
  assert.ok(
    blocked.playerY <= 400,
    `player crossed the still-closed main gate at Y=${blocked.playerY}`
  );

  await page.keyboard.press("f");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().gateOpen === true,
    undefined,
    { timeout: 5_000 }
  );
  const opened = await state(page);
  assert.equal(opened.gateOpen, true, "operate did not open the gate");
  assert.equal(opened.completedObjectiveCount, 1);
  assert.equal(opened.activeHostileCount, 2);
  assert.equal(opened.completedWaveCount, 0);

  await page.keyboard.press("f");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().repeatedTriggerCount === 1,
    undefined,
    { timeout: 5_000 }
  );
  const repeated = await state(page);
  assert.equal(repeated.activeHostileCount, 2, "repeat operate duplicated wave spawns");
  assert.equal(repeated.defeatedHostileCount, 0);
  await page.screenshot({
    path: resolve(evidenceDirectory, "07-wave-one-active-repeat-safe.png"),
    fullPage: true
  });

  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().playerHealth === 0,
    undefined,
    { timeout: 35_000 }
  );
  await page.waitForFunction(
    () => document.getElementById("telemetry")?.textContent?.includes("hp 0"),
    undefined,
    { timeout: 1_000 }
  );
  const defeated = await state(page);
  assert.equal(defeated.terminalCompleted, false);
  assert.equal(defeated.activeHostileCount > 0, true);
  await page.screenshot({
    path: resolve(evidenceDirectory, "08-player-defeated.png"),
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
  assert.equal(retried.playerHealth, 160);
  assert.equal(retried.activeHostileCount, 0);
  assert.equal(retried.defeatedHostileCount, 0);
  assert.equal(retried.completedWaveCount, 0);
  assert.equal(retried.completedObjectiveCount, 0);
  assert.equal(retried.terminalCompleted, false);
  assert.equal(retried.repeatedTriggerCount, 0);
  assert.equal(retried.craftStage, 1);
  assert.equal(retried.craftSelectedMaterial, 0);
  assert.equal(retried.craftCompletedOperations, 0);
  assert.equal(retried.craftTrialCount, 0);
  assert.equal(retried.craftMistakeCount, 0);
  assert.equal(retried.craftReworkCount, 0);
  assert.equal(retried.craftCompleted, false);
  assert.equal(retried.workshopFunds, 100);
  assert.equal(retried.workshopSpentFunds, 0);
  assert.equal(retried.workshopStockFlexible, 1);
  assert.equal(retried.workshopStockSalvage, 2);
  assert.equal(retried.workshopQuality, 0);
  assert.equal(retried.workshopOccupied, false);
  assert.equal(retried.orderFulfilled, false);
  assert.equal(retried.shortcutOpen, false);
  await page.screenshot({
    path: resolve(evidenceDirectory, "09-death-retry-restored-all-systems.png"),
    fullPage: true
  });

  await hold(page, "w", 1_050);
  await page.keyboard.press("f");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().activeHostileCount === 2,
    undefined,
    { timeout: 5_000 }
  );
  const terminal = await fightToTerminal(page);
  assert.equal(terminal.defeatedHostileCount, 4);
  assert.equal(terminal.activeHostileCount, 0);
  assert.equal(terminal.completedWaveCount, 2);
  assert.equal(terminal.completedObjectiveCount, 2);
  assert.equal(terminal.terminalCompleted, true);
  assert.ok(terminal.acceptedAttackCount > 0);
  assert.ok(terminal.playerY > 1100, `player did not cross opened gate: Y=${terminal.playerY}`);
  await page.screenshot({
    path: resolve(evidenceDirectory, "10-two-waves-terminal-complete.png"),
    fullPage: true
  });

  await page.keyboard.press("r");
  await page.waitForFunction(
    () => window.__tgdSystemDemo.getState().retryCount === 2,
    undefined,
    { timeout: 5_000 }
  );
  const terminalRetried = await state(page);
  assert.equal(terminalRetried.gateOpen, false);
  assert.equal(terminalRetried.playerHealth, 160);
  assert.equal(terminalRetried.completedWaveCount, 0);
  assert.equal(terminalRetried.completedObjectiveCount, 0);
  assert.equal(terminalRetried.terminalCompleted, false);
  assert.equal(terminalRetried.workshopFunds, 100);
  assert.equal(terminalRetried.orderFulfilled, false);
  assert.equal(terminalRetried.shortcutOpen, false);

  await page.waitForTimeout(500);
  const shellPageErrors = await page.evaluate(
    () => window.__tgdSystemDemo.getPageErrors()
  );
  pageErrors.push(...shellPageErrors);
  assert.deepEqual(consoleErrors, [], "browser console errors were emitted");
  assert.deepEqual(pageErrors, [], "browser page errors were emitted");
  assert.deepEqual(requestErrors, [], "browser request errors were emitted");

  routeEvidence = {
    taskId: "DEMO-085",
    productVersion: "Demo 0.8.5",
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
    craftOpened,
    wrongOrder,
    trialReady,
    reworkRequired,
    reworked,
    craftCompleted,
    orderDelivered,
    duplicateDelivery,
    shortcutCrossed,
    blocked,
    opened,
    repeated,
    defeated,
    retried,
    terminal,
    terminalRetried,
    consoleErrors,
    pageErrors,
    requestErrors,
    screenshots: [
      "01-initial-craft-bench-and-closed-gate.png",
      "02-craft-need-and-material-choice.png",
      "03-rain-trial-rework-required.png",
      "04-reworked-canopy-retrial-passed.png",
      "05-order-delivered-shortcut-open.png",
      "06-shortcut-traversed-main-gate-closed.png",
      "07-wave-one-active-repeat-safe.png",
      "08-player-defeated.png",
      "09-death-retry-restored-all-systems.png",
      "10-two-waves-terminal-complete.png"
    ]
  };
  await writeFile(
    resolve(evidenceDirectory, "evidence.json"),
    `${JSON.stringify(routeEvidence, null, 2)}\n`,
    "utf8"
  );
  process.stdout.write(
    `[demo-085] real Web workshop + alternate route + combat passed ${JSON.stringify({
      browser: routeEvidence.browser,
      craft: {
        wrongOrderPreservedOperations:
          wrongOrder.craftCompletedOperations,
        trials: craftCompleted.craftTrialCount,
        mistakes: craftCompleted.craftMistakeCount,
        reworks: craftCompleted.craftReworkCount,
        completed: craftCompleted.craftCompleted
      },
      workshop: {
        funds: orderDelivered.workshopFunds,
        spent: orderDelivered.workshopSpentFunds,
        remainingStocks: [
          orderDelivered.workshopStockFlexible,
          orderDelivered.workshopStockSalvage
        ],
        quality: orderDelivered.workshopQuality,
        orderFulfilled: orderDelivered.orderFulfilled,
        shortcutOpen: orderDelivered.shortcutOpen,
        shortcutCrossedY: shortcutCrossed.playerY
      },
      blockedMoveCount: blocked.blockedMoveCount,
      repeatedTriggerCount: repeated.repeatedTriggerCount,
      terminal: {
        waves: terminal.completedWaveCount,
        objectives: terminal.completedObjectiveCount,
        defeatedHostiles: terminal.defeatedHostileCount,
        acceptedAttacks: terminal.acceptedAttackCount
      },
      deathRetryPose: [retried.playerX, retried.playerY],
      evidenceDirectory
    })}\n`
  );
} finally {
  await context.close();
  await launched.browser.close();
  await server.close();
}
