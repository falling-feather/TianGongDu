import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import test from "node:test";
import vm from "node:vm";

const root = resolve(import.meta.dirname, "..");

async function loadRenderer() {
  const context = vm.createContext({ console, queueMicrotask });
  vm.runInContext(
    await readFile(resolve(root, "apps/web-shell/quest-ui-renderer.js"), "utf8"),
    context,
    { filename: "quest-ui-renderer.js" }
  );
  return context.TgdQuestUiRenderer;
}

function emptyResult() {
  return {
    id: "0000000000000000",
    objective: "0000000000000000",
    statusName: "not_applicable",
    rejectionReasonName: "none"
  };
}

function projectionEvent(renderer, {
  cueId,
  objectiveId,
  sourceName,
  surfaceName = "gameplay",
  polarityName = "positive",
  sequence = "51",
  primaryResult = emptyResult(),
  secondaryResult = emptyResult()
}) {
  return {
    sessionGeneration: 17,
    messageSequence: sequence === "51" ? "7" : "8",
    sequence,
    checksum: sequence === "51" ? "6666777788889999" : "777788889999aaaa",
    cue: renderer.stableContentKeyHex(cueId),
    objective: renderer.stableContentKeyHex(objectiveId),
    sourceName,
    surfaceName,
    polarityName,
    attemptTimeClassificationName: "qa_only_value",
    primaryResult,
    secondaryResult,
    choiceOptions: []
  };
}

class FakeElement {
  constructor(tagName, ownerDocument) {
    this.tagName = tagName.toUpperCase();
    this.ownerDocument = ownerDocument;
    this.children = [];
    this.dataset = {};
    this.attributes = new Map();
    this.listeners = new Map();
    this.hidden = false;
    this.className = "";
    this.textContent = "";
    this.disabled = false;
  }

  append(...children) {
    this.children.push(...children);
  }

  replaceChildren(...children) {
    this.children = [...children];
  }

  setAttribute(name, value) {
    this.attributes.set(name, String(value));
  }

  getAttribute(name) {
    return this.attributes.get(name) ?? null;
  }

  addEventListener(type, listener) {
    const listeners = this.listeners.get(type) ?? [];
    listeners.push(listener);
    this.listeners.set(type, listeners);
  }

  focus() {
    this.ownerDocument.activeElement = this;
    for (const listener of this.listeners.get("focus") ?? []) listener();
  }

  querySelectorAll(selector) {
    const matches = [];
    const visit = (element) => {
      const button = selector === "button" || selector === "button[data-choice-index]";
      const classSelector = selector.startsWith(".") ? selector.slice(1) : null;
      if ((button && element.tagName === "BUTTON") ||
          (classSelector && element.className.split(/\s+/).includes(classSelector))) {
        matches.push(element);
      }
      for (const child of element.children) visit(child);
    };
    for (const child of this.children) visit(child);
    return matches;
  }
}

function fakeDom() {
  const document = {
    activeElement: null,
    createElement(tagName) {
      return new FakeElement(tagName, document);
    }
  };
  return { document, root: new FakeElement("div", document) };
}

function choiceEvent(renderer, cueId, options) {
  return {
    sessionGeneration: 17,
    messageSequence: "5",
    sequence: "41",
    checksum: "5555666677778888",
    cue: renderer.stableContentKeyHex(cueId),
    objective: "1111222233334444",
    sourceName: "choice_available",
    surfaceName: "choice",
    polarityName: "positive",
    attemptTimeClassificationName: "qualifying_craft_decision",
    choiceOptions: options.map(([interaction, selection]) => ({
      interaction: renderer.stableContentKeyHex(interaction),
      selection: renderer.stableContentKeyHex(selection)
    }))
  };
}

