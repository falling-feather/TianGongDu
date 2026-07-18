const ABI_VERSION_1_0 = 0x00010000;
const ABI_VERSION_1_1 = 0x00010001;
const MAX_CANONICAL_PACKAGE_BYTES = 4 * 1024 * 1024;
const HEADER_BYTES = 120;
const ARTIFACT_BYTES = 16;
const ABI_1_1_PREFIX_BYTES = HEADER_BYTES + ARTIFACT_BYTES;
const MAX_RESULT_BYTES = MAX_CANONICAL_PACKAGE_BYTES + ABI_1_1_PREFIX_BYTES;
const DIAGNOSTIC_BYTES = 48;
const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder("utf-8", { fatal: true });

export class SandboxPackageServiceTransportError extends Error {
  constructor(status, operation) {
    super(operation + " failed with Sandbox service transport status " + status);
    this.name = "SandboxPackageServiceTransportError";
    this.status = status;
    this.operation = operation;
  }
}

function call(module, name, ...args) {
  const fn = module["_" + name];
  if (typeof fn !== "function") {
    throw new SandboxPackageServiceTransportError(9, "missing export " + name);
  }
  return fn(...args);
}

function requireSuccess(status, operation) {
  if (status !== 1) throw new SandboxPackageServiceTransportError(status, operation);
}

function allocate(module, bytes) {
  const pointer = module._malloc(bytes);
  if (!pointer) throw new SandboxPackageServiceTransportError(7, "allocation");
  module.HEAPU8.fill(0, pointer, pointer + bytes);
  return pointer;
}

function view(module, pointer, bytes) {
  return new DataView(module.HEAPU8.buffer, pointer, bytes);
}

function keyAt(data, offset) {
  return Object.freeze({
    low: data.getUint32(offset, true),
    high: data.getUint32(offset + 4, true)
  });
}

function bytesAt(bytes, offset, length, total) {
  if (offset > total || length > total - offset) {
    throw new SandboxPackageServiceTransportError(9, "truncated result bytes");
  }
  const owned = new Uint8Array(length);
  owned.set(new Uint8Array(bytes.buffer, bytes.byteOffset + offset, length));
  return owned;
}

function idAt(bytes, offset, length, total) {
  return textDecoder.decode(bytesAt(bytes, offset, length, total));
}

