import { randomBytes } from "node:crypto";
import { readFile } from "node:fs/promises";
import http from "node:http";
import path from "node:path";
import { pathToFileURL } from "node:url";

import { createLocalWorkspace } from "./local-workspace.mjs";
import { createSandboxPackageServiceLoader } from "./sandbox-package-service-loader.mjs";
import { createWorkbenchController } from "./workbench-controller.mjs";

const MAX_REQUEST_BYTES = 2 * 1024 * 1024;
const LOOPBACK_HOST = "127.0.0.1";
const STATIC_FILES = new Map([
  ["/", [new URL("../public/index.html", import.meta.url), "text/html; charset=utf-8"]],
  [
    "/workbench.mjs",
    [new URL("../public/workbench.mjs", import.meta.url), "text/javascript; charset=utf-8"]
  ],
  [
    "/workbench.css",
    [new URL("../public/workbench.css", import.meta.url), "text/css; charset=utf-8"]
  ]
]);
const PREVIEW_STATIC_FILES = new Map([
  [
    "/preview/tiangongdu-system-demo.html",
    ["tiangongdu-system-demo.html", "text/html; charset=utf-8"]
  ],
  [
    "/preview/tiangongdu-system-demo.js",
    ["tiangongdu-system-demo.js", "text/javascript; charset=utf-8"]
  ],
  [
    "/preview/tiangongdu-system-demo.wasm",
    ["tiangongdu-system-demo.wasm", "application/wasm"]
  ]
]);

function securityHeaders(contentType) {
  return {
    "Cache-Control": "no-store",
    "Content-Type": contentType,
    "Content-Security-Policy":
      "default-src 'self'; script-src 'self'; style-src 'self'; " +
      "connect-src 'self'; img-src 'self' data:; object-src 'none'; " +
      "base-uri 'none'; frame-ancestors 'none'; form-action 'self'",
    "Referrer-Policy": "no-referrer",
    "X-Content-Type-Options": "nosniff",
    "X-Frame-Options": "DENY"
  };
}

function previewSecurityHeaders(contentType) {
  return {
    "Cache-Control": "no-store",
    "Content-Type": contentType,
    "Content-Security-Policy":
      "default-src 'self'; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; " +
      "style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; " +
      "object-src 'none'; base-uri 'none'; frame-ancestors 'self'; form-action 'self'",
    "Referrer-Policy": "no-referrer",
    "X-Content-Type-Options": "nosniff",
    "X-Frame-Options": "SAMEORIGIN"
  };
}

function writeBytes(response, status, bytes, contentType, extraHeaders = {}) {
  response.writeHead(status, {
    ...securityHeaders(contentType),
    "Content-Length": bytes.byteLength,
    ...extraHeaders
  });
  response.end(bytes);
}

function writeJson(response, status, value) {
  writeBytes(
    response,
    status,
    Buffer.from(JSON.stringify(value), "utf8"),
    "application/json; charset=utf-8"
  );
}

function writePreviewBytes(response, bytes, contentType) {
  response.writeHead(200, {
    ...previewSecurityHeaders(contentType),
    "Content-Length": bytes.byteLength
  });
  response.end(bytes);
}

async function readJson(request) {
  if (!request.headers["content-type"]?.startsWith("application/json")) {
    const error = new Error("request content type must be application/json");
    error.code = "invalid_request";
    error.status = 415;
    throw error;
  }
  const chunks = [];
  let total = 0;
  for await (const chunk of request) {
    total += chunk.byteLength;
    if (total > MAX_REQUEST_BYTES) {
      const error = new Error("request body is too large");
      error.code = "request_too_large";
      error.status = 413;
      throw error;
    }
    chunks.push(chunk);
  }
  try {
    return JSON.parse(Buffer.concat(chunks).toString("utf8"));
  } catch {
    const error = new Error("request body is not valid JSON");
    error.code = "invalid_request";
    error.status = 400;
    throw error;
  }
}

function hasSessionCookie(request, sessionToken) {
  const cookies = (request.headers.cookie ?? "")
    .split(";")
    .map((part) => part.trim());
  return cookies.includes("tgd_workbench_session=" + sessionToken);
}

function browserState(controller) {
  const {
    cas: privateCas,
    lastError: privateLastError,
    ...state
  } = controller.view();
  void privateCas;
  void privateLastError;
  return state;
}

function apiError(error, controller) {
  return {
    status: Number.isInteger(error?.status) ? error.status : 500,
    body: {
      error: {
        code: typeof error?.code === "string" ? error.code : "internal_error",
        message:
          typeof error?.code === "string"
            ? "Workbench request was not completed"
            : "Workbench request failed"
      },
      state: browserState(controller)
    }
  };
}

function expectBrowserRequest(value, keys) {
  if (
    value === null ||
    typeof value !== "object" ||
    Array.isArray(value) ||
    Object.getPrototypeOf(value) !== Object.prototype ||
    Reflect.ownKeys(value).length !== keys.length ||
    Reflect.ownKeys(value).some(
      (key) => typeof key !== "string" || !keys.includes(key)
    )
  ) {
    const error = new Error("invalid browser request");
    error.code = "invalid_request";
    error.status = 400;
    throw error;
  }
  return value;
}

