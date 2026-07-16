export const SANDBOX_AUTHORING_FORMAT = "tgd.sandbox.authoring";
export const SANDBOX_AUTHORING_VERSION = "1.1.0";

export const SANDBOX_AUTHORING_CAPACITIES = Object.freeze({
  regions: 16,
  assets: 128,
  actors: 15,
  groundBlockers: 64,
  safePoints: 16,
  interactions: 64,
  mechanisms: 16,
  waves: 16,
  waveSpawns: 15,
  objectives: 64,
  interactionBindings: 64,
  mechanismBindings: 16,
  editorItems: 512
});

const MAX_ID_UTF8_BYTES = 96;
const MAX_LABEL_UTF8_BYTES = 1024;
const INT16_MIN = -32768;
const INT16_MAX = 32767;
const INT32_MIN = -2147483648;
const INT32_MAX = 2147483647;
const UINT16_MAX = 65535;
const UINT32_MAX = 4294967295;

const ASSET_KINDS = new Set([
  "player",
  "actor",
  "obstacle",
  "interaction",
  "mechanism",
  "safe_point",
  "effect"
]);

const TRIGGER_KINDS = new Set([
  "session_started",
  "interaction_completed",
  "mechanism_activated",
  "objective_completed",
  "wave_completed"
]);

const COMPLETION_KINDS = new Set([
  "interaction_completed",
  "mechanism_activated",
  "wave_completed"
]);

const textEncoder = new TextEncoder();

export class SandboxAuthoringFormatError extends Error {
  constructor(code, path, message) {
    super(path + ": " + message);
    this.name = "SandboxAuthoringFormatError";
    this.code = code;
    this.path = path;
  }
}

function fail(code, path, message) {
  throw new SandboxAuthoringFormatError(code, path, message);
}

function parseSource(source) {
  if (typeof source !== "string") {
    return source;
  }
  try {
    return JSON.parse(source);
  } catch {
    fail("invalid_json", "$", "source is not valid JSON");
  }
}

function isPlainObject(value) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function expectExactObject(value, path, expectedKeys) {
  if (!isPlainObject(value)) {
    fail("invalid_object", path, "expected a plain object");
  }
  const actualKeys = Reflect.ownKeys(value);
  for (const key of actualKeys) {
    if (typeof key !== "string" || !expectedKeys.includes(key)) {
      fail("unknown_field", path, "unknown field " + String(key));
    }
  }
  for (const key of expectedKeys) {
    if (!Object.prototype.hasOwnProperty.call(value, key)) {
      fail("missing_field", path, "missing field " + key);
    }
  }
  return value;
}

function expectString(value, path, maxUtf8Bytes) {
  if (typeof value !== "string") {
    fail("invalid_string", path, "expected a string");
  }
  if (textEncoder.encode(value).byteLength > maxUtf8Bytes) {
    fail("string_too_long", path, "UTF-8 string exceeds " + maxUtf8Bytes + " bytes");
  }
  return value;
}

function expectId(value, path) {
  return expectString(value, path, MAX_ID_UTF8_BYTES);
}

function expectInteger(value, path, minimum, maximum) {
  if (!Number.isSafeInteger(value)) {
    fail("invalid_integer", path, "expected a safe integer");
  }
  if (value < minimum || value > maximum) {
    fail(
      "integer_out_of_range",
      path,
      "expected an integer in [" + minimum + ", " + maximum + "]"
    );
  }
  return Object.is(value, -0) ? 0 : value;
}

function expectEnum(value, path, allowed) {
  if (typeof value !== "string" || !allowed.has(value)) {
    fail("unknown_enum", path, "unsupported enum value");
  }
  return value;
}