export function decodeSandboxPackageServiceResult(source) {
  const bytes = source instanceof Uint8Array ? source : new Uint8Array(source);
  if (bytes.byteLength < HEADER_BYTES || bytes.byteLength > MAX_RESULT_BYTES) {
    throw new SandboxPackageServiceTransportError(9, "result size out of range");
  }
  const data = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const total = data.getUint32(76, true);
  const count = data.getUint32(8, true);
  const diagnosticsOffset = data.getUint32(68, true);
  const idBytesOffset = data.getUint32(72, true);
  const outcome = data.getUint8(1);
  const compileStatus = data.getUint8(2);
  const packageError = data.getUint8(3);
  const bindingCode = data.getUint8(44);
  const bindingDomain = data.getUint8(45);
  const bindingField = data.getUint16(46, true);
  const abiMajor = data.getUint16(80, true);
  const abiMinor = data.getUint16(82, true);
  const prefixBytes = abiMinor === 0 ? HEADER_BYTES : ABI_1_1_PREFIX_BYTES;
  if (
    data.getUint8(0) !== 1 ||
    abiMajor !== 1 ||
    (abiMinor !== 0 && abiMinor !== 1) ||
    bytes.byteLength < prefixBytes ||
    total !== bytes.byteLength ||
    diagnosticsOffset !== prefixBytes ||
    count > Math.floor((idBytesOffset - diagnosticsOffset) / DIAGNOSTIC_BYTES) ||
    diagnosticsOffset + count * DIAGNOSTIC_BYTES !== idBytesOffset
  ) {
    throw new SandboxPackageServiceTransportError(9, "incomplete result layout");
  }
  if (outcome < 1 || outcome > 7 || compileStatus < 1 || compileStatus > 4 ||
      packageError > 18 ||
      !((bindingCode >= 1 && bindingCode <= 21) || bindingCode === 255) ||
      !((bindingDomain >= 1 && bindingDomain <= 4) || bindingDomain === 255) ||
      !((bindingField >= 1 && bindingField <= 7) || bindingField === 65535)) {
    throw new SandboxPackageServiceTransportError(9, "unknown result enum");
  }
  if (data.getUint16(86, true) !== 0) {
    throw new SandboxPackageServiceTransportError(9, "non-zero binding reserved bytes");
  }
  for (let index = 104; index < 120; index += 1) {
    if (data.getUint8(index) !== 0) {
      throw new SandboxPackageServiceTransportError(9, "non-zero result reserved bytes");
    }
  }
  let packageOffset = 0;
  let packageLength = 0;
  if (abiMinor === 1) {
    packageOffset = data.getUint32(120, true);
    packageLength = data.getUint32(124, true);
    for (let index = 128; index < ABI_1_1_PREFIX_BYTES; index += 1) {
      if (data.getUint8(index) !== 0) {
        throw new SandboxPackageServiceTransportError(9, "non-zero artifact reserved bytes");
      }
    }
  }
  const diagnostics = [];
  const idRanges = [];
  for (let index = 0; index < count; index += 1) {
    const offset = diagnosticsOffset + index * DIAGNOSTIC_BYTES;
    const flags = data.getUint16(offset + 6, true);
    const code = data.getUint16(offset, true);
    const severity = data.getUint8(offset + 2);
    const section = data.getUint8(offset + 3);
    const field = data.getUint16(offset + 4, true);
    if ((flags & ~3) !== 0 || data.getUint32(offset + 44, true) !== 0 ||
        code < 1 || code > 33 || (severity !== 1 && severity !== 2) ||
        section > 13 || field > 38) {
      throw new SandboxPackageServiceTransportError(9, "invalid diagnostic flags");
    }
    const subjectOffset = data.getUint32(offset + 28, true);
    const subjectLength = data.getUint32(offset + 32, true);
    const relatedOffset = data.getUint32(offset + 36, true);
    const relatedLength = data.getUint32(offset + 40, true);
    const validateId = (present, idOffset, idLength) => {
      if (!present) {
        if (idOffset !== 0 || idLength !== 0) {
          throw new SandboxPackageServiceTransportError(9, "absent diagnostic ID has bytes");
        }
        return null;
      }
      if (idLength === 0 || idLength > 96 || idOffset < idBytesOffset ||
          idOffset > total || idLength > total - idOffset) {
        throw new SandboxPackageServiceTransportError(9, "invalid diagnostic ID range");
      }
      idRanges.push([idOffset, idOffset + idLength]);
      return idAt(bytes, idOffset, idLength, total);
    };
    const subjectId = (flags & 1) !== 0
      ? validateId(true, subjectOffset, subjectLength)
      : validateId(false, subjectOffset, subjectLength);
    const relatedId = (flags & 2) !== 0
      ? validateId(true, relatedOffset, relatedLength)
      : validateId(false, relatedOffset, relatedLength);
    diagnostics.push(Object.freeze({
      code,
      severity,
      section,
      field,
      recordIndex: data.getUint32(offset + 8, true),
      subjectKey: keyAt(data, offset + 12),
      relatedKey: keyAt(data, offset + 20),
      subjectId,
      relatedId
    }));
  }
  const bindingFlags = data.getUint16(84, true);
  if ((bindingFlags & ~3) !== 0 || data.getUint16(86, true) !== 0) {
    throw new SandboxPackageServiceTransportError(9, "invalid binding diagnostic flags");
  }
  const bindingSubjectLength = data.getUint32(92, true);
  const bindingRelatedLength = data.getUint32(100, true);
  const bindingSubjectOffset = data.getUint32(88, true);
  const bindingRelatedOffset = data.getUint32(96, true);
  const bindingId = (present, idOffset, idLength) => {
    if (!present) {
      if (idOffset !== 0 || idLength !== 0) {
        throw new SandboxPackageServiceTransportError(9, "absent binding ID has bytes");
      }
      return null;
    }
    if (idLength === 0 || idLength > 96 || idOffset < idBytesOffset ||
        idOffset > total || idLength > total - idOffset) {
      throw new SandboxPackageServiceTransportError(9, "invalid binding ID range");
    }
    idRanges.push([idOffset, idOffset + idLength]);
    return idAt(bytes, idOffset, idLength, total);
  };
  const bindingValidation = Object.freeze({
    code: bindingCode,
    domain: bindingDomain,
    field: bindingField,
    recordIndex: data.getUint32(48, true),
    subjectKey: keyAt(data, 52),
    relatedKey: keyAt(data, 60),
    subjectId: bindingId((bindingFlags & 1) !== 0,
      bindingSubjectOffset, bindingSubjectLength),
    relatedId: bindingId((bindingFlags & 2) !== 0,
      bindingRelatedOffset, bindingRelatedLength)
  });
  let nextIdOffset = idBytesOffset;
  for (const [start, end] of idRanges) {
    if (start !== nextIdOffset) {
      throw new SandboxPackageServiceTransportError(9, "non-canonical diagnostic ID coverage");
    }
    nextIdOffset = end;
  }
  let packageBytes = null;
  if (abiMinor === 1 && outcome === 1) {
    if (packageLength === 0 || packageOffset !== nextIdOffset ||
        packageOffset > total || packageLength > total - packageOffset ||
        packageOffset + packageLength !== total) {
      throw new SandboxPackageServiceTransportError(9, "invalid canonical package coverage");
    }
    packageBytes = bytesAt(bytes, packageOffset, packageLength, total);
  } else {
    if ((abiMinor === 1 && (packageOffset !== 0 || packageLength !== 0)) ||
        nextIdOffset !== total) {
      throw new SandboxPackageServiceTransportError(9, "unexpected package or trailing bytes");
    }
  }
  return Object.freeze({
    complete: true,
    outcome,
    compileStatus,
    packageError,
    identity: Object.freeze({
      generation: data.getUint32(4, true),
      checksum: Object.freeze(Array.from(bytes.slice(12, 44)))
    }),
    diagnostics: Object.freeze(diagnostics),
    bindingValidation,
    packageBytes
  });
}

