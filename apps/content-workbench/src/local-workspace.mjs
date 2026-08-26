import { createHash, randomUUID } from "node:crypto";
import { promises as fileSystem } from "node:fs";
import path from "node:path";

const MAX_DOCUMENT_BYTES = 2 * 1024 * 1024;
const WINDOWS_DEVICE_NAME = /^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i;
const utf8Decoder = new TextDecoder("utf-8", { fatal: true });

export class LocalWorkspaceError extends Error {
  constructor(code, message, status = 400) {
    super(message);
    this.name = "LocalWorkspaceError";
    this.code = code;
    this.status = status;
  }
}

function fail(code, message, status = 400) {
  throw new LocalWorkspaceError(code, message, status);
}

function wrapIoError(error, context) {
  if (error instanceof LocalWorkspaceError) {
    return error;
  }
  if (error?.code === "ENOENT") {
    return new LocalWorkspaceError("not_found", context + ": file not found", 404);
  }
  if (error?.code === "EACCES" || error?.code === "EPERM") {
    return new LocalWorkspaceError("access_denied", context + ": access denied", 403);
  }
  return new LocalWorkspaceError(
    "io_error",
    context + ": " + (error?.message ?? "I/O failure"),
    500
  );
}

function isInside(root, candidate) {
  const relative = path.relative(root, candidate);
  return (
    relative === "" ||
    (relative !== ".." &&
      !relative.startsWith(".." + path.sep) &&
      !path.isAbsolute(relative))
  );
}

function normalizeRelativeJsonPath(value) {
  if (typeof value !== "string" || value.length === 0) {
    fail("invalid_path", "document path must be a non-empty string");
  }
  if (value.includes("\0")) {
    fail("invalid_path", "document path contains a null character");
  }
  if (
    path.win32.isAbsolute(value) ||
    path.posix.isAbsolute(value) ||
    /^[a-zA-Z]:/.test(value) ||
    /^[\\/]{2}/.test(value)
  ) {
    fail("path_escape", "document path must be relative to the workspace");
  }

  const slashPath = value.replaceAll("\\", "/");
  const segments = slashPath.split("/");
  if (
    segments.some(
      (segment) =>
        segment === "" ||
        segment === "." ||
        segment === ".." ||
        segment.includes(":") ||
        /[. ]$/.test(segment) ||
        WINDOWS_DEVICE_NAME.test(segment.split(".")[0])
    )
  ) {
    fail("path_escape", "document path is not canonical or is unsafe");
  }
  if (path.posix.extname(segments.at(-1)).toLowerCase() !== ".json") {
    fail("invalid_path", "document path must end in .json");
  }
  return segments.join("/");
}

function contentCas(bytes) {
  return "sha256:" + createHash("sha256").update(bytes).digest("base64url");
}

function ensureDocumentSize(byteLength) {
  if (byteLength > MAX_DOCUMENT_BYTES) {
    fail(
      "document_too_large",
      "authoring document exceeds " + MAX_DOCUMENT_BYTES + " bytes",
      413
    );
  }
}