test("正式 Quest UI renderer 覆盖抵达线索全集且保留 authored 顺序", async () => {
  const renderer = await loadRenderer();
  const event = choiceEvent(renderer, "ui.f1.rain.choice.arrival-clue", [
    ["f1_interaction_arrival_clue_high_water_tags", "f1_choice_arrival_high_water_tags"],
    ["f1_interaction_arrival_clue_drowned_manifest", "f1_choice_arrival_drowned_manifest"],
    ["f1_interaction_arrival_clue_follow_bell", "f1_choice_arrival_follow_bell"]
  ]);
  const model = renderer.createViewModel(event);
  assert.equal(model.title, "先看哪一处？");
  assert.deepEqual(model.options.map((option) => option.label), [
    "上探高水痕",
    "下探溺水货单",
    "循钟声前行"
  ]);
  assert.deepEqual(
    model.options.map((option) => option.selection),
    event.choiceOptions.map((option) => option.selection)
  );
});

test("正式 Quest UI renderer 按投影顺序生成系缆选项模型", async () => {
  const renderer = await loadRenderer();
  const event = choiceEvent(renderer, "ui.f1.rain.choice.mooring-method", [
    ["f1_interaction_choose_cross_belay", "f1_choice_mooring_cross_belay"],
    ["f1_interaction_choose_quick_hitch", "f1_choice_mooring_quick_hitch"]
  ]);
  const model = renderer.createViewModel(event);
  assert.equal(model.title, "如何稳住渡船");
  assert.equal(model.inputContext, "menu");
  assert.equal(model.options.length, 2);
  assert.equal(model.options[0].label, "交叉系缆");
  assert.equal(model.options[1].label, "快速结");
  assert.equal(model.options[0].interaction, event.choiceOptions[0].interaction);
  assert.equal(model.options[1].selection, event.choiceOptions[1].selection);
  assert.equal(model.objective, event.objective);
  assert.equal(model.sequence, event.sequence);
  assert.equal(model.checksum, event.checksum);
  assert.equal(model.messageSequence, event.messageSequence);
  assert.equal("attemptTimeClassificationName" in model, false);
});

test("正式 Quest UI renderer 覆盖训练道全集且不重排 authored selections", async () => {
  const renderer = await loadRenderer();
  const event = choiceEvent(renderer, "ui.f1.training.choice.lane", [
    ["f1_interaction_choose_training_leeward_lane", "f1_choice_training_leeward_lane"],
    ["f1_interaction_choose_training_windward_lane", "f1_choice_training_windward_lane"]
  ]);
  event.attemptTimeClassificationName = "qualifying_dialogue_decision";
  const model = renderer.createViewModel(event);
  assert.equal(model.title, "选择训练道");
  assert.equal(model.options.length, 2);
  assert.equal(model.options[0].label, "背风线");
  assert.equal(model.options[0].hint, "侧后来击；沿弧形标记调整站位");
  assert.equal(model.options[1].label, "迎风线");
  assert.equal(model.options[1].hint, "正面来击；沿楔形标记守住中线");
  assert.equal(model.options[0].selection, event.choiceOptions[0].selection);
  assert.equal(model.options[1].selection, event.choiceOptions[1].selection);
});

test("正式 Quest UI renderer 对未知 cue、缺标签和跨 surface 选项失败关闭", async () => {
  const renderer = await loadRenderer();
  const canonical = choiceEvent(renderer, "ui.f1.rain.choice.mooring-method", [
    ["f1_interaction_choose_cross_belay", "f1_choice_mooring_cross_belay"],
    ["f1_interaction_choose_quick_hitch", "f1_choice_mooring_quick_hitch"]
  ]);

  assert.throws(() => renderer.createViewModel({ ...canonical, cue: "1111111111111111" }));
  assert.throws(() => renderer.createViewModel({
    ...canonical,
    choiceOptions: [{
      interaction: "2222222222222222",
      selection: "3333333333333333"
    }]
  }));
  assert.throws(() => renderer.createViewModel({
    ...canonical,
    sourceName: "objective_state",
    surfaceName: "gameplay"
  }));
});

test("正式 renderer 只暴露 Action Registry 的导航、确认和取消语义", async () => {
  const renderer = await loadRenderer();
  assert.equal(renderer.actionIds.navigate, "ui_navigate");
  assert.equal(renderer.actionIds.confirm, "ui_confirm");
  assert.equal(renderer.actionIds.cancel, "ui_cancel");
  assert.equal(Object.keys(renderer.actionIds).length, 3);
});

