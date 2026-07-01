const canvas = document.querySelector("#scene");
const ctx = canvas.getContext("2d", { alpha: false });

const DAY_MINUTES = 24 * 60;
const TIME_SCALE = 1.8;
const BASE_FOUR_EYES = {
  inheritance: 52,
  market: 45,
  dailyLife: 60,
  ritualFaith: 54
};

const ACTIONS = {
  light: { label: "纸刃", cooldown: 0.22, stamina: 8 },
  spin: { label: "伞旋", cooldown: 0.75, stamina: 16, wind: 10 },
  wind: { label: "收风", cooldown: 1.15, wind: 25 },
  dash: { label: "借风", cooldown: 0.85, wind: 18 },
  guard: { label: "架伞", cooldown: 0.1 }
};

const INVENTORY_DEFS = {
  bamboo: { name: "青篾", type: "材料", description: "竹林小径采得的伞骨料，适合做临时补强。" },
  tungOil: { name: "桐油", type: "材料", description: "纸伞铺常备的防雨油，可稳定伞面张力。" },
  rainPaper: { name: "雨纹纸", type: "材料", description: "受潮后仍能吃风的纸料，是试伞补片的底材。" },
  windBell: { name: "风铃铆", type: "机关", description: "能校准屋檐风口，让断桥雨幕露出短暂空隙。", usable: true },
  orderSlip: { name: "夜市凭单", type: "委托", description: "跑单人留下的急单凭据，会牵动营生与师承评价。", usable: true },
  rainPatch: { name: "试伞补片", type: "消耗", description: "临时修补纸伞，恢复生机与气力。", usable: true }
};

const MAP_NODES = [
  {
    id: "water_entry",
    name: "水路入镇",
    type: "入口",
    x: 230,
    y: 642,
    spawn: { x: 298, y: 626 },
    description: "乌篷船靠岸处，雨声最密，适合重新整理伞骨。",
    discoverRadius: 150
  },
  {
    id: "main_alley",
    name: "水巷主路",
    type: "街巷",
    x: 566,
    y: 470,
    spawn: { x: 566, y: 470 },
    description: "居民屋檐压得很低，雨线会暴露可交互的检查点。",
    discoverRadius: 170
  },
  {
    id: "umbrella_shop",
    name: "纸伞铺",
    type: "据点",
    x: 918,
    y: 324,
    spawn: { x: 858, y: 390 },
    description: "纸伞匠沈雨的铺子，可修补纸伞、承接夜市委托。",
    discoverRadius: 190
  },
  {
    id: "old_bridge",
    name: "旧桥下层",
    type: "战斗",
    x: 430,
    y: 592,
    spawn: { x: 444, y: 568 },
    description: "桥影会遮住雨魇，适合检验架伞、弹反与归位。",
    discoverRadius: 165
  },
  {
    id: "bamboo_path",
    name: "竹林小径",
    type: "采集",
    x: 1216,
    y: 454,
    spawn: { x: 1172, y: 478 },
    description: "青篾散在竹筐旁，路面湿滑但能避开主巷追击。",
    discoverRadius: 175
  },
  {
    id: "night_market",
    name: "夜市口",
    type: "委托",
    x: 1090,
    y: 624,
    spawn: { x: 1050, y: 604 },
    description: "酉时后灯牌亮起，跑单人会带来远方急单。",
    discoverRadius: 180
  },
  {
    id: "roof_route",
    name: "屋檐上层",
    type: "回访",
    x: 1190,
    y: 214,
    spawn: { x: 1136, y: 244 },
    description: "借风可达的高处路线，能绕开水巷低洼段。",
    discoverRadius: 140,
    hiddenUntilFlag: "roofRouteSeen"
  },
  {
    id: "rain_curtain",
    name: "断桥雨幕",
    type: "封锁",
    x: 1394,
    y: 250,
    spawn: { x: 1346, y: 288 },
    description: "后续 Boss 房入口，现在需要风铃铆校准风口。",
    discoverRadius: 160,
    requiresFlag: "rainCurtainOpen"
  }
];

const GATHER_SPOTS = [
  {
    id: "oil_jars",
    name: "铺前桐油罐",
    x: 734,
    y: 318,
    item: "tungOil",
    qty: 1,
    radius: 78,
    cooldown: 240,
    message: "取出一小盏桐油，伞面能多撑一阵。"
  },
  {
    id: "rain_paper",
    name: "潮纸箱",
    x: 398,
    y: 396,
    item: "rainPaper",
    qty: 1,
    radius: 76,
    cooldown: 260,
    message: "翻到一张还未破浆的雨纹纸。"
  },
  {
    id: "bamboo_basket",
    name: "竹林料篓",
    x: 1248,
    y: 482,
    item: "bamboo",
    qty: 2,
    radius: 92,
    cooldown: 220,
    message: "挑出两段韧性尚好的青篾。"
  },
  {
    id: "wind_bell",
    name: "屋檐风铃",
    x: 1136,
    y: 238,
    item: "windBell",
    qty: 1,
    radius: 88,
    once: true,
    message: "摘下风铃铆，屋檐风口的角度能重新校准。"
  }
];

const NPC_DEFS = [
  {
    id: "master_shen",
    name: "纸伞匠沈雨",
    roles: ["师承", "工艺"],
    trust: 2,
    tension: 0,
    color: "#8fd6a3",
    schedules: [
      { from: 6 * 60, to: 18 * 60, x: 934, y: 330, place: "纸伞铺", active: true },
      { from: 18 * 60, to: 22 * 60, x: 1048, y: 574, place: "夜市口", active: true },
      { from: 22 * 60, to: 6 * 60, x: 934, y: 330, place: "铺内后间", active: false }
    ],
    lines: {
      default: ["伞面不是盾，先听雨线，再决定要不要架。", "青篾、雨纹纸、桐油齐了，就能做一枚试伞补片。"],
      evening: ["夜市的灯一亮，急单就会催人省工。你自己拿主意。", "若要走完整工序，慢一点，但伞会记住每一处手劲。"]
    }
  },
  {
    id: "market_runner",
    name: "夜市跑单人",
    roles: ["委托", "营生"],
    trust: 0,
    tension: 1,
    color: "#e6b661",
    schedules: [
      { from: 17 * 60 + 30, to: 23 * 60 + 30, x: 1092, y: 620, place: "夜市口", active: true },
      { from: 23 * 60 + 30, to: 17 * 60 + 30, x: 282, y: 628, place: "水路入镇", active: true }
    ],
    lines: {
      default: ["远方客船等伞，钱急，规矩也急。", "你若接单，我给凭单；若走全工序，夜市也会等你的说法。"],
      night: ["雨越大越好做生意，只是别把手艺做薄了。"]
    }
  },
  {
    id: "alley_resident",
    name: "巷口住户阿棠",
    roles: ["居民", "线索"],
    trust: 1,
    tension: 0,
    color: "#83c6e8",
    schedules: [
      { from: 7 * 60, to: 20 * 60, x: 562, y: 482, place: "水巷主路", active: true },
      { from: 20 * 60, to: 23 * 60, x: 700, y: 536, place: "桥边灯下", active: true },
      { from: 23 * 60, to: 7 * 60, x: 562, y: 482, place: "住户屋内", active: false }
    ],
    lines: {
      default: ["旧桥下有伞影，靠近前先看地上的雨纹。", "竹林那边有料篓，匠人们常把青篾临时放在那里。"],
      night: ["夜里别贴水走，雨魇会从桥洞里跟上来。"]
    }
  }
];

