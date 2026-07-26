import assert from "node:assert/strict";
import {
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile
} from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import test from "node:test";

import { createWorkbenchController } from "../src/workbench-controller.mjs";
import { startWorkbenchServer } from "../src/workbench-server.mjs";

const fixtureUrl = new URL(
  "../../../content/design/system-demo.sandbox.json",
  import.meta.url
);

function providerIdentity(generation) {
  return Object.freeze({
    generation,
    checksum: Object.freeze(Array(32).fill(generation))
  });
}

function compilerResult(generation) {
  return Object.freeze({
    complete: true,
    outcome: 1,
    compileStatus: 1,
    packageError: 0,
    identity: providerIdentity(generation),
    diagnostics: Object.freeze([]),
    bindingValidation: Object.freeze({
      code: 1,
      domain: 255,
      field: 65535,
      recordIndex: 0,
      subjectId: null,
      relatedId: null
    }),
    packageBytes: Uint8Array.of(84, 71, 68, generation)
  });
}

function compilerService() {
  let generation = 0;
  return {
    identity() {
      return providerIdentity(generation);
    },
    compileAndPublish() {
      generation += 1;
      return compilerResult(generation);
    }
  };
}

async function openedController(options = {}) {
  let text = await readFile(fixtureUrl, "utf8");
  const workspace = {
    async read() {
      return {
        relativePath: "system-demo.sandbox.json",
        cas: "cas-1",
        text
      };
    },
    async save({ text: nextText }) {
      text = nextText;
      return { cas: "cas-2" };
    }
  };
  const controller = createWorkbenchController({ workspace, ...options });
  await controller.open({
    relativePath: "system-demo.sandbox.json",
    confirmDiscard: false
  });
  return controller;
}

test("object creation owns IDs, bindings, wave membership, and editor labels", async () => {
  const controller = await openedController();
  controller.createObject({
    kind: "actors",
    id: "actor.system_demo.entry.slot_c",
    label: "Entry Slot C",
    sourceId: "actor.system_demo.entry.slot_a",
    mode: "duplicate",
    expectedRevision: 0
  });

  const actor = controller.view().document.runtime.actors.find(
    ({ id }) => id === "actor.system_demo.entry.slot_c"
  );
  assert.ok(actor);
  assert.equal(actor.pose.x, -650);
  assert.deepEqual(
    controller.view().document.runtime.waveSpawns.find(
      ({ actorId }) => actorId === actor.id
    ),
    {
      waveId: "wave.system_demo.entry",
      actorId: actor.id,
      delayTicks: 0,
      spawnOrder: 2
    }
  );
  assert.deepEqual(
    controller.view().document.runtime.actorBindings.find(
      ({ actorId }) => actorId === actor.id
    ),
    {
      actorId: actor.id,
      profileId: "jn_enemy_leaking_umbrella_doll",
      faction: "hostile",
      duty: "pressure",
      maxHealth: 54
    }
  );
  assert.equal(
    controller.view().document.editor.items.find(({ id }) => id === actor.id)
      ?.label,
    "Entry Slot C"
  );

  controller.createObject({
    kind: "interactions",
    id: "interaction.system_demo.console.copy",
    label: "Gate Console Copy",
    sourceId: "interaction.system_demo.console",
    mode: "duplicate",
    expectedRevision: 1
  });
  assert.deepEqual(
    controller.view().document.runtime.interactionBindings.find(
      ({ interactionId }) =>
        interactionId === "interaction.system_demo.console.copy"
    ),
    {
      interactionId: "interaction.system_demo.console.copy",
      operation: "operate",
      rangeMm: 1200,
      targetMechanismId: "mechanism.system_demo.gate"
    }
  );
  assert.equal(controller.view().revision, 2);
  assert.equal(controller.view().dirty, true);
});

