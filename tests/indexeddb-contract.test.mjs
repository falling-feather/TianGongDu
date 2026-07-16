import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import test from "node:test";
import vm from "node:vm";

import {
  renderIndexedDbContract,
  validateIndexedDbContract
} from "../tools/generate-indexeddb-contract.mjs";

const root = resolve(import.meta.dirname, "..");
const contractPath = resolve(root, "content/design/indexeddb-v1.json");
const generatedPath = resolve(root, "apps/web-shell/indexeddb-v1.generated.js");

async function loadBrowserContract() {
  const context = vm.createContext({ console });
  for (const path of [
    resolve(root, "apps/web-shell/web-abi.generated.js"),
    generatedPath,
    resolve(root, "apps/web-shell/indexeddb-storage.js")
  ]) {
    vm.runInContext(await readFile(path, "utf8"), context, { filename: path });
  }
  return {
    abi: context.TgdWebAbi,
    indexedDb: context.TgdIndexedDbContract,
    storage: context.TgdIndexedDbStorage
  };
}

function writeId(view, offset, high, low) {
  view.setBigUint64(offset, high, true);
  view.setBigUint64(offset + 8, low, true);
}

function writeHead(view, offset, profile, snapshot, sequence, digestByte) {
  writeId(view, offset, ...profile);
  writeId(view, offset + 16, ...snapshot);
  view.setBigUint64(offset + 32, sequence, true);
  new Uint8Array(view.buffer, offset + 40, 32).fill(digestByte);
}

function storageWriteMessage(abi, { withOperation = false } = {}) {
  const snapshotBytes = Uint8Array.of(1, 2, 3, 4, 5);
  const body = new Uint8Array(
    snapshotBytes.byteLength + (withOperation ? abi.payload.persistentOperationV1Bytes : 0)
  );
  body.set(snapshotBytes);
  const output = new Uint8Array(
    abi.headerBytes + abi.payload.storageRequestV1HeaderBytes + body.byteLength
  );
  const view = new DataView(output.buffer);
  const profile = [0x1111222233334444n, 0x5555666677778888n];
  const snapshot = [0x9999aaaabbbbccccn, 0xddddeeeeffff0001n];
  const request = [0x0102030405060708n, 0x1112131415161718n];
  if (withOperation) {
    const operationView = new DataView(body.buffer, snapshotBytes.byteLength);
    writeId(operationView, 0, 0x9e7f1d2b9a71e71en, 0x3a5eae7e53b6c75fn);
    writeId(operationView, 16, ...profile);
    operationView.setBigUint64(32, 0n, true);
    operationView.setBigUint64(40, 1n, true);
    operationView.setUint16(48, 1, true);
    operationView.setUint16(50, 1, true);
    operationView.setBigUint64(56, 0x7100000000000001n, true);
    operationView.setBigUint64(64, 0x7200000000000001n, true);
    operationView.setBigUint64(72, 0x7300000000000001n, true);
  }
  view.setUint16(0, abi.major, true);
  view.setUint16(2, abi.minor, true);
  view.setUint16(4, abi.messageType.storage_request, true);
  view.setUint16(6, 1, true);
  view.setUint32(8, abi.payload.storageRequestV1HeaderBytes + body.byteLength, true);
  view.setUint32(12, 17, true);
  view.setBigUint64(16, 3n, true);
  writeId(view, 24, ...request);
  const offset = abi.headerBytes;
  view.setUint16(offset, abi.storageOperation.write_atomic, true);
  view.setUint16(offset + 2, 2, true);
  view.setUint16(offset + 4, 1, true);
  view.setUint16(offset + 6, withOperation ? 1 : 0, true);
  view.setUint32(offset + 8, 0, true);
  view.setUint32(offset + 12, 1, true);
  view.setUint32(offset + 16, body.byteLength, true);
  view.setUint32(offset + 20, 0, true);
  view.setUint32(offset + 24, body.byteLength, true);
  view.setUint32(offset + 28, snapshotBytes.byteLength, true);
  writeId(view, offset + 32, ...profile);
  writeId(view, offset + 48, ...snapshot);
  writeHead(view, offset + 64, profile, [0n, 0n], 0n, 0);
  writeHead(view, offset + 136, profile, snapshot, 1n, 0x5a);
  output.set(body, abi.headerBytes + abi.payload.storageRequestV1HeaderBytes);
  return output;
}

