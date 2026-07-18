import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { pathToFileURL } from "node:url";

import {
  SandboxPackageServiceClient,
  SandboxPackageServiceTransportError,
  decodeSandboxPackageServiceResult
} from "../src/sandbox-package-service-client.mjs";
import {
  normalizeSandboxAuthoringDocument,
  projectSandboxRuntimeDocument
} from "../src/authoring-document.mjs";

function minimalResult() {
  const bytes = new Uint8Array(120);
  const data = new DataView(bytes.buffer);
  data.setUint8(0, 1);
  data.setUint8(1, 1);
  data.setUint8(2, 1);
  data.setUint32(4, 1, true);
  data.setUint32(68, 120, true);
  data.setUint32(72, 120, true);
  data.setUint32(76, 120, true);
  data.setUint16(80, 1, true);
  data.setUint16(82, 0, true);
  data.setUint8(44, 1);
  data.setUint8(45, 255);
  data.setUint16(46, 65535, true);
  return bytes;
}

function diagnosticResult() {
  const bytes = new Uint8Array(120 + 48 + 7);
  bytes.set(minimalResult(), 0);
  const data = new DataView(bytes.buffer);
  data.setUint8(1, 2);
  data.setUint8(2, 2);
  data.setUint8(3, 17);
  data.setUint32(8, 1, true);
  data.setUint32(68, 120, true);
  data.setUint32(72, 168, true);
  data.setUint32(76, bytes.length, true);
  data.setUint8(44, 255);
  data.setUint8(45, 255);
  data.setUint16(46, 65535, true);
  data.setUint16(120, 21, true);
  data.setUint8(122, 1);
  data.setUint8(123, 5);
  data.setUint16(124, 10, true);
  data.setUint16(126, 3, true);
  data.setUint32(128, 0, true);
  data.setUint32(148, 168, true);
  data.setUint32(152, 3, true);
  data.setUint32(156, 171, true);
  data.setUint32(160, 4, true);
  bytes.set(new TextEncoder().encode("one-two"), 168);
  return bytes;
}

async function validRuntime() {
  const fixtureUrl = new URL("fixtures/system-demo-authoring.v1.valid.json", import.meta.url);
  const source = JSON.parse(await readFile(fixtureUrl, "utf8"));
  return projectSandboxRuntimeDocument(normalizeSandboxAuthoringDocument(source));
}

function growingMock({ assetLimit = Infinity, failMallocAt = 0 } = {}) {
  const memory = new WebAssembly.Memory({ initial: 48, maximum: 256 });
  let next = 1024;
  let stringRef = 0;
  let generation = 0;
  const counters = {
    requestCreate: 0,
    copyUtf8: 0,
    append: 0,
    submit: 0,
    malloc: 0,
    allocations: [],
    frees: [],
    utf8Bytes: []
  };
  const module = {
    HEAPU8: new Uint8Array(memory.buffer),
    counters,
    _malloc(bytes) {
      counters.malloc += 1;
      if (counters.malloc === failMallocAt) return 0;
      const pointer = next;
      next = (next + bytes + 7) & ~7;
      while (next > memory.buffer.byteLength) memory.grow(1);
      module.HEAPU8 = new Uint8Array(memory.buffer);
      counters.allocations.push(pointer);
      return pointer;
    },
    _free(pointer) { counters.frees.push(pointer); },
    _tgd_sandbox_compiler_service_abi_version() { return 0x00010000; },
    _tgd_sandbox_compiler_service_create(output) {
      memory.grow(1);
      module.HEAPU8 = new Uint8Array(memory.buffer);
      new DataView(memory.buffer).setBigUint64(output, 0x100000001n, true);
      return 1;
    },
    _tgd_sandbox_compiler_service_destroy() { return 1; },
    _tgd_sandbox_compiler_service_read_identity(_service, output) {
      const data = new DataView(memory.buffer);
      data.setUint32(output, generation, true);
      module.HEAPU8.fill(0, output + 4, output + 36);
      return 1;
    },
    _tgd_sandbox_compile_request_create(_service, _expected, output) {
      counters.requestCreate += 1;
      memory.grow(1);
      module.HEAPU8 = new Uint8Array(memory.buffer);
      new DataView(memory.buffer).setBigUint64(output, 0x200000001n, true);
      return 1;
    },
    _tgd_sandbox_compile_request_cancel() { return 1; },
    _tgd_sandbox_compile_request_copy_utf8(_service, _request, _bytes, _length, output) {
      counters.copyUtf8 += 1;
      counters.utf8Bytes.push(Array.from(new Uint8Array(memory.buffer, _bytes, _length)));
      memory.grow(1);
      module.HEAPU8 = new Uint8Array(memory.buffer);
      new DataView(memory.buffer).setUint32(output, ++stringRef, true);
      return 1;
    },
    _tgd_sandbox_compile_request_set_metadata() { return 1; },
    _tgd_sandbox_compile_request_set_player() { return 1; },
    _tgd_sandbox_compile_request_submit(_service, _request, output, _capacity, written) {
      counters.submit += 1;
      generation += 1;
      const result = minimalResult();
      new DataView(result.buffer).setUint32(4, generation, true);
      module.HEAPU8.set(result, output);
      new DataView(memory.buffer).setUint32(written, result.length, true);
      return 1;
    }
  };
  const appendNames = [
    "region", "asset", "actor", "ground_blocker", "safe_point", "interaction",
    "mechanism", "wave", "wave_spawn", "objective", "interaction_binding",
    "mechanism_binding"
  ];
  for (const name of appendNames) {
    module["_tgd_sandbox_compile_request_append_" + name] = () => {
      counters.append += 1;
      if (name === "asset" && counters.append > assetLimit) return 6;
      return 1;
    };
  }
  return module;
}