const assetKinds = Object.freeze({
  player: 1, actor: 2, obstacle: 3, interaction: 4,
  mechanism: 5, safe_point: 6, effect: 7
});
const triggerKinds = Object.freeze({
  session_started: 1, interaction_completed: 2, mechanism_activated: 3,
  objective_completed: 4, wave_completed: 5
});
const completionKinds = Object.freeze({
  interaction_completed: 1, mechanism_activated: 2, wave_completed: 3
});

function structuralError(path, expected) {
  throw new TypeError(path + " must be " + expected);
}

function expectObject(value, path, expectedKeys) {
  if (value === null || typeof value !== "object" || Array.isArray(value) ||
      (Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)) {
    structuralError(path, "an object");
  }
  const actualKeys = Reflect.ownKeys(value);
  if (actualKeys.length !== expectedKeys.length ||
      actualKeys.some((key) => typeof key !== "string" || !expectedKeys.includes(key))) {
    structuralError(path, "a closed object with fields " + expectedKeys.join(", "));
  }
  for (const key of expectedKeys) {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor || !("value" in descriptor) || descriptor.enumerable !== true) {
      structuralError(path + "." + key, "an enumerable data property");
    }
  }
  return value;
}

function expectArray(value, path) {
  if (!Array.isArray(value)) structuralError(path, "an array");
  const keys = Reflect.ownKeys(value);
  if (keys.some((key) => key !== "length" &&
      (typeof key !== "string" || !/^(0|[1-9][0-9]*)$/.test(key)))) {
    structuralError(path, "a closed dense array");
  }
  for (let index = 0; index < value.length; index += 1) {
    const descriptor = Object.getOwnPropertyDescriptor(value, String(index));
    if (!descriptor || !("value" in descriptor) || descriptor.enumerable !== true) {
      structuralError(path + "[" + index + "]", "an enumerable data element");
    }
  }
  return value;
}

