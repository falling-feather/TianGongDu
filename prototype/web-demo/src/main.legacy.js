const canvas = document.querySelector("#scene");
const ctx = canvas.getContext("2d", { alpha: false });

const BASE_FOUR_EYES = {
  inheritance: 52,
  market: 45,
  dailyLife: 60,
  ritualFaith: 54
};

const ACTIONS = {
  light: { label: "纸刺", cooldown: 0.22, stamina: 8 },
  spin: { label: "旋伞", cooldown: 0.75, stamina: 16, wind: 10 },
  wind: { label: "收风", cooldown: 1.15, wind: 25 },
  dash: { label: "借风", cooldown: 0.85, wind: 18 },
  guard: { label: "架伞", cooldown: 0.1 }
};

function createEnemies() {
  return [
    { x: 525, y: 282, disorder: 55, max: 55, phase: 0.3, label: "雨魇", restored: false },
    { x: 816, y: 390, disorder: 70, max: 70, phase: 2.1, label: "破伞影", restored: false }
  ];
}

const world = {
  width: 1280,
  height: 720,
  time: 0,
  weather: 0,
  choice: null,
  restore: 1,
  message: "纸伞匠人正在观察你的架伞节奏。",
  context: "检视雨线",
  log: ["抵达听雨轩，雨线里有细小伞骨声。"],
  fourEyes: { ...BASE_FOUR_EYES },
  cooldowns: Object.fromEntries(Object.keys(ACTIONS).map((action) => [action, 0])),
  player: {
    x: 614,
    y: 396,
    vx: 0,
    vy: 0,
    vitality: 100,
    stamina: 100,
    wind: 52,
    guard: false,
    action: "idle",
    actionTimer: 0,
    facing: 1
  },
  enemies: createEnemies(),
  vent: { x: 858, y: 482, pulse: 0 },
  shop: { x: 935, y: 330 }
};

const keys = new Set();
const elements = {
  vitalityMeter: document.querySelector("#vitalityMeter"),
  staminaMeter: document.querySelector("#staminaMeter"),
  vitalityText: document.querySelector("#vitalityText"),
  staminaText: document.querySelector("#staminaText"),
  windPips: document.querySelector("#windPips"),
  restoreRow: document.querySelector("#restoreRow"),
  objective: document.querySelector("#objective"),
  stateLine: document.querySelector("#stateLine"),
  contextPrompt: document.querySelector("#contextPrompt"),
  eventLog: document.querySelector("#eventLog"),
  rushOrderButton: document.querySelector("#rushOrderButton"),
  fullProcessButton: document.querySelector("#fullProcessButton"),
  resetButton: document.querySelector("#resetButton"),
  hotbarButtons: [...document.querySelectorAll(".hotbar button")],
  fourEyes: {
    inheritance: {
      meter: document.querySelector("#inheritanceMeter"),
      text: document.querySelector("#inheritanceText")
    },
    market: {
      meter: document.querySelector("#marketMeter"),
      text: document.querySelector("#marketText")
    },
    dailyLife: {
      meter: document.querySelector("#dailyLifeMeter"),
      text: document.querySelector("#dailyLifeText")
    },
    ritualFaith: {
      meter: document.querySelector("#ritualFaithMeter"),
      text: document.querySelector("#ritualFaithText")
    }
  }
};

for (let i = 0; i < 8; i += 1) {
  elements.windPips.append(document.createElement("i"));
}

for (let i = 0; i < 5; i += 1) {
  elements.restoreRow.append(document.createElement("i"));
}

const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const lerp = (a, b, t) => a + (b - a) * t;

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  canvas.width = Math.floor(canvas.clientWidth * dpr);
  canvas.height = Math.floor(canvas.clientHeight * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

window.addEventListener("resize", resize);
resize();

window.addEventListener("keydown", (event) => {
  if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", " "].includes(event.key)) {
    event.preventDefault();
  }
  keys.add(event.key.toLowerCase());
  if (event.key.toLowerCase() === "1") lightStrike("light");
  if (event.key.toLowerCase() === "2") lightStrike("spin");
  if (event.key.toLowerCase() === "e") gatherWind();
  if (event.key.toLowerCase() === "r") borrowWind();
  if (event.key.toLowerCase() === "f") interact();
});

