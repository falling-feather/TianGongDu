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
  "template_elite_encounter",
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
  "ADR-0008-本地优先与多端云同步.md",
  "ADR-0009-斗战神优先的2.5D斜向全景.md"
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

const handoffContractMarkers = [
  ["docs/00-项目总纲.md", ["体验指导优先级", "《斗战神》是整体体验的第一指导", "团队交付入口", "第一轮必读顺序", "任务卡必须同时写明"]],
  ["docs/01-开发者文档.md", ["团队拓扑与模块所有权", "Definition of Ready", "首个 Bootstrap 迭代", "工程能力工作包索引", "DEV-CAP-04"]],
  ["docs/02-版本规划与验收.md", ["f1_rainy_umbrella_trial", "2.5D 斜向全景", "当前可交接成熟度", "0–30 / 31–60 / 61–90", "标准证据包", "F1 跨团队派工表", "F1-DEV-04", "任务卡复制模板"]],
  ["docs/04-游戏设计总纲.md", ["2.5D 斜向全景空间与相机", "玩家本体、推进与装备边界", "F1 战斗手感种子", "命名遭遇等级"]],
  ["docs/05-世界与叙事圣经.md", ["玩家角色正史边界", "技术年代感与幻想边界", "天工院介入程序"]],
  ["docs/06-内容生产规范.md", ["内容 Definition of Ready", "template_elite_encounter", "F1 金标准实例"]],
  ["docs/07-1.0地区与内容蓝图.md", ["Boss 制作展开", "核心 NPC 内在驱动", "生产成本与容量校准"]],
  ["docs/08-UI-UX与可访问性.md", ["地面八向移动", "纵跃/跨越", "DOM、Canvas 与状态所有权矩阵", "保存与同步用语"]],
  ["docs/09-术语与索引.md", ["2.5D 斜向全景", "体验指导优先级", "待决问题登记", "文档与实现状态词", "文档维护门"]],
  ["docs/01-developer/10-技术架构与依赖规则.md", ["斜向全景权威坐标与碰撞", "JS↔WASM ABI v1.0", "G1 工具锁清单"]],
  ["docs/01-developer/12-内容存档与版本契约.md", ["Web 持久化唯一主路径", "Snapshot/Operation v1 Envelope", "七条版本线"]],
  ["docs/01-developer/15-服务端热更新与模组边界.md", ["普通单机进度与官方权益", "Sync API v1 错误包络", "PostgreSQL v1 不变量"]],
  ["docs/01-developer/16-测试CI与发布门禁.md", ["标准证据包", "机器 Schema 验证成熟度", "门禁例外"]],
  ["docs/01-developer/17-平台构建部署与运维.md", ["环境矩阵与隔离", "Single 与 Pthreads 部署拓扑", "服务目标、告警与灾备"]]
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

async function validateHandoffDocumentation(root, errors) {
  const docs = [
    ...topDocuments.map((file) => join("docs", file)),
    ...developerDocuments.map((file) => join("docs", "01-developer", file))
  ];
  for (const relativePath of docs) {
    const source = await readFile(join(root, relativePath), "utf8");
    for (const marker of ["> 状态", "> 最后更新", "> 维护者角色"]) {
      pushIf(errors, !source.includes(marker), `开发对接文档缺少元信息 ${marker}：${relativePath}`);
    }
  }

  for (const [relativePath, markers] of handoffContractMarkers) {
    const source = await readFile(join(root, relativePath), "utf8");
    for (const marker of markers) {
      pushIf(errors, !source.includes(marker), `开发对接合同缺少“${marker}”：${relativePath}`);
    }
  }

  const planning = await readFile(join(root, "docs", "02-版本规划与验收.md"), "utf8");
  pushIf(errors, planning.includes("当前处于 **F0.1"), "F0.1 已完成，02 不得继续把它写成当前阶段。");
  pushIf(errors, !planning.includes("### 4.1 F1 硬门") || !planning.includes("### 4.2 F1 条件/决策 Spike"), "F1 必须区分硬门与条件/决策 Spike。");
  pushIf(errors, !planning.includes("不是无条件上线承诺"), "30/60/90 必须声明团队容量前提与非无条件工期。");

  const developerGuide = await readFile(join(root, "docs", "01-开发者文档.md"), "utf8");
  pushIf(errors, developerGuide.includes("共同 Owner"), "01 不得使用共同 Owner；必须有单一最终 A。");

  const assertSingleOwnerRows = (source, rowPrefix, ownerIndex, label) => {
    for (const line of source.split(/\r?\n/).filter((entry) => entry.startsWith(rowPrefix))) {
      const cells = line.split("|").slice(1, -1).map((cell) => cell.trim());
      const owner = cells[ownerIndex] ?? "";
      pushIf(errors, !owner || /\s[+/]\s|共同\s*Owner|、.*负责人/.test(owner), `${label} 必须恰有一个最终 Owner：${line}`);
    }
  };
  assertSingleOwnerRows(planning, "| R-", 3, "风险项");
  assertSingleOwnerRows(planning, "| 0–30", 2, "G1");
  assertSingleOwnerRows(planning, "| 31–60", 2, "G2");
  assertSingleOwnerRows(planning, "| 61–90", 2, "G3");
  const indexSource = await readFile(join(root, "docs", "09-术语与索引.md"), "utf8");
  assertSingleOwnerRows(indexSource, "| OQ-", 4, "待决问题");

  const packageMetadata = JSON.parse(await readFile(join(root, "package.json"), "utf8"));
  const history = await readFile(join(root, "docs", "03-发布历史.md"), "utf8");
  pushIf(errors, !history.includes(packageMetadata.version), `发布历史缺少当前设计版本：${packageMetadata.version}`);
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
  const actionRegistryPath = join(root, "content", "design", "action-registry.json");
  const actionSchemaPath = join(root, "content", "schemas", "action-registry.schema.json");
  const catalog = JSON.parse(await readFile(catalogPath, "utf8"));
  const templateRegistry = JSON.parse(await readFile(templateRegistryPath, "utf8"));
  const technicalBaseline = JSON.parse(await readFile(technicalBaselinePath, "utf8"));
  const actionRegistry = JSON.parse(await readFile(actionRegistryPath, "utf8"));
  JSON.parse(await readFile(schemaPath, "utf8"));
  JSON.parse(await readFile(templateSchemaPath, "utf8"));
  JSON.parse(await readFile(technicalSchemaPath, "utf8"));
  JSON.parse(await readFile(actionSchemaPath, "utf8"));

  await validateDocumentHierarchy(root, errors);
  await validateLocalMarkdownLinks(root, errors);
  await validateCurrentArchitectureText(root, errors);
  await validateHandoffDocumentation(root, errors);

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
  pushIf(errors, technicalBaseline.experiencePresentation?.viewModel !== "2.5d-oblique-panoramic", "正式视角必须为 2.5D 斜向全景。");
  pushIf(errors, technicalBaseline.experiencePresentation?.primaryGuidance !== "douzhanshen", "整体体验必须以《斗战神》为第一指导。");
  pushIf(errors, technicalBaseline.experiencePresentation?.secondaryReadabilityReference !== "warm-snow", "《暖雪》只能作为俯斜战斗可读性的第二参考。");
  pushIf(errors, technicalBaseline.experiencePresentation?.gameplaySpace !== "ground-plane-xy-plus-elevation-and-floor-layer" || technicalBaseline.experiencePresentation?.groundPlaneDepthMovement !== true, "Gameplay 必须使用可纵深走位的地面平面加独立高度/层。");
  pushIf(errors, technicalBaseline.experiencePresentation?.cameraControl !== "fixed-or-authored-limited-no-free-orbit", "镜头必须固定或由作者受控，不提供自由旋转基线。");
  pushIf(errors, technicalBaseline.experiencePresentation?.referenceUseBoundary !== "high-level-design-language-only", "参考作品只能用于高层设计语言。");
  pushIf(errors, technicalBaseline.gameCore.language !== "C++20", "游戏核心必须使用 C++20。");
  pushIf(errors, technicalBaseline.gameCore.simulationHz !== 60, "游戏核心必须维持 60 Hz 固定模拟。");
  pushIf(errors, technicalBaseline.runtimeHost.engine !== "Axmol", "运行时宿主必须为 Axmol 基线。");
  pushIf(errors, technicalBaseline.runtimeHost.webCompiler !== "Emscripten", "网页构建必须通过 Emscripten 输出 WebAssembly。");
  pushIf(errors, technicalBaseline.runtimeHost.compilerVersionPolicy !== "pin-exact-version-before-g1-exit", "Emscripten 精确版本必须在 G1 出口前锁定。");
  pushIf(errors, technicalBaseline.toolchainLock?.requiredByGate !== "G1" || technicalBaseline.toolchainLock?.manifestPath !== "toolchains/toolchain-lock.json", "技术基线必须声明 G1 工具链锁文件入口。");
  pushIf(errors, !["planned-not-locked", "locked-and-validated"].includes(technicalBaseline.toolchainLock?.status), "工具链锁状态非法。");
  if (technicalBaseline.toolchainLock?.status === "locked-and-validated") {
    const lockPath = join(root, technicalBaseline.toolchainLock.manifestPath);
    try {
      const lock = JSON.parse(await readFile(lockPath, "utf8"));
      const requiredTools = ["axmol", "emscripten", "cmake", "ninja", "clang", "msvc", "node", "python"];
      pushIf(errors, lock.status !== "locked-and-validated", "工具链锁文件必须声明 locked-and-validated。");
      pushIf(errors, typeof lock.g1EvidenceBuildId !== "string" || lock.g1EvidenceBuildId.length === 0, "工具链锁文件缺少 G1 evidence build ID。");
      for (const tool of requiredTools) {
        const entry = lock.tools?.[tool];
        pushIf(errors, !entry || typeof entry.version !== "string" || entry.version.length === 0 || /latest/i.test(entry.version), `工具链锁缺少 ${tool} 精确 version。`);
        pushIf(errors, !entry || typeof entry.integrity !== "string" || entry.integrity.length === 0, `工具链锁缺少 ${tool} commit/hash/integrity。`);
      }
    } catch {
      errors.push(`工具链标记已锁定，但锁文件不存在或无效：${technicalBaseline.toolchainLock.manifestPath}`);
    }
  }
  pushIf(errors, technicalBaseline.runtimeHost.graphicsBaseline !== "WebGL2", "网页渲染基线必须为 WebGL2。");
  pushIf(errors, technicalBaseline.webShell.hudAndMenus !== "DOM", "HUD 与菜单必须使用可访问的 DOM 层。");
  pushIf(errors, technicalBaseline.webShell.localSave !== "IndexedDB", "浏览器本地存档必须落在 IndexedDB。");
  pushIf(errors, technicalBaseline.webShell.cppPersistenceBridge !== "AsyncIndexedDBPrimary-IDBFSCompatibilityOnly", "Profile 必须以异步 IndexedDB 为唯一主路径，IDBFS 仅兼容。");
  pushIf(errors, technicalBaseline.cloudSync.optional !== true || technicalBaseline.cloudSync.localFirst !== true, "云同步必须可选且遵循本地优先。");
  pushIf(errors, technicalBaseline.cloudSync.serviceLanguage !== "C++20", "同步服务基线必须使用 C++20。");
  pushIf(errors, !technicalBaseline.platformTargets.some((target) => target.id === "web_desktop" && target.release === "1.0-required"), "Web 桌面浏览器必须是 1.0 必选平台。");
  pushIf(errors, !technicalBaseline.browserVariants.some((variant) => variant.id === "web_single_thread" && variant.required), "单线程 WebAssembly 构建必须作为兼容基线。");
  pushIf(errors, countDuplicates(technicalBaseline.browserVariants.map((variant) => variant.id)).length > 0, "浏览器构建变体 ID 不得重复。");
  pushIf(errors, countDuplicates(technicalBaseline.platformTargets.map((target) => target.id)).length > 0, "平台目标 ID 不得重复。");
  const actionIds = actionRegistry.actions.map((action) => action.id);
  const actionContextIds = actionRegistry.contexts.map((context) => context.id);
  const requiredActionIds = [
    "move_x", "move_y", "jump", "light_attack", "heavy_attack", "guard", "evade", "weapon_skill", "lantern", "interact",
    "stance_previous", "stance_next", "stance_direct_1", "stance_direct_2", "stance_direct_3", "switch_weapon", "quick_item",
    "open_map", "open_journal", "ui_navigate", "ui_confirm", "ui_cancel", "ui_tab_previous", "ui_tab_next", "pause", "text_input"
  ];
  pushIf(errors, countDuplicates(actionIds).length > 0, "ActionId 不得重复。");
  pushIf(errors, countDuplicates(actionContextIds).length > 0, "输入 Context ID 不得重复。");
  for (const id of ["gameplay", "dialogue", "menu", "text_entry", "system"]) pushIf(errors, !actionContextIds.includes(id), `缺少输入 Context：${id}`);
  for (const id of requiredActionIds) pushIf(errors, !actionIds.includes(id), `缺少基础 ActionId：${id}`);
  const contextPriorities = actionRegistry.contexts.map((context) => context.priority);
  pushIf(errors, countDuplicates(contextPriorities).length > 0, "输入 Context priority 不得重复。");
  const expectedContextPriorities = { system: 0, gameplay: 10, dialogue: 20, menu: 30, text_entry: 40 };
  for (const [contextId, priority] of Object.entries(expectedContextPriorities)) {
    const actual = actionRegistry.contexts.find((context) => context.id === contextId)?.priority;
    pushIf(errors, actual !== priority, `${contextId} Context priority 必须为 ${priority}，当前为 ${actual}。`);
  }
  for (const action of actionRegistry.actions) {
    for (const contextId of action.contexts) pushIf(errors, !actionContextIds.includes(contextId), `${action.id} 引用了不存在的 Context：${contextId}`);
    pushIf(errors, action.clearOnBlur !== true, `${action.id} 必须在 blur/context clear 时释放。`);
  }

  const actionById = new Map(actionRegistry.actions.map((action) => [action.id, action]));
  const bindingTuples = actionRegistry.defaultBindings.map((binding) => `${binding.profile}|${binding.control}|${binding.gesture}|${binding.actionId}`);
  const duplicateBindingTuples = countDuplicates(bindingTuples);
  pushIf(errors, duplicateBindingTuples.length > 0, `默认输入映射存在完全重复项：${duplicateBindingTuples.join(", ")}`);
  for (const binding of actionRegistry.defaultBindings) {
    pushIf(errors, !actionById.has(binding.actionId), `默认输入映射引用不存在的 ActionId：${binding.actionId}`);
  }

  const bindingsByPhysicalTrigger = new Map();
  for (const binding of actionRegistry.defaultBindings) {
    const key = `${binding.profile}|${binding.control}|${binding.gesture}`;
    const group = bindingsByPhysicalTrigger.get(key) ?? [];
    group.push(binding);
    bindingsByPhysicalTrigger.set(key, group);
  }
  for (const [trigger, bindings] of bindingsByPhysicalTrigger) {
    for (let leftIndex = 0; leftIndex < bindings.length; leftIndex += 1) {
      for (let rightIndex = leftIndex + 1; rightIndex < bindings.length; rightIndex += 1) {
        const leftAction = actionById.get(bindings[leftIndex].actionId);
        const rightAction = actionById.get(bindings[rightIndex].actionId);
        if (!leftAction || !rightAction || leftAction.id === rightAction.id) continue;
        const overlappingContexts = leftAction.contexts.filter((contextId) => rightAction.contexts.includes(contextId));
        pushIf(errors, overlappingContexts.length > 0, `默认输入映射在同一 Context 发生硬冲突：${trigger} → ${leftAction.id}/${rightAction.id} (${overlappingContexts.join(",")})`);
      }
    }
  }

  const hasProfileBinding = (actionId, profile) => actionRegistry.defaultBindings.some((binding) => binding.actionId === actionId && binding.profile === profile);
  const dualProfileActions = [
    "move_x", "move_y", "jump", "light_attack", "heavy_attack", "guard", "evade", "weapon_skill", "lantern", "interact",
    "stance_previous", "stance_next", "switch_weapon", "quick_item", "open_map", "open_journal", "ui_navigate", "ui_confirm", "ui_cancel",
    "ui_tab_previous", "ui_tab_next", "pause"
  ];
  for (const actionId of dualProfileActions) {
    pushIf(errors, !hasProfileBinding(actionId, "keyboard_mouse"), `${actionId} 缺少 keyboard_mouse 默认映射。`);
    pushIf(errors, !hasProfileBinding(actionId, "gamepad_standard"), `${actionId} 缺少 gamepad_standard 默认映射。`);
  }
  for (const actionId of ["stance_direct_1", "stance_direct_2", "stance_direct_3"]) {
    pushIf(errors, !hasProfileBinding(actionId, "keyboard_mouse"), `${actionId} 缺少键盘直接架势映射。`);
  }
  const textControls = actionRegistry.defaultBindings.filter((binding) => binding.actionId === "text_input" && binding.profile === "text_input").map((binding) => binding.control);
  pushIf(errors, !textControls.includes("DOM.BeforeInputOrInputCommit"), "普通非 IME 文本必须提供 beforeinput/input 提交路径。");
  pushIf(errors, !textControls.includes("DOM.CompositionCommit"), "IME 文本必须提供 composition commit 路径。");
  pushIf(errors, actionRegistry.defaultBindings.some((binding) => binding.control === "Keyboard.Tab"), "Tab 必须保留给 DOM 焦点导航，不得作为默认 Action 映射。");
  pushIf(errors, !actionRegistry.defaultBindings.some((binding) => binding.profile === "keyboard_mouse" && binding.control === "Keyboard.KeyC" && binding.actionId === "evade"), "08 声明的闪身备用键 C 必须存在于 Action Registry。");
  const gamepadBack = actionRegistry.defaultBindings.filter((binding) => binding.profile === "gamepad_standard" && binding.control === "Gamepad.ButtonBack");
  pushIf(errors, !gamepadBack.some((binding) => binding.actionId === "open_map" && binding.gesture === "tap_deferred"), "Gamepad Back 短按地图必须延迟到释放后裁决。");
  pushIf(errors, !gamepadBack.some((binding) => binding.actionId === "open_journal" && binding.gesture === "hold"), "Gamepad Back 长按必须打开日志且抑制短按地图。");
  pushIf(errors, !actionRegistry.defaultBindings.some((binding) => binding.profile === "gamepad_standard" && binding.control === "Gamepad.Dpad" && binding.actionId === "ui_navigate"), "手柄 UI 导航必须同时支持 D-pad。");
  pushIf(errors, actionRegistry.rules?.physicalRepeatCreatesPressEdges !== false, "物理 key repeat 不得生成 Gameplay press edge。");
  pushIf(errors, actionRegistry.rules?.textCompositionNeverEntersGameplay !== true, "IME/text composition 不得进入 Gameplay。");
  pushIf(errors, actionRegistry.rules?.higherContextPriorityWins !== true || actionRegistry.rules?.unhandledInputFallsThrough !== true, "输入必须按高优先 Context 消费并允许未处理输入向下回退。");
  pushIf(errors, actionRegistry.rules?.browserTabReservedForDomFocus !== true, "Tab 必须保留给 DOM 焦点导航。");
  pushIf(errors, actionRegistry.rules?.escapeBrowserExitPrecedesActionDispatch !== true, "Escape 必须先处理浏览器全屏/Pointer Lock 退出，再参与下一次 Action 派发。");
  pushIf(errors, actionRegistry.rules?.sharedTapHoldEmitsExactlyOneAction !== true, "短按/长按复用必须保证只派发一个 Action。");
  pushIf(errors, actionRegistry.rules?.ordinaryAndImeTextUseCommitPaths !== true, "普通文本与 IME 必须各有提交路径。");
  pushIf(errors, !Number.isInteger(actionRegistry.rules?.holdThresholdMs) || actionRegistry.rules.holdThresholdMs < 200 || actionRegistry.rules.holdThresholdMs > 700, "holdThresholdMs 必须在 200–700 ms。 ");
  pushIf(errors, !Number.isInteger(actionRegistry.rules?.uiRepeatDelayMs) || actionRegistry.rules.uiRepeatDelayMs < 200 || actionRegistry.rules.uiRepeatDelayMs > 800, "uiRepeatDelayMs 必须在 200–800 ms。 ");
  pushIf(errors, !Number.isInteger(actionRegistry.rules?.uiRepeatIntervalMs) || actionRegistry.rules.uiRepeatIntervalMs < 40 || actionRegistry.rules.uiRepeatIntervalMs > 250 || actionRegistry.rules.uiRepeatIntervalMs >= actionRegistry.rules.uiRepeatDelayMs, "uiRepeatIntervalMs 必须在 40–250 ms 且小于首次延迟。 ");
  pushIf(errors, typeof actionRegistry.rules?.gamepadStickDeadzone !== "number" || actionRegistry.rules.gamepadStickDeadzone <= 0 || actionRegistry.rules.gamepadStickDeadzone > 0.5, "gamepadStickDeadzone 必须在 (0, 0.5]。 ");
  pushIf(errors, typeof actionRegistry.rules?.gamepadTriggerDeadzone !== "number" || actionRegistry.rules.gamepadTriggerDeadzone < 0 || actionRegistry.rules.gamepadTriggerDeadzone > 0.5, "gamepadTriggerDeadzone 必须在 [0, 0.5]。 ");
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
  const chapterIds = new Set(catalog.mainChapters.map((chapter) => chapter.id));
  const questChainIds = new Set(catalog.questChains.map((quest) => quest.id));
  const enemyFamilyIds = new Set(catalog.enemyFamilies.map((enemy) => enemy.id));
  const subregions = catalog.regions.flatMap((region) => region.subregions);
  const allIds = [
    ...catalog.combatSystems.map((entry) => entry.id),
    ...catalog.weapons.map((entry) => entry.id),
    ...regionIds,
    ...subregions.map((entry) => entry.id),
    ...chapterIds,
    ...questChainIds,
    ...enemyFamilyIds,
    ...bossIds,
    ...npcIds,
    catalog.f1VerticalSlice.id
  ];
  const duplicateIds = countDuplicates(allIds);
  pushIf(errors, duplicateIds.length > 0, `发现重复稳定 ID：${duplicateIds.join(", ")}`);

  const assignedBossIds = new Set();
  const assignedNpcIds = new Set();
  const assignedQuestNpcIds = new Set();
  for (const region of catalog.regions) {
    pushIf(errors, region.mainMinutes < 300, `${region.name} 主线预算少于 5 小时。`);
    pushIf(errors, region.subregions.length < 5, `${region.name} 的大型子地区少于 5 个。`);
    pushIf(errors, region.sideQuestChains < 5, `${region.name} 的支线链少于 5 条。`);
    pushIf(errors, region.pointsOfInterest < 30, `${region.name} 的兴趣点少于 30 个。`);

    const regionChapters = catalog.mainChapters.filter((chapter) => chapter.regionId === region.id);
    const regionQuests = catalog.questChains.filter((quest) => quest.regionId === region.id);
    const regionEnemies = catalog.enemyFamilies.filter((enemy) => enemy.regionId === region.id);
    pushIf(errors, regionChapters.length !== region.mainChapters, `${region.name} 的章节对象数 ${regionChapters.length} 与预算 ${region.mainChapters} 不一致。`);
    pushIf(errors, regionChapters.reduce((sum, chapter) => sum + chapter.minutes, 0) !== region.mainMinutes, `${region.name} 的章节分钟无法复算到 ${region.mainMinutes}。`);
    pushIf(errors, regionQuests.length !== region.sideQuestChains, `${region.name} 的支线链对象数 ${regionQuests.length} 与预算 ${region.sideQuestChains} 不一致。`);
    pushIf(errors, regionQuests.reduce((sum, quest) => sum + quest.estimatedMinutes, 0) !== region.sideMinutes, `${region.name} 的支线分钟无法复算到 ${region.sideMinutes}。`);
    pushIf(errors, regionEnemies.length < 8, `${region.name} 的敌人家族少于 8 个。`);

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

  for (const chapter of catalog.mainChapters) {
    pushIf(errors, !regionIds.has(chapter.regionId), `${chapter.name} 引用了不存在的地区：${chapter.regionId}`);
  }

  for (const quest of catalog.questChains) {
    pushIf(errors, !regionIds.has(quest.regionId), `${quest.name} 引用了不存在的地区：${quest.regionId}`);
    for (const npcId of quest.participantNpcIds) {
      pushIf(errors, !npcIds.has(npcId), `${quest.name} 引用了不存在的 NPC：${npcId}`);
      const npc = catalog.npcs.find((entry) => entry.id === npcId);
      pushIf(errors, npc && npc.regionId !== quest.regionId, `${quest.name} 与参与 NPC ${npcId} 不属于同一地区。`);
      assignedQuestNpcIds.add(npcId);
    }
    const owners = catalog.npcs.filter((npc) => npc.questChainId === quest.id && npc.questRole === "owner");
    pushIf(errors, owners.length !== 1, `${quest.name} 必须恰有 1 名 owner，当前为 ${owners.length}。`);
    for (const npcId of quest.participantNpcIds) {
      const npc = catalog.npcs.find((entry) => entry.id === npcId);
      pushIf(errors, npc && npc.questChainId !== quest.id, `${quest.name} 的参与者 ${npcId} 反向引用了其他支线链。`);
    }
  }

  for (const enemy of catalog.enemyFamilies) {
    pushIf(errors, !regionIds.has(enemy.regionId), `${enemy.name} 引用了不存在的地区：${enemy.regionId}`);
  }

  for (const boss of catalog.bosses) {
    pushIf(errors, !regionIds.has(boss.regionId), `${boss.name} 引用了不存在的地区：${boss.regionId}`);
    const minimumPhases = { elite: 2, chapter: 3, region: 4 }[boss.rank];
    pushIf(errors, !minimumPhases, `${boss.name} 使用未知等级：${boss.rank}`);
    pushIf(errors, minimumPhases && boss.phases.length < minimumPhases, `${boss.name} 的 ${boss.rank} 等级至少需要 ${minimumPhases} 次机制阶段。`);
    pushIf(errors, typeof boss.resolutionSummary !== "string" || boss.resolutionSummary.length === 0, `${boss.name} 缺少归位结果摘要。`);
    pushIf(errors, !assignedBossIds.has(boss.id), `${boss.name} 未分配到任何子地区。`);
  }

  for (const npc of catalog.npcs) {
    pushIf(errors, !regionIds.has(npc.regionId), `${npc.name} 引用了不存在的地区：${npc.regionId}`);
    pushIf(errors, npc.interactionSystems.length < 2, `${npc.name} 少于 2 种交互系统。`);
    pushIf(errors, npc.relationshipArc.length < 3, `${npc.name} 的关系弧少于 3 个阶段。`);
    pushIf(errors, !assignedNpcIds.has(npc.id), `${npc.name} 未分配到任何子地区。`);
    pushIf(errors, !questChainIds.has(npc.questChainId), `${npc.name} 引用了不存在的支线链：${npc.questChainId}`);
    const quest = catalog.questChains.find((entry) => entry.id === npc.questChainId);
    pushIf(errors, quest && !quest.participantNpcIds.includes(npc.id), `${npc.name} 未列入其支线链 ${npc.questChainId} 的参与者。`);
    pushIf(errors, quest && quest.regionId !== npc.regionId, `${npc.name} 与支线链 ${npc.questChainId} 不属于同一地区。`);
    pushIf(errors, !assignedQuestNpcIds.has(npc.id), `${npc.name} 未参与任何正式支线链。`);
  }

  const f1 = catalog.f1VerticalSlice;
  pushIf(errors, f1.channel !== "prototype_f1" || f1.profileNamespace !== "prototype_f1", "F1 纵切必须使用独立 prototype_f1 渠道与 Profile 命名空间。");
  pushIf(errors, f1.startFixtureId !== "fixture_f1_rainy_umbrella_start", "F1 必须使用唯一受控起始夹具。");
  pushIf(errors, f1.playableTargetMinutes !== 60 || f1.endToEndTestBudgetMinutes !== 70, "F1 必须区分不少于 60 分钟的可玩目标与 70 分钟端到端测试预算。");
  pushIf(errors, !chapterIds.has(f1.chapterId), `F1 引用了不存在的章节：${f1.chapterId}`);
  pushIf(errors, !bossIds.has(f1.bossId), `F1 引用了不存在的 Boss：${f1.bossId}`);
  for (const id of f1.subregionIds) pushIf(errors, !subregions.some((entry) => entry.id === id), `F1 引用了不存在的子地区：${id}`);
  for (const id of f1.npcIds) pushIf(errors, !npcIds.has(id), `F1 引用了不存在的 NPC：${id}`);
  for (const id of f1.enemyFamilyIds) pushIf(errors, !enemyFamilyIds.has(id), `F1 引用了不存在的敌人家族：${id}`);
  const f1Chapter = catalog.mainChapters.find((entry) => entry.id === f1.chapterId);
  const f1Boss = catalog.bosses.find((entry) => entry.id === f1.bossId);
  pushIf(errors, f1Chapter?.regionId !== "region_jiangnan" || f1Boss?.regionId !== "region_jiangnan", "F1 章节与 Boss 必须属于江南纵切。");
  for (const id of f1.subregionIds) {
    const region = catalog.regions.find((entry) => entry.subregions.some((subregion) => subregion.id === id));
    pushIf(errors, region?.id !== "region_jiangnan", `F1 子地区不属于江南：${id}`);
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
    mainChapters: catalog.mainChapters.length,
    sideQuestChains: catalog.questChains.length,
    enemyFamilies: catalog.enemyFamilies.length,
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
      mainChapters: catalog.mainChapters.length,
      questChains: catalog.questChains.length,
      enemyFamilies: catalog.enemyFamilies.length,
      f1VerticalSlice: catalog.f1VerticalSlice.id,
      templates: templateRegistry.templates.length,
      coreLanguage: technicalBaseline.gameCore.language,
      engine: technicalBaseline.runtimeHost.engine,
      viewModel: technicalBaseline.experiencePresentation.viewModel,
      primaryGuidance: technicalBaseline.experiencePresentation.primaryGuidance,
      toolchainLockStatus: technicalBaseline.toolchainLock.status,
      webTarget: technicalBaseline.runtimeHost.wasmTarget,
      cloudSync: technicalBaseline.cloudSync.optional,
      activeArchitectureDecisions: activeArchitectureDecisions.length,
      platformTargets: technicalBaseline.platformTargets.length,
      architectureModules: architectureSkeleton.length,
      handoffContractMarkers: handoffContractMarkers.length,
      actionContexts: actionRegistry.contexts.length,
      actions: actionRegistry.actions.length,
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
