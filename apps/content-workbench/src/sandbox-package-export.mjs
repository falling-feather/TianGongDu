import { createHash } from "node:crypto";

const FALLBACK_FILENAME = "sandbox-package.tgdsbx";
const SAFE_STEM = /^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$/;
const WINDOWS_RESERVED = /^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i;
const PACKAGE_SHA256 = /^sha256:[0-9a-f]{64}$/;

function presentationFilename(relativePath) {
  if (typeof relativePath !== "string") {
    return FALLBACK_FILENAME;
  }
  const leaf = relativePath.replaceAll("\\", "/").split("/").at(-1);
  if (typeof leaf !== "string" || !leaf.toLowerCase().endsWith(".json")) {
    return FALLBACK_FILENAME;
  }
  const stem = leaf.slice(0, -5);
  if (
    !SAFE_STEM.test(stem) ||
    stem.endsWith(".") ||
    WINDOWS_RESERVED.test(stem)
  ) {
    return FALLBACK_FILENAME;
  }
  return stem + ".tgdsbx";
}

function sha256(bytes) {
  return "sha256:" + createHash("sha256").update(bytes).digest("hex");
}

export function createSandboxPackageExport({
  relativePath,
  packageBytes,
  packageSha256
}) {
  if (!(packageBytes instanceof Uint8Array) || packageBytes.byteLength === 0) {
    throw new Error("prepared Sandbox package bytes are unavailable");
  }
  if (
    typeof packageSha256 !== "string" ||
    !PACKAGE_SHA256.test(packageSha256)
  ) {
    throw new Error("prepared Sandbox package SHA-256 is invalid");
  }
  const bytes = new Uint8Array(packageBytes);
  if (sha256(bytes) !== packageSha256) {
    throw new Error("prepared Sandbox package bytes no longer match");
  }
  return Object.freeze({
    filename: presentationFilename(relativePath),
    bytes,
    byteLength: bytes.byteLength,
    packageSha256
  });
}
