const INT16_MIN = -32768;
const INT16_MAX = 32767;
const INT32_MIN = -2147483648;
const INT32_MAX = 2147483647;
const UINT32_MAX = 4294967295;

const USER_MESSAGES = Object.freeze({
  invalid_document: "文件字段格式不符合当前编辑器要求。当前草稿保持不变。",
  invalid_path: "请选择工作区内既有的相对 JSON 文件。",
  path_escape: "该路径不在当前工作区的安全范围内。",
  not_found: "未找到该作者文档。",
  access_denied: "当前用户无法读取或保存该文件。",
  document_too_large: "该作者文档超过当前编辑器允许的大小。",
  invalid_encoding: "该作者文档不是有效的 UTF-8 文本。",
  stale_revision: "草稿已在其他操作中更新，请检查当前字段后重试。",
  external_change: "磁盘文件已被外部修改，当前内存草稿没有被覆盖。",
  dirty_confirmation_required: "请明确确认是否丢弃当前未保存修改。",
  unknown_entity: "选中的对象已不存在，当前草稿保持不变。",
  invalid_request: "当前操作包含无法接受的字段值。",
  io_error: "文件操作未完成，原文件与当前草稿均保持不变。",
  save_failed: "保存未完成，当前草稿仍处于未保存状态。",
  no_document: "请先打开一个作者文档。",
  compiler_unavailable: "共享内容检查当前不可用。",
  check_in_flight: "共享内容检查正在进行，请等待本次检查完成。",
  package_not_ready: "当前草稿没有可导出的已准备包，请重新执行共享内容检查。",
  export_failed: "未能准备下载；作者草稿和已准备包保持不变。",
  duplicate_id: "Stable ID 已被其他对象使用，请换一个 ID。",
  required_object: "全包必须保留唯一玩家起点；可使用“复制并替换”建立新的玩家记录。",
  object_referenced: "该对象仍被必需字段引用；请先修改引用对象。",
  preview_unavailable: "当前构建没有可用的系统 Demo Preview Host。",
  preview_failed: "Preview 候选未就绪；上一份可见画面保持不变。"
});

const elements = {
  workspaceForm: document.querySelector("#workspace-form"),
  pathInput: document.querySelector("#path-input"),
  openButton: document.querySelector("#open-button"),
  reloadButton: document.querySelector("#reload-button"),
  saveButton: document.querySelector("#save-button"),
  schema: document.querySelector("#schema-value"),
  revision: document.querySelector("#revision-value"),
  savedRevision: document.querySelector("#saved-revision-value"),
  dirty: document.querySelector("#dirty-value"),
  openedPath: document.querySelector("#opened-path"),
  conflict: document.querySelector("#conflict-banner"),
  resolveConflictButton: document.querySelector("#resolve-conflict-button"),
  conflictDialog: document.querySelector("#conflict-dialog"),
  continueEditingButton: document.querySelector("#continue-editing-button"),
  loadDiskButton: document.querySelector("#load-disk-button"),
  createObjectButton: document.querySelector("#create-object-button"),
  duplicateObjectButton: document.querySelector("#duplicate-object-button"),
  deleteObjectButton: document.querySelector("#delete-object-button"),
  objectTree: document.querySelector("#object-tree"),
  sceneCanvas: document.querySelector("#scene-canvas"),
  sceneCanvasEmpty: document.querySelector("#scene-canvas-empty"),
  canvasZoom: document.querySelector("#canvas-zoom"),
  canvasSnap: document.querySelector("#canvas-snap"),
  selectionSummary: document.querySelector("#selection-summary"),
  inspectorForm: document.querySelector("#inspector-form"),
  inspectorFieldset: document.querySelector("#inspector-fieldset"),
  fieldGrid: document.querySelector("#field-grid"),
  applyButton: document.querySelector("#apply-button"),
  contentCheckButton: document.querySelector("#content-check-button"),
  packageExportButton: document.querySelector("#package-export-button"),
  contentCheckSummary: document.querySelector("#content-check-summary"),
  diagnosticList: document.querySelector("#diagnostic-list"),
  diagnosticCountLive: document.querySelector("#diagnostic-count-live"),
  previewLaunchButton: document.querySelector("#preview-launch-button"),
  previewReloadButton: document.querySelector("#preview-reload-button"),
  previewStatus: document.querySelector("#preview-status"),
  previewIdentity: document.querySelector("#preview-identity"),
  previewStage: document.querySelector("#preview-stage"),
  previewPlaceholder: document.querySelector("#preview-placeholder"),
  objectDialog: document.querySelector("#object-dialog"),
  objectDialogForm: document.querySelector("#object-dialog-form"),
  objectDialogTitle: document.querySelector("#object-dialog-title"),
  objectDialogSummary: document.querySelector("#object-dialog-summary"),
  objectIdInput: document.querySelector("#object-id-input"),
  objectLabelInput: document.querySelector("#object-label-input"),
  objectDialogError: document.querySelector("#object-dialog-error"),
  confirmObjectButton: document.querySelector("#confirm-object-button"),
  cancelObjectButton: document.querySelector("#cancel-object-button"),
  deleteDialog: document.querySelector("#delete-dialog"),
  deleteDialogForm: document.querySelector("#delete-dialog-form"),
  deleteDialogSummary: document.querySelector("#delete-dialog-summary"),
  deleteReferenceList: document.querySelector("#delete-reference-list"),
  deleteDialogError: document.querySelector("#delete-dialog-error"),
  confirmDeleteButton: document.querySelector("#confirm-delete-button"),
  cancelDeleteButton: document.querySelector("#cancel-delete-button"),
  status: document.querySelector("#status-live"),
  error: document.querySelector("#error-live")
};

const GROUPS = [
  {
    key: "player",
    label: "Player",
    singular: true,
    prefix: "player.system_demo",
    assetKind: "player",
    records: (runtime) => [runtime.player]
  },
  {
    key: "actors",
    label: "Actors",
    prefix: "actor.system_demo",
    assetKind: "actor",
    records: (runtime) => runtime.actors
  },
  {
    key: "groundBlockers",
    label: "Ground Blockers",
    prefix: "blocker.system_demo",
    assetKind: "obstacle",
    records: (runtime) => runtime.groundBlockers
  },
  {
    key: "safePoints",
    label: "Safe Points",
    prefix: "safe_point.system_demo",
    assetKind: "safe_point",
    records: (runtime) => runtime.safePoints
  },
  {
    key: "interactions",
    label: "Interactions",
    prefix: "interaction.system_demo",
    assetKind: "interaction",
    records: (runtime) => runtime.interactions
  },
  {
    key: "mechanisms",
    label: "Mechanisms",
    prefix: "mechanism.system_demo",
    assetKind: "mechanism",
    records: (runtime) => runtime.mechanisms
  },
  {
    key: "waves",
    label: "Waves",
    prefix: "wave.system_demo",
    authoringOnly: true,
    records: (runtime) => runtime.waves
  },
  {
    key: "objectives",
    label: "Objectives",
    prefix: "objective.system_demo",
    authoringOnly: true,
    records: (runtime) => runtime.objectives
  },
  {
    key: "craftMaterials",
    label: "Craft Materials",
    prefix: "material.craft",
    authoringOnly: true,
    records: (runtime) => runtime.craftMaterials
  },
  {
    key: "craftWorkstations",
    label: "Craft Workstations",
    prefix: "workstation.craft",
    assetKind: "interaction",
    authoringOnly: true,
    records: (runtime) => runtime.craftWorkstations
  },
  {
    key: "craftProcesses",
    label: "Craft Processes",
    prefix: "craft_process",
    authoringOnly: true,
    records: (runtime) => runtime.craftProcesses
  },
  {
    key: "craftSteps",
    label: "Craft Steps",
    prefix: "craft_step",
    authoringOnly: true,
    records: (runtime) => runtime.craftSteps
  }
];

let state = {
  opened: false,
  relativePath: null,
  documentLease: null,
  conflict: false,
  document: null,
  revision: null,
  savedRevision: null,
  dirty: false,
  lastError: null,
  contentCheck: {
    status: "unavailable",
    hasPreparedPackage: false,
    diagnostics: [],
    preparedPackageLease: null
  },
  preview: {
    available: false,
    publication: null
  }
};
let selectedGroup = "player";
let selectedId = null;
let treeActiveKey = "group:player";
let conflictTrigger = null;
let applyInFlight = false;
let saveInFlight = false;
let openInFlight = false;
let reloadInFlight = false;
let documentEpoch = 0;
let contentActionEpoch = 0;
let contentCheckInFlight = false;
let contentCheckRequestSequence = 0;
let packageExportInFlight = false;
let packageExportRequestSequence = 0;
let objectMutationInFlight = false;
let objectDialogMode = "create";
let canvasMutationInFlight = false;
let canvasDrag = null;
let canvasZoomPercent = 100;
let previewPublishInFlight = false;
let previewCandidate = null;
let livePreview = null;
let previewFeedback = null;
let activeFields = [];
const expandedGroups = new Set(GROUPS.map((group) => group.key));
const fieldBuffers = new Map();

function clearFeedback() {
  elements.error.textContent = "";
}

function setStatus(message) {
  elements.status.textContent = message;
}

function setError(message) {
  elements.error.textContent = message;
}

function userMessage(code) {
  return USER_MESSAGES[code] ?? "操作未完成，当前草稿保持不变。";
}

function presentError(error) {
  if (error.code === "external_change" && state.conflict) {
    setError("");
    setStatus("检测到磁盘文件变化。内存草稿仍保留，请打开冲突处理。");
    return;
  }
  setError(userMessage(error.code));
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    method: options.method ?? "GET",
    headers: options.body ? { "Content-Type": "application/json" } : {},
    body: options.body ? JSON.stringify(options.body) : undefined
  });
  const payload = await response.json();
  if (!response.ok) {
    const error = new Error(userMessage(payload.error?.code));
    error.code = payload.error?.code ?? "request_failed";
    if (payload.state) {
      if (options.deferErrorState) {
        error.state = payload.state;
      } else {
        state = payload.state;
      }
    }
    throw error;
  }
  return payload.state;
}

async function packageExportApi(body) {
  const response = await fetch("/api/package-export", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });
  if (!response.ok) {
    const payload = await response.json().catch(() => ({}));
    const error = new Error(userMessage(payload.error?.code));
    error.code = payload.error?.code ?? "export_failed";
    error.state = payload.state;
    throw error;
  }
  const disposition = response.headers.get("content-disposition") ?? "";
  const filename = /^attachment; filename="([A-Za-z0-9][A-Za-z0-9._-]{0,79}\.tgdsbx)"$/.exec(
    disposition
  )?.[1];
  const blob = await response.blob();
  if (!filename || blob.size === 0) {
    const error = new Error(userMessage("export_failed"));
    error.code = "export_failed";
    throw error;
  }
  return { filename, blob };
}