window.addEventListener("keyup", (event) => {
  keys.delete(event.key.toLowerCase());
});

elements.rushOrderButton.addEventListener("click", () => choose("rush"));
elements.fullProcessButton.addEventListener("click", () => choose("full"));
elements.resetButton.addEventListener("click", resetDemo);

for (const button of elements.hotbarButtons) {
  button.addEventListener("click", () => {
    const action = button.dataset.action;
    if (action === "wind") gatherWind();
    if (action === "dash") borrowWind();
    if (action === "guard") pulseGuard();
    if (action === "light" || action === "spin") lightStrike(action);
  });
}

function addLog(text) {
  world.log = [text, ...world.log.filter((entry) => entry !== text)].slice(0, 4);
}

function setCooldown(action) {
  world.cooldowns[action] = ACTIONS[action].cooldown;
}

function isCooling(action) {
  return world.cooldowns[action] > 0;
}

function spend(action) {
  const config = ACTIONS[action];
  if (isCooling(action)) {
    world.message = `${config.label}还在回势。`;
    return false;
  }
  if (config.stamina && world.player.stamina < config.stamina) {
    world.message = "气力不足，先稳住身位。";
    return false;
  }
  if (config.wind && world.player.wind < config.wind) {
    world.message = "风息不足，先架伞接雨势。";
    return false;
  }
  if (config.stamina) {
    world.player.stamina = clamp(world.player.stamina - config.stamina, 0, 100);
  }
  if (config.wind) {
    world.player.wind = clamp(world.player.wind - config.wind, 0, 100);
  }
  setCooldown(action);
  return true;
}

function applyFourEyeDelta(delta) {
  for (const [key, value] of Object.entries(delta)) {
    world.fourEyes[key] = clamp(world.fourEyes[key] + value, 0, 100);
  }
}

function choose(kind) {
  world.choice = kind;
  world.fourEyes = { ...BASE_FOUR_EYES };
  if (kind === "rush") {
    applyFourEyeDelta({ market: 15, inheritance: -10, ritualFaith: -5 });
    world.message = "夜市急单已接：市场回声增强，师傅对省工略有迟疑。";
    world.restore = Math.max(world.restore, 2);
    addLog("接下远方急单，夜市灯牌亮起。");
  } else {
    applyFourEyeDelta({ inheritance: 15, ritualFaith: 10, market: -5 });
    world.message = "完整工序已定：师谱与礼信更稳，雨巷修复更温润。";
    world.restore = Math.max(world.restore, 3);
    addLog("坚持完整试伞，沈雨点头记下一笔。");
  }
  elements.rushOrderButton.classList.toggle("active", kind === "rush");
  elements.fullProcessButton.classList.toggle("active", kind === "full");
}

function getContextTarget() {
  const downedEnemy = world.enemies.find((enemy) => !enemy.restored && enemy.disorder <= 0 && dist(world.player, enemy) < 92);
  if (downedEnemy) {
    return { type: "restore", label: `归位 ${downedEnemy.label}`, enemy: downedEnemy };
  }

  if (dist(world.player, world.shop) < 130) {
    return { type: "shop", label: "请教师傅" };
  }

  if (dist(world.player, world.vent) < 110) {
    return { type: "vent", label: "检校风口" };
  }

  return { type: "inspect", label: "检视雨线" };
}

