# DEMO-082｜Demo 0.8.2 编辑器闭环一验收表

> 状态：Accepted / Web authoring loop
> 日期：2026-07-26
> Owner / Reviewer：新主开发与唯一集成 Owner
> 实现提交：`a6496c0eeeb5c704509e77b8b0e162bbbde52bc1`
> 分支：`codex/demo/082-editor-crud-launch`

## 设计师可见结果

- Workbench 公开对象树可新增、复制、受控删除或重建 player、Actor placement、ground blocker、safe point、interaction 和 mechanism。
- Inspector 可编辑作者标签、位置、高度、floor、方向、interaction range、region 和按 `SandboxAssetKind` 过滤的 Stable Asset ID；运行身份仍只使用 Stable ID。
- 二维场地显示 bounds/grid、对象脚点、方向、interaction range 和 blocker AABB；支持点击选择、拖拽、方向键移动、100 mm 吸附和缩放。
- 共享 C++ 诊断可定位回对象/字段。保存并检查通过后可 `Launch` 真实 Axmol Web Host；修改后可 `Safe Reload`。
- 新候选未 ready 或草稿校验失败时，上一份 live package 与可见画面保持不变。

## 自动与真实路线

```powershell
node tools/run-toolchain.mjs cmake --preset web-release-single
node tools/run-toolchain.mjs cmake --build --preset web-release-single --target tgd_system_demo_web --parallel 8
npm --prefix apps/content-workbench test
$env:TGD_SANDBOX_SERVICE_BUILD_DIRECTORY='build/web-release-single'
npm --prefix apps/content-workbench run test:browser
npm run test:cpp:msvc:release
$env:TGD_DEMO_082_EVIDENCE_DIRECTORY='docs/evidence/demo-082'
npm run test:system-demo-workbench
```

正式采集使用 Edge `150.0.4078.99`，路线为：

1. 复制 `actor.system_demo.entry.slot_a` 为新 Actor，并从二维画布向右移动 100 mm。
2. 复制互动点，触发“同一目标 Mechanism 存在多个写入者”诊断，通过 locator 返回新对象。
3. 删除旧互动点修复诊断，保存 revision 4，重新检查并 Launch generation 1。
4. 再把 Actor 向右移动 100 mm，保存 revision 5；受控阻塞新 package 请求，确认 generation 1 仍可见。
5. 释放请求，确认 generation 2 才替换旧 frame，且 package SHA 已变化。
6. 在 revision 6 新增无效重复互动，确认共享检查失败、Reload 阻塞、generation 2 与旧 live frame 保持。
7. 确认主页面及全部 Host frame 的 console、page、request error 均为 0。

机器原始数据见 [`evidence.json`](evidence.json)。其中 Launch package 为 2,848 bytes / `sha256:8c1f35d8c9030cb9bb7cc22eba5f7ca4314b9a60882fdebcdce95d5b7a50e87b`，Safe Reload package 为 2,848 bytes / `sha256:c0acc3cf4b96801b4228489f96446951a2fe36aae7433edc483c5ca6eac6f936`。

## 画面证据

- [`01-crud-diagnostic.png`](01-crud-diagnostic.png)：新对象、二维场地、Inspector 与共享诊断 locator。
- [`02-launch-live.png`](02-launch-live.png)：generation 1 已在 Workbench 内显示真实 System Demo Host。
- [`03-reload-keeps-live.png`](03-reload-keeps-live.png)：新候选请求被阻塞时，旧 live frame 仍可见。
- [`04-reloaded-live.png`](04-reloaded-live.png)：generation 2 ready 后完成可见交换。
- [`05-invalid-keeps-live.png`](05-invalid-keeps-live.png)：revision 6 无效草稿有精确诊断，generation 2 画面保持。

集成 Owner 已逐张复核布局、可读性、候选遮挡和失败保持。供非实现者复验的人工路线与上节 1–7 完全相同；本里程碑没有把该复验写成独立真人可用性研究或 20–30 分钟试玩。

## 依据、放行边界与回退

- 任务历史：[`03 的 2.2.1-system.17`](../../03-发布历史.md)；原任务已在 2026-08-26 周期收束时从 02 迁入历史。实现依据继续使用 [`12 §21.6–21.9`](../../01-developer/12-内容存档与版本契约.md#216-format-11-producerdecoder-与-owning-document)、[`13 §2–10`](../../01-developer/13-编辑器与模板生产.md#2-当前能力表)、[`16 §5.4/§18.1`](../../01-developer/16-测试CI与发布门禁.md#54-demo-082-workbench--web-preview-路线) 与 [`17 §2–4`](../../01-developer/17-平台构建部署与运维.md#2-cmake-preset)。
- 实际内容/资源入口：[`system-demo.sandbox.json`](../../../content/design/system-demo.sandbox.json)、[`template-registry.json`](../../../content/templates/template-registry.json)、[`v1-content-catalog.json`](../../../content/design/v1-content-catalog.json) 与 [`runtime-import-manifest.json`](../../../assets_src/system-sandbox-blockouts/runtime/runtime-import-manifest.json)。
- 本验收只放行 Demo 0.8.2 的本机 Web 作者闭环；Actor profile/duty/skill、Wave/Objective/terminal、两波战斗、完整 Undo/Redo、Windows 可见 Preview、三浏览器 manifest、工艺经营、NPC 与完整 Demo 仍未完成。
- Windows MSVC Release 29/29 只证明共享 C++ 回归，不是可见 Windows 窗口证据。
- 回退点为实现提交父提交 `c8a084c0c763306d47c4968c35e3436fda2a949a`；唯一作者 JSON、旧 F1 Host、validator、Profile、历史分支和 worktree 未被删除或覆盖。
