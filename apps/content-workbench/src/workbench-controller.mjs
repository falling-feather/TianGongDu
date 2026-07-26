import { createHash, randomBytes } from "node:crypto";

import { projectSandboxRuntimeDocument } from "./authoring-document.mjs";
import {
  createSandboxEditorState,
  reduceSandboxEditorState,
  serializeSandboxEditorState
} from "./editor-state.mjs";
import { presentSandboxDiagnostics } from "./sandbox-diagnostics-presentation.mjs";

const OBJECT_DEFINITIONS = Object.freeze({
  player: Object.freeze({ collection: null, assetKind: "player" }),
  actors: Object.freeze({ collection: "actors", assetKind: "actor" }),
  groundBlockers: Object.freeze({
    collection: "groundBlockers",
    assetKind: "obstacle"
  }),
  safePoints: Object.freeze({
    collection: "safePoints",
    assetKind: "safe_point"
  }),
  interactions: Object.freeze({
    collection: "interactions",
    assetKind: "interaction"
  }),
  mechanisms: Object.freeze({
    collection: "mechanisms",
    assetKind: "mechanism"
  })
});
const OBJECT_KINDS = new Set(Object.keys(OBJECT_DEFINITIONS));
const EMPTY_DIAGNOSTICS = Object.freeze([]);
const MAX_PREVIEW_PUBLICATIONS = 4;

import { createSandboxPackageExport } from "./sandbox-package-export.mjs";

export class WorkbenchControllerError extends Error {
  constructor(code, message, status = 400) {
    super(message);
    this.name = "WorkbenchControllerError";
    this.code = code;
    this.status = status;
  }
}

function fail(code, message, status = 400) {
  throw new WorkbenchControllerError(code, message, status);
}

function isPlainObject(value) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function expectExactObject(value, keys, label) {
  if (!isPlainObject(value)) {
    fail("invalid_request", label + " must be a plain object");
  }
  const actual = Reflect.ownKeys(value);
  if (
    actual.length !== keys.length ||
    actual.some((key) => typeof key !== "string" || !keys.includes(key))
  ) {
    fail("invalid_request", label + " has an invalid shape");
  }
  return value;
}

function expectRevision(value) {
  if (!Number.isSafeInteger(value) || value < 0) {
    fail("invalid_request", "expectedRevision must be a non-negative integer");
  }
  return value;
}

function createDocumentLease() {
  return randomBytes(24).toString("base64url");
}

function expectOpaqueLease(value, label) {
  if (
    typeof value !== "string" ||
    !/^[A-Za-z0-9_-]{32}$/.test(value)
  ) {
    fail("invalid_request", label + " is invalid");
  }
  return value;
}

function expectDocumentLease(value) {
  return expectOpaqueLease(value, "expectedDocumentLease");
}

function expectBoolean(value, label) {
  if (typeof value !== "boolean") {
    fail("invalid_request", label + " must be a boolean");
  }
  return value;
}

function expectString(value, label) {
  if (typeof value !== "string") {
    fail("invalid_request", label + " must be a string");
  }
  return value;
}

function expectNullableString(value, label) {
  if (value !== null && typeof value !== "string") {
    fail("invalid_request", label + " must be null or a string");
  }
  return value;
}

function expectObjectKind(value) {
  if (!OBJECT_KINDS.has(value)) {
    fail("invalid_request", "unsupported object kind");
  }
  return value;
}

function expectAuthorLabel(value) {
  expectString(value, "label");
  const trimmed = value.trim();
  if (trimmed.length === 0 || new TextEncoder().encode(trimmed).byteLength > 160) {
    fail("invalid_request", "label must contain 1 to 160 UTF-8 bytes");
  }
  return trimmed;
}

function expectPose(value) {
  return expectExactObject(value, ["x", "y", "height", "floorLayer"], "pose");
}

function editorItemIndex(document, id) {
  return document.editor.items.findIndex((item) => item.id === id);
}

function updateEditorLabel(document, id, label) {
  const index = editorItemIndex(document, id);
  if (index < 0) {
    return;
  }
  document.editor.items[index] = {
    ...document.editor.items[index],
    label: expectAuthorLabel(label)
  };
}

function placementFrom(record, values, extra = {}) {
  expectPose(values.pose);
  return {
    id: record.id,
    regionId: values.regionId,
    assetId: values.assetId,
    ...extra,
    pose: {
      x: values.pose.x,
      y: values.pose.y,
      height: values.pose.height,
      floorLayer: values.pose.floorLayer
    },
    facingMillidegrees: values.facingMillidegrees
  };
}

