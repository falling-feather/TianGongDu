(() => {
  "use strict";

  const FNV_OFFSET = 14_695_981_039_346_656_037n;
  const FNV_PRIME = 1_099_511_628_211n;
  const MAX_U64 = (1n << 64n) - 1n;
  const actionIds = Object.freeze({
    navigate: "ui_navigate",
    confirm: "ui_confirm",
    cancel: "ui_cancel"
  });

  function stableContentKeyHex(name) {
    if (typeof name !== "string" || !/^[a-z0-9._-]+$/.test(name)) {
      throw new TypeError("Stable presentation IDs must use the canonical ASCII alphabet");
    }
    let hash = FNV_OFFSET;
    for (let index = 0; index < name.length; index += 1) {
      hash ^= BigInt(name.charCodeAt(index));
      hash = (hash * FNV_PRIME) & MAX_U64;
    }
    return hash.toString(16).padStart(16, "0");
  }

  function optionCatalog(entries) {
    return Object.freeze(Object.fromEntries(entries.map(([selectionId, label, hint]) => [
      stableContentKeyHex(selectionId),
      Object.freeze({ label, hint })
    ])));
  }

  function presentationVariant(selector, copy) {
    const normalizedSelector = {
      sourceName: selector.sourceName ?? null,
      polarityName: selector.polarityName ?? null,
      objective: selector.objectiveId ? stableContentKeyHex(selector.objectiveId) : null,
      primaryId: selector.primaryId ? stableContentKeyHex(selector.primaryId) : null,
      primaryStatusName: selector.primaryStatusName ?? null,
      primaryRejectionReasonName: selector.primaryRejectionReasonName ?? null,
      secondaryId: selector.secondaryId ? stableContentKeyHex(selector.secondaryId) : null,
      secondaryStatusName: selector.secondaryStatusName ?? null,
      secondaryRejectionReasonName: selector.secondaryRejectionReasonName ?? null
    };
    return Object.freeze({
      selector: Object.freeze(normalizedSelector),
      copy: Object.freeze({ ...copy })
    });
  }

  function presentationVariantMatches(event, selector) {
    return (selector.sourceName === null || selector.sourceName === event.sourceName) &&
      (selector.polarityName === null || selector.polarityName === event.polarityName) &&
      (selector.objective === null || selector.objective === event.objective) &&
      (selector.primaryId === null || selector.primaryId === event.primaryResult?.id) &&
      (selector.primaryStatusName === null ||
        selector.primaryStatusName === event.primaryResult?.statusName) &&
      (selector.primaryRejectionReasonName === null ||
        selector.primaryRejectionReasonName === event.primaryResult?.rejectionReasonName) &&
      (selector.secondaryId === null || selector.secondaryId === event.secondaryResult?.id) &&
      (selector.secondaryStatusName === null ||
        selector.secondaryStatusName === event.secondaryResult?.statusName) &&
      (selector.secondaryRejectionReasonName === null ||
        selector.secondaryRejectionReasonName === event.secondaryResult?.rejectionReasonName);
  }

  function resolvePresentationCopy(event, presentation) {
    if (!presentation.variants) return presentation;
    const matches = presentation.variants.filter(({ selector }) =>
      presentationVariantMatches(event, selector)
    );
    if (matches.length !== 1) {
      throw new TypeError(
        `Quest UI Presentation selector expected one match for cue ${event.cue}, got ${matches.length}`
      );
    }
    return Object.freeze({ ...presentation, ...matches[0].copy });
  }

  const catalogEntries = [
    {
      cueId: "ui.f1.rain.choice.arrival-clue",
      eyebrow: "雨渡 · 抵达线索",
      title: "先看哪一处？",
      prompt: "三条观察路线都会回到渡船主线；选择只提交观察意图。",
      positive: "抵达线索已确认。",
      negative: "原选择保持，不重复推进。",
      options: optionCatalog([
        ["f1_choice_arrival_high_water_tags", "上探高水痕", "高侧补丁与水签"],
        ["f1_choice_arrival_drowned_manifest", "下探溺水货单", "低侧积水与漂浮货单"],
        ["f1_choice_arrival_follow_bell", "循钟声前行", "留在中轴读取主水尺"]
      ])
    },
    {
      cueId: "ui.f1.rain.choice.mooring-method",
      eyebrow: "雨渡 · 系泊",
      title: "如何稳住渡船",
      prompt: "选择交叉系缆分担负载，或先打快速结再观察受力。",
      positive: "系泊方法已提交。",
      negative: "当前系泊方法需要纠正。",
      options: optionCatalog([
        ["f1_choice_mooring_cross_belay", "交叉系缆", "连接高低锚，分担两侧负载"],
        ["f1_choice_mooring_quick_hitch", "快速结", "先固定低锚，再读取并纠正受力"]
      ])
    },
    {
      cueId: "ui.f1.rain.mooring-load",
      eyebrow: "雨渡 · 系泊反馈",
      title: "系泊受力",
      prompt: "读取当前系泊反馈。",
      positive: "系缆负载稳定。",
      negative: "快速结已接受，但负载过高；按现场反馈继续纠正。",
      options: Object.freeze({})
    },
    {
      cueId: "ui.f1.rain.bell-feedback",
      eyebrow: "雨渡 · 工坊钟",
      title: "工坊钟反馈",
      prompt: "听清门楼是否回应当前钟码。",
      positive: "工坊钟已按正确顺序回应。",
      negative: "前置线索尚未完成，钟声没有推进任务。",
      options: Object.freeze({}),
      variants: Object.freeze([
        presentationVariant({
          sourceName: "interaction_feedback",
          objectiveId: "f1_objective_sound_workshop_bell",
          primaryId: "f1_interaction_sound_workshop_bell",
          primaryStatusName: "rejected",
          primaryRejectionReasonName: "prerequisite_incomplete"
        }, {
          title: "钟声没有回应",
          prompt: "门楼没有回应；先读钟架上的工位钟码。",
          detail: "钟架刻纹仍未读，当前鸣钟顺序无效。"
        }),
        presentationVariant({
          sourceName: "interaction_feedback",
          objectiveId: "f1_objective_sound_workshop_bell",
          primaryId: "f1_interaction_sound_workshop_bell",
          primaryStatusName: "accepted"
        }, {
          title: "门楼回应",
          prompt: "钟码正确，门楼灯火依次亮起。",
          detail: "连续灯形从渡船延伸到门楼，可以继续前行。"
        })
      ])
    },
    {
      cueId: "ui.f1.training.choice.lane",
      eyebrow: "沈砚训练 · 选线",
      title: "选择训练道",
      prompt: "路线改变来击方向与取标空间，不改变必须掌握的两种架势。",
      positive: "训练道已提交。",
      negative: "训练道选择未被接受。",
      options: optionCatalog([
        ["f1_choice_training_windward_lane", "迎风线", "正面来击；沿楔形标记守住中线"],
        ["f1_choice_training_leeward_lane", "背风线", "侧后来击；沿弧形标记调整站位"]
      ])
    },
    {
      cueId: "ui.f1.training.phase",
      eyebrow: "沈砚训练 · 当前阶段",
      title: "训练阶段",
      prompt: "看清来击方向，再完成当前架势。",
      positive: "当前训练阶段已更新。",
      negative: "训练阶段没有推进。",
      options: Object.freeze({}),
      variants: Object.freeze([
        presentationVariant({
          sourceName: "objective_state",
          objectiveId: "f1_objective_eavesguard_counter"
        }, {
          title: "檐守 · 接住冲击",
          prompt: "伞架开始蓄力；看清地面线标所示的来击方向，再以檐守接住冲击。",
          detail: "檐守用于接住当前来击并保住站位。"
        }),
        presentationVariant({
          sourceName: "objective_state",
          objectiveId: "f1_objective_flower_turn_counter"
        }, {
          title: "翻花 · 斜向穿位",
          prompt: "高位训练架开始掠过；沿地面线标越线后以翻花闪避。",
          detail: "翻花用于穿过斜向威胁并抢到侧后位置。"
        })
      ])
    },
    {
      cueId: "ui.f1.training.action-proof",
      eyebrow: "沈砚训练 · 动作反馈",
      title: "动作证明",
      prompt: "看清动作是否接住来击，以及指定证明靶是否被击破。",
      positive: "动作证明已接受。",
      negative: "动作已触发，但目标结果未通过。",
      options: Object.freeze({}),
      variants: Object.freeze([
        presentationVariant({
          sourceName: "combat_feedback",
          objectiveId: "f1_objective_eavesguard_counter",
          primaryId: "f1_trigger_eavesguard_counter",
          primaryStatusName: "accepted",
          secondaryStatusName: "not_applicable"
        }, {
          title: "檐守反制成立",
          prompt: "檐守成功接住伞架冲击；下一步用重击把力量送回去。",
          detail: "格挡接触点与定力反馈同时确认动作有效。"
        }),
        presentationVariant({
          sourceName: "combat_feedback",
          objectiveId: "f1_objective_break_flower_turn_target",
          primaryId: "f1_trigger_flower_turn_heavy",
          primaryStatusName: "accepted",
          secondaryId: "f1_outcome_break_flower_turn_target",
          secondaryStatusName: "rejected",
          secondaryRejectionReasonName: "wrong_target"
        }, {
          title: "动作成立，目标未破",
          prompt: "翻花重击已经起手，但没有命中指定证明靶。",
          detail: "保留已完成的轻重击组合，只需重新命中正确目标。"
        })
      ])
    },
    {
      cueId: "ui.f1.training.recovery",
      eyebrow: "沈砚训练 · 安全点",
      title: "训练恢复",
      prompt: "恢复会保留已确认选择与已完成目标。",
      positive: "从训练安全点恢复，未重复推进。",
      negative: "本次失败尝试已清除，可以从安全点再试。",
      options: Object.freeze({}),
      variants: Object.freeze([
        presentationVariant({
          sourceName: "recovery_offer",
          objectiveId: "f1_objective_eavesguard_counter"
        }, {
          title: "回到沈砚训练点",
          prompt: "已完成的对话、选线和取标保留；从檐守格挡继续。",
          detail: "训练伞架已归位，可以立即再试。"
        }),
        presentationVariant({
          sourceName: "recovery_resume",
          objectiveId: "f1_objective_flower_turn_counter"
        }, {
          title: "训练进度已恢复",
          prompt: "已选训练线和完成的翻花越线保持，只重建当前训练架。",
          detail: "从当前翻花阶段继续，不重放对话，也不改线。"
        })
      ])
    }
  ];

  const defaultCatalog = Object.freeze(Object.fromEntries(catalogEntries.map((entry) => [
    stableContentKeyHex(entry.cueId),
    Object.freeze({ ...entry, options: entry.options })
  ])));

  function requireHexKey(value, label, { allowZero = false } = {}) {
    if (typeof value !== "string" || !/^[0-9a-f]{16}$/.test(value) ||
        (!allowZero && value === "0000000000000000")) {
      throw new TypeError(`${label} must be a canonical StableContentKey`);
    }
    return value;
  }

  function createViewModel(event, catalog = defaultCatalog) {
    if (!event || typeof event !== "object") {
      throw new TypeError("Quest UI renderer requires a decoded projection event");
    }
    const sequence = typeof event.sequence === "string" && /^[1-9][0-9]*$/.test(event.sequence)
      ? event.sequence
      : null;
    const messageSequence = typeof event.messageSequence === "string" &&
      /^[1-9][0-9]*$/.test(event.messageSequence) ? event.messageSequence : null;
    if (!sequence || !messageSequence ||
        !Number.isInteger(event.sessionGeneration) || event.sessionGeneration <= 0) {
      throw new TypeError("Quest UI projection identity is invalid");
    }
    const checksum = requireHexKey(event.checksum, "projection checksum");
    const cue = requireHexKey(event.cue, "cue");
    const objective = requireHexKey(event.objective, "objective");
    const catalogEntry = catalog[cue];
    if (!catalogEntry) {
      throw new TypeError(`No formal Presentation catalog entry exists for cue ${cue}`);
    }
    const presentation = resolvePresentationCopy(event, catalogEntry);
    if (!Array.isArray(event.choiceOptions)) {
      throw new TypeError("Quest UI choice options must be an array");
    }
    const choice = event.sourceName === "choice_available" && event.surfaceName === "choice";
    if (choice !== (event.choiceOptions.length > 0)) {
      throw new TypeError("Quest UI choice surface and authored option set drifted");
    }
    const options = event.choiceOptions.map((option) => {
      const interaction = requireHexKey(option.interaction, "choice interaction");
      const selection = requireHexKey(option.selection, "choice selection");
      const label = presentation.options[selection];
      if (!label) {
        throw new TypeError(`Choice selection ${selection} has no Presentation label`);
      }
      return Object.freeze({ interaction, selection, label: label.label, hint: label.hint });
    });
    const recovery = event.polarityName === "recovery";
    const detail = presentation.detail ?? (recovery
      ? event.sourceName === "recovery_offer" ? presentation.negative : presentation.positive
      : event.polarityName === "negative" ? presentation.negative : presentation.positive);
    return Object.freeze({
      sessionGeneration: event.sessionGeneration,
      messageSequence,
      sequence,
      checksum,
      objective,
      cue,
      sourceName: event.sourceName,
      surfaceName: event.surfaceName,
      polarityName: event.polarityName,
      eyebrow: presentation.eyebrow,
      title: presentation.title,
      prompt: presentation.prompt,
      detail,
      inputContext: choice ? "menu" : "gameplay",
      options: Object.freeze(options)
    });
  }

  function createRenderer(root, options = {}) {
    if (!root || typeof root.replaceChildren !== "function" || !root.ownerDocument) {
      throw new TypeError("Quest UI renderer requires a DOM root");
    }
    if (typeof options.submitIntent !== "function") {
      throw new TypeError("Quest UI renderer requires an intent submission boundary");
    }
    const document = root.ownerDocument;
    const panel = document.createElement("section");
    panel.className = "quest-ui-panel";
    panel.setAttribute("role", "dialog");
    panel.setAttribute("aria-modal", "false");
    panel.setAttribute("aria-busy", "false");
    panel.tabIndex = -1;
    const eyebrow = document.createElement("p");
    eyebrow.className = "quest-ui-eyebrow";
    const title = document.createElement("h2");
    title.className = "quest-ui-title";
    const prompt = document.createElement("p");
    prompt.className = "quest-ui-prompt";
    const detail = document.createElement("p");
    detail.className = "quest-ui-detail";
    detail.setAttribute("aria-live", "polite");
    const choices = document.createElement("div");
    choices.className = "quest-ui-choices";
    choices.setAttribute("role", "group");
    choices.setAttribute("aria-label", "任务选择");
    panel.append(eyebrow, title, prompt, choices, detail);
    root.replaceChildren(panel);
    root.hidden = true;

    let latestModel = null;
    let latestMessageSequence = 0n;
    let focusedIndex = 0;
    let submitting = false;
    let awaitingAuthority = false;

    function restoreFocus() {
      if (typeof options.restoreFocus === "function") options.restoreFocus();
    }

    function hide() {
      const wasHidden = root.hidden;
      root.hidden = true;
      root.dataset.surface = "none";
      panel.setAttribute("aria-busy", "false");
      submitting = false;
      awaitingAuthority = false;
      if (!wasHidden) restoreFocus();
      return !wasHidden;
    }

    function focusChoice(index) {
      const buttons = choices.querySelectorAll("button[data-choice-index]");
      if (buttons.length === 0) return false;
      focusedIndex = (index + buttons.length) % buttons.length;
      buttons[focusedIndex].focus({ preventScroll: true });
      return true;
    }

    async function submitFocusedChoice(index = focusedIndex) {
      if (submitting || awaitingAuthority || !latestModel || latestModel.options.length === 0) {
        return false;
      }
      const target = latestModel.options[index];
      if (!target) return false;
      const submittedSequence = latestModel.sequence;
      const submittedChecksum = latestModel.checksum;
      submitting = true;
      root.dataset.submitState = "pending";
      panel.setAttribute("aria-busy", "true");
      for (const button of choices.querySelectorAll("button")) {
        button.setAttribute("aria-disabled", "true");
      }
      detail.textContent = "正在提交选择意图…";
      try {
        await options.submitIntent({
          projectionSequence: submittedSequence,
          projectionChecksum: submittedChecksum,
          objective: latestModel.objective,
          interaction: target.interaction,
          selection: target.selection
        });
        if (
          latestModel && latestModel.sequence === submittedSequence &&
          latestModel.checksum === submittedChecksum && !root.hidden
        ) {
          awaitingAuthority = true;
          root.dataset.submitState = "awaiting_authority";
          detail.textContent = "选择已提交，等待任务确认。";
        }
        return true;
      } catch (error) {
        awaitingAuthority = false;
        root.dataset.submitState = "rejected";
        panel.setAttribute("aria-busy", "false");
        detail.textContent = "选择尚未提交，请重试。";
        for (const button of choices.querySelectorAll("button")) {
          button.setAttribute("aria-disabled", "false");
        }
        options.onDiagnostic?.({ event: "selection_rejected", error: String(error) });
        focusChoice(index);
        return false;
      } finally {
        submitting = false;
        if (root.dataset.submitState === "pending") root.dataset.submitState = "idle";
      }
    }

    function dispatchAction(action) {
      if (!action || action.phase !== "pressed") return false;
      if ((submitting || awaitingAuthority) &&
          (action.id === actionIds.navigate || action.id === actionIds.confirm)) {
        return false;
      }
      if (action.id === actionIds.navigate) {
        const delta = action.y < 0 || action.x > 0 ? 1 : -1;
        return focusChoice(focusedIndex + delta);
      }
      if (action.id === actionIds.confirm) {
        const index = Number.isInteger(action.targetIndex) ? action.targetIndex : focusedIndex;
        return submitFocusedChoice(index);
      }
      if (action.id === actionIds.cancel) {
        if (latestModel?.surfaceName === "choice") {
          detail.textContent = "当前任务需要先完成一项选择。";
          return false;
        }
        hide();
        return true;
      }
      return false;
    }

    function acknowledgeClose(acknowledgement) {
      if (!acknowledgement || acknowledgement.reason !== 1 ||
          acknowledgement.reasonName !== "selection_committed" ||
          typeof acknowledgement.messageSequence !== "string" ||
          !/^[1-9][0-9]*$/.test(acknowledgement.messageSequence) ||
          !latestModel || latestModel.surfaceName !== "choice" ||
          acknowledgement.sessionGeneration !== latestModel.sessionGeneration ||
          acknowledgement.projectionSequence !== latestModel.sequence ||
          acknowledgement.projectionChecksum !== latestModel.checksum) {
        return false;
      }
      const incomingMessageSequence = BigInt(acknowledgement.messageSequence);
      if (incomingMessageSequence <= latestMessageSequence) return false;
      latestMessageSequence = incomingMessageSequence;
      hide();
      return true;
    }

    function render(event) {
      const model = createViewModel(event, options.catalog ?? defaultCatalog);
      const incomingMessageSequence = BigInt(model.messageSequence);
      if (incomingMessageSequence <= latestMessageSequence) return false;
      if (latestModel) {
        const incoming = BigInt(model.sequence);
        const previous = BigInt(latestModel.sequence);
        if (incoming < previous ||
            (incoming === previous && model.checksum !== latestModel.checksum)) {
          return false;
        }
        if (incoming === previous && model.checksum === latestModel.checksum) {
          latestMessageSequence = incomingMessageSequence;
          latestModel = model;
          return true;
        }
      }
      const wasChoice = latestModel?.surfaceName === "choice";
      latestModel = model;
      latestMessageSequence = incomingMessageSequence;
      focusedIndex = 0;
      submitting = false;
      awaitingAuthority = false;
      root.dataset.surface = model.surfaceName;
      root.dataset.polarity = model.polarityName;
      root.dataset.sequence = model.sequence;
      root.dataset.checksum = model.checksum;
      root.dataset.submitState = "idle";
      panel.setAttribute("aria-busy", "false");
      panel.setAttribute("aria-modal", model.surfaceName === "choice" ? "true" : "false");
      eyebrow.textContent = model.eyebrow;
      title.textContent = model.title;
      prompt.textContent = model.prompt;
      detail.textContent = model.detail;
      choices.replaceChildren();
      model.options.forEach((choice, index) => {
        const button = document.createElement("button");
        button.type = "button";
        button.dataset.choiceIndex = String(index);
        button.dataset.interaction = choice.interaction;
        button.dataset.selection = choice.selection;
        const label = document.createElement("strong");
        label.textContent = choice.label;
        const hint = document.createElement("span");
        hint.textContent = choice.hint;
        button.append(label, hint);
        button.addEventListener("focus", () => { focusedIndex = index; });
        button.addEventListener("click", () => {
          void dispatchAction({ id: actionIds.confirm, phase: "pressed", targetIndex: index });
        });
        choices.append(button);
      });
      root.hidden = false;
      if (model.surfaceName === "choice") {
        queueMicrotask(() => { focusChoice(0); });
      } else if (wasChoice) {
        restoreFocus();
      }
      return true;
    }

    panel.addEventListener("keydown", (event) => {
      if (event.key === "ArrowUp" || event.key === "ArrowLeft") {
        event.preventDefault();
        dispatchAction({ id: actionIds.navigate, phase: "pressed", x: -1, y: 1 });
      } else if (event.key === "ArrowDown" || event.key === "ArrowRight") {
        event.preventDefault();
        dispatchAction({ id: actionIds.navigate, phase: "pressed", x: 1, y: -1 });
      } else if (event.key === "Escape") {
        event.preventDefault();
        dispatchAction({ id: actionIds.cancel, phase: "pressed" });
      }
    });

    return Object.freeze({
      acknowledgeClose,
      dispatchAction,
      render,
      getModel: () => latestModel,
      dispose: () => {
        latestModel = null;
        latestMessageSequence = 0n;
        root.replaceChildren();
        root.hidden = true;
      }
    });
  }

  globalThis.TgdQuestUiRenderer = Object.freeze({
    actionIds,
    createRenderer,
    createViewModel,
    defaultCatalog,
    stableContentKeyHex
  });
})();