function expectString(value, path, strings) {
  if (typeof value !== "string") structuralError(path, "a string");
  for (let index = 0; index < value.length; index += 1) {
    const unit = value.charCodeAt(index);
    if (unit >= 0xd800 && unit <= 0xdbff) {
      const next = index + 1 < value.length ? value.charCodeAt(index + 1) : 0;
      if (next < 0xdc00 || next > 0xdfff) {
        structuralError(path, "a Unicode string without unpaired surrogates");
      }
      index += 1;
    } else if (unit >= 0xdc00 && unit <= 0xdfff) {
      structuralError(path, "a Unicode string without unpaired surrogates");
    }
  }
  strings.add(value);
}

function expectInteger(value, minimum, maximum, path) {
  if (!Number.isInteger(value) || value < minimum || value > maximum) {
    structuralError(path, "an integer in [" + minimum + ", " + maximum + "]");
  }
}

function expectChecksum(value, path) {
  if (Array.isArray(value)) {
    expectArray(value, path);
  } else if (value instanceof Uint8Array) {
    const keys = Reflect.ownKeys(value);
    if (keys.length !== value.length ||
        keys.some((key, index) => key !== String(index))) {
      structuralError(path, "a dense byte array with no extra own keys");
    }
    for (let index = 0; index < value.length; index += 1) {
      const descriptor = Object.getOwnPropertyDescriptor(value, String(index));
      if (!descriptor || !("value" in descriptor) || descriptor.enumerable !== true) {
        structuralError(path + "[" + index + "]", "an enumerable data byte");
      }
    }
  } else {
    structuralError(path, "exactly 32 bytes");
  }
  if (value.length !== 32) structuralError(path, "exactly 32 bytes");
  for (let index = 0; index < value.length; index += 1)
    expectInteger(value[index], 0, 255, path + "[" + index + "]");
}

function checkBounds(value, path) {
  const bounds = expectObject(value, path, [
    "minX", "maxX", "minY", "maxY", "minHeight", "maxHeight",
    "minFloorLayer", "maxFloorLayer"
  ]);
  for (const name of ["minX", "maxX", "minY", "maxY", "minHeight", "maxHeight"])
    expectInteger(bounds[name], -2147483648, 2147483647, path + "." + name);
  for (const name of ["minFloorLayer", "maxFloorLayer"])
    expectInteger(bounds[name], -32768, 32767, path + "." + name);
}

function checkPose(value, path) {
  const pose = expectObject(value, path, ["x", "y", "height", "floorLayer"]);
  for (const name of ["x", "y", "height"])
    expectInteger(pose[name], -2147483648, 2147483647, path + "." + name);
  expectInteger(pose.floorLayer, -32768, 32767, path + ".floorLayer");
}

function checkPlacement(value, path, strings) {
  const record = expectObject(value, path,
    ["id", "regionId", "assetId", "pose", "facingMillidegrees"]);
  expectString(record.id, path + ".id", strings);
  expectString(record.regionId, path + ".regionId", strings);
  expectString(record.assetId, path + ".assetId", strings);
  checkPose(record.pose, path + ".pose");
  expectInteger(record.facingMillidegrees, 0, 4294967295,
    path + ".facingMillidegrees");
}