function findUnique(records, keyName, keyValue, label) {
  const matches = records
    .map((record, index) => ({ record, index }))
    .filter(({ record }) => record[keyName] === keyValue);
  if (matches.length === 0) {
    fail("unknown_entity", label + " was not found", 404);
  }
  if (matches.length > 1) {
    fail("ambiguous_entity", label + " is ambiguous", 409);
  }
  return matches[0];
}

function updateCandidate(document, kind, id, values) {
  const candidate = structuredClone(document);
  const hasEditorLabel =
    isPlainObject(values) &&
    Object.prototype.hasOwnProperty.call(values, "editorLabel");

  if (kind === "player") {
    expectExactObject(
      values,
      [
        "regionId",
        "assetId",
        "initialSafePointId",
        "pose",
        "facingMillidegrees",
        ...(hasEditorLabel ? ["editorLabel"] : [])
      ],
      "player values"
    );
    if (candidate.runtime.player.id !== id) {
      fail("unknown_entity", "player was not found", 404);
    }
    candidate.runtime.player = placementFrom(
      candidate.runtime.player,
      values,
      { initialSafePointId: values.initialSafePointId }
    );
    if (hasEditorLabel) {
      updateEditorLabel(candidate, id, values.editorLabel);
    }
    return candidate;
  }

  const collection = candidate.runtime[kind];
  const match = findUnique(collection, "id", id, kind + " record");

  if (kind === "groundBlockers") {
    expectExactObject(
      values,
      [
        "regionId",
        "assetId",
        "minX",
        "maxX",
        "minY",
        "maxY",
        "minHeight",
        "maxHeight",
        "floorLayer",
        ...(hasEditorLabel ? ["editorLabel"] : [])
      ],
      "ground blocker values"
    );
    collection[match.index] = {
      id,
      regionId: values.regionId,
      assetId: values.assetId,
      minX: values.minX,
      maxX: values.maxX,
      minY: values.minY,
      maxY: values.maxY,
      minHeight: values.minHeight,
      maxHeight: values.maxHeight,
      floorLayer: values.floorLayer
    };
    if (hasEditorLabel) {
      updateEditorLabel(candidate, id, values.editorLabel);
    }
    return candidate;
  }

  const keys = [
    "regionId",
    "assetId",
    "pose",
    "facingMillidegrees",
    ...(hasEditorLabel ? ["editorLabel"] : [])
  ];
  if (kind === "interactions" || kind === "mechanisms") {
    keys.push("binding");
  }
  expectExactObject(values, keys, kind + " values");
  collection[match.index] = placementFrom(match.record, values);

  if (kind === "interactions" && values.binding !== null) {
    expectExactObject(
      values.binding,
      ["operation", "rangeMm", "targetMechanismId"],
      "interaction binding"
    );
    const binding = findUnique(
      candidate.runtime.interactionBindings,
      "interactionId",
      id,
      "interaction binding"
    );
    candidate.runtime.interactionBindings[binding.index] = {
      interactionId: id,
      operation: values.binding.operation,
      rangeMm: values.binding.rangeMm,
      targetMechanismId: values.binding.targetMechanismId
    };
  }

  if (kind === "mechanisms" && values.binding !== null) {
    expectExactObject(
      values.binding,
      ["activation", "targetGroundBlockerId"],
      "mechanism binding"
    );
    const binding = findUnique(
      candidate.runtime.mechanismBindings,
      "mechanismId",
      id,
      "mechanism binding"
    );
    candidate.runtime.mechanismBindings[binding.index] = {
      mechanismId: id,
      activation: values.binding.activation,
      targetGroundBlockerId: values.binding.targetGroundBlockerId
    };
  }

  if (hasEditorLabel) {
    updateEditorLabel(candidate, id, values.editorLabel);
  }
  return candidate;
}

function allStableIds(document) {
  const runtime = document.runtime;
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
    ...runtime.objectives.map(({ id }) => id)
  ]);
}

function objectRecords(document, kind) {
  const definition = OBJECT_DEFINITIONS[kind];
  return definition.collection === null
    ? [document.runtime.player]
    : document.runtime[definition.collection];
}

function findObject(document, kind, id) {
  return findUnique(objectRecords(document, kind), "id", id, kind + " record")
    .record;
}

