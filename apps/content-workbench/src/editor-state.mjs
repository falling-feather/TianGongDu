import {
  normalizeSandboxAuthoringDocument,
  serializeSandboxAuthoringDocument
} from "./authoring-document.mjs";

const COLLECTIONS = Object.freeze({
  regions: ["runtime", "regions", (record) => record.id, expectStringKey],
  assets: ["runtime", "assets", (record) => record.id, expectStringKey],
  actors: ["runtime", "actors", (record) => record.id, expectStringKey],
  groundBlockers: [
    "runtime",
    "groundBlockers",
    (record) => record.id,
    expectStringKey
  ],
  safePoints: ["runtime", "safePoints", (record) => record.id, expectStringKey],
  interactions: [
    "runtime",
    "interactions",
    (record) => record.id,
    expectStringKey
  ],
  mechanisms: [
    "runtime",
    "mechanisms",
    (record) => record.id,
    expectStringKey
  ],
  waves: ["runtime", "waves", (record) => record.id, expectStringKey],
  waveSpawns: [
    "runtime",
    "waveSpawns",
    waveSpawnKey,
    expectWaveSpawnKey
  ],
  objectives: ["runtime", "objectives", (record) => record.id, expectStringKey],
  interactionBindings: [
    "runtime",
    "interactionBindings",
    (record) => record.interactionId,
    expectStringKey
  ],
  mechanismBindings: [
    "runtime",
    "mechanismBindings",
    (record) => record.mechanismId,
    expectStringKey
  ],
  editorItems: ["editor", "items", (record) => record.id, expectStringKey]
});

const COMMAND_KEYS = Object.freeze({
  "entity.add": ["type", "expectedRevision", "collection", "value"],
  "entity.update": ["type", "expectedRevision", "collection", "key", "patch"],
  "entity.delete": ["type", "expectedRevision", "collection", "key"],
  "document.import": ["type", "expectedRevision", "source"],
  "document.mark_saved": ["type", "expectedRevision"]
});

function isPlainObject(value) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function expectStringKey(value) {
  if (typeof value !== "string") {
    throw new Error("entity key must be a string");
  }
  return value;
}

function waveSpawnKey(value) {
  return JSON.stringify([value.waveId, value.spawnOrder, value.actorId]);
}

function expectWaveSpawnKey(value) {
  if (!isPlainObject(value)) {
    throw new Error("wave spawn key must be an object");
  }
  const expected = ["waveId", "spawnOrder", "actorId"];
  const keys = Reflect.ownKeys(value);
  if (
    keys.length !== expected.length ||
    keys.some((key) => typeof key !== "string" || !expected.includes(key))
  ) {
    throw new Error("wave spawn key has an invalid shape");
  }
  if (
    typeof value.waveId !== "string" ||
    !Number.isSafeInteger(value.spawnOrder) ||
    typeof value.actorId !== "string"
  ) {
    throw new Error("wave spawn key has invalid primitive values");
  }
  return waveSpawnKey(value);
}