function questUiEventMessage(abi, { combat = false } = {}) {
  const output = new Uint8Array(abi.headerBytes + abi.payload.questUiEventV1Bytes);
  const view = new DataView(output.buffer);
  const projectionSequence = 41n;
  const projectionChecksum = 0x5555666677778888n;
  const beat = 0x101n;
  const objective = 0x102n;
  view.setUint16(0, abi.major, true);
  view.setUint16(2, abi.minor, true);
  view.setUint16(4, abi.messageType.quest_ui_event, true);
  view.setUint16(6, 1, true);
  view.setUint32(8, abi.payload.questUiEventV1Bytes, true);
  view.setUint32(12, 17, true);
  view.setBigUint64(16, 5n, true);
  writeId(view, 24, projectionChecksum, projectionSequence);
  const offset = abi.headerBytes;
  const identities = [
    projectionSequence,
    97n,
    0x1111222233334444n,
    projectionChecksum,
    0x100n,
    beat,
    objective,
    0x103n,
    objective,
    combat ? 0x201n : 0n,
    combat ? 0x200n : 0n,
    combat ? 0x202n : 0n,
    combat ? objective : 0n
  ];
  identities.forEach((identity, index) => view.setBigUint64(offset + index * 8, identity, true));
  view.setUint16(offset + 104, combat ? 4 : 1, true);
  view.setUint16(offset + 106, combat ? 0 : 1, true);
  view.setUint16(offset + 108, combat ? 1 : 0, true);
  view.setUint16(offset + 110, 1, true);
  view.setUint16(offset + 112, combat ? 10 : 3, true);
  view.setUint16(offset + 114, combat ? 1 : 0, true);
  view.setUint16(offset + 116, 0, true);
  view.setUint16(offset + 118, combat ? 2 : 0, true);
  view.setUint16(offset + 120, combat ? 3 : 0, true);
  view.setUint16(offset + 122, combat ? 0 : 2, true);
  view.setUint16(offset + 124, 1, true);
  view.setUint16(offset + 126, 1, true);
  view.setUint16(offset + 128, 1, true);
  view.setUint16(offset + 130, 2, true);
  let cursor = offset + 136;
  if (!combat) {
    view.setBigUint64(cursor, 0x301n, true);
    view.setBigUint64(cursor + 8, 0x401n, true);
    view.setBigUint64(cursor + 16, 0x302n, true);
    view.setBigUint64(cursor + 24, 0x402n, true);
  }
  cursor += abi.payload.questUiChoiceOptionCapacity * 16;
  view.setBigUint64(cursor, 0x501n, true);
  view.setBigUint64(cursor + 8, 0x601n, true);
  cursor += abi.payload.questUiSelectedOptionCapacity * 16;
  view.setBigUint64(cursor, 0x701n, true);
  cursor += abi.payload.questUiActorCapacity * 8;
  view.setBigUint64(cursor, 0x702n, true);
  cursor += abi.payload.questUiActorCapacity * 8;
  view.setBigUint64(cursor, 0x801n, true);
  view.setBigUint64(cursor + 8, 0x802n, true);
  return output;
}

