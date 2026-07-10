import { access, readFile, readdir } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const modulePath = fileURLToPath(import.meta.url);
const defaultRoot = resolve(dirname(modulePath), "..");

const topDocuments = [
  "00-项目总纲.md",
  "01-开发者文档.md",
  "02-版本规划与验收.md",
  "03-发布历史.md",
  "04-游戏设计总纲.md",
  "05-世界与叙事圣经.md",
  "06-内容生产规范.md",
  "07-1.0地区与内容蓝图.md",
  "08-UI-UX与可访问性.md",
  "09-术语与索引.md"
];

const developerDocuments = [
  "10-技术架构与依赖规则.md",
  "11-Runtime与Gameplay组件模型.md",
  "12-内容存档与版本契约.md",
  "13-编辑器与模板生产.md",
  "14-资产管线与性能预算.md",
  "15-服务端热更新与模组边界.md",
  "16-测试CI与发布门禁.md",
  "17-平台构建部署与运维.md",
  "18-安全依赖与许可证.md",
  "19-ADR索引.md"
];

const requiredTemplates = [
  "template_core_npc",
  "template_standard_enemy",
  "template_chapter_boss",
  "template_region_boss",
  "template_quest_chain",
  "template_dialogue",
  "template_weapon_family",
  "template_subregion"
];

const activeArchitectureDecisions = [
  "ADR-0005-Cpp网页首发与Axmol宿主.md",
  "ADR-0006-Cpp分层核心与Wasm平台边界.md",
  "ADR-0007-浏览器内容包与存档同步契约.md",
  "ADR-0008-本地优先与多端云同步.md"
];

const supersededArchitectureDecisions = [
  ["ADR-0001-桌面原生与Godot宿主.md", "ADR-0005"],
  ["ADR-0002-混合组件与分层核心.md", "ADR-0006"],
  ["ADR-0003-内容包与存档版本契约.md", "ADR-0007"],
  ["ADR-0004-离线优先更新与受限模组.md", "ADR-0008"]
];

const architectureSkeleton = [
  "apps/web-shell",
  "apps/native-shell",
  "apps/content-workbench",
  "src/contracts",
  "src/runtime",
  "src/gameplay",
  "src/presentation-axmol",
  "src/platform",
  "src/content-core",
  "src/sync-contracts",
  "server/sync-service"
];

function countDuplicates(items) {
  const seen = new Set();
  const duplicates = new Set();
  for (const item of items) {
    if (seen.has(item)) duplicates.add(item);
    seen.add(item);
  }
  return [...duplicates];
}

function pushIf(errors, condition, message) {
  if (condition) errors.push(message);
}

async function validateDocumentHierarchy(root, errors) {
  const docsRoot = join(root, "docs");
  const developerRoot = join(docsRoot, "01-developer");
  const topEntries = await readdir(docsRoot, { withFileTypes: true });
  const actualTopDocs = topEntries
    .filter((entry) => entry.isFile() && entry.name.endsWith(".md"))
    .map((entry) => entry.name)
    .sort();
  const actualDeveloperDocs = (await readdir(developerRoot, { withFileTypes: true }))
    .filter((entry) => entry.isFile() && entry.name.endsWith(".md"))
    .map((entry) => entry.name)
    .sort();

  pushIf(errors, actualTopDocs.length !== 10, `顶层文档必须恰好 10 份，当前为 ${actualTopDocs.length} 份。`);
  pushIf(errors, actualDeveloperDocs.length !== 10, `开发者二级文档必须恰好 10 份，当前为 ${actualDeveloperDocs.length} 份。`);

  for (const file of topDocuments) {
    pushIf(errors, !actualTopDocs.includes(file), `缺少顶层文档：${file}`);
  }
  for (const file of developerDocuments) {
    pushIf(errors, !actualDeveloperDocs.includes(file), `缺少开发者二级文档：${file}`);
  }

  const adrRoot = join(developerRoot, "adr");
  const adrFiles = await readdir(adrRoot);
  for (const file of activeArchitectureDecisions) {
    pushIf(errors, !adrFiles.includes(file), `缺少当前 ADR：${file}`);
    if (adrFiles.includes(file)) {
      const source = await readFile(join(adrRoot, file), "utf8");
      pushIf(errors, !source.includes("> 状态：Accepted"), `当前 ADR 未标记 Accepted：${file}`);
    }
  }
  for (const [file, replacement] of supersededArchitectureDecisions) {
    pushIf(errors, !adrFiles.includes(file), `缺少历史 ADR：${file}`);
    if (adrFiles.includes(file)) {
      const source = await readFile(join(adrRoot, file), "utf8");
      pushIf(errors, !source.includes(`> 状态：Superseded by ${replacement}`), `历史 ADR 未正确指向 ${replacement}：${file}`);
    }
  }
}