async function loadCompilerService({
  sandboxService,
  sandboxBuildDirectory
}) {
  if (sandboxService !== null && sandboxBuildDirectory !== null) {
    throw new Error(
      "sandboxService and sandboxBuildDirectory are mutually exclusive"
    );
  }
  if (sandboxService !== null) {
    return sandboxService;
  }
  if (sandboxBuildDirectory === null) {
    return null;
  }
  try {
    return await createSandboxPackageServiceLoader({
      buildDirectory: sandboxBuildDirectory
    }).load();
  } catch {
    return null;
  }
}

async function resolveSystemDemoWebRoot({
  systemDemoWebRoot,
  sandboxBuildDirectory
}) {
  const candidate =
    systemDemoWebRoot ??
    (sandboxBuildDirectory === null
      ? null
      : path.resolve(sandboxBuildDirectory, "dist", "web"));
  if (candidate === null) {
    return null;
  }
  const resolved = path.resolve(candidate);
  try {
    await Promise.all(
      [...PREVIEW_STATIC_FILES.values()].map(([filename]) =>
        readFile(path.join(resolved, filename))
      )
    );
    return resolved;
  } catch {
    if (systemDemoWebRoot !== null) {
      throw new Error(
        "system Demo Web root does not contain the required host artifacts"
      );
    }
    return null;
  }
}