test("duplicate IDs, required player, and referenced safe point fail closed", async () => {
  const controller = await openedController();
  assert.throws(
    () =>
      controller.createObject({
        kind: "actors",
        id: "actor.system_demo.entry.slot_a",
        label: "Duplicate",
        sourceId: "actor.system_demo.entry.slot_a",
        mode: "duplicate",
        expectedRevision: 0
      }),
    (error) => error.code === "duplicate_id" && error.status === 409
  );
  assert.throws(
    () =>
      controller.deleteObject({
        kind: "player",
        id: "player.system_demo.start",
        expectedRevision: 0
      }),
    (error) => error.code === "required_object" && error.status === 409
  );
  assert.throws(
    () =>
      controller.deleteObject({
        kind: "safePoints",
        id: "safe_point.system_demo.initial",
        expectedRevision: 0
      }),
    (error) => error.code === "object_referenced" && error.status === 409
  );
  assert.equal(controller.view().revision, 0);
  assert.equal(controller.view().dirty, false);
});

test("player replacement and dependent delete operations stay atomic", async () => {
  const controller = await openedController();
  controller.createObject({
    kind: "player",
    id: "player.system_demo.rebuilt",
    label: "Rebuilt Player",
    sourceId: "player.system_demo.start",
    mode: "duplicate",
    expectedRevision: 0
  });
  assert.equal(
    controller.view().document.runtime.player.id,
    "player.system_demo.rebuilt"
  );
  assert.equal(
    controller.view().document.editor.items.some(
      ({ id }) => id === "player.system_demo.start"
    ),
    false
  );

  controller.deleteObject({
    kind: "actors",
    id: "actor.system_demo.entry.slot_a",
    expectedRevision: 1
  });
  assert.equal(
    controller.view().document.runtime.waveSpawns.some(
      ({ actorId }) => actorId === "actor.system_demo.entry.slot_a"
    ),
    false
  );
  assert.equal(
    controller.view().document.runtime.actorBindings.some(
      ({ actorId }) => actorId === "actor.system_demo.entry.slot_a"
    ),
    false
  );

  controller.deleteObject({
    kind: "interactions",
    id: "interaction.system_demo.console",
    expectedRevision: 2
  });
  assert.equal(
    controller.view().document.runtime.interactionBindings.some(
      ({ interactionId }) =>
        interactionId === "interaction.system_demo.console"
    ),
    false
  );
  assert.equal(
    controller.view().document.runtime.interactionBindings.length,
    1
  );
  assert.equal(controller.view().revision, 3);
});

test("editor label changes remain editor-only and round-trip through update", async () => {
  const controller = await openedController();
  const actor = controller.view().document.runtime.actors[0];
  controller.updateObject({
    kind: "actors",
    id: actor.id,
    values: {
      regionId: actor.regionId,
      assetId: actor.assetId,
      pose: actor.pose,
      facingMillidegrees: actor.facingMillidegrees,
      editorLabel: "入口压迫位"
    },
    expectedRevision: 0
  });
  assert.equal(
    controller.view().document.editor.items.find(({ id }) => id === actor.id)
      ?.label,
    "入口压迫位"
  );
  assert.deepEqual(
    controller.view().document.runtime.actors.find(({ id }) => id === actor.id),
    actor
  );
});