test("同一训练阶段 cue 按 focus Objective 保留檐守与翻花现场语义", async () => {
  const renderer = await loadRenderer();
  const eavesguard = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.phase",
    objectiveId: "f1_objective_eavesguard_counter",
    sourceName: "objective_state"
  }));
  const flowerTurn = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.phase",
    objectiveId: "f1_objective_flower_turn_counter",
    sourceName: "objective_state",
    sequence: "52"
  }));
  assert.equal(eavesguard.title, "檐守 · 接住冲击");
  assert.match(eavesguard.prompt, /看清地面线标所示的来击方向/);
  assert.match(eavesguard.detail, /接住当前来击并保住站位/);
  assert.equal(flowerTurn.title, "翻花 · 斜向穿位");
  assert.match(flowerTurn.prompt, /越线后以翻花闪避/);
  assert.match(flowerTurn.detail, /抢到侧后位置/);
  assert.doesNotMatch(`${eavesguard.eyebrow}${eavesguard.detail}`, /qa_only_value|合格|计时/);
  assert.equal("attemptTimeClassificationName" in eavesguard, false);

  const withoutClassification = projectionEvent(renderer, {
    cueId: "ui.f1.training.phase",
    objectiveId: "f1_objective_eavesguard_counter",
    sourceName: "objective_state"
  });
  delete withoutClassification.attemptTimeClassificationName;
  assert.equal(renderer.createViewModel(withoutClassification).detail, eavesguard.detail);

  assert.throws(() => renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.phase",
    objectiveId: "f1_objective_choose_training_lane",
    sourceName: "objective_state"
  })), /expected one match/);
});

test("同一恢复 cue 按 offer/resume 与 focus Objective 区分重试和返回", async () => {
  const renderer = await loadRenderer();
  const offer = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.recovery",
    objectiveId: "f1_objective_eavesguard_counter",
    sourceName: "recovery_offer",
    surfaceName: "failure",
    polarityName: "recovery"
  }));
  const resume = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.recovery",
    objectiveId: "f1_objective_flower_turn_counter",
    sourceName: "recovery_resume",
    polarityName: "recovery",
    sequence: "52"
  }));
  assert.equal(offer.title, "回到沈砚训练点");
  assert.match(offer.prompt, /选线和取标保留/);
  assert.match(offer.detail, /训练伞架已归位/);
  assert.equal(resume.title, "训练进度已恢复");
  assert.match(resume.prompt, /只重建当前训练架/);
  assert.match(resume.detail, /不重放对话，也不改线/);
  assert.doesNotMatch(`${offer.detail}${resume.detail}`, /合格|计时|classification/);
});

test("钟声与动作证明按 raw result 呈现现场因果而非内部状态术语", async () => {
  const renderer = await loadRenderer();
  const result = (id, objectiveId, statusName, rejectionReasonName = "none") => ({
    id: renderer.stableContentKeyHex(id),
    objective: renderer.stableContentKeyHex(objectiveId),
    statusName,
    rejectionReasonName
  });
  const bellRejected = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.rain.bell-feedback",
    objectiveId: "f1_objective_sound_workshop_bell",
    sourceName: "interaction_feedback",
    polarityName: "negative",
    primaryResult: result(
      "f1_interaction_sound_workshop_bell",
      "f1_objective_sound_workshop_bell",
      "rejected",
      "prerequisite_incomplete"
    )
  }));
  const bellAccepted = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.rain.bell-feedback",
    objectiveId: "f1_objective_sound_workshop_bell",
    sourceName: "interaction_feedback",
    primaryResult: result(
      "f1_interaction_sound_workshop_bell",
      "f1_objective_sound_workshop_bell",
      "accepted"
    ),
    sequence: "52"
  }));
  assert.equal(bellRejected.title, "钟声没有回应");
  assert.match(bellRejected.detail, /刻纹仍未读/);
  assert.equal(bellAccepted.title, "门楼回应");
  assert.match(bellAccepted.prompt, /灯火依次亮起/);

  const eavesguard = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.action-proof",
    objectiveId: "f1_objective_eavesguard_counter",
    sourceName: "combat_feedback",
    primaryResult: result(
      "f1_trigger_eavesguard_counter",
      "f1_objective_eavesguard_counter",
      "accepted"
    )
  }));
  const flowerWrongTarget = renderer.createViewModel(projectionEvent(renderer, {
    cueId: "ui.f1.training.action-proof",
    objectiveId: "f1_objective_break_flower_turn_target",
    sourceName: "combat_feedback",
    polarityName: "negative",
    primaryResult: result(
      "f1_trigger_flower_turn_heavy",
      "f1_objective_commit_flower_turn_heavy",
      "accepted"
    ),
    secondaryResult: result(
      "f1_outcome_break_flower_turn_target",
      "f1_objective_break_flower_turn_target",
      "rejected",
      "wrong_target"
    ),
    sequence: "52"
  }));
  assert.equal(eavesguard.title, "檐守反制成立");
  assert.match(eavesguard.detail, /接触点与定力反馈/);
  assert.equal(flowerWrongTarget.title, "动作成立，目标未破");
  assert.match(flowerWrongTarget.detail, /重新命中正确目标/);
  assert.doesNotMatch(
    `${bellRejected.prompt}${eavesguard.prompt}${flowerWrongTarget.prompt}`,
    /快照|核对|classification|qualifying/
  );
});

