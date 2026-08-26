# DEMO-084｜江南伞作试用与返工证据

> 产品版本：Demo 0.8.4
> 实现提交：`b7867c10336685af060dce9bbce7f98010baa8c9`
> Owner：新主开发与唯一集成 Owner
> 取证日期：2026-07-26

## 验收结论

Demo 0.8.4 的本机技术候选通过：同一真实 Axmol Web Host 从唯一 canonical `.tgdsbx` 建立 `CraftSession`，玩家可接近伞作工位、读取需求、二选一选材、按顺序执行两步操作、运行雨试、看到可恢复失败、返工并复试完成。该路线没有使用一键合成、纯进度条、DOM 自行结算或第二份 Gameplay 状态。

本证据同时回归 Demo 0.8.3 的关门阻挡、幂等触发、玩家自然倒地、全局 `R` 局部重建、两波/两目标和 terminal。Workbench 仍从同一作者源和 compiler service 生成包，并新增四组 update-only 工艺面板。

## 不可变输入与产物

| 身份 | 值 |
| --- | --- |
| 作者格式 | `tgd.sandbox.authoring / 1.3.0` |
| `.tgdsbx` 格式 | `1.3`，append-only sections `17–21` |
| Compiler Service ABI | `1.3` / `0x00010003` |
| 唯一作者源 SHA-256 | `16ce86671277eddf2b55f1ef3cfb90f27a8ad0cb91be0221db89403882280abc` |
| canonical package | `4,128` bytes |
| canonical package SHA-256 | `ff93c16e31ba8c6c62ecdd7134f0b64aed63386a3dde464d505837b040848975` |
| provider checksum | `a56f05e188731a11525af38a599506b3361b5d5107444c87f81d0d65f8532e78` |
| generated service module SHA-256 | `d70c2fddf19008934eb371c0fd24e8aaa06ee66ac5aa72be7ad3c517f7817485` |
| generated service WASM SHA-256 | `434a7c9444c816f096df8bd6bedda0b9661691cbdd7e9659cf28eebe1fd7b551` |

工艺实例只引用已登记的 `template_craft_process` 与研究登记 `RS-CRAFT-001–002`。现实来源、团队推断和幻想/系统改写分别记录在 [`../../09-术语与索引.md`](../../09-术语与索引.md)；“硬质补片首次雨试后需重新张紧”是本 Demo 的可返工系统分支，不冒充传统技艺史实。

## 真实 Web 路线

环境为 Microsoft Edge `150.0.4078.99`，完整结构化结果见 [`web/evidence.json`](web/evidence.json)。

| 检查点 | 实测 |
| --- | --- |
| 工位与需求 | 玩家移动到 `(1260, -3250)` 后按 `C` 进入工艺模式 |
| 选材 | 选择硬质回收纸补片；材料结果由 package Definition 持有 |
| 错序 | 未先对齐伞骨就尝试贴补，`completedOperations` 保持 `0` |
| 正确工序 | `3` 对齐伞骨、`4` 贴补，完成 `2/2` 操作 |
| 首次试用 | `T` 雨试后进入 `rework_required`；trial `1`、mistake `1` |
| 返工复试 | `G` 重新张紧后再次 `T`；trial `2`、rework `1`、completed `true` |
| 0.8.3 回归 | 闭门阻挡 `28`；重复触发 `1`；击败 `4` 名敌人；完成 `2` 波、`2` 目标和 terminal；accepted attacks `59` |
| 恢复 | 玩家自然降至 `0 HP` 后 `R` 回 `(-1250, -3000)`；工艺、门、Encounter 同时回初始状态 |
| 浏览器错误 | console `0`、page `0`、request `0` |

关键画面：

- [工位、材料与需求](web/02-craft-need-and-material-choice.png)
- [雨试失败并明确要求返工](web/03-rain-trial-rework-required.png)
- [返工后复试通过](web/04-reworked-canopy-retrial-passed.png)
- [死亡后所有本地系统恢复](web/07-death-retry-restored-all-systems.png)
- [两波与 terminal 回归完成](web/08-two-waves-terminal-complete.png)

## Workbench 路线

Workbench 结构化结果见 [`workbench/evidence.json`](workbench/evidence.json)。该脚本沿用 DEMO-082 的六类对象 CRUD、Launch/Safe Reload 和失败保持回归身份，因此 JSON 的 `taskId/productVersion` 仍为 `DEMO-082 / Demo 0.8.2`；精确 commit 已绑定 D84 实现，`route.craftPanels` 额外断言以下四组 D84 面板真实可见：

1. Craft Materials
2. Craft Workstations
3. Craft Processes
4. Craft Steps

四组面板只修改现有记录，不开放伪完整 CRUD。作者路线仍完成 revision `4` Launch、revision `5` Safe Reload、revision `6` 无效草稿保持 generation `2` 与旧 live frame，主页面和 Host frame 的 console/page/request error 均为 `0`。

- [四组工艺 update-only 面板](workbench/00-craft-update-only-panels.png)
- [Launch 后真实 Host](workbench/02-launch-live.png)
- [无效草稿保留上一有效画面](workbench/05-invalid-keeps-live.png)

## 自动门

- MSVC Debug 全量构建通过；CTest `31/31`。
- Emscripten 3.1.73 / Axmol 2.11.4 Release Single `tgd_system_demo_web` 构建通过。
- 根 Node `74/74`；Workbench `61 pass + 2 expected environment skips`。
- 真实 generated module/WASM 环境下 Workbench 浏览器套件 `3/3`。
- `npm run lint:architecture`、`npm run validate:design`、证据合同自测与 `git diff --check` 通过。

## 尚未放行

- 本证据由实现者执行，不能替代 [`../../02-版本规划与验收.md`](../../02-版本规划与验收.md) §6.2 要求的非实现者试玩；该人工门保持 Open，并最迟在 Demo Candidate 聚合验收前补齐。
- 工艺事实仍待真实从业者审阅；研究登记当前只证明公开来源和团队边界清晰。
- D84 不含库存、订单、成本、品质结算、交付或地区/战斗后果；这些属于 DEMO-085。
- 目前只放行本机单 Edge Internal Blockout；Windows 可见窗口、Chrome/Edge/Firefox 完整矩阵、20–30 分钟真人路线和发布部署均未放行。
- `R` 是当前 Host 内 Craft + thin runtime + Encounter 的局部整组重建，不是跨 Profile、离开返回、包代次和双端的通用聚合事务；后者属于 DEMO-088。