function questUiCloseAckMessage(abi, overrides = {}) {
  const projectionSequence = overrides.projectionSequence ?? 41n;
  const projectionChecksum = overrides.projectionChecksum ?? 0x5555666677778888n;
  const output = new Uint8Array(abi.headerBytes + abi.payload.questUiCloseAckV1Bytes);
  const view = new DataView(output.buffer);
  view.setUint16(0, abi.major, true);
  view.setUint16(2, abi.minor, true);
  view.setUint16(4, abi.messageType.quest_ui_close_ack, true);
  view.setUint16(6, 1, true);
  view.setUint32(8, abi.payload.questUiCloseAckV1Bytes, true);
  view.setUint32(12, overrides.sessionGeneration ?? 17, true);
  view.setBigUint64(16, 6n, true);
  writeId(view, 24, projectionChecksum, projectionSequence);
  const offset = abi.headerBytes;
  view.setBigUint64(offset, projectionSequence, true);
  view.setBigUint64(offset + 8, projectionChecksum, true);
  view.setUint16(
    offset + 16,
    overrides.reason ?? abi.questUiCloseReason.selection_committed,
    true
  );
  return output;
}

test("IndexedDB v1 机器合同与浏览器生成物保持同步", async () => {
  const contract = validateIndexedDbContract(JSON.parse(await readFile(contractPath, "utf8")));
  assert.equal(await readFile(generatedPath, "utf8"), renderIndexedDbContract(contract));
  assert.deepEqual(
    contract.stores.map((store) => [store.recordKind, store.name]),
    [
      [1, "profile_heads"],
      [2, "snapshots"],
      [3, "operations"],
      [4, "profile_meta"],
      [5, "device_settings"],
      [6, "migration_workspace"]
    ]
  );
});

test("Quest UI Web 信封无损解码双结果槽并精确编码选择意图", async () => {
  const { abi, storage } = await loadBrowserContract();
  const choice = storage.test.decodeQuestUiEvent(questUiEventMessage(abi));
  assert.equal(choice.sequence, 41n);
  assert.equal(choice.checksum, 0x5555666677778888n);
  assert.equal(choice.source, 1);
  assert.equal(choice.choiceOptions.length, 2);
  assert.deepEqual(
    Array.from(choice.choiceOptions, ({ interaction, selection }) => [interaction, selection]),
    [[0x301n, 0x401n], [0x302n, 0x402n]]
  );
  const publicChoice = storage.test.publicQuestUiEvent(choice);
  assert.equal(publicChoice.messageSequence, "5");
  assert.equal(publicChoice.sequence, "41");
  assert.equal(publicChoice.checksum, "5555666677778888");
  assert.equal(publicChoice.sourceName, "choice_available");
  assert.equal(publicChoice.choiceOptions[1].interaction, "0000000000000302");

  const intent = storage.test.encodeQuestUiSelectionIntent({
    projectionSequence: publicChoice.sequence,
    projectionChecksum: publicChoice.checksum,
    objective: publicChoice.objective,
    interaction: publicChoice.choiceOptions[1].interaction,
    selection: publicChoice.choiceOptions[1].selection,
    sessionGeneration: 17,
    sequence: 6n
  });
  const intentView = new DataView(intent.buffer);
  assert.equal(intent.byteLength, abi.headerBytes + abi.payload.questUiSelectionIntentV1Bytes);
  assert.equal(intentView.getUint16(4, true), abi.messageType.quest_ui_selection_intent);
  assert.equal(intentView.getBigUint64(24, true), choice.checksum);
  assert.equal(intentView.getBigUint64(32, true), choice.sequence);
  assert.equal(intentView.getBigUint64(40, true), choice.sequence);
  assert.equal(intentView.getBigUint64(48, true), choice.checksum);
  assert.equal(intentView.getBigUint64(64, true), choice.choiceOptions[1].interaction);
  assert.equal(intentView.getBigUint64(72, true), choice.choiceOptions[1].selection);

  const combat = storage.test.decodeQuestUiEvent(questUiEventMessage(abi, { combat: true }));
  assert.equal(combat.primaryResult.status, 1);
  assert.equal(combat.primaryResult.rejectionReason, 0);
  assert.equal(combat.secondaryResult.status, 2);
  assert.equal(combat.secondaryResult.rejectionReason, 3);
  assert.equal(storage.test.publicQuestUiEvent(combat).polarityName, "negative");
});

