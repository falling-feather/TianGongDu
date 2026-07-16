import assert from "node:assert/strict";
import {
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  symlink,
  writeFile
} from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import test from "node:test";

import { serializeSandboxAuthoringDocument } from "../src/authoring-document.mjs";
import { createLocalWorkspace } from "../src/local-workspace.mjs";
import { createWorkbenchController } from "../src/workbench-controller.mjs";

const fixtureUrl = new URL(
  "./fixtures/system-demo-authoring.v1.valid.json",
  import.meta.url
);

async function temporaryWorkspace(t) {
  const root = await mkdtemp(path.join(tmpdir(), "tgd-workbench-"));
  t.after(() => rm(root, { recursive: true, force: true }));
  await copyFile(fixtureUrl, path.join(root, "demo.json"));
  return root;
}

function placementValues(record) {
  return {
    regionId: record.regionId,
    assetId: record.assetId,
    pose: { ...record.pose },
    facingMillidegrees: record.facingMillidegrees
  };
}

function editableValues(document, kind, id) {
  if (kind === "player") {
    return {
      ...placementValues(document.runtime.player),
      initialSafePointId: document.runtime.player.initialSafePointId
    };
  }
  const record = document.runtime[kind].find((value) => value.id === id);
  if (kind === "groundBlockers") {
    const { id: ignored, ...values } = record;
    void ignored;
    return values;
  }
  const values = placementValues(record);
  if (kind === "interactions") {
    const binding = document.runtime.interactionBindings.find(
      (value) => value.interactionId === id
    );
    values.binding = binding
      ? {
          operation: binding.operation,
          rangeMm: binding.rangeMm,
          targetMechanismId: binding.targetMechanismId
        }
      : null;
  }
  if (kind === "mechanisms") {
    const binding = document.runtime.mechanismBindings.find(
      (value) => value.mechanismId === id
    );
    values.binding = binding
      ? {
          activation: binding.activation,
          targetGroundBlockerId: binding.targetGroundBlockerId
        }
      : null;
  }
  return values;
}

async function openedController(root, options = {}) {
  const workspace = await createLocalWorkspace({ rootPath: root, ...options });
  const controller = createWorkbenchController({ workspace });
  await controller.open({ relativePath: "demo.json", confirmDiscard: false });
  return controller;
}

function stateSnapshot(controller) {
  const state = controller.editorState;
  return {
    document: serializeSandboxAuthoringDocument(state.document),
    lastValid: serializeSandboxAuthoringDocument(state.lastValidDocument),
    revision: state.revision,
    savedRevision: state.savedRevision,
    dirty: state.dirty
  };
}

test("six explicit object kinds edit, preserve hidden sections, and round-trip", async (t) => {
  const root = await temporaryWorkspace(t);
  const controller = await openedController(root);
  const initial = structuredClone(controller.editorState.document);
  const edits = [
    ["player", "player.start", "pose", "x", 101],
    ["actors", "actor.demo", "pose", "x", 2201],
    ["groundBlockers", "blocker.gate", null, "minX", 999],
    ["safePoints", "safe.start", "pose", "x", -999],
    ["interactions", "interaction.console", "binding", "rangeMm", 1300],
    ["mechanisms", "mechanism.gate", "pose", "x", 1301]
  ];

  for (const [kind, id, nested, field, value] of edits) {
    const values = editableValues(controller.editorState.document, kind, id);
    if (nested) {
      values[nested][field] = value;
    } else {
      values[field] = value;
    }
    controller.updateObject({
      kind,
      id,
      values,
      expectedRevision: controller.editorState.revision
    });
  }

  assert.equal(controller.editorState.revision, 6);
  assert.equal(controller.editorState.dirty, true);
  for (const section of [
    "regions",
    "assets",
    "waves",
    "waveSpawns",
    "objectives"
  ]) {
    assert.deepEqual(
      controller.editorState.document.runtime[section],
      initial.runtime[section]
    );
  }
  assert.deepEqual(controller.editorState.document.editor, initial.editor);

  const beforeSaveCas = controller.view().cas;
  const saved = await controller.save({
    expectedRevision: 6,
    expectedCas: beforeSaveCas
  });
  assert.equal(saved.dirty, false);
  assert.equal(saved.savedRevision, 6);
  const firstBytes = await readFile(path.join(root, "demo.json"), "utf8");
  assert.equal(
    firstBytes,
    serializeSandboxAuthoringDocument(controller.editorState.document)
  );

  const reopened = await openedController(root);
  assert.deepEqual(reopened.editorState.document, controller.editorState.document);
  await reopened.save({
    expectedRevision: 0,
    expectedCas: reopened.view().cas
  });
  assert.equal(await readFile(path.join(root, "demo.json"), "utf8"), firstBytes);
});