const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const lerp = (a, b, t) => a + (b - a) * t;

function createEnemies() {
  return [
    { id: "rain_wraith", x: 520, y: 454, homeX: 520, homeY: 454, aggroRange: 310, disorder: 55, max: 55, phase: 0.3, label: "雨魇", restored: false, attackCooldown: 1.4, windup: 0, attackFlash: 0, damage: 10 },
    { id: "broken_umbrella_shadow", x: 454, y: 598, homeX: 454, homeY: 598, aggroRange: 285, disorder: 78, max: 78, phase: 2.1, label: "破伞影", restored: false, attackCooldown: 2.2, windup: 0, attackFlash: 0, damage: 14 }
  ];
}

function createNpcs() {
  return NPC_DEFS.map((npc) => ({
    ...npc,
    x: npc.schedules[0].x,
    y: npc.schedules[0].y,
    place: npc.schedules[0].place,
    active: npc.schedules[0].active
  }));
}

function createGatherSpots() {
  return GATHER_SPOTS.map((spot) => ({ ...spot, cooldownLeft: 0, taken: false }));
}

function createWorld() {
  return {
    width: 1600,
    height: 900,
    time: 0,
    totalMinutes: 16 * 60 + 20,
    weather: 0,
    choice: null,
    restore: 1,
    started: false,
    message: "纸伞匠沈雨正在观察你的架伞节奏。",
    context: "检视雨线",
    objective: "沿雨巷探索，拜访纸伞匠，收集修伞材料并校准屋檐风口。",
    log: ["抵达听雨轩，雨线里有细小伞骨声。"],
    fourEyes: { ...BASE_FOUR_EYES },
    cooldowns: Object.fromEntries(Object.keys(ACTIONS).map((action) => [action, 0])),
    inventory: { rainPaper: 1, tungOil: 1, rainPatch: 1 },
    flags: { roofRouteSeen: false, rainCurtainOpen: false, residentGift: false },
    discovered: new Set(["water_entry", "main_alley", "umbrella_shop", "old_bridge"]),
    visited: new Set(["umbrella_shop"]),
    activePanel: "map",
    currentLocationId: "umbrella_shop",
    talkIndex: {},
    player: { x: 858, y: 390, vx: 0, vy: 0, vitality: 100, stamina: 100, wind: 52, guard: false, action: "idle", actionTimer: 0, facing: 1 },
    enemies: createEnemies(),
    npcs: createNpcs(),
    gatherSpots: createGatherSpots(),
    vent: { x: 1046, y: 428, pulse: 0 },
    shop: { x: 918, y: 324 }
  };
}

let world = createWorld();
const keys = new Set();

const elements = {
  cover: document.querySelector("#cover"),
  startButton: document.querySelector("#startButton"),
  coverMapButton: document.querySelector("#coverMapButton"),
  vitalityMeter: document.querySelector("#vitalityMeter"),
  staminaMeter: document.querySelector("#staminaMeter"),
  vitalityText: document.querySelector("#vitalityText"),
  staminaText: document.querySelector("#staminaText"),
  windPips: document.querySelector("#windPips"),
  timeText: document.querySelector("#timeText"),
  timePeriod: document.querySelector("#timePeriod"),
  locationText: document.querySelector("#locationText"),
  weatherText: document.querySelector("#weatherText"),
  restoreRow: document.querySelector("#restoreRow"),
  objective: document.querySelector("#objective"),
  stateLine: document.querySelector("#stateLine"),
  contextPrompt: document.querySelector("#contextPrompt"),
  eventLog: document.querySelector("#eventLog"),
  rushOrderButton: document.querySelector("#rushOrderButton"),
  fullProcessButton: document.querySelector("#fullProcessButton"),
  resetButton: document.querySelector("#resetButton"),
  restButton: document.querySelector("#restButton"),
  craftButton: document.querySelector("#craftButton"),
  hotbarButtons: [...document.querySelectorAll(".hotbar button")],
  panelTabs: [...document.querySelectorAll(".drawer-tabs button")],
  drawerPanels: {
    map: document.querySelector("#mapPanel"),
    inventory: document.querySelector("#inventoryPanel"),
    npcs: document.querySelector("#npcsPanel"),
    journal: document.querySelector("#journalPanel")
  },
  miniMapNodes: document.querySelector("#miniMapNodes"),
  mapBoard: document.querySelector("#mapBoard"),
  mapDetails: document.querySelector("#mapDetails"),
  inventoryList: document.querySelector("#inventoryList"),
  npcList: document.querySelector("#npcList"),
  fourEyes: {
    inheritance: { meter: document.querySelector("#inheritanceMeter"), text: document.querySelector("#inheritanceText") },
    market: { meter: document.querySelector("#marketMeter"), text: document.querySelector("#marketText") },
    dailyLife: { meter: document.querySelector("#dailyLifeMeter"), text: document.querySelector("#dailyLifeText") },
    ritualFaith: { meter: document.querySelector("#ritualFaithMeter"), text: document.querySelector("#ritualFaithText") }
  }
};

