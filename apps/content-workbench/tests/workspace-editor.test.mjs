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
import { startWorkbenchServer } from "../src/workbench-server.mjs";

const fixtureUrl = new URL(
  "../../../content/design/system-demo.sandbox.json",
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
  const {
    observeRead,
    compilerService = null,
    ...workspaceOptions
  } = options;
  const localWorkspace = await createLocalWorkspace({
    rootPath: root,
    ...workspaceOptions
  });
  const workspace = observeRead
    ? {
        read(...args) {
          observeRead(...args);
          return localWorkspace.read(...args);
        },
        save(...args) {
          return localWorkspace.save(...args);
        }
      }
    : localWorkspace;
  const controller = createWorkbenchController({ workspace, compilerService });
  await controller.open({ relativePath: "demo.json", confirmDiscard: false });
  return controller;
}

function providerIdentity(generation) {
  return Object.freeze({
    generation,
    checksum: Object.freeze(Array(32).fill(generation))
  });
}

function validBindingResult() {
  return Object.freeze({
    code: 1,
    domain: 255,
    field: 65535,
    recordIndex: 0,
    subjectId: null,
    relatedId: null
  });
}

function compilerResult({
  outcome,
  generation,
  diagnostics = [],
  bindingValidation = validBindingResult(),
  packageBytes = outcome === 1 ? new Uint8Array([84, 71, 68, generation]) : null
}) {
  return Object.freeze({
    complete: true,
    outcome,
    compileStatus: outcome === 1 ? 1 : 2,
    packageError: outcome === 1 ? 0 : 17,
    identity: providerIdentity(generation),
    diagnostics: Object.freeze(diagnostics),
    bindingValidation,
    packageBytes
  });
}

function packageDiagnostic({
  code,
  severity = 1,
  section,
  field,
  recordIndex,
  subjectId,
  relatedId = null
}) {
  return Object.freeze({
    code,
    severity,
    section,
    field,
    recordIndex,
    subjectId,
    relatedId
  });
}