test("Quest UI Web 解码对身份、分类、结果形态和非零尾部失败关闭", async () => {
  const { abi, storage } = await loadBrowserContract();
  const canonical = questUiEventMessage(abi);
  const offset = abi.headerBytes;

  const mismatchedIdentity = canonical.slice();
  new DataView(mismatchedIdentity.buffer).setBigUint64(24, 1n, true);
  assert.throws(() => storage.test.decodeQuestUiEvent(mismatchedIdentity));

  const incompatibleClassification = canonical.slice();
  new DataView(incompatibleClassification.buffer).setUint16(offset + 112, 9, true);
  assert.throws(() => storage.test.decodeQuestUiEvent(incompatibleClassification));

  const nonzeroReserved = canonical.slice();
  new DataView(nonzeroReserved.buffer).setUint16(offset + 132, 1, true);
  assert.throws(() => storage.test.decodeQuestUiEvent(nonzeroReserved));

  const nonzeroChoiceTail = canonical.slice();
  new DataView(nonzeroChoiceTail.buffer).setBigUint64(offset + 136 + 32, 0x303n, true);
  assert.throws(() => storage.test.decodeQuestUiEvent(nonzeroChoiceTail));

  const invalidCombatReason = questUiEventMessage(abi, { combat: true });
  new DataView(invalidCombatReason.buffer).setUint16(offset + 116, 3, true);
  assert.throws(() => storage.test.decodeQuestUiEvent(invalidCombatReason));
});

test("Quest UI close acknowledgement 只接受精确投影身份和零预留字段", async () => {
  const { abi, storage } = await loadBrowserContract();
  const canonical = questUiCloseAckMessage(abi);
  const acknowledgement = storage.test.decodeQuestUiCloseAck(canonical);
  assert.equal(acknowledgement.sessionGeneration, 17);
  assert.equal(acknowledgement.projectionSequence, 41n);
  assert.equal(acknowledgement.projectionChecksum, 0x5555666677778888n);
  assert.equal(acknowledgement.reason, abi.questUiCloseReason.selection_committed);
  assert.deepEqual(
    { ...storage.test.publicQuestUiCloseAck(acknowledgement) },
    {
      sessionGeneration: 17,
      messageSequence: "6",
      projectionSequence: "41",
      projectionChecksum: "5555666677778888",
      reason: 1,
      reasonName: "selection_committed"
    }
  );

  const wrongChecksum = canonical.slice();
  new DataView(wrongChecksum.buffer).setBigUint64(24, 1n, true);
  assert.throws(() => storage.test.decodeQuestUiCloseAck(wrongChecksum));

  const wrongSequence = canonical.slice();
  new DataView(wrongSequence.buffer).setBigUint64(32, 1n, true);
  assert.throws(() => storage.test.decodeQuestUiCloseAck(wrongSequence));

  assert.throws(() => storage.test.decodeQuestUiCloseAck(
    questUiCloseAckMessage(abi, { sessionGeneration: 0 })
  ));
  assert.throws(() => storage.test.decodeQuestUiCloseAck(
    questUiCloseAckMessage(abi, { reason: 2 })
  ));

  const nonzeroReserved = canonical.slice();
  new DataView(nonzeroReserved.buffer).setUint16(abi.headerBytes + 18, 1, true);
  assert.throws(() => storage.test.decodeQuestUiCloseAck(nonzeroReserved));

  const projection = storage.test.decodeQuestUiEvent(questUiEventMessage(abi));
  const afterProjection = storage.test.advanceUiMessageSequence(0n, projection, 17);
  assert.equal(afterProjection, 5n);
  assert.equal(
    storage.test.advanceUiMessageSequence(afterProjection, acknowledgement, 17),
    6n
  );
  assert.throws(() => storage.test.advanceUiMessageSequence(6n, acknowledgement, 17));
  assert.throws(() => storage.test.advanceUiMessageSequence(6n, projection, 17));
  assert.equal(storage.test.advanceUiMessageSequence(6n, {
    ...acknowledgement,
    sessionGeneration: 18,
    messageSequence: 7n
  }, 17), 6n);
});