function firstAssetId(runtime, kind) {
  const asset = runtime.assets.find((candidate) => candidate.kind === kind);
  if (!asset) {
    fail("invalid_document", "no compatible Stable Asset is available", 422);
  }
  return asset.id;
}

function defaultPose(runtime, offset = 0) {
  return {
    x: Math.max(runtime.bounds.minX, Math.min(runtime.bounds.maxX, offset)),
    y: Math.max(runtime.bounds.minY, Math.min(runtime.bounds.maxY, offset)),
    height: Math.max(runtime.bounds.minHeight, 0),
    floorLayer: runtime.bounds.minFloorLayer
  };
}

function shiftedPose(runtime, pose, offset) {
  return {
    ...pose,
    x: Math.max(runtime.bounds.minX, Math.min(runtime.bounds.maxX, pose.x + offset)),
    y: Math.max(runtime.bounds.minY, Math.min(runtime.bounds.maxY, pose.y + offset))
  };
}

function nextEditorOrdinal(document) {
  return document.editor.items.reduce(
    (maximum, item) => Math.max(maximum, item.ordinal + 1),
    0
  );
}

function addEditorItem(document, id, label, sourceId) {
  const sourceIndex =
    sourceId === null ? -1 : editorItemIndex(document, sourceId);
  const source =
    sourceIndex < 0
      ? {
          canvasX: 80 + (nextEditorOrdinal(document) % 8) * 120,
          canvasY: 40 + Math.floor(nextEditorOrdinal(document) / 8) * 100
        }
      : document.editor.items[sourceIndex];
  document.editor.items.push({
    id,
    label: expectAuthorLabel(label),
    ordinal: nextEditorOrdinal(document),
    canvasX: source.canvasX + (sourceIndex < 0 ? 0 : 32),
    canvasY: source.canvasY + (sourceIndex < 0 ? 0 : 32)
  });
}

function nextSpawnOrder(runtime, waveId) {
  return runtime.waveSpawns.reduce(
    (maximum, spawn) =>
      spawn.waveId === waveId
        ? Math.max(maximum, spawn.spawnOrder + 1)
        : maximum,
    0
  );
}

function addObjectCandidate(document, kind, id, label, sourceId, mode) {
  const candidate = structuredClone(document);
  const runtime = candidate.runtime;
  const definition = OBJECT_DEFINITIONS[kind];
  if (allStableIds(candidate).has(id)) {
    fail("duplicate_id", "Stable ID is already in use", 409);
  }
  const offset = mode === "duplicate" ? 350 : 700;
  const source =
    sourceId === null
      ? objectRecords(candidate, kind)[0] ?? null
      : findObject(candidate, kind, sourceId);

  if (kind === "player") {
    const previousId = runtime.player.id;
    const base = source ?? runtime.player;
    runtime.player = {
      ...base,
      id,
      assetId: base.assetId ?? firstAssetId(runtime, definition.assetKind),
      pose: shiftedPose(runtime, base.pose ?? defaultPose(runtime), offset)
    };
    const previousEditorIndex = editorItemIndex(candidate, previousId);
    const previousEditor =
      previousEditorIndex < 0 ? null : candidate.editor.items[previousEditorIndex];
    candidate.editor.items = candidate.editor.items.filter(
      (item) => item.id !== previousId
    );
    candidate.editor.items.push({
      id,
      label: expectAuthorLabel(label),
      ordinal: previousEditor?.ordinal ?? nextEditorOrdinal(candidate),
      canvasX: (previousEditor?.canvasX ?? 80) + 32,
      canvasY: (previousEditor?.canvasY ?? 40) + 32
    });
    return candidate;
  }

  let record;
  if (kind === "groundBlockers") {
    const base =
      source ??
      {
        regionId: runtime.regions[0]?.id,
        assetId: firstAssetId(runtime, definition.assetKind),
        minX: -500,
        maxX: 500,
        minY: -250,
        maxY: 250,
        minHeight: 0,
        maxHeight: 1800,
        floorLayer: runtime.bounds.minFloorLayer
      };
    record = {
      ...base,
      id,
      minX: base.minX + offset,
      maxX: base.maxX + offset,
      minY: base.minY + offset,
      maxY: base.maxY + offset
    };
  } else {
    const base =
      source ??
      {
        regionId: runtime.regions[0]?.id,
        assetId: firstAssetId(runtime, definition.assetKind),
        pose: defaultPose(runtime),
        facingMillidegrees: 0
      };
    record = {
      ...base,
      id,
      assetId: base.assetId ?? firstAssetId(runtime, definition.assetKind),
      pose: shiftedPose(runtime, base.pose ?? defaultPose(runtime), offset)
    };
  }
  runtime[definition.collection].push(record);
  addEditorItem(candidate, id, label, source?.id ?? null);

  if (kind === "actors" && source) {
    const sourceSpawn = runtime.waveSpawns.find(
      (spawn) => spawn.actorId === source.id
    );
    if (sourceSpawn) {
      runtime.waveSpawns.push({
        waveId: sourceSpawn.waveId,
        actorId: id,
        delayTicks: sourceSpawn.delayTicks,
        spawnOrder: nextSpawnOrder(runtime, sourceSpawn.waveId)
      });
    }
  }
  if (kind === "interactions" && source) {
    const sourceBinding = runtime.interactionBindings.find(
      (binding) => binding.interactionId === source.id
    );
    if (sourceBinding) {
      runtime.interactionBindings.push({
        ...sourceBinding,
        interactionId: id
      });
    }
  }
  if (kind === "mechanisms" && source) {
    const sourceBinding = runtime.mechanismBindings.find(
      (binding) => binding.mechanismId === source.id
    );
    if (sourceBinding) {
      runtime.mechanismBindings.push({
        ...sourceBinding,
        mechanismId: id
      });
    }
  }
  return candidate;
}

