import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const required = ["schemaVersion", "id", "displayNameKey", "tags", "designerNote"];

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function findJsonFiles(dir) {
  return readdirSync(dir)
    .flatMap((entry) => {
      const fullPath = join(dir, entry);
      if (statSync(fullPath).isDirectory()) {
        return findJsonFiles(fullPath);
      }
      return fullPath.endsWith(".json") ? [fullPath] : [];
    })
    .sort();
}

describe("content schema smoke test", () => {
  const files = findJsonFiles(join(root, "content"));

  it("finds content files", () => {
    assert.ok(files.length >= 6);
  });

  for (const file of files) {
    it(`${file} has required AI-collaboration fields`, () => {
      const data = readJson(file);
      for (const field of required) {
        assert.ok(field in data, `${field} missing`);
      }
      assert.equal(typeof data.schemaVersion, "number");
      assert.ok(Array.isArray(data.tags));
      assert.ok(data.id.includes("."));
    });
  }
});
