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

const CRAFT_RECIPES = {
  wind_hungry_surface: {
    id: "trait.umbrella.wind_hungry_surface",
    label: "伞面吃风",
    actionLabel: "糊纸吃风",
    description: "加强纸面张力，完美架伞和收风更容易积累风息。",
    requiredItems: { bamboo: 1, rainPaper: 1, tungOil: 1 },
    craftFields: { paperTension: 82, oilCoverage: 58, dryingState: 68, trialScore: 76 },
    bonuses: { guardWind: 4, gatherDamage: 12, dashWindDiscount: 0, wetResistance: 4 },
    fourEyes: { inheritance: 3, ritualFaith: 1 },
    umbrellaFill: "#f3e8c8",
    npcReview: "唐油点头：这面纸吃风，雨重时能稳住架势。",
    productionRefs: ["trait.umbrella.wind_hungry_surface", "interaction.jiangnan_rain_alley.craft_paper_oil", "sfx.umbrella.open"]
  },
  stable_blue_dye: {
    id: "trait.umbrella.stable_blue_dye",
    label: "蓝染稳色",
    actionLabel: "上油稳色",
    description: "蓝染与桐油更均匀，湿滞地面和雨幕压迫更好处理。",
    requiredItems: { rainPaper: 1, tungOil: 1 },
    craftFields: { paperTension: 64, oilCoverage: 86, dryingState: 74, trialScore: 72 },
    bonuses: { guardWind: 1, gatherDamage: 4, dashWindDiscount: 3, wetResistance: 15 },
    fourEyes: { market: 2, ritualFaith: 3 },
    umbrellaFill: "#bedbec",
    variantId: "asset.variant.umbrella.blue_lantern",
    npcReview: "蓝文把伞面举到灯下：蓝色没有浮，夜市一眼能认出。",
    productionRefs: ["asset.variant.umbrella.blue_lantern", "trait.umbrella.stable_blue_dye", "interaction.jiangnan_rain_alley.blue_pattern"]
  },
  quick_order_finish: {
    id: "trait.umbrella.quick_frame",
    label: "急单省工",
    actionLabel: "急单试伞",
    description: "省下晾晒时间，交付更快，但伞旋稳定性稍差。",
    requiredItems: { bamboo: 1, tungOil: 1 },
    craftFields: { paperTension: 48, oilCoverage: 52, dryingState: 40, trialScore: 54 },
    bonuses: { guardWind: 0, gatherDamage: -6, dashWindDiscount: 1, wetResistance: -4 },
    fourEyes: { market: 4, inheritance: -2 },
    umbrellaFill: "#e5dcc2",
    npcReview: "沈雨皱眉：能赶货，但线脚偏松，打旧桥前最好返修。",
    productionRefs: ["trait.umbrella.quick_frame", "trait.umbrella.loose_thread", "sfx.umbrella.close"]
  }
};

const CRAFT_INTERACTION_HOTSPOT = {
  id: "interaction.jiangnan_rain_alley.craft_paper_oil",
  nodeId: "dye_paper_court",
  label: "糊纸上油",
  x: 1036,
  y: 458,
  radius: 132
};

const CONTENT_URLS = {
  region: "/content/regions/jiangnan_rain_alley.json",
  subregions: "/content/subregions/jiangnan_rain_alley_subregions.json",
  quests: "/content/quests/jiangnan_rain_alley_quests.json",
  npcs: "/content/npcs/rain_alley_m1_npcs.json",
  buildings: "/content/buildings/jiangnan_rain_alley_buildings.json",
  interactions: "/content/interactions/jiangnan_rain_alley_interactions.json",
  enemies: "/content/enemies/rain_alley_m1_enemies.json",
  boss: "/content/bosses/rain_alley_boss.json",
  assets: "/content/assets/jiangnan_rain_alley_assets.json",
  audio: "/content/audio/jiangnan_rain_alley_audio.json",
  largeAreas: "/content/large_areas/jiangnan_large_areas.json",
  editors: "/content/editors/jiangnan_editor_templates.json",
  localization: "/content/localization/zh-CN.json"
};

const CONTENT_NODE_BY_SUBREGION = {
  "subregion.jiangnan_rain_alley.water_entry": "water_entry",
  "subregion.jiangnan_rain_alley.town_awning": "town_awning",
  "subregion.jiangnan_rain_alley.main_water_lane": "main_alley",
  "subregion.jiangnan_rain_alley.umbrella_shop": "umbrella_shop",
  "subregion.jiangnan_rain_alley.bamboo_path": "bamboo_path",
  "subregion.jiangnan_rain_alley.dye_paper_court": "dye_paper_court",
  "subregion.jiangnan_rain_alley.wind_bell_lane": "wind_bell_lane",
  "subregion.jiangnan_rain_alley.night_market": "night_market",
  "subregion.jiangnan_rain_alley.old_bridge_lower": "old_bridge",
  "subregion.jiangnan_rain_alley.roof_route": "roof_route",
  "subregion.jiangnan_rain_alley.rain_curtain_bridge": "rain_curtain"
};

const STEP_NODE_HINTS = {
  "quest_step.jiangnan_rain_alley.arrive_by_water": "water_entry",
  "quest_step.jiangnan_rain_alley.check_delivery": "town_awning",
  "quest_step.jiangnan_rain_alley.learn_guard": "umbrella_shop",
  "quest_step.jiangnan_rain_alley.choose_bamboo": "bamboo_path",
  "quest_step.jiangnan_rain_alley.paper_and_oil": "dye_paper_court",
  "quest_step.jiangnan_rain_alley.market_decision": "night_market",
  "quest_step.jiangnan_rain_alley.find_rivet": "roof_route",
  "quest_step.jiangnan_rain_alley.old_bridge_trial": "old_bridge",
  "quest_step.jiangnan_rain_alley.boss_restoration": "rain_curtain",
  "quest_step.jiangnan_rain_alley.record_in_tiangong": "umbrella_shop"
};

const PRODUCTION_WHITEBOX_REFS = new Set([
  "subregion.jiangnan_rain_alley.water_entry",
  "subregion.jiangnan_rain_alley.town_awning",
  "subregion.jiangnan_rain_alley.main_water_lane",
  "subregion.jiangnan_rain_alley.umbrella_shop",
  "subregion.jiangnan_rain_alley.bamboo_path",
  "subregion.jiangnan_rain_alley.dye_paper_court",
  "subregion.jiangnan_rain_alley.wind_bell_lane",
  "subregion.jiangnan_rain_alley.night_market",
  "subregion.jiangnan_rain_alley.old_bridge_lower",
  "subregion.jiangnan_rain_alley.roof_route",
  "subregion.jiangnan_rain_alley.rain_curtain_bridge",
  "tile.water_lane",
  "tile.workshop",
  "tile.bamboo_path",
  "tile.dye_paper_court",
  "tile.night_market",
  "tile.old_bridge",
  "tile.rain_curtain_bridge",
  "building.jiangnan_rain_alley.umbrella_shop",
  "building.jiangnan_rain_alley.dock_shed",
  "building.jiangnan_rain_alley.dye_court",
  "building.jiangnan_rain_alley.market_ledger",
  "building.jiangnan_rain_alley.old_bridge_shed",
  "building.jiangnan_rain_alley.roof_store",
  "npc.master_shen_yu",
  "npc.market_runner",
  "npc.alley_resident",
  "enemy.rain_wraith",
  "enemy.broken_umbrella_shadow",
  "boss.rain_alley_broken_umbrella_obsession",
  "skill.umbrella.guard",
  "skill.umbrella.gather_wind",
  "skill.umbrella.borrow_wind",
  "skill.umbrella.umbrella_spin",
  "skill.umbrella.parry_counter",
  "resource.wind_breath",
  "ui.four_eyes",
  "ui.quest",
  "ui.skill",
  "ui.material",
  "ui.restoration",
  "asset.variant.umbrella.blue_lantern"
]);