for (let i = 0; i < 8; i += 1) elements.windPips.append(document.createElement("i"));
for (let i = 0; i < 5; i += 1) elements.restoreRow.append(document.createElement("i"));

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  canvas.width = Math.floor(canvas.clientWidth * dpr);
  canvas.height = Math.floor(canvas.clientHeight * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

window.addEventListener("resize", resize);
resize();

window.addEventListener("keydown", (event) => {
  const key = event.key.toLowerCase();
  if (["arrowup", "arrowdown", "arrowleft", "arrowright", " "].includes(key)) event.preventDefault();
  keys.add(key);
  if (key === "1") lightStrike("light");
  if (key === "2") lightStrike("spin");
  if (key === "e") gatherWind();
  if (key === "r") borrowWind();
  if (key === "f") interact();
  if (key === "m") setPanel("map");
  if (key === "i") setPanel("inventory");
  if (key === "n") setPanel("npcs");
  if (key === "j") setPanel("journal");
  if (key === "t") advanceTime(30, "你在檐下小憩一刻，雨势换了声调。");
});

window.addEventListener("keyup", (event) => {
  keys.delete(event.key.toLowerCase());
});

elements.startButton.addEventListener("click", () => startGame("map"));
elements.coverMapButton.addEventListener("click", () => startGame("map"));
elements.rushOrderButton.addEventListener("click", () => choose("rush"));
elements.fullProcessButton.addEventListener("click", () => choose("full"));
elements.resetButton.addEventListener("click", resetDemo);
elements.restButton.addEventListener("click", () => advanceTime(30, "你在檐下小憩一刻，雨势换了声调。"));
elements.craftButton.addEventListener("click", craftUmbrellaPatch);
elements.inventoryList.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-use-item]");
  if (button) useItem(button.dataset.useItem);
});
elements.mapBoard.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-node]");
  if (button) travelToNode(button.dataset.node);
});

for (const button of elements.panelTabs) {
  button.addEventListener("click", () => setPanel(button.dataset.panel));
}

for (const button of elements.hotbarButtons) {
  button.addEventListener("click", () => {
    const action = button.dataset.action;
    if (action === "wind") gatherWind();
    if (action === "dash") borrowWind();
    if (action === "guard") pulseGuard();
    if (action === "light" || action === "spin") lightStrike(action);
  });
}

function startGame(panel = "map") {
  world.started = true;
  elements.cover.classList.add("hidden");
  setPanel(panel);
  addLog("纸伞门径试炼开始，雨巷地图已经展开。");
}

function setPanel(panel) {
  world.activePanel = panel;
  for (const button of elements.panelTabs) {
    const active = button.dataset.panel === panel;
    button.classList.toggle("active", active);
    button.setAttribute("aria-selected", String(active));
  }
  for (const [key, node] of Object.entries(elements.drawerPanels)) {
    node.classList.toggle("active", key === panel);
  }
  syncUi();
}

function addLog(text) {
  world.log = [text, ...world.log.filter((entry) => entry !== text)].slice(0, 6);
}

