import { readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const inputPath = resolve(root, "content/design/indexeddb-v1.json");
const outputPath = resolve(root, "apps/web-shell/indexeddb-v1.generated.js");

function fail(message) {
  throw new Error(`IndexedDB v1 contract: ${message}`);
}

export function validateIndexedDbContract(contract) {
  if (contract.schemaVersion !== "1.0.0") fail("unsupported schemaVersion");
  if (
    contract.database?.name !== "tiangongdu.prototype_f1.internal.profile" ||
    contract.database?.version !== 1 ||
    contract.database?.channel !== "prototype_f1"
  ) {
    fail("database identity drifted");
  }
  if (!Array.isArray(contract.stores) || contract.stores.length !== 6) {
    fail("exactly six v1 stores are required");
  }
  const expectedStores = [
    { recordKind: 1, name: "profile_heads", keyPath: ["channel", "profileId"], indexes: [] },
    {
      recordKind: 2,
      name: "snapshots",
      keyPath: ["profileId", "snapshotId"],
      indexes: [
        {
          name: "by_profile_sequence",
          keyPath: ["profileId", "logicalSequence"],
          unique: false
        }
      ]
    },
    {
      recordKind: 3,
      name: "operations",
      keyPath: ["profileId", "operationId"],
      indexes: [
        {
          name: "by_profile_device_sequence",
          keyPath: ["profileId", "deviceId", "logicalSequence"],
          unique: false
        },
        {
          name: "by_profile_status",
          keyPath: ["profileId", "status"],
          unique: false
        }
      ]
    },
    { recordKind: 4, name: "profile_meta", keyPath: "profileId", indexes: [] },
    {
      recordKind: 5,
      name: "device_settings",
      keyPath: ["channel", "settingGroup"],
      indexes: []
    },
    {
      recordKind: 6,
      name: "migration_workspace",
      keyPath: ["profileId", "migrationId"],
      indexes: []
    }
  ];
  const names = new Set();
  const kinds = new Set();
  for (const [index, store] of contract.stores.entries()) {
    const expected = expectedStores[index];
    if (JSON.stringify(store) !== JSON.stringify(expected)) {
      fail(`store ${index + 1} must match the frozen ${expected.name} v1 contract`);
    }
    if (names.has(store.name) || kinds.has(store.recordKind)) fail("duplicate store identity");
    names.add(store.name);
    kinds.add(store.recordKind);
    if (!(typeof store.keyPath === "string" || Array.isArray(store.keyPath))) {
      fail(`${store.name} has an invalid keyPath`);
    }
    if (!Array.isArray(store.indexes)) fail(`${store.name} indexes must be an array`);
  }
  return contract;
}

export function renderIndexedDbContract(contract) {
  const serialized = JSON.stringify(contract, null, 2)
    .split("\n")
    .map((line) => `  ${line}`)
    .join("\n");
  return `// Generated from content/design/indexeddb-v1.json. Do not edit by hand.\n(() => {\n  \"use strict\";\n  const deepFreeze = (value) => {\n    if (value && typeof value === \"object\" && !Object.isFrozen(value)) {\n      Object.freeze(value);\n      for (const child of Object.values(value)) deepFreeze(child);\n    }\n    return value;\n  };\n  globalThis.TgdIndexedDbContract = deepFreeze(\n${serialized}\n  );\n})();\n`;
}

async function main() {
  const contract = validateIndexedDbContract(JSON.parse(await readFile(inputPath, "utf8")));
  const rendered = renderIndexedDbContract(contract);
  if (process.argv.includes("--check")) {
    const current = await readFile(outputPath, "utf8").catch(() => "");
    if (current !== rendered) fail("generated browser contract is stale; run npm run generate:indexeddb-contract");
    return;
  }
  await writeFile(outputPath, rendered, "utf8");
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(import.meta.filename)) {
  await main();
}