function interact() {
  const target = getContextTarget();

  if (target.type === "restore") {
    target.enemy.restored = true;
    world.restore = Math.max(world.restore, 3 + world.enemies.filter((enemy) => enemy.restored).length);
    world.player.wind = clamp(world.player.wind + 12, 0, 100);
    world.message = `${target.enemy.label}已归位，伞面余风回到工灯。`;
    addLog(`${target.enemy.label}归位，雨声退后一层。`);
    return;
  }

  if (target.type === "shop") {
    choose(world.choice === "full" ? "rush" : "full");
    return;
  }

  if (target.type === "vent") {
    world.player.wind = clamp(world.player.wind + 22, 0, 100);
    world.restore = Math.max(world.restore, 2);
    world.vent.pulse = 1;
    world.message = "工灯照见风口，伞面吃风更稳。";
    addLog("检校风口，屋檐回访线索浮现。");
    return;
  }

  world.message = "工灯扫过雨线，弱点在伞骨开合的一瞬。";
  addLog("雨线显形：可弹反的攻势会带短促亮边。");
}

function resetDemo() {
  world.choice = null;
  world.restore = 1;
  world.message = "纸伞匠人正在观察你的架伞节奏。";
  world.context = "检视雨线";
  world.log = ["抵达听雨轩，雨线里有细小伞骨声。"];
  world.fourEyes = { ...BASE_FOUR_EYES };
  world.cooldowns = Object.fromEntries(Object.keys(ACTIONS).map((action) => [action, 0]));
  Object.assign(world.player, {
    x: 614,
    y: 396,
    vitality: 100,
    stamina: 100,
    wind: 52,
    guard: false,
    action: "idle",
    actionTimer: 0,
    facing: 1
  });
  world.enemies = createEnemies();
  elements.rushOrderButton.classList.remove("active");
  elements.fullProcessButton.classList.remove("active");
  addLog("试炼重置，雨巷回到初始节奏。");
  syncUi();
}

function pulseGuard() {
  if (!spend("guard")) return;
  world.player.guard = true;
  world.player.action = "guard";
  world.player.actionTimer = 0.35;
}

function lightStrike(action = "light") {
  if (!spend(action)) return;
  const radius = action === "spin" ? 112 : 76;
  const power = action === "spin" ? 18 : 10;
  let hit = false;
  for (const enemy of world.enemies) {
    if (!enemy.restored && enemy.disorder > 0 && dist(world.player, enemy) < radius) {
      enemy.disorder = clamp(enemy.disorder - power, 0, enemy.max);
      hit = true;
      if (enemy.disorder <= 0) {
        world.message = `${enemy.label}失序已散，靠近按 F 完成归位。`;
        addLog(`${enemy.label}露出归位窗口。`);
      }
    }
  }
  if (!hit) {
    world.message = action === "spin" ? "旋伞扫开雨幕，但没有命中器影。" : "纸刺落空，雨魇仍在游移。";
  }
  world.player.action = action;
  world.player.actionTimer = action === "spin" ? 0.36 : 0.22;
}

function gatherWind() {
  if (!spend("wind")) return;
  world.player.action = "gather";
  world.player.actionTimer = 0.42;
  world.vent.pulse = 1;
  let hit = false;
  for (const enemy of world.enemies) {
    if (!enemy.restored && enemy.disorder > 0 && dist(world.player, enemy) < 190) {
      enemy.x = lerp(enemy.x, world.player.x, 0.18);
      enemy.y = lerp(enemy.y, world.player.y, 0.18);
      enemy.disorder = clamp(enemy.disorder - 18, 0, enemy.max);
      hit = true;
      if (enemy.disorder <= 0) {
        addLog(`${enemy.label}被收风卷出破绽。`);
      }
    }
  }
  world.message = hit ? "收风卷起雨幕，破绽显形。" : "收风扫过空巷，风息在雨面散开。";
}

function borrowWind() {
  if (!spend("dash")) return;
  const towardVent = dist(world.player, world.vent) < 220;
  const target = towardVent
    ? world.vent
    : {
        x: world.player.x + world.player.facing * 96,
        y: world.player.y - 24
      };
  world.player.x = clamp(lerp(world.player.x, target.x, 0.72), 240, 990);
  world.player.y = clamp(lerp(world.player.y, target.y, 0.72), 245, 545);
  world.player.action = "dash";
  world.player.actionTimer = 0.3;
  world.restore = Math.max(world.restore, 2);
  world.message = towardVent ? "借风越过水面，屋檐回访点已显形。" : "伞面借风，身位重整。";
  if (towardVent) addLog("借风越行成功，风口标记已校准。");
}