function createCompilerService(responses) {
  const queue = [...responses];
  let identity = providerIdentity(0);
  let closed = 0;
  const calls = [];
  return {
    calls,
    get closed() {
      return closed;
    },
    identity() {
      return providerIdentity(identity.generation);
    },
    compileAndPublish(runtime, expectedIdentity) {
      calls.push({ runtime, expectedIdentity });
      const response = queue.shift();
      if (response instanceof Error) {
        throw response;
      }
      const result =
        typeof response === "function"
          ? response(runtime, expectedIdentity)
          : response;
      if (result.outcome === 1) {
        identity = result.identity;
      }
      return result;
    },
    close() {
      closed += 1;
    }
  };
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

function contentCheckRequest(
  controller,
  expectedRevision = controller.editorState.revision
) {
  return {
    expectedRevision,
    expectedDocumentLease: controller.view().documentLease
  };
}

test("six explicit object kinds edit, preserve hidden sections, and round-trip", async (t) => {
  const root = await temporaryWorkspace(t);
  const controller = await openedController(root);
  const initial = structuredClone(controller.editorState.document);
  const edits = [
    ["player", "player.system_demo.start", "pose", "x", 101],
    ["actors", "actor.system_demo.entry.slot_a", "pose", "x", 2201],
    ["groundBlockers", "blocker.system_demo.gate", null, "minX", -2499],
    ["safePoints", "safe_point.system_demo.initial", "pose", "x", -999],
    ["interactions", "interaction.system_demo.console", "binding", "rangeMm", 1300],
    ["mechanisms", "mechanism.system_demo.gate", "pose", "x", 1301]
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
    "safe_point.system_demo.initial"
  );
  values.pose.x = "not-an-integer";

  assert.throws(
    () =>
      controller.updateObject({
        kind: "safePoints",
        id: "safe_point.system_demo.initial",
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
        id: "actor.system_demo.entry.slot_a",
        values: editableValues(
          controller.editorState.document,
          "actors",
          "actor.system_demo.entry.slot_a"
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
    "actor.system_demo.entry.slot_a"
  );
  values.pose.x += 1;
  controller.updateObject({
    kind: "actors",
    id: "actor.system_demo.entry.slot_a",
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
    "actor.system_demo.entry.slot_a"
  );
  secondValues.pose.y += 1;
  second.updateObject({
    kind: "actors",
    id: "actor.system_demo.entry.slot_a",
    values: secondValues,
    expectedRevision: 0
  });
  const external = await readFile(path.join(root, "demo.json"), "utf8");
  const changedExternal = external.replace(
    "Terminal Objective",
    "Externally Modified Terminal"
  );
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
      "player.system_demo.start"
    );
    values.pose.x += 10;
    controller.updateObject({
      kind: "player",
      id: "player.system_demo.start",
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
    "player.system_demo.start"
  );
  first.pose.x = 111;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
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
    "player.system_demo.start"
  );
  second.pose.x = 222;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
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
    "player.system_demo.start"
  );
  values.pose.x = 311;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
    values,
    expectedRevision: 0
  });
  const oldSave = controller.save({
    expectedRevision: 1,
    expectedCas: controller.view().cas
  });
  await atReplace;

  const opening = controller.open({
    relativePath: "other.json",
    confirmDiscard: true
  });
  releaseReplace();
  await oldSave;
  const opened = await opening;
  const openedCas = opened.cas;
  assert.equal(opened.relativePath, "other.json");
  assert.equal(opened.revision, 0);

  assert.equal(controller.view().relativePath, "other.json");
  assert.equal(controller.view().cas, openedCas);
  assert.equal(controller.view().conflict, false);
  assert.equal(controller.editorState.revision, 0);
  assert.equal(controller.editorState.lastError, null);

  const next = editableValues(
    controller.editorState.document,
    "player",
    "player.system_demo.start"
  );
  next.pose.x = 322;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
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
    "player.system_demo.start"
  );
  first.pose.x = 411;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
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
    "player.system_demo.start"
  );
  second.pose.x = 422;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
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

test("slash aliases wait for the pending save and keep its CAS saveable", async (t) => {
  const root = await temporaryWorkspace(t);
  await mkdir(path.join(root, "dir"));
  await copyFile(fixtureUrl, path.join(root, "dir", "demo.json"));
  let reachedReplace;
  let releaseReplace;
  const atReplace = new Promise((resolve) => {
    reachedReplace = resolve;
  });
  const release = new Promise((resolve) => {
    releaseReplace = resolve;
  });
  const readCalls = [];
  const controller = await openedController(root, {
    observeRead(relativePath) {
      readCalls.push(relativePath);
    },
    async faultInjector(name) {
      if (name === "replace") {
        reachedReplace();
        await release;
      }
    }
  });
  await controller.open({
    relativePath: "dir/demo.json",
    confirmDiscard: false
  });
  readCalls.length = 0;
  const first = editableValues(
    controller.editorState.document,
    "player",
    "player.system_demo.start"
  );
  first.pose.x = 511;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
    values: first,
    expectedRevision: 0
  });
  const pendingSave = controller.save({
    expectedRevision: 1,
    expectedCas: controller.view().cas
  });
  await atReplace;
  let openSettled = false;
  const openingAlias = controller.open({
    relativePath: "dir\\demo.json",
    confirmDiscard: true
  }).finally(() => {
    openSettled = true;
  });
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(readCalls, []);
  assert.equal(openSettled, false);
  releaseReplace();
  const completedSave = await pendingSave;
  const openedAlias = await openingAlias;

  assert.deepEqual(readCalls, ["dir\\demo.json"]);
  assert.equal(openSettled, true);
  assert.equal(openedAlias.relativePath, "dir/demo.json");
  assert.equal(openedAlias.cas, completedSave.cas);
  assert.equal(openedAlias.revision, 0);
  assert.equal(openedAlias.dirty, false);
  assert.equal(openedAlias.conflict, false);
  assert.equal(controller.editorState.document.runtime.player.pose.x, 511);
  assert.equal(controller.editorState.lastError, null);

  const second = editableValues(
    controller.editorState.document,
    "player",
    "player.system_demo.start"
  );
  second.pose.x = 522;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
    values: second,
    expectedRevision: 0
  });
  const saved = await controller.save({
    expectedRevision: 1,
    expectedCas: openedAlias.cas
  });
  assert.equal(saved.dirty, false);
  assert.equal(saved.conflict, false);
  assert.equal(controller.editorState.lastError, null);
  assert.equal(
    JSON.parse(await readFile(path.join(root, "dir", "demo.json"), "utf8")).runtime.player.pose.x,
    522
  );
});

