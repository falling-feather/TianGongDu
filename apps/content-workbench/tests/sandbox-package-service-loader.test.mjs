import assert from "node:assert/strict";
import { after, test } from "node:test";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

import {
  SandboxPackageServiceLoaderError,
  createSandboxPackageServiceLoader
} from "../src/sandbox-package-service-loader.mjs";
import {
  normalizeSandboxAuthoringDocument,
  projectSandboxRuntimeDocument
} from "../src/authoring-document.mjs";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const temporaryParent = join(repositoryRoot, ".tmp", "content-workbench-loader-" +
  process.pid + "-" + Date.now());
await mkdir(temporaryParent, { recursive: true });
after(async () => rm(temporaryParent, { recursive: true, force: true }));

const fixtureSource = await readFile(
  new URL("../../../content/design/system-demo.sandbox.json", import.meta.url),
  "utf8"
);

function validRuntime() {
  return projectSandboxRuntimeDocument(
    normalizeSandboxAuthoringDocument(JSON.parse(fixtureSource))
  );
}

function generatedModuleSource(counterKey, { abi = 0x00010000, initialize = true } = {}) {
  const initializationGuard = initialize ? "" :
    `if (!globalThis[${JSON.stringify(`${counterKey}_ready`)}]) throw new Error("fixture initialization failure");`;
  return `
const counters = globalThis[${JSON.stringify(counterKey)}] = {
  factory: 0, destroy: 0, submit: 0, locatedWasm: "", noInitialRun: false
};
export default async function createModule(options) {
  ${initializationGuard}
  counters.factory += 1;
  counters.noInitialRun = options.noInitialRun === true;
  counters.locatedWasm = options.locateFile("tgd-sandbox-package-service-abi.wasm");
// Keep the fake heap above the append-only 1.1 result ceiling (4,194,440 bytes).
const buffer = new ArrayBuffer(8 * 1024 * 1024);
  let next = 1024;
  let stringRef = 0;
  let generation = 0;
  const module = {
    HEAPU8: new Uint8Array(buffer),
    _malloc(bytes) {
      const pointer = next;
      next = (next + bytes + 7) & ~7;
      return next <= buffer.byteLength ? pointer : 0;
    },
    _free() {},
    _tgd_sandbox_compiler_service_abi_version() { return ${abi}; },
    _tgd_sandbox_compiler_service_create(output) {
      new DataView(buffer).setBigUint64(output, 0x100000001n, true);
      return 1;
    },
    _tgd_sandbox_compiler_service_destroy() { counters.destroy += 1; return 1; },
    _tgd_sandbox_compiler_service_read_identity(_service, output) {
      const data = new DataView(buffer);
      data.setUint32(output, generation, true);
      module.HEAPU8.fill(0, output + 4, output + 36);
      return 1;
    },
    _tgd_sandbox_compile_request_create(_service, _identity, output) {
      new DataView(buffer).setBigUint64(output, 0x200000001n, true);
      return 1;
    },
    _tgd_sandbox_compile_request_cancel() { return 1; },
    _tgd_sandbox_compile_request_copy_utf8(_service, _request, _bytes, _length, output) {
      new DataView(buffer).setUint32(output, ++stringRef, true);
      return 1;
    },
    _tgd_sandbox_compile_request_set_metadata() { return 1; },
    _tgd_sandbox_compile_request_set_player() { return 1; },
    _tgd_sandbox_compile_request_submit(_service, _request, output, _capacity, written) {
      counters.submit += 1;
      generation += 1;
      module.HEAPU8.fill(0, output, output + 120);
      const data = new DataView(buffer);
      data.setUint8(output, 1);
      data.setUint8(output + 1, 1);
      data.setUint8(output + 2, 1);
      data.setUint32(output + 4, generation, true);
      data.setUint8(output + 44, 1);
      data.setUint8(output + 45, 255);
      data.setUint16(output + 46, 65535, true);
      data.setUint32(output + 68, 120, true);
      data.setUint32(output + 72, 120, true);
      data.setUint32(output + 76, 120, true);
      data.setUint16(output + 80, 1, true);
      data.setUint32(written, 120, true);
      return 1;
    }
  };
  for (const name of [
    "region", "asset", "actor", "ground_blocker", "safe_point", "interaction",
    "mechanism", "wave", "wave_spawn", "objective", "interaction_binding",
    "mechanism_binding"
  ]) module["_tgd_sandbox_compile_request_append_" + name] = () => 1;
  return module;
}
`;
}

async function createBuildFixture({ source, writeModule = true, writeWasm = true } = {}) {
  const buildRoot = await mkdtemp(join(temporaryParent, "case-"));
  const artifactRoot = join(buildRoot, "dist", "web");
  await mkdir(artifactRoot, { recursive: true });
  const counterKey = "__tgd_loader_" + Math.random().toString(36).slice(2);
  if (writeModule) {
    await writeFile(
      join(artifactRoot, "tgd-sandbox-package-service-abi.mjs"),
      source ?? generatedModuleSource(counterKey),
      "utf8"
    );
  }
  if (writeWasm) {
    await writeFile(join(artifactRoot, "tgd-sandbox-package-service-abi.wasm"), new Uint8Array());
  }
  return {
    buildDirectory: relative(repositoryRoot, buildRoot).split(sep).join("/"),
    counterKey
  };
}

function hasCode(code) {
  return (error) => error instanceof SandboxPackageServiceLoaderError && error.code === code;
}