test("Sandbox service result decoder accepts only a complete ABI 1.0 result", () => {
  const decoded = decodeSandboxPackageServiceResult(minimalResult());
  assert.equal(decoded.complete, true);
  assert.equal(decoded.outcome, 1);
  assert.equal(decoded.identity.generation, 1);
  assert.deepEqual(decoded.diagnostics, []);
  assert.equal(decoded.bindingValidation.code, 1);
});

test("Sandbox service result decoder exposes no partial diagnostic result", () => {
  const truncated = minimalResult().slice(0, 119);
  assert.throws(
    () => decodeSandboxPackageServiceResult(truncated),
    SandboxPackageServiceTransportError
  );
  const badCount = minimalResult();
  new DataView(badCount.buffer).setUint32(8, 1, true);
  assert.throws(
    () => decodeSandboxPackageServiceResult(badCount),
    SandboxPackageServiceTransportError
  );
});

test("result decoder rejects unknown enums and non-canonical ID coverage", () => {
  const valid = diagnosticResult();
  const decoded = decodeSandboxPackageServiceResult(valid);
  assert.equal(decoded.diagnostics[0].subjectId, "one");
  assert.equal(decoded.diagnostics[0].relatedId, "-two");

  const mutations = [
    (data) => data.setUint8(1, 0),
    (data) => data.setUint8(2, 255),
    (data) => data.setUint8(3, 255),
    (data) => data.setUint16(120, 34, true),
    (data) => data.setUint8(122, 255),
    (data) => data.setUint8(123, 14),
    (data) => data.setUint16(124, 65535, true),
    (data) => data.setUint32(148, 119, true),
    (data) => data.setUint32(156, 168, true),
    (data) => data.setUint16(126, 2, true),
    (data) => data.setUint32(164, 1, true)
  ];
  for (const mutate of mutations) {
    const bytes = valid.slice();
    mutate(new DataView(bytes.buffer));
    assert.throws(
      () => decodeSandboxPackageServiceResult(bytes),
      SandboxPackageServiceTransportError
    );
  }
  const trailing = new Uint8Array(valid.length + 1);
  trailing.set(valid);
  new DataView(trailing.buffer).setUint32(76, trailing.length, true);
  assert.throws(
    () => decodeSandboxPackageServiceResult(trailing),
    SandboxPackageServiceTransportError
  );
});

test("client survives memory growth during create and every UTF-8 copy", async () => {
  const module = growingMock();
  const client = SandboxPackageServiceClient.create(module);
  assert.equal(typeof client.serviceHandle, "bigint");
  const result = client.publish(await validRuntime());
  assert.equal(result.complete, true);
  assert.equal(result.identity.generation, 1);
  assert.equal(module.counters.requestCreate, 1);
  assert.ok(module.counters.copyUtf8 > 0);
  assert.ok(module.counters.append > 0);
  assert.equal(module.counters.submit, 1);
  client.destroy();
});