test("a failed save releases the document-switch barrier", async (t) => {
  const root = await temporaryWorkspace(t);
  await copyFile(fixtureUrl, path.join(root, "other.json"));
  let reachedReplace;
  let releaseReplace;
  let failNextReplace = true;
  const atReplace = new Promise((resolve) => {
    reachedReplace = resolve;
  });
  const release = new Promise((resolve) => {
    releaseReplace = resolve;
  });
  const readCalls = [];
  const controller = await openedController(root, {
    observeRead(relativePath) {
      readCalls.push(relativePath);
    },
    async faultInjector(name) {
      if (name === "replace" && failNextReplace) {
        failNextReplace = false;
        reachedReplace();
        await release;
        throw new Error("injected replace failure");
      }
    }
  });
  readCalls.length = 0;
  const first = editableValues(
    controller.editorState.document,
    "player",
    "player.system_demo.start"
  );
  first.pose.x = 611;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
    values: first,
    expectedRevision: 0
  });
  let saveRejected = false;
  const failedSave = controller
    .save({
      expectedRevision: 1,
      expectedCas: controller.view().cas
    })
    .catch((error) => {
      saveRejected = true;
      return error;
    });
  await atReplace;
  let openSettled = false;
  const opening = controller.open({
    relativePath: "other.json",
    confirmDiscard: true
  }).finally(() => {
    openSettled = true;
  });
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(readCalls, []);
  assert.equal(openSettled, false);
  releaseReplace();
  const [failure, opened] = await Promise.all([failedSave, opening]);

  assert.deepEqual(readCalls, ["other.json"]);
  assert.equal(openSettled, true);
  assert.equal(saveRejected, true);
  assert.ok(failure instanceof Error);
  assert.equal(opened.relativePath, "other.json");
  assert.equal(opened.revision, 0);
  assert.equal(opened.conflict, false);
  assert.equal(controller.editorState.lastError, null);

  const second = editableValues(
    controller.editorState.document,
    "player",
    "player.system_demo.start"
  );
  second.pose.x = 622;
  controller.updateObject({
    kind: "player",
    id: "player.system_demo.start",
    values: second,
    expectedRevision: 0
  });
  const saved = await controller.save({
    expectedRevision: 1,
    expectedCas: opened.cas
  });
  assert.equal(saved.dirty, false);
  assert.equal(saved.conflict, false);
  assert.equal(
    JSON.parse(await readFile(path.join(root, "other.json"), "utf8")).runtime.player.pose.x,
    622
  );
});