function triggerPackageDownload({ filename, blob }) {
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.hidden = true;
  anchor.tabIndex = -1;
  anchor.href = url;
  anchor.download = filename;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 0);
}

function groupByKey(key) {
  return GROUPS.find((group) => group.key === key);
}

function runtimeIds() {
  if (!state.document) {
    return new Set();
  }
  const runtime = state.document.runtime;
  return new Set([
    runtime.player.id,
    ...runtime.regions.map(({ id }) => id),
    ...runtime.assets.map(({ id }) => id),
    ...runtime.actors.map(({ id }) => id),
    ...runtime.groundBlockers.map(({ id }) => id),
    ...runtime.safePoints.map(({ id }) => id),
    ...runtime.interactions.map(({ id }) => id),
    ...runtime.mechanisms.map(({ id }) => id),
    ...runtime.waves.map(({ id }) => id),
    ...runtime.objectives.map(({ id }) => id),
    ...runtime.craftMaterials.map(({ id }) => id),
    ...runtime.craftWorkstations.map(({ id }) => id),
    ...runtime.craftProcesses.map(({ id }) => id),
    ...runtime.craftSteps.map(({ id }) => id)
  ]);
}

function nextObjectId(group, sourceId = null) {
  const occupied = runtimeIds();
  const stem =
    sourceId === null
      ? group.prefix + ".new"
      : sourceId.replace(/(?:\.copy(?:_\d+)?)?$/, "") + ".copy";
  if (!occupied.has(stem)) {
    return stem;
  }
  for (let index = 2; index < 1000; index += 1) {
    const candidate = stem + "_" + index;
    if (!occupied.has(candidate)) {
      return candidate;
    }
  }
  return group.prefix + ".new_" + Date.now();
}

function optionsForRegion() {
  return state.document?.runtime.regions.map(({ id }) => id) ?? [];
}

function optionsForAsset(group) {
  return (
    state.document?.runtime.assets
      .filter(({ kind }) => kind === group.assetKind)
      .map(({ id }) => id) ?? []
  );
}

function recordsFor(group) {
  return state.document ? group.records(state.document.runtime) : [];
}

function editorLabel(id) {
  return (
    state.document?.editor.items.find((item) => item.id === id)?.label ?? id
  );
}

function currentRecord() {
  if (!state.document) {
    return null;
  }
  const group = groupByKey(selectedGroup);
  return recordsFor(group).find((record) => record.id === selectedId) ?? null;
}

function objectBufferKey(group = selectedGroup, id = selectedId) {
  return group + "\u0000" + id;
}

function hasFieldBuffers() {
  return [...fieldBuffers.values()].some((buffer) => buffer.size > 0);
}

function authorActionInFlight() {
  return (
    applyInFlight ||
    saveInFlight ||
    openInFlight ||
    reloadInFlight ||
    objectMutationInFlight ||
    canvasMutationInFlight ||
    previewPublishInFlight
  );
}

function updateSaveAvailability() {
  const nativeDisabled = !state.opened || hasFieldBuffers();
  elements.saveButton.disabled = nativeDisabled;
  elements.saveButton.setAttribute(
    "aria-disabled",
    String(nativeDisabled || state.conflict || saveInFlight)
  );
  elements.saveButton.setAttribute("aria-busy", String(saveInFlight));
}

function updateApplyAvailability() {
  elements.applyButton.setAttribute("aria-disabled", String(applyInFlight));
  elements.applyButton.setAttribute("aria-busy", String(applyInFlight));
}

function contentCheckSummary(check) {
  switch (check.status) {
    case "unavailable":
      return "共享内容检查当前不可用。作者草稿保持不变。";
    case "idle":
      return state.opened
        ? "尚未执行共享内容检查。"
        : "打开作者文档后可执行共享内容检查。";
    case "compiling":
      return "正在执行共享内容检查。";
    case "publishing":
      return "内容检查已完成，正在准备包。";
    case "ready":
      return "包已准备；可导出下载，但尚未启动 Preview 或试玩。";
    case "stale":
      return check.hasPreparedPackage
        ? "当前草稿的共享内容检查结果已过期；上一份已准备包保持不变。"
        : "当前草稿的共享内容检查结果已过期；尚无已准备包。";
    case "validation_failed":
      return check.hasPreparedPackage
        ? "共享内容检查未通过；上一份已准备包保持不变。"
        : "共享内容检查未通过；尚无已准备包。";
    case "bridge_failed":
      return "共享内容检查未完成；作者草稿和已准备包状态未改变。";
    default:
      return "共享内容检查当前不可用。作者草稿保持不变。";
  }
}

function resolveDiagnosticLocator(locator) {
  if (
    locator === null ||
    typeof locator !== "object" ||
    (state.contentCheck.status !== "ready" &&
      state.contentCheck.status !== "validation_failed")
  ) {
    return null;
  }
  const group = groupByKey(locator.group);
  if (!group) {
    return null;
  }
  const record = recordsFor(group).find(({ id }) => id === locator.stableId);
  if (!record) {
    return null;
  }
  const field = fieldsFor(group, record).find(
    (candidate) => !candidate.section && candidate.name === locator.field
  );
  return field ? { group, record, field } : null;
}

function locateDiagnostic(locator) {
  const resolved = resolveDiagnosticLocator(locator);
  if (!resolved) {
    return;
  }
  selectedGroup = resolved.group.key;
  selectedId = resolved.record.id;
  expandedGroups.add(selectedGroup);
  treeActiveKey = "object:" + selectedGroup + ":" + selectedId;
  render();
  const target = elements.inspectorForm.elements.namedItem(resolved.field.name);
  target?.focus();
}

function renderContentCheck() {
  const check = state.contentCheck;
  elements.contentCheckSummary.dataset.status = check.status;
  elements.contentCheckSummary.textContent = contentCheckSummary(check);
  if (check.status !== "ready" && check.status !== "validation_failed") {
    elements.diagnosticCountLive.textContent = "";
  }
  const nativeDisabled = !state.opened || check.status === "unavailable";
  const interactionBlocked =
    hasFieldBuffers() ||
    state.conflict ||
    contentCheckInFlight ||
    authorActionInFlight() ||
    packageExportInFlight;
  elements.contentCheckButton.disabled = nativeDisabled;
  elements.contentCheckButton.setAttribute(
    "aria-disabled",
    String(nativeDisabled || interactionBlocked)
  );
  elements.contentCheckButton.setAttribute(
    "aria-busy",
    String(contentCheckInFlight)
  );
  const exportNativeDisabled = !state.opened;
  const exportBlocked =
    check.status !== "ready" ||
    typeof check.preparedPackageLease !== "string" ||
    hasFieldBuffers() ||
    state.conflict ||
    contentCheckInFlight ||
    authorActionInFlight() ||
    packageExportInFlight;
  elements.packageExportButton.disabled = exportNativeDisabled;
  elements.packageExportButton.setAttribute(
    "aria-disabled",
    String(exportNativeDisabled || exportBlocked)
  );
  elements.packageExportButton.setAttribute(
    "aria-busy",
    String(packageExportInFlight)
  );
  elements.diagnosticList.replaceChildren();
  for (const diagnostic of check.diagnostics) {
    const item = document.createElement("li");
    item.className = "diagnostic-item";
    const severity = document.createElement("span");
    severity.className = "diagnostic-severity";
    severity.textContent = diagnostic.severity === "warning" ? "提醒" : "错误";
    const message = document.createElement("p");
    message.className = "diagnostic-message";
    message.textContent = diagnostic.message;
    item.append(severity, message);
    if (resolveDiagnosticLocator(diagnostic.locator)) {
      const locate = document.createElement("button");
      locate.className = "button diagnostic-locator";
      locate.type = "button";
      locate.textContent = "定位到字段";
      locate.addEventListener("click", () =>
        locateDiagnostic(diagnostic.locator)
      );
      item.append(locate);
    }
    elements.diagnosticList.append(item);
  }
  renderPreviewControls();
}

function previewInteractionBlocked() {
  return (
    !state.opened ||
    !state.preview.available ||
    state.contentCheck.status !== "ready" ||
    typeof state.contentCheck.preparedPackageLease !== "string" ||
    hasFieldBuffers() ||
    state.conflict ||
    contentCheckInFlight ||
    packageExportInFlight ||
    authorActionInFlight()
  );
}

function renderPreviewControls() {
  const unavailable = !state.preview.available;
  const blocked = previewInteractionBlocked();
  elements.previewLaunchButton.disabled = unavailable || livePreview !== null;
  elements.previewLaunchButton.setAttribute(
    "aria-disabled",
    String(blocked || livePreview !== null)
  );
  elements.previewLaunchButton.setAttribute(
    "aria-busy",
    String(previewPublishInFlight && livePreview === null)
  );
  elements.previewReloadButton.disabled = unavailable || livePreview === null;
  elements.previewReloadButton.setAttribute(
    "aria-disabled",
    String(blocked || livePreview === null)
  );
  elements.previewReloadButton.setAttribute(
    "aria-busy",
    String(previewPublishInFlight && livePreview !== null)
  );
  elements.previewPlaceholder.hidden =
    livePreview !== null || previewCandidate !== null;

  if (previewFeedback) {
    elements.previewStatus.textContent = previewFeedback;
  } else if (unavailable) {
    elements.previewStatus.textContent =
      "当前构建尚未连接 Preview Host；作者草稿与 Export 仍可使用。";
  } else if (previewCandidate) {
    elements.previewStatus.textContent =
      "新候选正在隐藏启动；上一份可见画面保持运行。";
  } else if (livePreview) {
    elements.previewStatus.textContent =
      state.contentCheck.status === "ready"
        ? "Preview 正在运行；当前 fresh package 可安全重载。"
        : "Preview 正在运行；草稿已变化，上一份画面保持不变。";
  } else {
    elements.previewStatus.textContent =
      state.contentCheck.status === "ready"
        ? "fresh package 已准备，可 Launch 到真实系统 Demo Host。"
        : "先完成共享内容检查，再 Launch。";
  }
  const identity = livePreview ?? state.preview.publication;
  elements.previewIdentity.textContent = identity
    ? "generation " +
      identity.generation +
      " / " +
      identity.packageSha256.replace(/^sha256:/, "").slice(0, 12) +
      "… / " +
      identity.packageBytes +
      " bytes"
    : "generation - / package -";
}

function announceDiagnosticCount() {
  const count = state.contentCheck.diagnostics.length;
  elements.diagnosticCountLive.textContent =
    count === 0
      ? "共享内容检查完成，没有发现问题。"
      : "共享内容检查发现 " + count + " 个问题。";
}

function beginContentAction() {
  contentActionEpoch += 1;
  elements.diagnosticCountLive.textContent = "";
}