function spend(action) {
  const config = ACTIONS[action];
  if (world.cooldowns[action] > 0) {
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
  if (config.stamina) world.player.stamina = clamp(world.player.stamina - config.stamina, 0, 100);
  if (config.wind) world.player.wind = clamp(world.player.wind - config.wind, 0, 100);
  world.cooldowns[action] = config.cooldown;
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
    addItem("orderSlip", 1);
    world.message = "夜市急单已接：营生回声增强，师承对省工略有迟疑。";
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
  setPanel("journal");
}

function addItem(id, qty = 1) {
  world.inventory[id] = (world.inventory[id] ?? 0) + qty;
}

function removeItem(id, qty = 1) {
  if ((world.inventory[id] ?? 0) < qty) return false;
  world.inventory[id] -= qty;
  if (world.inventory[id] <= 0) delete world.inventory[id];
  return true;
}

function hasItem(id, qty = 1) {
  return (world.inventory[id] ?? 0) >= qty;
}

function craftUmbrellaPatch() {
  if (!hasItem("bamboo", 2) || !hasItem("rainPaper", 1) || !hasItem("tungOil", 1)) {
    world.message = "修补纸伞需要青篾 x2、雨纹纸 x1、桐油 x1。";
    addLog("纸伞补片材料还不齐。");
    setPanel("inventory");
    return;
  }
  removeItem("bamboo", 2);
  removeItem("rainPaper", 1);
  removeItem("tungOil", 1);
  addItem("rainPatch", 1);
  world.player.stamina = clamp(world.player.stamina + 18, 0, 100);
  applyFourEyeDelta({ inheritance: 3, dailyLife: 2 });
  world.message = "试伞补片完成，伞面张力更稳。";
  addLog("用青篾、雨纹纸和桐油做成一枚试伞补片。");
  setPanel("inventory");
}

function useItem(id) {
  if (id === "rainPatch") {
    if (!removeItem("rainPatch", 1)) return;
    world.player.vitality = clamp(world.player.vitality + 26, 0, 100);
    world.player.stamina = clamp(world.player.stamina + 18, 0, 100);
    world.message = "试伞补片贴合伞面，生机与气力回稳。";
    addLog("使用试伞补片，纸伞重新吃风。");
    return;
  }
  if (id === "windBell") {
    world.flags.rainCurtainOpen = true;
    world.discovered.add("rain_curtain");
    world.message = "风铃铆校准完成，断桥雨幕露出可通行的短隙。";
    addLog("风铃铆扣入屋檐风口，断桥雨幕暂时打开。");
    setPanel("map");
    return;
  }
  if (id === "orderSlip") choose("rush");
}

function getClockMinutes() {
  return Math.floor(world.totalMinutes % DAY_MINUTES);
}

function getDay() {
  return Math.floor(world.totalMinutes / DAY_MINUTES) + 1;
}

function formatClock() {
  const minutes = getClockMinutes();
  const hour = Math.floor(minutes / 60);
  const minute = minutes % 60;
  return `第 ${getDay()} 日 ${String(hour).padStart(2, "0")}:${String(minute).padStart(2, "0")}`;
}

function getPeriod() {
  const minutes = getClockMinutes();
  if (minutes >= 5 * 60 && minutes < 7 * 60) return { id: "dawn", name: "卯时", tone: "晨雾未散" };
  if (minutes >= 7 * 60 && minutes < 11 * 60) return { id: "morning", name: "辰巳", tone: "檐雨清亮" };
  if (minutes >= 11 * 60 && minutes < 17 * 60) return { id: "afternoon", name: "午未申", tone: "雨线渐密" };
  if (minutes >= 17 * 60 && minutes < 21 * 60) return { id: "evening", name: "酉戌", tone: "夜市起灯" };
  return { id: "night", name: "亥子丑", tone: "水巷入夜" };
}

function getWeatherText() {
  const phase = Math.sin(world.weather * 0.38);
  if (phase > 0.45) return "雨势转急，风口脉动明显";
  if (phase < -0.35) return "细雨贴地，桥洞影子更深";
  return "湿风从水巷卷入铺檐";
}

function advanceTime(minutes, message) {
  world.totalMinutes += minutes;
  world.weather += minutes * 0.05;
  if (message) {
    world.message = message;
    addLog(message);
  }
  syncNpcSchedules();
  discoverNearbyNodes();
  syncUi();
}

function timeInRange(minutes, from, to) {
  if (from <= to) return minutes >= from && minutes < to;
  return minutes >= from || minutes < to;
}

function syncNpcSchedules() {
  const minutes = getClockMinutes();
  for (const npc of world.npcs) {
    const schedule = npc.schedules.find((item) => timeInRange(minutes, item.from, item.to)) ?? npc.schedules[0];
    npc.x = schedule.x;
    npc.y = schedule.y;
    npc.place = schedule.place;
    npc.active = schedule.active;
  }
}

function getLocationById(id) {
  return MAP_NODES.find((node) => node.id === id);
}

function isNodeVisible(node) {
  if (node.hiddenUntilFlag && !world.flags[node.hiddenUntilFlag]) return false;
  return world.discovered.has(node.id) || !node.hiddenUntilFlag;
}

function isNodeUnlocked(node) {
  if (!node.requiresFlag) return true;
  return Boolean(world.flags[node.requiresFlag]);
}

function discoverNearbyNodes() {
  let closest = getLocationById(world.currentLocationId);
  let closestDistance = closest ? dist(world.player, closest) : Infinity;
  for (const node of MAP_NODES) {
    if (!isNodeVisible(node)) continue;
    const d = dist(world.player, node);
    if (d < node.discoverRadius && !world.discovered.has(node.id)) {
      world.discovered.add(node.id);
      addLog(`发现区域：${node.name}。`);
    }
    if (d < closestDistance) {
      closest = node;
      closestDistance = d;
    }
  }
  if (closest && closestDistance < closest.discoverRadius + 80) {
    world.currentLocationId = closest.id;
    world.visited.add(closest.id);
  }
}

function travelToNode(id) {
  const node = getLocationById(id);
  if (!node || !world.discovered.has(id)) return;
  if (!isNodeUnlocked(node)) {
    world.message = node.id === "rain_curtain" ? "断桥雨幕需要风铃铆校准。" : `${node.name}暂未打开。`;
    addLog(world.message);
    return;
  }
  const target = node.spawn ?? node;
  world.player.x = target.x;
  world.player.y = target.y;
  world.currentLocationId = node.id;
  world.visited.add(node.id);
  advanceTime(8, `沿地图前往${node.name}。`);
}

function getContextTarget() {
  const downedEnemy = world.enemies.find((enemy) => !enemy.restored && enemy.disorder <= 0 && dist(world.player, enemy) < 92);
  if (downedEnemy) return { type: "restore", label: `归位 ${downedEnemy.label}`, enemy: downedEnemy };
  const npc = world.npcs.find((item) => item.active && dist(world.player, item) < 86);
  if (npc) return { type: "npc", label: `交谈 ${npc.name}`, npc };
  const gatherSpot = world.gatherSpots.find((spot) => !(spot.taken && spot.once) && spot.cooldownLeft <= 0 && dist(world.player, spot) < spot.radius);
  if (gatherSpot) return { type: "gather", label: `拾取 ${gatherSpot.name}`, spot: gatherSpot };
  const rainCurtain = getLocationById("rain_curtain");
  if (dist(world.player, rainCurtain) < 120 && !world.flags.rainCurtainOpen) return { type: "locked_node", label: "校准断桥雨幕", node: rainCurtain };
  if (dist(world.player, world.vent) < 110) return { type: "vent", label: "校准风口" };
  return { type: "inspect", label: "检视雨线" };
}

function interact() {
  const target = getContextTarget();
  if (target.type === "restore") {
    target.enemy.restored = true;
    world.restore = Math.max(world.restore, 3 + world.enemies.filter((enemy) => enemy.restored).length);
    world.player.wind = clamp(world.player.wind + 12, 0, 100);
    addItem("rainPaper", 1);
    world.message = `${target.enemy.label}已归位，伞面余风回到工息。`;
    addLog(`${target.enemy.label}归位，雨声退后一层。`);
    return;
  }
  if (target.type === "npc") {
    talkToNpc(target.npc);
    return;
  }
  if (target.type === "gather") {
    collectSpot(target.spot);
    return;
  }
  if (target.type === "locked_node") {
    if (hasItem("windBell")) useItem("windBell");
    else {
      world.message = "雨幕太密，需要在屋檐上层找到风铃铆。";
      addLog("断桥雨幕暂时无法穿过。");
    }
    return;
  }
  if (target.type === "vent") {
    world.player.wind = clamp(world.player.wind + 22, 0, 100);
    world.restore = Math.max(world.restore, 2);
    world.flags.roofRouteSeen = true;
    world.discovered.add("roof_route");
    world.vent.pulse = 1;
    world.message = "工息照见风口，屋檐上层路线显形。";
    addLog("校准风口，屋檐回访线索浮现。");
    return;
  }
  world.message = "工息扫过雨线，弱点在伞骨开合的一瞬。";
  addLog("雨线显形：可弹反的攻势会带短促亮边。");
}

function collectSpot(spot) {
  addItem(spot.item, spot.qty);
  spot.taken = true;
  spot.cooldownLeft = spot.once ? Infinity : spot.cooldown;
  world.message = spot.message;
  addLog(`${spot.name}：获得${INVENTORY_DEFS[spot.item].name} x${spot.qty}。`);
  setPanel("inventory");
}

function talkToNpc(npc) {
  const period = getPeriod().id;
  const bucket = npc.lines[period] ?? (period === "evening" ? npc.lines.evening : null) ?? npc.lines.default;
  const index = world.talkIndex[npc.id] ?? 0;
  const line = bucket[index % bucket.length];
  world.talkIndex[npc.id] = index + 1;
  world.message = `${npc.name}：${line}`;
  addLog(world.message);
  if (npc.id === "market_runner" && getClockMinutes() >= 17 * 60 && !hasItem("orderSlip")) {
    addItem("orderSlip", 1);
    addLog("夜市跑单人递来一张夜市凭单。");
  }
  if (npc.id === "alley_resident" && !world.flags.residentGift) {
    world.flags.residentGift = true;
    addItem("bamboo", 1);
    applyFourEyeDelta({ dailyLife: 4 });
    addLog("阿棠送来一段备用青篾，民用评价上升。");
  }
  setPanel("npcs");
}

function resetDemo() {
  world = createWorld();
  elements.cover.classList.remove("hidden");
  elements.rushOrderButton.classList.remove("active");
  elements.fullProcessButton.classList.remove("active");
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
  if (!hit) world.message = action === "spin" ? "伞旋扫开雨幕，但没有命中伞影。" : "纸刃落空，雨魇仍在游移。";
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
      if (enemy.disorder <= 0) addLog(`${enemy.label}被收风卷出破绽。`);
    }
  }
  world.message = hit ? "收风卷起雨幕，破绽显形。" : "收风扫过空巷，风息在雨面散开。";
}