test("浏览器存储桥按显式小端编码 Boot 与保存命令", async () => {
  const { abi, storage } = await loadBrowserContract();
  const profile = storage.test.idFromHex("11112222333344445555666677778888");
  const packageSet = storage.test.idFromHex("9999aaaabbbbccccddddeeeeffff0001");
  const request = storage.test.idFromHex("01020304050607081112131415161718");
  const boot = storage.test.encodeBootMessage({
    profileId: profile,
    packageSetId: packageSet,
    requestSeed: request,
    sessionGeneration: 17,
    sequence: 3n
  });
  const bootView = new DataView(boot.buffer);
  assert.equal(boot.byteLength, 92);
  assert.equal(bootView.getUint16(4, true), abi.messageType.boot_config);
  assert.equal(bootView.getUint32(12, true), 17);
  assert.equal(bootView.getBigUint64(40, true), profile.high);
  assert.equal(bootView.getBigUint64(72, true), request.high);
  assert.equal(bootView.getUint32(88, true), 17);

  const save = storage.test.encodeSaveCommand({
    snapshotId: request,
    sessionGeneration: 17,
    sequence: 4n
  });
  const saveView = new DataView(save.buffer);
  assert.equal(save.byteLength, 60);
  assert.equal(saveView.getUint16(4, true), abi.messageType.ui_command);
  assert.equal(saveView.getUint16(40, true), 1);
  assert.equal(saveView.getUint16(42, true), 2);
  assert.equal(saveView.getBigUint64(44, true), request.high);

  const retry = storage.test.encodeRetryCommand({
    commandId: request,
    sessionGeneration: 17,
    sequence: 5n
  });
  const retryView = new DataView(retry.buffer);
  assert.equal(retryView.getUint16(40, true), abi.uiCommand.retry_pending_save);
  assert.equal(retryView.getBigUint64(44, true), request.high);
});

test("尚无快照的 Guest UI 事件允许空 snapshot identity", async () => {
  const { abi, storage } = await loadBrowserContract();
  const event = new Uint8Array(abi.headerBytes + abi.payload.uiEventV1Bytes);
  const view = new DataView(event.buffer);
  view.setUint16(0, abi.major, true);
  view.setUint16(2, abi.minor, true);
  view.setUint16(4, abi.messageType.ui_event, true);
  view.setUint16(6, 1, true);
  view.setUint32(8, abi.payload.uiEventV1Bytes, true);
  view.setUint32(12, 17, true);
  view.setBigUint64(16, 1n, true);
  view.setUint16(40, 2, true);
  const decoded = storage.test.decodeProfileUiEvent(event);
  assert.equal(decoded.stateName, "loading_head");
  assert.equal(decoded.hasSnapshot, false);
  assert.equal(decoded.committedSaveCount, 0n);
  assert.equal(storage.test.idToHex(decoded.snapshotId), "0".repeat(32));
});

test("请求种子即使随机源返回全零也保留非零 high half", async () => {
  const { storage } = await loadBrowserContract();
  const generated = storage.test.randomId({
    getRandomValues: (output) => output.fill(0)
  });
  assert.notEqual(generated.high, 0n);
  assert.equal(generated.low, 0n);
});

