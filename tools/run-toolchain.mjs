import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve, delimiter } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

const modulePath = fileURLToPath(import.meta.url);
const defaultRoot = resolve(dirname(modulePath), "..");

function normalizedEnvironment(source) {
  const environment = {};
  const names = new Map();
  for (const [rawName, value] of Object.entries(source)) {
    const folded = rawName.toLowerCase();
    if (names.has(folded)) continue;
    const name = folded === "path" ? "PATH" : rawName;
    names.set(folded, name);
    environment[name] = value;
  }
  return environment;
}

function setEnvironmentValue(environment, name, value) {
  for (const existing of Object.keys(environment)) {
    if (existing.toLowerCase() === name.toLowerCase() && existing !== name) {
      delete environment[existing];
    }
  }
  environment[name] = value;
}

function prependPath(environment, additions) {
  const current = environment.PATH?.split(delimiter).filter(Boolean) ?? [];
  const seen = new Set();
  const combined = [];
  for (const path of [...additions, ...current]) {
    const folded = path.toLowerCase();
    if (seen.has(folded)) continue;
    seen.add(folded);
    combined.push(path);
  }
  setEnvironmentValue(environment, "PATH", combined.join(delimiter));
}

function absoluteToolPath(root, entry, relativeExecutable = entry.executable) {
  if (!entry.installRoot || !relativeExecutable) return null;
  return resolve(root, entry.installRoot, relativeExecutable);
}

export function buildToolchainEnvironment(root, lock, source = process.env) {
  const environment = normalizedEnvironment(source);
  const emsdkRoot = resolve(root, lock.supportArtifacts.emsdkManager.installRoot);
  const emscriptenRoot = resolve(root, lock.tools.emscripten.installRoot);
  const nodeExecutable = absoluteToolPath(root, lock.tools.node);
  const pythonExecutable = absoluteToolPath(root, lock.tools.python);
  const axmolRoot = resolve(root, lock.tools.axmol.installRoot);

  setEnvironmentValue(environment, "EMSDK", emsdkRoot.replaceAll("\\", "/"));
  setEnvironmentValue(environment, "EM_CONFIG", join(emsdkRoot, ".emscripten"));
  setEnvironmentValue(environment, "EMSDK_NODE", nodeExecutable);
  setEnvironmentValue(environment, "EMSDK_PYTHON", pythonExecutable);
  setEnvironmentValue(environment, "AX_ROOT", axmolRoot.replaceAll("\\", "/"));
  setEnvironmentValue(environment, "EMSDK_KEEP_DOWNLOADS", "1");

  const pathAdditions = [
    dirname(absoluteToolPath(root, lock.tools.cmake)),
    dirname(absoluteToolPath(root, lock.tools.ninja)),
    lock.tools.clang.installRoot ? dirname(absoluteToolPath(root, lock.tools.clang)) : null,
    emsdkRoot,
    join(emscriptenRoot, "emscripten"),
    join(emscriptenRoot, "bin"),
    dirname(nodeExecutable),
    dirname(pythonExecutable)
  ].filter(Boolean);
  prependPath(environment, pathAdditions);
  return environment;
}

function mergeEnvironment(base, overlay) {
  const merged = normalizedEnvironment(base);
  for (const [name, value] of Object.entries(overlay)) {
    setEnvironmentValue(merged, name, value);
  }
  return merged;
}

export function lockedMsvcToolsetVersion(entry) {
  const locator = entry?.executable ?? "";
  if (!locator.toLowerCase().startsWith("vswhere:")) {
    throw new Error("MSVC executable must use the vswhere: locator.");
  }
  const match = locator.slice("vswhere:".length).match(/(?:^|[\\/])VC[\\/]Tools[\\/]MSVC[\\/]([^\\/]+)(?:[\\/]|$)/i);
  if (!match || !/^\d+\.\d+\.\d+$/.test(match[1])) {
    throw new Error(`MSVC locator does not contain an exact toolset directory: ${locator}`);
  }
  return match[1];
}