for (const recipe of Object.values(CRAFT_RECIPES)) {
  PRODUCTION_WHITEBOX_REFS.add(recipe.id);
  for (const ref of recipe.productionRefs) PRODUCTION_WHITEBOX_REFS.add(ref);
}

const PRODUCTION_AUDIO_CUE_REFS = new Set([
  "region.jiangnan_rain_alley",
  "subregion.jiangnan_rain_alley.water_entry",
  "subregion.jiangnan_rain_alley.main_water_lane",
  "subregion.jiangnan_rain_alley.umbrella_shop",
  "subregion.jiangnan_rain_alley.night_market",
  "subregion.jiangnan_rain_alley.old_bridge_lower",
  "subregion.jiangnan_rain_alley.wind_bell_lane",
  "subregion.jiangnan_rain_alley.rain_curtain_bridge",
  "boss.rain_alley_broken_umbrella_obsession",
  "pathway.paper_umbrella",
  "skill.umbrella.guard",
  "skill.umbrella.parry_counter",
  "skill.umbrella.gather_wind",
  "skill.umbrella.borrow_wind",
  "interaction.jiangnan_rain_alley.bridge_boss_restore",
  "interaction.jiangnan_rain_alley.wind_bell_tuning",
  "trap.ringing_latch"
]);

let contentPack = {
  loaded: false,
  error: null,
  strings: {},
  region: null,
  subregions: [],
  mainlineSteps: [],
  sideQuests: [],
  npcs: [],
  buildings: [],
  interactions: [],
  enemies: [],
  boss: null,
  assets: null,
  audio: null,
  largeAreas: [],
  points: [],
  selfSupplyLoops: [],
  editors: []
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
    id: "town_awning",
    name: "镇口雨棚",
    type: "对话",
    x: 402,
    y: 534,
    spawn: { x: 420, y: 530 },
    description: "茶棚、验货桌和跑单人聚在一处，适合交付材料和确认伞单。",
    discoverRadius: 160
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
    id: "dye_paper_court",
    name: "染纸晒场",
    type: "工艺",
    x: 1008,
    y: 430,
    spawn: { x: 998, y: 432 },
    description: "晾纸架和桐油台连在风燥廊上，可验证伞面、上油和湿滞抗性。",
    discoverRadius: 170,
    hiddenUntilFlag: "dyeCourtSeen"
  },
  {
    id: "wind_bell_lane",
    name: "风铃巷",
    type: "机关",
    x: 1258,
    y: 328,
    spawn: { x: 1236, y: 348 },
    description: "风铃和漏音井会提示断桥雨幕的风向，需要收风和听音校准。",
    discoverRadius: 165,
    hiddenUntilFlag: "windBellLaneSeen"
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

function createBossState() {
  return {
    id: "boss.rain_alley_broken_umbrella_obsession",
    label: "雨巷破伞执念",
    active: false,
    restored: false,
    x: 1360,
    y: 282,
    disorder: 1000,
    max: 1000,
    phase: 0,
    pulse: 0,
    attackTimer: 2.6
  };
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
    flags: { roofRouteSeen: false, rainCurtainOpen: false, residentGift: false, dyeCourtSeen: false, windBellLaneSeen: false },
    discovered: new Set(["water_entry", "town_awning", "main_alley", "umbrella_shop", "old_bridge"]),
    visited: new Set(["umbrella_shop"]),
    activePanel: "map",
    currentLocationId: "umbrella_shop",
    largeMapFocusId: "large_area.jiangnan.misty_bamboo_alley",
    talkIndex: {},
    m1: { stepIndex: 0, completedStepIds: new Set(), loadedOnce: false },
    editorDraft: { selectedTemplateId: null, dirty: false, exportedText: "" },
    craft: {
      traitId: null,
      label: "未定伞面",
      paperTension: 0,
      oilCoverage: 0,
      dryingState: 0,
      trialScore: 0,
      bonuses: { guardWind: 0, gatherDamage: 0, dashWindDiscount: 0, wetResistance: 0 },
      umbrellaFill: "#efe7d3",
      npcReview: "唐油和蓝文还在等你到染纸晒场定伞面。"
    },
    productionCue: {
      label: "等待资源事件",
      refs: ["asset_group.jiangnan.skill_vfx", "audio_pack.jiangnan_rain_alley_m1"]
    },
    boss: createBossState(),
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
    largeMap: document.querySelector("#largeMapPanel"),
    editor: document.querySelector("#editorPanel"),
    m1: document.querySelector("#m1Panel"),
    journal: document.querySelector("#journalPanel")
  },
  miniMapNodes: document.querySelector("#miniMapNodes"),
  mapBoard: document.querySelector("#mapBoard"),
  mapDetails: document.querySelector("#mapDetails"),
  inventoryList: document.querySelector("#inventoryList"),
  craftBoard: document.querySelector("#craftBoard"),
  npcList: document.querySelector("#npcList"),
  largeMapBoard: document.querySelector("#largeMapBoard"),
  largeMapDetails: document.querySelector("#largeMapDetails"),
  editorStatus: document.querySelector("#editorStatus"),
  editorBoard: document.querySelector("#editorBoard"),
  exportEditorButton: document.querySelector("#exportEditorButton"),
  resetEditorButton: document.querySelector("#resetEditorButton"),
  contentStatus: document.querySelector("#contentStatus"),
  m1Summary: document.querySelector("#m1Summary"),
  productionBoard: document.querySelector("#productionBoard"),
  questTrack: document.querySelector("#questTrack"),
  bossReadout: document.querySelector("#bossReadout"),
  advanceM1Button: document.querySelector("#advanceM1Button"),
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
  if (key === "l") setPanel("largeMap");
  if (key === "o") setPanel("editor");
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
elements.advanceM1Button.addEventListener("click", advanceM1Flow);
elements.exportEditorButton.addEventListener("click", exportEditorDraft);
elements.resetEditorButton.addEventListener("click", resetEditorDraft);
elements.inventoryList.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-use-item]");
  if (button) useItem(button.dataset.useItem);
});
elements.craftBoard.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-craft-recipe]");
  if (button) applyCraftRecipe(button.dataset.craftRecipe);
});
elements.mapBoard.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-node]");
  if (button) travelToNode(button.dataset.node);
});
elements.largeMapBoard.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-large-area]");
  if (button) focusLargeArea(button.dataset.largeArea);
});
elements.editorBoard.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-editor-template]");
  if (button) selectEditorTemplate(button.dataset.editorTemplate);
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

async function fetchJson(url) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) throw new Error(`${url} ${response.status}`);
  return response.json();
}

async function loadContentPack() {
  try {
    const entries = await Promise.all(Object.entries(CONTENT_URLS).map(async ([key, url]) => [key, await fetchJson(url)]));
    const data = Object.fromEntries(entries);
    contentPack = {
      loaded: true,
      error: null,
      strings: data.localization.strings ?? {},
      region: data.region,
      subregions: data.subregions.subregions ?? [],
      mainlineSteps: data.quests.steps ?? [],
      sideQuests: data.quests.sideQuests ?? [],
      npcs: data.npcs.npcs ?? [],
      buildings: data.buildings.buildings ?? [],
      interactions: data.interactions.interactions ?? [],
      enemies: data.enemies.enemies ?? [],
      boss: data.boss,
      assets: data.assets,
      audio: data.audio,
      largeAreas: data.largeAreas.largeAreas ?? [],
      points: data.largeAreas.points ?? [],
      selfSupplyLoops: data.largeAreas.selfSupplyLoops ?? [],
      editors: data.editors.templates ?? [],
      editorContract: data.editors
    };
    applyContentPackToMap();
    syncEditorDraftAfterLoad();
    world.m1.loadedOnce = true;
    world.objective = "江南内容库已载入：可在“大地图”查看 6 区 36 点位，或在“编辑器”预览内容模板。";
    addLog(`载入江南内容库：${contentPack.subregions.length} 个旧子地区、${contentPack.largeAreas.length} 个大区、${contentPack.points.length} 个点位、${contentPack.editors.length} 个编辑器模板。`);
  } catch (error) {
    contentPack = { ...contentPack, loaded: false, error: error instanceof Error ? error.message : String(error) };
    world.message = `江南 M1 内容包读取失败：${contentPack.error}`;
    addLog(world.message);
  }
  syncUi();
}