test("shared diagnostics preserve author and package last-valid layers", async (t) => {
  const root = await temporaryWorkspace(t);
  const actorId = "actor.system_demo.entry.slot_a";
  const interactionId = "interaction.system_demo.console";
  const published = compilerResult({ outcome: 1, generation: 1 });
  const missingReference = compilerResult({
    outcome: 2,
    generation: 1,
    diagnostics: [
      packageDiagnostic({
        code: 21,
        section: 6,
        field: 10,
        recordIndex: 0,
        subjectId: actorId
      })
    ]
  });
  const graphAndBinding = compilerResult({
    outcome: 2,
    generation: 1,
    diagnostics: [
      packageDiagnostic({
        code: 23,
        section: 13,
        field: 26,
        recordIndex: 0,
        subjectId: "objective.system_demo.entry"
      })
    ],
    bindingValidation: Object.freeze({
      code: 8,
      domain: 1,
      field: 3,
      recordIndex: 0,
      subjectId: interactionId,
      relatedId: null
    })
  });
  const service = createCompilerService([
    published,
    missingReference,
    graphAndBinding,
    new Error("injected transport detail"),
    compilerResult({ outcome: 3, generation: 1 })
  ]);
  const controller = await openedController(root, { compilerService: service });
  assert.equal(controller.view().contentCheck.status, "idle");

  const originalDocument = controller.editorState.document;
  const originalLastValid = controller.editorState.lastValidDocument;
  const ready = controller.checkContent(contentCheckRequest(controller, 0));
  assert.equal(ready.contentCheck.status, "ready");
  assert.equal(ready.contentCheck.hasPreparedPackage, true);
  assert.deepEqual(ready.contentCheck.diagnostics, []);
  assert.strictEqual(controller.editorState.document, originalDocument);
  assert.strictEqual(controller.editorState.lastValidDocument, originalLastValid);
  assert.equal(Object.hasOwn(ready.contentCheck, "generation"), false);
  assert.equal(Object.hasOwn(ready.contentCheck, "checksum"), false);
  assert.equal(Object.hasOwn(ready.contentCheck, "packageBytes"), false);
  const publishedEvidence = controller.validatedPackageEvidence();
  assert.equal(publishedEvidence.revision, 0);
  assert.equal(publishedEvidence.generation, 1);
  assert.equal(publishedEvidence.packageBytes, 4);
  assert.match(publishedEvidence.projectionSha256, /^sha256:[0-9a-f]{64}$/);
  assert.match(publishedEvidence.packageSha256, /^sha256:[0-9a-f]{64}$/);
  assert.equal(Object.hasOwn(service.calls[0].runtime, "editor"), false);
  assert.equal(service.calls[0].runtime.packageId, "system-demo.package");
  assert.deepEqual(service.calls[0].expectedIdentity, providerIdentity(0));

  const values = editableValues(
    controller.editorState.document,
    "actors",
    actorId
  );
  values.regionId = "region.missing";
  controller.updateObject({
    kind: "actors",
    id: actorId,
    values,
    expectedRevision: 0
  });
  assert.equal(controller.view().contentCheck.status, "stale");
  const invalidAuthorDocument = controller.editorState.document;
  const invalidAuthorLastValid = controller.editorState.lastValidDocument;

  const rejected = controller.checkContent(contentCheckRequest(controller, 1));
  assert.equal(rejected.contentCheck.status, "validation_failed");
  assert.equal(rejected.contentCheck.hasPreparedPackage, true);
  assert.deepEqual(rejected.contentCheck.diagnostics, [
    {
      severity: "error",
      message: "存在指向缺失对象的引用。",
      locator: {
        group: "actors",
        stableId: actorId,
        field: "regionId"
      }
    }
  ]);
  assert.deepEqual(controller.validatedPackageEvidence(), publishedEvidence);
  assert.strictEqual(controller.editorState.document, invalidAuthorDocument);
  assert.strictEqual(
    controller.editorState.lastValidDocument,
    invalidAuthorLastValid
  );

  const ordered = controller.checkContent(contentCheckRequest(controller, 1));
  assert.equal(ordered.contentCheck.status, "validation_failed");
  assert.equal(ordered.contentCheck.diagnostics.length, 2);
  assert.equal(ordered.contentCheck.diagnostics[0].message, "依赖关系中存在循环。");
  assert.equal(ordered.contentCheck.diagnostics[0].locator, null);
  assert.deepEqual(ordered.contentCheck.diagnostics[1].locator, {
    group: "interactions",
    stableId: interactionId,
    field: "rangeMm"
  });
  const orderedDiagnostics = ordered.contentCheck.diagnostics;

  const bridgeFailure = controller.checkContent(
    contentCheckRequest(controller, 1)
  );
  assert.equal(bridgeFailure.contentCheck.status, "bridge_failed");
  assert.deepEqual(bridgeFailure.contentCheck.diagnostics, orderedDiagnostics);
  assert.deepEqual(controller.validatedPackageEvidence(), publishedEvidence);
  assert.strictEqual(controller.editorState.document, invalidAuthorDocument);
  assert.strictEqual(
    controller.editorState.lastValidDocument,
    invalidAuthorLastValid
  );

  const stale = controller.checkContent(contentCheckRequest(controller, 1));
  assert.equal(stale.contentCheck.status, "stale");
  assert.deepEqual(stale.contentCheck.diagnostics, orderedDiagnostics);
  assert.deepEqual(controller.validatedPackageEvidence(), publishedEvidence);
  assert.equal(service.calls.length, 5);
  for (const call of service.calls.slice(1)) {
    assert.deepEqual(call.expectedIdentity, providerIdentity(1));
  }
});

