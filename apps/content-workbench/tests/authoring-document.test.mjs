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
  new URL("../../../content/design/system-demo.sandbox.json", import.meta.url),
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
  assert.equal(bytes, fixtureText);
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

test("unique system Demo author source has exact globally distinct Stable IDs", () => {
  const source = fixture();
  assert.equal(source.runtime.packageId, "system-demo.package");
  assert.equal(source.runtime.sandboxId, "system-demo.sandbox");
  assert.equal(
    source.runtime.completionObjectiveId,
    "objective.system_demo.terminal"
  );
  assert.equal(source.runtime.assets.length, 12);
  assert.equal(source.runtime.actors.length, 4);
  assert.equal(source.runtime.waves.length, 2);
  assert.equal(source.runtime.waveSpawns.length, 4);
  assert.equal(source.runtime.objectives.length, 2);
  assert.equal(source.runtime.interactionBindings.length, 1);
  assert.equal(source.runtime.mechanismBindings.length, 1);
  assert.equal(source.editor.items.length, 13);

  const recordIds = [
    source.runtime.packageId,
    source.runtime.sandboxId,
    source.runtime.player.id
  ];
  for (const collection of [
    "regions",
    "assets",
    "actors",
    "groundBlockers",
    "safePoints",
    "interactions",
    "mechanisms",
    "waves",
    "objectives"
  ]) {
    recordIds.push(...source.runtime[collection].map((record) => record.id));
  }
  assert.equal(new Set(recordIds).size, recordIds.length);
});

test("unique system Demo source preserves the frozen typed chain and neutral wave slots", () => {
  const { runtime, editor } = fixture();
  assert.deepEqual(runtime.bounds, runtime.regions[0].bounds);
  assert.deepEqual(runtime.bounds, {
    minX: -2500,
    maxX: 2500,
    minY: -4000,
    maxY: 10000,
    minHeight: -500,
    maxHeight: 3000,
    minFloorLayer: 0,
    maxFloorLayer: 0
  });
  assert.deepEqual(
    runtime.actors.map(({ id, assetId }) => [id, assetId]),
    [
      [
        "actor.system_demo.entry.slot_a",
        "asset.system_demo.enemy.pressure"
      ],
      [
        "actor.system_demo.entry.slot_b",
        "asset.system_demo.enemy.flanker"
      ],
      [
        "actor.system_demo.followup.slot_a",
        "asset.system_demo.enemy.pressure"
      ],
      [
        "actor.system_demo.followup.slot_b",
        "asset.system_demo.enemy.elite"
      ]
    ]
  );
  assert.deepEqual(
    runtime.waveSpawns.map(
      ({ waveId, actorId, delayTicks, spawnOrder }) =>
        [waveId, actorId, delayTicks, spawnOrder]
    ),
    [
      [
        "wave.system_demo.entry",
        "actor.system_demo.entry.slot_a",
        0,
        0
      ],
      [
        "wave.system_demo.entry",
        "actor.system_demo.entry.slot_b",
        0,
        1
      ],
      [
        "wave.system_demo.followup",
        "actor.system_demo.followup.slot_a",
        0,
        0
      ],
      [
        "wave.system_demo.followup",
        "actor.system_demo.followup.slot_b",
        0,
        1
      ]
    ]
  );
  assert.deepEqual(runtime.interactionBindings, [
    {
      interactionId: "interaction.system_demo.console",
      operation: "operate",
      rangeMm: 1200,
      targetMechanismId: "mechanism.system_demo.gate"
    }
  ]);
  assert.deepEqual(runtime.mechanismBindings, [
    {
      mechanismId: "mechanism.system_demo.gate",
      activation: "one_shot_activate",
      targetGroundBlockerId: "blocker.system_demo.gate"
    }
  ]);
  assert.deepEqual(
    runtime.objectives.map(
      ({ id, predecessorObjectiveId, completion }) =>
        [id, predecessorObjectiveId, completion.kind, completion.targetId]
    ),
    [
      [
        "objective.system_demo.open_gate",
        "",
        "mechanism_activated",
        "mechanism.system_demo.gate"
      ],
      [
        "objective.system_demo.terminal",
        "objective.system_demo.open_gate",
        "wave_completed",
        "wave.system_demo.followup"
      ]
    ]
  );
  assert.deepEqual(
    runtime.waves.map(({ id, predecessorWaveId, trigger }) =>
      [id, predecessorWaveId, trigger.kind, trigger.targetId]
    ),
    [
      [
        "wave.system_demo.entry",
        "",
        "objective_completed",
        "objective.system_demo.open_gate"
      ],
      [
        "wave.system_demo.followup",
        "wave.system_demo.entry",
        "wave_completed",
        "wave.system_demo.entry"
      ]
    ]
  );

  const effectIds = runtime.assets
    .filter(({ kind }) => kind === "effect")
    .map(({ id }) => id);
  assert.deepEqual(effectIds, [
    "asset.system_demo.skill.eavesguard.hit",
    "asset.system_demo.skill.eavesguard.telegraph",
    "asset.system_demo.skill.flower_turn.hit",
    "asset.system_demo.skill.flower_turn.telegraph"
  ]);
  assert.equal(
    runtime.actors.some(({ assetId }) => effectIds.includes(assetId)),
    false
  );

  const editorOwnerIds = [
    runtime.player.id,
    ...runtime.actors.map(({ id }) => id),
    ...runtime.groundBlockers.map(({ id }) => id),
    ...runtime.safePoints.map(({ id }) => id),
    ...runtime.interactions.map(({ id }) => id),
    ...runtime.mechanisms.map(({ id }) => id),
    ...runtime.waves.map(({ id }) => id),
    ...runtime.objectives.map(({ id }) => id)
  ].sort();
  assert.deepEqual(
    editor.items.map(({ id }) => id).sort(),
    editorOwnerIds
  );
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
    "player.system_demo.start"
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

test("player and placement facing use the uint32 structural range", () => {
  const maximum = 4294967295;
  for (const value of [0, maximum]) {
    const source = fixture();
    source.runtime.player.facingMillidegrees = value;
    source.runtime.actors[0].facingMillidegrees = value;
    assert.doesNotThrow(() => normalizeSandboxAuthoringDocument(source));
  }

  for (const [owner, value] of [
    ["player", -1],
    ["player", maximum + 1],
    ["placement", -1],
    ["placement", maximum + 1]
  ]) {
    expectFormatFailure((source) => {
      if (owner === "player") {
        source.runtime.player.facingMillidegrees = value;
      } else {
        source.runtime.actors[0].facingMillidegrees = value;
      }
    });
  }
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