function markContentCheckStaleForFieldBuffer() {
  beginContentAction();
  if (state.opened && state.contentCheck.status !== "unavailable") {
    state = {
      ...state,
      contentCheck: {
        ...state.contentCheck,
        status: "stale"
      }
    };
    elements.diagnosticCountLive.textContent = "";
  }
}

function bufferedEntry(field) {
  return fieldBuffers.get(objectBufferKey())?.get(field.name) ?? null;
}

function recordFieldBuffer(field, input, invalid = false) {
  const key = objectBufferKey();
  let buffer = fieldBuffers.get(key);
  const original = String(field.value);
  const previous = buffer?.get(field.name) ?? null;
  const next =
    input.value === original ? null : { value: input.value, invalid };
  const changed =
    previous?.value !== next?.value || previous?.invalid !== next?.invalid;
  if (!changed) {
    updateSaveAvailability();
    return;
  }
  if (next === null) {
    buffer?.delete(field.name);
    if (buffer?.size === 0) {
      fieldBuffers.delete(key);
    }
  } else {
    if (!buffer) {
      buffer = new Map();
      fieldBuffers.set(key, buffer);
    }
    buffer.set(field.name, next);
  }
  markContentCheckStaleForFieldBuffer();
  renderContentCheck();
  updateSaveAvailability();
}

function clearCurrentBuffer() {
  fieldBuffers.delete(objectBufferKey());
}

function addSection(label) {
  const heading = document.createElement("p");
  heading.className = "field-section";
  heading.textContent = label;
  elements.fieldGrid.append(heading);
}

function textField(name, label, value, options = {}) {
  return {
    name,
    label,
    value,
    kind: options.kind ?? "text",
    min: options.min,
    max: options.max,
    readonly: options.readonly ?? false,
    options: options.options ?? null,
    wide: options.wide ?? false
  };
}

function placementFields(group, record) {
  return [
    textField("regionId", "Region ID", record.regionId, {
      kind: "enum",
      options: optionsForRegion()
    }),
    textField("assetId", "Stable Asset ID", record.assetId, {
      kind: "enum",
      options: optionsForAsset(group)
    }),
    textField("x", "Pose X", record.pose.x, {
      kind: "integer",
      min: INT32_MIN,
      max: INT32_MAX
    }),
    textField("y", "Pose Y", record.pose.y, {
      kind: "integer",
      min: INT32_MIN,
      max: INT32_MAX
    }),
    textField("height", "Height", record.pose.height, {
      kind: "integer",
      min: INT32_MIN,
      max: INT32_MAX
    }),
    textField("floorLayer", "Floor Layer", record.pose.floorLayer, {
      kind: "integer",
      min: INT16_MIN,
      max: INT16_MAX
    }),
    textField(
      "facingMillidegrees",
      "Facing (millidegrees)",
      record.facingMillidegrees,
      { kind: "integer", min: 0, max: UINT32_MAX }
    )
  ];
}

function fieldsFor(group, record) {
  const fields = [
    textField("id", "Stable ID（只读，可复制）", record.id, {
      readonly: true,
      wide: true
    }),
    textField("editorLabel", "作者标签", editorLabel(record.id), {
      wide: true
    })
  ];

  if (group.key === "waves") {
    fields.push(
      textField("regionId", "Region ID", record.regionId, {
        kind: "enum",
        options: optionsForRegion()
      }),
      textField(
        "predecessorWaveId",
        "Predecessor Wave",
        record.predecessorWaveId,
        {
          kind: "enum",
          options: ["", ...state.document.runtime.waves
            .filter((value) => value.id !== record.id)
            .map((value) => value.id)]
        }
      ),
      { section: "Wave Trigger" },
      textField("triggerKind", "Trigger Kind", record.trigger.kind, {
        kind: "enum",
        options: [
          "session_started",
          "interaction_completed",
          "mechanism_activated",
          "objective_completed",
          "wave_completed"
        ]
      }),
      textField("triggerTargetId", "Trigger Target ID", record.trigger.targetId, {
        kind: "enum",
        options: [
          "",
          ...state.document.runtime.interactions.map(({ id }) => id),
          ...state.document.runtime.mechanisms.map(({ id }) => id),
          ...state.document.runtime.objectives.map(({ id }) => id),
          ...state.document.runtime.waves.map(({ id }) => id)
        ]
      })
    );
    const spawns = state.document.runtime.waveSpawns
      .filter((spawn) => spawn.waveId === record.id)
      .sort((left, right) =>
        left.spawnOrder - right.spawnOrder ||
        left.actorId.localeCompare(right.actorId)
      );
    spawns.forEach((spawn, index) => {
      fields.push({ section: "Spawn " + (index + 1) });
      fields.push(
        textField("spawnActorId_" + index, "Actor ID", spawn.actorId, {
          kind: "enum",
          options: state.document.runtime.actors.map(({ id }) => id)
        }),
        textField("spawnDelayTicks_" + index, "Delay (ticks)", spawn.delayTicks, {
          kind: "integer",
          min: 0,
          max: UINT32_MAX
        }),
        textField("spawnOrder_" + index, "Spawn Order", spawn.spawnOrder, {
          kind: "integer",
          min: 0,
          max: UINT16_MAX
        })
      );
    });
    return fields;
  }

  if (group.key === "objectives") {
    fields.push(
      textField("regionId", "Region ID", record.regionId, {
        kind: "enum",
        options: optionsForRegion()
      }),
      textField(
        "predecessorObjectiveId",
        "Predecessor Objective",
        record.predecessorObjectiveId,
        {
          kind: "enum",
          options: ["", ...state.document.runtime.objectives
            .filter((value) => value.id !== record.id)
            .map((value) => value.id)]
        }
      ),
      { section: "Objective Completion" },
      textField("completionKind", "Completion Kind", record.completion.kind, {
        kind: "enum",
        options: [
          "interaction_completed",
          "mechanism_activated",
          "wave_completed"
        ]
      }),
      textField(
        "completionTargetId",
        "Completion Target ID",
        record.completion.targetId,
        {
          kind: "enum",
          options: [
            ...state.document.runtime.interactions.map(({ id }) => id),
            ...state.document.runtime.mechanisms.map(({ id }) => id),
            ...state.document.runtime.waves.map(({ id }) => id)
          ]
        }
      ),
      textField(
        "terminal",
        "Package Terminal",
        state.document.runtime.completionObjectiveId === record.id ? "yes" : "no",
        { kind: "enum", options: ["yes", "no"] }
      )
    );
    return fields;
  }

  if (group.key === "craftMaterials") {
    return fields;
  }

  if (group.key === "craftProcesses") {
    fields.push(
      textField("workstationId", "Workstation ID", record.workstationId, {
        kind: "enum",
        options: state.document.runtime.craftWorkstations.map(({ id }) => id)
      }),
      textField("needId", "Need ID", record.needId, { wide: true }),
      textField("outputItemId", "Output Item ID", record.outputItemId, {
        wide: true
      }),
      textField("trialStepId", "Trial Step ID", record.trialStepId, {
        kind: "enum",
        options: state.document.runtime.craftSteps
          .filter((step) => step.processId === record.id && step.kind === "trial")
          .map(({ id }) => id)
      })
    );
    const choices = state.document.runtime.craftMaterialChoices
      .filter((choice) => choice.processId === record.id)
      .sort((left, right) => left.materialId.localeCompare(right.materialId));
    choices.forEach((choice, index) => {
      fields.push({ section: "Material Choice " + (index + 1) });
      fields.push(
        textField("materialId_" + index, "Material ID", choice.materialId, {
          kind: "enum",
          options: state.document.runtime.craftMaterials.map(({ id }) => id)
        }),
        textField("materialOutcome_" + index, "Trial Outcome", choice.outcome, {
          kind: "enum",
          options: ["passes_trial", "requires_rework"]
        })
      );
    });
    return fields;
  }

  if (group.key === "craftSteps") {
    fields.push(
      textField("processId", "Craft Process ID", record.processId, {
        kind: "enum",
        options: state.document.runtime.craftProcesses.map(({ id }) => id)
      }),
      textField(
        "predecessorStepId",
        "Predecessor Step ID",
        record.predecessorStepId,
        {
          kind: "enum",
          options: [
            "",
            ...state.document.runtime.craftSteps
              .filter(
                (step) =>
                  step.id !== record.id && step.processId === record.processId
              )
              .map(({ id }) => id)
          ]
        }
      ),
      textField("actionId", "Action ID", record.actionId, { wide: true }),
      textField("kind", "Step Kind", record.kind, {
        kind: "enum",
        options: ["operation", "trial", "rework"]
      })
    );
    return fields;
  }

  if (group.key === "groundBlockers") {
    fields.push(
      textField("regionId", "Region ID", record.regionId, {
        kind: "enum",
        options: optionsForRegion()
      }),
      textField("assetId", "Stable Asset ID", record.assetId, {
        kind: "enum",
        options: optionsForAsset(group)
      }),
      ...[
        ["minX", "Min X", INT32_MIN, INT32_MAX],
        ["maxX", "Max X", INT32_MIN, INT32_MAX],
        ["minY", "Min Y", INT32_MIN, INT32_MAX],
        ["maxY", "Max Y", INT32_MIN, INT32_MAX],
        ["minHeight", "Min Height", INT32_MIN, INT32_MAX],
        ["maxHeight", "Max Height", INT32_MIN, INT32_MAX],
        ["floorLayer", "Floor Layer", INT16_MIN, INT16_MAX]
      ].map(([name, label, min, max]) =>
        textField(name, label, record[name], { kind: "integer", min, max })
      )
    );
    return fields;
  }

  fields.push(...placementFields(group, record));
  if (group.key === "actors") {
    const binding = state.document.runtime.actorBindings.find(
      (value) => value.actorId === record.id
    );
    if (binding) {
      fields.push({ section: "Actor Gameplay Binding（显式入包）" });
      fields.push(
        textField("profileId", "Content Catalog Profile ID", binding.profileId, {
          wide: true
        }),
        textField("faction", "Faction", binding.faction, {
          kind: "enum",
          options: ["hostile"]
        }),
        textField("duty", "Tactical Duty", binding.duty, {
          kind: "enum",
          options: ["pressure", "flanker", "harrier", "controller"]
        }),
        textField("maxHealth", "Max Health", binding.maxHealth, {
          kind: "integer",
          min: 1,
          max: 100000
        })
      );
    }
  }
  if (group.key === "player") {
    fields.splice(
      4,
      0,
      textField(
        "initialSafePointId",
        "Initial Safe Point ID",
        record.initialSafePointId,
        {
          kind: "enum",
          options: state.document.runtime.safePoints.map(({ id }) => id)
        }
      )
    );
  }

  if (group.key === "interactions") {
    const binding = state.document.runtime.interactionBindings.find(
      (value) => value.interactionId === record.id
    );
    if (binding) {
      fields.push({ section: "Interaction Binding（既有记录）" });
      fields.push(
        textField("operation", "Operation", binding.operation, {
          kind: "enum",
          options: ["operate"]
        }),
        textField("rangeMm", "Range (mm)", binding.rangeMm, {
          kind: "integer",
          min: 500,
          max: 3000
        }),
        textField(
          "targetMechanismId",
          "Target Mechanism ID",
          binding.targetMechanismId,
          {
            kind: "enum",
            options: state.document.runtime.mechanisms.map((value) => value.id)
          }
        )
      );
    }
  }

  if (group.key === "mechanisms") {
    const binding = state.document.runtime.mechanismBindings.find(
      (value) => value.mechanismId === record.id
    );
    if (binding) {
      fields.push({ section: "Mechanism Binding（既有记录）" });
      fields.push(
        textField("activation", "Activation", binding.activation, {
          kind: "enum",
          options: ["one_shot_activate"]
        }),
        textField(
          "targetGroundBlockerId",
          "Target Ground Blocker ID",
          binding.targetGroundBlockerId,
          {
            kind: "enum",
            options: state.document.runtime.groundBlockers.map(
              (value) => value.id
            )
          }
        )
      );
    }
  }
  return fields;
}

