import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  SANDBOX_AUTHORING_FORMAT,
  SANDBOX_AUTHORING_VERSION,
  SandboxAuthoringFormatError,
  migrateSandboxAuthoringDocument,
  normalizeSandboxAuthoringDocument,
  projectSandboxRuntimeDocument,
  serializeSandboxAuthoringDocument
} from "../src/authoring-document.mjs";

const fixtureText = await readFile(
  new URL("./fixtures/system-demo-authoring.v1.valid.json", import.meta.url),
  "utf8"
);

function fixture() {
  return JSON.parse(fixtureText);
}

function expectFormatFailure(mutator) {
  const source = fixture();
  mutator(source);
  assert.throws(
    () => normalizeSandboxAuthoringDocument(source),
    SandboxAuthoringFormatError
  );
}

test("strict 1.1.0 document round-trips as an owning frozen value", () => {
  const source = fixture();
  const normalized = normalizeSandboxAuthoringDocument(source);
  const originalX = normalized.runtime.player.pose.x;

  source.runtime.player.pose.x = 999999;
  source.editor.items[0].label = "changed outside";

  assert.equal(normalized.format, SANDBOX_AUTHORING_FORMAT);
  assert.equal(normalized.schemaVersion, SANDBOX_AUTHORING_VERSION);
  assert.equal(normalized.runtime.player.pose.x, originalX);
  assert.notEqual(normalized.editor.items[0].label, "changed outside");
  assert.ok(Object.isFrozen(normalized));
  assert.ok(Object.isFrozen(normalized.runtime.player.pose));

  const bytes = serializeSandboxAuthoringDocument(normalized);
  const reloaded = normalizeSandboxAuthoringDocument(bytes);
  assert.deepEqual(reloaded, normalized);
  assert.equal(serializeSandboxAuthoringDocument(reloaded), bytes);
});

test("normalization is deterministic for shuffled arrays and object keys", () => {
  const ordered = fixture();
  const shuffled = fixture();
  const idCollections = [
    "regions",
    "assets",
    "actors",
    "groundBlockers",
    "safePoints",
    "interactions",
    "mechanisms",
    "waves",
    "objectives"
  ];
  for (const source of [ordered, shuffled]) {
    for (const collection of idCollections) {
      const record = structuredClone(source.runtime[collection][0]);
      record.id += ".z";
      source.runtime[collection].push(record);
    }
    source.runtime.waveSpawns.push({
      ...source.runtime.waveSpawns[0],
      actorId: "actor.z",
      spawnOrder: 1
    });
    source.runtime.interactionBindings.push({
      ...source.runtime.interactionBindings[0],
      interactionId: "interaction.z"
    });
    source.runtime.mechanismBindings.push({
      ...source.runtime.mechanismBindings[0],
      mechanismId: "mechanism.z"
    });
    source.editor.items.push({
      id: "editor.z",
      label: "z",
      ordinal: 99,
      canvasX: 1,
      canvasY: 1
    });
  }
  for (const value of Object.values(shuffled.runtime)) {
    if (Array.isArray(value)) {
      value.reverse();
    }
  }
  shuffled.editor.items.reverse();
  shuffled.runtime = Object.fromEntries(
    Object.entries(shuffled.runtime).reverse()
  );

  assert.equal(
    serializeSandboxAuthoringDocument(shuffled),
    serializeSandboxAuthoringDocument(ordered)
  );
});

test("editor-only changes never leak into the runtime projection", () => {
  const source = fixture();
  const changed = fixture();
  changed.editor.items[0].label = "another label";
  changed.editor.items[0].ordinal = 99;
  changed.editor.items[0].canvasX = -400;
  changed.editor.items[0].canvasY = 700;

  const projection = projectSandboxRuntimeDocument(source);
  assert.deepEqual(projection, projectSandboxRuntimeDocument(changed));
  assert.equal(Object.prototype.hasOwnProperty.call(projection, "editor"), false);
  assert.ok(Object.isFrozen(projection));
  assert.notEqual(
    serializeSandboxAuthoringDocument(source),
    serializeSandboxAuthoringDocument(changed)
  );
});