function findVisualStudioEnvironment(root, environment, msvcEntry) {
  const programFilesX86 = environment["ProgramFiles(x86)"] ?? "C:\\Program Files (x86)";
  const vswhere = join(programFilesX86, "Microsoft Visual Studio", "Installer", "vswhere.exe");
  if (!existsSync(vswhere)) throw new Error(`vswhere not found: ${vswhere}`);

  const discovery = spawnSync(
    vswhere,
    [
      "-latest",
      "-products",
      "*",
      "-requires",
      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
      "-property",
      "installationPath"
    ],
    { encoding: "utf8", env: environment, windowsHide: true }
  );
  if (discovery.error) throw discovery.error;
  if (discovery.status !== 0 || !discovery.stdout?.trim()) {
    throw new Error(`Visual Studio C++ toolset discovery failed: ${discovery.stderr?.trim() ?? "no diagnostic"}`);
  }

  const vsDevCmd = join(discovery.stdout.trim(), "Common7", "Tools", "VsDevCmd.bat");
  const toolsetVersion = lockedMsvcToolsetVersion(msvcEntry);
  const commandFile = resolve(root, ".cache", "tgd-vsdev-env.cmd");
  mkdirSync(dirname(commandFile), { recursive: true });
  writeFileSync(
    commandFile,
    `@echo off\r\ncall "${vsDevCmd}" -no_logo -arch=x64 -host_arch=x64 -vcvars_ver=${toolsetVersion} >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\nset\r\n`,
    "utf8"
  );
  const capture = spawnSync(
    environment.ComSpec ?? "C:\\Windows\\System32\\cmd.exe",
    ["/d", "/q", "/c", basename(commandFile)],
    { cwd: dirname(commandFile), encoding: "utf8", env: environment, windowsHide: true }
  );
  if (capture.error) throw capture.error;
  if (capture.status !== 0) {
    throw new Error(`VsDevCmd failed: ${capture.stderr?.trim() ?? "no diagnostic"}`);
  }

  const visualStudioEnvironment = {};
  for (const line of capture.stdout.split(/\r?\n/)) {
    const separator = line.indexOf("=");
    if (separator <= 0) continue;
    visualStudioEnvironment[line.slice(0, separator)] = line.slice(separator + 1);
  }
  const selectedToolset = visualStudioEnvironment.VCToolsVersion?.replace(/[\\/]+$/, "");
  if (selectedToolset !== toolsetVersion) {
    throw new Error(`VsDevCmd selected MSVC ${selectedToolset ?? "unknown"}; expected ${toolsetVersion}.`);
  }
  return visualStudioEnvironment;
}

export function resolveToolInvocation(root, lock, tool, arguments_) {
  const direct = (key) => {
    const entry = lock.tools[key];
    if (!entry?.artifactVerified) throw new Error(`${key} artifact is not verified yet.`);
    const executable = absoluteToolPath(root, entry);
    if (!executable || !existsSync(executable)) throw new Error(`${key} executable not found: ${executable}`);
    return { executable, arguments: arguments_ };
  };

  if (tool === "cmake") return direct("cmake");
  if (tool === "ninja") return direct("ninja");
  if (tool === "clang-cl") return direct("clang");
  if (tool === "node") return direct("node");
  if (tool === "python") return direct("python");
  if (tool === "wasm-clang") {
    const entry = lock.tools.emscripten;
    if (!entry.artifactVerified) throw new Error("emscripten artifact is not verified yet.");
    const executable = resolve(root, entry.installRoot, "bin", "clang.exe");
    if (!existsSync(executable)) throw new Error(`wasm clang executable not found: ${executable}`);
    return { executable, arguments: arguments_ };
  }
  if (tool === "emcc" || tool === "emcmake") {
    const entry = lock.tools.emscripten;
    if (!entry.artifactVerified) throw new Error("emscripten artifact is not verified yet.");
    const pythonExecutable = absoluteToolPath(root, lock.tools.python);
    const script = resolve(root, entry.installRoot, "emscripten", `${tool}.py`);
    if (!existsSync(script)) throw new Error(`${tool} script not found: ${script}`);
    return { executable: pythonExecutable, arguments: [script, ...arguments_] };
  }

  throw new Error(`Unknown locked tool '${tool}'.`);
}

async function main() {
  const rootArgument = process.argv.find((argument) => argument.startsWith("--root="));
  const root = resolve(rootArgument?.slice(7) ?? defaultRoot);
  const positional = process.argv.slice(2).filter((argument) => argument !== rootArgument);
  const useVsDev = positional.includes("--vsdev") || positional.some((argument) => argument.includes("windows-clang"));
  const filtered = positional.filter((argument) => argument !== "--vsdev");
  const [tool, ...arguments_] = filtered;
  if (!tool) throw new Error("Usage: node tools/run-toolchain.mjs [--vsdev] <tool> [...args]");

  const lock = JSON.parse(readFileSync(resolve(root, "toolchains", "toolchain-lock.json"), "utf8"));
  let environment = buildToolchainEnvironment(root, lock);
  if (useVsDev) {
    environment = mergeEnvironment(environment, findVisualStudioEnvironment(root, environment, lock.tools.msvc));
    environment = buildToolchainEnvironment(root, lock, environment);
  }

  const invocation = resolveToolInvocation(root, lock, tool, arguments_);
  const result = spawnSync(invocation.executable, invocation.arguments, {
    cwd: root,
    env: environment,
    stdio: "inherit",
    windowsHide: true
  });
  if (result.error) throw result.error;
  process.exitCode = result.status ?? 1;
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? "").href) {
  await main();
}