function renderField(field) {
  if (field.section) {
    addSection(field.section);
    return;
  }
  const label = document.createElement("label");
  label.className = "form-field" + (field.wide ? " wide" : "");
  label.textContent = field.label;

  let input;
  if (field.kind === "enum") {
    input = document.createElement("select");
    const options = [...field.options];
    if (!options.includes(String(field.value))) {
      options.unshift(String(field.value));
    }
    for (const value of options) {
      const option = document.createElement("option");
      option.value = value;
      option.textContent = value;
      input.append(option);
    }
  } else {
    input = document.createElement("input");
    input.type = "text";
    input.inputMode = field.kind === "integer" ? "numeric" : "text";
    input.autocomplete = "off";
    input.spellcheck = false;
  }
  const buffered = bufferedEntry(field);
  input.name = field.name;
  input.value = buffered?.value ?? String(field.value);
  input.readOnly = field.readonly;
  input.setAttribute("aria-describedby", "error-live");
  if (buffered?.invalid) {
    input.setAttribute("aria-invalid", "true");
  }
  input.addEventListener("input", () => {
    input.removeAttribute("aria-invalid");
    recordFieldBuffer(field, input, false);
    clearFeedback();
  });
  input.addEventListener("change", () => {
    input.removeAttribute("aria-invalid");
    recordFieldBuffer(field, input, false);
    clearFeedback();
  });
  input.addEventListener("keydown", (event) => {
    if (event.key !== "Escape" || field.readonly) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    input.value = String(field.value);
    input.removeAttribute("aria-invalid");
    recordFieldBuffer(field, input, false);
    clearFeedback();
    setStatus("已撤销当前字段的未应用输入。");
  });
  label.append(input);
  elements.fieldGrid.append(label);
}

function activeTreeItem() {
  return elements.objectTree.querySelector(
    '[data-tree-key="' + CSS.escape(treeActiveKey) + '"]'
  );
}

function visibleTreeItems() {
  return [...elements.objectTree.querySelectorAll('[role="treeitem"]')].filter(
    (item) => !item.closest('[role="group"][hidden]')
  );
}

function updateTreeActiveVisual() {
  for (const item of elements.objectTree.querySelectorAll('[role="treeitem"]')) {
    item.classList.toggle("tree-active", item.dataset.treeKey === treeActiveKey);
  }
  const active = activeTreeItem();
  if (active) {
    elements.objectTree.setAttribute("aria-activedescendant", active.id);
  } else {
    elements.objectTree.removeAttribute("aria-activedescendant");
  }
}

function setTreeActive(key) {
  treeActiveKey = key;
  updateTreeActiveVisual();
}

function selectObject(groupKey, id) {
  selectedGroup = groupKey;
  selectedId = id;
  treeActiveKey = "object:" + groupKey + ":" + id;
  clearFeedback();
  render();
  elements.objectTree.focus();
}

function selectGroup(groupKey) {
  selectedGroup = groupKey;
  selectedId = recordsFor(groupByKey(groupKey))[0]?.id ?? null;
  treeActiveKey = "group:" + groupKey;
  clearFeedback();
  render();
  elements.objectTree.focus();
}

function renderTree() {
  elements.objectTree.replaceChildren();
  let nodeIndex = 0;
  for (const group of GROUPS) {
    const records = recordsFor(group);
    const groupItem = document.createElement("li");
    const groupKey = "group:" + group.key;
    groupItem.id = "tree-node-" + nodeIndex;
    nodeIndex += 1;
    groupItem.className = "tree-item tree-group";
    groupItem.dataset.treeKey = groupKey;
    groupItem.dataset.nodeType = "group";
    groupItem.dataset.groupKey = group.key;
    groupItem.setAttribute("role", "treeitem");
    groupItem.setAttribute("aria-level", "1");
    groupItem.setAttribute(
      "aria-expanded",
      String(expandedGroups.has(group.key))
    );

    const groupRow = document.createElement("div");
    groupRow.className = "tree-row";
    groupRow.textContent = group.label;
    const count = document.createElement("span");
    count.textContent = String(records.length);
    count.setAttribute("aria-label", records.length + " records");
    groupRow.append(count);
    groupRow.addEventListener("click", () => selectGroup(group.key));
    groupItem.append(groupRow);

    const childGroup = document.createElement("ul");
    childGroup.className = "tree-children";
    childGroup.setAttribute("role", "group");
    childGroup.hidden = !expandedGroups.has(group.key);
    for (const record of records) {
      const objectItem = document.createElement("li");
      const objectKey = "object:" + group.key + ":" + record.id;
      objectItem.id = "tree-node-" + nodeIndex;
      nodeIndex += 1;
      objectItem.className = "tree-item tree-object";
      objectItem.dataset.treeKey = objectKey;
      objectItem.dataset.nodeType = "object";
      objectItem.dataset.groupKey = group.key;
      objectItem.dataset.objectKind = group.key;
      objectItem.dataset.objectId = record.id;
      objectItem.setAttribute("role", "treeitem");
      objectItem.setAttribute("aria-level", "2");
      objectItem.setAttribute(
        "aria-selected",
        String(selectedGroup === group.key && selectedId === record.id)
      );
      const objectRow = document.createElement("div");
      objectRow.className = "tree-row";
      const label = document.createElement("span");
      label.textContent = editorLabel(record.id);
      const stableId = document.createElement("small");
      stableId.textContent = record.id;
      objectRow.append(label, stableId);
      objectRow.addEventListener("click", () =>
        selectObject(group.key, record.id)
      );
      objectItem.append(objectRow);
      childGroup.append(objectItem);
    }
    groupItem.append(childGroup);
    elements.objectTree.append(groupItem);
  }

  if (!activeTreeItem()) {
    treeActiveKey = state.opened
      ? "object:" + selectedGroup + ":" + selectedId
      : "group:player";
  }
  updateTreeActiveVisual();
}

function renderInspector() {
  elements.fieldGrid.replaceChildren();
  activeFields = [];
  const record = currentRecord();
  if (!record) {
    elements.inspectorFieldset.disabled = true;
    elements.selectionSummary.textContent = state.opened
      ? "当前分类没有记录"
      : "选择一个对象开始编辑";
    return;
  }

  elements.inspectorFieldset.disabled = false;
  elements.selectionSummary.textContent =
    selectedGroup +
    " / " +
    record.id +
    (fieldBuffers.get(objectBufferKey())?.size ? " / 有未应用输入" : "");
  activeFields = fieldsFor(groupByKey(selectedGroup), record);
  for (const field of activeFields) {
    renderField(field);
  }
}

function updateObjectActionAvailability() {
  const record = currentRecord();
  const group = groupByKey(selectedGroup);
  const blocked =
    !state.opened ||
    hasFieldBuffers() ||
    state.conflict ||
    authorActionInFlight() ||
    contentCheckInFlight ||
    packageExportInFlight;
  const authoringOnly = group?.authoringOnly === true;
  elements.createObjectButton.disabled = !state.opened || authoringOnly;
  elements.createObjectButton.setAttribute(
    "aria-disabled",
    String(blocked || authoringOnly)
  );
  elements.createObjectButton.textContent =
    authoringOnly ? "面板" : group?.singular ? "重建" : "新增";
  elements.duplicateObjectButton.disabled = !record || authoringOnly;
  elements.duplicateObjectButton.setAttribute(
    "aria-disabled",
    String(blocked || !record || authoringOnly)
  );
  elements.duplicateObjectButton.textContent =
    authoringOnly ? "不可复制" : group?.singular ? "复制并替换" : "复制";
  elements.deleteObjectButton.disabled =
    !record || group?.singular === true || authoringOnly;
  elements.deleteObjectButton.setAttribute(
    "aria-disabled",
    String(blocked || !record || group?.singular === true || authoringOnly)
  );
  elements.deleteObjectButton.title =
    authoringOnly
      ? "0.8.3 面板只编辑既有 Wave/Objective 拓扑"
      : group?.singular === true
        ? "全包必须保留唯一玩家起点"
        : "";
}

function svgElement(name, attributes = {}) {
  const element = document.createElementNS("http://www.w3.org/2000/svg", name);
  for (const [key, value] of Object.entries(attributes)) {
    element.setAttribute(key, String(value));
  }
  return element;
}

function canvasPoint(x, y) {
  const bounds = state.document.runtime.bounds;
  return { x, y: bounds.maxY - y };
}

function canvasViewBox() {
  const bounds = state.document.runtime.bounds;
  const zoom = canvasZoomPercent / 100;
  const width = (bounds.maxX - bounds.minX) / zoom;
  const height = (bounds.maxY - bounds.minY) / zoom;
  const centerX = (bounds.minX + bounds.maxX) / 2;
  const centerY = (bounds.maxY - bounds.minY) / 2;
  return [
    centerX - width / 2,
    centerY - height / 2,
    width,
    height
  ].join(" ");
}

function canvasObjectLabel(id) {
  const label = editorLabel(id);
  return label.length > 24 ? label.slice(0, 23) + "…" : label;
}