function cloneValue(value) {
  if (value !== null && typeof value === "object") {
    const isArray = Array.isArray(value);
    const clone = isArray ? [] : Object.create(Object.getPrototypeOf(value));
    for (const key of Reflect.ownKeys(value)) {
      if (isArray && key === "length") {
        continue;
      }
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      if (Object.prototype.hasOwnProperty.call(descriptor, "value")) {
        Object.defineProperty(clone, key, {
          value: cloneValue(descriptor.value),
          enumerable: descriptor.enumerable,
          configurable: true,
          writable: true
        });
        continue;
      }
      Object.defineProperty(clone, key, {
        get: descriptor.get,
        set: descriptor.set,
        enumerable: descriptor.enumerable,
        configurable: true,
      });
    }
    return clone;
  }
  return value;
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

function freezeState(fields) {
  return deepFreeze(fields);
}

function stateWithError(state, code, message) {
  return freezeState({
    document: state.document,
    revision: state.revision,
    savedRevision: state.savedRevision,
    dirty: state.dirty,
    lastValidDocument: state.lastValidDocument,
    lastError: { code, message }
  });
}

function stateWithoutError(state) {
  if (state.lastError === null) {
    return state;
  }
  return freezeState({
    document: state.document,
    revision: state.revision,
    savedRevision: state.savedRevision,
    dirty: state.dirty,
    lastValidDocument: state.lastValidDocument,
    lastError: null
  });
}

function expectCommandShape(command) {
  if (!isPlainObject(command) || typeof command.type !== "string") {
    throw new Error("command must be a plain object with a type");
  }
  const expectedKeys = COMMAND_KEYS[command.type];
  if (!expectedKeys) {
    throw new Error("unsupported command type");
  }
  const actualKeys = Reflect.ownKeys(command);
  if (
    actualKeys.some(
      (key) => typeof key !== "string" || !expectedKeys.includes(key)
    )
  ) {
    throw new Error("command has an unknown field");
  }
  if (
    expectedKeys.some(
      (key) => !Object.prototype.hasOwnProperty.call(command, key)
    )
  ) {
    throw new Error("command is missing a field");
  }
  if (
    !Number.isSafeInteger(command.expectedRevision) ||
    command.expectedRevision < 0
  ) {
    throw new Error("expectedRevision must be a non-negative safe integer");
  }
  return command;
}

function collectionFor(command) {
  if (typeof command.collection !== "string") {
    throw new Error("collection must be a string");
  }
  const collection = COLLECTIONS[command.collection];
  if (!collection) {
    throw new Error("unsupported collection");
  }
  return collection;
}

function recordsFor(document, collection) {
  return document[collection[0]][collection[1]];
}

function matchingIndexes(records, collection, key) {
  const token = collection[3](key);
  const indexes = [];
  for (let index = 0; index < records.length; index += 1) {
    if (collection[2](records[index]) === token) {
      indexes.push(index);
    }
  }
  return indexes;
}

function applyPatchDescriptors(target, patch) {
  for (const key of Reflect.ownKeys(patch)) {
    Object.defineProperty(
      target,
      key,
      Object.getOwnPropertyDescriptor(patch, key)
    );
  }
  return target;
}

function completeDocumentChange(state, normalized) {
  if (
    serializeSandboxAuthoringDocument(normalized) ===
    serializeSandboxAuthoringDocument(state.document)
  ) {
    return stateWithoutError(state);
  }
  if (state.revision === Number.MAX_SAFE_INTEGER) {
    throw new Error("revision cannot be incremented safely");
  }
  const revision = state.revision + 1;
  return freezeState({
    document: normalized,
    revision,
    savedRevision: state.savedRevision,
    dirty: revision !== state.savedRevision,
    lastValidDocument: normalized,
    lastError: null
  });
}

function applyEntityCommand(state, command) {
  const collection = collectionFor(command);
  const candidate = cloneValue(state.document);
  const records = recordsFor(candidate, collection);

  if (command.type === "entity.add") {
    records.push(cloneValue(command.value));
    return completeDocumentChange(
      state,
      normalizeSandboxAuthoringDocument(candidate)
    );
  }

  const indexes = matchingIndexes(records, collection, command.key);
  if (indexes.length === 0) {
    return stateWithError(state, "unknown_entity", "entity key was not found");
  }
  if (indexes.length > 1) {
    return stateWithError(
      state,
      "ambiguous_entity",
      "entity key matched multiple records"
    );
  }

  const index = indexes[0];
  if (command.type === "entity.delete") {
    records.splice(index, 1);
  } else {
    if (!isPlainObject(command.patch)) {
      return stateWithError(
        state,
        "invalid_command",
        "entity patch must be a plain object"
      );
    }
    records[index] = applyPatchDescriptors(
      records[index],
      cloneValue(command.patch)
    );
  }
  return completeDocumentChange(
    state,
    normalizeSandboxAuthoringDocument(candidate)
  );
}

function applyImport(state, source) {
  return completeDocumentChange(
    state,
    normalizeSandboxAuthoringDocument(source)
  );
}

export function createSandboxEditorState(source) {
  const document = normalizeSandboxAuthoringDocument(source);
  return freezeState({
    document,
    revision: 0,
    savedRevision: 0,
    dirty: false,
    lastValidDocument: document,
    lastError: null
  });
}

export function reduceSandboxEditorState(state, rawCommand) {
  let command;
  try {
    command = expectCommandShape(rawCommand);
  } catch (error) {
    return stateWithError(state, "invalid_command", error.message);
  }

  if (command.expectedRevision !== state.revision) {
    return stateWithError(
      state,
      "stale_revision",
      "expected revision does not match current revision"
    );
  }

  try {
    if (command.type.startsWith("entity.")) {
      return applyEntityCommand(state, command);
    }
    if (command.type === "document.import") {
      return applyImport(state, command.source);
    }
    return freezeState({
      document: state.document,
      revision: state.revision,
      savedRevision: state.revision,
      dirty: false,
      lastValidDocument: state.lastValidDocument,
      lastError: null
    });
  } catch (error) {
    return stateWithError(state, "invalid_document", error.message);
  }
}

export function serializeSandboxEditorState(state) {
  return serializeSandboxAuthoringDocument(state.document);
}
