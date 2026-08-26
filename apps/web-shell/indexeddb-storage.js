(() => {
  "use strict";

  const abi = globalThis.TgdWebAbi;
  const schema = globalThis.TgdIndexedDbContract;
  if (!abi || !schema) {
    throw new Error("TgdWebAbi and TgdIndexedDbContract must load before indexeddb-storage.js");
  }

  const CHANNEL = schema.database.channel;
  const MAX_U64 = (1n << 64n) - 1n;
  const ZERO_ID = Object.freeze({ high: 0n, low: 0n });
  const ZERO_DIGEST = "0".repeat(64);
  const PACKAGE_SET_ID = "82cba9bf9f2251f8b74575fa8bda891d";
  const GUEST_PROFILE_KEY = "tgd.prototype_f1.guest-profile-id.v1";
  const GUEST_DEVICE_KEY = "tgd.prototype_f1.guest-device-id.v1";
  const FNV_OFFSET = 14_695_981_039_346_656_037n;
  const FNV_PRIME = 1_099_511_628_211n;
  const COMPLETION_HEADER_BYTES = abi.payload.storageCompletionV1HeaderBytes;
  const REQUEST_HEADER_BYTES = abi.payload.storageRequestV1HeaderBytes;
  const MAX_COMPLETION_CHUNK_BYTES =
    abi.maxMessageBytes - abi.headerBytes - COMPLETION_HEADER_BYTES;
  const MAX_REQUEST_CHUNK_BYTES =
    abi.maxMessageBytes - abi.headerBytes - REQUEST_HEADER_BYTES;

  const storageError = Object.freeze({
    none: 0,
    notFound: 1,
    conflict: 2,
    quota: 3,
    corrupt: 4,
    unavailable: 5,
    cancelled: 6,
    timeout: 7,
    invalidRequest: 8,
    internal: 9
  });

  const profileStateNames = Object.freeze([
    "uninitialized",
    "configured",
    "loading_head",
    "loading_snapshot",
    "ready",
    "saving",
    "restore_failed",
    "save_failed",
    "conflict_read_only",
    "recovery_required",
    "storage_unavailable"
  ]);

  const profileErrorNames = Object.freeze([
    "none",
    "invalid_config",
    "invalid_state",
    "invalid_snapshot",
    "allocation_failed",
    "backpressure",
    "storage_unavailable",
    "storage_conflict",
    "storage_quota",
    "storage_corrupt",
    "cancelled",
    "timeout",
    "protocol_violation",
    "internal"
  ]);

  const questUiSourceNames = Object.freeze([
    "invalid",
    "choice_available",
    "interaction_feedback",
    "objective_state",
    "combat_feedback",
    "recovery_offer",
    "recovery_resume"
  ]);
  const questUiSurfaceNames = Object.freeze(["gameplay", "choice", "failure"]);
  const questUiPolarityNames = Object.freeze(["positive", "negative", "recovery"]);
  const questUiObjectiveStateNames = Object.freeze(["locked", "active", "completed"]);
  const questUiResultStatusNames = Object.freeze([
    "not_applicable",
    "accepted",
    "rejected",
    "ignored_repeat",
    "pending"
  ]);
  const questUiRejectionReasonNames = Object.freeze([
    "none",
    "prerequisite_incomplete",
    "selection_already_committed",
    "wrong_target"
  ]);
  const questUiAttemptTimeClassificationNames = Object.freeze([
    "unspecified",
    "qualifying_first_visit",
    "repeat_no_progress",
    "qualifying_craft_decision",
    "qualifying_error_feedback",
    "qualifying_wrong_order_feedback",
    "qualifying_craft_confirmation",
    "qualifying_dialogue_decision",
    "qualifying_training_risk",
    "qualifying_combat_proof",
    "qualifying_combat_feedback",
    "failure_retry_excluded",
    "resume_no_duplicate_progress"
  ]);

  class WireError extends Error {}

  function bytes(value) {
    if (value instanceof Uint8Array) return value;
    if (value instanceof ArrayBuffer) return new Uint8Array(value);
    if (ArrayBuffer.isView(value)) {
      return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
    }
    throw new TypeError("Expected an ArrayBuffer or typed array");
  }

  function bytesToHex(value) {
    return Array.from(bytes(value), (byte) => byte.toString(16).padStart(2, "0")).join("");
  }

  function hexToBytes(value, expectedBytes) {
    if (typeof value !== "string" || !new RegExp(`^[0-9a-f]{${expectedBytes * 2}}$`).test(value)) {
      throw new WireError(`Expected ${expectedBytes}-byte lowercase hexadecimal value`);
    }
    const output = new Uint8Array(expectedBytes);
    for (let index = 0; index < output.length; index += 1) {
      output[index] = Number.parseInt(value.slice(index * 2, index * 2 + 2), 16);
    }
    return output;
  }

  function idToHex(id) {
    return `${id.high.toString(16).padStart(16, "0")}${id.low
      .toString(16)
      .padStart(16, "0")}`;
  }

  function idFromHex(value) {
    if (typeof value !== "string" || !/^[0-9a-f]{32}$/.test(value)) {
      throw new WireError("StableId128 must be 32 lowercase hexadecimal characters");
    }
    return {
      high: BigInt(`0x${value.slice(0, 16)}`),
      low: BigInt(`0x${value.slice(16)}`)
    };
  }

  function idEmpty(id) {
    return id.high === 0n && id.low === 0n;
  }

  function stableKeyToHex(value) {
    if (typeof value !== "bigint" || value < 0n || value > MAX_U64) {
      throw new WireError("StableContentKey must be an unsigned 64-bit bigint");
    }
    return value.toString(16).padStart(16, "0");
  }

  function stableKeyFromHex(value) {
    if (typeof value !== "string" || !/^[0-9a-f]{16}$/.test(value)) {
      throw new WireError("StableContentKey must be 16 lowercase hexadecimal characters");
    }
    return BigInt(`0x${value}`);
  }

  function sequenceFromValue(value) {
    const parsed = typeof value === "bigint"
      ? value
      : typeof value === "string" && /^[0-9]+$/.test(value)
        ? BigInt(value)
        : -1n;
    if (parsed <= 0n || parsed > MAX_U64) {
      throw new WireError("Quest UI sequence must be a nonzero unsigned 64-bit value");
    }
    return parsed;
  }

  function stableKeyFromValue(value) {
    const parsed = typeof value === "bigint" ? value : stableKeyFromHex(value);
    if (parsed <= 0n || parsed > MAX_U64) {
      throw new WireError("Quest UI identity must be a nonzero unsigned 64-bit value");
    }
    return parsed;
  }

  function idEqual(left, right) {
    return left.high === right.high && left.low === right.low;
  }

  function readId(view, offset) {
    return {
      high: view.getBigUint64(offset, true),
      low: view.getBigUint64(offset + 8, true)
    };
  }

  function writeId(view, offset, id) {
    view.setBigUint64(offset, id.high, true);
    view.setBigUint64(offset + 8, id.low, true);
  }

  function readDigest(view, offset) {
    return bytesToHex(new Uint8Array(view.buffer, view.byteOffset + offset, 32));
  }

  function writeDigest(view, offset, digest) {
    new Uint8Array(view.buffer, view.byteOffset + offset, 32).set(hexToBytes(digest, 32));
  }

  function emptyHead(profileId = ZERO_ID) {
    return {
      profileId,
      snapshotId: ZERO_ID,
      logicalSequence: 0n,
      envelopeHash: ZERO_DIGEST
    };
  }

  function readHead(view, offset) {
    return {
      profileId: readId(view, offset),
      snapshotId: readId(view, offset + 16),
      logicalSequence: view.getBigUint64(offset + 32, true),
      envelopeHash: readDigest(view, offset + 40)
    };
  }

  function writeHead(view, offset, head) {
    writeId(view, offset, head.profileId);
    writeId(view, offset + 16, head.snapshotId);
    view.setBigUint64(offset + 32, head.logicalSequence, true);
    writeDigest(view, offset + 40, head.envelopeHash);
  }

  function decodeHeader(value, expectedType) {
    const input = bytes(value);
    if (input.byteLength < abi.headerBytes || input.byteLength > abi.maxMessageBytes) {
      throw new WireError("Message size is outside the Web ABI limit");
    }
    const view = new DataView(input.buffer, input.byteOffset, input.byteLength);
    const header = {
      abiMajor: view.getUint16(0, true),
      abiMinor: view.getUint16(2, true),
      messageType: view.getUint16(4, true),
      payloadVersion: view.getUint16(6, true),
      payloadLength: view.getUint32(8, true),
      sessionGeneration: view.getUint32(12, true),
      sequence: view.getBigUint64(16, true),
      requestId: readId(view, 24)
    };
    if (header.abiMajor !== abi.major || header.abiMinor > abi.minor) {
      throw new WireError("Incompatible Web ABI version");
    }
    if (
      header.messageType !== expectedType ||
      header.payloadVersion !== 1 ||
      header.payloadLength !== input.byteLength - abi.headerBytes ||
      header.sessionGeneration === 0 ||
      header.sequence === 0n
    ) {
      throw new WireError("Invalid Web ABI message header");
    }
    return { input, view, header };
  }

  function decodeStorageRequest(value) {
    const decoded = decodeHeader(value, abi.messageType.storage_request);
    const { input, view, header } = decoded;
    if (header.payloadLength < REQUEST_HEADER_BYTES) {
      throw new WireError("Storage request payload is truncated");
    }
    const offset = abi.headerBytes;
    const operation = view.getUint16(offset, true);
    const recordKind = view.getUint16(offset + 2, true);
    const durability = view.getUint16(offset + 4, true);
    const operationCount = view.getUint16(offset + 6, true);
    const chunkIndex = view.getUint32(offset + 8, true);
    const chunkCount = view.getUint32(offset + 12, true);
    const totalBytes = view.getUint32(offset + 16, true);
    const chunkOffset = view.getUint32(offset + 20, true);
    const chunkLength = view.getUint32(offset + 24, true);
    let snapshotByteLength = view.getUint32(offset + 28, true);
    const key = {
      recordKind,
      profileId: readId(view, offset + 32),
      recordId: readId(view, offset + 48)
    };
    const expectedHead = readHead(view, offset + 64);
    const nextHead = readHead(view, offset + 136);
    const chunkStart = abi.headerBytes + REQUEST_HEADER_BYTES;
    if (
      operation < abi.storageOperation.read ||
      operation > abi.storageOperation.request_persistence ||
      idEmpty(header.requestId) ||
      recordKind < 1 ||
      recordKind > schema.stores.length ||
      durability > 1 ||
      chunkCount === 0 ||
      chunkIndex >= chunkCount ||
      totalBytes > abi.payload.maxStorageTransferBytes ||
      chunkOffset > totalBytes ||
      chunkLength > totalBytes - chunkOffset ||
      chunkLength !== input.byteLength - chunkStart
    ) {
      throw new WireError("Invalid storage request metadata");
    }
    if (operation === abi.storageOperation.write_atomic) {
      if (header.abiMinor === 0) {
        if (operationCount !== 0 || snapshotByteLength !== 0) {
          throw new WireError("Web ABI 1.0 atomic metadata must stay reserved");
        }
        snapshotByteLength = totalBytes;
      }
      const expectedChunkCount = Math.ceil(totalBytes / MAX_REQUEST_CHUNK_BYTES);
      const expectedChunkOffset = chunkIndex * MAX_REQUEST_CHUNK_BYTES;
      const expectedChunkLength = Math.min(
        MAX_REQUEST_CHUNK_BYTES,
        totalBytes - expectedChunkOffset
      );
      if (
        recordKind !== 2 ||
        totalBytes === 0 ||
        snapshotByteLength === 0 ||
        operationCount > abi.payload.maxAtomicOperationsPerWrite ||
        snapshotByteLength +
            operationCount * abi.payload.persistentOperationV1Bytes !== totalBytes ||
        idEmpty(key.profileId) ||
        idEmpty(key.recordId) ||
        chunkCount !== expectedChunkCount ||
        chunkOffset !== expectedChunkOffset ||
        chunkLength !== expectedChunkLength
      ) {
        throw new WireError("Atomic writes require a non-empty snapshot key and payload");
      }
    } else if (
      operationCount !== 0 ||
      snapshotByteLength !== 0 ||
      totalBytes !== 0 ||
      chunkLength !== 0 ||
      chunkIndex !== 0 ||
      chunkCount !== 1 ||
      chunkOffset !== 0
    ) {
      throw new WireError("Only atomic writes may carry request bytes");
    }
    return {
      header,
      operation,
      recordKind,
      durability,
      operationCount,
      snapshotByteLength,
      chunkIndex,
      chunkCount,
      totalBytes,
      chunkOffset,
      chunkLength,
      key,
      expectedHead,
      nextHead,
      chunk: input.slice(chunkStart)
    };
  }

  function encodeHeader(view, messageType, payloadLength, sessionGeneration, sequence, requestId) {
    view.setUint16(0, abi.major, true);
    view.setUint16(2, abi.minor, true);
    view.setUint16(4, messageType, true);
    view.setUint16(6, 1, true);
    view.setUint32(8, payloadLength, true);
    view.setUint32(12, sessionGeneration, true);
    view.setBigUint64(16, sequence, true);
    writeId(view, 24, requestId);
  }

  function encodeBootMessage({ profileId, packageSetId, requestSeed, sessionGeneration, sequence }) {
    const output = new Uint8Array(abi.headerBytes + abi.payload.bootConfigV1Bytes);
    const view = new DataView(output.buffer);
    encodeHeader(
      view,
      abi.messageType.boot_config,
      abi.payload.bootConfigV1Bytes,
      sessionGeneration,
      sequence,
      requestSeed
    );
    let offset = abi.headerBytes;
    writeId(view, offset, profileId);
    writeId(view, offset + 16, packageSetId);
    writeId(view, offset + 32, requestSeed);
    view.setUint32(offset + 48, sessionGeneration, true);
    return output;
  }

  function encodeUiCommand({ command, commandId, sessionGeneration, sequence, checkpointKind = 2 }) {
    const output = new Uint8Array(abi.headerBytes + abi.payload.uiCommandV1Bytes);
    const view = new DataView(output.buffer);
    encodeHeader(
      view,
      abi.messageType.ui_command,
      abi.payload.uiCommandV1Bytes,
      sessionGeneration,
      sequence,
      commandId
    );
    const offset = abi.headerBytes;
    view.setUint16(offset, command, true);
    view.setUint16(offset + 2, checkpointKind, true);
    writeId(view, offset + 4, commandId);
    return output;
  }

  function encodeSaveCommand({ snapshotId, sessionGeneration, sequence, checkpointKind = 2 }) {
    return encodeUiCommand({
      command: abi.uiCommand.save_guest_checkpoint,
      commandId: snapshotId,
      sessionGeneration,
      sequence,
      checkpointKind
    });
  }

  function encodeRetryCommand({ commandId, sessionGeneration, sequence }) {
    return encodeUiCommand({
      command: abi.uiCommand.retry_pending_save,
      commandId,
      sessionGeneration,
      sequence
    });
  }

  function decodeProfileUiEvent(value) {
    const decoded = decodeHeader(value, abi.messageType.ui_event);
    const { view, header } = decoded;
    if (header.payloadLength !== abi.payload.uiEventV1Bytes) {
      throw new WireError("Profile UI event has the wrong v1 size");
    }
    const offset = abi.headerBytes;
    const state = view.getUint16(offset, true);
    const error = view.getUint16(offset + 2, true);
    const flags = view.getUint32(offset + 4, true);
    if (
      state >= profileStateNames.length ||
      error >= profileErrorNames.length ||
      (flags & ~3) !== 0
    ) {
      throw new WireError("Profile UI event contains an unknown state");
    }
    const event = {
      sessionGeneration: header.sessionGeneration,
      messageSequence: header.sequence,
      state,
      stateName: profileStateNames[state],
      error,
      errorName: profileErrorNames[error],
      hasSnapshot: (flags & 1) !== 0,
      hasPendingSave: (flags & 2) !== 0,
      committedSaveCount: view.getBigUint64(offset + 8, true),
      logicalSequence: view.getBigUint64(offset + 16, true),
      snapshotId: readId(view, offset + 24)
    };
    if (!idEqual(event.snapshotId, header.requestId)) {
      throw new WireError("Profile UI event snapshot identity drifted");
    }
    return event;
  }

  function questUiResultShapeValid(result) {
    if (
      result.status < 0 ||
      result.status >= questUiResultStatusNames.length ||
      result.rejectionReason < 0 ||
      result.rejectionReason >= questUiRejectionReasonNames.length
    ) {
      return false;
    }
    if (result.status === 0) {
      return result.id === 0n && result.objective === 0n && result.rejectionReason === 0;
    }
    if (result.id === 0n || result.objective === 0n) return false;
    if (result.status === 1 || result.status === 4) return result.rejectionReason === 0;
    if (result.status === 2) return result.rejectionReason !== 0;
    if (result.status === 3) return result.rejectionReason === 2;
    return false;
  }

  function questUiResultNotApplicable(result) {
    return result.status === 0 && result.id === 0n && result.objective === 0n &&
      result.rejectionReason === 0;
  }

  function questUiClassificationMatchesSource(source, classification) {
    switch (source) {
      case 1:
        return classification === 1 || classification === 3 || classification === 7;
      case 2:
        return [2, 3, 4, 5, 6].includes(classification);
      case 3:
        return classification === 1 || classification === 8;
      case 4:
        return classification === 9 || classification === 10;
      case 5:
        return classification === 11;
      case 6:
        return classification === 12;
      default:
        return false;
    }
  }

  function questUiSourceShapeValid(event) {
    switch (event.source) {
      case 1:
      case 3:
      case 5:
      case 6:
        return questUiResultNotApplicable(event.primaryResult) &&
          questUiResultNotApplicable(event.secondaryResult);
      case 2:
        return event.primaryResult.status !== 0 && event.primaryResult.status !== 4 &&
          questUiResultNotApplicable(event.secondaryResult);
      case 4:
        if (
          event.primaryResult.status === 0 ||
          event.primaryResult.status === 3 ||
          event.secondaryResult.status === 3
        ) {
          return false;
        }
        return questUiResultNotApplicable(event.secondaryResult) ||
          event.primaryResult.status === 1;
      default:
        return false;
    }
  }

  function questUiSurfaceAndPolarityValid(event) {
    const expectedSurface = event.source === 1 ? 1 : event.source === 5 ? 2 : 0;
    if (event.surface !== expectedSurface) return false;
    const recovery = event.source === 5 || event.source === 6;
    if (recovery) return event.polarity === 2;
    if (event.polarity === 2) return false;
    if (event.source === 1 || event.source === 3) return event.polarity === 0;
    const effectiveResult = questUiResultNotApplicable(event.secondaryResult)
      ? event.primaryResult
      : event.secondaryResult;
    return ![2, 3].includes(effectiveResult.status) || event.polarity === 1;
  }

  function decodeQuestUiEvent(value) {
    const decoded = decodeHeader(value, abi.messageType.quest_ui_event);
    const { view, header } = decoded;
    if (header.payloadLength !== abi.payload.questUiEventV1Bytes) {
      throw new WireError("Quest UI event has the wrong v1 size");
    }
    const offset = abi.headerBytes;
    const event = {
      sessionGeneration: header.sessionGeneration,
      messageSequence: header.sequence,
      sequence: view.getBigUint64(offset, true),
      tick: view.getBigUint64(offset + 8, true),
      questChecksum: view.getBigUint64(offset + 16, true),
      checksum: view.getBigUint64(offset + 24, true),
      cue: view.getBigUint64(offset + 32, true),
      beat: view.getBigUint64(offset + 40, true),
      objective: view.getBigUint64(offset + 48, true),
      safePoint: view.getBigUint64(offset + 56, true),
      pendingObjective: view.getBigUint64(offset + 64, true),
      primaryResult: {
        id: view.getBigUint64(offset + 72, true),
        objective: view.getBigUint64(offset + 80, true),
        status: view.getUint16(offset + 114, true),
        rejectionReason: view.getUint16(offset + 116, true)
      },
      secondaryResult: {
        id: view.getBigUint64(offset + 88, true),
        objective: view.getBigUint64(offset + 96, true),
        status: view.getUint16(offset + 118, true),
        rejectionReason: view.getUint16(offset + 120, true)
      },
      source: view.getUint16(offset + 104, true),
      surface: view.getUint16(offset + 106, true),
      polarity: view.getUint16(offset + 108, true),
      objectiveState: view.getUint16(offset + 110, true),
      attemptTimeClassification: view.getUint16(offset + 112, true),
      choiceOptions: [],
      selectedOptions: [],
      activeActorKeys: [],
      defeatedActorKeys: [],
      retainedObjectives: []
    };
    const choiceOptionCount = view.getUint16(offset + 122, true);
    const selectedOptionCount = view.getUint16(offset + 124, true);
    const activeActorCount = view.getUint16(offset + 126, true);
    const defeatedActorCount = view.getUint16(offset + 128, true);
    const retainedObjectiveCount = view.getUint16(offset + 130, true);
    if (
      event.sequence === 0n || event.questChecksum === 0n || event.checksum === 0n ||
      event.cue === 0n || event.beat === 0n || event.objective === 0n ||
      event.safePoint === 0n ||
      header.requestId.high !== event.checksum || header.requestId.low !== event.sequence ||
      event.source <= 0 || event.source >= questUiSourceNames.length ||
      event.surface < 0 || event.surface >= questUiSurfaceNames.length ||
      event.polarity < 0 || event.polarity >= questUiPolarityNames.length ||
      event.objectiveState < 0 || event.objectiveState >= questUiObjectiveStateNames.length ||
      event.attemptTimeClassification <= 0 ||
      event.attemptTimeClassification >= questUiAttemptTimeClassificationNames.length ||
      view.getUint16(offset + 132, true) !== 0 ||
      view.getUint16(offset + 134, true) !== 0 ||
      choiceOptionCount > abi.payload.questUiChoiceOptionCapacity ||
      selectedOptionCount > abi.payload.questUiSelectedOptionCapacity ||
      activeActorCount > abi.payload.questUiActorCapacity ||
      defeatedActorCount > abi.payload.questUiActorCapacity ||
      retainedObjectiveCount > abi.payload.questUiRetainedObjectiveCapacity ||
      (event.objectiveState === 1
        ? event.pendingObjective !== event.objective
        : event.pendingObjective !== 0n) ||
      !questUiResultShapeValid(event.primaryResult) ||
      !questUiResultShapeValid(event.secondaryResult) ||
      !questUiClassificationMatchesSource(
        event.source,
        event.attemptTimeClassification
      ) ||
      !questUiSourceShapeValid(event) ||
      !questUiSurfaceAndPolarityValid(event)
    ) {
      throw new WireError("Quest UI event violates the authoritative projection contract");
    }

    let cursor = offset + 136;
    const interactionKeys = new Set();
    const selectionKeys = new Set();
    for (let index = 0; index < abi.payload.questUiChoiceOptionCapacity; index += 1) {
      const interaction = view.getBigUint64(cursor, true);
      const selection = view.getBigUint64(cursor + 8, true);
      cursor += 16;
      if (index >= choiceOptionCount) {
        if (interaction !== 0n || selection !== 0n) {
          throw new WireError("Quest UI choice option tail is not canonical");
        }
        continue;
      }
      const interactionIdentity = interaction.toString();
      const selectionIdentity = selection.toString();
      if (
        interaction === 0n || selection === 0n ||
        interactionKeys.has(interactionIdentity) || selectionKeys.has(selectionIdentity)
      ) {
        throw new WireError("Quest UI choice options are empty or duplicated");
      }
      interactionKeys.add(interactionIdentity);
      selectionKeys.add(selectionIdentity);
      event.choiceOptions.push({ interaction, selection });
    }
    if ((event.source === 1) !== (choiceOptionCount > 0)) {
      throw new WireError("Quest UI choice options do not match the projection source");
    }

    const selectedObjectives = new Set();
    for (let index = 0; index < abi.payload.questUiSelectedOptionCapacity; index += 1) {
      const objective = view.getBigUint64(cursor, true);
      const selection = view.getBigUint64(cursor + 8, true);
      cursor += 16;
      if (index >= selectedOptionCount) {
        if (objective !== 0n || selection !== 0n) {
          throw new WireError("Quest UI selected option tail is not canonical");
        }
        continue;
      }
      const identity = objective.toString();
      if (objective === 0n || selection === 0n || selectedObjectives.has(identity)) {
        throw new WireError("Quest UI selected options are empty or duplicated");
      }
      selectedObjectives.add(identity);
      event.selectedOptions.push({ objective, selection });
    }

    const activeActors = new Set();
    let previousActor = 0n;
    for (let index = 0; index < abi.payload.questUiActorCapacity; index += 1) {
      const actor = view.getBigUint64(cursor, true);
      cursor += 8;
      if (index >= activeActorCount) {
        if (actor !== 0n) throw new WireError("Quest UI active Actor tail is not canonical");
        continue;
      }
      if (actor === 0n || actor <= previousActor) {
        throw new WireError("Quest UI active Actors are not canonical");
      }
      previousActor = actor;
      activeActors.add(actor.toString());
      event.activeActorKeys.push(actor);
    }
    previousActor = 0n;
    for (let index = 0; index < abi.payload.questUiActorCapacity; index += 1) {
      const actor = view.getBigUint64(cursor, true);
      cursor += 8;
      if (index >= defeatedActorCount) {
        if (actor !== 0n) throw new WireError("Quest UI defeated Actor tail is not canonical");
        continue;
      }
      if (actor === 0n || actor <= previousActor || activeActors.has(actor.toString())) {
        throw new WireError("Quest UI defeated Actors are not canonical");
      }
      previousActor = actor;
      event.defeatedActorKeys.push(actor);
    }

    const retainedObjectives = new Set();
    for (let index = 0; index < abi.payload.questUiRetainedObjectiveCapacity; index += 1) {
      const objective = view.getBigUint64(cursor, true);
      cursor += 8;
      if (index >= retainedObjectiveCount) {
        if (objective !== 0n) {
          throw new WireError("Quest UI retained Objective tail is not canonical");
        }
        continue;
      }
      const identity = objective.toString();
      if (objective === 0n || retainedObjectives.has(identity)) {
        throw new WireError("Quest UI retained Objectives are empty or duplicated");
      }
      retainedObjectives.add(identity);
      event.retainedObjectives.push(objective);
    }
    if (cursor !== decoded.input.byteLength) {
      throw new WireError("Quest UI event payload layout drifted");
    }
    return event;
  }

  function publicQuestUiResult(result) {
    return Object.freeze({
      id: stableKeyToHex(result.id),
      objective: stableKeyToHex(result.objective),
      status: result.status,
      statusName: questUiResultStatusNames[result.status],
      rejectionReason: result.rejectionReason,
      rejectionReasonName: questUiRejectionReasonNames[result.rejectionReason]
    });
  }

  function publicQuestUiEvent(event) {
    if (!event) return null;
    return Object.freeze({
      sessionGeneration: event.sessionGeneration,
      messageSequence: event.messageSequence.toString(),
      sequence: event.sequence.toString(),
      tick: event.tick.toString(),
      questChecksum: stableKeyToHex(event.questChecksum),
      checksum: stableKeyToHex(event.checksum),
      cue: stableKeyToHex(event.cue),
      beat: stableKeyToHex(event.beat),
      objective: stableKeyToHex(event.objective),
      safePoint: stableKeyToHex(event.safePoint),
      pendingObjective: stableKeyToHex(event.pendingObjective),
      primaryResult: publicQuestUiResult(event.primaryResult),
      secondaryResult: publicQuestUiResult(event.secondaryResult),
      source: event.source,
      sourceName: questUiSourceNames[event.source],
      surface: event.surface,
      surfaceName: questUiSurfaceNames[event.surface],
      polarity: event.polarity,
      polarityName: questUiPolarityNames[event.polarity],
      objectiveState: event.objectiveState,
      objectiveStateName: questUiObjectiveStateNames[event.objectiveState],
      attemptTimeClassification: event.attemptTimeClassification,
      attemptTimeClassificationName:
        questUiAttemptTimeClassificationNames[event.attemptTimeClassification],
      choiceOptions: Object.freeze(event.choiceOptions.map((option) => Object.freeze({
        interaction: stableKeyToHex(option.interaction),
        selection: stableKeyToHex(option.selection)
      }))),
      selectedOptions: Object.freeze(event.selectedOptions.map((option) => Object.freeze({
        objective: stableKeyToHex(option.objective),
        selection: stableKeyToHex(option.selection)
      }))),
      activeActorKeys: Object.freeze(event.activeActorKeys.map(stableKeyToHex)),
      defeatedActorKeys: Object.freeze(event.defeatedActorKeys.map(stableKeyToHex)),
      retainedObjectives: Object.freeze(event.retainedObjectives.map(stableKeyToHex))
    });
  }

  function decodeQuestUiCloseAck(value) {
    const decoded = decodeHeader(value, abi.messageType.quest_ui_close_ack);
    const { view, header } = decoded;
    if (header.payloadLength !== abi.payload.questUiCloseAckV1Bytes) {
      throw new WireError("Quest UI close acknowledgement has the wrong v1 size");
    }
    const offset = abi.headerBytes;
    const acknowledgement = {
      sessionGeneration: header.sessionGeneration,
      messageSequence: header.sequence,
      projectionSequence: view.getBigUint64(offset, true),
      projectionChecksum: view.getBigUint64(offset + 8, true),
      reason: view.getUint16(offset + 16, true)
    };
    if (
      acknowledgement.sessionGeneration === 0 ||
      acknowledgement.projectionSequence === 0n ||
      acknowledgement.projectionChecksum === 0n ||
      acknowledgement.reason !== abi.questUiCloseReason.selection_committed ||
      view.getUint16(offset + 18, true) !== 0 ||
      view.getUint16(offset + 20, true) !== 0 ||
      view.getUint16(offset + 22, true) !== 0 ||
      header.requestId.high !== acknowledgement.projectionChecksum ||
      header.requestId.low !== acknowledgement.projectionSequence
    ) {
      throw new WireError("Quest UI close acknowledgement identity drifted");
    }
    return acknowledgement;
  }

  function publicQuestUiCloseAck(acknowledgement) {
    if (!acknowledgement) return null;
    return Object.freeze({
      sessionGeneration: acknowledgement.sessionGeneration,
      messageSequence: acknowledgement.messageSequence.toString(),
      projectionSequence: acknowledgement.projectionSequence.toString(),
      projectionChecksum: stableKeyToHex(acknowledgement.projectionChecksum),
      reason: acknowledgement.reason,
      reasonName: "selection_committed"
    });
  }

  function encodeQuestUiSelectionIntent({
    projectionSequence,
    projectionChecksum,
    objective,
    interaction,
    selection,
    sessionGeneration,
    sequence
  }) {
    const projectionSequenceValue = sequenceFromValue(projectionSequence);
    const projectionChecksumValue = stableKeyFromValue(projectionChecksum);
    const objectiveValue = stableKeyFromValue(objective);
    const interactionValue = stableKeyFromValue(interaction);
    const selectionValue = stableKeyFromValue(selection);
    const wireSequence = sequenceFromValue(sequence);
    if (!Number.isInteger(sessionGeneration) || sessionGeneration <= 0 ||
        sessionGeneration > 0xffff_ffff) {
      throw new WireError("Quest UI intent requires a nonzero uint32 session generation");
    }
    const output = new Uint8Array(
      abi.headerBytes + abi.payload.questUiSelectionIntentV1Bytes
    );
    const view = new DataView(output.buffer);
    encodeHeader(
      view,
      abi.messageType.quest_ui_selection_intent,
      abi.payload.questUiSelectionIntentV1Bytes,
      sessionGeneration,
      wireSequence,
      { high: projectionChecksumValue, low: projectionSequenceValue }
    );
    const offset = abi.headerBytes;
    view.setBigUint64(offset, projectionSequenceValue, true);
    view.setBigUint64(offset + 8, projectionChecksumValue, true);
    view.setBigUint64(offset + 16, objectiveValue, true);
    view.setBigUint64(offset + 24, interactionValue, true);
    view.setBigUint64(offset + 32, selectionValue, true);
    return output;
  }

  function uiMessageType(value) {
    const input = bytes(value);
    if (input.byteLength < abi.headerBytes) {
      throw new WireError("UI event message is truncated");
    }
    return new DataView(input.buffer, input.byteOffset, input.byteLength).getUint16(4, true);
  }

  function encodeCompletionMessages(completion, nextSequence) {
    const body = completion.error === storageError.none
      ? bytes(completion.bytes ?? new Uint8Array())
      : new Uint8Array();
    const chunkCount = body.byteLength === 0
      ? 1
      : Math.ceil(body.byteLength / MAX_COMPLETION_CHUNK_BYTES);
    const messages = [];
    let chunkOffset = 0;
    for (let chunkIndex = 0; chunkIndex < chunkCount; chunkIndex += 1) {
      const chunkLength = body.byteLength === 0
        ? 0
        : Math.min(MAX_COMPLETION_CHUNK_BYTES, body.byteLength - chunkOffset);
      const payloadLength = COMPLETION_HEADER_BYTES + chunkLength;
      const output = new Uint8Array(abi.headerBytes + payloadLength);
      const view = new DataView(output.buffer);
      encodeHeader(
        view,
        abi.messageType.storage_completion,
        payloadLength,
        completion.request.header.sessionGeneration,
        nextSequence(),
        completion.request.header.requestId
      );
      const offset = abi.headerBytes;
      view.setUint16(offset, completion.request.operation, true);
      view.setUint16(offset + 2, completion.error, true);
      view.setUint16(offset + 4, completion.request.recordKind, true);
      view.setUint16(offset + 6, completion.persistenceGranted ? 1 : 0, true);
      view.setUint32(offset + 8, chunkIndex, true);
      view.setUint32(offset + 12, chunkCount, true);
      view.setUint32(offset + 16, body.byteLength, true);
      view.setUint32(offset + 20, chunkOffset, true);
      view.setUint32(offset + 24, chunkLength, true);
      view.setUint32(offset + 28, 0, true);
      writeId(view, offset + 32, completion.request.key.profileId);
      writeId(view, offset + 48, completion.request.key.recordId);
      writeHead(
        view,
        offset + 64,
        completion.head ?? emptyHead(completion.request.key.profileId)
      );
      view.setBigUint64(offset + 136, completion.usageBytes ?? 0n, true);
      view.setBigUint64(offset + 144, completion.quotaBytes ?? 0n, true);
      output.set(body.subarray(chunkOffset, chunkOffset + chunkLength), abi.headerBytes + COMPLETION_HEADER_BYTES);
      messages.push(output);
      chunkOffset += chunkLength;
    }
    return messages;
  }

  function requestResult(request) {
    return new Promise((resolve, reject) => {
      request.addEventListener("success", () => resolve(request.result), { once: true });
      request.addEventListener("error", () => reject(request.error ?? new Error("IndexedDB request failed")), {
        once: true
      });
    });
  }

  function transactionDone(transaction) {
    return new Promise((resolve, reject) => {
      transaction.addEventListener("complete", resolve, { once: true });
      transaction.addEventListener(
        "abort",
        () => reject(transaction.error ?? new DOMException("Transaction aborted", "AbortError")),
        { once: true }
      );
      transaction.addEventListener(
        "error",
        () => reject(transaction.error ?? new Error("IndexedDB transaction failed")),
        { once: true }
      );
    });
  }

  function mapStorageException(error) {
    switch (error?.name) {
      case "QuotaExceededError":
        return storageError.quota;
      case "ConstraintError":
        return storageError.conflict;
      case "DataError":
      case "DataCloneError":
      case "InvalidAccessError":
        return storageError.invalidRequest;
      case "AbortError":
      case "InvalidStateError":
      case "NotFoundError":
      case "UnknownError":
      case "VersionError":
        return storageError.unavailable;
      default:
        return storageError.internal;
    }
  }

  function validStoredHead(record, expectedProfileId) {
    return Boolean(
      record &&
      record.schemaVersion === 1 &&
      record.channel === CHANNEL &&
      record.profileId === expectedProfileId &&
      /^[0-9a-f]{32}$/.test(record.snapshotId) &&
      record.snapshotId !== "0".repeat(32) &&
      /^\d{20}$/.test(record.logicalSequence) &&
      BigInt(record.logicalSequence) > 0n &&
      BigInt(record.logicalSequence) <= MAX_U64 &&
      /^[0-9a-f]{64}$/.test(record.envelopeHash) &&
      record.envelopeHash !== ZERO_DIGEST
    );
  }

  function headFromRecord(record, expectedProfileId) {
    if (!validStoredHead(record, expectedProfileId)) {
      throw new WireError("IndexedDB Profile Head is corrupt");
    }
    return {
      profileId: idFromHex(record.profileId),
      snapshotId: idFromHex(record.snapshotId),
      logicalSequence: BigInt(record.logicalSequence),
      envelopeHash: record.envelopeHash
    };
  }

  function headToRecord(head) {
    return {
      schemaVersion: 1,
      channel: CHANNEL,
      profileId: idToHex(head.profileId),
      snapshotId: idToHex(head.snapshotId),
      logicalSequence: head.logicalSequence.toString().padStart(20, "0"),
      envelopeHash: head.envelopeHash
    };
  }

  function headMatchesRecord(head, record) {
    if (head.logicalSequence === 0n) {
      return !record && idEmpty(head.snapshotId) && head.envelopeHash === ZERO_DIGEST;
    }
    try {
      const stored = headFromRecord(record, idToHex(head.profileId));
      return (
        idEqual(stored.profileId, head.profileId) &&
        idEqual(stored.snapshotId, head.snapshotId) &&
        stored.logicalSequence === head.logicalSequence &&
        stored.envelopeHash === head.envelopeHash
      );
    } catch {
      return false;
    }
  }

  function hashU64(hash, value) {
    let next = hash;
    for (let index = 0n; index < 8n; index += 1n) {
      next ^= (value >> (index * 8n)) & 0xffn;
      next = (next * FNV_PRIME) & MAX_U64;
    }
    return next;
  }

  function rewardOperationId(profileId, rewardDedupKey) {
    let high = hashU64(FNV_OFFSET, 0x7265776172642d31n);
    high = hashU64(high, profileId.high);
    high = hashU64(high, profileId.low);
    high = hashU64(high, rewardDedupKey);
    let low = hashU64(FNV_OFFSET, 0x636c61696d2d7631n);
    low = hashU64(low, rewardDedupKey);
    low = hashU64(low, profileId.low);
    low = hashU64(low, profileId.high);
    if (high === 0n && low === 0n) low = 1n;
    return { high, low };
  }

  function decodeAtomicOperations(request, deviceId) {
    const output = [];
    const operationIds = new Set();
    const rewardDedupKeys = new Set();
    for (let index = 0; index < request.operationCount; index += 1) {
      const offset = request.snapshotByteLength +
        index * abi.payload.persistentOperationV1Bytes;
      const operationBytes = request.body.subarray(
        offset,
        offset + abi.payload.persistentOperationV1Bytes
      );
      const view = new DataView(
        operationBytes.buffer,
        operationBytes.byteOffset,
        operationBytes.byteLength
      );
      const operationId = readId(view, 0);
      const profileId = readId(view, 16);
      const baseRevision = view.getBigUint64(32, true);
      const createdLogicalTime = view.getBigUint64(40, true);
      const domain = view.getUint16(48, true);
      const payloadVersion = view.getUint16(50, true);
      const reserved = view.getUint32(52, true);
      const sourceId = view.getBigUint64(56, true);
      const rewardId = view.getBigUint64(64, true);
      const rewardDedupKey = view.getBigUint64(72, true);
      const canonicalId = rewardOperationId(profileId, rewardDedupKey);
      const operationIdHex = idToHex(operationId);
      const rewardDedupHex = rewardDedupKey.toString(16).padStart(16, "0");
      if (
        idEmpty(operationId) ||
        !idEqual(profileId, request.key.profileId) ||
        !idEqual(operationId, canonicalId) ||
        baseRevision < request.expectedHead.logicalSequence ||
        baseRevision >= createdLogicalTime ||
        createdLogicalTime > request.nextHead.logicalSequence ||
        domain !== 1 ||
        payloadVersion !== 1 ||
        reserved !== 0 ||
        sourceId === 0n ||
        rewardId === 0n ||
        rewardDedupKey === 0n ||
        operationIds.has(operationIdHex) ||
        rewardDedupKeys.has(rewardDedupHex)
      ) {
        throw new WireError("Atomic write contains an invalid PersistentOperationV1");
      }
      operationIds.add(operationIdHex);
      rewardDedupKeys.add(rewardDedupHex);
      output.push({
        schemaVersion: 1,
        profileId: idToHex(profileId),
        operationId: operationIdHex,
        deviceId,
        baseRevision: baseRevision.toString().padStart(20, "0"),
        logicalSequence: createdLogicalTime.toString().padStart(20, "0"),
        domain,
        payloadVersion,
        sourceId: sourceId.toString(16).padStart(16, "0"),
        rewardId: rewardId.toString(16).padStart(16, "0"),
        rewardDedupKey: rewardDedupHex,
        status: "pending",
        bytes: operationBytes.buffer.slice(
          operationBytes.byteOffset,
          operationBytes.byteOffset + operationBytes.byteLength
        )
      });
    }
    return output;
  }

  function transactionWithDurability(database, stores, mode, strict) {
    if (strict) {
      try {
        return database.transaction(stores, mode, { durability: "strict" });
      } catch {
        // Firefox and older Chromium accept the transaction but not the durability option.
      }
    }
    return database.transaction(stores, mode);
  }

  class IndexedDbBackend {
    constructor(environment = {}) {
      this.indexedDB = environment.indexedDB ?? globalThis.indexedDB;
      this.navigator = environment.navigator ?? globalThis.navigator;
      if (
        environment.testFaultMode &&
        !["quota_once", "reward_abort_once", "reward_conflict_once"].includes(
          environment.testFaultMode
        )
      ) {
        throw new TypeError(`Unknown IndexedDB test fault: ${environment.testFaultMode}`);
      }
      if (
        typeof environment.deviceId !== "string" ||
        !/^[0-9a-f]{32}$/.test(environment.deviceId) ||
        environment.deviceId === "0".repeat(32)
      ) {
        throw new TypeError("A stable 128-bit deviceId is required for IndexedDB operations");
      }
      this.deviceId = environment.deviceId;
      this.remainingQuotaFailures = environment.testFaultMode === "quota_once" ? 1 : 0;
      this.remainingRewardAbortFailures =
        environment.testFaultMode === "reward_abort_once" ? 1 : 0;
      this.remainingRewardConflictFailures =
        environment.testFaultMode === "reward_conflict_once" ? 1 : 0;
      this.databasePromise = null;
      this.databaseHandle = null;
      this.transfers = new Map();
    }

    open() {
      if (this.databasePromise) return this.databasePromise;
      this.databasePromise = new Promise((resolve, reject) => {
        if (!this.indexedDB) {
          reject(new DOMException("IndexedDB is unavailable", "InvalidStateError"));
          return;
        }
        const request = this.indexedDB.open(schema.database.name, schema.database.version);
        request.addEventListener("upgradeneeded", () => {
          const database = request.result;
          for (const storeDefinition of schema.stores) {
            const store = database.createObjectStore(storeDefinition.name, {
              keyPath: storeDefinition.keyPath
            });
            for (const index of storeDefinition.indexes) {
              store.createIndex(index.name, index.keyPath, { unique: index.unique });
            }
          }
        });
        request.addEventListener(
          "blocked",
          () => reject(new DOMException("IndexedDB upgrade is blocked", "InvalidStateError")),
          { once: true }
        );
        request.addEventListener(
          "error",
          () => reject(request.error ?? new Error("IndexedDB open failed")),
          { once: true }
        );
        request.addEventListener(
          "success",
          () => {
            const database = request.result;
            const missing = schema.stores.filter(
              (store) => !database.objectStoreNames.contains(store.name)
            );
            if (database.version !== schema.database.version || missing.length > 0) {
              database.close();
              reject(new DOMException("IndexedDB v1 schema is incomplete", "VersionError"));
              return;
            }
            database.addEventListener("versionchange", () => database.close());
            this.databaseHandle = database;
            resolve(database);
          },
          { once: true }
        );
      });
      return this.databasePromise;
    }

    close() {
      this.databaseHandle?.close();
      this.databaseHandle = null;
      this.databasePromise = null;
      this.transfers.clear();
    }

    assemble(request) {
      const requestId = idToHex(request.header.requestId);
      let transfer = this.transfers.get(requestId);
      if (request.chunkIndex === 0) {
        if (transfer) throw new WireError("Duplicate first storage request chunk");
        transfer = {
          signature: this.signature(request),
          nextChunkIndex: 0,
          nextChunkOffset: 0,
          body: new Uint8Array(request.totalBytes),
          request
        };
        this.transfers.set(requestId, transfer);
      } else if (!transfer || transfer.signature !== this.signature(request)) {
        throw new WireError("Storage request chunk metadata drifted");
      }
      if (
        request.chunkIndex !== transfer.nextChunkIndex ||
        request.chunkOffset !== transfer.nextChunkOffset
      ) {
        throw new WireError("Storage request chunks are out of order");
      }
      transfer.body.set(request.chunk, request.chunkOffset);
      transfer.nextChunkIndex += 1;
      transfer.nextChunkOffset += request.chunkLength;
      if (transfer.nextChunkIndex !== request.chunkCount) return null;
      this.transfers.delete(requestId);
      if (transfer.nextChunkOffset !== request.totalBytes) {
        throw new WireError("Storage request chunks do not cover the declared payload");
      }
      return { ...transfer.request, body: transfer.body };
    }

    signature(request) {
      return [
        request.header.sessionGeneration,
        idToHex(request.header.requestId),
        request.operation,
        request.recordKind,
        request.durability,
        request.operationCount,
        request.snapshotByteLength,
        request.chunkCount,
        request.totalBytes,
        idToHex(request.key.profileId),
        idToHex(request.key.recordId),
        idToHex(request.expectedHead.profileId),
        idToHex(request.expectedHead.snapshotId),
        request.expectedHead.logicalSequence.toString(),
        request.expectedHead.envelopeHash,
        idToHex(request.nextHead.profileId),
        idToHex(request.nextHead.snapshotId),
        request.nextHead.logicalSequence.toString(),
        request.nextHead.envelopeHash
      ].join("|");
    }

    async handle(request) {
      let assembled;
      try {
        assembled = this.assemble(request);
      } catch (error) {
        this.transfers.delete(idToHex(request.header.requestId));
        return { request, error: storageError.invalidRequest };
      }
      if (!assembled) return null;
      try {
        switch (assembled.operation) {
          case abi.storageOperation.read:
            return await this.read(assembled);
          case abi.storageOperation.write_atomic:
            return await this.writeAtomic(assembled);
          case abi.storageOperation.list:
            return { request: assembled, error: storageError.invalidRequest };
          case abi.storageOperation.delete:
            return await this.deleteRecord(assembled);
          case abi.storageOperation.estimate_quota:
            return await this.estimateQuota(assembled);
          case abi.storageOperation.request_persistence:
            return await this.requestPersistence(assembled);
          default:
            return { request: assembled, error: storageError.invalidRequest };
        }
      } catch (error) {
        return { request: assembled, error: mapStorageException(error) };
      }
    }

    async read(request) {
      const database = await this.open();
      const profileId = idToHex(request.key.profileId);
      if (request.recordKind === 1) {
        if (idEmpty(request.key.profileId) || !idEmpty(request.key.recordId)) {
          return { request, error: storageError.invalidRequest };
        }
        const transaction = database.transaction("profile_heads", "readonly");
        const done = transactionDone(transaction);
        const record = await requestResult(
          transaction.objectStore("profile_heads").get([CHANNEL, profileId])
        );
        await done;
        if (!record) return { request, error: storageError.notFound };
        try {
          return { request, error: storageError.none, head: headFromRecord(record, profileId) };
        } catch {
          return { request, error: storageError.corrupt };
        }
      }
      if (request.recordKind === 2) {
        const snapshotId = idToHex(request.key.recordId);
        if (idEmpty(request.key.profileId) || idEmpty(request.key.recordId)) {
          return { request, error: storageError.invalidRequest };
        }
        const transaction = database.transaction("snapshots", "readonly");
        const done = transactionDone(transaction);
        const record = await requestResult(
          transaction.objectStore("snapshots").get([profileId, snapshotId])
        );
        await done;
        if (!record) return { request, error: storageError.notFound };
        if (
          record.schemaVersion !== 1 ||
          record.profileId !== profileId ||
          record.snapshotId !== snapshotId ||
          !(record.bytes instanceof ArrayBuffer)
        ) {
          return { request, error: storageError.corrupt };
        }
        return { request, error: storageError.none, bytes: new Uint8Array(record.bytes) };
      }
      return { request, error: storageError.invalidRequest };
    }

    async writeAtomic(request) {
      const profileId = idToHex(request.key.profileId);
      const snapshotId = idToHex(request.key.recordId);
      if (
        !idEqual(request.key.profileId, request.nextHead.profileId) ||
        !idEqual(request.key.recordId, request.nextHead.snapshotId) ||
        !idEqual(request.expectedHead.profileId, request.nextHead.profileId) ||
        request.nextHead.logicalSequence <= request.expectedHead.logicalSequence ||
        request.nextHead.envelopeHash === ZERO_DIGEST
      ) {
        return { request, error: storageError.invalidRequest };
      }
      let operationRecords;
      try {
        operationRecords = decodeAtomicOperations(request, this.deviceId);
      } catch {
        return { request, error: storageError.invalidRequest };
      }
      if (this.remainingQuotaFailures > 0) {
        this.remainingQuotaFailures -= 1;
        return { request, error: storageError.quota };
      }
      const database = await this.open();
      const outcome = await new Promise((resolve) => {
        let forcedError = null;
        const transaction = transactionWithDurability(
          database,
          ["snapshots", "operations", "profile_heads"],
          "readwrite",
          request.durability === 1
        );
        const heads = transaction.objectStore("profile_heads");
        const snapshots = transaction.objectStore("snapshots");
        const operations = transaction.objectStore("operations");
        const headRequest = heads.get([CHANNEL, profileId]);
        headRequest.addEventListener("success", () => {
          const current = headRequest.result;
          if (current && !validStoredHead(current, profileId)) {
            forcedError = storageError.corrupt;
            transaction.abort();
            return;
          }
          if (!headMatchesRecord(request.expectedHead, current)) {
            forcedError = storageError.conflict;
            transaction.abort();
            return;
          }
          if (operationRecords.length > 0 && this.remainingRewardConflictFailures > 0) {
            this.remainingRewardConflictFailures -= 1;
            forcedError = storageError.conflict;
            transaction.abort();
            return;
          }
          const nextRecord = headToRecord(request.nextHead);
          snapshots.add({
            schemaVersion: 1,
            profileId,
            snapshotId,
            logicalSequence: nextRecord.logicalSequence,
            envelopeHash: request.nextHead.envelopeHash,
            bytes: request.body.buffer.slice(
              request.body.byteOffset,
              request.body.byteOffset + request.snapshotByteLength
            )
          });
          for (const operation of operationRecords) operations.add(operation);
          heads.put(nextRecord);
          if (operationRecords.length > 0 && this.remainingRewardAbortFailures > 0) {
            this.remainingRewardAbortFailures -= 1;
            forcedError = storageError.unavailable;
            transaction.abort();
          }
        });
        transaction.addEventListener("complete", () => resolve(storageError.none), { once: true });
        transaction.addEventListener(
          "abort",
          () => resolve(forcedError ?? mapStorageException(transaction.error)),
          { once: true }
        );
        transaction.addEventListener(
          "error",
          () => {
            if (forcedError === null && transaction.error) {
              forcedError = mapStorageException(transaction.error);
            }
          }
        );
      });
      return outcome === storageError.none
        ? { request, error: storageError.none, head: request.nextHead }
        : { request, error: outcome };
    }

    async deleteRecord(request) {
      const definition = schema.stores[request.recordKind - 1];
      if (!definition || (request.recordKind !== 1 && request.recordKind !== 2)) {
        return { request, error: storageError.invalidRequest };
      }
      const profileId = idToHex(request.key.profileId);
      const key = request.recordKind === 1
        ? [CHANNEL, profileId]
        : [profileId, idToHex(request.key.recordId)];
      const database = await this.open();
      const transaction = database.transaction(definition.name, "readwrite");
      const done = transactionDone(transaction);
      transaction.objectStore(definition.name).delete(key);
      await done;
      return { request, error: storageError.none };
    }

    async estimateQuota(request) {
      const estimate = await this.navigator?.storage?.estimate?.();
      if (!estimate) return { request, error: storageError.unavailable };
      const integer = (value) => {
        const numeric = Number(value ?? 0);
        if (!Number.isFinite(numeric)) return 0n;
        const normalized = BigInt(Math.max(0, Math.trunc(numeric)));
        return normalized > MAX_U64 ? MAX_U64 : normalized;
      };
      return {
        request,
        error: storageError.none,
        usageBytes: integer(estimate.usage),
        quotaBytes: integer(estimate.quota)
      };
    }

    async requestPersistence(request) {
      const persist = this.navigator?.storage?.persist;
      if (typeof persist !== "function") {
        return { request, error: storageError.unavailable };
      }
      return {
        request,
        error: storageError.none,
        persistenceGranted: Boolean(await persist.call(this.navigator.storage))
      };
    }

    async inspectSchema() {
      const database = await this.open();
      return {
        name: database.name,
        version: database.version,
        stores: Array.from(database.objectStoreNames)
      };
    }

    async exportProfile(profileId) {
      const database = await this.open();
      const transaction = database.transaction(["profile_heads", "snapshots"], "readonly");
      const done = transactionDone(transaction);
      const headRecord = await requestResult(
        transaction.objectStore("profile_heads").get([CHANNEL, profileId])
      );
      const head = headFromRecord(headRecord, profileId);
      const snapshotId = idToHex(head.snapshotId);
      const snapshotRecord = await requestResult(
        transaction.objectStore("snapshots").get([profileId, snapshotId])
      );
      await done;
      if (
        !snapshotRecord ||
        snapshotRecord.schemaVersion !== 1 ||
        snapshotRecord.profileId !== profileId ||
        snapshotRecord.snapshotId !== snapshotId ||
        !(snapshotRecord.bytes instanceof ArrayBuffer)
      ) {
        throw new WireError("Current Profile snapshot is missing or unreadable");
      }
      return Object.freeze({
        fileName: `tiangongdu-${CHANNEL}-${profileId.slice(0, 8)}-${snapshotId.slice(0, 8)}.tgdprofile`,
        mediaType: "application/vnd.tiangongdu.profile+octet-stream",
        profileId,
        snapshotId,
        logicalSequence: head.logicalSequence.toString(),
        envelopeHash: head.envelopeHash,
        bytes: new Uint8Array(snapshotRecord.bytes)
      });
    }
  }

  function randomId(cryptoObject) {
    if (!cryptoObject?.getRandomValues) {
      throw new Error("Cryptographic randomness is required for StableId128 generation");
    }
    const words = new Uint32Array(4);
    cryptoObject.getRandomValues(words);
    // ProfileStorageCoordinator reserves a zero high half for invalid request seeds.
    if (words[0] === 0 && words[1] === 0) words[0] = 1;
    return idFromHex(
      Array.from(words, (word) => word.toString(16).padStart(8, "0")).join("")
    );
  }

  function guestProfileId(storage, cryptoObject) {
    try {
      const current = storage?.getItem(GUEST_PROFILE_KEY);
      if (current && /^[0-9a-f]{32}$/.test(current) && current !== "0".repeat(32)) {
        return idFromHex(current);
      }
      const created = randomId(cryptoObject);
      storage?.setItem(GUEST_PROFILE_KEY, idToHex(created));
      return created;
    } catch {
      return idFromHex("dfca66f2b1df5ad6a868786558aa23e5");
    }
  }

  function guestDeviceId(storage, cryptoObject) {
    try {
      const current = storage?.getItem(GUEST_DEVICE_KEY);
      if (current && /^[0-9a-f]{32}$/.test(current) && current !== "0".repeat(32)) {
        return idToHex(idFromHex(current));
      }
      const created = randomId(cryptoObject);
      storage?.setItem(GUEST_DEVICE_KEY, idToHex(created));
      return idToHex(created);
    } catch {
      return "fa141c7dbb5a5a3390a45ea99f3f2628";
    }
  }

  function randomSessionGeneration(cryptoObject) {
    const value = new Uint32Array(1);
    cryptoObject.getRandomValues(value);
    return value[0] || 1;
  }

  function publicProfileState(event) {
    if (!event) return null;
    return Object.freeze({
      sessionGeneration: event.sessionGeneration,
      messageSequence: event.messageSequence.toString(),
      state: event.state,
      stateName: event.stateName,
      error: event.error,
      errorName: event.errorName,
      hasSnapshot: event.hasSnapshot,
      hasPendingSave: event.hasPendingSave,
      committedSaveCount: event.committedSaveCount.toString(),
      logicalSequence: event.logicalSequence.toString(),
      snapshotId: idToHex(event.snapshotId)
    });
  }

  function advanceUiMessageSequence(current, event, expectedGeneration) {
    if (event.sessionGeneration !== expectedGeneration) return current;
    if (event.messageSequence <= current) {
      throw new WireError("UI event message sequence is stale or duplicated");
    }
    return event.messageSequence;
  }

  function createBridge(module, options = {}) {
    if (!module || typeof module.ccall !== "function") {
      throw new TypeError("An initialized Emscripten Module with ccall is required");
    }
    const cryptoObject = options.crypto ?? globalThis.crypto;
    const localStorageObject = options.localStorage ?? globalThis.localStorage;
    const profileId = guestProfileId(localStorageObject, cryptoObject);
    const deviceId = guestDeviceId(localStorageObject, cryptoObject);
    const backend = new IndexedDbBackend({ ...options, deviceId });
    const packageSetId = idFromHex(PACKAGE_SET_ID);
    const requestSeed = randomId(cryptoObject);
    const sessionGeneration = randomSessionGeneration(cryptoObject);
    let outboundSequence = 0n;
    let completionSequence = 0n;
    let latestProfileEvent = null;
    let latestQuestUiEvent = null;
    let latestQuestUiCloseAck = null;
    let latestUiMessageSequence = 0n;
    let activeDrain = null;
    let drainAgain = false;

    const call = (name, resultType = "number", argumentTypes = [], values = []) =>
      module.ccall(name, resultType, argumentTypes, values);

    function callWithBytes(name, value) {
      const input = bytes(value);
      const pointer = module._malloc(input.byteLength || 1);
      if (!pointer) throw new Error(`WASM allocation failed for ${name}`);
      try {
        module.HEAPU8.set(input, pointer);
        return call(name, "number", ["number", "number"], [pointer, input.byteLength]);
      } finally {
        module._free(pointer);
      }
    }

    function pollBytes(peekName, pollName) {
      const required = call(peekName);
      if (required === 0) return null;
      if (required < 0 || required > abi.maxMessageBytes) {
        throw new Error(`${peekName} returned invalid size ${required}`);
      }
      const pointer = module._malloc(required);
      if (!pointer) throw new Error(`WASM allocation failed for ${pollName}`);
      try {
        const written = call(pollName, "number", ["number", "number"], [pointer, required]);
        if (written !== required) throw new Error(`${pollName} wrote ${written}, expected ${required}`);
        return module.HEAPU8.slice(pointer, pointer + written);
      } finally {
        module._free(pointer);
      }
    }

    function publishUiEvents() {
      while (true) {
        const message = pollBytes("tgd_web_peek_ui_event_size", "tgd_web_poll_ui_event");
        if (!message) break;
        const messageType = uiMessageType(message);
        if (messageType === abi.messageType.ui_event) {
          const event = decodeProfileUiEvent(message);
          if (event.sessionGeneration !== sessionGeneration) continue;
          latestUiMessageSequence = advanceUiMessageSequence(
            latestUiMessageSequence,
            event,
            sessionGeneration
          );
          latestProfileEvent = event;
          options.onProfileState?.(publicProfileState(event));
        } else if (messageType === abi.messageType.quest_ui_event) {
          const event = decodeQuestUiEvent(message);
          if (event.sessionGeneration !== sessionGeneration) continue;
          latestUiMessageSequence = advanceUiMessageSequence(
            latestUiMessageSequence,
            event,
            sessionGeneration
          );
          latestQuestUiEvent = event;
          options.onQuestUiEvent?.(publicQuestUiEvent(event));
        } else if (messageType === abi.messageType.quest_ui_close_ack) {
          const acknowledgement = decodeQuestUiCloseAck(message);
          if (acknowledgement.sessionGeneration !== sessionGeneration) continue;
          latestUiMessageSequence = advanceUiMessageSequence(
            latestUiMessageSequence,
            acknowledgement,
            sessionGeneration
          );
          latestQuestUiCloseAck = acknowledgement;
          options.onQuestUiClose?.(publicQuestUiCloseAck(acknowledgement));
        } else {
          throw new WireError(`WASM emitted unknown UI message type ${messageType}`);
        }
      }
    }

    async function drainOnce() {
      publishUiEvents();
      while (true) {
        const message = pollBytes(
          "tgd_web_peek_platform_request_size",
          "tgd_web_poll_platform_request"
        );
        if (!message) break;
        const request = decodeStorageRequest(message);
        if (request.header.sessionGeneration !== sessionGeneration) {
          throw new WireError("WASM emitted a request for a stale session generation");
        }
        const completion = await backend.handle(request);
        if (completion) {
          for (const response of encodeCompletionMessages(completion, () => ++completionSequence)) {
            const accepted = callWithBytes("tgd_web_complete_async_request", response);
            if (accepted !== abi.errorCode.none) {
              throw new Error(`WASM rejected IndexedDB completion with Web ABI error ${accepted}`);
            }
          }
        }
        publishUiEvents();
      }
      publishUiEvents();
    }

    function drain() {
      if (activeDrain) {
        drainAgain = true;
        return activeDrain;
      }
      activeDrain = (async () => {
        do {
          drainAgain = false;
          await drainOnce();
        } while (drainAgain);
      })()
        .catch((error) => {
          options.onFatal?.(error);
          throw error;
        })
        .finally(() => {
          activeDrain = null;
        });
      return activeDrain;
    }

    async function boot() {
      const packedVersion = call("tgd_web_abi_version");
      const expectedVersion = (abi.major << 16) | abi.minor;
      if (packedVersion !== expectedVersion) {
        throw new Error(`Web ABI version mismatch: WASM=${packedVersion}, JS=${expectedVersion}`);
      }
      const message = encodeBootMessage({
        profileId,
        packageSetId,
        requestSeed,
        sessionGeneration,
        sequence: ++outboundSequence
      });
      const result = callWithBytes("tgd_web_boot", message);
      if (result !== abi.errorCode.none) {
        throw new Error(`Guest Profile boot failed with Web ABI error ${result}`);
      }
      await drain();
      return publicProfileState(latestProfileEvent);
    }

    async function saveGuestCheckpoint(checkpointKind = 2) {
      if (!latestProfileEvent || latestProfileEvent.stateName !== "ready") {
        throw new Error("Guest Profile is not ready to save");
      }
      const snapshotId = randomId(cryptoObject);
      const result = callWithBytes(
        "tgd_web_submit_ui_command",
        encodeSaveCommand({
          snapshotId,
          sessionGeneration,
          sequence: ++outboundSequence,
          checkpointKind
        })
      );
      if (result !== abi.errorCode.none) {
        throw new Error(`Guest checkpoint request failed with Web ABI error ${result}`);
      }
      publishUiEvents();
      await drain();
      return publicProfileState(latestProfileEvent);
    }

    async function retryPendingSave() {
      if (
        !latestProfileEvent ||
        latestProfileEvent.stateName !== "save_failed" ||
        !latestProfileEvent.hasPendingSave
      ) {
        throw new Error("Guest Profile has no retryable pending save");
      }
      const result = callWithBytes(
        "tgd_web_submit_ui_command",
        encodeRetryCommand({
          commandId: randomId(cryptoObject),
          sessionGeneration,
          sequence: ++outboundSequence
        })
      );
      if (result !== abi.errorCode.none) {
        throw new Error(`Guest checkpoint retry failed with Web ABI error ${result}`);
      }
      publishUiEvents();
      await drain();
      return publicProfileState(latestProfileEvent);
    }

    async function submitQuestUiSelection(intent) {
      const message = encodeQuestUiSelectionIntent({
        ...intent,
        sessionGeneration,
        sequence: ++outboundSequence
      });
      const result = callWithBytes("tgd_web_submit_quest_ui_selection_intent", message);
      if (result !== abi.errorCode.none) {
        throw new Error(`Quest UI selection failed with Web ABI error ${result}`);
      }
      publishUiEvents();
      await drain();
      return publicQuestUiEvent(latestQuestUiEvent);
    }

    function exportProfile() {
      return backend.exportProfile(idToHex(profileId));
    }

    function shutdown() {
      call("tgd_web_request_shutdown", null);
      backend.close();
    }

    return Object.freeze({
      boot,
      drain,
      exportProfile,
      retryPendingSave,
      saveGuestCheckpoint,
      submitQuestUiSelection,
      shutdown,
      getProfileState: () => publicProfileState(latestProfileEvent),
      getQuestUiState: () => publicQuestUiEvent(latestQuestUiEvent),
      getQuestUiCloseState: () => publicQuestUiCloseAck(latestQuestUiCloseAck),
      inspectSchema: () => backend.inspectSchema(),
      identity: Object.freeze({
        channel: CHANNEL,
        profileId: idToHex(profileId),
        deviceId,
        packageSetId: idToHex(packageSetId),
        sessionGeneration
      })
    });
  }

  globalThis.TgdIndexedDbStorage = Object.freeze({
    createBridge,
    database: Object.freeze({
      name: schema.database.name,
      version: schema.database.version,
      stores: Object.freeze(schema.stores.map((store) => store.name))
    }),
    profileStateNames,
    profileErrorNames,
    test: Object.freeze({
      decodeStorageRequest,
      decodeAtomicOperations,
      decodeProfileUiEvent,
      decodeQuestUiEvent,
      decodeQuestUiCloseAck,
      advanceUiMessageSequence,
      encodeBootMessage,
      encodeQuestUiSelectionIntent,
      encodeRetryCommand,
      encodeSaveCommand,
      encodeCompletionMessages,
      idFromHex,
      idToHex,
      publicQuestUiCloseAck,
      publicQuestUiEvent,
      randomId,
      rewardOperationId,
      stableKeyFromHex,
      stableKeyToHex,
      storageError
    })
  });
})();