test("Unicode transport preserves valid pairs and rejects unpaired surrogates before allocation", async () => {
  const pairedModule = growingMock();
  const pairedClient = SandboxPackageServiceClient.create(pairedModule);
  const pairedRuntime = structuredClone(await validRuntime());
  pairedRuntime.packageId += "\ud83d\ude00";
  pairedClient.publish(pairedRuntime);
  assert.ok(pairedModule.counters.utf8Bytes.some((bytes) =>
    bytes.join(",") === Array.from(new TextEncoder().encode(pairedRuntime.packageId)).join(",")));
  pairedClient.destroy();

  for (const invalid of ["bad\ud800", "bad\udc00"]) {
    const module = growingMock();
    const client = SandboxPackageServiceClient.create(module);
    const runtime = structuredClone(await validRuntime());
    runtime.packageId = invalid;
    const expected = client.identity();
    const before = {
      malloc: module.counters.malloc,
      allocations: module.counters.allocations.length,
      frees: module.counters.frees.length
    };
    assert.throws(() => client.publish(runtime, expected), TypeError);
    assert.deepEqual({
      malloc: module.counters.malloc,
      allocations: module.counters.allocations.length,
      frees: module.counters.frees.length
    }, before);
    assert.equal(module.counters.requestCreate, 0);
    assert.equal(module.counters.copyUtf8, 0);
    assert.equal(client.identity().generation, 0);
    client.destroy();
  }
});

test("output allocation failure releases scratch once before any request or publication", async () => {
  const module = growingMock({ failMallocAt: 3 });
  const client = SandboxPackageServiceClient.create(module);
  const runtime = await validRuntime();
  assert.throws(
    () => client.publish(runtime, { generation: 0, checksum: Array(32).fill(0) }),
    (error) => error instanceof SandboxPackageServiceTransportError && error.status === 7
  );
  const scratch = module.counters.allocations[1];
  assert.equal(module.counters.frees.filter((pointer) => pointer === scratch).length, 1);
  assert.equal(module.counters.requestCreate, 0);
  assert.equal(module.counters.append, 0);
  assert.equal(module.counters.submit, 0);
  assert.equal(client.identity().generation, 0);
  client.destroy();
});

test("structural marshalling rejects closed-shape and wire integer drift before C ABI request", async () => {
  const runtime = await validRuntime();
  const cases = [];
  const addCase = (mutate) => {
    const value = structuredClone(runtime);
    mutate(value);
    cases.push(value);
  };
  addCase((value) => { value.packageId = 7; });
  addCase((value) => { value.player.facingMillidegrees = 4294967296; });
  addCase((value) => { value.waveSpawns[0].spawnOrder = 1.5; });
  addCase((value) => { value.waveSpawns[0].delayTicks = -1; });
  addCase((value) => { Object.defineProperty(value, "hidden", { value: true }); });
  addCase((value) => { value[Symbol("hidden")] = true; });
  addCase((value) => {
    Object.defineProperty(value, "packageId", { enumerable: true, get: () => "trap" });
  });
  addCase((value) => { value.assets.extra = true; });
  addCase((value) => { delete value.assets[0]; });
  for (const value of cases) {
    const module = growingMock();
    const client = SandboxPackageServiceClient.create(module);
    assert.throws(() => client.publish(value), TypeError);
    assert.equal(module.counters.requestCreate, 0);
    assert.equal(module.counters.append, 0);
    assert.equal(module.counters.submit, 0);
    client.destroy();
  }
  const checksumWithSymbol = Array(32).fill(0);
  checksumWithSymbol[Symbol("hidden")] = true;
  const checksumWithHidden = Array(32).fill(0);
  Object.defineProperty(checksumWithHidden, "hidden", { value: true });
  const checksumWithAccessor = Array(32).fill(0);
  Object.defineProperty(checksumWithAccessor, "0", { enumerable: true, get: () => 0 });
  const sparseChecksum = Array(32).fill(0);
  delete sparseChecksum[7];
  for (const expected of [
    { generation: 4294967296, checksum: Array(32).fill(0) },
    { generation: 0, checksum: Array(31).fill(0) },
    { generation: 0, checksum: [...Array(31).fill(0), 256] },
    { generation: 0, checksum: checksumWithSymbol },
    { generation: 0, checksum: checksumWithHidden },
    { generation: 0, checksum: checksumWithAccessor },
    { generation: 0, checksum: sparseChecksum }
  ]) {
    const module = growingMock();
    const client = SandboxPackageServiceClient.create(module);
    assert.throws(() => client.publish(runtime, expected), TypeError);
    assert.equal(module.counters.requestCreate, 0);
    assert.equal(module.counters.append, 0);
    assert.equal(module.counters.submit, 0);
    client.destroy();
  }
});