function validateStructuralInput(runtime, expectedIdentity) {
  const strings = new Set();
  const expected = expectObject(expectedIdentity, "expectedIdentity", ["generation", "checksum"]);
  expectInteger(expected.generation, 0, 4294967295, "expectedIdentity.generation");
  expectChecksum(expected.checksum, "expectedIdentity.checksum");

  const root = expectObject(runtime, "runtime", [
    "packageId", "sandboxId", "bounds", "completionObjectiveId", "player",
    "regions", "assets", "actors", "groundBlockers", "safePoints",
    "interactions", "mechanisms", "waves", "waveSpawns", "objectives",
    "interactionBindings", "mechanismBindings"
  ]);
  for (const name of ["packageId", "sandboxId", "completionObjectiveId"])
    expectString(root[name], "runtime." + name, strings);
  checkBounds(root.bounds, "runtime.bounds");
  const player = expectObject(root.player, "runtime.player", [
    "id", "regionId", "assetId", "initialSafePointId", "pose", "facingMillidegrees"
  ]);
  expectString(player.id, "runtime.player.id", strings);
  expectString(player.regionId, "runtime.player.regionId", strings);
  expectString(player.assetId, "runtime.player.assetId", strings);
  expectString(player.initialSafePointId, "runtime.player.initialSafePointId", strings);
  checkPose(player.pose, "runtime.player.pose");
  expectInteger(player.facingMillidegrees, 0, 4294967295,
    "runtime.player.facingMillidegrees");
  for (const [name, values] of [["regions", root.regions], ["assets", root.assets]]) {
    expectArray(values, "runtime." + name).forEach((value, index) => {
      const record = expectObject(value, "runtime." + name + "[" + index + "]",
        name === "regions" ? ["id", "bounds"] : ["id", "kind"]);
      expectString(record.id, "runtime." + name + "[" + index + "].id", strings);
      if (name === "regions") checkBounds(record.bounds, "runtime.regions[" + index + "].bounds");
      else expectString(record.kind, "runtime.assets[" + index + "].kind", strings);
    });
  }
  for (const name of ["actors", "safePoints", "interactions", "mechanisms"])
    expectArray(root[name], "runtime." + name).forEach((value, index) =>
      checkPlacement(value, "runtime." + name + "[" + index + "]", strings));
  expectArray(root.groundBlockers, "runtime.groundBlockers").forEach((value, index) => {
    const path = "runtime.groundBlockers[" + index + "]";
    const record = expectObject(value, path, [
      "id", "regionId", "assetId", "minX", "maxX", "minY", "maxY",
      "minHeight", "maxHeight", "floorLayer"
    ]);
    for (const name of ["id", "regionId", "assetId"])
      expectString(record[name], path + "." + name, strings);
    for (const name of ["minX", "maxX", "minY", "maxY", "minHeight", "maxHeight"])
      expectInteger(record[name], -2147483648, 2147483647, path + "." + name);
    expectInteger(record.floorLayer, -32768, 32767, path + ".floorLayer");
  });
  expectArray(root.waves, "runtime.waves").forEach((value, index) => {
    const path = "runtime.waves[" + index + "]";
    const record = expectObject(value, path,
      ["id", "regionId", "predecessorWaveId", "trigger"]);
    for (const name of ["id", "regionId", "predecessorWaveId"])
      expectString(record[name], path + "." + name, strings);
    const trigger = expectObject(record.trigger, path + ".trigger", ["kind", "targetId"]);
    expectString(trigger.kind, path + ".trigger.kind", strings);
    expectString(trigger.targetId, path + ".trigger.targetId", strings);
  });
  expectArray(root.waveSpawns, "runtime.waveSpawns").forEach((value, index) => {
    const path = "runtime.waveSpawns[" + index + "]";
    const record = expectObject(value, path, ["waveId", "actorId", "delayTicks", "spawnOrder"]);
    expectString(record.waveId, path + ".waveId", strings);
    expectString(record.actorId, path + ".actorId", strings);
    expectInteger(record.delayTicks, 0, 4294967295, path + ".delayTicks");
    expectInteger(record.spawnOrder, 0, 65535, path + ".spawnOrder");
  });
  expectArray(root.objectives, "runtime.objectives").forEach((value, index) => {
    const path = "runtime.objectives[" + index + "]";
    const record = expectObject(value, path,
      ["id", "regionId", "predecessorObjectiveId", "completion"]);
    for (const name of ["id", "regionId", "predecessorObjectiveId"])
      expectString(record[name], path + "." + name, strings);
    const completion = expectObject(record.completion, path + ".completion", ["kind", "targetId"]);
    expectString(completion.kind, path + ".completion.kind", strings);
    expectString(completion.targetId, path + ".completion.targetId", strings);
  });
  expectArray(root.interactionBindings, "runtime.interactionBindings").forEach((value, index) => {
    const path = "runtime.interactionBindings[" + index + "]";
    const record = expectObject(value, path,
      ["interactionId", "operation", "rangeMm", "targetMechanismId"]);
    expectString(record.interactionId, path + ".interactionId", strings);
    expectString(record.targetMechanismId, path + ".targetMechanismId", strings);
    expectString(record.operation, path + ".operation", strings);
    expectInteger(record.rangeMm, -2147483648, 2147483647, path + ".rangeMm");
  });
  expectArray(root.mechanismBindings, "runtime.mechanismBindings").forEach((value, index) => {
    const path = "runtime.mechanismBindings[" + index + "]";
    const record = expectObject(value, path,
      ["mechanismId", "activation", "targetGroundBlockerId"]);
    expectString(record.mechanismId, path + ".mechanismId", strings);
    expectString(record.targetGroundBlockerId, path + ".targetGroundBlockerId", strings);
    expectString(record.activation, path + ".activation", strings);
  });
  return strings;
}