function t(key, fallback = key) {
  return contentPack.strings[key] ?? fallback;
}

function applyContentPackToMap() {
  for (const subregion of contentPack.subregions) {
    const nodeId = CONTENT_NODE_BY_SUBREGION[subregion.id];
    const node = nodeId ? getLocationById(nodeId) : null;
    if (!node) continue;
    node.name = t(subregion.displayNameKey, node.name);
    node.description = `${subregion.purpose} / ${subregion.productionRole === "core" ? "M1核心" : subregion.productionRole}`;
    node.sourceSubregionId = subregion.id;
  }
}

function getCurrentMainlineStep() {
  return contentPack.mainlineSteps[world.m1.stepIndex] ?? null;
}

function completeStep(step) {
  world.m1.completedStepIds.add(step.id);
  world.m1.stepIndex = Math.min(world.m1.stepIndex + 1, Math.max(contentPack.mainlineSteps.length - 1, 0));
  addLog(`主线推进：${t(step.displayNameKey, step.id)}。`);
}

function unlockNode(id) {
  const node = getLocationById(id);
  if (!node) return;
  if (node.hiddenUntilFlag) world.flags[node.hiddenUntilFlag] = true;
  if (node.requiresFlag === "rainCurtainOpen") world.flags.rainCurtainOpen = true;
  world.discovered.add(id);
}

function advanceM1Flow() {
  if (!contentPack.loaded) {
    world.message = contentPack.error ? `内容包尚未可用：${contentPack.error}` : "江南 M1 内容包仍在读取。";
    addLog(world.message);
    setPanel("m1");
    return;
  }

  const step = getCurrentMainlineStep();
  if (!step) {
    world.message = "江南 M1 主线步骤已经全部走完。";
    setPanel("m1");
    return;
  }

  const nodeId = STEP_NODE_HINTS[step.id];
  if (nodeId) unlockNode(nodeId);

  if (step.id === "quest_step.jiangnan_rain_alley.choose_bamboo") {
    addItem("bamboo", 2);
    unlockNode("dye_paper_court");
  }
  if (step.id === "quest_step.jiangnan_rain_alley.paper_and_oil") {
    addItem("rainPaper", 1);
    addItem("tungOil", 1);
    unlockNode("wind_bell_lane");
    world.flags.windBellLaneSeen = true;
  }
  if (step.id === "quest_step.jiangnan_rain_alley.market_decision" && !world.choice) {
    choose("material");
  }
  if (step.id === "quest_step.jiangnan_rain_alley.find_rivet") {
    addItem("windBell", 1);
    world.flags.roofRouteSeen = true;
    world.flags.windBellLaneSeen = true;
    unlockNode("roof_route");
  }
  if (step.id === "quest_step.jiangnan_rain_alley.old_bridge_trial") {
    world.enemies.forEach((enemy) => {
      if (enemy.id === "broken_umbrella_shadow") enemy.disorder = 0;
    });
    unlockNode("old_bridge");
  }
  if (step.id === "quest_step.jiangnan_rain_alley.boss_restoration") {
    if (!world.flags.rainCurtainOpen) {
      if (!hasItem("windBell")) addItem("windBell", 1);
      useItem("windBell");
    }
    startBossEncounter();
    world.message = "断桥雨幕已打开：用纸刃、伞旋、收风和借风削减 Boss 失序，归零后按 F 归位。";
    setPanel("m1");
    return;
  }
  if (step.id === "quest_step.jiangnan_rain_alley.record_in_tiangong" && !world.boss.restored) {
    world.message = "还不能落款：先完成雨巷破伞执念的归位。";
    setPanel("m1");
    return;
  }

  completeStep(step);
  const nextNode = nodeId ? getLocationById(nodeId) : null;
  if (nextNode) travelToNode(nextNode.id);
  world.objective = getCurrentMainlineStep()
    ? `当前 M1 主线：${t(getCurrentMainlineStep().displayNameKey, getCurrentMainlineStep().id)}。`
    : "江南 M1 主线已经完成，等待后续可玩关卡扩展。";
  setPanel("m1");
}

function addLog(text) {
  world.log = [text, ...world.log.filter((entry) => entry !== text)].slice(0, 6);
}

function spend(action) {
  const config = ACTIONS[action];
  const windCost = getActionWindCost(action);
  if (world.cooldowns[action] > 0) {
    world.message = `${config.label}还在回势。`;
    return false;
  }
  if (config.stamina && world.player.stamina < config.stamina) {
    world.message = "气力不足，先稳住身位。";
    return false;
  }
  if (windCost && world.player.wind < windCost) {
    world.message = "风息不足，先架伞接雨势。";
    return false;
  }
  if (config.stamina) world.player.stamina = clamp(world.player.stamina - config.stamina, 0, 100);
  if (windCost) world.player.wind = clamp(world.player.wind - windCost, 0, 100);
  world.cooldowns[action] = config.cooldown;
  return true;
}

function getActionWindCost(action) {
  const base = ACTIONS[action]?.wind ?? 0;
  if (!base) return 0;
  if (action === "dash") return Math.max(0, base - (world.craft.bonuses.dashWindDiscount ?? 0));
  return base;
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
  } else if (kind === "full") {
    applyFourEyeDelta({ inheritance: 15, ritualFaith: 10, market: -5 });
    world.message = "完整工序已定：师谱与礼信更稳，雨巷修复更温润。";
    world.restore = Math.max(world.restore, 3);
    addLog("坚持完整试伞，沈雨点头记下一笔。");
  } else {
    applyFourEyeDelta({ market: 5, inheritance: 5, dailyLife: 5 });
    addItem("bamboo", 1);
    world.message = "折中补修已定：先补风折竹，再保夜市交付和旧桥稳定。";
    world.restore = Math.max(world.restore, 3);
    addLog("选择折中补修，阿青记下风折竹返修法。");
  }
  elements.rushOrderButton.classList.toggle("active", kind === "rush");
  elements.fullProcessButton.classList.toggle("active", kind === "full");
  setPanel(kind === "material" ? "m1" : "journal");
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
  emitProductionCue("试伞补片制作", ["asset.variant.umbrella.blue_lantern", "asset_group.jiangnan.skill_vfx", "sfx.umbrella.open"]);
  world.message = "试伞补片完成，伞面张力更稳。";
  addLog("用青篾、雨纹纸和桐油做成一枚试伞补片。");
  setPanel("inventory");
}

function applyCraftRecipe(recipeId) {
  const recipe = CRAFT_RECIPES[recipeId];
  if (!recipe) return;
  const missing = Object.entries(recipe.requiredItems)
    .filter(([itemId, qty]) => !hasItem(itemId, qty))
    .map(([itemId, qty]) => `${INVENTORY_DEFS[itemId]?.name ?? itemId} x${qty}`);
  if (missing.length) {
    world.message = `${recipe.actionLabel}还缺：${missing.join("、")}。`;
    addLog(`染纸晒场材料不足：${missing.join("、")}。`);
    setPanel("inventory");
    return;
  }

  for (const [itemId, qty] of Object.entries(recipe.requiredItems)) removeItem(itemId, qty);
  world.craft = {
    traitId: recipe.id,
    label: recipe.label,
    ...recipe.craftFields,
    bonuses: { ...recipe.bonuses },
    umbrellaFill: recipe.umbrellaFill,
    variantId: recipe.variantId ?? null,
    npcReview: recipe.npcReview
  };
  world.player.wind = clamp(world.player.wind + Math.round(recipe.craftFields.trialScore / 5), 0, 100);
  world.player.stamina = clamp(world.player.stamina + Math.max(0, Math.round(recipe.bonuses.wetResistance / 2)), 0, 100);
  applyFourEyeDelta(recipe.fourEyes);
  completeStepIds(["quest_step.jiangnan_rain_alley.paper_and_oil"]);
  world.m1.stepIndex = Math.max(world.m1.stepIndex, contentPack.mainlineSteps.findIndex((step) => step.id === "quest_step.jiangnan_rain_alley.market_decision"));
  world.flags.windBellLaneSeen = true;
  unlockNode("wind_bell_lane");
  emitProductionCue(recipe.actionLabel, recipe.productionRefs);
  world.message = `${recipe.label}完成：${recipe.npcReview}`;
  addLog(`染纸晒场完成${recipe.actionLabel}，获得 ${recipe.id}。`);
  setPanel("inventory");
}

