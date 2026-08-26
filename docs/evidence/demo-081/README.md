# DEMO-081｜Demo 0.8.1 Web Host 验收表

> 状态：Accepted / Internal Blockout
> 日期：2026-07-26
> Owner / Reviewer：新主开发与唯一集成 Owner
> 实现提交：`40108a32fa0fff313fb36110646704ec04ef2b2e`
> 分支：`codex/demo/081-web-visible-host`

## 玩家可见结果

- 真实 Edge 加载隔离的 `tiangongdu-system-demo.html`，页面明确标记 `Internal Blockout`。
- Host 读取唯一 `system-demo.tgdsbx`，包大小为 2,712 bytes；解析并实际显示 12 个 Stable Asset 槽。
- 玩家可用 WASD/方向键移动；关闭的门产生权威碰撞阻挡。
- 玩家在机关范围内执行 `F / operate` 后开门，并可越过原阻挡区域。
- `R / local retry` 恢复作者安全点 `(-1250,-3000)`，关闭门并生成新的 runtime generation。

## 自动与真实路线

```powershell
node tools/run-toolchain.mjs cmake --build --preset web-release-single --target tgd_system_demo_web --parallel 8
$env:TGD_SYSTEM_DEMO_EVIDENCE_DIRECTORY='docs/evidence/demo-081'
npm run test:system-demo-web
```

正式采集使用 Edge `150.0.4078.99`，路线为：

1. 初始点确认包、资源数、门关闭和玩家位置。
2. 直行撞门，确认没有越过且 `blockedMoveCount > 0`。
3. 横移到机关，执行 `operate`，确认门打开。
4. 穿过门到 Y=2270。
5. 执行 local retry，确认玩家回到 `(-1250,-3000)` 且门重新关闭。
6. 确认 console、page 与 request error 均为 0。

机器原始数据见 [`evidence.json`](evidence.json)。

## 画面证据

- [`01-initial-closed-gate.png`](01-initial-closed-gate.png)：初始关门、12 个资源槽和玩家出生点。
- [`02-opened-and-crossed.png`](02-opened-and-crossed.png)：机关已应用、门已打开、玩家已越过门。
- [`03-retried-closed-gate.png`](03-retried-closed-gate.png)：重试后安全点与关门状态恢复。

## 放行边界与回退

- 本验收只放行 `Demo 0.8.1` Internal Blockout；不代表两波战斗、伞作技艺、工坊经营、三名 NPC、Windows 可见 Host 或完整 20–30 分钟 Demo 已完成。
- 新 Host 与旧 F1 target 隔离，不复制 F1 Layer、Profile 或奖励状态；旧 F1 回归入口仍保留。
- 回退点是实现提交的父提交；canonical 作者源、包生成器和旧 Host 未被覆盖或删除。