test("document leases reject stale and conflicted checks before compilation", async (t) => {
  const root = await temporaryWorkspace(t);
  await copyFile(fixtureUrl, path.join(root, "other.json"));
  const service = createCompilerService([
    compilerResult({ outcome: 1, generation: 1 }),
    compilerResult({ outcome: 1, generation: 2 })
  ]);
  const controller = await openedController(root, { compilerService: service });

  const firstReady = controller.checkContent(contentCheckRequest(controller, 0));
  assert.equal(firstReady.contentCheck.status, "ready");
  assert.equal(service.calls.length, 1);
  const publishedEvidence = controller.validatedPackageEvidence();

  const reloadLease = controller.view().documentLease;
  await controller.reload({ confirmDiscard: false });
  assert.equal(controller.editorState.revision, 0);
  assert.notEqual(controller.view().documentLease, reloadLease);
  const reloadedDocument = controller.editorState.document;
  const reloadedLastValid = controller.editorState.lastValidDocument;
  assert.throws(
    () =>
      controller.checkContent({
        expectedRevision: 0,
        expectedDocumentLease: reloadLease
      }),
    (error) => error.code === "stale_revision"
  );
  assert.equal(service.calls.length, 1);
  assert.deepEqual(controller.validatedPackageEvidence(), publishedEvidence);
  assert.strictEqual(controller.editorState.document, reloadedDocument);
  assert.strictEqual(controller.editorState.lastValidDocument, reloadedLastValid);

  const openLease = controller.view().documentLease;
  await controller.open({
    relativePath: "other.json",
    confirmDiscard: false
  });
  assert.equal(controller.editorState.revision, 0);
  assert.notEqual(controller.view().documentLease, openLease);
  const openedDocument = controller.editorState.document;
  const openedLastValid = controller.editorState.lastValidDocument;
  assert.throws(
    () =>
      controller.checkContent({
        expectedRevision: 0,
        expectedDocumentLease: openLease
      }),
    (error) => error.code === "stale_revision"
  );
  assert.throws(
    () => controller.checkContent({ expectedRevision: 0 }),
    (error) => error.code === "invalid_request"
  );
  assert.throws(
    () =>
      controller.checkContent({
        expectedRevision: 0,
        expectedDocumentLease: 7
      }),
    (error) => error.code === "invalid_request"
  );
  assert.equal(service.calls.length, 1);
  assert.deepEqual(controller.validatedPackageEvidence(), publishedEvidence);
  assert.strictEqual(controller.editorState.document, openedDocument);
  assert.strictEqual(controller.editorState.lastValidDocument, openedLastValid);
  assert.equal(
    Object.hasOwn(controller.view(), "runningPreviewSession"),
    false
  );

  const external = JSON.parse(
    await readFile(path.join(root, "other.json"), "utf8")
  );
  external.editor.items[0].label = "external document lease conflict";
  await writeFile(
    path.join(root, "other.json"),
    JSON.stringify(external, null, 2) + "\n",
    "utf8"
  );
  const leaseBeforeConflict = controller.view().documentLease;
  await assert.rejects(
    controller.save({
      expectedRevision: 0,
      expectedCas: controller.view().cas
    }),
    (error) => error.code === "external_change"
  );
  assert.equal(controller.view().conflict, true);
  assert.notEqual(controller.view().documentLease, leaseBeforeConflict);
  const conflictedDocument = controller.editorState.document;
  const conflictedLastValid = controller.editorState.lastValidDocument;
  assert.throws(
    () => controller.checkContent(contentCheckRequest(controller, 0)),
    (error) => error.code === "external_change"
  );
  assert.equal(service.calls.length, 1);
  assert.deepEqual(controller.validatedPackageEvidence(), publishedEvidence);
  assert.strictEqual(controller.editorState.document, conflictedDocument);
  assert.strictEqual(
    controller.editorState.lastValidDocument,
    conflictedLastValid
  );

  await controller.reload({ confirmDiscard: true });
  const recovered = controller.checkContent(contentCheckRequest(controller, 0));
  assert.equal(recovered.contentCheck.status, "ready");
  assert.equal(service.calls.length, 2);
  assert.equal(controller.validatedPackageEvidence().generation, 2);
});

test("content check fails closed for partial output and duplicate submission", async (t) => {
  const root = await temporaryWorkspace(t);
  let controller;
  let reentrantFailure = null;
  const service = createCompilerService([
    () => {
      try {
        controller.checkContent(contentCheckRequest(controller, 0));
      } catch (error) {
        reentrantFailure = error;
      }
      return compilerResult({ outcome: 1, generation: 1 });
    },
    Object.freeze({
      ...compilerResult({ outcome: 1, generation: 2 }),
      packageBytes: null
    }),
    compilerResult({
      outcome: 2,
      generation: 1,
      diagnostics: Array.from({ length: 513 }, () =>
        packageDiagnostic({
          code: 21,
          section: 6,
          field: 10,
          recordIndex: 0,
          subjectId: "actor.system_demo.entry.slot_a"
        })
      )
    })
  ]);
  controller = await openedController(root, { compilerService: service });
  controller.checkContent(contentCheckRequest(controller, 0));
  assert.equal(reentrantFailure?.code, "check_in_flight");
  assert.equal(service.calls.length, 1);
  const evidence = controller.validatedPackageEvidence();

  const missingArtifact = controller.checkContent(
    contentCheckRequest(controller, 0)
  );
  assert.equal(missingArtifact.contentCheck.status, "bridge_failed");
  assert.deepEqual(controller.validatedPackageEvidence(), evidence);

  const overPresentationCapacity = controller.checkContent(
    contentCheckRequest(controller, 0)
  );
  assert.equal(overPresentationCapacity.contentCheck.status, "bridge_failed");
  assert.deepEqual(controller.validatedPackageEvidence(), evidence);
  assert.equal(service.calls.length, 3);
});