function appendCanvasPlacement(group, record) {
  const point = canvasPoint(record.pose.x, record.pose.y);
  if (group.key === "interactions") {
    const binding = state.document.runtime.interactionBindings.find(
      (candidate) => candidate.interactionId === record.id
    );
    if (binding) {
      elements.sceneCanvas.append(
        svgElement("circle", {
          class: "canvas-range",
          cx: point.x,
          cy: point.y,
          r: binding.rangeMm
        })
      );
    }
  }
  const node = svgElement("g", {
    class: "canvas-object",
    "data-kind": group.key,
    "data-object-id": record.id,
    "aria-label": group.label + " " + editorLabel(record.id),
    "aria-selected": selectedGroup === group.key && selectedId === record.id,
    role: "button",
    tabindex: "0"
  });
  const radius = group.key === "player" ? 230 : 175;
  node.append(
    svgElement(group.key === "actors" ? "polygon" : "circle", {
      class: "canvas-object-shape",
      ...(group.key === "actors"
        ? {
            points:
              point.x +
              "," +
              (point.y - radius) +
              " " +
              (point.x + radius) +
              "," +
              (point.y + radius) +
              " " +
              (point.x - radius) +
              "," +
              (point.y + radius)
          }
        : { cx: point.x, cy: point.y, r: radius })
    })
  );
  const angle =
    (Number(record.facingMillidegrees % 360000) / 1000 - 90) *
    (Math.PI / 180);
  node.append(
    svgElement("line", {
      class: "canvas-facing",
      x1: point.x,
      y1: point.y,
      x2: point.x + Math.cos(angle) * 460,
      y2: point.y + Math.sin(angle) * 460
    })
  );
  const label = svgElement("text", {
    class: "canvas-object-label",
    x: point.x + 260,
    y: point.y - 250
  });
  label.textContent = canvasObjectLabel(record.id);
  node.append(label);
  node.addEventListener("pointerdown", beginCanvasDrag);
  node.addEventListener("click", () => selectObject(group.key, record.id));
  node.addEventListener("keydown", (event) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      selectObject(group.key, record.id);
    }
  });
  elements.sceneCanvas.append(node);
}

function appendCanvasBlocker(group, record) {
  const topLeft = canvasPoint(record.minX, record.maxY);
  const node = svgElement("g", {
    class: "canvas-object",
    "data-kind": group.key,
    "data-object-id": record.id,
    "aria-label": group.label + " " + editorLabel(record.id),
    "aria-selected": selectedGroup === group.key && selectedId === record.id,
    role: "button",
    tabindex: "0"
  });
  node.append(
    svgElement("rect", {
      class: "canvas-blocker",
      x: topLeft.x,
      y: topLeft.y,
      width: record.maxX - record.minX,
      height: record.maxY - record.minY
    })
  );
  const label = svgElement("text", {
    class: "canvas-object-label",
    x: topLeft.x + 120,
    y: topLeft.y - 110
  });
  label.textContent = canvasObjectLabel(record.id);
  node.append(label);
  node.addEventListener("pointerdown", beginCanvasDrag);
  node.addEventListener("click", () => selectObject(group.key, record.id));
  elements.sceneCanvas.append(node);
}

function renderCanvas() {
  elements.sceneCanvas.replaceChildren();
  elements.sceneCanvasEmpty.hidden = state.opened;
  elements.canvasZoom.disabled = !state.opened;
  elements.canvasSnap.disabled = !state.opened;
  if (!state.opened) {
    elements.sceneCanvas.removeAttribute("viewBox");
    return;
  }
  const runtime = state.document.runtime;
  const bounds = runtime.bounds;
  elements.sceneCanvas.setAttribute("viewBox", canvasViewBox());
  elements.sceneCanvas.setAttribute(
    "aria-label",
    "Sandbox 二维场地，当前选择 " + (selectedId ?? "无")
  );
  const region = svgElement("rect", {
    class: "canvas-region",
    x: bounds.minX,
    y: 0,
    width: bounds.maxX - bounds.minX,
    height: bounds.maxY - bounds.minY
  });
  elements.sceneCanvas.append(region);
  for (
    let x = Math.ceil(bounds.minX / 1000) * 1000;
    x <= bounds.maxX;
    x += 1000
  ) {
    elements.sceneCanvas.append(
      svgElement("line", {
        class: "canvas-grid-line",
        x1: x,
        y1: 0,
        x2: x,
        y2: bounds.maxY - bounds.minY
      })
    );
  }
  for (
    let y = Math.ceil(bounds.minY / 1000) * 1000;
    y <= bounds.maxY;
    y += 1000
  ) {
    const canvasY = bounds.maxY - y;
    elements.sceneCanvas.append(
      svgElement("line", {
        class: "canvas-grid-line",
        x1: bounds.minX,
        y1: canvasY,
        x2: bounds.maxX,
        y2: canvasY
      })
    );
  }
  for (const group of GROUPS) {
    if (group.authoringOnly) {
      continue;
    }
    for (const record of recordsFor(group)) {
      if (group.key === "groundBlockers") {
        appendCanvasBlocker(group, record);
      } else {
        appendCanvasPlacement(group, record);
      }
    }
  }
}

function valuesForCanvasUpdate(group, record, deltaX, deltaY) {
  const label = editorLabel(record.id);
  if (group.key === "groundBlockers") {
    return {
      regionId: record.regionId,
      assetId: record.assetId,
      minX: record.minX + deltaX,
      maxX: record.maxX + deltaX,
      minY: record.minY + deltaY,
      maxY: record.maxY + deltaY,
      minHeight: record.minHeight,
      maxHeight: record.maxHeight,
      floorLayer: record.floorLayer,
      editorLabel: label
    };
  }
  const values = {
    regionId: record.regionId,
    assetId: record.assetId,
    pose: {
      ...record.pose,
      x: record.pose.x + deltaX,
      y: record.pose.y + deltaY
    },
    facingMillidegrees: record.facingMillidegrees,
    editorLabel: label
  };
  if (group.key === "player") {
    values.initialSafePointId = record.initialSafePointId;
  }
  if (group.key === "interactions") {
    const binding = state.document.runtime.interactionBindings.find(
      (candidate) => candidate.interactionId === record.id
    );
    values.binding = binding
      ? {
          operation: binding.operation,
          rangeMm: binding.rangeMm,
          targetMechanismId: binding.targetMechanismId
        }
      : null;
  }
  if (group.key === "actors") {
    const binding = state.document.runtime.actorBindings.find(
      (candidate) => candidate.actorId === record.id
    );
    values.binding = binding
      ? {
          profileId: binding.profileId,
          faction: binding.faction,
          duty: binding.duty,
          maxHealth: binding.maxHealth
        }
      : null;
  }
  if (group.key === "mechanisms") {
    const binding = state.document.runtime.mechanismBindings.find(
      (candidate) => candidate.mechanismId === record.id
    );
    values.binding = binding
      ? {
          activation: binding.activation,
          targetGroundBlockerId: binding.targetGroundBlockerId
        }
      : null;
  }
  return values;
}

function pointerInCanvas(event) {
  const matrix = elements.sceneCanvas.getScreenCTM();
  if (!matrix) {
    return null;
  }
  return new DOMPoint(event.clientX, event.clientY).matrixTransform(
    matrix.inverse()
  );
}

function beginCanvasDrag(event) {
  if (
    event.button !== 0 ||
    !state.opened ||
    hasFieldBuffers() ||
    authorActionInFlight()
  ) {
    return;
  }
  const group = groupByKey(event.currentTarget.dataset.kind);
  const id = event.currentTarget.dataset.objectId;
  const record = recordsFor(group).find((candidate) => candidate.id === id);
  const start = pointerInCanvas(event);
  if (!record || !start) {
    return;
  }
  event.preventDefault();
  selectObject(group.key, id);
  const node = elements.sceneCanvas.querySelector(
    '[data-kind="' +
      CSS.escape(group.key) +
      '"][data-object-id="' +
      CSS.escape(id) +
      '"]'
  );
  canvasDrag = {
    pointerId: event.pointerId,
    group,
    id,
    record: structuredClone(record),
    start,
    node,
    deltaX: 0,
    deltaY: 0
  };
  elements.sceneCanvas.setPointerCapture(event.pointerId);
}

function snapped(value) {
  return elements.canvasSnap.checked ? Math.round(value / 100) * 100 : Math.round(value);
}

async function commitCanvasMove(group, record, deltaX, deltaY) {
  if (deltaX === 0 && deltaY === 0) {
    renderCanvas();
    return;
  }
  canvasMutationInFlight = true;
  beginContentAction();
  renderContentCheck();
  updateObjectActionAvailability();
  clearFeedback();
  try {
    state = await api("/api/update", {
      method: "POST",
      body: {
        kind: group.key,
        id: record.id,
        values: valuesForCanvasUpdate(group, record, deltaX, deltaY),
        expectedRevision: state.revision
      }
    });
    render();
    setStatus(
      "画布已移动 " +
        record.id +
        "：ΔX " +
        deltaX +
        " mm / ΔY " +
        deltaY +
        " mm，revision " +
        state.revision +
        "。"
    );
  } catch (error) {
    render();
    presentError(error);
  } finally {
    canvasMutationInFlight = false;
    renderContentCheck();
    updateObjectActionAvailability();
  }
}

function referenceDescriptions(groupKey, id) {
  if (!state.document) {
    return [];
  }
  const runtime = state.document.runtime;
  const descriptions = [];
  if (groupKey === "actors") {
    for (const spawn of runtime.waveSpawns.filter(
      (candidate) => candidate.actorId === id
    )) {
      descriptions.push(
        "将同步移除波次 " + spawn.waveId + " 的 spawnOrder " + spawn.spawnOrder
      );
    }
  }
  if (
    groupKey === "safePoints" &&
    runtime.player.initialSafePointId === id
  ) {
    descriptions.push("玩家 Initial Safe Point 正在引用；删除会被拒绝");
  }
  if (groupKey === "interactions") {
    for (const binding of runtime.interactionBindings.filter(
      (candidate) => candidate.interactionId === id
    )) {
      descriptions.push(
        "将同步移除指向机关 " + binding.targetMechanismId + " 的 operate binding"
      );
    }
  }
  if (groupKey === "mechanisms") {
    for (const binding of runtime.interactionBindings.filter(
      (candidate) => candidate.targetMechanismId === id
    )) {
      descriptions.push("将同步移除互动点 " + binding.interactionId + " 的 binding");
    }
    for (const objective of runtime.objectives.filter(
      (candidate) =>
        candidate.completion.kind === "mechanism_activated" &&
        candidate.completion.targetId === id
    )) {
      descriptions.push("目标 " + objective.id + " 仍引用该机关，检查将报告诊断");
    }
  }
  if (groupKey === "groundBlockers") {
    for (const binding of runtime.mechanismBindings.filter(
      (candidate) => candidate.targetGroundBlockerId === id
    )) {
      descriptions.push("将同步移除机关 " + binding.mechanismId + " 的 blocker binding");
    }
  }
  return descriptions;
}