test("actor gameplay bindings and authored wave/objective panels update atomically", async () => {
  const controller = await openedController();
  const opened = controller.view().document;
  const actor = opened.runtime.actors[0];

  controller.updateObject({
    kind: "actors",
    id: actor.id,
    values: {
      regionId: actor.regionId,
      assetId: actor.assetId,
      pose: actor.pose,
      facingMillidegrees: actor.facingMillidegrees,
      binding: {
        profileId: "jn_enemy_towline_water_hand",
        faction: "hostile",
        duty: "controller",
        maxHealth: 91
      }
    },
    expectedRevision: 0
  });
  assert.deepEqual(
    controller.view().document.runtime.actorBindings.find(
      ({ actorId }) => actorId === actor.id
    ),
    {
      actorId: actor.id,
      profileId: "jn_enemy_towline_water_hand",
      faction: "hostile",
      duty: "controller",
      maxHealth: 91
    }
  );

  const entryWave = controller
    .view()
    .document.runtime.waves.find(({ id }) => id === "wave.system_demo.entry");
  const entrySpawns = controller
    .view()
    .document.runtime.waveSpawns.filter(
      ({ waveId }) => waveId === entryWave.id
    );
  controller.updateObject({
    kind: "waves",
    id: entryWave.id,
    values: {
      regionId: entryWave.regionId,
      predecessorWaveId: entryWave.predecessorWaveId,
      trigger: entryWave.trigger,
      spawns: entrySpawns.map((spawn, index) => ({
        actorId: spawn.actorId,
        delayTicks: 6 + index,
        spawnOrder: spawn.spawnOrder
      }))
    },
    expectedRevision: 1
  });
  assert.deepEqual(
    controller
      .view()
      .document.runtime.waveSpawns.filter(
        ({ waveId }) => waveId === entryWave.id
      )
      .map(({ delayTicks }) => delayTicks),
    [6, 7]
  );

  const terminal = controller
    .view()
    .document.runtime.objectives.find(
      ({ id }) => id === "objective.system_demo.terminal"
    );
  controller.updateObject({
    kind: "objectives",
    id: terminal.id,
    values: {
      regionId: terminal.regionId,
      predecessorObjectiveId: terminal.predecessorObjectiveId,
      completion: {
        kind: "wave_completed",
        targetId: "wave.system_demo.entry"
      },
      terminal: true
    },
    expectedRevision: 2
  });
  assert.equal(
    controller.view().document.runtime.completionObjectiveId,
    terminal.id
  );
  assert.equal(controller.view().revision, 3);

  assert.throws(
    () =>
      controller.updateObject({
        kind: "waves",
        id: entryWave.id,
        values: {
          regionId: entryWave.regionId,
          predecessorWaveId: entryWave.predecessorWaveId,
          trigger: entryWave.trigger,
          spawns: entrySpawns,
          unsupportedCreate: true
        },
        expectedRevision: 3
      }),
    (error) => error.code === "invalid_request"
  );
  assert.equal(controller.view().revision, 3);
});

test("craft material, workstation, process, and step panels update one valid authoring document", async () => {
  const controller = await openedController();
  const material = controller.view().document.runtime.craftMaterials[0];
  controller.updateObject({
    kind: "craftMaterials",
    id: material.id,
    values: { editorLabel: "Flexible Trial Patch" },
    expectedRevision: 0
  });
  assert.equal(
    controller.view().document.editor.items.find(({ id }) => id === material.id)
      ?.label,
    "Flexible Trial Patch"
  );

  const workstation = controller.view().document.runtime.craftWorkstations[0];
  controller.updateObject({
    kind: "craftWorkstations",
    id: workstation.id,
    values: {
      regionId: workstation.regionId,
      assetId: workstation.assetId,
      pose: { ...workstation.pose, x: workstation.pose.x - 100 },
      facingMillidegrees: 180000,
      editorLabel: "Canopy Tuning Bench"
    },
    expectedRevision: 1
  });
  assert.equal(
    controller.view().document.runtime.craftWorkstations[0].pose.x,
    workstation.pose.x - 100
  );

  const process = controller.view().document.runtime.craftProcesses[0];
  const choices = controller.view().document.runtime.craftMaterialChoices
    .filter(({ processId }) => processId === process.id)
    .map(({ materialId, outcome }) => ({ materialId, outcome }));
  controller.updateObject({
    kind: "craftProcesses",
    id: process.id,
    values: {
      workstationId: process.workstationId,
      needId: "need.jiangnan.umbrella.canopy_tension.preview",
      outputItemId: process.outputItemId,
      trialStepId: process.trialStepId,
      materialChoices: choices,
      editorLabel: "Canopy Tuning Preview"
    },
    expectedRevision: 2
  });
  assert.deepEqual(
    controller.view().document.runtime.craftMaterialChoices
      .filter(({ processId }) => processId === process.id)
      .map(({ materialId, outcome }) => ({ materialId, outcome })),
    choices
  );

  const step = controller.view().document.runtime.craftSteps[0];
  controller.updateObject({
    kind: "craftSteps",
    id: step.id,
    values: {
      processId: step.processId,
      predecessorStepId: step.predecessorStepId,
      actionId: "craft_action.umbrella.align_rib.preview",
      kind: step.kind,
      editorLabel: "Align Ribs Preview"
    },
    expectedRevision: 3
  });
  assert.equal(
    controller.view().document.runtime.craftSteps[0].actionId,
    "craft_action.umbrella.align_rib.preview"
  );
  assert.equal(controller.view().revision, 4);

  assert.throws(
    () =>
      controller.updateObject({
        kind: "craftProcesses",
        id: process.id,
        values: {
          workstationId: process.workstationId,
          needId: process.needId,
          outputItemId: process.outputItemId,
          trialStepId: process.trialStepId,
          materialChoices: choices,
          unsupportedCreate: true
        },
        expectedRevision: 4
      }),
    (error) => error.code === "invalid_request"
  );
  assert.equal(controller.view().revision, 4);
});