test("server keeps CAS and package identity outside the browser DTO", async (t) => {
  const root = await temporaryWorkspace(t);
  const service = createCompilerService([
    compilerResult({ outcome: 1, generation: 1 })
  ]);
  const running = await startWorkbenchServer({
    workspaceRoot: root,
    sandboxService: service
  });
  t.after(() => running.close());
  const shell = await fetch(running.url);
  const cookie = shell.headers.get("set-cookie").split(";")[0];
  const origin = new URL(running.url).origin;
  const request = async (pathname, body) =>
    fetch(new URL(pathname, running.url), {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Cookie: cookie,
        Origin: origin
      },
      body: JSON.stringify(body)
    });

  const openedResponse = await request("/api/open", {
    relativePath: "demo.json",
    confirmDiscard: false
  });
  assert.equal(openedResponse.status, 200);
  const opened = (await openedResponse.json()).state;
  assert.equal(Object.hasOwn(opened, "cas"), false);
  assert.equal(Object.hasOwn(opened, "lastError"), false);
  assert.equal(typeof opened.documentLease, "string");

  const beforeInvalidUpdate = stateSnapshot(running.controller);
  const invalidValues = editableValues(
    running.controller.editorState.document,
    "player",
    "player.system_demo.start"
  );
  invalidValues.pose.x = 1.5;
  const invalidUpdateResponse = await request("/api/update", {
    kind: "player",
    id: "player.system_demo.start",
    values: invalidValues,
    expectedRevision: 0
  });
  assert.equal(invalidUpdateResponse.status, 422);
  const invalidUpdatePayload = await invalidUpdateResponse.json();
  const internalMessage = running.controller.editorState.lastError?.message;
  assert.equal(typeof internalMessage, "string");
  assert.equal(
    Object.hasOwn(invalidUpdatePayload.state, "lastError"),
    false
  );
  assert.deepEqual(
    Reflect.ownKeys(invalidUpdatePayload.error).sort(),
    ["code", "message"]
  );
  assert.equal(typeof invalidUpdatePayload.error.code, "string");
  assert.equal(
    invalidUpdatePayload.error.message,
    "Workbench request was not completed"
  );
  const invalidSerialized = JSON.stringify(invalidUpdatePayload);
  assert.equal(
    /lastError|JSONPath|\$\.|exception|stack/i.test(invalidSerialized),
    false
  );
  assert.equal(invalidSerialized.includes(internalMessage), false);
  assert.deepEqual(stateSnapshot(running.controller), beforeInvalidUpdate);

  const missingLeaseResponse = await request("/api/content-check", {
    expectedRevision: 0
  });
  assert.equal(missingLeaseResponse.status, 400);
  const invalidLeaseResponse = await request("/api/content-check", {
    expectedRevision: 0,
    expectedDocumentLease: 7
  });
  assert.equal(invalidLeaseResponse.status, 400);
  const extraLeaseResponse = await request("/api/content-check", {
    expectedRevision: 0,
    expectedDocumentLease: opened.documentLease,
    unexpected: true
  });
  assert.equal(extraLeaseResponse.status, 400);
  assert.equal(service.calls.length, 0);

  const checkedResponse = await request("/api/content-check", {
    expectedRevision: 0,
    expectedDocumentLease: opened.documentLease
  });
  assert.equal(checkedResponse.status, 200);
  const checked = (await checkedResponse.json()).state;
  const browserBytes = JSON.stringify(checked);
  assert.equal(Object.hasOwn(checked, "cas"), false);
  assert.equal(/generation|checksum|packageBytes|subjectKey|relatedKey/.test(browserBytes), false);
  assert.equal(checked.contentCheck.status, "ready");

  const rejectedSave = await request("/api/save", {
    expectedRevision: 0,
    expectedCas: "browser-must-not-own-this"
  });
  assert.equal(rejectedSave.status, 400);
  const saved = await request("/api/save", { expectedRevision: 0 });
  assert.equal(saved.status, 200);
  assert.equal(Object.hasOwn((await saved.json()).state, "cas"), false);
});
