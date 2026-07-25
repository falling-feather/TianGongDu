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
  export_failed: "未能准备下载；作者草稿和已准备包保持不变。"
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
  objectTree: document.querySelector("#object-tree"),
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
  status: document.querySelector("#status-live"),
  error: document.querySelector("#error-live")
};

const GROUPS = [
  { key: "player", label: "Player", records: (runtime) => [runtime.player] },
  { key: "actors", label: "Actors", records: (runtime) => runtime.actors },
  {
    key: "groundBlockers",
    label: "Ground Blockers",
    records: (runtime) => runtime.groundBlockers
  },
  {
    key: "safePoints",
    label: "Safe Points",
    records: (runtime) => runtime.safePoints
  },
  {
    key: "interactions",
    label: "Interactions",
    records: (runtime) => runtime.interactions
  },
  {
    key: "mechanisms",
    label: "Mechanisms",
    records: (runtime) => runtime.mechanisms
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
  return applyInFlight || saveInFlight || openInFlight || reloadInFlight;
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
      return "包已准备；尚未导出，也未启动 Preview 或试玩。";
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

function placementFields(record) {
  return [
    textField("regionId", "Region ID", record.regionId),
    textField("assetId", "Asset ID", record.assetId),
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
    })
  ];

  if (group.key === "groundBlockers") {
    fields.push(
      textField("regionId", "Region ID", record.regionId),
      textField("assetId", "Asset ID", record.assetId),
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

  fields.push(...placementFields(record));
  if (group.key === "player") {
    fields.splice(
      3,
      0,
      textField(
        "initialSafePointId",
        "Initial Safe Point ID",
        record.initialSafePointId
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
  renderContentCheck();
  updateSaveAvailability();
  updateApplyAvailability();
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
  if (selectedGroup === "groundBlockers") {
    return raw;
  }
  const values = {
    regionId: raw.regionId,
    assetId: raw.assetId,
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