function render() {
  const documentValue = state.document;
  elements.schema.textContent = documentValue
    ? documentValue.schemaVersion
    : "未打开";
  elements.revision.textContent = state.revision ?? "-";
  elements.savedRevision.textContent = state.savedRevision ?? "-";
  elements.dirty.textContent = state.dirty ? "是" : "否";
  elements.openedPath.textContent = state.relativePath ?? "未打开";
  elements.conflict.hidden = !state.conflict;
  elements.reloadButton.disabled = !state.opened;
  if (state.relativePath && document.activeElement !== elements.pathInput) {
    elements.pathInput.value = state.relativePath;
  }
  renderTree();
  renderInspector();
  renderCanvas();
  renderContentCheck();
  renderPreviewControls();
  updateSaveAvailability();
  updateApplyAvailability();
  updateObjectActionAvailability();
}

function parseFormValues() {
  const raw = {};
  let firstInvalid = null;
  for (const field of activeFields) {
    if (field.section || field.readonly) {
      continue;
    }
    const input = elements.inspectorForm.elements.namedItem(field.name);
    input.removeAttribute("aria-invalid");
    if (field.kind === "integer") {
      const source = input.value.trim();
      const value = /^-?\d+$/.test(source) ? Number(source) : Number.NaN;
      if (
        !Number.isSafeInteger(value) ||
        value < field.min ||
        value > field.max
      ) {
        input.setAttribute("aria-invalid", "true");
        recordFieldBuffer(field, input, true);
        firstInvalid ??= input;
        continue;
      }
      raw[field.name] = value;
    } else {
      raw[field.name] = input.value;
    }
  }
  if (firstInvalid) {
    firstInvalid.focus();
    const error = new Error(
      "请输入字段允许范围内的十进制整数；无效值尚未写入草稿。"
    );
    error.code = "local_invalid";
    throw error;
  }
  return raw;
}

function requestValues(raw) {
  if (selectedGroup === "waves") {
    const record = currentRecord();
    const spawnCount = state.document.runtime.waveSpawns.filter(
      (spawn) => spawn.waveId === record.id
    ).length;
    const spawns = [];
    for (let index = 0; index < spawnCount; index += 1) {
      spawns.push({
        actorId: raw["spawnActorId_" + index],
        delayTicks: raw["spawnDelayTicks_" + index],
        spawnOrder: raw["spawnOrder_" + index]
      });
    }
    return {
      regionId: raw.regionId,
      predecessorWaveId: raw.predecessorWaveId,
      trigger: {
        kind: raw.triggerKind,
        targetId: raw.triggerTargetId
      },
      spawns,
      editorLabel: raw.editorLabel
    };
  }
  if (selectedGroup === "objectives") {
    return {
      regionId: raw.regionId,
      predecessorObjectiveId: raw.predecessorObjectiveId,
      completion: {
        kind: raw.completionKind,
        targetId: raw.completionTargetId
      },
      terminal: raw.terminal === "yes",
      editorLabel: raw.editorLabel
    };
  }
  if (selectedGroup === "craftMaterials") {
    return { editorLabel: raw.editorLabel };
  }
  if (selectedGroup === "craftProcesses") {
    const record = currentRecord();
    const materialCount = state.document.runtime.craftMaterialChoices.filter(
      (choice) => choice.processId === record.id
    ).length;
    const materialChoices = [];
    for (let index = 0; index < materialCount; index += 1) {
      materialChoices.push({
        materialId: raw["materialId_" + index],
        outcome: raw["materialOutcome_" + index]
      });
    }
    return {
      workstationId: raw.workstationId,
      needId: raw.needId,
      outputItemId: raw.outputItemId,
      trialStepId: raw.trialStepId,
      materialChoices,
      editorLabel: raw.editorLabel
    };
  }
  if (selectedGroup === "craftSteps") {
    return {
      processId: raw.processId,
      predecessorStepId: raw.predecessorStepId,
      actionId: raw.actionId,
      kind: raw.kind,
      editorLabel: raw.editorLabel
    };
  }
  if (selectedGroup === "groundBlockers") {
    return raw;
  }
  const values = {
    regionId: raw.regionId,
    assetId: raw.assetId,
    editorLabel: raw.editorLabel,
    pose: {
      x: raw.x,
      y: raw.y,
      height: raw.height,
      floorLayer: raw.floorLayer
    },
    facingMillidegrees: raw.facingMillidegrees
  };
  if (selectedGroup === "player") {
    values.initialSafePointId = raw.initialSafePointId;
  }
  if (selectedGroup === "actors") {
    values.binding = Object.prototype.hasOwnProperty.call(raw, "profileId")
      ? {
          profileId: raw.profileId,
          faction: raw.faction,
          duty: raw.duty,
          maxHealth: raw.maxHealth
        }
      : null;
  }
  if (selectedGroup === "interactions") {
    values.binding = Object.prototype.hasOwnProperty.call(raw, "operation")
      ? {
          operation: raw.operation,
          rangeMm: raw.rangeMm,
          targetMechanismId: raw.targetMechanismId
        }
      : null;
  }
  if (selectedGroup === "mechanisms") {
    values.binding = Object.prototype.hasOwnProperty.call(raw, "activation")
      ? {
          activation: raw.activation,
          targetGroundBlockerId: raw.targetGroundBlockerId
        }
      : null;
  }
  return values;
}

function needsDiscardConfirmation() {
  return state.dirty || hasFieldBuffers();
}

async function openDocument() {
  if (openInFlight || reloadInFlight) {
    return;
  }
  clearFeedback();
  const hadLocalChanges = needsDiscardConfirmation();
  const confirmDiscard = hadLocalChanges
    ? window.confirm(
        "当前有未保存修改或未应用输入。确认打开其他文档并丢弃这些内容吗？"
      )
    : false;
  if (hadLocalChanges && !confirmDiscard) {
    setStatus("已取消打开，内存草稿和表单输入保持不变。");
    return;
  }
  openInFlight = true;
  renderContentCheck();
  beginContentAction();
  try {
    state = await api("/api/open", {
      method: "POST",
      body: {
        relativePath: elements.pathInput.value,
        confirmDiscard
      }
    });
    documentEpoch += 1;
    fieldBuffers.clear();
    selectedGroup = "player";
    selectedId = state.document.runtime.player.id;
    treeActiveKey = "object:player:" + selectedId;
    clearFeedback();
    render();
    setStatus("已打开 " + state.relativePath + "。字段格式可保存，内容尚未校验。");
    elements.objectTree.focus();
  } catch (error) {
    if (state.relativePath) {
      elements.pathInput.value = state.relativePath;
    }
    render();
    presentError(error);
  } finally {
    openInFlight = false;
    renderContentCheck();
  }
}

async function reloadDocument(confirmFromConflict = false) {
  if (reloadInFlight || openInFlight) {
    return false;
  }
  clearFeedback();
  const hadLocalChanges = needsDiscardConfirmation();
  const confirmDiscard = confirmFromConflict
    ? true
    : hadLocalChanges
      ? window.confirm(
          "重新加载会丢弃当前未保存修改和未应用输入，是否继续？"
        )
      : false;
  if (hadLocalChanges && !confirmDiscard) {
    setStatus("已取消重新加载，内存草稿和表单输入保持不变。");
    return false;
  }
  reloadInFlight = true;
  renderContentCheck();
  beginContentAction();
  try {
    state = await api("/api/reload", {
      method: "POST",
      body: { confirmDiscard }
    });
    documentEpoch += 1;
    fieldBuffers.clear();
    const records = recordsFor(groupByKey(selectedGroup));
    if (!records.some((record) => record.id === selectedId)) {
      selectedGroup = "player";
      selectedId = state.document.runtime.player.id;
    }
    treeActiveKey = "object:" + selectedGroup + ":" + selectedId;
    clearFeedback();
    render();
    setStatus("已从磁盘重新加载 " + state.relativePath + "。");
    elements.objectTree.focus();
    return true;
  } catch (error) {
    render();
    presentError(error);
    return false;
  } finally {
    reloadInFlight = false;
    renderContentCheck();
  }
}

function captureFocusedControl() {
  const active = document.activeElement;
  if (!(active instanceof HTMLElement)) {
    return null;
  }
  return {
    id: active.id,
    name: active.getAttribute("name") ?? "",
    selectionStart:
      typeof active.selectionStart === "number" ? active.selectionStart : null,
    selectionEnd: typeof active.selectionEnd === "number" ? active.selectionEnd : null
  };
}

function restoreFocusedControl(snapshot) {
  if (!snapshot) {
    return;
  }
  let target = snapshot.id ? document.getElementById(snapshot.id) : null;
  if (!target && snapshot.name) {
    target = Array.from(elements.inspectorForm.elements).find(
      (element) => element.getAttribute("name") === snapshot.name
    );
  }
  if (!(target instanceof HTMLElement)) {
    return;
  }
  target.focus();
  if (
    snapshot.selectionStart !== null &&
    snapshot.selectionEnd !== null &&
    typeof target.setSelectionRange === "function"
  ) {
    target.setSelectionRange(snapshot.selectionStart, snapshot.selectionEnd);
  }
}

async function saveDocument() {
  if (!state.opened || state.conflict || saveInFlight) {
    return;
  }
  if (hasFieldBuffers()) {
    setError("请先应用属性，或在字段中按 Escape 撤销未应用输入。");
    return;
  }
  clearFeedback();
  const savingState = state;
  const savingRevision = savingState.revision;
  const savingEpoch = documentEpoch;
  beginContentAction();
  saveInFlight = true;
  updateSaveAvailability();
  renderContentCheck();
  elements.saveButton.focus();
  try {
    const savedState = await api("/api/save", {
      method: "POST",
      deferErrorState: true,
      body: {
        expectedRevision: savingState.revision
      }
    });
    if (
      documentEpoch !== savingEpoch ||
      savedState.revision !== state.revision
    ) {
      return;
    }
    const focusedControl = captureFocusedControl();
    state = savedState;
    render();
    restoreFocusedControl(focusedControl);
    if (state.dirty) {
      setStatus(
        "磁盘已保存 revision " +
          savingRevision +
          " 快照；较新的内存修改仍未保存。"
      );
    } else {
      setStatus("已保存 revision " + state.savedRevision + "。");
    }
  } catch (error) {
    if (
      documentEpoch !== savingEpoch ||
      state.revision !== savingRevision
    ) {
      return;
    }
    const focusedControl = captureFocusedControl();
    if (error.state) {
      state = error.state;
    }
    render();
    restoreFocusedControl(focusedControl);
    presentError(error);
  } finally {
    saveInFlight = false;
    updateSaveAvailability();
    renderContentCheck();
  }
}