function enumValue(table, value) {
  return Object.prototype.hasOwnProperty.call(table, value) ? table[value] : 255;
}

function writeBounds(data, offset, bounds) {
  data.setInt32(offset, bounds.minX, true);
  data.setInt32(offset + 4, bounds.maxX, true);
  data.setInt32(offset + 8, bounds.minY, true);
  data.setInt32(offset + 12, bounds.maxY, true);
  data.setInt32(offset + 16, bounds.minHeight, true);
  data.setInt32(offset + 20, bounds.maxHeight, true);
  data.setInt16(offset + 24, bounds.minFloorLayer, true);
  data.setInt16(offset + 26, bounds.maxFloorLayer, true);
}

function writePose(data, offset, pose) {
  data.setInt32(offset, pose.x, true);
  data.setInt32(offset + 4, pose.y, true);
  data.setInt32(offset + 8, pose.height, true);
  data.setInt16(offset + 12, pose.floorLayer, true);
}

export class SandboxPackageServiceClient {
  constructor(module, serviceHandle) {
    this.module = module;
    this.serviceHandle = serviceHandle;
    this.destroyed = false;
  }

  static create(module) {
    const abiVersion = call(module, "tgd_sandbox_compiler_service_abi_version");
    if (abiVersion !== ABI_VERSION_1_0 && abiVersion !== ABI_VERSION_1_1) {
      throw new SandboxPackageServiceTransportError(9, "incompatible ABI");
    }
    const pointer = allocate(module, 8);
    try {
      requireSuccess(call(module, "tgd_sandbox_compiler_service_create", pointer), "service create");
      const handle = view(module, pointer, 8).getBigUint64(0, true);
      if (typeof handle !== "bigint" || handle === 0n) {
        throw new SandboxPackageServiceTransportError(9, "service handle is not BigInt");
      }
      return new SandboxPackageServiceClient(module, handle);
    } finally {
      module._free(pointer);
    }
  }