async function validateCurrentArchitectureText(root, errors) {
  const currentFiles = [
    "README.md",
    join("docs", "00-项目总纲.md"),
    join("docs", "01-开发者文档.md"),
    ...developerDocuments.slice(0, 9).map((file) => join("docs", "01-developer", file))
  ];
  const forbidden = ["Godot", ".NET", "C#", "PCK"];
  for (const relativePath of currentFiles) {
    const source = await readFile(join(root, relativePath), "utf8");
    for (const term of forbidden) {
      pushIf(errors, source.includes(term), `当前技术文档仍含已取代术语 ${term}：${relativePath}`);
    }
  }
}

async function collectMarkdownFiles(directory) {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const fullPath = join(directory, entry.name);
    if (entry.isDirectory()) files.push(...await collectMarkdownFiles(fullPath));
    if (entry.isFile() && entry.name.endsWith(".md")) files.push(fullPath);
  }
  return files;
}

async function validateLocalMarkdownLinks(root, errors) {
  const files = [join(root, "README.md"), ...await collectMarkdownFiles(join(root, "docs"))];
  const markdownLink = /\[[^\]]*\]\(([^)]+)\)/g;
  for (const file of files) {
    const source = await readFile(file, "utf8");
    for (const match of source.matchAll(markdownLink)) {
      const rawTarget = match[1].trim().replace(/^<|>$/g, "");
      if (!rawTarget || rawTarget.startsWith("#") || /^[a-z][a-z0-9+.-]*:/i.test(rawTarget)) continue;
      const targetPath = decodeURIComponent(rawTarget.split("#", 1)[0]);
      const absoluteTarget = resolve(dirname(file), targetPath);
      try {
        await access(absoluteTarget);
      } catch {
        errors.push(`失效本地链接：${file.slice(root.length + 1)} → ${rawTarget}`);
      }
    }
  }
}

