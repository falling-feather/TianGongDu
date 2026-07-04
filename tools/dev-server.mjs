import { createServer } from "node:http";
import { createReadStream, existsSync, statSync } from "node:fs";
import { extname, join, normalize, resolve, sep } from "node:path";

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) {
  args.set(process.argv[i], process.argv[i + 1]);
}

const root = resolve(args.get("--root") ?? "prototype/web-demo");
const workspaceRoot = resolve(args.get("--workspace") ?? ".");
const contentRoot = resolve(workspaceRoot, "content");
const host = args.get("--host") ?? "127.0.0.1";
const port = Number(args.get("--port") ?? 4174);

const types = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
  ".svg": "image/svg+xml"
};

function toSafePath(url) {
  const pathname = decodeURIComponent(new URL(url, `http://${host}:${port}`).pathname);
  if (pathname === "/content" || pathname.startsWith("/content/")) {
    const contentPath = pathname.replace(/^\/content\/?/, "");
    const fullContentPath = normalize(join(contentRoot, contentPath));
    if (!fullContentPath.startsWith(contentRoot + sep) && fullContentPath !== contentRoot) {
      return null;
    }
    return fullContentPath;
  }
  const rawPath = pathname === "/" ? "/index.html" : pathname;
  const fullPath = normalize(join(root, rawPath));
  if (!fullPath.startsWith(root + sep) && fullPath !== root) {
    return null;
  }
  return fullPath;
}

const server = createServer((request, response) => {
  const pathname = new URL(request.url ?? "/", `http://${host}:${port}`).pathname;
  if (pathname === "/favicon.ico") {
    response.writeHead(204, { "cache-control": "no-store" });
    response.end();
    return;
  }

  const fullPath = toSafePath(request.url ?? "/");
  if (!fullPath || !existsSync(fullPath) || !statSync(fullPath).isFile()) {
    response.writeHead(404, { "content-type": "text/plain; charset=utf-8" });
    response.end("Not found");
    return;
  }

  response.writeHead(200, {
    "content-type": types[extname(fullPath)] ?? "application/octet-stream",
    "cache-control": "no-store"
  });
  createReadStream(fullPath).pipe(response);
});

server.listen(port, host, () => {
  console.log(`Heavenwrights demo serving ${root}`);
  console.log(`http://${host}:${port}/`);
});