function update(dt) {
  world.time += dt;
  world.weather += dt * 0.8;
  world.vent.pulse = Math.max(0, world.vent.pulse - dt * 1.8);

  for (const action of Object.keys(world.cooldowns)) {
    world.cooldowns[action] = Math.max(0, world.cooldowns[action] - dt);
  }

  const player = world.player;

  let mx = 0;
  let my = 0;
  if (keys.has("a") || keys.has("arrowleft")) mx -= 1;
  if (keys.has("d") || keys.has("arrowright")) mx += 1;
  if (keys.has("w") || keys.has("arrowup")) my -= 1;
  if (keys.has("s") || keys.has("arrowdown")) my += 1;

  player.guard = keys.has("q") || (player.action === "guard" && player.actionTimer > 0);
  const wantsDash = keys.has("shift") || keys.has(" ");
  const len = Math.hypot(mx, my) || 1;
  const speed = player.guard ? 82 : wantsDash && player.stamina > 12 ? 190 : 122;

  if (wantsDash && (mx || my)) {
    player.stamina = clamp(player.stamina - dt * 24, 0, 100);
  } else if (!player.guard) {
    player.stamina = clamp(player.stamina + dt * 18, 0, 100);
  }

  if (player.guard) {
    player.stamina = clamp(player.stamina - dt * 8, 0, 100);
    player.wind = clamp(player.wind + dt * 5, 0, 100);
  }

  player.x = clamp(player.x + (mx / len) * speed * dt, 240, 990);
  player.y = clamp(player.y + (my / len) * speed * dt, 245, 545);
  if (mx) player.facing = Math.sign(mx);
  player.actionTimer = Math.max(0, player.actionTimer - dt);
  if (player.actionTimer === 0 && player.action !== "idle") {
    player.action = player.guard ? "guard" : "idle";
  }

  for (const enemy of world.enemies) {
    if (enemy.restored || enemy.disorder <= 0) continue;
    const angle = Math.atan2(player.y - enemy.y, player.x - enemy.x);
    const sway = Math.sin(world.time * 2 + enemy.phase) * 0.5;
    enemy.x += Math.cos(angle + sway * 0.2) * dt * 22;
    enemy.y += Math.sin(angle) * dt * 16;
    if (dist(player, enemy) < 56) {
      if (player.guard && player.stamina > 0) {
        player.wind = clamp(player.wind + dt * 24, 0, 100);
        enemy.disorder = clamp(enemy.disorder - dt * 7, 0, enemy.max);
        world.message = "架伞稳住雨针，风息正在积累。";
        if (enemy.disorder <= 0) addLog(`${enemy.label}被架伞定住。`);
      } else {
        player.vitality = clamp(player.vitality - dt * 1.2, 35, 100);
      }
    }
  }

  if (world.enemies.every((enemy) => enemy.restored)) {
    world.restore = 5;
    world.message = "雨巷初段归位，纸伞门径通过首轮检验。";
  }

  world.context = getContextTarget().label;
  syncUi();
}

function syncUi() {
  const player = world.player;
  elements.vitalityMeter.style.width = `${player.vitality}%`;
  elements.staminaMeter.style.width = `${player.stamina}%`;
  elements.vitalityText.textContent = `${Math.round(player.vitality)}/100`;
  elements.staminaText.textContent = `${Math.round(player.stamina)}/100`;
  [...elements.windPips.children].forEach((pip, index) => {
    pip.classList.toggle("filled", index < Math.round(player.wind / 12.5));
  });
  [...elements.restoreRow.children].forEach((pip, index) => {
    pip.classList.toggle("filled", index < world.restore);
  });

  for (const [key, ui] of Object.entries(elements.fourEyes)) {
    const value = world.fourEyes[key];
    ui.meter.style.width = `${value}%`;
    ui.text.textContent = Math.round(value);
  }

  elements.stateLine.textContent = world.message;
  elements.contextPrompt.querySelector("span").textContent = world.context;
  elements.eventLog.replaceChildren(
    ...world.log.map((entry) => {
      const item = document.createElement("li");
      item.textContent = entry;
      return item;
    })
  );

  elements.objective.textContent =
    world.restore >= 5
      ? "雨魇已散，返回纸伞铺完成《天工录》落款。"
      : "沿巷前行，穿越风口，抵达巷尽处的廊桥。";

  for (const button of elements.hotbarButtons) {
    const action = button.dataset.action;
    const cooldown = world.cooldowns[action] ?? 0;
    button.classList.toggle("active", action === world.player.action);
    button.classList.toggle("cooling", cooldown > 0.05);
    button.dataset.cooldown = cooldown > 0.05 ? `${cooldown.toFixed(1)}s` : "";
  }
}