export async function validateProject(projectRoot = defaultRoot) {
  const root = resolve(projectRoot);
  const errors = [];
  const catalogPath = join(root, "content", "design", "v1-content-catalog.json");
  const schemaPath = join(root, "content", "schemas", "v1-content-catalog.schema.json");
  const templateRegistryPath = join(root, "content", "templates", "template-registry.json");
  const templateSchemaPath = join(root, "content", "schemas", "template-registry.schema.json");
  const technicalBaselinePath = join(root, "content", "design", "technical-baseline.json");
  const technicalSchemaPath = join(root, "content", "schemas", "technical-baseline.schema.json");
  const catalog = JSON.parse(await readFile(catalogPath, "utf8"));
  const templateRegistry = JSON.parse(await readFile(templateRegistryPath, "utf8"));
  const technicalBaseline = JSON.parse(await readFile(technicalBaselinePath, "utf8"));
  JSON.parse(await readFile(schemaPath, "utf8"));
  JSON.parse(await readFile(templateSchemaPath, "utf8"));
  JSON.parse(await readFile(technicalSchemaPath, "utf8"));

  await validateDocumentHierarchy(root, errors);
  await validateLocalMarkdownLinks(root, errors);
  await validateCurrentArchitectureText(root, errors);

  pushIf(errors, catalog.combatSystems.length < 2, "1.0 至少需要 2 套战斗系统。");
  pushIf(errors, catalog.weapons.length < 2, "1.0 至少需要 2 条武器体系。");
  pushIf(errors, catalog.regions.length < 3, "1.0 至少需要 3 个地区。");
  pushIf(errors, catalog.bosses.length < 10, "1.0 至少需要 10 个 Boss。");
  pushIf(errors, catalog.npcs.length < 20, "1.0 至少需要 20 个核心 NPC。");

  const templateIds = templateRegistry.templates.map((template) => template.id);
  const duplicateTemplateIds = countDuplicates(templateIds);
  pushIf(errors, duplicateTemplateIds.length > 0, `发现重复模板 ID：${duplicateTemplateIds.join(", ")}`);
  for (const templateId of requiredTemplates) {
    pushIf(errors, !templateIds.includes(templateId), `缺少基础内容模板：${templateId}`);
  }
  for (const template of templateRegistry.templates) {
    pushIf(errors, template.requiredModules.length < 3, `${template.id} 的必需模块少于 3 个。`);
    pushIf(errors, template.generatedArtifacts.length < 3, `${template.id} 的生成产物少于 3 个。`);
    pushIf(errors, template.validators.length < 3, `${template.id} 的验证规则少于 3 个。`);
  }

  pushIf(errors, technicalBaseline.productTarget !== "browser-first-cross-platform-action-rpg", "技术基线必须以浏览器首发、多端复用为目标。");
  pushIf(errors, technicalBaseline.gameCore.language !== "C++20", "游戏核心必须使用 C++20。");
  pushIf(errors, technicalBaseline.gameCore.simulationHz !== 60, "游戏核心必须维持 60 Hz 固定模拟。");
  pushIf(errors, technicalBaseline.runtimeHost.engine !== "Axmol", "运行时宿主必须为 Axmol 基线。");
  pushIf(errors, technicalBaseline.runtimeHost.webCompiler !== "Emscripten", "网页构建必须通过 Emscripten 输出 WebAssembly。");
  pushIf(errors, technicalBaseline.runtimeHost.graphicsBaseline !== "WebGL2", "网页渲染基线必须为 WebGL2。");
  pushIf(errors, technicalBaseline.webShell.hudAndMenus !== "DOM", "HUD 与菜单必须使用可访问的 DOM 层。");
  pushIf(errors, technicalBaseline.webShell.localSave !== "IndexedDB", "浏览器本地存档必须落在 IndexedDB。");
  pushIf(errors, technicalBaseline.cloudSync.optional !== true || technicalBaseline.cloudSync.localFirst !== true, "云同步必须可选且遵循本地优先。");
  pushIf(errors, technicalBaseline.cloudSync.serviceLanguage !== "C++20", "同步服务基线必须使用 C++20。");
  pushIf(errors, !technicalBaseline.platformTargets.some((target) => target.id === "web_desktop" && target.release === "1.0-required"), "Web 桌面浏览器必须是 1.0 必选平台。");
  pushIf(errors, !technicalBaseline.browserVariants.some((variant) => variant.id === "web_single_thread" && variant.required), "单线程 WebAssembly 构建必须作为兼容基线。");
  for (const relativePath of architectureSkeleton) {
    try {
      await access(join(root, relativePath));
    } catch {
      errors.push(`缺少技术架构目录：${relativePath}`);
    }
  }

  const regionIds = new Set(catalog.regions.map((region) => region.id));
  const bossIds = new Set(catalog.bosses.map((boss) => boss.id));
  const npcIds = new Set(catalog.npcs.map((npc) => npc.id));
  const subregions = catalog.regions.flatMap((region) => region.subregions);
  const allIds = [
    ...catalog.combatSystems.map((entry) => entry.id),
    ...catalog.weapons.map((entry) => entry.id),
    ...regionIds,
    ...subregions.map((entry) => entry.id),
    ...bossIds,
    ...npcIds
  ];
  const duplicateIds = countDuplicates(allIds);
  pushIf(errors, duplicateIds.length > 0, `发现重复稳定 ID：${duplicateIds.join(", ")}`);

  const assignedBossIds = new Set();
  const assignedNpcIds = new Set();
  for (const region of catalog.regions) {
    pushIf(errors, region.mainMinutes < 300, `${region.name} 主线预算少于 5 小时。`);
    pushIf(errors, region.subregions.length < 5, `${region.name} 的大型子地区少于 5 个。`);
    pushIf(errors, region.sideQuestChains < 5, `${region.name} 的支线链少于 5 条。`);
    pushIf(errors, region.pointsOfInterest < 30, `${region.name} 的兴趣点少于 30 个。`);

    for (const subregion of region.subregions) {
      for (const bossId of subregion.bossIds) {
        pushIf(errors, !bossIds.has(bossId), `${subregion.name} 引用了不存在的 Boss：${bossId}`);
        assignedBossIds.add(bossId);
      }
      for (const npcId of subregion.npcIds) {
        pushIf(errors, !npcIds.has(npcId), `${subregion.name} 引用了不存在的 NPC：${npcId}`);
        assignedNpcIds.add(npcId);
      }
    }
  }

  for (const boss of catalog.bosses) {
    pushIf(errors, !regionIds.has(boss.regionId), `${boss.name} 引用了不存在的地区：${boss.regionId}`);
    pushIf(errors, boss.phases.length < 2, `${boss.name} 少于 2 个阶段。`);
    pushIf(errors, !assignedBossIds.has(boss.id), `${boss.name} 未分配到任何子地区。`);
  }

  for (const npc of catalog.npcs) {
    pushIf(errors, !regionIds.has(npc.regionId), `${npc.name} 引用了不存在的地区：${npc.regionId}`);
    pushIf(errors, npc.interactionSystems.length < 2, `${npc.name} 少于 2 种交互系统。`);
    pushIf(errors, npc.relationshipArc.length < 3, `${npc.name} 的关系弧少于 3 个阶段。`);
    pushIf(errors, !assignedNpcIds.has(npc.id), `${npc.name} 未分配到任何子地区。`);
  }

  const actualMainMinutes = catalog.regions.reduce((total, region) => total + region.mainMinutes, 0);
  const actualSubregionCount = subregions.length;
  const targets = catalog.contentTargets;
  const expectedTargets = {
    mainMinutes: actualMainMinutes,
    mainAndSideMinutes: catalog.regions.reduce((sum, region) => sum + region.mainMinutes + region.sideMinutes, 0),
    completionistMinutes: catalog.regions.reduce((sum, region) => sum + region.completionistMinutes, 0),
    regions: catalog.regions.length,
    subregions: actualSubregionCount,
    bosses: catalog.bosses.length,
    coreNpcs: catalog.npcs.length,
    combatSystems: catalog.combatSystems.length,
    weaponFamilies: catalog.weapons.length,
    mainChapters: catalog.regions.reduce((sum, region) => sum + region.mainChapters, 0),
    sideQuestChains: catalog.regions.reduce((sum, region) => sum + region.sideQuestChains, 0),
    pointsOfInterest: catalog.regions.reduce((sum, region) => sum + region.pointsOfInterest, 0)
  };
  for (const [key, value] of Object.entries(expectedTargets)) {
    pushIf(errors, targets[key] !== value, `contentTargets.${key}=${targets[key]}，实际应为 ${value}。`);
  }

  return {
    valid: errors.length === 0,
    errors,
    summary: {
      combatSystems: catalog.combatSystems.length,
      weapons: catalog.weapons.length,
      regions: catalog.regions.length,
      subregions: actualSubregionCount,
      bosses: catalog.bosses.length,
      npcs: catalog.npcs.length,
      templates: templateRegistry.templates.length,
      coreLanguage: technicalBaseline.gameCore.language,
      engine: technicalBaseline.runtimeHost.engine,
      webTarget: technicalBaseline.runtimeHost.wasmTarget,
      cloudSync: technicalBaseline.cloudSync.optional,
      activeArchitectureDecisions: activeArchitectureDecisions.length,
      platformTargets: technicalBaseline.platformTargets.length,
      architectureModules: architectureSkeleton.length,
      mainMinutes: actualMainMinutes
    }
  };
}

if (process.argv[1] && resolve(process.argv[1]) === modulePath) {
  const result = await validateProject();
  if (!result.valid) {
    console.error("天工渡 1.0 设计校验失败：");
    for (const error of result.errors) console.error(`- ${error}`);
    process.exitCode = 1;
  } else {
    console.log("天工渡 1.0 设计校验通过：");
    console.log(JSON.stringify(result.summary, null, 2));
  }
}
