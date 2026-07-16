import {
  createSandboxEditorState,
  reduceSandboxEditorState,
  serializeSandboxEditorState
} from "./editor-state.mjs";

const OBJECT_KINDS = new Set([
  "player",
  "actors",
  "groundBlockers",
  "safePoints",
  "interactions",
  "mechanisms"
]);

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

function expectBoolean(value, label) {
  if (typeof value !== "boolean") {
    fail("invalid_request", label + " must be a boolean");
  }
  return value;
}

function expectPose(value) {
  return expectExactObject(value, ["x", "y", "height", "floorLayer"], "pose");
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

  if (kind === "player") {
    expectExactObject(
      values,
      [
        "regionId",
        "assetId",
        "initialSafePointId",
        "pose",
        "facingMillidegrees"
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
        "floorLayer"
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
    return candidate;
  }

  const keys = ["regionId", "assetId", "pose", "facingMillidegrees"];
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

export function createWorkbenchController({ workspace }) {
  if (
    workspace === null ||
    typeof workspace !== "object" ||
    typeof workspace.read !== "function" ||
    typeof workspace.save !== "function"
  ) {
    fail("invalid_workspace", "workspace must provide read and save functions");
  }

  let editorState = null;
  let relativePath = null;
  let cas = null;
  let conflict = false;

  function view() {
    return {
      opened: editorState !== null,
      relativePath,
      cas,
      conflict,
      document: editorState?.document ?? null,
      revision: editorState?.revision ?? null,
      savedRevision: editorState?.savedRevision ?? null,
      dirty: editorState?.dirty ?? false,
      lastError: editorState?.lastError ?? null
    };
  }

  function requireOpen() {
    if (editorState === null) {
      fail("no_document", "open an authoring document first", 409);
    }
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

  async function open(request) {
    expectExactObject(
      request,
      ["relativePath", "confirmDiscard"],
      "open request"
    );
    requireDiscardConfirmation(request.confirmDiscard);

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
    return view();
  }

  async function reload(request) {
    expectExactObject(request, ["confirmDiscard"], "reload request");
    requireOpen();
    requireDiscardConfirmation(request.confirmDiscard);

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
    return view();
  }

  function updateObject(request) {
    expectExactObject(
      request,
      ["kind", "id", "values", "expectedRevision"],
      "update request"
    );
    requireOpen();
    if (!OBJECT_KINDS.has(request.kind)) {
      fail("invalid_request", "unsupported object kind");
    }
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
    const nextState = reduceSandboxEditorState(editorState, {
      type: "document.import",
      expectedRevision: request.expectedRevision,
      source: candidate
    });
    editorState = nextState;
    if (nextState.lastError) {
      fail(nextState.lastError.code, nextState.lastError.message, 422);
    }
    return view();
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
      fail("external_change", "document CAS no longer matches", 409);
    }

    const serialized = serializeSandboxEditorState(editorState);
    let saved;
    try {
      saved = await workspace.save({
        relativePath,
        expectedCas: cas,
        text: serialized
      });
    } catch (error) {
      if (error?.code === "external_change") {
        conflict = true;
      }
      throw convertExternalError(error, "save_failed");
    }

    cas = saved.cas;
    conflict = false;
    editorState = reduceSandboxEditorState(editorState, {
      type: "document.mark_saved",
      expectedRevision: revision
    });
    return view();
  }

  return Object.freeze({
    view,
    open,
    reload,
    updateObject,
    save,
    get editorState() {
      return editorState;
    }
  });
}