test("invalid values and stale revisions preserve document and last-valid", async (t) => {
  const root = await temporaryWorkspace(t);
  const controller = await openedController(root);
  const before = stateSnapshot(controller);
  const values = editableValues(
    controller.editorState.document,
    "safePoints",
    "safe.start"
  );
  values.pose.x = "not-an-integer";

  assert.throws(
    () =>
      controller.updateObject({
        kind: "safePoints",
        id: "safe.start",
        values,
        expectedRevision: 0
      }),
    (error) => error.code === "invalid_document"
  );
  assert.deepEqual(stateSnapshot(controller), before);

  assert.throws(
    () =>
      controller.updateObject({
        kind: "actors",
        id: "actor.demo",
        values: editableValues(
          controller.editorState.document,
          "actors",
          "actor.demo"
        ),
        expectedRevision: 9
      }),
    (error) => error.code === "stale_revision"
  );
  assert.deepEqual(stateSnapshot(controller), before);
});

test("malformed, unsupported, and closed-shape loads preserve the opened state", async (t) => {
  const root = await temporaryWorkspace(t);
  const controller = await openedController(root);
  const beforeView = controller.view();
  const beforeState = stateSnapshot(controller);
  const fixture = JSON.parse(await readFile(fixtureUrl, "utf8"));

  await writeFile(path.join(root, "malformed.json"), "{", "utf8");
  fixture.schemaVersion = "1.0.0";
  await writeFile(path.join(root, "version.json"), JSON.stringify(fixture), "utf8");
  fixture.schemaVersion = "1.1.0";
  fixture.runtime.player.pose.z = 0;
  await writeFile(path.join(root, "closed.json"), JSON.stringify(fixture), "utf8");

  for (const relativePath of ["malformed.json", "version.json", "closed.json"]) {
    await assert.rejects(
      controller.open({ relativePath, confirmDiscard: false }),
      (error) => error.code === "invalid_document"
    );
    assert.equal(controller.view().relativePath, beforeView.relativePath);
    assert.equal(controller.view().cas, beforeView.cas);
    assert.deepEqual(stateSnapshot(controller), beforeState);
  }

  const unopened = createWorkbenchController({
    workspace: await createLocalWorkspace({ rootPath: root })
  });
  await assert.rejects(
    unopened.open({ relativePath: "malformed.json", confirmDiscard: false }),
    (error) => error.code === "invalid_document"
  );
  assert.equal(unopened.view().opened, false);
});

test("workspace rejects path escapes, directories, and reparse escapes", async (t) => {
  const parent = await mkdtemp(path.join(tmpdir(), "tgd-path-boundary-"));
  t.after(() => rm(parent, { recursive: true, force: true }));
  const root = path.join(parent, "root");
  const outside = path.join(parent, "outside");
  await mkdir(root);
  await mkdir(outside);
  await writeFile(path.join(root, "valid.json"), await readFile(fixtureUrl));
  await writeFile(path.join(outside, "outside.json"), await readFile(fixtureUrl));
  await mkdir(path.join(root, "directory.json"));
  await symlink(
    outside,
    path.join(root, "escape"),
    process.platform === "win32" ? "junction" : "dir"
  );
  const workspace = await createLocalWorkspace({ rootPath: root });

  for (const candidate of [
    "../outside/outside.json",
    path.join(root, "valid.json"),
    "C:\\outside.json",
    "\\\\server\\share\\outside.json",
    "bad\0name.json",
    "directory.json",
    "escape/outside.json"
  ]) {
    await assert.rejects(workspace.read(candidate));
  }
  assert.equal((await workspace.read("valid.json")).relativePath, "valid.json");
});

