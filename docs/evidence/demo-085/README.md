# DEMO-085｜小型工坊经营与路线后果证据

> 产品版本：Demo 0.8.5
>
> 实现提交：`acbc627a6fb7424d4045ebbc41b25e2436c39300`
>
> Owner：新主开发与唯一集成 Owner
>
> 取证日期：2026-07-26

## 验收结论

Demo 0.8.5 的本机技术候选通过：唯一江南系统 Demo 包现在同时持有有限材料库存、初始资金、材料成本、基础品质、返工品质增益、工位占用、一个订单、最低品质、奖励和一条显式世界后果。玩家在同一真实 Axmol Web Host 中购买材料、完成伞作操作、经历雨试失败与返工、交付合格成品，并由交付打开一条实际可穿越的屋顶捷径；原主门仍关闭，证明结果改变的是作者指定路线，而不是直接清空全部碰撞。

经营真相由 C++ `WorkshopSession` 持有。DOM、Axmol 节点、浏览器脚本和墙钟只提交输入或读取快照，不扣库存、不结算资金、不计算品质、不发订单奖励，也不直接开门。订单交付先在 Workshop 与 Encounter 候选上预检，再通过既有 typed interaction → mechanism → blocker 命令链提交；重复交付不会重复奖励或重复触发后果。

## 不可变输入与产物

| 身份 | 值 |
| --- | --- |
| 作者格式 | `tgd.sandbox.authoring / 1.4.0` |
| `.tgdsbx` 格式 | `1.4`；append-only sections `22–24` |
| Compiler Service ABI | `1.4` / `0x00010004` |
| 唯一作者源 SHA-256 | `ae9584df6f74fcf9cc3e15c9977ce7d443637268a9a114dc1ddf204ac2911210` |
| canonical package | `4,832` bytes |
| canonical package SHA-256 | `b51f99aab88ccb4cefc4a945cf00c36de726c5478e302e281bb0c58603e3112e` |
| provider checksum | `d3001d59da2792f72bdf121ae6585aa24a4a8a3b26116a24ab8e23954a4dcebe` |
| generated service module SHA-256 | `2b7e7f133173087216e9807911e6e055ad72414765ac2922c27b448bbc8b717a` |
| generated service WASM SHA-256 | `33882116aa73e33a1647333b3fed1e80443d07e68957af523e0c0900a0473360` |

Sections 22–24 分别为 `workshops / workshop_material_stocks / workshop_orders`，不复用 1–21 的既有编号。旧 format 1.0–1.3 仍不做二进制原地升级；应从权威作者源重新编译。JS client 可以解码旧结果布局，但当旧 ABI 收到其不能表达的 Actor、工艺或经营记录时会在创建 request 前失败关闭，禁止静默丢字段。

## 经营取舍与交付后果

| 路线 | 成本与库存 | 品质与工序 | 结果 |
| --- | --- | --- | --- |
| 柔韧树皮纸补片 | 成本 `80`，库存 `1` | 基础品质 `9000`，按序操作后首次雨试直接通过 | 高成本、少步骤、交付后资金 `45` |
| 硬质回收纸补片 | 成本 `30`，库存 `2` | 基础品质 `6500`；首次雨试失败，返工增加 `1900` 后品质 `8400` | 低成本、需返工、交付后资金 `95` |

订单最低品质为 `8000`、需求量为 `1`、奖励为 `25`。自动验收走低成本返工路线：错序保持 operation `0`；两项正确操作后首次雨试得到 trial `1`、mistake `1`；返工复试得到 trial `2`、rework `1`、completed `true`。交付后库存为 `[1,1]`、累计支出 `30`、资金 `95`、工位释放、订单 fulfilled；再次交付保持资金、订单和路线不变。

交付的 `consequenceTargetId` 指向 `interaction.system_demo.workshop_shortcut`，再由既有 binding 激活 `mechanism.system_demo.roof_shortcut` 并关闭 `blocker.system_demo.roof_shortcut`。玩家实测穿过右侧捷径到达 `Y=2090`，同时左侧主门仍关闭且产生 `25` 次阻挡；之后再正常操作主门并完成两波战斗。

## 真实 Web 路线

环境为 Microsoft Edge `150.0.4078.99`。完整结构化结果见 [`web/evidence.json`](web/evidence.json)，其中 commit 精确绑定本页实现提交。