test("trusted loader resolves only the fixed generated pair and returns owning values", async () => {
  const fixture = await createBuildFixture();
  const loader = createSandboxPackageServiceLoader({ buildDirectory: fixture.buildDirectory });
  const service = await loader.load();
  assert.deepEqual(Object.keys(service), []);
  assert.deepEqual(service.identity(), {
    generation: 0,
    checksum: Object.freeze(Array(32).fill(0))
  });
  const result = service.compileAndPublish(validRuntime());
  assert.equal(result.complete, true);
  assert.equal(result.outcome, 1);
  assert.equal(result.identity.generation, 1);
  assert.equal(Object.isFrozen(result), true);
  assert.equal(Object.isFrozen(result.diagnostics), true);
  const counters = globalThis[fixture.counterKey];
  assert.equal(counters.factory, 1);
  assert.equal(counters.noInitialRun, true);
  assert.equal(dirname(counters.locatedWasm).endsWith(join("dist", "web")), true);
  assert.equal(counters.submit, 1);
  service.close();
  assert.equal(counters.destroy, 1);
  assert.throws(() => service.identity(), hasCode("service_closed"));
  assert.throws(() => service.close(), hasCode("service_closed"));
});

test("loader options are closed and reject paths, URLs, symbols, and hidden fields", () => {
  const withSymbol = { buildDirectory: "build/web" };
  withSymbol[Symbol("hidden")] = true;
  const withHidden = { buildDirectory: "build/web" };
  Object.defineProperty(withHidden, "hidden", { value: true });
  for (const options of [
    null,
    {},
    { buildDirectory: "" },
    { buildDirectory: "." },
    { buildDirectory: "build/../escape" },
    { buildDirectory: "https://example.invalid/module.mjs" },
    { buildDirectory: "C:\\outside\\build" },
    { buildDirectory: "/outside/build" },
    { buildDirectory: "build/web", modulePath: "evil.mjs" },
    withSymbol,
    withHidden
  ]) {
    assert.throws(() => createSandboxPackageServiceLoader(options), hasCode("invalid_options"));
  }
});

test("missing pair, bad factory, bad ABI, and initialization failure stay closed", async () => {
  const missingModule = await createBuildFixture({ writeModule: false });
  await assert.rejects(
    createSandboxPackageServiceLoader({ buildDirectory: missingModule.buildDirectory }).load(),
    hasCode("artifact_unavailable")
  );

  const missingWasm = await createBuildFixture({ writeWasm: false });
  await assert.rejects(
    createSandboxPackageServiceLoader({ buildDirectory: missingWasm.buildDirectory }).load(),
    hasCode("artifact_unavailable")
  );

  const badFactory = await createBuildFixture({ source: "export const notAFactory = true;\n" });
  await assert.rejects(
    createSandboxPackageServiceLoader({ buildDirectory: badFactory.buildDirectory }).load(),
    hasCode("module_invalid")
  );

  const badAbiSource = generatedModuleSource("__tgd_loader_bad_abi", { abi: 0x00020000 });
  const badAbi = await createBuildFixture({ source: badAbiSource });
  await assert.rejects(
    createSandboxPackageServiceLoader({ buildDirectory: badAbi.buildDirectory }).load(),
    hasCode("module_invalid")
  );

  const failedKey = "__tgd_loader_failed";
  const failedSource = generatedModuleSource(failedKey, { initialize: false });
  const failed = await createBuildFixture({ source: failedSource });
  const failedLoader = createSandboxPackageServiceLoader({ buildDirectory: failed.buildDirectory });
  await assert.rejects(failedLoader.load(), hasCode("module_initialization_failed"));
  await assert.rejects(failedLoader.load(), hasCode("duplicate_load"));
  globalThis[`${failedKey}_ready`] = true;
  const recovered = await createSandboxPackageServiceLoader({
    buildDirectory: failed.buildDirectory
  }).load();
  assert.deepEqual(recovered.identity(), { generation: 0, checksum: Array(32).fill(0) });
  recovered.close();
});

test("duplicate loaders fail without disturbing the first loaded provider", async () => {
  const fixture = await createBuildFixture();
  const firstLoader = createSandboxPackageServiceLoader({ buildDirectory: fixture.buildDirectory });
  const firstLoad = firstLoader.load();
  await assert.rejects(firstLoader.load(), hasCode("duplicate_load"));
  const service = await firstLoad;
  const secondLoader = createSandboxPackageServiceLoader({ buildDirectory: fixture.buildDirectory });
  await assert.rejects(secondLoader.load(), hasCode("duplicate_load"));
  assert.equal(service.identity().generation, 0);
  assert.equal(service.compileAndPublish(validRuntime()).identity.generation, 1);
  assert.equal(globalThis[fixture.counterKey].factory, 1);
  service.close();
});

const generatedBuildDirectory = process.env.TGD_SANDBOX_SERVICE_BUILD_DIRECTORY;
test("a successfully delivered module remains one-shot after close", async () => {
  const fixture = await createBuildFixture();
  const service = await createSandboxPackageServiceLoader({
    buildDirectory: fixture.buildDirectory
  }).load();
  service.close();
  await assert.rejects(
    createSandboxPackageServiceLoader({ buildDirectory: fixture.buildDirectory }).load(),
    hasCode("duplicate_load")
  );
});

test("repository generated-WASM module loads through the production loader", {
  skip: generatedBuildDirectory ? false :
    "set TGD_SANDBOX_SERVICE_BUILD_DIRECTORY to a repository-relative CMake binary directory"
}, async () => {
  const loader = createSandboxPackageServiceLoader({
    buildDirectory: generatedBuildDirectory
  });
  const service = await loader.load();
  assert.equal(typeof service.identity().generation, "number");
  const result = service.compileAndPublish(validRuntime());
  assert.equal(result.complete, true);
  assert.equal(result.outcome, 1);
  assert.ok(result.packageBytes instanceof Uint8Array);
  assert.ok(result.packageBytes.length > 0);
  service.close();
});