function deleteObjectCandidate(document, kind, id) {
  if (kind === "player") {
    fail(
      "required_object",
      "the authoring document must retain exactly one player",
      409
    );
  }
  const candidate = structuredClone(document);
  const runtime = candidate.runtime;
  const collection = runtime[OBJECT_DEFINITIONS[kind].collection];
  const match = findUnique(collection, "id", id, kind + " record");
  if (
    kind === "safePoints" &&
    runtime.player.initialSafePointId === match.record.id
  ) {
    fail(
      "object_referenced",
      "the player's initial safe point cannot be deleted",
      409
    );
  }
  collection.splice(match.index, 1);
  candidate.editor.items = candidate.editor.items.filter(
    (item) => item.id !== id
  );
  if (kind === "actors") {
    runtime.waveSpawns = runtime.waveSpawns.filter(
      (spawn) => spawn.actorId !== id
    );
  }
  if (kind === "interactions") {
    runtime.interactionBindings = runtime.interactionBindings.filter(
      (binding) => binding.interactionId !== id
    );
  }
  if (kind === "mechanisms") {
    runtime.mechanismBindings = runtime.mechanismBindings.filter(
      (binding) => binding.mechanismId !== id
    );
    runtime.interactionBindings = runtime.interactionBindings.filter(
      (binding) => binding.targetMechanismId !== id
    );
  }
  if (kind === "groundBlockers") {
    runtime.mechanismBindings = runtime.mechanismBindings.filter(
      (binding) => binding.targetGroundBlockerId !== id
    );
  }
  return candidate;
}

function convertExternalError(error, fallbackCode) {
  if (error instanceof WorkbenchControllerError) {
    return error;
  }
  if (typeof error?.code === "string" && Number.isInteger(error?.status)) {
    return new WorkbenchControllerError(error.code, error.message, error.status);
  }
  return new WorkbenchControllerError(
    fallbackCode,
    error?.message ?? "Workbench operation failed",
    400
  );
}

function contentCheckState(
  status,
  hasPreparedPackage,
  diagnostics = EMPTY_DIAGNOSTICS,
  preparedPackageLease = null
) {
  return Object.freeze({
    status,
    hasPreparedPackage,
    diagnostics: Object.freeze([...diagnostics]),
    preparedPackageLease
  });
}

function requireCompilerService(value) {
  if (value === null) {
    return null;
  }
  if (
    typeof value !== "object" ||
    typeof value.identity !== "function" ||
    typeof value.compileAndPublish !== "function"
  ) {
    fail(
      "invalid_compiler_service",
      "compilerService must provide identity and compileAndPublish functions"
    );
  }
  return value;
}

function ownProviderIdentity(value) {
  if (
    value === null ||
    typeof value !== "object" ||
    !Number.isSafeInteger(value.generation) ||
    value.generation < 0 ||
    value.generation > 0xffffffff ||
    !Array.isArray(value.checksum) ||
    value.checksum.length !== 32 ||
    value.checksum.some(
      (byte) => !Number.isSafeInteger(byte) || byte < 0 || byte > 255
    )
  ) {
    throw new Error("Sandbox package service returned an invalid identity");
  }
  return Object.freeze({
    generation: value.generation,
    checksum: Object.freeze([...value.checksum])
  });
}