test("选择提交等待权威 close，保持焦点与 busy，失败文案不泄露 ABI", async () => {
  const rendererApi = await loadRenderer();
  const { document, root } = fakeDom();
  let resolveSubmit;
  const submit = new Promise((resolve) => { resolveSubmit = resolve; });
  let restoreCount = 0;
  const diagnostics = [];
  const renderer = rendererApi.createRenderer(root, {
    submitIntent: () => submit,
    restoreFocus: () => { restoreCount += 1; },
    onDiagnostic: (diagnostic) => diagnostics.push(diagnostic)
  });
  const event = choiceEvent(rendererApi, "ui.f1.rain.choice.mooring-method", [
    ["f1_interaction_choose_cross_belay", "f1_choice_mooring_cross_belay"],
    ["f1_interaction_choose_quick_hitch", "f1_choice_mooring_quick_hitch"]
  ]);
  assert.equal(renderer.render(event), true);
  await Promise.resolve();
  const panel = root.querySelectorAll(".quest-ui-panel")[0];
  const buttons = root.querySelectorAll("button");
  buttons[1].focus();
  const submission = renderer.dispatchAction({
    id: rendererApi.actionIds.confirm,
    phase: "pressed",
    targetIndex: 1
  });
  assert.equal(panel.getAttribute("aria-busy"), "true");
  assert.equal(buttons[1].getAttribute("aria-disabled"), "true");
  assert.equal(buttons[1].disabled, false);
  assert.equal(document.activeElement, buttons[1]);
  assert.equal(renderer.dispatchAction({
    id: rendererApi.actionIds.confirm,
    phase: "pressed",
    targetIndex: 1
  }), false);
  resolveSubmit();
  assert.equal(await submission, true);
  assert.equal(root.hidden, false);
  assert.equal(root.dataset.submitState, "awaiting_authority");
  assert.equal(panel.getAttribute("aria-busy"), "true");
  assert.equal(document.activeElement, buttons[1]);

  assert.equal(renderer.acknowledgeClose({
    sessionGeneration: 17,
    messageSequence: "6",
    projectionSequence: event.sequence,
    projectionChecksum: "1111222233334444",
    reason: 1,
    reasonName: "selection_committed"
  }), false);
  assert.equal(root.hidden, false);
  assert.equal(restoreCount, 0);
  assert.equal(renderer.acknowledgeClose({
    sessionGeneration: 17,
    messageSequence: event.messageSequence,
    projectionSequence: event.sequence,
    projectionChecksum: event.checksum,
    reason: 1,
    reasonName: "selection_committed"
  }), false);
  assert.equal(root.hidden, false);
  assert.equal(restoreCount, 0);
  const close = {
    sessionGeneration: 17,
    messageSequence: "6",
    projectionSequence: event.sequence,
    projectionChecksum: event.checksum,
    reason: 1,
    reasonName: "selection_committed"
  };
  assert.equal(renderer.acknowledgeClose(close), true);
  assert.equal(root.hidden, true);
  assert.equal(restoreCount, 1);
  assert.equal(renderer.acknowledgeClose(close), false);
  assert.equal(restoreCount, 1);
  assert.deepEqual(diagnostics, []);

  const rejectedDom = fakeDom();
  const rejectedDiagnostics = [];
  const rejectedRenderer = rendererApi.createRenderer(rejectedDom.root, {
    submitIntent: () => Promise.reject(new Error("Web ABI error 9 / transport trace")),
    restoreFocus: () => {},
    onDiagnostic: (diagnostic) => rejectedDiagnostics.push(diagnostic)
  });
  rejectedRenderer.render(event);
  await Promise.resolve();
  const rejectedButtons = rejectedDom.root.querySelectorAll("button");
  rejectedButtons[0].focus();
  assert.equal(await rejectedRenderer.dispatchAction({
    id: rendererApi.actionIds.confirm,
    phase: "pressed",
    targetIndex: 0
  }), false);
  const rejectedPanel = rejectedDom.root.querySelectorAll(".quest-ui-panel")[0];
  const rejectedDetail = rejectedDom.root.querySelectorAll(".quest-ui-detail")[0];
  assert.equal(rejectedPanel.getAttribute("aria-busy"), "false");
  assert.equal(rejectedButtons[0].getAttribute("aria-disabled"), "false");
  assert.equal(rejectedDom.document.activeElement, rejectedButtons[0]);
  assert.equal(rejectedDetail.textContent, "选择尚未提交，请重试。");
  assert.doesNotMatch(rejectedDetail.textContent, /ABI|transport|error 9/i);
  assert.match(rejectedDiagnostics[0].error, /ABI error 9/);
});