function borrowWind() {
  if (!spend("dash")) return;
  const towardVent = dist(world.player, world.vent) < 240;
  const target = towardVent ? { x: 1142, y: 246 } : { x: world.player.x + world.player.facing * 116, y: world.player.y - 26 };
  world.player.x = clamp(lerp(world.player.x, target.x, 0.74), 170, 1430);
  world.player.y = clamp(lerp(world.player.y, target.y, 0.74), 190, 720);
  world.player.action = "dash";
  world.player.actionTimer = 0.3;
  world.restore = Math.max(world.restore, 2);
  if (towardVent) {
    world.flags.roofRouteSeen = true;
    world.discovered.add("roof_route");
    world.currentLocationId = "roof_route";
    world.message = "借风越过低檐，屋檐上层路线已显形。";
    addLog("借风越行成功，风口标记已校准。");
  } else {
    world.message = "伞面借风，身位重整。";
  }
}

function update(dt) {
  world.time += dt;
  world.weather += dt * 0.8;
  if (!world.started) {
    syncNpcSchedules();
    world.context = getContextTarget().label;
    syncUi();
    return;
  }
  world.totalMinutes += dt * TIME_SCALE;
  world.vent.pulse = Math.max(0, world.vent.pulse - dt * 1.8);
  syncNpcSchedules();
  for (const action of Object.keys(world.cooldowns)) world.cooldowns[action] = Math.max(0, world.cooldowns[action] - dt);
  for (const spot of world.gatherSpots) {
    if (Number.isFinite(spot.cooldownLeft)) {
      spot.cooldownLeft = Math.max(0, spot.cooldownLeft - dt * TIME_SCALE);
      if (spot.cooldownLeft === 0) spot.taken = false;
    }
  }
  updatePlayer(dt);
  updateEnemies(dt);
  discoverNearbyNodes();
  if (world.enemies.every((enemy) => enemy.restored)) {
    world.restore = 5;
    world.message = "雨巷初段归位，纸伞门径通过首轮检验。";
    world.objective = "返回纸伞铺完成《天工录》落款，或打开断桥雨幕继续扩展。";
  }
  world.context = getContextTarget().label;
  syncUi();
}

function updatePlayer(dt) {
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
  if (wantsDash && (mx || my)) player.stamina = clamp(player.stamina - dt * 24, 0, 100);
  else if (!player.guard) player.stamina = clamp(player.stamina + dt * 18, 0, 100);
  if (player.guard) {
    player.stamina = clamp(player.stamina - dt * 8, 0, 100);
    player.wind = clamp(player.wind + dt * 5, 0, 100);
  }
  player.x = clamp(player.x + (mx / len) * speed * dt, 170, 1430);
  player.y = clamp(player.y + (my / len) * speed * dt, 190, 720);
  if (mx) player.facing = Math.sign(mx);
  player.actionTimer = Math.max(0, player.actionTimer - dt);
  if (player.actionTimer === 0 && player.action !== "idle") player.action = player.guard ? "guard" : "idle";
}

function updateEnemies(dt) {
  const player = world.player;
  for (const enemy of world.enemies) {
    if (enemy.restored || enemy.disorder <= 0) continue;
    enemy.attackCooldown = Math.max(0, enemy.attackCooldown - dt);
    enemy.attackFlash = Math.max(0, enemy.attackFlash - dt);
    if (enemy.windup > 0) {
      enemy.windup -= dt;
      if (enemy.windup <= 0) resolveEnemyAttack(enemy);
      continue;
    }
    const d = dist(player, enemy);
    if (d > enemy.aggroRange) {
      enemy.x = lerp(enemy.x, enemy.homeX + Math.sin(world.time + enemy.phase) * 18, dt * 1.4);
      enemy.y = lerp(enemy.y, enemy.homeY + Math.cos(world.time * 0.7 + enemy.phase) * 10, dt * 1.4);
      continue;
    }
    if (d < 76 && enemy.attackCooldown <= 0) {
      enemy.windup = 0.72;
      enemy.attackCooldown = 2.4;
      world.message = `${enemy.label}压低伞面，攻击雨线正在亮起。`;
      continue;
    }
    const angle = Math.atan2(player.y - enemy.y, player.x - enemy.x);
    const sway = Math.sin(world.time * 2 + enemy.phase) * 0.5;
    enemy.x += Math.cos(angle + sway * 0.2) * dt * 20;
    enemy.y += Math.sin(angle) * dt * 15;
  }
}

function resolveEnemyAttack(enemy) {
  const player = world.player;
  enemy.attackFlash = 0.24;
  if (dist(player, enemy) > 72) return;
  if (player.guard && player.stamina > 0) {
    player.wind = clamp(player.wind + 14, 0, 100);
    player.stamina = clamp(player.stamina - 8, 0, 100);
    enemy.disorder = clamp(enemy.disorder - 11, 0, enemy.max);
    world.message = "架伞稳住雨针，风息正在积累。";
    if (enemy.disorder <= 0) addLog(`${enemy.label}被架伞定住。`);
  } else {
    player.vitality = clamp(player.vitality - enemy.damage, 25, 100);
    world.message = `${enemy.label}的雨针擦过伞肩，生机下降。`;
  }
}

