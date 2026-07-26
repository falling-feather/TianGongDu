const MAX_PRESENTATION_DIAGNOSTICS = 512;

const PACKAGE_MESSAGES = Object.freeze({
  1: "存在重复的 Stable ID。",
  2: "Sandbox 世界边界无效。",
  3: "区域边界无效。",
  4: "对象引用了不存在的区域。",
  5: "对象引用了不存在的资产。",
  6: "对象使用的资产类型不匹配。",
  7: "Player 引用了不存在的安全点。",
  8: "Player 放置无效。",
  9: "Actor 放置无效。",
  10: "Ground Blocker 定义无效。",
  11: "Safe Point 放置无效。",
  12: "对象位于允许边界之外。",
  13: "Player 初始位置被阻挡。",
  14: "Safe Point 位置被阻挡。",
  15: "Interaction 放置无效。",
  16: "Mechanism 放置无效。",
  17: "Wave 定义无效。",
  18: "Wave Spawn 定义无效。",
  19: "Objective 定义无效。",
  20: "对象数量超过内容包容量。",
  21: "存在指向缺失对象的引用。",
  22: "引用目标的对象类型不匹配。",
  23: "依赖关系中存在循环。",
  24: "存在无法到达的内容节点。",
  25: "重试路径的内容状态不一致。",
  26: "缺少所需的平台变体。",
  27: "缺少资产元数据。",
  28: "缺少资产锚点。",
  29: "关键信息仅通过颜色区分。",
  30: "通用占位资源发生冲突。",
  31: "资产许可不允许当前使用方式。",
  32: "内容超过 Web 预算。",
  33: "Stable ID 格式无效。"
});

const BINDING_MESSAGES = Object.freeze({
  2: "Interaction Binding 数量超过容量。",
  3: "Mechanism Binding 数量超过容量。",
  4: "Interaction Binding 的 Interaction ID 无效。",
  5: "Interaction Binding 指向不存在的 Interaction。",
  6: "同一 Interaction 存在重复 Binding。",
  7: "Interaction Operation 无效。",
  8: "Interaction 操作距离无效。",
  9: "目标 Mechanism ID 无效。",
  10: "Interaction Binding 指向不存在的 Mechanism。",
  11: "Interaction 缺少所需 Binding。",
  12: "Mechanism Binding 的 Mechanism ID 无效。",
  13: "Mechanism Binding 指向不存在的 Mechanism。",
  14: "同一 Mechanism 存在重复 Binding。",
  15: "Mechanism Activation 无效。",
  16: "目标 Ground Blocker ID 无效。",
  17: "Mechanism Binding 指向不存在的 Ground Blocker。",
  18: "同一 Ground Blocker 存在多个写入者。",
  19: "Mechanism 缺少所需 Binding。",
  20: "同一目标 Mechanism 存在多个写入者。",
  21: "存在未被 Interaction 引用的 Mechanism。",
  22: "Actor Gameplay Binding 数量超过容量。",
  23: "Actor Gameplay Binding 的 Actor ID 无效。",
  24: "Actor Gameplay Binding 指向不存在的 Actor。",
  25: "同一 Actor 存在重复 Gameplay Binding。",
  26: "Actor Gameplay Binding 的 Profile ID 无效。",
  27: "Actor Gameplay Binding 的 Faction 无效。",
  28: "Actor Gameplay Binding 的 Tactical Duty 无效。",
  29: "Actor Gameplay Binding 的 Max Health 无效。",
  30: "Wave Spawn 引用的 Actor 缺少 Gameplay Binding。"
});

const PACKAGE_FIELDS = Object.freeze({
  1: "id",
  2: "minX",
  3: "maxX",
  4: "minY",
  5: "maxY",
  6: "minHeight",
  7: "maxHeight",
  10: "regionId",
  11: "assetId",
  13: "initialSafePointId",
  14: "x",
  15: "y",
  16: "height",
  17: "floorLayer",
  18: "facingMillidegrees"
});

const BINDING_FIELDS = Object.freeze({
  1: "id",
  2: "operation",
  3: "rangeMm",
  4: "targetMechanismId",
  5: "id",
  6: "activation",
  7: "targetGroundBlockerId",
  8: "id",
  9: "profileId",
  10: "faction",
  11: "duty",
  12: "maxHealth"
});

const VISIBLE_FIELDS = Object.freeze({
  player: new Set([
    "id",
    "regionId",
    "assetId",
    "initialSafePointId",
    "x",
    "y",
    "height",
    "floorLayer",
    "facingMillidegrees",
    "profileId",
    "faction",
    "duty",
    "maxHealth"
  ]),
  actors: new Set([
    "id",
    "regionId",
    "assetId",
    "x",
    "y",
    "height",
    "floorLayer",
    "facingMillidegrees"
  ]),
  groundBlockers: new Set([
    "id",
    "regionId",
    "assetId",
    "minX",
    "maxX",
    "minY",
    "maxY",
    "minHeight",
    "maxHeight",
    "floorLayer"
  ]),
  safePoints: new Set([
    "id",
    "regionId",
    "assetId",
    "x",
    "y",
    "height",
    "floorLayer",
    "facingMillidegrees"
  ]),
  interactions: new Set([
    "id",
    "regionId",
    "assetId",
    "x",
    "y",
    "height",
    "floorLayer",
    "facingMillidegrees",
    "operation",
    "rangeMm",
    "targetMechanismId"
  ]),
  mechanisms: new Set([
    "id",
    "regionId",
    "assetId",
    "x",
    "y",
    "height",
    "floorLayer",
    "facingMillidegrees",
    "activation",
    "targetGroundBlockerId"
  ])
});

