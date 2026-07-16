import assert from "node:assert/strict";
import { mkdir, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

import { chromium } from "playwright";

const origin = process.env.TGD_SAMPLE_ORIGIN ?? "http://127.0.0.1:4178";
const root = resolve(import.meta.dirname, "..");
const evidence = resolve(root, "evidence");
await mkdir(evidence, { recursive: true });

const browser = await chromium.launch({ channel: "chrome", headless: true });
const context = await browser.newContext({ viewport: { width: 1672, height: 1050 } });
const page = await context.newPage();
const consoleProblems = [];
const pageErrors = [];

page.on("console", (message) => {
  if (["warning", "error"].includes(message.type())) {
    consoleProblems.push({ type: message.type(), text: message.text() });
  }
});
page.on("pageerror", (error) => pageErrors.push(error.message));

function state() {
  return page.evaluate(() => ({ ...document.body.dataset }));
}

const report = {
  status: "failed",
  browserVersion: "",
  states: {},
  screenshots: [],
  consoleProblems,
  pageErrors,
  responsive: null
};

try {
  await page.goto(origin, { waitUntil: "domcontentloaded", timeout: 30_000 });
  await page.waitForFunction(() => document.body.dataset.round === "waiting", undefined, {
    timeout: 15_000
  });
  const frame = page.locator(".game-frame");
  await frame.screenshot({ path: resolve(evidence, "01-start-screen.png") });
  report.screenshots.push("evidence/01-start-screen.png");

  await page.getByRole("button", { name: "开始演示" }).click();
  await page.waitForFunction(() => document.body.dataset.round === "playing");
  report.states.initial = await state();
  assert.equal(report.states.initial.playerHealth, "100");
  assert.equal(report.states.initial.enemyHealth, "100");

  for (let index = 0; index < 21; index += 1) await page.keyboard.press("d");
  await page.waitForFunction(() => Number(document.body.dataset.playerX) >= 690);
  report.states.afterMovement = await state();
  assert(Number(report.states.afterMovement.playerX) > Number(report.states.initial.playerX));
  await frame.screenshot({ path: resolve(evidence, "02-active-combat.png") });
  report.screenshots.push("evidence/02-active-combat.png");

  await page.waitForFunction(
    () => document.body.dataset.enemyTelegraphing === "true",
    undefined,
    { timeout: 10_000 }
  );
  report.states.telegraph = await state();
  await page.keyboard.press("Space");
  report.states.afterDodgeInput = await state();
  assert.notEqual(
    report.states.afterDodgeInput.lastAction,
    report.states.telegraph.lastAction,
    "Space did not submit the dodge action"
  );

  await page.waitForFunction(
    () => document.body.dataset.enemyTelegraphing === "false",
    undefined,
    { timeout: 3_000 }
  );
  report.states.afterDodge = await state();
  assert.equal(report.states.afterDodge.playerHealth, "100");
  await frame.screenshot({ path: resolve(evidence, "03-dodge-success.png") });
  report.screenshots.push("evidence/03-dodge-success.png");

  await page.keyboard.press("k");
  await page.waitForFunction(() => Number(document.body.dataset.enemyHealth) <= 64);
  await page.waitForTimeout(760);
  await page.keyboard.press("k");
  await page.waitForFunction(() => Number(document.body.dataset.enemyHealth) <= 28);
  await page.waitForTimeout(760);
  await page.keyboard.press("q");
  await page.waitForFunction(() => document.body.dataset.stance === "flower");
  await page.keyboard.press("j");
  await page.waitForFunction(() => Number(document.body.dataset.enemyHealth) <= 10);
  await page.waitForTimeout(360);
  await page.keyboard.press("j");
  await page.waitForFunction(() => document.body.dataset.round === "victory", undefined, {
    timeout: 3_000
  });
  await page.getByRole("heading", { name: "镇守已破" }).waitFor({ state: "visible" });
  report.states.victory = await state();
  assert.equal(report.states.victory.enemyHealth, "0");
  await frame.screenshot({ path: resolve(evidence, "04-victory.png") });
  report.screenshots.push("evidence/04-victory.png");

  const mobile = await context.newPage();
  await mobile.setViewportSize({ width: 760, height: 600 });
  await mobile.goto(origin, { waitUntil: "domcontentloaded", timeout: 30_000 });
  await mobile.waitForFunction(() => document.body.dataset.round === "waiting");
  report.responsive = await mobile.evaluate(() => ({
    viewportWidth: window.innerWidth,
    documentWidth: document.documentElement.scrollWidth,
    frameWidth: document.querySelector(".game-frame")?.getBoundingClientRect().width ?? 0,
    startVisible: !document.querySelector("#start-modal")?.hasAttribute("hidden")
  }));
  assert(report.responsive.documentWidth <= report.responsive.viewportWidth);
  assert.equal(report.responsive.startVisible, true);
  await mobile.close();

  assert.deepEqual(consoleProblems, []);
  assert.deepEqual(pageErrors, []);
  report.browserVersion = browser.version();
  report.status = "passed";
} finally {
  await writeFile(resolve(evidence, "playtest-report.json"), `${JSON.stringify(report, null, 2)}\n`);
  await browser.close();
}

console.log(JSON.stringify(report, null, 2));
