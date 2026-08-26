import { lstat, realpath } from "node:fs/promises";
import { dirname, isAbsolute, join, relative, resolve, sep, win32 } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { SandboxPackageServiceClient } from "./sandbox-package-service-client.mjs";

const GENERATED_MODULE_NAME = "tgd-sandbox-package-service-abi.mjs";
const GENERATED_WASM_NAME = "tgd-sandbox-package-service-abi.wasm";
const REPOSITORY_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const claimedModules = new Set();

const messages = Object.freeze({
  invalid_options: "Sandbox compiler build directory is invalid",
  artifact_unavailable: "Sandbox compiler generated artifacts are unavailable",
  duplicate_load: "Sandbox compiler module has already been loaded",
  module_invalid: "Sandbox compiler module contract is invalid",
  module_initialization_failed: "Sandbox compiler module initialization failed",
  service_closed: "Sandbox compiler service is closed"
});

export class SandboxPackageServiceLoaderError extends Error {
  constructor(code) {
    super(messages[code] ?? messages.module_initialization_failed);
    this.name = "SandboxPackageServiceLoaderError";
    this.code = code;
  }
}

function fail(code) {
  throw new SandboxPackageServiceLoaderError(code);
}

function expectOptions(value) {
  if (value === null || typeof value !== "object" || Array.isArray(value) ||
      (Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)) {
    fail("invalid_options");
  }
  const keys = Reflect.ownKeys(value);
  if (keys.length !== 1 || keys[0] !== "buildDirectory") fail("invalid_options");
  const descriptor = Object.getOwnPropertyDescriptor(value, "buildDirectory");
  if (!descriptor || !("value" in descriptor) || descriptor.enumerable !== true) {
    fail("invalid_options");
  }
  return descriptor.value;
}

function buildSegments(value) {
  if (typeof value !== "string" || value.length === 0 || value.includes("\0") ||
      isAbsolute(value) || win32.isAbsolute(value)) {
    fail("invalid_options");
  }
  const normalized = value.replaceAll("\\", "/");
  if (normalized.startsWith("/") || normalized.startsWith("//")) fail("invalid_options");
  const segments = normalized.split("/");
  if (segments.some((segment) => segment.length === 0 || segment === "." || segment === ".." ||
      segment.includes(":"))) {
    fail("invalid_options");
  }
  return Object.freeze(segments);
}

function isStrictDescendant(root, candidate) {
  const value = relative(root, candidate);
  return value.length > 0 && value !== ".." && !value.startsWith(".." + sep) &&
    !isAbsolute(value);
}

async function resolveArtifacts(segments) {
  let repositoryRoot;
  let buildRoot;
  let artifactRoot;
  try {
    repositoryRoot = await realpath(REPOSITORY_ROOT);
    buildRoot = await realpath(join(repositoryRoot, ...segments));
    if (!isStrictDescendant(repositoryRoot, buildRoot)) fail("artifact_unavailable");
    artifactRoot = await realpath(join(buildRoot, "dist", "web"));
    if (!isStrictDescendant(buildRoot, artifactRoot)) fail("artifact_unavailable");
  } catch (error) {
    if (error instanceof SandboxPackageServiceLoaderError) throw error;
    fail("artifact_unavailable");
  }

  const resolveRegularArtifact = async (name) => {
    const lexicalPath = join(buildRoot, "dist", "web", name);
    try {
      const information = await lstat(lexicalPath);
      if (!information.isFile() || information.isSymbolicLink()) fail("artifact_unavailable");
      const actualPath = await realpath(lexicalPath);
      if (dirname(actualPath) !== artifactRoot || !isStrictDescendant(buildRoot, actualPath)) {
        fail("artifact_unavailable");
      }
      return actualPath;
    } catch (error) {
      if (error instanceof SandboxPackageServiceLoaderError) throw error;
      fail("artifact_unavailable");
    }
  };

  return Object.freeze({
    modulePath: await resolveRegularArtifact(GENERATED_MODULE_NAME),
    wasmPath: await resolveRegularArtifact(GENERATED_WASM_NAME)
  });
}

function claimKey(modulePath) {
  return process.platform === "win32" ? modulePath.toLowerCase() : modulePath;
}

class LoadedSandboxPackageService {
  #client;
  #closed = false;

  constructor(client) {
    this.#client = client;
    Object.freeze(this);
  }

  identity() {
    if (this.#closed) fail("service_closed");
    return this.#client.identity();
  }

  compileAndPublish(runtime, expectedIdentity) {
    if (this.#closed) fail("service_closed");
    return arguments.length === 1
      ? this.#client.publish(runtime)
      : this.#client.publish(runtime, expectedIdentity);
  }

  close() {
    if (this.#closed) fail("service_closed");
    this.#closed = true;
    this.#client.destroy();
  }
}

class SandboxPackageServiceLoader {
  #segments;
  #state = "idle";

  constructor(segments) {
    this.#segments = segments;
    Object.freeze(this);
  }

  async load() {
    if (this.#state !== "idle") fail("duplicate_load");
    this.#state = "loading";
    let claimedKey = null;
    try {
      const artifacts = await resolveArtifacts(this.#segments);
      const key = claimKey(artifacts.modulePath);
      if (claimedModules.has(key)) fail("duplicate_load");
      claimedModules.add(key);
      claimedKey = key;

      let namespace;
      try {
        namespace = await import(pathToFileURL(artifacts.modulePath).href);
      } catch {
        fail("module_initialization_failed");
      }
      if (typeof namespace.default !== "function") fail("module_invalid");

      let module;
      try {
        module = await namespace.default({
          noInitialRun: true,
          locateFile(name) {
            if (name !== GENERATED_WASM_NAME) fail("module_invalid");
            return artifacts.wasmPath;
          }
        });
      } catch (error) {
        if (error instanceof SandboxPackageServiceLoaderError) throw error;
        fail("module_initialization_failed");
      }
      if (module === null || typeof module !== "object") fail("module_invalid");

      let client;
      try {
        client = SandboxPackageServiceClient.create(module);
      } catch {
        fail("module_invalid");
      }
      this.#state = "loaded";
      return new LoadedSandboxPackageService(client);
    } catch (error) {
      if (claimedKey !== null) claimedModules.delete(claimedKey);
      this.#state = "failed";
      if (error instanceof SandboxPackageServiceLoaderError) throw error;
      fail("module_initialization_failed");
    }
  }
}

export function createSandboxPackageServiceLoader(options) {
  return new SandboxPackageServiceLoader(buildSegments(expectOptions(options)));
}