export async function createLocalWorkspace({
  rootPath,
  faultInjector = null
}) {
  if (typeof rootPath !== "string" || rootPath.length === 0) {
    fail("invalid_workspace", "workspace root must be a non-empty string");
  }
  if (faultInjector !== null && typeof faultInjector !== "function") {
    fail("invalid_workspace", "faultInjector must be a function");
  }

  const absoluteRoot = path.resolve(rootPath);
  let realRoot;
  try {
    const rootStat = await fileSystem.stat(absoluteRoot);
    if (!rootStat.isDirectory()) {
      fail("invalid_workspace", "workspace root must be a directory");
    }
    realRoot = await fileSystem.realpath(absoluteRoot);
  } catch (error) {
    throw wrapIoError(error, "open workspace root");
  }

  const locks = new Map();

  async function checkpoint(name, details) {
    if (faultInjector) {
      await faultInjector(name, details);
    }
  }

  async function resolveExistingTarget(relativePath) {
    const canonicalPath = normalizeRelativeJsonPath(relativePath);
    const absolutePath = path.resolve(
      absoluteRoot,
      ...canonicalPath.split("/")
    );
    if (!isInside(absoluteRoot, absolutePath)) {
      fail("path_escape", "document path escapes the workspace");
    }

    try {
      const linkStat = await fileSystem.lstat(absolutePath);
      if (linkStat.isSymbolicLink()) {
        fail("path_escape", "symbolic-link documents are not supported");
      }
      const realPath = await fileSystem.realpath(absolutePath);
      if (!isInside(realRoot, realPath)) {
        fail("path_escape", "resolved document path escapes the workspace");
      }
      const stat = await fileSystem.stat(absolutePath);
      if (!stat.isFile()) {
        fail("invalid_path", "document path must identify a regular file");
      }
      ensureDocumentSize(stat.size);
      return { canonicalPath, absolutePath, realPath };
    } catch (error) {
      throw wrapIoError(error, "resolve document path");
    }
  }

  async function withTargetLock(key, action) {
    const previous = locks.get(key) ?? Promise.resolve();
    let release;
    const gate = new Promise((resolve) => {
      release = resolve;
    });
    const tail = previous.then(() => gate);
    locks.set(key, tail);
    await previous;
    try {
      return await action();
    } finally {
      release();
      if (locks.get(key) === tail) {
        locks.delete(key);
      }
    }
  }

  async function read(relativePath) {
    const target = await resolveExistingTarget(relativePath);
    try {
      const bytes = await fileSystem.readFile(target.absolutePath);
      ensureDocumentSize(bytes.byteLength);
      const verified = await resolveExistingTarget(target.canonicalPath);
      if (verified.realPath !== target.realPath) {
        fail("external_change", "document changed while it was being opened", 409);
      }
      let text;
      try {
        text = utf8Decoder.decode(bytes);
      } catch {
        fail("invalid_encoding", "authoring document must be valid UTF-8");
      }
      return Object.freeze({
        relativePath: target.canonicalPath,
        text,
        cas: contentCas(bytes)
      });
    } catch (error) {
      throw wrapIoError(error, "read document");
    }
  }

  async function save({ relativePath, expectedCas, text }) {
    const canonicalPath = normalizeRelativeJsonPath(relativePath);
    if (typeof expectedCas !== "string" || expectedCas.length === 0) {
      fail("invalid_cas", "expectedCas must be a non-empty string");
    }
    if (typeof text !== "string") {
      fail("invalid_document", "serialized document must be a string");
    }
    const outputBytes = Buffer.from(text, "utf8");
    ensureDocumentSize(outputBytes.byteLength);

    return withTargetLock(canonicalPath, async () => {
      const initialTarget = await resolveExistingTarget(canonicalPath);
      let currentBytes;
      try {
        currentBytes = await fileSystem.readFile(initialTarget.absolutePath);
      } catch (error) {
        throw wrapIoError(error, "read document before save");
      }
      if (contentCas(currentBytes) !== expectedCas) {
        fail("external_change", "document CAS no longer matches", 409);
      }

      const temporaryPath = path.join(
        path.dirname(initialTarget.absolutePath),
        "." +
          path.basename(initialTarget.absolutePath) +
          ".tgdtmp-" +
          process.pid +
          "-" +
          randomUUID()
      );
      let handle = null;
      let temporaryExists = false;

      try {
        handle = await fileSystem.open(temporaryPath, "wx", 0o600);
        temporaryExists = true;
        await checkpoint("write", {
          relativePath: canonicalPath,
          temporaryPath
        });
        await handle.writeFile(outputBytes);
        await checkpoint("sync", {
          relativePath: canonicalPath,
          temporaryPath
        });
        await handle.sync();
        await handle.close();
        handle = null;

        const replaceTarget = await resolveExistingTarget(canonicalPath);
        const replaceBytes = await fileSystem.readFile(replaceTarget.absolutePath);
        if (contentCas(replaceBytes) !== expectedCas) {
          fail("external_change", "document changed before replacement", 409);
        }

        await checkpoint("replace", {
          relativePath: canonicalPath,
          temporaryPath
        });
        await fileSystem.rename(temporaryPath, replaceTarget.absolutePath);
        temporaryExists = false;
        return Object.freeze({
          relativePath: canonicalPath,
          cas: contentCas(outputBytes)
        });
      } catch (error) {
        if (handle) {
          try {
            await handle.close();
          } catch {
            // The original failure remains authoritative.
          }
        }
        if (temporaryExists) {
          try {
            await fileSystem.unlink(temporaryPath);
          } catch {
            // Best-effort cleanup; never mask the save failure.
          }
        }
        throw wrapIoError(error, "save document");
      }
    });
  }

  return Object.freeze({
    rootPath: realRoot,
    read,
    save
  });
}