function useItem(id) {
  if (id === "rainPatch") {
    if (!removeItem("rainPatch", 1)) return;
    world.player.vitality = clamp(world.player.vitality + 26, 0, 100);
    world.player.stamina = clamp(world.player.stamina + 18, 0, 100);
    emitProductionCue("试伞补片使用", ["asset_group.jiangnan.skill_vfx", "sfx.umbrella.close", "resource.wind_breath"]);
    world.message = "试伞补片贴合伞面，生机与气力回稳。";
    addLog("使用试伞补片，纸伞重新吃风。");
    return;
  }
  if (id === "windBell") {
    world.flags.rainCurtainOpen = true;
    world.discovered.add("rain_curtain");
    emitProductionCue("风铃机关校准", ["sfx.wind_bell.correct", "interaction.jiangnan_rain_alley.wind_bell_tuning", "asset_group.jiangnan.skill_vfx"]);
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
  if (node.id === "rain_curtain") startBossEncounter();
}

function startBossEncounter() {
  unlockNode("rain_curtain");
  const target = getLocationById("rain_curtain");
  if (target) {
    world.currentLocationId = "rain_curtain";
    world.visited.add("rain_curtain");
    world.player.x = target.spawn?.x ?? target.x;
    world.player.y = target.spawn?.y ?? target.y;
  }
  if (!world.boss.active && !world.boss.restored) {
    world.boss.active = true;
    world.boss.disorder = world.boss.max;
    world.boss.phase = 1;
    world.boss.attackTimer = 2.2;
    emitProductionCue("Boss 显形", ["asset_group.jiangnan.boss_sprites", "bgm.jiangnan_rain_alley.boss", "amb.rain.heavy"]);
    world.objective = "断桥雨幕已开：削减雨巷破伞执念的失序，归零后按 F 进行归位。";
    addLog("雨巷破伞执念显形，断桥雨幕进入 Boss 验收。");
  }
}

function getBossPhase() {
  if (world.boss.restored) return { id: "restoration", name: "归位", ratio: 0 };
  const ratio = world.boss.disorder / world.boss.max;
  if (ratio <= 0.4) return { id: "p3", name: "裂伞露核", ratio };
  if (ratio <= 0.7) return { id: "p2", name: "风口错位", ratio };
  return { id: "p1", name: "伞骨横雨", ratio };
}

function getCraftInteractionTarget() {
  const node = getLocationById(CRAFT_INTERACTION_HOTSPOT.nodeId);
  if (!node || !isNodeVisible(node) || !isNodeUnlocked(node)) return null;
  const point = { x: CRAFT_INTERACTION_HOTSPOT.x, y: CRAFT_INTERACTION_HOTSPOT.y };
  const nodePoint = node.spawn ?? node;
  const nearHotspot = dist(world.player, point) < CRAFT_INTERACTION_HOTSPOT.radius;
  const nearNode = dist(world.player, nodePoint) < CRAFT_INTERACTION_HOTSPOT.radius;
  if (!nearHotspot && !nearNode) return null;
  return {
    type: "craft",
    label: CRAFT_INTERACTION_HOTSPOT.label,
    point,
    interactionId: CRAFT_INTERACTION_HOTSPOT.id
  };
}

function damageBoss(amount, source, range = 190) {
  if (!world.boss.active || world.boss.restored || world.boss.disorder <= 0) return false;
  if (dist(world.player, world.boss) > range) return false;
  world.boss.disorder = clamp(world.boss.disorder - amount, 0, world.boss.max);
  world.boss.pulse = 1;
  const phase = getBossPhase();
  world.boss.phase = phase.id === "p3" ? 3 : phase.id === "p2" ? 2 : 1;
  emitProductionCue(`Boss ${phase.name}`, ["asset_group.jiangnan.boss_sprites", `bossphase.rain_alley_broken_umbrella_obsession.${phase.id}`, "bgm.jiangnan_rain_alley.boss"]);
  world.message = `${source}击中裂伞核，Boss 失序降至 ${Math.round(world.boss.disorder)}。`;
  if (world.boss.disorder <= 0) {
    world.message = "裂伞核已经安静，靠近按 F 完成归位。";
    addLog("雨巷破伞执念露出归位窗口。");
  }
  return true;
}

function restoreBoss() {
  world.boss.restored = true;
  world.boss.active = false;
  world.restore = 5;
  emitProductionCue("Boss 归位", ["asset_group.jiangnan.boss_sprites", "sfx.boss.core_restore", "mix.jiangnan_rain_alley.boss_to_restoration"]);
  completeStepIds(["quest_step.jiangnan_rain_alley.boss_restoration"]);
  world.m1.stepIndex = Math.max(world.m1.stepIndex, contentPack.mainlineSteps.findIndex((step) => step.id === "quest_step.jiangnan_rain_alley.record_in_tiangong"));
  world.objective = "雨巷破伞执念已经归位：返回天工录落款，查看江南 M1 数据闭环。";
  world.message = "断桥雨幕归位，旧桥雨声降了一层。";
  addLog("Boss 归位完成：归器礼触发，四维回声等待落款。");
  setPanel("m1");
}

function completeStepIds(stepIds) {
  for (const id of stepIds) world.m1.completedStepIds.add(id);
}

function getContextTarget() {
  const downedEnemy = world.enemies.find((enemy) => !enemy.restored && enemy.disorder <= 0 && dist(world.player, enemy) < 92);
  if (downedEnemy) return { type: "restore", label: `归位 ${downedEnemy.label}`, enemy: downedEnemy };
  if (world.boss.active && !world.boss.restored && world.boss.disorder <= 0 && dist(world.player, world.boss) < 170) {
    return { type: "boss_restore", label: "归位 雨巷破伞执念" };
  }
  const npc = world.npcs.find((item) => item.active && dist(world.player, item) < 86);
  if (npc) return { type: "npc", label: `交谈 ${npc.name}`, npc };
  const gatherSpot = world.gatherSpots.find((spot) => !(spot.taken && spot.once) && spot.cooldownLeft <= 0 && dist(world.player, spot) < spot.radius);
  if (gatherSpot) return { type: "gather", label: `拾取 ${gatherSpot.name}`, spot: gatherSpot };
  const craftTarget = getCraftInteractionTarget();
  if (craftTarget) return craftTarget;
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
    world.message = `${target.enemy.label}已归位，伞面余风回到工灯。`;
    addLog(`${target.enemy.label}归位，雨声退后一层。`);
    return;
  }
  if (target.type === "boss_restore") {
    restoreBoss();
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
  if (target.type === "craft") {
    openCraftInteraction(target);
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
    world.message = "工灯照见风口，屋檐上层路线显形。";
    addLog("校准风口，屋檐回访线索浮现。");
    return;
  }
  world.message = "工灯扫过雨线，弱点在伞骨开合的一瞬。";
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

function openCraftInteraction(target) {
  world.flags.dyeCourtSeen = true;
  world.currentLocationId = CRAFT_INTERACTION_HOTSPOT.nodeId;
  world.visited.add(CRAFT_INTERACTION_HOTSPOT.nodeId);
  emitProductionCue("染纸晒场工艺入口", [
    target.interactionId,
    "subregion.jiangnan_rain_alley.dye_paper_court",
    "building.jiangnan_rain_alley.dye_court"
  ]);
  world.message = "染纸晒场已聚焦：选择伞面工艺，立即验证战斗参数、外观和 NPC 评价。";
  addLog("染纸晒场交互：糊纸、上油、晾晒与试伞进入同一工艺白盒。");
  setPanel("inventory");
  if (!elements.craftBoard) return;
  elements.craftBoard.classList.add("is-highlighted");
  elements.craftBoard.scrollIntoView({ block: "nearest", behavior: "smooth" });
  window.setTimeout(() => elements.craftBoard?.classList.remove("is-highlighted"), 1400);
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
  emitProductionCue("架伞", ["skill.umbrella.guard", "sfx.umbrella.block", "asset_group.jiangnan.skill_vfx"]);
}

function lightStrike(action = "light") {
  if (!spend(action)) return;
  emitProductionCue(action === "spin" ? "伞旋" : "纸刃", [action === "spin" ? "skill.umbrella.umbrella_spin" : "skill.umbrella.parry_counter", "sfx.parry.wind", "asset_group.jiangnan.skill_vfx"]);
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
    hit = damageBoss(action === "spin" ? 72 : 42, ACTIONS[action].label, action === "spin" ? 176 : 132);
  }
  if (!hit) world.message = action === "spin" ? "伞旋扫开雨幕，但没有命中伞影。" : "纸刃落空，雨魇仍在游移。";
  world.player.action = action;
  world.player.actionTimer = action === "spin" ? 0.36 : 0.22;
}

function gatherWind() {
  if (!spend("wind")) return;
  emitProductionCue("收风", ["skill.umbrella.gather_wind", "sfx.gather_wind", "asset_group.jiangnan.skill_vfx"]);
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
  if (!hit) {
    hit = damageBoss(88 + (world.craft.bonuses.gatherDamage ?? 0), "收风", 310);
  }
  world.message = hit ? "收风卷起雨幕，破绽显形。" : "收风扫过空巷，风息在雨面散开。";
}

function borrowWind() {
  if (!spend("dash")) return;
  emitProductionCue("借风越行", ["skill.umbrella.borrow_wind", "sfx.borrow_wind", "asset_group.jiangnan.skill_vfx"]);
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
  damageBoss(64, "借风越行", 220);
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
  updateBoss(dt);
  discoverNearbyNodes();
  if (!world.boss.active && world.enemies.every((enemy) => enemy.restored)) {
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
    player.wind = clamp(player.wind + dt * (5 + (world.craft.bonuses.guardWind ?? 0) * 0.35), 0, 100);
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

function updateBoss(dt) {
  if (!world.boss.active || world.boss.restored) return;
  world.boss.pulse = Math.max(0, world.boss.pulse - dt * 2.2);
  world.boss.attackTimer = Math.max(0, world.boss.attackTimer - dt);
  const phase = getBossPhase();
  world.boss.phase = phase.id === "p3" ? 3 : phase.id === "p2" ? 2 : 1;
  if (world.boss.attackTimer > 0 || world.boss.disorder <= 0) return;
  const close = dist(world.player, world.boss) < 210;
  if (close && world.player.guard && world.player.stamina > 8) {
    world.player.wind = clamp(world.player.wind + 18 + (world.craft.bonuses.guardWind ?? 0), 0, 100);
    world.player.stamina = clamp(world.player.stamina - 10, 0, 100);
    damageBoss(world.boss.phase === 3 ? 64 : 42, "架伞弹雨", 240);
  } else if (close) {
    const wetMitigation = Math.max(0, world.craft.bonuses.wetResistance ?? 0) * 0.18;
    world.player.vitality = clamp(world.player.vitality - Math.max(4, (world.boss.phase === 3 ? 18 : 12) - wetMitigation), 20, 100);
    world.message = "雨幕坠压逼近，架伞或借风能稳住身位。";
  } else {
    world.message = "断桥雨幕在远处聚拢，靠近裂伞核才能削减失序。";
  }
  world.boss.attackTimer = world.boss.phase === 3 ? 1.6 : 2.2;
}

function resolveEnemyAttack(enemy) {
  const player = world.player;
  enemy.attackFlash = 0.24;
  if (dist(player, enemy) > 72) return;
  if (player.guard && player.stamina > 0) {
    player.wind = clamp(player.wind + 14 + (world.craft.bonuses.guardWind ?? 0), 0, 100);
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
  renderCraftBoard();
  renderNpcs();
  renderLargeMapPanel();
  renderEditorPanel();
  renderM1Panel();
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

function renderCraftBoard() {
  if (!elements.craftBoard) return;
  const craft = world.craft;
  const status = document.createElement("section");
  status.className = "craft-status";
  status.innerHTML = `
    <div>
      <b>${craft.label}</b>
      <span>${craft.traitId ?? "trait.pending.paper_surface"}</span>
    </div>
    <p>${craft.npcReview}</p>
    <dl>
      <div><dt>纸面张力</dt><dd>${craft.paperTension}</dd></div>
      <div><dt>上油覆盖</dt><dd>${craft.oilCoverage}</dd></div>
      <div><dt>晾晒状态</dt><dd>${craft.dryingState}</dd></div>
      <div><dt>试伞评分</dt><dd>${craft.trialScore}</dd></div>
    </dl>
  `;

  const options = document.createElement("div");
  options.className = "craft-options";
  for (const [id, recipe] of Object.entries(CRAFT_RECIPES)) {
    const button = document.createElement("button");
    const missing = Object.entries(recipe.requiredItems).filter(([itemId, qty]) => !hasItem(itemId, qty));
    button.type = "button";
    button.dataset.craftRecipe = id;
    button.disabled = missing.length > 0;
    button.className = craft.traitId === recipe.id ? "active" : "";
    const needs = Object.entries(recipe.requiredItems)
      .map(([itemId, qty]) => `${INVENTORY_DEFS[itemId]?.name ?? itemId}x${qty}`)
      .join(" / ");
    button.innerHTML = `
      <b>${recipe.label}</b>
      <span>${recipe.description}</span>
      <small>${needs}${missing.length ? " · 材料不足" : ""}</small>
    `;
    options.append(button);
  }

  elements.craftBoard.replaceChildren(status, options);
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

function renderLargeMapPanel() {
  if (!elements.largeMapBoard || !elements.largeMapDetails) return;
  if (!contentPack.loaded) {
    elements.largeMapBoard.replaceChildren(createProductionNotice("等待江南大地图内容库载入。"));
    elements.largeMapDetails.textContent = "六大区、点位和自给循环会从 /content/large_areas 读取。";
    return;
  }

  if (!world.largeMapFocusId || !contentPack.largeAreas.some((area) => area.id === world.largeMapFocusId)) {
    world.largeMapFocusId = contentPack.largeAreas[0]?.id ?? null;
  }

  const cards = contentPack.largeAreas.map((area) => {
    const pointCount = area.pointIds?.length ?? 0;
    const legacyCount = area.oldSubregionIds?.length ?? 0;
    const card = document.createElement("button");
    card.type = "button";
    card.className = `large-area-card ${area.id === world.largeMapFocusId ? "active" : ""}`;
    card.dataset.largeArea = area.id;
    card.style.setProperty("--area-color", area.themeColor ?? "#d6b56c");
    card.innerHTML = `
      <b>${t(area.displayNameKey, area.id)}</b>
      <span>${area.primaryDimensions.join(" / ")} · ${area.durationMinutes.join("-")} 分钟</span>
      <small>${pointCount} 点位 · ${legacyCount} 个 07 节点</small>
    `;
    return card;
  });
  elements.largeMapBoard.replaceChildren(...cards);
  renderLargeMapDetails();
}

function renderLargeMapDetails() {
  const activeArea = contentPack.largeAreas.find((area) => area.id === world.largeMapFocusId) ?? contentPack.largeAreas[0];
  if (!activeArea) {
    elements.largeMapDetails.textContent = "大地图数据为空。";
    return;
  }
  const pointRows = (activeArea.pointIds ?? [])
    .map((pointId) => contentPack.points.find((point) => point.id === pointId))
    .filter(Boolean)
    .map((point) => {
      const row = document.createElement("div");
      row.className = "point-row";
      row.innerHTML = `
        <div>
          <b>${t(point.displayNameKey, point.id)}</b>
          <span>${point.type} · ${point.gates.join(" / ")}</span>
        </div>
        <strong>${point.contentRefs.length} refs</strong>
      `;
      return row;
    });

  const loopRows = contentPack.selfSupplyLoops
    .filter((loop) => loop.largeAreaIds.includes(activeArea.id))
    .map((loop) => {
      const row = document.createElement("div");
      row.className = "loop-row";
      row.innerHTML = `
        <b>${t(loop.displayNameKey, loop.id)}</b>
        <span>${loop.steps.join(" -> ")}</span>
      `;
      return row;
    });

  const summary = document.createElement("section");
  summary.className = "large-area-detail-head";
  summary.innerHTML = `
    <b>${t(activeArea.displayNameKey, activeArea.id)}</b>
    <span>${activeArea.resourceLoop.inputs.join(" / ")} -> ${activeArea.resourceLoop.outputs.join(" / ")}</span>
    <small>迁移：${(activeArea.oldSubregionIds ?? []).join("、") || "13 新增大区"}</small>
  `;

  const points = document.createElement("section");
  points.className = "large-area-detail-section";
  points.innerHTML = "<h3>内部点位</h3>";
  points.append(...pointRows);

  const loops = document.createElement("section");
  loops.className = "large-area-detail-section";
  loops.innerHTML = "<h3>自给循环</h3>";
  loops.append(...loopRows.length ? loopRows : [createProductionNotice("该区自给循环待下一批细化。")]);

  elements.largeMapDetails.replaceChildren(summary, points, loops);
}

function focusLargeArea(id) {
  world.largeMapFocusId = id;
  const area = contentPack.largeAreas.find((item) => item.id === id);
  if (area) {
    world.message = `大地图聚焦：${t(area.displayNameKey, id)}，${area.pointIds.length} 个点位已入库。`;
    addLog(`查看大地图：${t(area.displayNameKey, id)}。`);
  }
  for (const card of elements.largeMapBoard.querySelectorAll("button[data-large-area]")) {
    card.classList.toggle("active", card.dataset.largeArea === id);
  }
  renderLargeMapDetails();
  elements.stateLine.textContent = world.message;
  renderJournal();
}

function syncEditorDraftAfterLoad() {
  if (!contentPack.editors.length) return;
  if (!world.editorDraft.selectedTemplateId) {
    world.editorDraft.selectedTemplateId = contentPack.editors[0].id;
  }
}

function selectEditorTemplate(id) {
  world.editorDraft.selectedTemplateId = id;
  world.editorDraft.dirty = true;
  const template = contentPack.editors.find((item) => item.id === id);
  if (template) {
    world.message = `编辑器模板：${t(template.displayNameKey, id)} 已选中。`;
    addLog(`选择编辑器模板：${t(template.displayNameKey, id)}。`);
  }
  refreshEditorPanelDetail();
  elements.stateLine.textContent = world.message;
  renderJournal();
}

function buildEditableContentModel(template) {
  return {
    schemaVersion: 1,
    id: `draft.${template.id.replace("editor_template.jiangnan.", "")}.example`,
    displayNameKey: `${template.id}.draft.name`,
    tags: ["draft", "editor_export", "jiangnan"],
    designerNote: `由 ${t(template.displayNameKey, template.id)} 生成的内存草稿，需通过校验后才可写入 content/。`,
    status: "draft",
    owner: "planning",
    sourceNote: "prototype/web-demo editor preview",
    targetObjects: template.targetObjects,
    requiredFields: template.requiredFields,
    validationRules: template.validationRules,
    outputTargets: template.outputTargets
  };
}

function exportEditorDraft() {
  const template = contentPack.editors.find((item) => item.id === world.editorDraft.selectedTemplateId);
  if (!template) return;
  const draft = buildEditableContentModel(template);
  world.editorDraft.exportedText = JSON.stringify(draft, null, 2);
  world.editorDraft.dirty = false;
  world.message = "编辑器草稿已导出到预览区；正式写入仍需走内容校验。";
  addLog(`导出编辑器草稿：${draft.id}。`);
  refreshEditorPanelDetail();
  elements.stateLine.textContent = world.message;
  renderJournal();
}

function resetEditorDraft() {
  world.editorDraft = {
    selectedTemplateId: contentPack.editors[0]?.id ?? null,
    dirty: false,
    exportedText: ""
  };
  world.message = "编辑器草稿已重置。";
  refreshEditorPanelDetail();
  elements.stateLine.textContent = world.message;
  renderJournal();
}

function getSelectedEditorTemplate() {
  return contentPack.editors.find((item) => item.id === world.editorDraft.selectedTemplateId) ?? contentPack.editors[0];
}

function createEditorDetail(selected) {
  const detail = document.createElement("section");
  detail.className = "editor-detail";
  detail.dataset.editorDetail = "true";
  if (selected) {
    const fields = selected.requiredFields.map((field) => `<code>${field}</code>`).join("");
    const rules = selected.validationRules.map((rule) => `<code>${rule}</code>`).join("");
    const groups = selected.fieldGroups
      .map((group) => `<li><b>${group.id}</b><span>${group.fields.join(" / ")}</span></li>`)
      .join("");
    detail.innerHTML = `
      <h3>${t(selected.displayNameKey, selected.id)}</h3>
      <p>${selected.outputTargets.join(" / ")}</p>
      <div class="editor-chip-row">${fields}</div>
      <div class="editor-chip-row rules">${rules}</div>
      <ul>${groups}</ul>
      <pre>${world.editorDraft.exportedText || JSON.stringify(buildEditableContentModel(selected), null, 2)}</pre>
    `;
  }
  return detail;
}

function refreshEditorPanelDetail() {
  if (!contentPack.loaded || !elements.editorBoard || !elements.editorStatus) return;
  const selected = getSelectedEditorTemplate();
  elements.editorStatus.textContent = `已载入 ${contentPack.editors.length} 类编辑器模板；输出格式：${contentPack.editorContract?.outputFormat ?? "json"}。`;
  for (const button of elements.editorBoard.querySelectorAll("button[data-editor-template]")) {
    button.classList.toggle("active", button.dataset.editorTemplate === selected?.id);
  }
  const existingDetail = elements.editorBoard.querySelector("[data-editor-detail]");
  const nextDetail = createEditorDetail(selected);
  if (existingDetail) {
    existingDetail.replaceWith(nextDetail);
  } else {
    elements.editorBoard.append(nextDetail);
  }
}

function renderEditorPanel() {
  if (!elements.editorBoard || !elements.editorStatus) return;
  if (!contentPack.loaded) {
    elements.editorStatus.textContent = "等待编辑器模板载入...";
    elements.editorBoard.replaceChildren(createProductionNotice("编辑器会读取 /content/editors/jiangnan_editor_templates.json。"));
    return;
  }
  syncEditorDraftAfterLoad();
  const selected = getSelectedEditorTemplate();
  elements.editorStatus.textContent = `已载入 ${contentPack.editors.length} 类编辑器模板；输出格式：${contentPack.editorContract?.outputFormat ?? "json"}。`;

  const templateList = document.createElement("div");
  templateList.className = "editor-template-list";
  for (const template of contentPack.editors) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.editorTemplate = template.id;
    button.className = template.id === selected?.id ? "active" : "";
    button.innerHTML = `
      <b>${t(template.displayNameKey, template.id)}</b>
      <span>${template.targetObjects.join(" / ")}</span>
    `;
    templateList.append(button);
  }

  elements.editorBoard.replaceChildren(templateList, createEditorDetail(selected));
}

function renderM1Panel() {
  elements.contentStatus.textContent = contentPack.loaded
    ? `已载入 ${contentPack.subregions.length} 个旧子地区、${contentPack.largeAreas.length} 个大区、${contentPack.points.length} 个点位。`
    : contentPack.error
      ? `内容包读取失败：${contentPack.error}`
      : "正在读取江南 M1 内容包...";

  const coreCount = contentPack.region?.coreSubregionIds?.length ?? 0;
  elements.m1Summary.replaceChildren(
    createInfoPill("核心节点", `${coreCount}/7`),
    createInfoPill("大区", `${contentPack.largeAreas.length}/6`),
    createInfoPill("点位", `${contentPack.points.length}`),
    createInfoPill("支线", `${contentPack.sideQuests.length}`),
    createInfoPill("编辑器", `${contentPack.editors.length}`),
    createInfoPill("音频", `${getAudioProductionCount()}`)
  );

  renderProductionBoard();

  const steps = contentPack.mainlineSteps.map((step, index) => {
    const row = document.createElement("div");
    const done = world.m1.completedStepIds.has(step.id);
    const active = index === world.m1.stepIndex && !done;
    row.className = `quest-step ${done ? "done" : ""} ${active ? "active" : ""}`;
    row.innerHTML = `<b>${index + 1}. ${t(step.displayNameKey, step.id)}</b><span>${step.subregionId ? t(`${step.subregionId}.name`, step.subregionId) : "江南雨巷"}</span>`;
    return row;
  });
  elements.questTrack.replaceChildren(...steps.slice(0, 10));

  const phase = getBossPhase();
  const bossPercent = Math.round((world.boss.disorder / world.boss.max) * 100);
  const bossProduction = getBossProductionState();
  elements.bossReadout.innerHTML = `
    <b>${world.boss.label}</b>
    <span>${world.boss.restored ? "已归位" : world.boss.active ? `${phase.name} · 失序 ${bossPercent}%` : "未显形"}</span>
    <span class="boss-resource">资源：${bossProduction.requiredState} · ${bossProduction.audioId}</span>
    <i><em style="width:${world.boss.restored ? 100 : 100 - bossPercent}%"></em></i>
  `;
}

function renderProductionBoard() {
  if (!elements.productionBoard) return;
  if (!contentPack.loaded) {
    elements.productionBoard.replaceChildren(createProductionNotice("资源生产接口等待 M1 内容包载入。"));
    return;
  }

  const assetGroups = contentPack.assets?.assetGroups ?? [];
  const audioItems = getAudioProductionItems();
  const assetDemand = assetGroups.reduce((sum, group) => sum + (group.requiredCount ?? group.referenceIds?.length ?? 0), 0);
  const audioReady = audioItems.filter((item) => hasAnyProductionRef(item.usageIds, PRODUCTION_AUDIO_CUE_REFS)).length;
  const bossState = getBossProductionState();

  const header = document.createElement("div");
  header.className = "production-summary";
  header.append(
    createProductionMetric("美术组", `${assetGroups.length}`),
    createProductionMetric("需求量", `${assetDemand}`),
    createProductionMetric("音频项", `${audioItems.length}`),
    createProductionMetric("已接白盒", `${audioReady}`)
  );

  const cue = document.createElement("div");
  cue.className = "production-cue";
  cue.textContent = `${world.productionCue.label}: ${world.productionCue.refs.join(" / ")}`;

  elements.productionBoard.replaceChildren(
    header,
    cue,
    createProductionSection(
      "美术资源",
      "白盒绘制已接入场景，真实路径待资源 manifest。",
      assetGroups.map((group) => createAssetProductionRow(group))
    ),
    createProductionSection(
      "音频资源",
      "当前只验证触发关系，不播放未入库文件。",
      audioItems.map((item) => createAudioProductionRow(item))
    ),
    createProductionSection(
      "Boss 阶段",
      "Canvas placeholder 与正式 Boss sprite 状态对齐。",
      [createBossProductionRow(bossState)]
    )
  );
}

function createProductionNotice(text) {
  const node = document.createElement("div");
  node.className = "production-notice";
  node.textContent = text;
  return node;
}

function createProductionMetric(label, value) {
  const node = document.createElement("span");
  node.className = "production-metric";
  const number = document.createElement("b");
  number.textContent = value;
  node.append(number, document.createTextNode(label));
  return node;
}

function createProductionSection(title, detail, rows) {
  const section = document.createElement("section");
  section.className = "production-section";
  const head = document.createElement("div");
  head.className = "production-section-head";
  const titleNode = document.createElement("b");
  titleNode.textContent = title;
  const detailNode = document.createElement("span");
  detailNode.textContent = detail;
  head.append(titleNode, detailNode);
  const list = document.createElement("div");
  list.className = "production-list";
  list.append(...rows);
  section.append(head, list);
  return section;
}

function createAssetProductionRow(group) {
  const coverage = getProductionCoverage(group.referenceIds, PRODUCTION_WHITEBOX_REFS);
  const required = group.requiredCount ?? coverage.total;
  const state = getProductionRowState(coverage.ready, required);
  const row = createProductionRow({
    name: t(group.displayNameKey, group.id),
    meta: `${group.assetType ?? "asset"} · ${group.status ?? "draft"} · ${group.owner ?? contentPack.assets?.owner ?? "art"}`,
    value: `${coverage.ready}/${required}`,
    detail: group.requiredStates?.length ? `states: ${group.requiredStates.join(", ")}` : `refs: ${coverage.ready}/${coverage.total}`,
    state
  });
  row.dataset.assetGroupId = group.id;
  return row;
}

function createAudioProductionRow(item) {
  const coverage = getProductionCoverage(item.usageIds, PRODUCTION_AUDIO_CUE_REFS);
  const row = createProductionRow({
    name: t(item.displayNameKey, item.id),
    meta: `${item.type ?? "audio"} · ${item.status ?? "draft"} · ${item.bucket}`,
    value: coverage.total ? `${coverage.ready}/${coverage.total}` : "mix",
    detail: item.from && item.to ? `${item.from} -> ${item.to}` : `usage: ${coverage.ready}/${coverage.total}`,
    state: coverage.ready > 0 || item.bucket === "mix" ? "whitebox" : "planned"
  });
  row.dataset.audioId = item.id;
  return row;
}

function createBossProductionRow(info) {
  const row = createProductionRow({
    name: info.name,
    meta: `${info.assetGroupId} · ${info.audioId}`,
    value: info.requiredState,
    detail: info.ready ? "requiredState 已在 boss_sprites 声明，当前仍使用 Canvas placeholder。" : "缺 Boss sprite requiredState 映射。",
    state: info.ready ? "whitebox" : "planned"
  });
  row.dataset.bossResource = info.requiredState;
  return row;
}

function createProductionRow({ name, meta, value, detail, state }) {
  const row = document.createElement("div");
  row.className = `production-row ${state}`;
  const main = document.createElement("div");
  const title = document.createElement("b");
  title.textContent = name;
  const metaNode = document.createElement("span");
  metaNode.textContent = meta;
  const detailNode = document.createElement("small");
  detailNode.textContent = detail;
  main.append(title, metaNode, detailNode);
  const badge = document.createElement("strong");
  badge.textContent = value;
  row.append(main, badge);
  return row;
}

function getAudioProductionItems() {
  const audio = contentPack.audio ?? {};
  return [
    ...(audio.tracks ?? []).map((item) => ({ ...item, bucket: "bgm" })),
    ...(audio.ambient ?? []).map((item) => ({ ...item, bucket: "ambient" })),
    ...(audio.sfx ?? []).map((item) => ({ ...item, bucket: "sfx" })),
    ...(audio.mixRules ?? []).map((item) => ({ ...item, bucket: "mix", type: "mix" }))
  ];
}

function getAudioProductionCount() {
  return getAudioProductionItems().length;
}

function getProductionCoverage(ids = [], readyRefs) {
  const unique = [...new Set(ids)];
  const ready = unique.filter((id) => readyRefs.has(id)).length;
  return { ready, total: unique.length };
}

function hasAnyProductionRef(ids = [], readyRefs) {
  return ids.some((id) => readyRefs.has(id));
}

function getProductionRowState(ready, required) {
  if (required <= 0 || ready <= 0) return "planned";
  return ready >= required ? "whitebox" : "partial";
}

function getBossProductionState() {
  const phase = getBossPhase();
  const requiredStateByPhase = {
    p1: "phase_1",
    p2: "phase_2",
    p3: "phase_3",
    restoration: "restoration"
  };
  const requiredState = requiredStateByPhase[phase.id] ?? "phase_1";
  const bossGroup = contentPack.assets?.assetGroups?.find((group) => group.id === "asset_group.jiangnan.boss_sprites");
  return {
    name: `${world.boss.label} · ${phase.name}`,
    assetGroupId: bossGroup?.id ?? "asset_group.jiangnan.boss_sprites",
    audioId: world.boss.active ? "bgm.jiangnan_rain_alley.boss" : "sfx.boss.core_restore",
    requiredState,
    ready: Boolean(bossGroup?.requiredStates?.includes(requiredState))
  };
}

function emitProductionCue(label, refs) {
  world.productionCue = { label, refs };
}

function createInfoPill(label, value) {
  const pill = document.createElement("span");
  pill.className = "info-pill";
  pill.innerHTML = `<b>${value}</b>${label}`;
  return pill;
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
  drawDyePaperCourt();
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

function drawDyePaperCourt() {
  ctx.save();
  const visible = world.flags.dyeCourtSeen || world.discovered.has("dye_paper_court");
  ctx.globalAlpha = visible ? 1 : 0.44;
  ctx.fillStyle = "rgba(42, 54, 55, 0.82)";
  ctx.beginPath();
  ctx.moveTo(914, 374);
  ctx.lineTo(1100, 350);
  ctx.lineTo(1154, 476);
  ctx.lineTo(954, 510);
  ctx.closePath();
  ctx.fill();

  ctx.fillStyle = "rgba(81, 128, 144, 0.28)";
  ctx.fillRect(940, 392, 72, 46);
  ctx.fillStyle = world.craft.variantId === "asset.variant.umbrella.blue_lantern" ? "rgba(89, 157, 205, 0.72)" : "rgba(214, 225, 202, 0.5)";
  for (let i = 0; i < 4; i += 1) {
    ctx.beginPath();
    ctx.roundRect(972 + i * 34, 368 + (i % 2) * 18, 26, 82, 4);
    ctx.fill();
  }

  ctx.strokeStyle = "rgba(235, 216, 166, 0.32)";
  ctx.lineWidth = 4;
  for (let i = 0; i < 3; i += 1) {
    ctx.beginPath();
    ctx.moveTo(938 + i * 78, 384);
    ctx.lineTo(984 + i * 78, 500);
    ctx.stroke();
  }

  ctx.fillStyle = "rgba(219, 174, 86, 0.32)";
  ctx.beginPath();
  ctx.ellipse(1088, 440, 38, 18, -0.2, 0, Math.PI * 2);
  ctx.fill();
  drawUmbrella(1050, 474, 42, world.craft.umbrellaFill, -0.16);
  ctx.strokeStyle = "rgba(139, 226, 238, 0.68)";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.ellipse(CRAFT_INTERACTION_HOTSPOT.x, CRAFT_INTERACTION_HOTSPOT.y + 24, 46, 14, 0, 0, Math.PI * 2);
  ctx.stroke();
  drawAreaLabel(962, 356, "染纸晒场");
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
  const intensity = world.boss.active ? 1 : open ? 0.55 : 0.78;
  const coreX = world.boss.x;
  const coreY = world.boss.y - 30;
  const glow = ctx.createRadialGradient(coreX, coreY, 12, coreX, coreY, 230);
  glow.addColorStop(0, world.boss.active ? "rgba(224, 117, 92, 0.32)" : "rgba(132, 232, 224, 0.18)");
  glow.addColorStop(0.42, "rgba(85, 166, 184, 0.16)");
  glow.addColorStop(1, "rgba(8, 18, 24, 0)");
  ctx.fillStyle = glow;
  ctx.fillRect(1190, 88, 360, 340);

  for (let layer = 0; layer < 3; layer += 1) {
    ctx.strokeStyle = open
      ? `rgba(132, 232, 224, ${0.14 + layer * 0.08})`
      : `rgba(147, 197, 210, ${0.28 + layer * 0.14})`;
    ctx.lineWidth = open ? 1.5 + layer * 0.5 : 2.5 + layer;
    for (let i = 0; i < 34; i += 1) {
      const x = 1298 + i * 7 + Math.sin(world.time * (2.8 + layer) + i) * (5 + layer * 3);
      const y = 104 + layer * 12 + Math.cos(world.time + i) * 8;
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.bezierCurveTo(x - 12, y + 82, x - 30, y + 168, x - 38 - layer * 8, y + 268);
      ctx.stroke();
    }
  }

  drawBrokenUmbrellaCore(coreX, coreY, intensity);
  if (world.boss.active || world.boss.restored) drawBossApparition(coreX, coreY);
  ctx.fillStyle = open ? "rgba(129, 232, 220, 0.72)" : "rgba(220, 222, 206, 0.46)";
  ctx.font = "700 20px sans-serif";
  ctx.fillText(world.boss.active ? getBossPhase().name : open ? "雨幕短开" : "断桥雨幕", 1302, 122);
  ctx.restore();
}

function drawBrokenUmbrellaCore(x, y, intensity) {
  ctx.save();
  ctx.translate(x, y);
  ctx.rotate(Math.sin(world.time * 0.7) * 0.04);
  const radius = 76 + Math.sin(world.time * 2.2) * 4;
  ctx.strokeStyle = `rgba(238, 223, 180, ${0.2 + intensity * 0.32})`;
  ctx.lineWidth = 5;
  ctx.beginPath();
  ctx.ellipse(0, 0, radius, 28, 0, Math.PI, 0);
  ctx.stroke();
  ctx.lineWidth = 2;
  for (let i = -4; i <= 4; i += 1) {
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(i * 18, -24 - Math.abs(i) * 5);
    ctx.stroke();
  }
  ctx.fillStyle = world.boss.active ? "rgba(218, 91, 76, 0.72)" : "rgba(126, 232, 220, 0.62)";
  ctx.beginPath();
  ctx.ellipse(0, -4, 22 + world.boss.pulse * 12, 11 + world.boss.pulse * 4, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function drawBossApparition(x, y) {
  ctx.save();
  ctx.translate(x, y + 42);
  const phase = getBossPhase();
  const restoredAlpha = world.boss.restored ? 0.34 : 1;
  ctx.globalAlpha = restoredAlpha;
  const body = ctx.createRadialGradient(0, 0, 8, 0, 0, 132);
  body.addColorStop(0, phase.id === "p3" ? "rgba(230, 99, 82, 0.64)" : "rgba(142, 214, 219, 0.38)");
  body.addColorStop(1, "rgba(19, 29, 32, 0)");
  ctx.fillStyle = body;
  ctx.beginPath();
  ctx.ellipse(0, 28, 98, 132, Math.sin(world.time) * 0.08, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = phase.id === "p3" ? "rgba(244, 190, 130, 0.62)" : "rgba(178, 230, 224, 0.44)";
  ctx.lineWidth = 4;
  for (let i = 0; i < 7; i += 1) {
    const angle = -Math.PI * 0.85 + i * 0.28 + Math.sin(world.time + i) * 0.04;
    ctx.beginPath();
    ctx.moveTo(0, -12);
    ctx.quadraticCurveTo(Math.cos(angle) * 72, Math.sin(angle) * 36, Math.cos(angle) * 118, Math.sin(angle) * 96 + 60);
    ctx.stroke();
  }
  ctx.fillStyle = "#f1e7dc";
  ctx.font = "700 14px sans-serif";
  const label = world.boss.restored ? "已归位" : `${Math.round(world.boss.disorder)}/${world.boss.max}`;
  ctx.fillText(label, -38, -112);
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
  drawUmbrella(0, -48, player.guard ? 58 : 44, world.craft.umbrellaFill, player.guard ? 0 : -0.24 * player.facing);
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
  if (target.type === "craft") point = target.point;
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

loadContentPack();
syncUi();
requestAnimationFrame(frame);