test("stale CAS and external changes never overwrite the disk", async (t) => {
  const root = await temporaryWorkspace(t);
  const controller = await openedController(root);
  const values = editableValues(
    controller.editorState.document,
    "actors",
    "actor.demo"
  );
  values.pose.x += 1;
  controller.updateObject({
    kind: "actors",
    id: "actor.demo",
    values,
    expectedRevision: 0
  });

  await assert.rejects(
    controller.save({ expectedRevision: 1, expectedCas: "sha256:stale" }),
    (error) => error.code === "external_change"
  );
  assert.equal(controller.editorState.dirty, true);

  const second = await openedController(root);
  const secondValues = editableValues(
    second.editorState.document,
    "actors",
    "actor.demo"
  );
  secondValues.pose.y += 1;
  second.updateObject({
    kind: "actors",
    id: "actor.demo",
    values: secondValues,
    expectedRevision: 0
  });
  const external = await readFile(path.join(root, "demo.json"), "utf8");
  const changedExternal = external.replace("完成演示", "外部修改");
  await writeFile(path.join(root, "demo.json"), changedExternal, "utf8");
  await assert.rejects(
    second.save({ expectedRevision: 1, expectedCas: second.view().cas }),
    (error) => error.code === "external_change"
  );
  assert.equal(await readFile(path.join(root, "demo.json"), "utf8"), changedExternal);
  assert.equal(second.editorState.dirty, true);
  assert.equal(second.view().conflict, true);
});

test("write, sync, and replace faults clean temp files and retain dirty state", async (t) => {
  for (const checkpoint of ["write", "sync", "replace"]) {
    const root = await temporaryWorkspace(t);
    let armed = true;
    const controller = await openedController(root, {
      faultInjector(name) {
        if (armed && name === checkpoint) {
          armed = false;
          throw new Error("injected " + checkpoint + " failure");
        }
      }
    });
    const original = await readFile(path.join(root, "demo.json"), "utf8");
    const values = editableValues(
      controller.editorState.document,
      "player",
      "player.start"
    );
    values.pose.x += 10;
    controller.updateObject({
      kind: "player",
      id: "player.start",
      values,
      expectedRevision: 0
    });

    await assert.rejects(
      controller.save({ expectedRevision: 1, expectedCas: controller.view().cas }),
      (error) => error.code === "io_error"
    );
    assert.equal(await readFile(path.join(root, "demo.json"), "utf8"), original);
    assert.equal(controller.editorState.dirty, true);
    assert.equal(controller.editorState.savedRevision, 0);
    assert.equal(
      (await readdir(root)).some((name) => name.includes(".tgdtmp-")),
      false
    );
  }
});

test("an edit during save leaves the newer revision dirty", async (t) => {
  const root = await temporaryWorkspace(t);
  let reachedReplace;
  let releaseReplace;
  const atReplace = new Promise((resolve) => {
    reachedReplace = resolve;
  });
  const release = new Promise((resolve) => {
    releaseReplace = resolve;
  });
  const controller = await openedController(root, {
    async faultInjector(name) {
      if (name === "replace") {
        reachedReplace();
        await release;
      }
    }
  });

  const first = editableValues(
    controller.editorState.document,
    "player",
    "player.start"
  );
  first.pose.x = 111;
  controller.updateObject({
    kind: "player",
    id: "player.start",
    values: first,
    expectedRevision: 0
  });
  const originalCas = controller.view().cas;
  const savePromise = controller.save({
    expectedRevision: 1,
    expectedCas: originalCas
  });
  await atReplace;

  const second = editableValues(
    controller.editorState.document,
    "player",
    "player.start"
  );
  second.pose.x = 222;
  controller.updateObject({
    kind: "player",
    id: "player.start",
    values: second,
    expectedRevision: 1
  });
  releaseReplace();
  const result = await savePromise;

  assert.equal(result.revision, 2);
  assert.equal(result.savedRevision, 0);
  assert.equal(result.dirty, true);
  assert.notEqual(result.cas, originalCas);
  assert.equal(result.conflict, false);
  assert.equal(controller.editorState.lastError, null);
  assert.equal(controller.editorState.document.runtime.player.pose.x, 222);
  assert.equal(
    controller.editorState.lastValidDocument.runtime.player.pose.x,
    222
  );
  const disk = JSON.parse(await readFile(path.join(root, "demo.json"), "utf8"));
  assert.equal(disk.runtime.player.pose.x, 111);

  const reconciled = await controller.save({
    expectedRevision: 2,
    expectedCas: result.cas
  });
  assert.equal(reconciled.savedRevision, 2);
  assert.equal(reconciled.dirty, false);
  assert.equal(reconciled.conflict, false);
  assert.equal(
    JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
    222
  );
});