function sha256(value) {
  return "sha256:" + createHash("sha256").update(value).digest("hex");
}

function sandboxRuntimeProjection(document) {
  const runtime = projectSandboxRuntimeDocument(document);
  const bytes = Buffer.from(JSON.stringify(runtime), "utf8");
  return {
    runtime,
    projectionSha256: sha256(bytes)
  };
}

export function createWorkbenchController({
  workspace,
  compilerService = null,
  previewAvailable = false
}) {
  if (
    workspace === null ||
    typeof workspace !== "object" ||
    typeof workspace.read !== "function" ||
    typeof workspace.save !== "function"
  ) {
    fail("invalid_workspace", "workspace must provide read and save functions");
  }
  expectBoolean(previewAvailable, "previewAvailable");
  const service = requireCompilerService(compilerService);

  let editorState = null;
  let relativePath = null;
  let cas = null;
  let documentLease = createDocumentLease();
  let conflict = false;
  let documentEpoch = 0;
  let pendingSave = null;
  let activitySequence = 0;
  let checkSequence = 0;
  let checkInFlight = false;
  let validatedOwningPackage = null;
  let previewGeneration = 0;
  let latestPreviewPublication = null;
  const previewPublications = new Map();
  let contentCheck = contentCheckState(
    service ? "idle" : "unavailable",
    false
  );

  function view() {
    return {
      opened: editorState !== null,
      relativePath,
      cas,
      documentLease,
      conflict,
      document: editorState?.document ?? null,
      revision: editorState?.revision ?? null,
      savedRevision: editorState?.savedRevision ?? null,
      dirty: editorState?.dirty ?? false,
      lastError: editorState?.lastError ?? null,
      contentCheck,
      preview: Object.freeze({
        available: previewAvailable,
        publication:
          latestPreviewPublication === null
            ? null
            : Object.freeze({ ...latestPreviewPublication })
      })
    };
  }

  function hasPreparedPackage() {
    return validatedOwningPackage !== null;
  }

  function rotateDocumentLease() {
    documentLease = createDocumentLease();
  }

  function markContentCheckStale() {
    if (!service) {
      return;
    }
    contentCheck = contentCheckState(
      contentCheck.status === "idle" && !hasPreparedPackage() ? "idle" : "stale",
      hasPreparedPackage()
    );
  }

  function adoptOpenedDocumentCheckState() {
    if (!service) {
      contentCheck = contentCheckState("unavailable", hasPreparedPackage());
      return;
    }
    contentCheck = contentCheckState(
      hasPreparedPackage() || contentCheck.status !== "idle" ? "stale" : "idle",
      hasPreparedPackage()
    );
  }

  function requireOpen() {
    if (editorState === null) {
      fail("no_document", "open an authoring document first", 409);
    }
  }

  function adoptCandidate(candidate, expectedRevision) {
    const nextState = reduceSandboxEditorState(editorState, {
      type: "document.import",
      expectedRevision,
      source: candidate
    });
    editorState = nextState;
    if (nextState.lastError) {
      fail(nextState.lastError.code, nextState.lastError.message, 422);
    }
    activitySequence += 1;
    rotateDocumentLease();
    markContentCheckStale();
    return view();
  }

  function requireDiscardConfirmation(confirmDiscard) {
    expectBoolean(confirmDiscard, "confirmDiscard");
    if (editorState?.dirty && !confirmDiscard) {
      fail(
        "dirty_confirmation_required",
        "current unsaved changes require explicit discard confirmation",
        409
      );
    }
  }

  async function waitForPendingSave() {
    const pending = pendingSave;
    if (!pending) {
      return;
    }
    await pending.completion;
  }

  async function open(request) {
    expectExactObject(
      request,
      ["relativePath", "confirmDiscard"],
      "open request"
    );
    requireDiscardConfirmation(request.confirmDiscard);
    activitySequence += 1;
    rotateDocumentLease();
    await waitForPendingSave();

    let loaded;
    let nextState;
    try {
      loaded = await workspace.read(request.relativePath);
      nextState = createSandboxEditorState(loaded.text);
    } catch (error) {
      throw convertExternalError(error, "invalid_document");
    }

    editorState = nextState;
    relativePath = loaded.relativePath;
    cas = loaded.cas;
    conflict = false;
    documentEpoch += 1;
    adoptOpenedDocumentCheckState();
    return view();
  }

  async function reload(request) {
    expectExactObject(request, ["confirmDiscard"], "reload request");
    requireOpen();
    requireDiscardConfirmation(request.confirmDiscard);
    activitySequence += 1;
    rotateDocumentLease();
    await waitForPendingSave();

    let loaded;
    let nextState;
    try {
      loaded = await workspace.read(relativePath);
      nextState = createSandboxEditorState(loaded.text);
    } catch (error) {
      throw convertExternalError(error, "invalid_document");
    }

    editorState = nextState;
    cas = loaded.cas;
    conflict = false;
    documentEpoch += 1;
    adoptOpenedDocumentCheckState();
    return view();
  }

  function updateObject(request) {
    expectExactObject(
      request,
      ["kind", "id", "values", "expectedRevision"],
      "update request"
    );
    requireOpen();
    expectObjectKind(request.kind);
    if (typeof request.id !== "string") {
      fail("invalid_request", "object id must be a string");
    }
    expectRevision(request.expectedRevision);
    if (request.expectedRevision !== editorState.revision) {
      fail("stale_revision", "expected revision does not match", 409);
    }

    const candidate = updateCandidate(
      editorState.document,
      request.kind,
      request.id,
      request.values
    );
    return adoptCandidate(candidate, request.expectedRevision);
  }

  function createObject(request) {
    expectExactObject(
      request,
      [
        "kind",
        "id",
        "label",
        "sourceId",
        "mode",
        "expectedRevision"
      ],
      "create object request"
    );
    requireOpen();
    const kind = expectObjectKind(request.kind);
    const id = expectString(request.id, "id");
    const label = expectAuthorLabel(request.label);
    const sourceId = expectNullableString(request.sourceId, "sourceId");
    if (request.mode !== "create" && request.mode !== "duplicate") {
      fail("invalid_request", "mode must be create or duplicate");
    }
    expectRevision(request.expectedRevision);
    if (request.expectedRevision !== editorState.revision) {
      fail("stale_revision", "expected revision does not match", 409);
    }
    const candidate = addObjectCandidate(
      editorState.document,
      kind,
      id,
      label,
      sourceId,
      request.mode
    );
    return adoptCandidate(candidate, request.expectedRevision);
  }

  function deleteObject(request) {
    expectExactObject(
      request,
      ["kind", "id", "expectedRevision"],
      "delete object request"
    );
    requireOpen();
    const kind = expectObjectKind(request.kind);
    const id = expectString(request.id, "id");
    expectRevision(request.expectedRevision);
    if (request.expectedRevision !== editorState.revision) {
      fail("stale_revision", "expected revision does not match", 409);
    }
    const candidate = deleteObjectCandidate(editorState.document, kind, id);
    return adoptCandidate(candidate, request.expectedRevision);
  }

  async function save(request) {
    expectExactObject(
      request,
      ["expectedRevision", "expectedCas"],
      "save request"
    );
    requireOpen();
    const revision = expectRevision(request.expectedRevision);
    if (typeof request.expectedCas !== "string") {
      fail("invalid_request", "expectedCas must be a string");
    }
    if (revision !== editorState.revision) {
      fail("stale_revision", "expected revision does not match", 409);
    }
    if (conflict || request.expectedCas !== cas) {
      conflict = true;
      activitySequence += 1;
      rotateDocumentLease();
      markContentCheckStale();
      fail("external_change", "document CAS no longer matches", 409);
    }

    activitySequence += 1;
    rotateDocumentLease();
    markContentCheckStale();
    const savingEpoch = documentEpoch;
    const savingPath = relativePath;
    const savingCas = cas;
    const serialized = serializeSandboxEditorState(editorState);
    let completePendingSave;
    const operation = {
      relativePath: savingPath,
      completion: new Promise((resolve) => {
        completePendingSave = resolve;
      })
    };
    pendingSave = operation;
    try {
      let saved;
      try {
        saved = await workspace.save({
          relativePath: savingPath,
          expectedCas: savingCas,
          text: serialized
        });
      } catch (error) {
        if (
          error?.code === "external_change" &&
          documentEpoch === savingEpoch
        ) {
          conflict = true;
        }
        throw convertExternalError(error, "save_failed");
      }

      if (documentEpoch !== savingEpoch) {
        return view();
      }
      cas = saved.cas;
      conflict = false;
      if (editorState.revision === revision) {
        editorState = reduceSandboxEditorState(editorState, {
          type: "document.mark_saved",
          expectedRevision: revision
        });
      }
      return view();
    } finally {
      completePendingSave();
      if (pendingSave === operation) {
        pendingSave = null;
      }
    }
  }

  function checkContent(request) {
    expectExactObject(
      request,
      ["expectedRevision", "expectedDocumentLease"],
      "content check request"
    );
    requireOpen();
    const revision = expectRevision(request.expectedRevision);
    const expectedLease = expectDocumentLease(request.expectedDocumentLease);
    if (expectedLease !== documentLease) {
      fail("stale_revision", "document lease no longer matches", 409);
    }
    if (revision !== editorState.revision) {
      fail("stale_revision", "expected revision does not match", 409);
    }
    if (conflict) {
      fail("external_change", "document conflict must be resolved", 409);
    }
    if (!service) {
      fail("compiler_unavailable", "Sandbox compiler service is unavailable", 503);
    }
    if (checkInFlight) {
      fail("check_in_flight", "a content check is already running", 409);
    }

    const previousCheck = contentCheck;
    const checkingDocumentEpoch = documentEpoch;
    const checkingActivitySequence = activitySequence;
    const checkingSequence = ++checkSequence;
    const checkingDocument = editorState.lastValidDocument;
    checkInFlight = true;
    contentCheck = contentCheckState(
      "compiling",
      hasPreparedPackage(),
      previousCheck.diagnostics
    );

    const isFresh = () =>
      checkingSequence === checkSequence &&
      checkingDocumentEpoch === documentEpoch &&
      checkingActivitySequence === activitySequence &&
      editorState?.revision === revision &&
      editorState?.lastValidDocument === checkingDocument;

    try {
      const { runtime, projectionSha256 } =
        sandboxRuntimeProjection(checkingDocument);
      const expectedIdentity = ownProviderIdentity(service.identity());
      contentCheck = contentCheckState(
        "publishing",
        hasPreparedPackage(),
        previousCheck.diagnostics
      );
      const result = service.compileAndPublish(runtime, expectedIdentity);
      if (result && typeof result.then === "function") {
        throw new Error("Sandbox compiler service must submit synchronously");
      }
      if (!isFresh()) {
        contentCheck = contentCheckState("stale", hasPreparedPackage());
        return view();
      }

      const diagnostics = presentSandboxDiagnostics(result, runtime);
      if (result.outcome === 1) {
        if (
          !(result.packageBytes instanceof Uint8Array) ||
          result.packageBytes.byteLength === 0
        ) {
          throw new Error("published result did not include a canonical package");
        }
        const identity = ownProviderIdentity(result.identity);
        const packageBytes = new Uint8Array(result.packageBytes);
        const preparedPackageLease = createDocumentLease();
        validatedOwningPackage = {
          documentEpoch,
          revision,
          projectionSha256,
          identity,
          packageBytes,
          packageSha256: sha256(packageBytes),
          preparedPackageLease
        };
        contentCheck = contentCheckState(
          "ready",
          true,
          diagnostics,
          preparedPackageLease
        );
        return view();
      }
      if (result.outcome === 2) {
        contentCheck = contentCheckState(
          "validation_failed",
          hasPreparedPackage(),
          diagnostics
        );
        return view();
      }
      if (result.outcome === 3 || result.outcome === 4) {
        contentCheck = contentCheckState(
          "stale",
          hasPreparedPackage(),
          previousCheck.diagnostics
        );
        return view();
      }
      if (
        result.outcome === 5 ||
        result.outcome === 6 ||
        result.outcome === 7
      ) {
        contentCheck = contentCheckState(
          "bridge_failed",
          hasPreparedPackage(),
          previousCheck.diagnostics
        );
        return view();
      }
      throw new Error("Sandbox package service returned an unknown outcome");
    } catch {
      contentCheck = isFresh()
        ? contentCheckState(
            "bridge_failed",
            hasPreparedPackage(),
            previousCheck.diagnostics
          )
        : contentCheckState("stale", hasPreparedPackage());
      return view();
    } finally {
      checkInFlight = false;
    }
  }

  function requirePreparedPackage(request) {
    requireOpen();
    const revision = expectRevision(request.expectedRevision);
    const expectedDocumentLease = expectDocumentLease(
      request.expectedDocumentLease
    );
    const expectedPreparedPackageLease = expectOpaqueLease(
      request.expectedPreparedPackageLease,
      "expectedPreparedPackageLease"
    );
    if (
      expectedDocumentLease !== documentLease ||
      revision !== editorState.revision
    ) {
      fail("stale_revision", "package export document no longer matches", 409);
    }
    if (conflict) {
      fail("external_change", "document conflict must be resolved", 409);
    }
    if (
      contentCheck.status !== "ready" ||
      validatedOwningPackage === null ||
      contentCheck.preparedPackageLease === null
    ) {
      fail(
        "package_not_ready",
        "current document has no fresh prepared package",
        409
      );
    }
    if (
      expectedPreparedPackageLease !== contentCheck.preparedPackageLease ||
      expectedPreparedPackageLease !==
        validatedOwningPackage.preparedPackageLease ||
      validatedOwningPackage.documentEpoch !== documentEpoch ||
      validatedOwningPackage.revision !== revision
    ) {
      fail("stale_revision", "prepared package lease no longer matches", 409);
    }
    let currentProjectionSha256;
    try {
      currentProjectionSha256 = sandboxRuntimeProjection(
        editorState.lastValidDocument
      ).projectionSha256;
    } catch {
      fail(
        "package_not_ready",
        "current document projection is unavailable",
        409
      );
    }
    if (
      currentProjectionSha256 !== validatedOwningPackage.projectionSha256
    ) {
      fail(
        "stale_revision",
        "prepared package projection no longer matches",
        409
      );
    }
    return validatedOwningPackage;
  }

  function exportPackage(request) {
    expectExactObject(
      request,
      [
        "expectedRevision",
        "expectedDocumentLease",
        "expectedPreparedPackageLease"
      ],
      "package export request"
    );
    const prepared = requirePreparedPackage(request);
    return createSandboxPackageExport({
      relativePath,
      packageBytes: prepared.packageBytes,
      packageSha256: prepared.packageSha256
    });
  }

  function publishPreview(request) {
    expectExactObject(
      request,
      [
        "expectedRevision",
        "expectedDocumentLease",
        "expectedPreparedPackageLease"
      ],
      "preview publish request"
    );
    if (!previewAvailable) {
      fail("preview_unavailable", "system Demo Preview host is unavailable", 503);
    }
    const prepared = requirePreparedPackage(request);
    if (previewGeneration === Number.MAX_SAFE_INTEGER) {
      fail("preview_failed", "preview generation cannot advance", 409);
    }
    const token = createDocumentLease();
    const generation = previewGeneration + 1;
    const publication = {
      token,
      generation,
      revision: prepared.revision,
      packageSha256: prepared.packageSha256,
      packageBytes: prepared.packageBytes.byteLength,
      url:
        "/preview/tiangongdu-system-demo.html?workbenchPreview=" +
        token +
        "&generation=" +
        generation
    };
    const owned = {
      ...publication,
      bytes: new Uint8Array(prepared.packageBytes)
    };
    previewPublications.set(token, owned);
    while (previewPublications.size > MAX_PREVIEW_PUBLICATIONS) {
      previewPublications.delete(previewPublications.keys().next().value);
    }
    previewGeneration = generation;
    latestPreviewPublication = Object.freeze(publication);
    return view();
  }

  function previewPackage(token) {
    const owned = previewPublications.get(
      expectOpaqueLease(token, "preview token")
    );
    if (!owned) {
      fail("preview_not_found", "preview publication was not found", 404);
    }
    return Object.freeze({
      bytes: new Uint8Array(owned.bytes),
      packageSha256: owned.packageSha256,
      generation: owned.generation
    });
  }

  function validatedPackageEvidence() {
    if (!validatedOwningPackage) {
      return null;
    }
    return Object.freeze({
      documentEpoch: validatedOwningPackage.documentEpoch,
      revision: validatedOwningPackage.revision,
      projectionSha256: validatedOwningPackage.projectionSha256,
      generation: validatedOwningPackage.identity.generation,
      checksum: Object.freeze([...validatedOwningPackage.identity.checksum]),
      packageSha256: validatedOwningPackage.packageSha256,
      packageBytes: validatedOwningPackage.packageBytes.byteLength
    });
  }

  return Object.freeze({
    view,
    open,
    reload,
    updateObject,
    createObject,
    deleteObject,
    save,
    checkContent,
    exportPackage,
    publishPreview,
    previewPackage,
    validatedPackageEvidence,
    get editorState() {
      return editorState;
    }
  });
}