function invalidPresentation() {
  const error = new Error("Sandbox diagnostic presentation input is invalid");
  error.code = "diagnostic_presentation_invalid";
  throw error;
}

function expectInteger(value, minimum, maximum) {
  if (!Number.isSafeInteger(value) || value < minimum || value > maximum) {
    invalidPresentation();
  }
  return value;
}

function expectOptionalId(value) {
  if (value !== null && typeof value !== "string") {
    invalidPresentation();
  }
  return value;
}

function packageRecord(runtime, section, recordIndex) {
  const collections = {
    5: ["player", [runtime.player]],
    6: ["actors", runtime.actors],
    7: ["groundBlockers", runtime.groundBlockers],
    8: ["safePoints", runtime.safePoints],
    9: ["interactions", runtime.interactions],
    10: ["mechanisms", runtime.mechanisms]
  };
  const entry = collections[section];
  if (!entry || recordIndex >= entry[1].length) {
    return null;
  }
  return { group: entry[0], record: entry[1][recordIndex] };
}

function bindingRecord(runtime, domain, recordIndex, subjectId) {
  if (domain === 1) {
    const binding = runtime.interactionBindings[recordIndex];
    const record = binding
      ? runtime.interactions.find(({ id }) => id === binding.interactionId)
      : null;
    return record ? { group: "interactions", record } : null;
  }
  if (domain === 2) {
    const binding = runtime.mechanismBindings[recordIndex];
    const record = binding
      ? runtime.mechanisms.find(({ id }) => id === binding.mechanismId)
      : null;
    return record ? { group: "mechanisms", record } : null;
  }
  if (domain === 3) {
    const record = runtime.interactions[recordIndex];
    return record ? { group: "interactions", record } : null;
  }
  if (domain === 4) {
    const record = runtime.mechanisms[recordIndex];
    return record ? { group: "mechanisms", record } : null;
  }
  if (domain === 5) {
    const binding = runtime.actorBindings[recordIndex];
    const record = binding
      ? runtime.actors.find(({ id }) => id === binding.actorId)
      : null;
    return record ? { group: "actors", record } : null;
  }
  if (domain === 6) {
    const record = runtime.actors.find(({ id }) => id === subjectId);
    return record ? { group: "actors", record } : null;
  }
  return null;
}

function locatorFor(entry, field, subjectId) {
  if (
    !entry ||
    subjectId === null ||
    entry.record.id !== subjectId ||
    !VISIBLE_FIELDS[entry.group]?.has(field)
  ) {
    return null;
  }
  return Object.freeze({
    group: entry.group,
    stableId: entry.record.id,
    field
  });
}

function presentPackageDiagnostic(raw, runtime) {
  const code = expectInteger(raw?.code, 1, 33);
  const severity = expectInteger(raw?.severity, 1, 2);
  const section = expectInteger(raw?.section, 0, 13);
  const fieldCode = expectInteger(raw?.field, 0, 38);
  const recordIndex = expectInteger(raw?.recordIndex, 0, 0xffffffff);
  const subjectId = expectOptionalId(raw?.subjectId);
  expectOptionalId(raw?.relatedId);
  const field = PACKAGE_FIELDS[fieldCode] ?? null;
  return Object.freeze({
    severity: severity === 1 ? "error" : "warning",
    message: PACKAGE_MESSAGES[code],
    locator: field
      ? locatorFor(packageRecord(runtime, section, recordIndex), field, subjectId)
      : null
  });
}

function presentBindingDiagnostic(raw, runtime) {
  const code = expectInteger(raw?.code, 2, 30);
  const domain = expectInteger(raw?.domain, 1, 6);
  const fieldCode = expectInteger(raw?.field, 1, 12);
  const recordIndex = expectInteger(raw?.recordIndex, 0, 0xffffffff);
  const subjectId = expectOptionalId(raw?.subjectId);
  expectOptionalId(raw?.relatedId);
  const field = BINDING_FIELDS[fieldCode];
  return Object.freeze({
    severity: "error",
    message: BINDING_MESSAGES[code],
    locator: locatorFor(
      bindingRecord(runtime, domain, recordIndex, subjectId),
      field,
      subjectId
    )
  });
}

export function presentSandboxDiagnostics(result, runtime) {
  if (
    result === null ||
    typeof result !== "object" ||
    result.complete !== true ||
    !Array.isArray(result.diagnostics) ||
    result.bindingValidation === null ||
    typeof result.bindingValidation !== "object" ||
    runtime === null ||
    typeof runtime !== "object"
  ) {
    invalidPresentation();
  }
  const includeBinding = result.bindingValidation.code !== 1;
  const count = result.diagnostics.length + (includeBinding ? 1 : 0);
  if (count > MAX_PRESENTATION_DIAGNOSTICS) {
    invalidPresentation();
  }
  const diagnostics = result.diagnostics.map((raw) =>
    presentPackageDiagnostic(raw, runtime)
  );
  if (includeBinding) {
    diagnostics.push(presentBindingDiagnostic(result.bindingValidation, runtime));
  }
  return Object.freeze(diagnostics);
}

export { MAX_PRESENTATION_DIAGNOSTICS };