async function checkContent() {
  if (
    !state.opened ||
    state.contentCheck.status === "unavailable" ||
    hasFieldBuffers() ||
    state.conflict ||
    contentCheckInFlight ||
    authorActionInFlight() ||
    packageExportInFlight
  ) {
    return;
  }
  const requestSequence = ++contentCheckRequestSequence;
  const checkingDocumentEpoch = documentEpoch;
  const checkingActionEpoch = contentActionEpoch;
  const checkingRevision = state.revision;
  const checkingDocumentLease = state.documentLease;
  contentCheckInFlight = true;
  state = {
    ...state,
    contentCheck: {
      ...state.contentCheck,
      status: "compiling"
    }
  };
  renderContentCheck();
  try {
    const checkedState = await api("/api/content-check", {
      method: "POST",
      deferErrorState: true,
      body: {
        expectedRevision: checkingRevision,
        expectedDocumentLease: checkingDocumentLease
      }
    });
    if (
      requestSequence !== contentCheckRequestSequence ||
      documentEpoch !== checkingDocumentEpoch ||
      contentActionEpoch !== checkingActionEpoch ||
      state.documentLease !== checkingDocumentLease ||
      state.revision !== checkingRevision ||
      checkedState.documentLease !== checkingDocumentLease ||
      checkedState.revision !== checkingRevision
    ) {
      return;
    }
    const focusedControl = captureFocusedControl();
    state = checkedState;
    render();
    restoreFocusedControl(focusedControl);
    if (
      state.contentCheck.status === "ready" ||
      state.contentCheck.status === "validation_failed"
    ) {
      announceDiagnosticCount();
    }
  } catch (error) {
    if (
      requestSequence !== contentCheckRequestSequence ||
      documentEpoch !== checkingDocumentEpoch ||
      contentActionEpoch !== checkingActionEpoch ||
      state.documentLease !== checkingDocumentLease ||
      state.revision !== checkingRevision
    ) {
      return;
    }
    const focusedControl = captureFocusedControl();
    if (error.state) {
      state = error.state;
    } else {
      state = {
        ...state,
        contentCheck: {
          ...state.contentCheck,
          status: "bridge_failed"
        }
      };
    }
    render();
    restoreFocusedControl(focusedControl);
  } finally {
    if (requestSequence === contentCheckRequestSequence) {
      contentCheckInFlight = false;
      renderContentCheck();
    }
  }
}

async function exportPackage() {
  if (
    !state.opened ||
    state.contentCheck.status !== "ready" ||
    typeof state.contentCheck.preparedPackageLease !== "string" ||
    hasFieldBuffers() ||
    state.conflict ||
    contentCheckInFlight ||
    authorActionInFlight() ||
    packageExportInFlight
  ) {
    return;
  }
  const requestSequence = ++packageExportRequestSequence;
  const exportingDocumentEpoch = documentEpoch;
  const exportingActionEpoch = contentActionEpoch;
  const exportingRevision = state.revision;
  const exportingDocumentLease = state.documentLease;
  const exportingPackageLease = state.contentCheck.preparedPackageLease;
  packageExportInFlight = true;
  renderContentCheck();
  clearFeedback();
  const isFresh = () =>
    requestSequence === packageExportRequestSequence &&
    documentEpoch === exportingDocumentEpoch &&
    contentActionEpoch === exportingActionEpoch &&
    state.documentLease === exportingDocumentLease &&
    state.revision === exportingRevision &&
    state.contentCheck.status === "ready" &&
    state.contentCheck.preparedPackageLease === exportingPackageLease &&
    !hasFieldBuffers() &&
    !state.conflict;
  try {
    const artifact = await packageExportApi({
      expectedRevision: exportingRevision,
      expectedDocumentLease: exportingDocumentLease,
      expectedPreparedPackageLease: exportingPackageLease
    });
    if (!isFresh()) {
      return;
    }
    triggerPackageDownload(artifact);
    setStatus(
      artifact.filename +
        " 已交给浏览器下载；尚未启动 Preview 或试玩。"
    );
  } catch (error) {
    if (!isFresh()) {
      return;
    }
    const focusedControl = captureFocusedControl();
    if (error.state) {
      state = error.state;
      render();
      restoreFocusedControl(focusedControl);
    }
    if (!error.code) {
      error.code = "export_failed";
    }
    presentError(error);
  } finally {
    packageExportInFlight = false;
    renderContentCheck();
  }
}

function discardPreviewCandidate(message) {
  if (!previewCandidate) {
    return;
  }
  clearTimeout(previewCandidate.timeout);
  previewCandidate.frame.remove();
  previewCandidate = null;
  previewFeedback =
    message ?? "Preview 候选未就绪；上一份可见画面保持不变。";
  renderPreviewControls();
}

function stagePreviewCandidate(publication, mode) {
  discardPreviewCandidate(null);
  previewFeedback = null;
  const frame = document.createElement("iframe");
  frame.className = "preview-frame preview-candidate";
  frame.title =
    "TianGongDu system Demo Preview generation " + publication.generation;
  frame.src = publication.url;
  frame.hidden = true;
  frame.setAttribute("allow", "autoplay");
  elements.previewStage.append(frame);
  previewCandidate = {
    publication,
    mode,
    frame,
    timeout: window.setTimeout(
      () =>
        discardPreviewCandidate(
          "Preview 候选在 30 秒内没有报告 ready；上一份画面保持不变。"
        ),
      30_000
    )
  };
  renderPreviewControls();
}

async function publishPreview(mode) {
  if (previewPublishInFlight || previewInteractionBlocked()) {
    return;
  }
  const revision = state.revision;
  const documentLease = state.documentLease;
  const packageLease = state.contentCheck.preparedPackageLease;
  previewPublishInFlight = true;
  previewFeedback = null;
  renderPreviewControls();
  clearFeedback();
  try {
    const publishedState = await api("/api/preview-publish", {
      method: "POST",
      deferErrorState: true,
      body: {
        expectedRevision: revision,
        expectedDocumentLease: documentLease,
        expectedPreparedPackageLease: packageLease
      }
    });
    if (
      state.revision !== revision ||
      state.documentLease !== documentLease ||
      state.contentCheck.preparedPackageLease !== packageLease
    ) {
      return;
    }
    state = publishedState;
    stagePreviewCandidate(state.preview.publication, mode);
    setStatus(
      (mode === "reload" ? "Safe Reload" : "Launch") +
        " 已准备 generation " +
        state.preview.publication.generation +
        "；等待真实 Host ready。"
    );
  } catch (error) {
    if (error.state) {
      state = error.state;
    }
    previewFeedback = userMessage(error.code ?? "preview_failed");
    render();
    presentError(error);
  } finally {
    previewPublishInFlight = false;
    renderPreviewControls();
  }
}

window.addEventListener("message", (event) => {
  const candidate = previewCandidate;
  if (
    !candidate ||
    event.origin !== window.location.origin ||
    event.source !== candidate.frame.contentWindow ||
    event.data?.type !== "tgd-system-demo-preview-ready" ||
    event.data?.token !== candidate.publication.token ||
    event.data?.generation !== candidate.publication.generation
  ) {
    return;
  }
  clearTimeout(candidate.timeout);
  const previous = livePreview;
  candidate.frame.hidden = false;
  candidate.frame.className = "preview-frame preview-live";
  for (const frame of elements.previewStage.querySelectorAll(
    ".preview-frame.preview-live"
  )) {
    if (frame !== candidate.frame) {
      frame.remove();
    }
  }
  livePreview = {
    ...candidate.publication,
    runtimeState: event.data.state
  };
  previewCandidate = null;
  previewFeedback =
    (candidate.mode === "reload" ? "Safe Reload" : "Launch") +
    " 已提交 generation " +
    livePreview.generation +
    "；新画面已替换上一候选。";
  void previous;
  renderPreviewControls();
  setStatus(previewFeedback);
});

function openObjectDialog(mode) {
  if (
    !state.opened ||
    hasFieldBuffers() ||
    state.conflict ||
    authorActionInFlight()
  ) {
    return;
  }
  const group = groupByKey(selectedGroup);
  const source = currentRecord();
  if (mode === "duplicate" && !source) {
    return;
  }
  objectDialogMode = mode;
  elements.objectDialogTitle.textContent =
    mode === "duplicate"
      ? group.singular
        ? "复制并替换唯一玩家"
        : "复制 " + group.label
      : group.singular
        ? "重建唯一玩家"
        : "新增 " + group.label;
  elements.objectDialogSummary.textContent = group.singular
    ? "作者格式必须始终保留一个玩家；提交后新记录会原子替换旧玩家。"
    : mode === "duplicate"
      ? "复制会生成新 Stable ID、作者标签和偏移位置；关联 binding 使用同一受控结构。"
      : "从当前分类的受控结构创建记录；Gameplay 含义仍由共享检查决定。";
  elements.objectIdInput.value = nextObjectId(
    group,
    mode === "duplicate" ? source.id : null
  );
  elements.objectLabelInput.value =
    mode === "duplicate"
      ? editorLabel(source.id) + " 副本"
      : group.label.replace(/s$/, "") + " New";
  elements.objectDialogError.textContent = "";
  elements.confirmObjectButton.textContent =
    group.singular ? "替换玩家" : mode === "duplicate" ? "创建副本" : "创建对象";
  elements.objectDialog.showModal();
  requestAnimationFrame(() => {
    elements.objectIdInput.focus();
    elements.objectIdInput.select();
  });
}

async function submitObjectDialog() {
  if (objectMutationInFlight || !state.opened) {
    return;
  }
  const group = groupByKey(selectedGroup);
  const source = currentRecord();
  const id = elements.objectIdInput.value.trim();
  const label = elements.objectLabelInput.value.trim();
  if (!id || !label) {
    elements.objectDialogError.textContent = "Stable ID 与作者标签不能为空。";
    return;
  }
  objectMutationInFlight = true;
  elements.confirmObjectButton.setAttribute("aria-busy", "true");
  elements.confirmObjectButton.setAttribute("aria-disabled", "true");
  elements.objectDialogError.textContent = "";
  clearFeedback();
  beginContentAction();
  try {
    state = await api("/api/object-create", {
      method: "POST",
      body: {
        kind: group.key,
        id,
        label,
        sourceId: objectDialogMode === "duplicate" ? source?.id ?? null : null,
        mode: objectDialogMode,
        expectedRevision: state.revision
      }
    });
    fieldBuffers.clear();
    selectedGroup = group.key;
    selectedId = id;
    expandedGroups.add(group.key);
    treeActiveKey = "object:" + group.key + ":" + id;
    elements.objectDialog.close("created");
    render();
    setStatus(
      (objectDialogMode === "duplicate" ? "已创建副本 " : "已创建 ") +
        id +
        "，revision " +
        state.revision +
        "。请执行共享内容检查确认引用与玩法语义。"
    );
  } catch (error) {
    render();
    elements.objectDialogError.textContent = userMessage(error.code);
  } finally {
    objectMutationInFlight = false;
    elements.confirmObjectButton.setAttribute("aria-busy", "false");
    elements.confirmObjectButton.setAttribute("aria-disabled", "false");
    renderContentCheck();
    updateObjectActionAvailability();
  }
}