function withWorldTransform(draw) {
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  const scale = Math.max(w / world.width, h / world.height);
  const ox = (w - world.width * scale) / 2;
  const oy = (h - world.height * scale) / 2;
  ctx.save();
  ctx.translate(ox, oy);
  ctx.scale(scale, scale);
  draw();
  ctx.restore();
}

function draw() {
  withWorldTransform(() => {
    drawScene();
    drawEntities();
    drawWeather();
    drawContextHalo();
    drawVignette();
  });
}

function drawScene() {
  const gradient = ctx.createLinearGradient(0, 0, 0, world.height);
  gradient.addColorStop(0, "#10252d");
  gradient.addColorStop(0.48, "#13242a");
  gradient.addColorStop(1, "#081216");
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, world.width, world.height);

  drawCanal();
  drawStonePath();
  drawBridge();
  drawShop();
  drawLanterns();
  drawWindVent();
  drawForeground();
}

function drawCanal() {
  ctx.save();
  ctx.fillStyle = "#0b3640";
  ctx.beginPath();
  ctx.moveTo(0, 170);
  ctx.bezierCurveTo(185, 260, 270, 280, 420, 350);
  ctx.bezierCurveTo(310, 430, 245, 500, 0, 610);
  ctx.closePath();
  ctx.fill();

  const water = ctx.createLinearGradient(0, 180, 420, 550);
  water.addColorStop(0, "rgba(70, 164, 183, 0.22)");
  water.addColorStop(1, "rgba(5, 26, 34, 0.4)");
  ctx.fillStyle = water;
  ctx.fill();

  ctx.strokeStyle = "rgba(126, 206, 218, 0.24)";
  ctx.lineWidth = 2;
  for (let i = 0; i < 16; i += 1) {
    const x = 30 + i * 34 + Math.sin(world.time + i) * 8;
    const y = 226 + (i % 8) * 38;
    ctx.beginPath();
    ctx.ellipse(x, y, 12 + (i % 3) * 4, 4, 0, 0, Math.PI * 2);
    ctx.stroke();
  }
  ctx.restore();
}