export async function startWorkbenchServer({
  workspaceRoot,
  faultInjector,
  sandboxService = null,
  sandboxBuildDirectory = null,
  systemDemoWebRoot = null
}) {
  const workspace = await createLocalWorkspace({
    rootPath: workspaceRoot,
    faultInjector
  });
  const compilerService = await loadCompilerService({
    sandboxService,
    sandboxBuildDirectory
  });
  const previewRoot = await resolveSystemDemoWebRoot({
    systemDemoWebRoot,
    sandboxBuildDirectory
  });
  const controller = createWorkbenchController({
    workspace,
    compilerService,
    previewAvailable: previewRoot !== null
  });
  const sessionToken = randomBytes(24).toString("base64url");
  let origin = null;

  const server = http.createServer((request, response) => {
    void (async () => {
      const url = new URL(request.url ?? "/", origin ?? "http://127.0.0.1");

      if (STATIC_FILES.has(url.pathname)) {
        if (request.method !== "GET" && request.method !== "HEAD") {
          writeJson(response, 405, { error: { code: "method_not_allowed" } });
          return;
        }
        const [fileUrl, contentType] = STATIC_FILES.get(url.pathname);
        const bytes = await readFile(fileUrl);
        const headers =
          url.pathname === "/"
            ? {
                "Set-Cookie":
                  "tgd_workbench_session=" +
                  sessionToken +
                  "; HttpOnly; SameSite=Strict; Path=/"
              }
            : {};
        if (request.method === "HEAD") {
          response.writeHead(200, {
            ...securityHeaders(contentType),
            "Content-Length": bytes.byteLength,
            ...headers
          });
          response.end();
          return;
        }
        writeBytes(response, 200, bytes, contentType, headers);
        return;
      }

      if (PREVIEW_STATIC_FILES.has(url.pathname) && previewRoot !== null) {
        if (request.method !== "GET" && request.method !== "HEAD") {
          writeJson(response, 405, { error: { code: "method_not_allowed" } });
          return;
        }
        if (!hasSessionCookie(request, sessionToken)) {
          writeJson(response, 403, { error: { code: "invalid_session" } });
          return;
        }
        const [filename, contentType] = PREVIEW_STATIC_FILES.get(url.pathname);
        const bytes = await readFile(path.join(previewRoot, filename));
        if (request.method === "HEAD") {
          response.writeHead(200, {
            ...previewSecurityHeaders(contentType),
            "Content-Length": bytes.byteLength
          });
          response.end();
          return;
        }
        writePreviewBytes(response, bytes, contentType);
        return;
      }

      if (!url.pathname.startsWith("/api/")) {
        writeJson(response, 404, { error: { code: "not_found" } });
        return;
      }
      if (!hasSessionCookie(request, sessionToken)) {
        writeJson(response, 403, { error: { code: "invalid_session" } });
        return;
      }
      if (
        request.method !== "GET" &&
        request.headers.origin !== origin
      ) {
        writeJson(response, 403, { error: { code: "invalid_origin" } });
        return;
      }

      try {
        if (url.pathname === "/api/state" && request.method === "GET") {
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (url.pathname === "/api/open" && request.method === "POST") {
          await controller.open(await readJson(request));
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (url.pathname === "/api/reload" && request.method === "POST") {
          await controller.reload(await readJson(request));
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (url.pathname === "/api/update" && request.method === "POST") {
          controller.updateObject(await readJson(request));
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (
          url.pathname === "/api/object-create" &&
          request.method === "POST"
        ) {
          controller.createObject(await readJson(request));
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (
          url.pathname === "/api/object-delete" &&
          request.method === "POST"
        ) {
          controller.deleteObject(await readJson(request));
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (url.pathname === "/api/save" && request.method === "POST") {
          const body = expectBrowserRequest(
            await readJson(request),
            ["expectedRevision"]
          );
          await controller.save({
            expectedRevision: body.expectedRevision,
            expectedCas: controller.view().cas
          });
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (
          url.pathname === "/api/content-check" &&
          request.method === "POST"
        ) {
          const body = expectBrowserRequest(
            await readJson(request),
            ["expectedRevision", "expectedDocumentLease"]
          );
          controller.checkContent(body);
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (
          url.pathname === "/api/package-export" &&
          request.method === "POST"
        ) {
          const body = expectBrowserRequest(
            await readJson(request),
            [
              "expectedRevision",
              "expectedDocumentLease",
              "expectedPreparedPackageLease"
            ]
          );
          const artifact = controller.exportPackage(body);
          writeBytes(
            response,
            200,
            artifact.bytes,
            "application/octet-stream",
            {
              "Content-Disposition":
                'attachment; filename="' + artifact.filename + '"'
            }
          );
          return;
        }
        if (
          url.pathname === "/api/preview-publish" &&
          request.method === "POST"
        ) {
          const body = expectBrowserRequest(
            await readJson(request),
            [
              "expectedRevision",
              "expectedDocumentLease",
              "expectedPreparedPackageLease"
            ]
          );
          controller.publishPreview(body);
          writeJson(response, 200, { state: browserState(controller) });
          return;
        }
        if (
          url.pathname === "/api/preview-package" &&
          request.method === "GET"
        ) {
          if (previewRoot === null) {
            const error = new Error("system Demo Preview host is unavailable");
            error.code = "preview_unavailable";
            error.status = 503;
            throw error;
          }
          const publication = controller.previewPackage(
            url.searchParams.get("token")
          );
          writeBytes(
            response,
            200,
            publication.bytes,
            "application/octet-stream",
            {
              "X-TGD-Preview-Generation": String(publication.generation),
              "X-TGD-Package-SHA256": publication.packageSha256
            }
          );
          return;
        }
        writeJson(response, 404, { error: { code: "not_found" } });
      } catch (error) {
        const failure = apiError(error, controller);
        writeJson(response, failure.status, failure.body);
      }
    })().catch((error) => {
      if (!response.headersSent) {
        const failure = apiError(error, controller);
        writeJson(response, failure.status, failure.body);
      } else {
        response.destroy(error);
      }
    });
  });

  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, LOOPBACK_HOST, () => {
      server.off("error", reject);
      resolve();
    });
  });
  const address = server.address();
  if (address === null || typeof address === "string") {
    await new Promise((resolve) => server.close(resolve));
    compilerService?.close?.();
    throw new Error("Workbench server did not receive a TCP address");
  }
  origin = "http://" + LOOPBACK_HOST + ":" + address.port;

  return Object.freeze({
    url: origin + "/",
    workspaceRoot: workspace.rootPath,
    controller,
    async close() {
      try {
        await new Promise((resolve, reject) => {
          server.close((error) => (error ? reject(error) : resolve()));
        });
      } finally {
        compilerService?.close?.();
      }
    }
  });
}

function commandLineOptions(args) {
  let workspaceRoot = null;
  let sandboxBuildDirectory = null;
  for (let index = 0; index < args.length; index += 2) {
    const name = args[index];
    const value = args[index + 1];
    if (
      typeof value !== "string" ||
      (name !== "--workspace" && name !== "--sandbox-service-build")
    ) {
      throw new Error(
        "usage: npm start -- --workspace <existing-directory> " +
          "[--sandbox-service-build <repository-relative-build-directory>]"
      );
    }
    if (name === "--workspace" && workspaceRoot === null) {
      workspaceRoot = value;
    } else if (
      name === "--sandbox-service-build" &&
      sandboxBuildDirectory === null
    ) {
      sandboxBuildDirectory = value;
    } else {
      throw new Error("Workbench command-line option was repeated");
    }
  }
  if (workspaceRoot === null) {
    throw new Error(
      "usage: npm start -- --workspace <existing-directory> " +
        "[--sandbox-service-build <repository-relative-build-directory>]"
    );
  }
  return { workspaceRoot, sandboxBuildDirectory };
}

const invokedAsScript =
  process.argv[1] &&
  import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href;

if (invokedAsScript) {
  try {
    const running = await startWorkbenchServer(
      commandLineOptions(process.argv.slice(2))
    );
    console.log("Sandbox Content Workbench: " + running.url);
    console.log("Workspace root: " + running.workspaceRoot);
    process.once("SIGINT", async () => {
      await running.close();
      process.exitCode = 0;
    });
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