function openDeleteDialog() {
  const group = groupByKey(selectedGroup);
  const record = currentRecord();
  if (
    !record ||
    group.singular ||
    hasFieldBuffers() ||
    state.conflict ||
    authorActionInFlight()
  ) {
    return;
  }
  elements.deleteDialogSummary.textContent =
    "将从作者文档删除 " +
    editorLabel(record.id) +
    "（" +
    record.id +
    "）。删除后仍需重新执行共享内容检查。";
  elements.deleteReferenceList.replaceChildren();
  const references = referenceDescriptions(group.key, record.id);
  for (const description of references.length > 0
    ? references
    : ["未发现由当前工具管理的直接反向引用。"]) {
    const item = document.createElement("li");
    item.textContent = description;
    elements.deleteReferenceList.append(item);
  }
  elements.deleteDialogError.textContent = "";
  elements.deleteDialog.showModal();
  requestAnimationFrame(() => elements.confirmDeleteButton.focus());
}

async function submitDeleteDialog() {
  if (objectMutationInFlight) {
    return;
  }
  const group = groupByKey(selectedGroup);
  const record = currentRecord();
  if (!record || group.singular) {
    return;
  }
  const deletedId = record.id;
  objectMutationInFlight = true;
  elements.confirmDeleteButton.setAttribute("aria-busy", "true");
  elements.confirmDeleteButton.setAttribute("aria-disabled", "true");
  elements.deleteDialogError.textContent = "";
  beginContentAction();
  try {
    state = await api("/api/object-delete", {
      method: "POST",
      body: {
        kind: group.key,
        id: deletedId,
        expectedRevision: state.revision
      }
    });
    fieldBuffers.delete(objectBufferKey(group.key, deletedId));
    const remaining = recordsFor(group);
    selectedId = remaining[0]?.id ?? null;
    treeActiveKey =
      selectedId === null
        ? "group:" + group.key
        : "object:" + group.key + ":" + selectedId;
    elements.deleteDialog.close("deleted");
    render();
    setStatus(
      "已删除 " +
        deletedId +
        "，revision " +
        state.revision +
        "。共享内容检查结果已过期。"
    );
  } catch (error) {
    render();
    elements.deleteDialogError.textContent = userMessage(error.code);
  } finally {
    objectMutationInFlight = false;
    elements.confirmDeleteButton.setAttribute("aria-busy", "false");
    elements.confirmDeleteButton.setAttribute("aria-disabled", "false");
    renderContentCheck();
    updateObjectActionAvailability();
  }
}

elements.sceneCanvas.addEventListener("pointermove", (event) => {
  if (!canvasDrag || canvasDrag.pointerId !== event.pointerId) {
    return;
  }
  const current = pointerInCanvas(event);
  if (!current) {
    return;
  }
  canvasDrag.deltaX = snapped(current.x - canvasDrag.start.x);
  canvasDrag.deltaY = snapped(-(current.y - canvasDrag.start.y));
  if (canvasDrag.node) {
    canvasDrag.node.setAttribute(
      "transform",
      "translate(" + canvasDrag.deltaX + " " + -canvasDrag.deltaY + ")"
    );
  }
});

elements.sceneCanvas.addEventListener("pointerup", (event) => {
  if (!canvasDrag || canvasDrag.pointerId !== event.pointerId) {
    return;
  }
  const completed = canvasDrag;
  canvasDrag = null;
  if (elements.sceneCanvas.hasPointerCapture(event.pointerId)) {
    elements.sceneCanvas.releasePointerCapture(event.pointerId);
  }
  void commitCanvasMove(
    completed.group,
    completed.record,
    completed.deltaX,
    completed.deltaY
  );
});

elements.sceneCanvas.addEventListener("pointercancel", (event) => {
  if (!canvasDrag || canvasDrag.pointerId !== event.pointerId) {
    return;
  }
  canvasDrag = null;
  renderCanvas();
});

elements.sceneCanvas.addEventListener("keydown", (event) => {
  if (
    !["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(event.key) ||
    !currentRecord() ||
    hasFieldBuffers() ||
    authorActionInFlight()
  ) {
    return;
  }
  event.preventDefault();
  const step = elements.canvasSnap.checked ? 100 : 1;
  const deltaX =
    event.key === "ArrowLeft" ? -step : event.key === "ArrowRight" ? step : 0;
  const deltaY =
    event.key === "ArrowDown" ? -step : event.key === "ArrowUp" ? step : 0;
  void commitCanvasMove(
    groupByKey(selectedGroup),
    structuredClone(currentRecord()),
    deltaX,
    deltaY
  );
});

elements.objectTree.addEventListener("keydown", (event) => {
  const items = visibleTreeItems();
  const active = activeTreeItem() ?? items[0];
  if (!active || items.length === 0) {
    return;
  }
  const index = items.indexOf(active);
  if (event.key === "ArrowDown" || event.key === "ArrowUp") {
    event.preventDefault();
    const direction = event.key === "ArrowUp" ? -1 : 1;
    const target = items[Math.max(0, Math.min(items.length - 1, index + direction))];
    setTreeActive(target.dataset.treeKey);
    return;
  }
  if (event.key === "Home" || event.key === "End") {
    event.preventDefault();
    const target = event.key === "Home" ? items[0] : items.at(-1);
    setTreeActive(target.dataset.treeKey);
    return;
  }
  if (event.key === "ArrowRight") {
    event.preventDefault();
    if (active.dataset.nodeType === "group") {
      const groupKey = active.dataset.groupKey;
      if (!expandedGroups.has(groupKey)) {
        expandedGroups.add(groupKey);
        renderTree();
      } else {
        const firstChild = active.querySelector('[role="treeitem"]');
        if (firstChild) {
          setTreeActive(firstChild.dataset.treeKey);
        }
      }
    }
    return;
  }
  if (event.key === "ArrowLeft") {
    event.preventDefault();
    if (active.dataset.nodeType === "object") {
      setTreeActive("group:" + active.dataset.groupKey);
    } else if (expandedGroups.has(active.dataset.groupKey)) {
      expandedGroups.delete(active.dataset.groupKey);
      renderTree();
    }
    return;
  }
  if (event.key === "Enter") {
    event.preventDefault();
    if (active.dataset.nodeType === "object") {
      selectObject(active.dataset.groupKey, active.dataset.objectId);
    } else {
      const groupKey = active.dataset.groupKey;
      if (expandedGroups.has(groupKey)) {
        expandedGroups.delete(groupKey);
      } else {
        expandedGroups.add(groupKey);
      }
      renderTree();
      elements.objectTree.focus();
    }
  }
});

elements.createObjectButton.addEventListener("click", () => {
  if (elements.createObjectButton.getAttribute("aria-disabled") !== "true") {
    openObjectDialog("create");
  }
});

elements.duplicateObjectButton.addEventListener("click", () => {
  if (elements.duplicateObjectButton.getAttribute("aria-disabled") !== "true") {
    openObjectDialog("duplicate");
  }
});

elements.deleteObjectButton.addEventListener("click", () => {
  if (elements.deleteObjectButton.getAttribute("aria-disabled") !== "true") {
    openDeleteDialog();
  }
});

elements.objectDialogForm.addEventListener("submit", (event) => {
  event.preventDefault();
  void submitObjectDialog();
});

elements.cancelObjectButton.addEventListener("click", () => {
  elements.objectDialog.close("cancel");
});

elements.deleteDialogForm.addEventListener("submit", (event) => {
  event.preventDefault();
  void submitDeleteDialog();
});

elements.cancelDeleteButton.addEventListener("click", () => {
  elements.deleteDialog.close("cancel");
});

elements.canvasZoom.addEventListener("input", () => {
  canvasZoomPercent = Number(elements.canvasZoom.value);
  renderCanvas();
});

elements.workspaceForm.addEventListener("submit", (event) => {
  event.preventDefault();
  void openDocument();
});

elements.reloadButton.addEventListener("click", () => {
  void reloadDocument(false);
});

elements.saveButton.addEventListener("click", () => {
  void saveDocument();
});

elements.contentCheckButton.addEventListener("click", () => {
  void checkContent();
});

elements.packageExportButton.addEventListener("click", () => {
  void exportPackage();
});

elements.previewLaunchButton.addEventListener("click", () => {
  if (elements.previewLaunchButton.getAttribute("aria-disabled") !== "true") {
    void publishPreview("launch");
  }
});

elements.previewReloadButton.addEventListener("click", () => {
  if (elements.previewReloadButton.getAttribute("aria-disabled") !== "true") {
    void publishPreview("reload");
  }
});

elements.inspectorForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (applyInFlight) {
    return;
  }
  applyInFlight = true;
  updateApplyAvailability();
  renderContentCheck();
  elements.applyButton.focus();
  clearFeedback();
  const submittedGroup = selectedGroup;
  const submittedId = selectedId;
  const submittedBufferKey = objectBufferKey();
  let preserveInvalidFieldFocus = false;
  try {
    const raw = parseFormValues();
    beginContentAction();
    state = await api("/api/update", {
      method: "POST",
      body: {
        kind: submittedGroup,
        id: submittedId,
        values: requestValues(raw),
        expectedRevision: state.revision
      }
    });
    fieldBuffers.delete(submittedBufferKey);
    render();
    setStatus(
      "已应用 " + submittedId + " 的属性，revision " + state.revision + "。"
    );
  } catch (error) {
    if (error.code === "local_invalid") {
      preserveInvalidFieldFocus = true;
      setError(error.message);
    } else {
      presentError(error);
    }
  } finally {
    applyInFlight = false;
    updateApplyAvailability();
    renderContentCheck();
    if (!preserveInvalidFieldFocus) {
      elements.applyButton.focus();
    }
  }
});

elements.resolveConflictButton.addEventListener("click", () => {
  conflictTrigger = elements.resolveConflictButton;
  elements.conflictDialog.showModal();
  requestAnimationFrame(() => elements.continueEditingButton.focus());
});

elements.continueEditingButton.addEventListener("click", () => {
  elements.conflictDialog.close("continue");
});

elements.loadDiskButton.addEventListener("click", async () => {
  const loaded = await reloadDocument(true);
  if (loaded) {
    conflictTrigger = elements.objectTree;
    elements.conflictDialog.close("reload");
  }
});

elements.conflictDialog.addEventListener("close", () => {
  const target = conflictTrigger;
  conflictTrigger = null;
  requestAnimationFrame(() => target?.focus());
});

document.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
    event.preventDefault();
    void saveDocument();
  }
});

try {
  state = await api("/api/state");
  if (state.opened) {
    documentEpoch += 1;
    selectedId = state.document.runtime.player.id;
    treeActiveKey = "object:player:" + selectedId;
  }
  render();
  if (state.opened) {
    elements.objectTree.focus();
  } else {
    elements.openButton.focus();
  }
} catch (error) {
  setError("无法连接本地工作台。当前页面没有修改任何文件。");
  render();
  elements.openButton.focus();
}