test("浏览器存储桥解码原子写请求并分块编码完成消息", async () => {
  const { abi, storage } = await loadBrowserContract();
  const request = storage.test.decodeStorageRequest(storageWriteMessage(abi));
  assert.equal(request.operation, abi.storageOperation.write_atomic);
  assert.equal(request.recordKind, 2);
  assert.equal(request.durability, 1);
  assert.equal(request.operationCount, 0);
  assert.equal(request.snapshotByteLength, 5);
  assert.equal(storage.test.idToHex(request.key.profileId), "11112222333344445555666677778888");
  assert.equal(storage.test.idToHex(request.key.recordId), "9999aaaabbbbccccddddeeeeffff0001");
  assert.deepEqual(Array.from(request.chunk), [1, 2, 3, 4, 5]);

  let sequence = 0n;
  const largeBody = new Uint8Array(300_000).fill(0x3c);
  const messages = storage.test.encodeCompletionMessages(
    {
      request,
      error: storage.test.storageError.none,
      head: request.nextHead,
      bytes: largeBody
    },
    () => ++sequence
  );
  assert.equal(messages.length, 2);
  const first = new DataView(messages[0].buffer);
  const second = new DataView(messages[1].buffer);
  assert.equal(first.getUint16(4, true), abi.messageType.storage_completion);
  assert.equal(first.getUint32(48, true), 0);
  assert.equal(first.getUint32(52, true), 2);
  assert.equal(first.getUint32(56, true), largeBody.byteLength);
  assert.equal(second.getUint32(48, true), 1);
  assert.equal(second.getUint32(60, true), 261_952);
  assert.equal(second.getUint32(64, true), 300_000 - 261_952);
  assert(messages.every((message) => message.byteLength <= abi.maxMessageBytes));
});

test("浏览器存储桥兼容 Web ABI 1.0 的快照原子写请求", async () => {
  const { abi, storage } = await loadBrowserContract();
  const legacyMessage = storageWriteMessage(abi);
  const legacyView = new DataView(legacyMessage.buffer);
  legacyView.setUint16(2, 0, true);
  legacyView.setUint32(abi.headerBytes + 28, 0, true);

  const request = storage.test.decodeStorageRequest(legacyMessage);
  assert.equal(request.operationCount, 0);
  assert.equal(request.snapshotByteLength, 5);
  assert.deepEqual(Array.from(request.chunk), [1, 2, 3, 4, 5]);
});

test("奖励 Operation 与快照共享显式边界并生成稳定 IndexedDB 记录", async () => {
  const { abi, storage } = await loadBrowserContract();
  const request = storage.test.decodeStorageRequest(
    storageWriteMessage(abi, { withOperation: true })
  );
  assert.equal(request.operationCount, 1);
  assert.equal(request.snapshotByteLength, 5);
  assert.equal(request.totalBytes, 5 + abi.payload.persistentOperationV1Bytes);
  assert.equal(
    storage.test.idToHex(storage.test.rewardOperationId(
      storage.test.idFromHex("01020304050607081112131415161718"),
      0x7300000000000001n
    )),
    "d41837c7680ae62e2bd4eb04b4860ecf"
  );
  const operations = storage.test.decodeAtomicOperations(
    { ...request, body: request.chunk },
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  );
  assert.equal(operations.length, 1);
  assert.equal(operations[0].operationId, "9e7f1d2b9a71e71e3a5eae7e53b6c75f");
  assert.equal(operations[0].rewardDedupKey, "7300000000000001");
  assert.equal(operations[0].logicalSequence, "00000000000000000001");
  assert.equal(operations[0].status, "pending");
  assert.equal(operations[0].bytes.byteLength, abi.payload.persistentOperationV1Bytes);
  assert.throws(
    () => storage.test.decodeAtomicOperations(
      {
        ...request,
        expectedHead: { ...request.expectedHead, logicalSequence: 1n },
        body: request.chunk
      },
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    ),
    /invalid PersistentOperationV1/,
    "an Operation cannot be rebased behind the expected Profile Head"
  );
});
