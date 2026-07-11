import { createHash } from "node:crypto";
import { createReadStream } from "node:fs";
import { access, readFile, stat } from "node:fs/promises";
import { dirname, isAbsolute, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const modulePath = fileURLToPath(import.meta.url);
const defaultRoot = resolve(dirname(modulePath), "..");
const requiredTools = [
  "axmol",
  "emscripten",
  "cmake",
  "ninja",
  "clang",
  "msvc",
  "node",
  "python"
];
const approvedSourceHosts = new Set([
  "github.com",
  "releases.llvm.org",
  "storage.googleapis.com"
]);
const sha256Pattern = /^sha256:([0-9a-f]{64})$/;
const pendingPattern = /^pending:[a-z0-9-]+$/;

function pushIf(errors, condition, message) {
  if (condition) errors.push(message);
}

function isNonEmptyString(value) {
  return typeof value === "string" && value.length > 0;
}

function isWorkspaceRelative(path) {
  return isNonEmptyString(path) && !isAbsolute(path) && !path.split(/[\\/]/).includes("..");
}

function validateEntry(name, entry, errors) {
  pushIf(errors, !entry || typeof entry !== "object", `缺少工具链条目：${name}`);
  if (!entry || typeof entry !== "object") return;

  pushIf(errors, !isNonEmptyString(entry.version), `${name} 缺少精确 version。`);
  pushIf(errors, /latest/i.test(entry.version ?? ""), `${name} 禁止使用 latest。`);
  const hasSha = sha256Pattern.test(entry.integrity ?? "");
  const hasPending = pendingPattern.test(entry.integrity ?? "");
  pushIf(errors, !hasSha && !hasPending, `${name} integrity 必须为 sha256 或显式 pending。`);
  pushIf(errors, entry.artifactVerified === true && !hasSha, `${name} 已标记产物验证，但没有 SHA-256。`);
  pushIf(errors, entry.artifactVerified === true && entry.integrityScope === "pending", `${name} 已标记产物验证，但完整性作用域仍为 pending。`);

  const provenance = entry.provenance;
  pushIf(errors, !provenance || typeof provenance !== "object", `${name} 缺少 provenance。`);
  if (provenance?.url !== null && provenance?.url !== undefined) {
    try {
      const source = new URL(provenance.url);
      pushIf(errors, source.protocol !== "https:", `${name} 来源必须使用 HTTPS。`);
      pushIf(errors, !approvedSourceHosts.has(source.hostname), `${name} 来源主机不在官方白名单：${source.hostname}`);
    } catch {
      errors.push(`${name} 来源 URL 无效。`);
    }
  }
  pushIf(errors, provenance?.kind !== "bundled-by-parent" && provenance?.kind !== "installed-component" && !isNonEmptyString(provenance?.url), `${name} 官方产物缺少 URL。`);
  pushIf(errors, provenance?.kind === "bundled-by-parent" && !isNonEmptyString(provenance?.parent), `${name} bundled-by-parent 缺少 parent。`);

  for (const [field, path] of [["cachePath", entry.cachePath], ["installRoot", entry.installRoot]]) {
    if (path !== null && path !== undefined) {
      pushIf(errors, !isWorkspaceRelative(path), `${name}.${field} 必须是工作区内相对路径。`);
    }
  }
}

export function validateToolchainLock(lock, baseline) {
  const errors = [];
  pushIf(errors, lock?.schemaVersion !== "1.0.0", "工具链锁 schemaVersion 必须为 1.0.0。");
  pushIf(errors, !["candidate-being-validated", "locked-and-validated"].includes(lock?.status), "工具链锁状态非法。");
  pushIf(errors, lock?.target !== "windows-x86_64-web-single", "工具链锁目标必须为 windows-x86_64-web-single。");
  pushIf(errors, lock?.policy?.officialSourcesOnly !== true, "工具链必须只使用官方来源。");
  pushIf(errors, lock?.policy?.exactVersionsOnly !== true, "工具链必须锁定精确版本。");
  pushIf(errors, lock?.policy?.persistentSystemMutation !== "forbidden", "工具链恢复不得持久修改系统环境。");
  pushIf(errors, !Array.isArray(lock?.policy?.selectionNotes) || lock.policy.selectionNotes.length < 2, "工具链锁缺少版本选择证据。");
  pushIf(errors, !Array.isArray(lock?.policy?.promotionRequirements) || lock.policy.promotionRequirements.length < 5, "工具链锁缺少 G1 提升条件。");

  for (const tool of requiredTools) validateEntry(tool, lock?.tools?.[tool], errors);
  for (const [name, entry] of Object.entries(lock?.supportArtifacts ?? {})) validateEntry(`supportArtifacts.${name}`, entry, errors);

  const baselineStatus = baseline?.toolchainLock?.status;
  const manifestPath = baseline?.toolchainLock?.manifestPath;
  pushIf(errors, manifestPath !== "toolchains/toolchain-lock.json", "技术基线的工具链锁路径不一致。");
  pushIf(errors, baselineStatus === "locked-and-validated" && lock?.status !== "locked-and-validated", "技术基线已标记锁定，但锁文件尚未通过 G1。");
  pushIf(errors, baselineStatus === "planned-not-locked" && lock?.status === "locked-and-validated", "锁文件已通过 G1，但技术基线仍标记为 planned-not-locked。");

  if (lock?.status === "locked-and-validated") {
    pushIf(errors, !isNonEmptyString(lock.g1EvidenceBuildId), "已锁定工具链缺少 G1 evidence build ID。");
    for (const tool of requiredTools) {
      pushIf(errors, lock?.tools?.[tool]?.artifactVerified !== true, `${tool} 尚未完成产物验证，不能提升为 locked-and-validated。`);
      pushIf(errors, !sha256Pattern.test(lock?.tools?.[tool]?.integrity ?? ""), `${tool} 尚未锁定 SHA-256。`);
    }
  } else {
    pushIf(errors, lock?.g1EvidenceBuildId !== null, "候选工具链不得预填 G1 evidence build ID。");
  }

  return errors;
}

async function sha256(path) {
  const hash = createHash("sha256");
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  return hash.digest("hex");
}

async function verifyCachedEntry(root, name, entry, errors, notes) {
  if (!entry.cachePath) return;
  const absolutePath = resolve(root, entry.cachePath);
  const relativePath = relative(root, absolutePath);
  if (relativePath.startsWith("..") || isAbsolute(relativePath)) {
    errors.push(`${name} 缓存路径越出工作区。`);
    return;
  }

  try {
    await access(absolutePath);
  } catch {
    const message = `${name} 缓存尚未获取：${entry.cachePath}`;
    if (entry.artifactVerified) errors.push(message);
    else notes.push(message);
    return;
  }

  const file = await stat(absolutePath);
  if (Number.isInteger(entry.provenance?.expectedSizeBytes) && file.size !== entry.provenance.expectedSizeBytes) {
    errors.push(`${name} 缓存大小不符：期望 ${entry.provenance.expectedSizeBytes}，实际 ${file.size}。`);
  }
  const match = sha256Pattern.exec(entry.integrity ?? "");
  if (!match) {
    notes.push(`${name} 缓存存在，但 SHA-256 仍待锁定。`);
    return;
  }
  const actual = await sha256(absolutePath);
  pushIf(errors, actual !== match[1], `${name} SHA-256 不符：期望 ${match[1]}，实际 ${actual}。`);
  if (actual === match[1]) notes.push(`${name} SHA-256 已验证。`);
}

export async function verifyToolchainCache(root, lock) {
  const errors = [];
  const notes = [];
  for (const [name, entry] of Object.entries(lock.tools ?? {})) {
    await verifyCachedEntry(root, name, entry, errors, notes);
  }
  for (const [name, entry] of Object.entries(lock.supportArtifacts ?? {})) {
    await verifyCachedEntry(root, `supportArtifacts.${name}`, entry, errors, notes);
  }
  return { errors, notes };
}

async function main() {
  const root = resolve(process.argv.find((argument) => argument.startsWith("--root="))?.slice(7) ?? defaultRoot);
  const lock = JSON.parse(await readFile(resolve(root, "toolchains/toolchain-lock.json"), "utf8"));
  const baseline = JSON.parse(await readFile(resolve(root, "content/design/technical-baseline.json"), "utf8"));
  const errors = validateToolchainLock(lock, baseline);
  const notes = [];
  if (process.argv.includes("--cache")) {
    const cacheResult = await verifyToolchainCache(root, lock);
    errors.push(...cacheResult.errors);
    notes.push(...cacheResult.notes);
  }

  if (errors.length > 0) {
    console.error("Toolchain lock validation failed:");
    for (const error of errors) console.error(`- ${error}`);
    process.exitCode = 1;
    return;
  }

  console.log(`Toolchain lock valid: ${lock.status}`);
  for (const note of notes) console.log(`- ${note}`);
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? "").href) {
  await main();
}
