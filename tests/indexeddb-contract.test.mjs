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

function storageWriteMessage(abi) {
  const body = Uint8Array.of(1, 2, 3, 4, 5);
  const output = new Uint8Array(
    abi.headerBytes + abi.payload.storageRequestV1HeaderBytes + body.byteLength
  );
  const view = new DataView(output.buffer);
  const profile = [0x1111222233334444n, 0x5555666677778888n];
  const snapshot = [0x9999aaaabbbbccccn, 0xddddeeeeffff0001n];
  const request = [0x0102030405060708n, 0x1112131415161718n];
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
  view.setUint32(offset + 8, 0, true);
  view.setUint32(offset + 12, 1, true);
  view.setUint32(offset + 16, body.byteLength, true);
  view.setUint32(offset + 20, 0, true);
  view.setUint32(offset + 24, body.byteLength, true);
  writeId(view, offset + 32, ...profile);
  writeId(view, offset + 48, ...snapshot);
  writeHead(view, offset + 64, profile, [0n, 0n], 0n, 0);
  writeHead(view, offset + 136, profile, snapshot, 1n, 0x5a);
  output.set(body, abi.headerBytes + abi.payload.storageRequestV1HeaderBytes);
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