test("QA classification 不进入 renderer model、Action 或 close 语义", async () => {
  const rendererApi = await loadRenderer();
  const baseEvent = choiceEvent(rendererApi, "ui.f1.rain.choice.mooring-method", [
    ["f1_interaction_choose_cross_belay", "f1_choice_mooring_cross_belay"],
    ["f1_interaction_choose_quick_hitch", "f1_choice_mooring_quick_hitch"]
  ]);

  async function exercise(event) {
    const { root } = fakeDom();
    const intents = [];
    let restoreCount = 0;
    const renderer = rendererApi.createRenderer(root, {
      submitIntent: (intent) => {
        intents.push(intent);
        return Promise.resolve();
      },
      restoreFocus: () => { restoreCount += 1; }
    });
    assert.equal(renderer.render(event), true);
    await Promise.resolve();
    assert.equal(await renderer.dispatchAction({
      id: rendererApi.actionIds.confirm,
      phase: "pressed",
      targetIndex: 1
    }), true);
    const closed = renderer.acknowledgeClose({
      sessionGeneration: event.sessionGeneration,
      messageSequence: "6",
      projectionSequence: event.sequence,
      projectionChecksum: event.checksum,
      reason: 1,
      reasonName: "selection_committed"
    });
    return {
      model: renderer.getModel(),
      intent: intents[0],
      closed,
      hidden: root.hidden,
      restoreCount
    };
  }

  const changed = structuredClone(baseEvent);
  changed.attemptTimeClassificationName = "qa_changed_value";
  const missing = structuredClone(baseEvent);
  delete missing.attemptTimeClassificationName;
  const changedResult = await exercise(changed);
  const missingResult = await exercise(missing);
  assert.equal("attemptTimeClassificationName" in changedResult.model, false);
  assert.deepEqual(changedResult.model, missingResult.model);
  assert.deepEqual(changedResult.intent, missingResult.intent);
  assert.deepEqual(
    {
      closed: changedResult.closed,
      hidden: changedResult.hidden,
      restoreCount: changedResult.restoreCount
    },
    { closed: true, hidden: true, restoreCount: 1 }
  );
  assert.deepEqual(
    {
      closed: missingResult.closed,
      hidden: missingResult.hidden,
      restoreCount: missingResult.restoreCount
    },
    { closed: true, hidden: true, restoreCount: 1 }
  );
});
