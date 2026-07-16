import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  createSandboxEditorState,
  reduceSandboxEditorState,
  serializeSandboxEditorState
} from "../src/editor-state.mjs";

const fixtureText = await readFile(
  new URL("./fixtures/system-demo-authoring.v1.valid.json", import.meta.url),
  "utf8"
);

function fixture() {
  return JSON.parse(fixtureText);
}

function actor(state, id) {
  return state.document.runtime.actors.find((value) => value.id === id);
}

function snapshot(state) {
  return {
    document: serializeSandboxEditorState(state),
    lastValid: JSON.stringify(state.lastValidDocument),
    revision: state.revision,
    savedRevision: state.savedRevision,
    dirty: state.dirty
  };
}

function assertUnchanged(before, after) {
  assert.deepEqual(snapshot(after), before);
}

test("state owns and freezes normalized source data", () => {
  const source = fixture();
  const state = createSandboxEditorState(source);
  const originalX = state.document.runtime.player.pose.x;

  source.runtime.player.pose.x = 7000;
  assert.equal(state.document.runtime.player.pose.x, originalX);
  assert.equal(state.revision, 0);
  assert.equal(state.savedRevision, 0);
  assert.equal(state.dirty, false);
  assert.strictEqual(state.document, state.lastValidDocument);
  assert.ok(Object.isFrozen(state));
  assert.ok(Object.isFrozen(state.document.runtime.player.pose));
});

test("real changes increment revision and mark_saved accepts only current revision", () => {
  let state = createSandboxEditorState(fixture());
  const changedFacing = actor(state, "actor.demo").facingMillidegrees + 1000;

  state = reduceSandboxEditorState(state, {
    type: "entity.update",
    expectedRevision: 0,
    collection: "actors",
    key: "actor.demo",
    patch: { facingMillidegrees: changedFacing }
  });
  assert.equal(state.revision, 1);
  assert.equal(state.savedRevision, 0);
  assert.equal(state.dirty, true);
  assert.equal(actor(state, "actor.demo").facingMillidegrees, changedFacing);

  state = reduceSandboxEditorState(state, {
    type: "entity.update",
    expectedRevision: 1,
    collection: "actors",
    key: "actor.demo",
    patch: { facingMillidegrees: changedFacing }
  });
  assert.equal(state.revision, 1);

  const staleSnapshot = snapshot(state);
  const stale = reduceSandboxEditorState(state, {
    type: "document.mark_saved",
    expectedRevision: 0
  });
  assert.equal(stale.lastError.code, "stale_revision");
  assertUnchanged(staleSnapshot, stale);

  state = reduceSandboxEditorState(state, {
    type: "document.mark_saved",
    expectedRevision: 1
  });
  assert.equal(state.revision, 1);
  assert.equal(state.savedRevision, 1);
  assert.equal(state.dirty, false);
  assert.equal(state.lastError, null);
});

test("add and delete own input while unknown ids fail closed", () => {
  let state = createSandboxEditorState(fixture());
  const added = {
    id: "safe.extra",
    regionId: "region.demo",
    assetId: "asset.safe",
    pose: { x: -2000, y: 0, height: 0, floorLayer: 0 },
    facingMillidegrees: 0
  };

  state = reduceSandboxEditorState(state, {
    type: "entity.add",
    expectedRevision: 0,
    collection: "safePoints",
    value: added
  });
  added.pose.x = 9000;
  assert.equal(state.revision, 1);
  assert.equal(
    state.document.runtime.safePoints.find((value) => value.id === "safe.extra")
      .pose.x,
    -2000
  );

  state = reduceSandboxEditorState(state, {
    type: "entity.delete",
    expectedRevision: 1,
    collection: "safePoints",
    key: "safe.extra"
  });
  assert.equal(state.revision, 2);

  const beforeUnknown = snapshot(state);
  const unknownUpdate = reduceSandboxEditorState(state, {
    type: "entity.update",
    expectedRevision: 2,
    collection: "actors",
    key: "actor.missing",
    patch: { facingMillidegrees: 0 }
  });
  assert.equal(unknownUpdate.lastError.code, "unknown_entity");
  assertUnchanged(beforeUnknown, unknownUpdate);

  const unknownDelete = reduceSandboxEditorState(state, {
    type: "entity.delete",
    expectedRevision: 2,
    collection: "actors",
    key: "actor.missing"
  });
  assert.equal(unknownDelete.lastError.code, "unknown_entity");
  assertUnchanged(beforeUnknown, unknownDelete);
});