test("workshop stock and order panels update the economy and consequence atomically", async () => {
  const controller = await openedController();
  const opened = controller.view().document;
  const workshop = opened.runtime.workshops[0];
  const stocks = opened.runtime.workshopMaterialStocks
    .filter(({ workshopId }) => workshopId === workshop.id)
    .map(
      ({
        materialId,
        unitCost,
        initialQuantity,
        baseQuality,
        reworkQualityGain
      }) => ({
        materialId,
        unitCost,
        initialQuantity,
        baseQuality,
        reworkQualityGain
      })
    );

  controller.updateObject({
    kind: "workshops",
    id: workshop.id,
    values: {
      workstationId: workshop.workstationId,
      initialFunds: 110,
      materialStocks: stocks.map((stock, index) => ({
        ...stock,
        initialQuantity: stock.initialQuantity + index + 1
      })),
      editorLabel: "Umbrella Lane Workshop Preview"
    },
    expectedRevision: 0
  });
  assert.equal(
    controller.view().document.runtime.workshops[0].initialFunds,
    110
  );
  assert.deepEqual(
    controller.view().document.runtime.workshopMaterialStocks.map(
      ({ initialQuantity }) => initialQuantity
    ),
    [2, 4]
  );

  const order = controller.view().document.runtime.workshopOrders[0];
  controller.updateObject({
    kind: "workshopOrders",
    id: order.id,
    values: {
      workshopId: order.workshopId,
      processId: order.processId,
      requiredQuantity: order.requiredQuantity,
      minimumQuality: order.minimumQuality,
      rewardFunds: 30,
      consequenceKind: order.consequenceKind,
      consequenceTargetId: order.consequenceTargetId,
      editorLabel: "Roof Shortcut Order Preview"
    },
    expectedRevision: 1
  });
  assert.equal(
    controller.view().document.runtime.workshopOrders[0].rewardFunds,
    30
  );
  assert.equal(controller.view().revision, 2);

  assert.throws(
    () =>
      controller.updateObject({
        kind: "workshops",
        id: workshop.id,
        values: {
          workstationId: workshop.workstationId,
          initialFunds: 110,
          materialStocks: "not-an-array"
        },
        expectedRevision: 2
      }),
    (error) => error.code === "invalid_request"
  );
  assert.equal(controller.view().revision, 2);
  assert.deepEqual(
    controller.view().document.runtime.workshopMaterialStocks.map(
      ({ initialQuantity }) => initialQuantity
    ),
    [2, 4]
  );
});

test("Preview publication owns a fresh package and stale edits preserve the last publication", async () => {
  const controller = await openedController({
    compilerService: compilerService(),
    previewAvailable: true
  });
  const initial = controller.view();
  controller.checkContent({
    expectedRevision: initial.revision,
    expectedDocumentLease: initial.documentLease
  });
  const ready = controller.view();
  controller.publishPreview({
    expectedRevision: ready.revision,
    expectedDocumentLease: ready.documentLease,
    expectedPreparedPackageLease:
      ready.contentCheck.preparedPackageLease
  });
  const published = controller.view().preview.publication;
  assert.equal(published.generation, 1);
  assert.equal(published.packageBytes, 4);
  assert.match(
    published.url,
    /^\/preview\/tiangongdu-system-demo\.html\?workbenchPreview=/
  );
  assert.deepEqual(
    controller.previewPackage(published.token).bytes,
    Uint8Array.of(84, 71, 68, 1)
  );

  const actor = controller.view().document.runtime.actors[0];
  controller.updateObject({
    kind: "actors",
    id: actor.id,
    values: {
      regionId: actor.regionId,
      assetId: actor.assetId,
      pose: { ...actor.pose, x: actor.pose.x + 100 },
      facingMillidegrees: actor.facingMillidegrees
    },
    expectedRevision: controller.view().revision
  });
  assert.equal(controller.view().contentCheck.status, "stale");
  assert.throws(
    () =>
      controller.publishPreview({
        expectedRevision: controller.view().revision,
        expectedDocumentLease: controller.view().documentLease,
        expectedPreparedPackageLease:
          ready.contentCheck.preparedPackageLease
      }),
    (error) =>
      (error.code === "package_not_ready" || error.code === "stale_revision") &&
      error.status === 409
  );
  assert.deepEqual(controller.view().preview.publication, published);
  assert.deepEqual(
    controller.previewPackage(published.token).bytes,
    Uint8Array.of(84, 71, 68, 1)
  );
});