  identity() {
    const pointer = allocate(this.module, 36);
    try {
      requireSuccess(call(this.module, "tgd_sandbox_compiler_service_read_identity",
        this.serviceHandle, pointer), "identity");
      const data = view(this.module, pointer, 36);
      return Object.freeze({
        generation: data.getUint32(0, true),
        checksum: Object.freeze(Array.from(this.module.HEAPU8.slice(pointer + 4, pointer + 36)))
      });
    } finally {
      this.module._free(pointer);
    }
  }

  publish(runtime, expectedIdentity = this.identity()) {
    if (this.destroyed) throw new SandboxPackageServiceTransportError(2, "destroyed service");
    const authoredStrings = validateStructuralInput(runtime, expectedIdentity);
    const module = this.module;
    const scratch = allocate(module, 128);
    let output = 0;
    let request = 0n;
    let submitted = false;
    try {
      output = allocate(module, MAX_RESULT_BYTES);
      let current = view(module, scratch, 128);
      current.setUint32(0, expectedIdentity.generation, true);
      module.HEAPU8.set(expectedIdentity.checksum, scratch + 4);
      requireSuccess(call(module, "tgd_sandbox_compile_request_create",
        this.serviceHandle, scratch, scratch + 40), "request create");
      request = view(module, scratch, 128).getBigUint64(40, true);
      if (typeof request !== "bigint" || request === 0n) {
        throw new SandboxPackageServiceTransportError(9, "request handle is not BigInt");
      }
      const refs = new Map();
      for (const value of authoredStrings) {
        const bytes = textEncoder.encode(value);
        const input = bytes.length === 0 ? 0 : allocate(module, bytes.length);
        try {
          if (bytes.length !== 0) module.HEAPU8.set(bytes, input);
          requireSuccess(call(module, "tgd_sandbox_compile_request_copy_utf8",
            this.serviceHandle, request, input, bytes.length, scratch + 48), "copy UTF-8");
          refs.set(value, view(module, scratch, 128).getUint32(48, true));
        } finally {
          if (input) module._free(input);
        }
      }
      const text = (value) => refs.get(value);
      const invoke = (name, size, fill) => {
        module.HEAPU8.fill(0, scratch, scratch + size);
        fill(view(module, scratch, size));
        requireSuccess(call(module, name, this.serviceHandle, request, scratch), name);
      };
      invoke("tgd_sandbox_compile_request_set_metadata", 40, (record) => {
        record.setUint32(0, text(runtime.packageId), true);
        record.setUint32(4, text(runtime.sandboxId), true);
        record.setUint32(8, text(runtime.completionObjectiveId), true);
        writeBounds(record, 12, runtime.bounds);
      });
      invoke("tgd_sandbox_compile_request_set_player", 36, (record) => {
        record.setUint32(0, text(runtime.player.id), true);
        record.setUint32(4, text(runtime.player.regionId), true);
        record.setUint32(8, text(runtime.player.assetId), true);
        record.setUint32(12, text(runtime.player.initialSafePointId), true);
        writePose(record, 16, runtime.player.pose);
        record.setUint32(32, runtime.player.facingMillidegrees, true);
      });
      for (const value of runtime.regions) invoke("tgd_sandbox_compile_request_append_region", 32, (record) => {
        record.setUint32(0, text(value.id), true); writeBounds(record, 4, value.bounds);
      });
      for (const value of runtime.assets) invoke("tgd_sandbox_compile_request_append_asset", 8, (record) => {
        record.setUint32(0, text(value.id), true); record.setUint8(4, enumValue(assetKinds, value.kind));
      });
      const placements = [
        ["actor", runtime.actors], ["safe_point", runtime.safePoints],
        ["interaction", runtime.interactions], ["mechanism", runtime.mechanisms]
      ];
      for (const [name, values] of placements) for (const value of values)
        invoke("tgd_sandbox_compile_request_append_" + name, 32, (record) => {
          record.setUint32(0, text(value.id), true);
          record.setUint32(4, text(value.regionId), true);
          record.setUint32(8, text(value.assetId), true);
          writePose(record, 12, value.pose);
          record.setUint32(28, value.facingMillidegrees, true);
        });
      for (const value of runtime.groundBlockers) invoke("tgd_sandbox_compile_request_append_ground_blocker", 40, (record) => {
        record.setUint32(0, text(value.id), true); record.setUint32(4, text(value.regionId), true);
        record.setUint32(8, text(value.assetId), true); record.setInt32(12, value.minX, true);
        record.setInt32(16, value.maxX, true); record.setInt32(20, value.minY, true);
        record.setInt32(24, value.maxY, true); record.setInt32(28, value.minHeight, true);
        record.setInt32(32, value.maxHeight, true); record.setInt16(36, value.floorLayer, true);
      });
      for (const value of runtime.waves) invoke("tgd_sandbox_compile_request_append_wave", 20, (record) => {
        record.setUint32(0, text(value.id), true); record.setUint32(4, text(value.regionId), true);
        record.setUint32(8, text(value.predecessorWaveId), true);
        record.setUint32(12, text(value.trigger.targetId), true);
        record.setUint8(16, enumValue(triggerKinds, value.trigger.kind));
      });
      for (const value of runtime.waveSpawns) invoke("tgd_sandbox_compile_request_append_wave_spawn", 16, (record) => {
        record.setUint32(0, text(value.waveId), true); record.setUint32(4, text(value.actorId), true);
        record.setUint32(8, value.delayTicks, true); record.setUint16(12, value.spawnOrder, true);
      });
      for (const value of runtime.objectives) invoke("tgd_sandbox_compile_request_append_objective", 20, (record) => {
        record.setUint32(0, text(value.id), true); record.setUint32(4, text(value.regionId), true);
        record.setUint32(8, text(value.predecessorObjectiveId), true);
        record.setUint32(12, text(value.completion.targetId), true);
        record.setUint8(16, enumValue(completionKinds, value.completion.kind));
      });
      for (const value of runtime.interactionBindings) invoke("tgd_sandbox_compile_request_append_interaction_binding", 16, (record) => {
        record.setUint32(0, text(value.interactionId), true);
        record.setUint32(4, text(value.targetMechanismId), true);
        record.setInt32(8, value.rangeMm, true);
        record.setUint8(12, value.operation === "operate" ? 1 : 255);
      });
      for (const value of runtime.mechanismBindings) invoke("tgd_sandbox_compile_request_append_mechanism_binding", 12, (record) => {
        record.setUint32(0, text(value.mechanismId), true);
        record.setUint32(4, text(value.targetGroundBlockerId), true);
        record.setUint8(8, value.activation === "one_shot_activate" ? 1 : 255);
      });
      module.HEAPU8.fill(0, scratch, scratch + 4);
      const status = call(module, "tgd_sandbox_compile_request_submit",
        this.serviceHandle, request, output, MAX_RESULT_BYTES, scratch);
      submitted = true;
      requireSuccess(status, "submit");
      const written = view(module, scratch, 4).getUint32(0, true);
      if (written > MAX_RESULT_BYTES) throw new SandboxPackageServiceTransportError(9, "result overflow");
      return decodeSandboxPackageServiceResult(module.HEAPU8.slice(output, output + written));
    } finally {
      if (request !== 0n && !submitted) {
        call(module, "tgd_sandbox_compile_request_cancel", this.serviceHandle, request);
      }
      if (output) module._free(output);
      module._free(scratch);
    }
  }

  destroy() {
    if (this.destroyed) throw new SandboxPackageServiceTransportError(2, "service destroy");
    requireSuccess(call(this.module, "tgd_sandbox_compiler_service_destroy", this.serviceHandle),
      "service destroy");
    this.destroyed = true;
  }
}