test("transport capacity failure consumes no publish and exposes no partial result", async () => {
  const runtime = structuredClone(await validRuntime());
  runtime.assets = Array.from({ length: 130 }, (_, index) => ({
    id: "asset.capacity." + index,
    kind: "effect"
  }));
  const module = growingMock({ assetLimit: 129 });
  const client = SandboxPackageServiceClient.create(module);
  assert.throws(
    () => client.publish(runtime),
    (error) => error instanceof SandboxPackageServiceTransportError && error.status === 6
  );
  assert.equal(module.counters.submit, 0);
  assert.equal(client.identity().generation, 0);
  client.destroy();
});

const wasmModulePath = process.env.TGD_SANDBOX_SERVICE_WASM_MODULE;
test("generated Web evidence module executes the common probe and service client", {
  skip: wasmModulePath ? false : "set TGD_SANDBOX_SERVICE_WASM_MODULE for the Web execution gate"
}, async () => {
  const imported = await import(pathToFileURL(wasmModulePath).href);
  const module = await imported.default({ noInitialRun: true });
  assert.equal(module._tgd_sandbox_service_run_contract_probe(), 0);

  const runtime = await validRuntime();
  const client = SandboxPackageServiceClient.create(module);
  assert.equal(typeof client.serviceHandle, "bigint");
  assert.deepEqual(client.identity(), {
    generation: 0,
    checksum: Object.freeze(Array(32).fill(0))
  });
  const first = client.publish(runtime);
  assert.equal(first.complete, true);
  assert.equal(first.outcome, 1);
  assert.equal(first.identity.generation, 1);
  const second = client.publish(runtime, first.identity);
  assert.equal(second.outcome, 1);
  assert.equal(second.identity.generation, 2);
  assert.deepEqual(second.identity.checksum, first.identity.checksum);

  const staleGeneration = client.publish(runtime, first.identity);
  assert.equal(staleGeneration.complete, true);
  assert.equal(staleGeneration.outcome, 3);
  assert.deepEqual(staleGeneration.identity, second.identity);
  assert.deepEqual(client.identity(), second.identity);

  const wrongChecksum = [...second.identity.checksum];
  wrongChecksum[0] ^= 0xff;
  const staleChecksum = client.publish(runtime, {
    generation: second.identity.generation,
    checksum: wrongChecksum
  });
  assert.equal(staleChecksum.complete, true);
  assert.equal(staleChecksum.outcome, 4);
  assert.deepEqual(staleChecksum.identity, second.identity);
  assert.deepEqual(client.identity(), second.identity);

  const missingReference = structuredClone(runtime);
  missingReference.player.regionId = "sandbox.region.missing";
  const missingResult = client.publish(missingReference, second.identity);
  assert.equal(missingResult.outcome, 2);
  assert.ok(missingResult.diagnostics.length > 0);
  assert.equal(client.identity().generation, 2);

  const invalidBinding = structuredClone(runtime);
  invalidBinding.interactionBindings[0].rangeMm = 499;
  const bindingResult = client.publish(invalidBinding, second.identity);
  assert.equal(bindingResult.outcome, 2);
  assert.notEqual(bindingResult.bindingValidation.code, 1);
  assert.equal(client.identity().generation, 2);

  const vector = module._malloc(1048576);
  const written = module._malloc(4);
  try {
    assert.equal(module._tgd_sandbox_service_write_invalid_utf8_result(
      vector, 1048576, written), 0);
    const length = new DataView(module.HEAPU8.buffer).getUint32(written, true);
    const decoded = decodeSandboxPackageServiceResult(
      module.HEAPU8.slice(vector, vector + length));
    assert.equal(decoded.outcome, 2);
    assert.ok(decoded.diagnostics.length > 0);
  } finally {
    module._free(written);
    module._free(vector);
  }
  client.destroy();
});