test("Preview HTTP routes stay session-bound and expose only immutable published bytes", async (t) => {
  const root = await mkdtemp(path.join(tmpdir(), "tgd-preview-server-"));
  const previewRoot = path.join(root, "host");
  await mkdir(previewRoot);
  await copyFile(fixtureUrl, path.join(root, "demo.json"));
  await Promise.all([
    writeFile(
      path.join(previewRoot, "tiangongdu-system-demo.html"),
      "<!doctype html><title>preview host</title>",
      "utf8"
    ),
    writeFile(
      path.join(previewRoot, "tiangongdu-system-demo.js"),
      "globalThis.previewHostLoaded = true;",
      "utf8"
    ),
    writeFile(
      path.join(previewRoot, "tiangongdu-system-demo.wasm"),
      Uint8Array.of(0, 97, 115, 109)
    )
  ]);
  const running = await startWorkbenchServer({
    workspaceRoot: root,
    sandboxService: compilerService(),
    systemDemoWebRoot: previewRoot
  });
  t.after(async () => {
    await running.close();
    await rm(root, { recursive: true, force: true });
  });

  const landing = await fetch(running.url);
  assert.equal(landing.status, 200);
  const cookie = landing.headers.get("set-cookie")?.split(";")[0];
  assert.match(cookie, /^tgd_workbench_session=/);
  const origin = new URL(running.url).origin;
  const request = (url, options = {}) =>
    fetch(new URL(url, running.url), {
      ...options,
      headers: {
        Cookie: cookie,
        ...(options.body
          ? { "Content-Type": "application/json", Origin: origin }
          : {}),
        ...options.headers
      }
    });

  assert.equal(
    (await fetch(new URL("/preview/tiangongdu-system-demo.html", running.url)))
      .status,
    403
  );
  let response = await request("/api/open", {
    method: "POST",
    body: JSON.stringify({
      relativePath: "demo.json",
      confirmDiscard: false
    })
  });
  let state = (await response.json()).state;
  assert.equal(state.preview.available, true);
  response = await request("/api/content-check", {
    method: "POST",
    body: JSON.stringify({
      expectedRevision: state.revision,
      expectedDocumentLease: state.documentLease
    })
  });
  state = (await response.json()).state;
  response = await request("/api/preview-publish", {
    method: "POST",
    body: JSON.stringify({
      expectedRevision: state.revision,
      expectedDocumentLease: state.documentLease,
      expectedPreparedPackageLease:
        state.contentCheck.preparedPackageLease
    })
  });
  state = (await response.json()).state;
  assert.equal(state.preview.publication.packageBytes, 4);
  assert.equal(JSON.stringify(state).includes('"bytes"'), false);

  const previewPage = await request(state.preview.publication.url);
  assert.equal(previewPage.status, 200);
  assert.match(
    previewPage.headers.get("content-security-policy"),
    /frame-ancestors 'self'/
  );
  assert.equal(previewPage.headers.get("x-frame-options"), "SAMEORIGIN");
  assert.match(await previewPage.text(), /preview host/);

  const packageResponse = await request(
    "/api/preview-package?token=" + state.preview.publication.token
  );
  assert.equal(packageResponse.status, 200);
  assert.equal(
    packageResponse.headers.get("x-tgd-preview-generation"),
    "1"
  );
  assert.deepEqual(
    new Uint8Array(await packageResponse.arrayBuffer()),
    Uint8Array.of(84, 71, 68, 1)
  );
  assert.equal(
    (
      await request(
        "/api/preview-package?token=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      )
    ).status,
    404
  );
});