function compareStrings(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function compareStableTie(left, right) {
  return compareStrings(JSON.stringify(left), JSON.stringify(right));
}

function compareById(left, right) {
  return compareStrings(left.id, right.id) || compareStableTie(left, right);
}

function compareWaveSpawns(left, right) {
  return (
    compareStrings(left.waveId, right.waveId) ||
    left.spawnOrder - right.spawnOrder ||
    compareStrings(left.actorId, right.actorId) ||
    compareStableTie(left, right)
  );
}

function compareInteractionBindings(left, right) {
  return (
    compareStrings(left.interactionId, right.interactionId) ||
    compareStableTie(left, right)
  );
}

function compareMechanismBindings(left, right) {
  return (
    compareStrings(left.mechanismId, right.mechanismId) ||
    compareStableTie(left, right)
  );
}

function compareEditorItems(left, right) {
  return (
    left.ordinal - right.ordinal ||
    compareStrings(left.id, right.id) ||
    compareStableTie(left, right)
  );
}

function normalizeArray(value, path, capacity, normalizeRecord, comparator) {
  if (!Array.isArray(value)) {
    fail("invalid_array", path, "expected an array");
  }
  if (value.length > capacity) {
    fail("capacity_exceeded", path, "capacity " + capacity + " exceeded");
  }
  const records = value.map((record, index) =>
    normalizeRecord(record, path + "[" + index + "]")
  );
  records.sort(comparator);
  return records;
}

function normalizeBounds(value, path) {
  const source = expectExactObject(value, path, [
    "minX",
    "maxX",
    "minY",
    "maxY",
    "minHeight",
    "maxHeight",
    "minFloorLayer",
    "maxFloorLayer"
  ]);
  return {
    minX: expectInteger(source.minX, path + ".minX", INT32_MIN, INT32_MAX),
    maxX: expectInteger(source.maxX, path + ".maxX", INT32_MIN, INT32_MAX),
    minY: expectInteger(source.minY, path + ".minY", INT32_MIN, INT32_MAX),
    maxY: expectInteger(source.maxY, path + ".maxY", INT32_MIN, INT32_MAX),
    minHeight: expectInteger(
      source.minHeight,
      path + ".minHeight",
      INT32_MIN,
      INT32_MAX
    ),
    maxHeight: expectInteger(
      source.maxHeight,
      path + ".maxHeight",
      INT32_MIN,
      INT32_MAX
    ),
    minFloorLayer: expectInteger(
      source.minFloorLayer,
      path + ".minFloorLayer",
      INT16_MIN,
      INT16_MAX
    ),
    maxFloorLayer: expectInteger(
      source.maxFloorLayer,
      path + ".maxFloorLayer",
      INT16_MIN,
      INT16_MAX
    )
  };
}

function normalizePose(value, path) {
  const source = expectExactObject(value, path, [
    "x",
    "y",
    "height",
    "floorLayer"
  ]);
  return {
    x: expectInteger(source.x, path + ".x", INT32_MIN, INT32_MAX),
    y: expectInteger(source.y, path + ".y", INT32_MIN, INT32_MAX),
    height: expectInteger(
      source.height,
      path + ".height",
      INT32_MIN,
      INT32_MAX
    ),
    floorLayer: expectInteger(
      source.floorLayer,
      path + ".floorLayer",
      INT16_MIN,
      INT16_MAX
    )
  };
}

function normalizeRegion(value, path) {
  const source = expectExactObject(value, path, ["id", "bounds"]);
  return {
    id: expectId(source.id, path + ".id"),
    bounds: normalizeBounds(source.bounds, path + ".bounds")
  };
}

function normalizeAsset(value, path) {
  const source = expectExactObject(value, path, ["id", "kind"]);
  return {
    id: expectId(source.id, path + ".id"),
    kind: expectEnum(source.kind, path + ".kind", ASSET_KINDS)
  };
}

function normalizePlayer(value, path) {
  const source = expectExactObject(value, path, [
    "id",
    "regionId",
    "assetId",
    "initialSafePointId",
    "pose",
    "facingMillidegrees"
  ]);
  return {
    id: expectId(source.id, path + ".id"),
    regionId: expectId(source.regionId, path + ".regionId"),
    assetId: expectId(source.assetId, path + ".assetId"),
    initialSafePointId: expectId(
      source.initialSafePointId,
      path + ".initialSafePointId"
    ),
    pose: normalizePose(source.pose, path + ".pose"),
    facingMillidegrees: expectInteger(
      source.facingMillidegrees,
      path + ".facingMillidegrees",
      0,
      UINT32_MAX
    )
  };
}

function normalizePlacement(value, path) {
  const source = expectExactObject(value, path, [
    "id",
    "regionId",
    "assetId",
    "pose",
    "facingMillidegrees"
  ]);
  return {
    id: expectId(source.id, path + ".id"),
    regionId: expectId(source.regionId, path + ".regionId"),
    assetId: expectId(source.assetId, path + ".assetId"),
    pose: normalizePose(source.pose, path + ".pose"),
    facingMillidegrees: expectInteger(
      source.facingMillidegrees,
      path + ".facingMillidegrees",
      0,
      UINT32_MAX
    )
  };
}

function normalizeGroundBlocker(value, path) {
  const source = expectExactObject(value, path, [
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
  ]);
  return {
    id: expectId(source.id, path + ".id"),
    regionId: expectId(source.regionId, path + ".regionId"),
    assetId: expectId(source.assetId, path + ".assetId"),
    minX: expectInteger(source.minX, path + ".minX", INT32_MIN, INT32_MAX),
    maxX: expectInteger(source.maxX, path + ".maxX", INT32_MIN, INT32_MAX),
    minY: expectInteger(source.minY, path + ".minY", INT32_MIN, INT32_MAX),
    maxY: expectInteger(source.maxY, path + ".maxY", INT32_MIN, INT32_MAX),
    minHeight: expectInteger(
      source.minHeight,
      path + ".minHeight",
      INT32_MIN,
      INT32_MAX
    ),
    maxHeight: expectInteger(
      source.maxHeight,
      path + ".maxHeight",
      INT32_MIN,
      INT32_MAX
    ),
    floorLayer: expectInteger(
      source.floorLayer,
      path + ".floorLayer",
      INT16_MIN,
      INT16_MAX
    )
  };
}

function normalizeTrigger(value, path) {
  const source = expectExactObject(value, path, ["kind", "targetId"]);
  return {
    kind: expectEnum(source.kind, path + ".kind", TRIGGER_KINDS),
    targetId: expectId(source.targetId, path + ".targetId")
  };
}

function normalizeCompletion(value, path) {
  const source = expectExactObject(value, path, ["kind", "targetId"]);
  return {
    kind: expectEnum(source.kind, path + ".kind", COMPLETION_KINDS),
    targetId: expectId(source.targetId, path + ".targetId")
  };
}

function normalizeWave(value, path) {
  const source = expectExactObject(value, path, [
    "id",
    "regionId",
    "predecessorWaveId",
    "trigger"
  ]);
  return {
    id: expectId(source.id, path + ".id"),
    regionId: expectId(source.regionId, path + ".regionId"),
    predecessorWaveId: expectId(
      source.predecessorWaveId,
      path + ".predecessorWaveId"
    ),
    trigger: normalizeTrigger(source.trigger, path + ".trigger")
  };
}

function normalizeWaveSpawn(value, path) {
  const source = expectExactObject(value, path, [
    "waveId",
    "actorId",
    "delayTicks",
    "spawnOrder"
  ]);
  return {
    waveId: expectId(source.waveId, path + ".waveId"),
    actorId: expectId(source.actorId, path + ".actorId"),
    delayTicks: expectInteger(
      source.delayTicks,
      path + ".delayTicks",
      0,
      UINT32_MAX
    ),
    spawnOrder: expectInteger(
      source.spawnOrder,
      path + ".spawnOrder",
      0,
      UINT16_MAX
    )
  };
}

function normalizeObjective(value, path) {
  const source = expectExactObject(value, path, [
    "id",
    "regionId",
    "predecessorObjectiveId",
    "completion"
  ]);
  return {
    id: expectId(source.id, path + ".id"),
    regionId: expectId(source.regionId, path + ".regionId"),
    predecessorObjectiveId: expectId(
      source.predecessorObjectiveId,
      path + ".predecessorObjectiveId"
    ),
    completion: normalizeCompletion(source.completion, path + ".completion")
  };
}

function normalizeInteractionBinding(value, path) {
  const source = expectExactObject(value, path, [
    "interactionId",
    "operation",
    "rangeMm",
    "targetMechanismId"
  ]);
  return {
    interactionId: expectId(source.interactionId, path + ".interactionId"),
    operation:
      source.operation === "operate"
        ? source.operation
        : fail("unknown_enum", path + ".operation", "unsupported enum value"),
    rangeMm: expectInteger(source.rangeMm, path + ".rangeMm", 500, 3000),
    targetMechanismId: expectId(
      source.targetMechanismId,
      path + ".targetMechanismId"
    )
  };
}

function normalizeMechanismBinding(value, path) {
  const source = expectExactObject(value, path, [
    "mechanismId",
    "activation",
    "targetGroundBlockerId"
  ]);
  return {
    mechanismId: expectId(source.mechanismId, path + ".mechanismId"),
    activation:
      source.activation === "one_shot_activate"
        ? source.activation
        : fail("unknown_enum", path + ".activation", "unsupported enum value"),
    targetGroundBlockerId: expectId(
      source.targetGroundBlockerId,
      path + ".targetGroundBlockerId"
    )
  };
}

function normalizeEditorItem(value, path) {
  const source = expectExactObject(value, path, [
    "id",
    "label",
    "ordinal",
    "canvasX",
    "canvasY"
  ]);
  return {
    id: expectId(source.id, path + ".id"),
    label: expectString(source.label, path + ".label", MAX_LABEL_UTF8_BYTES),
    ordinal: expectInteger(source.ordinal, path + ".ordinal", 0, UINT32_MAX),
    canvasX: expectInteger(
      source.canvasX,
      path + ".canvasX",
      INT32_MIN,
      INT32_MAX
    ),
    canvasY: expectInteger(
      source.canvasY,
      path + ".canvasY",
      INT32_MIN,
      INT32_MAX
    )
  };
}

function normalizeRuntime(value, path) {
  const source = expectExactObject(value, path, [
    "packageId",
    "sandboxId",
    "bounds",
    "completionObjectiveId",
    "player",
    "regions",
    "assets",
    "actors",
    "groundBlockers",
    "safePoints",
    "interactions",
    "mechanisms",
    "waves",
    "waveSpawns",
    "objectives",
    "interactionBindings",
    "mechanismBindings"
  ]);
  return {
    packageId: expectId(source.packageId, path + ".packageId"),
    sandboxId: expectId(source.sandboxId, path + ".sandboxId"),
    bounds: normalizeBounds(source.bounds, path + ".bounds"),
    completionObjectiveId: expectId(
      source.completionObjectiveId,
      path + ".completionObjectiveId"
    ),
    player: normalizePlayer(source.player, path + ".player"),
    regions: normalizeArray(
      source.regions,
      path + ".regions",
      SANDBOX_AUTHORING_CAPACITIES.regions,
      normalizeRegion,
      compareById
    ),
    assets: normalizeArray(
      source.assets,
      path + ".assets",
      SANDBOX_AUTHORING_CAPACITIES.assets,
      normalizeAsset,
      compareById
    ),
    actors: normalizeArray(
      source.actors,
      path + ".actors",
      SANDBOX_AUTHORING_CAPACITIES.actors,
      normalizePlacement,
      compareById
    ),
    groundBlockers: normalizeArray(
      source.groundBlockers,
      path + ".groundBlockers",
      SANDBOX_AUTHORING_CAPACITIES.groundBlockers,
      normalizeGroundBlocker,
      compareById
    ),
    safePoints: normalizeArray(
      source.safePoints,
      path + ".safePoints",
      SANDBOX_AUTHORING_CAPACITIES.safePoints,
      normalizePlacement,
      compareById
    ),
    interactions: normalizeArray(
      source.interactions,
      path + ".interactions",
      SANDBOX_AUTHORING_CAPACITIES.interactions,
      normalizePlacement,
      compareById
    ),
    mechanisms: normalizeArray(
      source.mechanisms,
      path + ".mechanisms",
      SANDBOX_AUTHORING_CAPACITIES.mechanisms,
      normalizePlacement,
      compareById
    ),
    waves: normalizeArray(
      source.waves,
      path + ".waves",
      SANDBOX_AUTHORING_CAPACITIES.waves,
      normalizeWave,
      compareById
    ),
    waveSpawns: normalizeArray(
      source.waveSpawns,
      path + ".waveSpawns",
      SANDBOX_AUTHORING_CAPACITIES.waveSpawns,
      normalizeWaveSpawn,
      compareWaveSpawns
    ),
    objectives: normalizeArray(
      source.objectives,
      path + ".objectives",
      SANDBOX_AUTHORING_CAPACITIES.objectives,
      normalizeObjective,
      compareById
    ),
    interactionBindings: normalizeArray(
      source.interactionBindings,
      path + ".interactionBindings",
      SANDBOX_AUTHORING_CAPACITIES.interactionBindings,
      normalizeInteractionBinding,
      compareInteractionBindings
    ),
    mechanismBindings: normalizeArray(
      source.mechanismBindings,
      path + ".mechanismBindings",
      SANDBOX_AUTHORING_CAPACITIES.mechanismBindings,
      normalizeMechanismBinding,
      compareMechanismBindings
    )
  };
}

function normalizeEditor(value, path) {
  const source = expectExactObject(value, path, ["version", "items"]);
  return {
    version: expectInteger(source.version, path + ".version", 1, 1),
    items: normalizeArray(
      source.items,
      path + ".items",
      SANDBOX_AUTHORING_CAPACITIES.editorItems,
      normalizeEditorItem,
      compareEditorItems
    )
  };
}

function deepFreeze(value) {
  if (value === null || typeof value !== "object" || Object.isFrozen(value)) {
    return value;
  }
  for (const key of Reflect.ownKeys(value)) {
    deepFreeze(value[key]);
  }
  return Object.freeze(value);
}

function cloneValue(value) {
  if (Array.isArray(value)) {
    return value.map(cloneValue);
  }
  if (value !== null && typeof value === "object") {
    const clone = {};
    for (const key of Object.keys(value)) {
      Object.defineProperty(clone, key, {
        value: cloneValue(value[key]),
        enumerable: true,
        configurable: true,
        writable: true
      });
    }
    return clone;
  }
  return value;
}

export function normalizeSandboxAuthoringDocument(source) {
  const parsed = parseSource(source);
  const root = expectExactObject(parsed, "$", [
    "format",
    "schemaVersion",
    "runtime",
    "editor"
  ]);
  if (root.format !== SANDBOX_AUTHORING_FORMAT) {
    fail("unsupported_format", "$.format", "unsupported authoring format");
  }
  if (root.schemaVersion !== SANDBOX_AUTHORING_VERSION) {
    fail("unsupported_version", "$.schemaVersion", "unsupported schema version");
  }
  return deepFreeze({
    format: SANDBOX_AUTHORING_FORMAT,
    schemaVersion: SANDBOX_AUTHORING_VERSION,
    runtime: normalizeRuntime(root.runtime, "$.runtime"),
    editor: normalizeEditor(root.editor, "$.editor")
  });
}

export function migrateSandboxAuthoringDocument(source) {
  return normalizeSandboxAuthoringDocument(source);
}

export function serializeSandboxAuthoringDocument(document) {
  const normalized = normalizeSandboxAuthoringDocument(document);
  return JSON.stringify(normalized, null, 2) + "\n";
}

export function projectSandboxRuntimeDocument(document) {
  const normalized = normalizeSandboxAuthoringDocument(document);
  return deepFreeze(cloneValue(normalized.runtime));
}