test("open during save isolates the new document epoch and CAS", async (t) => {
  const root = await temporaryWorkspace(t);
  await copyFile(fixtureUrl, path.join(root, "other.json"));
  let reachedReplace;
  let releaseReplace;
  let delayFirstReplace = true;
  const atReplace = new Promise((resolve) => {
    reachedReplace = resolve;
  });
  const release = new Promise((resolve) => {
    releaseReplace = resolve;
  });
  const controller = await openedController(root, {
    async faultInjector(name) {
      if (delayFirstReplace && name === "replace") {
        delayFirstReplace = false;
        reachedReplace();
        await release;
      }
    }
  });
  const values = editableValues(
    controller.editorState.document,
    "player",
    "player.start"
  );
  values.pose.x = 311;
  controller.updateObject({
    kind: "player",
    id: "player.start",
    values,
    expectedRevision: 0
  });
  const oldSave = controller.save({
    expectedRevision: 1,
    expectedCas: controller.view().cas
  });
  await atReplace;

  const opened = await controller.open({
    relativePath: "other.json",
    confirmDiscard: true
  });
  const openedCas = opened.cas;
  assert.equal(opened.relativePath, "other.json");
  assert.equal(opened.revision, 0);
  releaseReplace();
  await oldSave;

  assert.equal(controller.view().relativePath, "other.json");
  assert.equal(controller.view().cas, openedCas);
  assert.equal(controller.view().conflict, false);
  assert.equal(controller.editorState.revision, 0);
  assert.equal(controller.editorState.lastError, null);

  const next = editableValues(
    controller.editorState.document,
    "player",
    "player.start"
  );
  next.pose.x = 322;
  controller.updateObject({
    kind: "player",
    id: "player.start",
    values: next,
    expectedRevision: 0
  });
  const saved = await controller.save({
    expectedRevision: 1,
    expectedCas: controller.view().cas
  });
  assert.equal(saved.dirty, false);
  assert.equal(saved.conflict, false);
  assert.equal(
    JSON.parse(await readFile(path.join(root, "other.json"), "utf8")).runtime.player.pose.x,
    322
  );
  assert.equal(
    JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
    311
  );
});

test("reload during save waits for the current file CAS and remains saveable", async (t) => {
  const root = await temporaryWorkspace(t);
  let reachedReplace;
  let releaseReplace;
  let delayFirstReplace = true;
  const atReplace = new Promise((resolve) => {
    reachedReplace = resolve;
  });
  const release = new Promise((resolve) => {
    releaseReplace = resolve;
  });
  const controller = await openedController(root, {
    async faultInjector(name) {
      if (delayFirstReplace && name === "replace") {
        delayFirstReplace = false;
        reachedReplace();
        await release;
      }
    }
  });
  const first = editableValues(
    controller.editorState.document,
    "player",
    "player.start"
  );
  first.pose.x = 411;
  controller.updateObject({
    kind: "player",
    id: "player.start",
    values: first,
    expectedRevision: 0
  });
  const oldSave = controller.save({
    expectedRevision: 1,
    expectedCas: controller.view().cas
  });
  await atReplace;
  const reloading = controller.reload({ confirmDiscard: true });
  releaseReplace();
  await oldSave;
  const reloaded = await reloading;

  assert.equal(reloaded.relativePath, "demo.json");
  assert.equal(reloaded.revision, 0);
  assert.equal(reloaded.dirty, false);
  assert.equal(reloaded.conflict, false);
  assert.equal(controller.editorState.document.runtime.player.pose.x, 411);
  assert.equal(controller.editorState.lastError, null);

  const second = editableValues(
    controller.editorState.document,
    "player",
    "player.start"
  );
  second.pose.x = 422;
  controller.updateObject({
    kind: "player",
    id: "player.start",
    values: second,
    expectedRevision: 0
  });
  const saved = await controller.save({
    expectedRevision: 1,
    expectedCas: controller.view().cas
  });
  assert.equal(saved.dirty, false);
  assert.equal(saved.conflict, false);
  assert.equal(
    JSON.parse(await readFile(path.join(root, "demo.json"), "utf8")).runtime.player.pose.x,
    422
  );
});