function syncUi() {
  const player = world.player;
  elements.vitalityMeter.style.width = `${player.vitality}%`;
  elements.staminaMeter.style.width = `${player.stamina}%`;
  elements.vitalityText.textContent = `${Math.round(player.vitality)}/100`;
  elements.staminaText.textContent = `${Math.round(player.stamina)}/100`;
  elements.timeText.textContent = formatClock();
  const period = getPeriod();
  elements.timePeriod.textContent = `${period.name} · ${period.tone}`;
  const location = getLocationById(world.currentLocationId);
  elements.locationText.textContent = location ? location.name : "雨巷";
  elements.weatherText.textContent = getWeatherText();
  [...elements.windPips.children].forEach((pip, index) => pip.classList.toggle("filled", index < Math.round(player.wind / 12.5)));
  [...elements.restoreRow.children].forEach((pip, index) => pip.classList.toggle("filled", index < world.restore));
  for (const [key, ui] of Object.entries(elements.fourEyes)) {
    const value = world.fourEyes[key];
    ui.meter.style.width = `${value}%`;
    ui.text.textContent = Math.round(value);
  }
  elements.objective.textContent = world.objective;
  elements.stateLine.textContent = world.message;
  elements.contextPrompt.querySelector("span").textContent = world.context;
  for (const button of elements.hotbarButtons) {
    const action = button.dataset.action;
    const cooldown = world.cooldowns[action] ?? 0;
    button.classList.toggle("active", action === world.player.action);
    button.classList.toggle("cooling", cooldown > 0.05);
    button.dataset.cooldown = cooldown > 0.05 ? `${cooldown.toFixed(1)}s` : "";
  }
  renderMiniMap();
  renderMapPanel();
  renderInventory();
  renderNpcs();
  renderJournal();
}

function renderMiniMap() {
  const markers = [];
  for (const node of MAP_NODES) {
    if (!isNodeVisible(node) || !world.discovered.has(node.id)) continue;
    const marker = document.createElement("i");
    marker.className = `mini-node ${node.id === world.currentLocationId ? "current" : ""} ${isNodeUnlocked(node) ? "" : "locked"}`;
    marker.style.left = `${(node.x / world.width) * 100}%`;
    marker.style.top = `${(node.y / world.height) * 100}%`;
    marker.title = node.name;
    markers.push(marker);
  }
  const player = document.createElement("i");
  player.className = "mini-player";
  player.style.left = `${(world.player.x / world.width) * 100}%`;
  player.style.top = `${(world.player.y / world.height) * 100}%`;
  markers.push(player);
  elements.miniMapNodes.replaceChildren(...markers);
}

function renderMapPanel() {
  const visibleNodes = MAP_NODES.filter((node) => isNodeVisible(node) && world.discovered.has(node.id));
  const nodeButtons = visibleNodes.map((node) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `map-node ${node.id === world.currentLocationId ? "current" : ""} ${isNodeUnlocked(node) ? "" : "locked"}`;
    button.dataset.node = node.id;
    button.style.left = `${(node.x / world.width) * 100}%`;
    button.style.top = `${(node.y / world.height) * 100}%`;
    button.innerHTML = `<b>${node.name.slice(0, 2)}</b><span>${node.type}</span>`;
    return button;
  });
  elements.mapBoard.replaceChildren(...nodeButtons);
  const location = getLocationById(world.currentLocationId);
  const locked = visibleNodes.filter((node) => !isNodeUnlocked(node)).map((node) => node.name);
  elements.mapDetails.textContent = location
    ? `${location.name}：${location.description}${locked.length ? ` / 封锁：${locked.join("、")}` : ""}`
    : "雨巷地图正在校准。";
}

function renderInventory() {
  const rows = Object.entries(INVENTORY_DEFS).map(([id, item]) => {
    const count = world.inventory[id] ?? 0;
    const row = document.createElement("div");
    row.className = `item-row ${count ? "" : "empty"}`;
    const usable = item.usable && count > 0;
    row.innerHTML = `
      <div>
        <b>${item.name}</b>
        <span>${item.type} · ${item.description}</span>
      </div>
      <strong>x${count}</strong>
      ${usable ? `<button type="button" data-use-item="${id}">使用</button>` : ""}
    `;
    return row;
  });
  elements.inventoryList.replaceChildren(...rows);
}

function renderNpcs() {
  const rows = world.npcs.map((npc) => {
    const row = document.createElement("div");
    row.className = `npc-row ${npc.active ? "" : "inactive"}`;
    const near = npc.active && dist(world.player, npc) < 86;
    row.innerHTML = `
      <div>
        <b>${npc.name}</b>
        <span>${npc.roles.join(" / ")} · ${npc.active ? npc.place : "暂不可见"}</span>
      </div>
      <strong>${near ? "可交谈" : `信任 ${npc.trust}`}</strong>
    `;
    return row;
  });
  elements.npcList.replaceChildren(...rows);
}

function renderJournal() {
  elements.eventLog.replaceChildren(
    ...world.log.map((entry) => {
      const item = document.createElement("li");
      item.textContent = entry;
      return item;
    })
  );
}

function getCamera() {
  const viewHeight = 720;
  const viewWidth = viewHeight * (canvas.clientWidth / Math.max(1, canvas.clientHeight));
  const width = Math.min(world.width, viewWidth);
  const height = Math.min(world.height, viewHeight);
  return {
    x: clamp(world.player.x - width * 0.5, 0, world.width - width),
    y: clamp(world.player.y - height * 0.52, 0, world.height - height),
    width,
    height
  };
}

function withWorldTransform(draw) {
  const camera = getCamera();
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  const scale = Math.min(w / camera.width, h / camera.height);
  const ox = (w - camera.width * scale) / 2;
  const oy = (h - camera.height * scale) / 2;
  ctx.save();
  ctx.translate(ox, oy);
  ctx.scale(scale, scale);
  ctx.translate(-camera.x, -camera.y);
  draw();
  ctx.restore();
}

function draw() {
  withWorldTransform(() => {
    drawScene();
    drawEnemyWarnings();
    drawEntities();
    drawWeather();
    drawContextHalo();
    drawVignette();
  });
}

function drawScene() {
  const period = getPeriod().id;
  const gradient = ctx.createLinearGradient(0, 0, 0, world.height);
  gradient.addColorStop(0, period === "night" ? "#07141a" : "#10252d");
  gradient.addColorStop(0.48, "#13242a");
  gradient.addColorStop(1, "#081216");
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, world.width, world.height);
  drawCanal();
  drawStonePath();
  drawBridge();
  drawShop();
  drawBambooGrove();
  drawNightMarket();
  drawRoofRoute();
  drawRainCurtain();
  drawLanterns();
  drawWindVent();
  drawGatherSpots();
  drawForeground();
}