test("migration is identity-only for 1.1.0", () => {
  assert.deepEqual(
    migrateSandboxAuthoringDocument(fixture()),
    normalizeSandboxAuthoringDocument(fixture())
  );
  expectFormatFailure((source) => {
    source.schemaVersion = "1.0.0";
  });
});

test("generic drafts may keep gameplay collections empty", () => {
  const source = fixture();
  for (const collection of [
    "actors",
    "groundBlockers",
    "interactions",
    "mechanisms",
    "waves",
    "waveSpawns",
    "objectives",
    "interactionBindings",
    "mechanismBindings"
  ]) {
    source.runtime[collection] = [];
  }
  assert.equal(
    normalizeSandboxAuthoringDocument(source).runtime.player.id,
    "player.start"
  );
});

test("malformed, missing, unknown, and wrong primitive inputs fail closed", () => {
  assert.throws(
    () => normalizeSandboxAuthoringDocument("{"),
    SandboxAuthoringFormatError
  );
  expectFormatFailure((source) => {
    delete source.runtime.player;
  });
  expectFormatFailure((source) => {
    source.unexpected = true;
  });
  expectFormatFailure((source) => {
    source.format = "tgd.sandbox.package";
  });
  expectFormatFailure((source) => {
    source.runtime.packageId = 42;
  });
  expectFormatFailure((source) => {
    source.runtime.player = [source.runtime.player, source.runtime.player];
  });
});

test("unknown enums, unsafe integers, and capacities fail closed", () => {
  expectFormatFailure((source) => {
    source.runtime.assets[0].kind = "unknown";
  });
  expectFormatFailure((source) => {
    source.runtime.bounds.minX = Number.MAX_SAFE_INTEGER + 1;
  });
  expectFormatFailure((source) => {
    source.runtime.waveSpawns[0].delayTicks = -1;
  });
  expectFormatFailure((source) => {
    const actor = source.runtime.actors[0];
    source.runtime.actors = Array.from({ length: 16 }, (_, index) => ({
      ...actor,
      id: "actor.capacity." + index
    }));
  });
});

test("GroundPose accepts only x/y/height/floorLayer", () => {
  expectFormatFailure((source) => {
    source.runtime.player.pose.z = 0;
  });
  expectFormatFailure((source) => {
    delete source.runtime.player.pose.height;
  });
  expectFormatFailure((source) => {
    source.runtime.player.pose.x = 0.5;
  });
});

test("legacy graph and timing shapes fail closed", () => {
  expectFormatFailure((source) => {
    source.runtime.waves[0].trigger.kind = "manual";
  });
  expectFormatFailure((source) => {
    source.runtime.waveSpawns[0].delayMs = 500;
  });
  expectFormatFailure((source) => {
    source.runtime.waves[0].prerequisites = [];
  });
  expectFormatFailure((source) => {
    source.runtime.waves[0].trigger.kind = "all_of";
  });
});

test("binding ranges and typed target shapes fail closed", () => {
  expectFormatFailure((source) => {
    source.runtime.interactionBindings[0].rangeMm = 499;
  });
  expectFormatFailure((source) => {
    source.runtime.interactionBindings[0].rangeMm = 3001;
  });
  expectFormatFailure((source) => {
    source.runtime.interactionBindings[0].targetMechanismId = null;
  });
  expectFormatFailure((source) => {
    const binding = source.runtime.interactionBindings[0];
    binding.target = binding.targetMechanismId;
    delete binding.targetMechanismId;
  });
  expectFormatFailure((source) => {
    source.runtime.safePointBindings = [];
  });
});

test("root npm test is wired to content-workbench tests", async () => {
  const rootPackage = JSON.parse(
    await readFile(new URL("../../../package.json", import.meta.url), "utf8")
  );
  assert.equal(
    rootPackage.scripts["test:content-workbench"],
    "npm --prefix apps/content-workbench test"
  );
  assert.match(rootPackage.scripts.test, /npm run test:content-workbench/);
});