function drawStonePath() {
  ctx.save();
  ctx.fillStyle = "#273336";
  ctx.beginPath();
  ctx.moveTo(310, 258);
  ctx.lineTo(910, 220);
  ctx.lineTo(1064, 514);
  ctx.lineTo(362, 588);
  ctx.lineTo(222, 418);
  ctx.closePath();
  ctx.fill();

  ctx.strokeStyle = "rgba(230, 236, 217, 0.08)";
  ctx.lineWidth = 1;
  for (let x = 240; x < 1080; x += 44) {
    ctx.beginPath();
    ctx.moveTo(x, 244);
    ctx.lineTo(x - 130, 590);
    ctx.stroke();
  }
  for (let y = 250; y < 590; y += 34) {
    ctx.beginPath();
    ctx.moveTo(245, y);
    ctx.lineTo(1060, y - 42);
    ctx.stroke();
  }

  ctx.fillStyle = "rgba(232, 181, 94, 0.2)";
  for (let i = 0; i < 18; i += 1) {
    const x = 480 + ((i * 97) % 440);
    const y = 280 + ((i * 53) % 250);
    ctx.beginPath();
    ctx.ellipse(x, y, 22, 5, -0.2, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();
}

function drawBridge() {
  ctx.save();
  ctx.translate(105, 520);
  ctx.rotate(-0.12);
  ctx.strokeStyle = "#152426";
  ctx.lineWidth = 26;
  ctx.beginPath();
  ctx.arc(122, 0, 150, Math.PI * 1.05, Math.PI * 1.95);
  ctx.stroke();
  ctx.strokeStyle = "#4b4a3f";
  ctx.lineWidth = 16;
  ctx.beginPath();
  ctx.arc(122, 0, 150, Math.PI * 1.05, Math.PI * 1.95);
  ctx.stroke();
  ctx.strokeStyle = "rgba(234, 216, 166, 0.3)";
  ctx.lineWidth = 2;
  for (let i = 0; i < 9; i += 1) {
    ctx.beginPath();
    ctx.moveTo(i * 34, -104 + (i % 2) * 4);
    ctx.lineTo(i * 34 - 24, -42);
    ctx.stroke();
  }
  ctx.restore();
}

function drawShop() {
  ctx.save();
  ctx.fillStyle = "#1d2020";
  ctx.beginPath();
  ctx.moveTo(676, 108);
  ctx.lineTo(1055, 120);
  ctx.lineTo(1018, 260);
  ctx.lineTo(620, 244);
  ctx.closePath();
  ctx.fill();

  ctx.fillStyle = "#11191c";
  for (let x = 640; x < 1030; x += 28) {
    ctx.fillRect(x, 120 + Math.sin(x) * 4, 24, 92);
  }

  ctx.fillStyle = "#7a4f29";
  ctx.fillRect(668, 216, 330, 120);
  ctx.fillStyle = "rgba(239, 178, 85, 0.28)";
  ctx.fillRect(686, 232, 292, 84);

  drawSign(640, 154, "听雨轩");
  drawUmbrella(734, 292, 54, "#efe2c7", -0.2);
  drawUmbrella(812, 278, 58, "#f2e5c8", 0.08);
  drawUmbrella(884, 302, 50, "#e6d8bf", -0.12);
  drawNpc(935, 330);
  ctx.restore();
}

function drawSign(x, y, text) {
  ctx.save();
  ctx.translate(x, y);
  ctx.fillStyle = "#dba557";
  ctx.fillRect(0, 0, 42, 116);
  ctx.strokeStyle = "#3f2b19";
  ctx.lineWidth = 4;
  ctx.strokeRect(0, 0, 42, 116);
  ctx.fillStyle = "#1f140b";
  ctx.font = "700 22px serif";
  [...text].forEach((char, index) => ctx.fillText(char, 10, 28 + index * 26));
  ctx.restore();
}

function drawLanterns() {
  ctx.save();
  for (const [x, y] of [
    [586, 528],
    [704, 234],
    [928, 268]
  ]) {
    const glow = ctx.createRadialGradient(x, y, 4, x, y, 70);
    glow.addColorStop(0, "rgba(239, 170, 70, 0.48)");
    glow.addColorStop(1, "rgba(239, 170, 70, 0)");
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(x, y, 70, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "#e9b058";
    ctx.fillRect(x - 10, y - 14, 20, 28);
  }
  ctx.restore();
}

function drawWindVent() {
  const { x, y } = world.vent;
  ctx.save();
  ctx.translate(x, y);
  ctx.strokeStyle = `rgba(105, 213, 239, ${0.45 + world.vent.pulse * 0.4})`;
  ctx.lineWidth = 3;
  for (let i = 0; i < 4; i += 1) {
    ctx.beginPath();
    ctx.ellipse(0, 0, 34 + i * 14 + Math.sin(world.time * 2 + i) * 4, 12 + i * 3, world.time + i * 0.5, 0, Math.PI * 1.65);
    ctx.stroke();
  }
  ctx.fillStyle = "rgba(169, 234, 250, 0.9)";
  ctx.beginPath();
  ctx.moveTo(0, -34);
  ctx.lineTo(12, -16);
  ctx.lineTo(4, -16);
  ctx.lineTo(4, 18);
  ctx.lineTo(-4, 18);
  ctx.lineTo(-4, -16);
  ctx.lineTo(-12, -16);
  ctx.closePath();
  ctx.fill();
  ctx.font = "700 18px sans-serif";
  ctx.fillText("风口", -18, 54);
  ctx.restore();
}

function drawForeground() {
  ctx.save();
  ctx.fillStyle = "rgba(12, 24, 18, 0.8)";
  ctx.beginPath();
  ctx.moveTo(0, 620);
  ctx.bezierCurveTo(300, 570, 502, 678, 830, 622);
  ctx.lineTo(1280, 720);
  ctx.lineTo(0, 720);
  ctx.closePath();
  ctx.fill();

  ctx.strokeStyle = "rgba(102, 139, 91, 0.52)";
  ctx.lineWidth = 5;
  for (let i = 0; i < 24; i += 1) {
    const x = 35 + (i * 23) % 270;
    ctx.beginPath();
    ctx.moveTo(x, 110);
    ctx.bezierCurveTo(x - 12, 210, x + 22, 294, x - 8, 380);
    ctx.stroke();
  }
  ctx.restore();
}

function drawEntities() {
  const sorted = [
    ...world.enemies.map((enemy) => ({ type: "enemy", entity: enemy })),
    { type: "player", entity: world.player }
  ].sort((a, b) => a.entity.y - b.entity.y);

  for (const item of sorted) {
    if (item.type === "enemy") drawEnemy(item.entity);
    else drawPlayer(item.entity);
  }

  if (world.player.action === "gather" && world.player.actionTimer > 0) {
    ctx.save();
    ctx.strokeStyle = "rgba(128, 229, 244, 0.75)";
    ctx.lineWidth = 4;
    ctx.beginPath();
    ctx.arc(world.player.x, world.player.y - 22, 190 * (1 - world.player.actionTimer), 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
  }
}

function drawPlayer(player) {
  ctx.save();
  ctx.translate(player.x, player.y);
  drawShadow(0, 18, 40, 12);
  if (player.action === "dash") {
    ctx.strokeStyle = "rgba(138, 224, 242, 0.42)";
    ctx.lineWidth = 8;
    ctx.beginPath();
    ctx.moveTo(-player.facing * 66, 16);
    ctx.lineTo(-player.facing * 14, -4);
    ctx.stroke();
  }
  drawUmbrella(0, -48, player.guard ? 58 : 44, "#efe7d3", player.guard ? 0 : -0.24 * player.facing);
  ctx.fillStyle = "#233947";
  ctx.fillRect(-10, -20, 20, 36);
  ctx.fillStyle = "#c9b187";
  ctx.beginPath();
  ctx.arc(0, -28, 10, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = player.guard ? "#a9edf1" : "#d8c494";
  ctx.lineWidth = player.guard ? 4 : 2;
  ctx.beginPath();
  ctx.moveTo(0, -42);
  ctx.lineTo(player.facing * 28, 14);
  ctx.stroke();
  if (player.guard) {
    ctx.strokeStyle = "rgba(175, 237, 238, 0.55)";
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(0, -22, 54, -0.3, Math.PI + 0.3);
    ctx.stroke();
  }
  ctx.restore();
}

function drawEnemy(enemy) {
  ctx.save();
  ctx.translate(enemy.x, enemy.y + Math.sin(world.time * 3 + enemy.phase) * 4);
  drawShadow(0, 18, 36, 10);

  if (enemy.restored) {
    ctx.globalAlpha = 0.42;
  }

  drawUmbrella(0, -40, 36, enemy.disorder <= 0 ? "#f2ead7" : "#d7ded9", Math.sin(world.time + enemy.phase) * 0.15);
  ctx.strokeStyle = enemy.disorder <= 0 ? "rgba(220, 236, 224, 0.45)" : "rgba(198, 232, 233, 0.72)";
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.moveTo(-10, -22);
  ctx.bezierCurveTo(-28, 6, -8, 18, -22, 36);
  ctx.moveTo(10, -22);
  ctx.bezierCurveTo(32, 4, 8, 22, 26, 36);
  ctx.stroke();
  if (!enemy.restored) {
    ctx.fillStyle = enemy.disorder <= 0 ? "#e6c879" : "#c85b55";
    ctx.fillRect(-28, -66, (56 * Math.max(enemy.disorder, 4)) / enemy.max, 4);
    ctx.fillStyle = "#f1e7dc";
    ctx.font = "700 13px sans-serif";
    ctx.fillText(enemy.disorder <= 0 ? "待归位" : enemy.label, -24, -74);
  }
  ctx.restore();
}

function drawContextHalo() {
  const target = getContextTarget();
  let point = world.player;
  if (target.type === "shop") point = world.shop;
  if (target.type === "vent") point = world.vent;
  if (target.type === "restore") point = target.enemy;
  if (target.type === "inspect") return;

  ctx.save();
  ctx.strokeStyle = target.type === "restore" ? "rgba(232, 200, 121, 0.72)" : "rgba(139, 226, 238, 0.62)";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.ellipse(point.x, point.y + 20, 54 + Math.sin(world.time * 4) * 4, 18, 0, 0, Math.PI * 2);
  ctx.stroke();
  ctx.restore();
}

function drawNpc(x, y) {
  ctx.save();
  ctx.translate(x, y);
  drawShadow(0, 20, 36, 10);
  ctx.fillStyle = "#244152";
  ctx.fillRect(-12, -34, 24, 52);
  ctx.fillStyle = "#d8b88b";
  ctx.beginPath();
  ctx.arc(0, -42, 10, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#92e3a5";
  ctx.font = "700 15px sans-serif";
  ctx.fillText("纸伞匠人", 18, -34);
  ctx.fillStyle = "rgba(255, 255, 255, 0.86)";
  ctx.beginPath();
  ctx.roundRect(-48, -70, 34, 22, 8);
  ctx.fill();
  ctx.fillStyle = "#28343a";
  ctx.fillText("...", -41, -54);
  ctx.restore();
}

function drawUmbrella(x, y, r, fill, angle) {
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(angle);
  ctx.fillStyle = fill;
  ctx.beginPath();
  ctx.ellipse(0, 0, r, r * 0.32, 0, Math.PI, 0);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = "rgba(82, 57, 31, 0.55)";
  ctx.lineWidth = 1;
  for (let i = -3; i <= 3; i += 1) {
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(i * (r / 3), -r * 0.28);
    ctx.stroke();
  }
  ctx.strokeStyle = "rgba(49, 36, 22, 0.64)";
  ctx.lineWidth = 2;
  ctx.stroke();
  ctx.restore();
}

function drawShadow(x, y, rx, ry) {
  ctx.save();
  ctx.fillStyle = "rgba(0, 0, 0, 0.32)";
  ctx.beginPath();
  ctx.ellipse(x, y, rx, ry, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function drawWeather() {
  ctx.save();
  ctx.strokeStyle = "rgba(153, 209, 226, 0.42)";
  ctx.lineWidth = 1;
  for (let i = 0; i < 140; i += 1) {
    const x = (i * 97 + world.weather * 190) % (world.width + 120) - 80;
    const y = (i * 53 + world.weather * 320) % (world.height + 120) - 60;
    ctx.beginPath();
    ctx.moveTo(x, y);
    ctx.lineTo(x - 9, y + 32);
    ctx.stroke();
  }
  ctx.restore();
}

function drawVignette() {
  const vignette = ctx.createRadialGradient(640, 360, 180, 640, 360, 780);
  vignette.addColorStop(0, "rgba(0,0,0,0)");
  vignette.addColorStop(1, "rgba(0,0,0,0.62)");
  ctx.fillStyle = vignette;
  ctx.fillRect(0, 0, world.width, world.height);
}

let lastTime = performance.now();
function frame(now) {
  const dt = Math.min((now - lastTime) / 1000, 0.05);
  lastTime = now;
  update(dt);
  draw();
  requestAnimationFrame(frame);
}

syncUi();
requestAnimationFrame(frame);
