import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import test from "node:test";

import {
  loadWebAbiContract,
  renderWebAbiOutputs,
  validateWebAbiContract
} from "../tools/generate-web-abi.mjs";

const root = resolve(import.meta.dirname, "..");
const outputPaths = Object.freeze({
  cHeader: resolve(root, "src/contracts/include/tgd/contracts/tgd_web_abi.h"),
  javascript: resolve(root, "apps/web-shell/web-abi.generated.js"),
  typescript: resolve(root, "apps/web-shell/web-abi.generated.d.ts")
});

test("Web ABI 的 C/JavaScript/TypeScript 合同保持同步", async () => {
  const contract = await loadWebAbiContract();
  const outputs = renderWebAbiOutputs(contract);
  for (const [name, path] of Object.entries(outputPaths)) {
    assert.equal(await readFile(path, "utf8"), outputs[name]);
  }
  assert.equal(contract.abi.headerBytes, 40);
  assert.equal(contract.abi.maxMessageBytes, 256 * 1024);
  assert.equal(contract.payloads.storageRequestV1HeaderBytes, 208);
  assert.equal(contract.payloads.storageCompletionV1HeaderBytes, 152);
  assert.equal(contract.payloads.maxStorageTransferBytes, 16 * 1024 * 1024 + 176);
});

test("Web ABI 各 ID 命名空间稳定且不碰撞", async () => {
  const contract = await loadWebAbiContract();
  for (const entries of [
    contract.messageTypes,
    contract.uiCommands,
    contract.storageOperations,
    contract.errorCodes
  ]) {
    assert.equal(new Set(entries.map((entry) => entry.id)).size, entries.length);
    assert.equal(new Set(entries.map((entry) => entry.name)).size, entries.length);
  }
  assert.equal(contract.messageTypes.find((entry) => entry.name === "storage_request")?.id, 100);
  assert.equal(contract.messageTypes.find((entry) => entry.name === "storage_completion")?.id, 101);
  assert.equal(contract.uiCommands.find((entry) => entry.name === "save_guest_checkpoint")?.id, 1);
  assert.equal(contract.uiCommands.find((entry) => entry.name === "retry_pending_save")?.id, 2);
});

test("Web ABI 对版本、顺序和消息元数据漂移失败关闭", async () => {
  const source = await loadWebAbiContract();
  const clone = () => structuredClone(source);

  const wrongVersion = clone();
  wrongVersion.abi.minor = 1;
  assert.throws(() => validateWebAbiContract(wrongVersion), /only supports 1\.0/);

  const wrongPayloadSize = clone();
  wrongPayloadSize.payloads.storageRequestV1HeaderBytes += 1;
  assert.throws(() => validateWebAbiContract(wrongPayloadSize), /payload size changed/);

  const unordered = clone();
  [unordered.storageOperations[0], unordered.storageOperations[1]] = [
    unordered.storageOperations[1],
    unordered.storageOperations[0]
  ];
  assert.throws(() => validateWebAbiContract(unordered), /strictly increasing/);

  const incomplete = clone();
  delete incomplete.messageTypes[0].direction;
  assert.throws(() => validateWebAbiContract(incomplete), /metadata is incomplete/);
});
