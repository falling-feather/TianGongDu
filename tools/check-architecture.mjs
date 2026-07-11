import { readFile, readdir } from "node:fs/promises";
import { dirname, extname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const modulePath = fileURLToPath(import.meta.url);
const defaultRoot = resolve(dirname(modulePath), "..");
const sourceExtensions = new Set([".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"]);

const rules = {
  contracts: {
    roots: ["src/contracts"],
    projectIncludes: new Set(["contracts"]),
    privateExternalIncludes: [],
  },
  runtime: {
    roots: ["src/runtime"],
    projectIncludes: new Set(["contracts", "runtime"]),
    privateExternalIncludes: [],
  },
  gameplay: {
    roots: ["src/gameplay"],
    projectIncludes: new Set(["contracts", "runtime", "gameplay"]),
    privateExternalIncludes: [],
  },
  presentation: {
    roots: ["src/presentation-axmol"],
    projectIncludes: new Set(["contracts", "runtime", "presentation"]),
    privateExternalIncludes: [/^(axmol|cocos2d)(\/|\.|$)/i],
  },
  platform: {
    roots: ["src/platform"],
    projectIncludes: new Set(["contracts", "runtime", "platform"]),
    privateExternalIncludes: [/^emscripten(\/|\.|$)/i],
  },
  content: {
    roots: ["src/content-core"],
    projectIncludes: new Set(["contracts", "content"]),
    privateExternalIncludes: [],
  },
  sync_contracts: {
    roots: ["src/sync-contracts"],
    projectIncludes: new Set(["contracts", "sync"]),
    privateExternalIncludes: [],
  },
  sync_service: {
    roots: ["server/sync-service"],
    projectIncludes: new Set(["contracts", "sync"]),
    privateExternalIncludes: [/^(drogon|pqxx|libpq)(\/|\.|$)/i],
  },
};

const sensitiveExternalInclude = /^(axmol|cocos2d|emscripten|drogon|pqxx|libpq)(\/|\.|$)/i;

function projectLayer(includePath) {
  const match = /^tgd\/([^/]+)/.exec(includePath);
  return match?.[1] ?? null;
}

export function inspectSource(layerName, source, sourcePath = "<memory>") {
  const rule = rules[layerName];
  if (!rule) throw new Error(`Unknown architecture layer: ${layerName}`);

  const errors = [];
  const isPublicHeader = sourcePath.split(/[\\/]/).includes("include");
  const includePattern = /^\s*#\s*include\s*[<"]([^>"]+)[>"]/gm;
  for (const match of source.matchAll(includePattern)) {
    const includePath = match[1].replaceAll("\\", "/");
    const includedLayer = projectLayer(includePath);
    if (includedLayer && !rule.projectIncludes.has(includedLayer)) {
      errors.push(`${sourcePath}: ${layerName} cannot include project layer ${includedLayer}: ${includePath}`);
      continue;
    }

    if (!sensitiveExternalInclude.test(includePath)) continue;
    const allowedPrivately = rule.privateExternalIncludes.some((pattern) => pattern.test(includePath));
    if (isPublicHeader || !allowedPrivately) {
      errors.push(`${sourcePath}: ${layerName} cannot expose/include external dependency: ${includePath}`);
    }
  }

  return errors;
}

async function collectSourceFiles(root) {
  const files = [];
  for (const entry of await readdir(root, { withFileTypes: true })) {
    const path = join(root, entry.name);
    if (entry.isDirectory()) files.push(...await collectSourceFiles(path));
    if (entry.isFile() && sourceExtensions.has(extname(entry.name).toLowerCase())) files.push(path);
  }
  return files;
}

export async function validateArchitecture(projectRoot = defaultRoot) {
  const root = resolve(projectRoot);
  const errors = [];
  for (const [layerName, rule] of Object.entries(rules)) {
    for (const relativeRoot of rule.roots) {
      const sourceRoot = join(root, relativeRoot);
      for (const sourcePath of await collectSourceFiles(sourceRoot)) {
        const source = await readFile(sourcePath, "utf8");
        const displayPath = relative(root, sourcePath).split(sep).join("/");
        errors.push(...inspectSource(layerName, source, displayPath));
      }
    }
  }
  return errors;
}

if (process.argv[1] && resolve(process.argv[1]) === modulePath) {
  const errors = await validateArchitecture();
  if (errors.length > 0) {
    console.error("天工渡架构依赖检查失败：");
    for (const error of errors) console.error(`- ${error}`);
    process.exitCode = 1;
  } else {
    console.log("天工渡架构依赖检查通过。\n");
  }
}