| 检查点 | 实测 |
| --- | --- |
| 初态 | 资金 `100`；柔韧/回收补片库存 `1/2`；工位空闲；订单未完成；两扇 blocker 均关闭 |
| 购买与占用 | 购买回收补片后资金 `70`、支出 `30`、库存 `2→1`、品质 `6500`、工位 occupied |
| 失败保持 | 错序操作不推进 Craft；未完成/品质不足时不能交付 |
| 返工闭环 | trial `2`、mistake `1`、rework `1`、品质 `8400`、output completed |
| 交付 | 资金 `95`、工位释放、订单 fulfilled、屋顶捷径 open；重复交付不重复结算 |
| 路线后果 | 右侧捷径可穿越到 `Y=2090`，左侧主门继续阻挡 |
| 0.8.3 回归 | 重复主门触发 `1`；击败 `4` 名敌人；完成 `2` 波、`2` 目标和 terminal |
| 局部恢复 | 自然倒地后 `R` 回 `(-1250,-3000)`；资金、库存、工位、订单、两扇门、Craft、Encounter 全部回作者初态 |
| 浏览器错误 | console `0`、page `0`、request `0` |

关键画面：

- [初始工位、库存与双路线](web/01-initial-craft-bench-and-closed-gate.png)
- [材料选择与资金/库存](web/02-craft-need-and-material-choice.png)
- [雨试失败与返工要求](web/03-rain-trial-rework-required.png)
- [返工复试与品质达标](web/04-reworked-canopy-retrial-passed.png)
- [订单交付并打开屋顶捷径](web/05-order-delivered-shortcut-open.png)
- [穿越捷径而主门仍关闭](web/06-shortcut-traversed-main-gate-closed.png)
- [死亡后经营、路线与战斗全回初态](web/09-death-retry-restored-all-systems.png)
- [两波与 terminal 回归完成](web/10-two-waves-terminal-complete.png)

## Workbench 路线

结构化结果见 [`workbench/evidence.json`](workbench/evidence.json)。真实 Edge 页面显示原四组 Craft 面板和新增两组 update-only 经营面板：

1. Workshops：工位引用、初始资金，以及两项材料的成本、库存、基础品质和返工增益。
2. Workshop Orders：工坊/工艺引用、需求量、最低品质、奖励和显式后果引用。

作者路线继续完成 revision `4` Launch generation `1`、revision `5` Safe Reload generation `2`、revision `6` 无效草稿保留上一 publication/live frame。两次 publication package 均为 `5,008` bytes 且 SHA 不同；主页面与 Host frame 的 console/page/request error 均为 `0`。

- [六组 Craft/Workshop update-only 面板](workbench/00-craft-workshop-update-only-panels.png)
- [Launch 后真实 Host](workbench/02-launch-live.png)
- [候选未 ready 时旧画面保持](workbench/03-reload-keeps-live.png)
- [无效草稿保留上一有效画面](workbench/05-invalid-keeps-live.png)

这些面板只更新现有记录，不宣称从空创建/复制/删除完整工艺经营图。

## 自动门

- Windows MSVC Debug 全量构建通过；CTest `32/32`。
- Emscripten `3.1.73` / Axmol `2.11.4` Release Single `tgd_system_demo_web` 严格构建通过。
- 根 Node `75/75`；Workbench `64 pass + 2 expected environment skips`。
- 真实 generated module/WASM 环境中的 compiler service client `15/15`，无跳过。
- `npm run lint:architecture`、`npm run validate:design`、`npm run validate:toolchain`、证据合同自测与 `git diff --check` 通过。
- `npm run validate:toolchain:cache` 仅报告本机未保留九个离线下载归档；已安装工具仍由锁文件校验并完成上述 MSVC/Emscripten 构建。该环境事实不被写成源码或产物失败。

## 尚未放行

- 本证据由实现者执行，不替代 [`../../02-版本规划与验收.md`](../../02-版本规划与验收.md) §6.2 的非实现者 20–30 分钟试玩；该人工门保持 Open。
- Windows 这里只完成真实 MSVC 构建与 Native 测试，没有可见产品窗口，因此不能填写 `visible-window-preview`。
- 当前只证明本机单 Edge Internal Blockout；Chrome/Edge/Firefox 完整矩阵、发布 manifest、部署与性能门仍属于 Demo 0.8.8–0.8.9。
- `R` 是当前 Host 内 Workshop/Craft + thin runtime + Encounter 的局部整组重建；离开返回、包代次、Profile、双端同包与通用 prepare/commit/rollback 仍属于 DEMO-088。
- 工艺事实仍沿用 `RS-CRAFT-001–002` 的来源/推断/幻想边界；真实从业者审阅仍 Open。本版本新增的是经营数值与路线后果，不把这些玩法数值冒充传统技艺史实。
- 战斗仍是 0.8.3 的最小轻/重击和两种普通职责；两架势、4–6 个技能、精英与手感验收进入 DEMO-086。

## 回退

实现提交可整体回退到父提交 `4e5ba62deeea65510babcfd6c8725aeb85c19e7d`，恢复 Demo 0.8.4 的 format/schema/ABI 1.3 与单 blocker 工艺 Host。回退时必须同时恢复作者源、compiler service、canonical 包、Host 和 Workbench，不能让 1.3 consumer 读取 1.4 包；不会删除 Demo 0.8.1–0.8.4 的历史证据、分支或 worktree。