test("import owns its source and failed imports preserve last-valid", () => {
  let state = createSandboxEditorState(fixture());
  const imported = fixture();
  imported.editor.items[0].label = "imported label";

  state = reduceSandboxEditorState(state, {
    type: "document.import",
    expectedRevision: 0,
    source: imported
  });
  assert.equal(state.revision, 1);

  imported.editor.items[0].label = "mutated after import";
  assert.equal(
    state.document.editor.items.some(
      (item) => item.label === "mutated after import"
    ),
    false
  );

  const beforeFailure = snapshot(state);
  const invalid = fixture();
  invalid.runtime.player.pose.z = 0;
  const failed = reduceSandboxEditorState(state, {
    type: "document.import",
    expectedRevision: 1,
    source: invalid
  });
  assert.equal(failed.lastError.code, "invalid_document");
  assert.strictEqual(failed.document, state.document);
  assert.strictEqual(failed.lastValidDocument, state.lastValidDocument);
  assertUnchanged(beforeFailure, failed);

  const malformed = reduceSandboxEditorState(state, {
    type: "document.import",
    expectedRevision: 1,
    source: "{"
  });
  assert.equal(malformed.lastError.code, "invalid_document");
  assertUnchanged(beforeFailure, malformed);
});

test("invalid mutations and stale revisions preserve document/revision/lastValid", () => {
  const state = createSandboxEditorState(fixture());
  const before = snapshot(state);

  const invalid = reduceSandboxEditorState(state, {
    type: "entity.update",
    expectedRevision: 0,
    collection: "interactionBindings",
    key: "interaction.console",
    patch: { rangeMm: 499 }
  });
  assert.equal(invalid.lastError.code, "invalid_document");
  assert.strictEqual(invalid.document, state.document);
  assert.strictEqual(invalid.lastValidDocument, state.lastValidDocument);
  assertUnchanged(before, invalid);

  const stale = reduceSandboxEditorState(state, {
    type: "entity.delete",
    expectedRevision: 4,
    collection: "actors",
    key: "actor.demo"
  });
  assert.equal(stale.lastError.code, "stale_revision");
  assert.strictEqual(stale.document, state.document);
  assert.strictEqual(stale.lastValidDocument, state.lastValidDocument);
  assertUnchanged(before, stale);
});

test("nested symbol and non-enumerable unknown fields survive cloning and fail closed", () => {
  const decorators = [
    (pose) => {
      pose[Symbol("unknown-pose-field")] = 1;
    },
    (pose) => {
      Object.defineProperty(pose, "hiddenUnknownPoseField", {
        value: 1,
        enumerable: false
      });
    }
  ];

  for (const decorate of decorators) {
    const state = createSandboxEditorState(fixture());
    const before = snapshot(state);
    const pose = { ...state.document.runtime.safePoints[0].pose };
    decorate(pose);

    const failed = reduceSandboxEditorState(state, {
      type: "entity.update",
      expectedRevision: 0,
      collection: "safePoints",
      key: "safe.start",
      patch: { pose }
    });

    assert.equal(failed.lastError.code, "invalid_document");
    assert.strictEqual(failed.document, state.document);
    assert.strictEqual(failed.lastValidDocument, state.lastValidDocument);
    assertUnchanged(before, failed);
  }
});

test("top-level patch descriptors survive merging and fail closed", () => {
  const decorators = [
    (patch) => {
      Object.defineProperty(patch, "hiddenUnknownPatchField", {
        value: 1,
        enumerable: false
      });
    },
    (patch) => {
      Object.defineProperty(patch, Symbol("unknown-patch-field"), {
        value: 1,
        enumerable: false
      });
    }
  ];

  for (const decorate of decorators) {
    const state = createSandboxEditorState(fixture());
    const before = snapshot(state);
    const patch = { facingMillidegrees: 90000 };
    decorate(patch);

    const failed = reduceSandboxEditorState(state, {
      type: "entity.update",
      expectedRevision: 0,
      collection: "safePoints",
      key: "safe.start",
      patch
    });

    assert.equal(failed.lastError.code, "invalid_document");
    assert.strictEqual(failed.document, state.document);
    assert.strictEqual(failed.lastValidDocument, state.lastValidDocument);
    assertUnchanged(before, failed);
  }
});