function drawCanal() {
  ctx.save();
  ctx.fillStyle = "#0b3640";
  ctx.beginPath();
  ctx.moveTo(0, 160);
  ctx.bezierCurveTo(225, 270, 330, 292, 515, 388);
  ctx.bezierCurveTo(390, 492, 292, 612, 0, 760);
  ctx.lineTo(0, 160);
  ctx.fill();
  const water = ctx.createLinearGradient(0, 180, 520, 660);
  water.addColorStop(0, "rgba(70, 164, 183, 0.25)");
  water.addColorStop(1, "rgba(5, 26, 34, 0.45)");
  ctx.fillStyle = water;
  ctx.fill();
  ctx.strokeStyle = "rgba(126, 206, 218, 0.24)";
  ctx.lineWidth = 2;
  for (let i = 0; i < 24; i += 1) {
    const x = 30 + i * 34 + Math.sin(world.time + i) * 8;
    const y = 226 + (i % 10) * 42;
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
  ctx.moveTo(306, 260);
  ctx.lineTo(986, 226);
  ctx.lineTo(1286, 438);
  ctx.lineTo(1380, 636);
  ctx.lineTo(570, 736);
  ctx.lineTo(220, 620);
  ctx.lineTo(222, 418);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = "rgba(230, 236, 217, 0.08)";
  ctx.lineWidth = 1;
  for (let x = 240; x < 1380; x += 44) {
    ctx.beginPath();
    ctx.moveTo(x, 244);
    ctx.lineTo(x - 150, 744);
    ctx.stroke();
  }
  for (let y = 250; y < 742; y += 34) {
    ctx.beginPath();
    ctx.moveTo(245, y);
    ctx.lineTo(1400, y - 58);
    ctx.stroke();
  }
  ctx.fillStyle = "rgba(232, 181, 94, 0.18)";
  for (let i = 0; i < 28; i += 1) {
    const x = 420 + ((i * 97) % 850);
    const y = 282 + ((i * 53) % 390);
    ctx.beginPath();
    ctx.ellipse(x, y, 22, 5, -0.2, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();
}

function drawBridge() {
  ctx.save();
  ctx.translate(142, 652);
  ctx.rotate(-0.12);
  ctx.strokeStyle = "#152426";
  ctx.lineWidth = 28;
  ctx.beginPath();
  ctx.arc(160, 0, 190, Math.PI * 1.04, Math.PI * 1.96);
  ctx.stroke();
  ctx.strokeStyle = "#4b4a3f";
  ctx.lineWidth = 17;
  ctx.beginPath();
  ctx.arc(160, 0, 190, Math.PI * 1.04, Math.PI * 1.96);
  ctx.stroke();
  ctx.strokeStyle = "rgba(234, 216, 166, 0.3)";
  ctx.lineWidth = 2;
  for (let i = 0; i < 12; i += 1) {
    ctx.beginPath();
    ctx.moveTo(i * 34, -132 + (i % 2) * 4);
    ctx.lineTo(i * 34 - 24, -48);
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
  for (let x = 640; x < 1030; x += 28) ctx.fillRect(x, 120 + Math.sin(x) * 4, 24, 92);
  ctx.fillStyle = "#7a4f29";
  ctx.fillRect(668, 216, 330, 120);
  ctx.fillStyle = "rgba(239, 178, 85, 0.28)";
  ctx.fillRect(686, 232, 292, 84);
  drawSign(640, 154, "听雨轩");
  drawUmbrella(734, 292, 54, "#efe2c7", -0.2);
  drawUmbrella(812, 278, 58, "#f2e5c8", 0.08);
  drawUmbrella(884, 302, 50, "#e6d8bf", -0.12);
  ctx.restore();
}

function drawBambooGrove() {
  ctx.save();
  ctx.fillStyle = "rgba(31, 70, 50, 0.58)";
  ctx.beginPath();
  ctx.moveTo(1110, 330);
  ctx.lineTo(1508, 280);
  ctx.lineTo(1518, 570);
  ctx.lineTo(1150, 610);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = "rgba(128, 194, 119, 0.48)";
  ctx.lineWidth = 5;
  for (let i = 0; i < 32; i += 1) {
    const x = 1130 + (i * 23) % 360;
    const top = 260 + (i % 5) * 12;
    ctx.beginPath();
    ctx.moveTo(x, top);
    ctx.bezierCurveTo(x - 18, top + 100, x + 24, top + 210, x - 8, top + 318);
    ctx.stroke();
  }
  drawAreaLabel(1195, 390, "竹林小径");
  ctx.restore();
}

function drawNightMarket() {
  ctx.save();
  const visible = getClockMinutes() >= 17 * 60 || getClockMinutes() < 2 * 60;
  ctx.globalAlpha = visible ? 1 : 0.48;
  for (let i = 0; i < 4; i += 1) {
    const x = 990 + i * 84;
    const y = 600 + (i % 2) * 18;
    ctx.fillStyle = "#2b2020";
    ctx.fillRect(x, y - 54, 66, 60);
    ctx.fillStyle = visible ? "rgba(226, 170, 76, 0.72)" : "rgba(126, 116, 88, 0.24)";
    ctx.fillRect(x + 8, y - 44, 50, 16);
    ctx.fillStyle = "#6b3c30";
    ctx.beginPath();
    ctx.moveTo(x - 8, y - 54);
    ctx.lineTo(x + 33, y - 86);
    ctx.lineTo(x + 74, y - 54);
    ctx.closePath();
    ctx.fill();
  }
  drawAreaLabel(1020, 656, "夜市口");
  ctx.restore();
}

function drawRoofRoute() {
  ctx.save();
  ctx.fillStyle = "rgba(36, 43, 39, 0.82)";
  ctx.beginPath();
  ctx.moveTo(1010, 210);
  ctx.lineTo(1370, 170);
  ctx.lineTo(1438, 258);
  ctx.lineTo(1066, 310);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = "rgba(234, 216, 166, 0.18)";
  ctx.lineWidth = 3;
  for (let x = 1040; x < 1390; x += 42) {
    ctx.beginPath();
    ctx.moveTo(x, 212);
    ctx.lineTo(x + 42, 284);
    ctx.stroke();
  }
  if (world.flags.roofRouteSeen) drawAreaLabel(1134, 196, "屋檐上层");
  ctx.restore();
}

function drawRainCurtain() {
  ctx.save();
  const open = world.flags.rainCurtainOpen;
  ctx.strokeStyle = open ? "rgba(132, 232, 224, 0.32)" : "rgba(147, 197, 210, 0.62)";
  ctx.lineWidth = open ? 2 : 4;
  for (let i = 0; i < 24; i += 1) {
    const x = 1335 + i * 7 + Math.sin(world.time * 3 + i) * 5;
    ctx.beginPath();
    ctx.moveTo(x, 128);
    ctx.lineTo(x - 24, 362);
    ctx.stroke();
  }
  ctx.fillStyle = open ? "rgba(129, 232, 220, 0.72)" : "rgba(220, 222, 206, 0.46)";
  ctx.font = "700 20px sans-serif";
  ctx.fillText(open ? "雨幕短开" : "断桥雨幕", 1302, 122);
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
  const nightGlow = getPeriod().id === "evening" || getPeriod().id === "night";
  for (const [x, y] of [[586, 528], [704, 234], [928, 268], [1058, 568], [1160, 622]]) {
    const glow = ctx.createRadialGradient(x, y, 4, x, y, nightGlow ? 86 : 58);
    glow.addColorStop(0, `rgba(239, 170, 70, ${nightGlow ? 0.56 : 0.32})`);
    glow.addColorStop(1, "rgba(239, 170, 70, 0)");
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(x, y, nightGlow ? 86 : 58, 0, Math.PI * 2);
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

function drawGatherSpots() {
  for (const spot of world.gatherSpots) {
    const available = !(spot.taken && spot.once) && spot.cooldownLeft <= 0;
    ctx.save();
    ctx.globalAlpha = available ? 1 : 0.32;
    ctx.translate(spot.x, spot.y);
    drawShadow(0, 18, 28, 9);
    ctx.fillStyle = available ? "#dcb665" : "#7f877f";
    ctx.beginPath();
    ctx.roundRect(-18, -22, 36, 32, 6);
    ctx.fill();
    ctx.strokeStyle = "rgba(45, 30, 18, 0.45)";
    ctx.strokeRect(-14, -18, 28, 24);
    ctx.fillStyle = "#f1e7dc";
    ctx.font = "700 13px sans-serif";
    ctx.fillText(spot.name.slice(0, 4), -24, -32);
    ctx.restore();
  }
}

function drawForeground() {
  ctx.save();
  ctx.fillStyle = "rgba(12, 24, 18, 0.8)";
  ctx.beginPath();
  ctx.moveTo(0, 790);
  ctx.bezierCurveTo(330, 736, 620, 842, 980, 770);
  ctx.lineTo(1600, 900);
  ctx.lineTo(0, 900);
  ctx.closePath();
  ctx.fill();
  ctx.restore();
}

function drawAreaLabel(x, y, text) {
  ctx.save();
  ctx.fillStyle = "rgba(8, 18, 20, 0.62)";
  ctx.beginPath();
  ctx.roundRect(x - 12, y - 23, text.length * 18 + 24, 32, 6);
  ctx.fill();
  ctx.fillStyle = "#ead49a";
  ctx.font = "700 18px sans-serif";
  ctx.fillText(text, x, y);
  ctx.restore();
}

function drawEnemyWarnings() {
  for (const enemy of world.enemies) {
    if (enemy.restored || enemy.disorder <= 0 || enemy.windup <= 0) continue;
    const pulse = 1 - enemy.windup / 0.72;
    ctx.save();
    ctx.strokeStyle = `rgba(230, 92, 82, ${0.35 + pulse * 0.35})`;
    ctx.fillStyle = `rgba(230, 92, 82, ${0.06 + pulse * 0.08})`;
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.ellipse(enemy.x, enemy.y + 18, 72 + pulse * 18, 26 + pulse * 8, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
    ctx.restore();
  }
}

function drawEntities() {
  const sorted = [
    ...world.enemies.map((enemy) => ({ type: "enemy", entity: enemy })),
    ...world.npcs.filter((npc) => npc.active).map((npc) => ({ type: "npc", entity: npc })),
    { type: "player", entity: world.player }
  ].sort((a, b) => a.entity.y - b.entity.y);
  for (const item of sorted) {
    if (item.type === "enemy") drawEnemy(item.entity);
    else if (item.type === "npc") drawNpc(item.entity);
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
  if (enemy.restored) ctx.globalAlpha = 0.42;
  if (enemy.attackFlash > 0) {
    ctx.shadowColor = "rgba(230, 92, 82, 0.8)";
    ctx.shadowBlur = 22;
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
  if (target.type === "npc") point = target.npc;
  if (target.type === "gather") point = target.spot;
  if (target.type === "vent") point = world.vent;
  if (target.type === "restore") point = target.enemy;
  if (target.type === "locked_node") point = target.node;
  if (target.type === "inspect") return;
  ctx.save();
  ctx.strokeStyle = target.type === "restore" ? "rgba(232, 200, 121, 0.72)" : "rgba(139, 226, 238, 0.62)";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.ellipse(point.x, point.y + 20, 54 + Math.sin(world.time * 4) * 4, 18, 0, 0, Math.PI * 2);
  ctx.stroke();
  ctx.restore();
}

function drawNpc(npc) {
  ctx.save();
  ctx.translate(npc.x, npc.y);
  drawShadow(0, 20, 36, 10);
  ctx.fillStyle = "#244152";
  ctx.fillRect(-12, -34, 24, 52);
  ctx.fillStyle = "#d8b88b";
  ctx.beginPath();
  ctx.arc(0, -42, 10, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = npc.color;
  ctx.font = "700 15px sans-serif";
  ctx.fillText(npc.name, 18, -34);
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
  for (let i = 0; i < 190; i += 1) {
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
  const camera = getCamera();
  const cx = camera.x + camera.width * 0.5;
  const cy = camera.y + camera.height * 0.5;
  const vignette = ctx.createRadialGradient(cx, cy, 180, cx, cy, 780);
  vignette.addColorStop(0, "rgba(0,0,0,0)");
  vignette.addColorStop(1, "rgba(0,0,0,0.62)");
  ctx.fillStyle = vignette;
  ctx.fillRect(camera.x, camera.y, camera.width, camera.height);
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
