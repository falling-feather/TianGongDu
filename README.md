# 天工渡 / TianGongDu

《天工渡》是一款浏览器首发、复用同一 C++ 核心扩展到 Windows 等平台的 2.5D 斜向全景动作与工艺经营角色扮演游戏。玩家作为年轻的“执灯渡人”，在东方工艺社会中学习技艺、经营工坊、探索、交往和战斗，让失序的器物、工序、责任与关系重新归位。

长期产品由三条同等重要的主线组成：亲手体验经过研究审阅的非遗技艺、经营工坊与地区生产网络、在探索和战斗中处理失序并让地区归位。当前不先扩写长剧情，而是推进 `F1-SYSTEM-DEMO-01`：用一个可编辑、可反复试玩的江南工艺经营与战斗系统沙盒，证明工艺经营有决策和手感、战斗有深度、工具确实能生产两类玩法。

## 当前结论

| 问题 | 结论 |
| --- | --- |
| 现有 F1 灰盒能运行吗 | 能。Web 灰盒已有移动、基础战斗、任务、两路线、四阶段 Boss、双结算、存档与固定回放，继续作为回归基线 |
| 新的系统型 Demo 能直接给玩家玩吗 | 还不能作为完整 Demo。当前真实 Web Internal Blockout 已能移动、完成伞作选材/两步操作/雨试/返工、操作机关、开门、执行两波/两目标战斗、死亡重试和 terminal，也能从 Workbench Launch/Safe Reload；尚无工坊订单经营、正式 Windows 窗口、三名 NPC 和完整战斗手感 |
| 编辑器能添加人物和敌人吗 | 公开页面已能新增/复制/受控删除或重建六类场地对象，并为已有敌人编辑 profile、阵营、职责和生命；现有两波/目标与工艺记录可 update-only 编辑，技能/loadout、人物模板和 Wave/Objective/工艺完整 CRUD 尚未完成 |
| 工艺经营系统已经实现吗 | 部分实现。通用材料、工位、工艺、两步有序操作、试用、失误和返工已进入 canonical 包、Gameplay、Workbench 与真实 Web；库存、工位占用、订单、成本、品质结算、交付和地区后果仍未实现 |
| 编辑器能完整制作关卡吗 | 还不能。六类对象、Actor Gameplay binding、现有两波/目标和工艺更新、二维摆放及 Web Launch/Safe Reload 已完成；Wave/Objective/工艺从空创建删除、订单经营、完整 Undo/Redo 和 Windows Launch 仍未完成 |
| Windows 和 Web 是否已证明同一 Demo | 核心与包格式已有共同测试；玩家可见的同包双端 Preview 仍未完成 |

工程底座完成度约为 **61%–66%**；按“设计师能制作、玩家能完成工艺经营与战斗闭环”的系统型 Demo 口径约为 **53%–58%**。这些数字用于排期，不是发布质量分数。

## 系统型 Demo 目标

最终交付是一块首轮约 20–30 分钟、可重复调试的江南工艺经营与战斗系统沙盒：

1. 在轻量编辑器中摆放玩家、敌人、障碍、互动点、机关、安全点和工艺工位。
2. 配置材料、工序、工位、订单/需求、返工结果，以及两到三组敌人波次和关卡目标。
3. 保存并导出同一内容包，直接启动 Windows 或 Web 试玩。
4. 玩家从地区需求出发，选择材料和工序，亲手完成一段伞器维护/调校，而不是在菜单中一键合成。
5. 玩家管理少量库存、工位、成本、品质与交付结果；失败允许返工或形成有用途的次品。
6. 制作结果改变探索路线、器物能力或战斗解法，战斗和探索所得也回到工坊与地区生产。
7. 玩家使用移动、轻重击、防守、闪避、两种架势和首批数据化技能完成两波职责明确的战斗。
8. 死亡、重试、离开返回和重载正确恢复工艺、工坊、机关、敌人和战斗状态。
9. Windows 与 Web 表现一致；灰盒对象、工位、敌人职责和工艺反馈不靠颜色也能辨认。

本 Demo 只做一个江南代表性闭环，不同时量产三地区；但数据、工具和验收必须保留“地区不是换皮”的边界。暂不优先：新手引导、长对话、剧情演出、最终美术、正式移动端、完整云服务、三地区量产和大型商业编辑器。

## 已有工程基础

- C++20 分层核心、固定 60 Hz 模拟、量化快照、命令回放与 Native/Web 测试。
- F1 Web Shell、IndexedDB/Profile 奖励原子持久化、重复奖励保护和故障回归。
- 唯一作者源 [`content/design/system-demo.sandbox.json`](content/design/system-demo.sandbox.json)。
- 有界 `.tgdsbx` 包、唯一 C++ 编译/校验器、last-valid provider、JS/WASM 桥和 canonical Export。
- Workbench 的文件打开、六类对象 CRUD、Actor Gameplay binding、现有 Wave/Objective 与材料/工位/工艺/步骤 update-only 面板、二维画布、受控 region/Stable Asset 选择、保存、重载、并发保护、共享诊断、Export 和 Web Launch/Safe Reload。
- Sandbox 运行链：权威玩家位姿、相对移动、typed interaction → mechanism → dynamic blocker，确定性 CraftSession，以及 Actor/Combat/Wave/Objective/terminal 和单 Host 局部重建。
- 12 个 Stable Asset ID、24 个 Standard/Low 灰盒产物和失败关闭的资源解析候选。
- 隔离的真实系统 Demo Web Host：加载唯一 canonical 包，显示可辨灰盒对象，并支持移动、伞作选材/有序操作/雨试/返工、关门碰撞、operate 开门、轻/重击、两波目标战斗、死亡与 local retry。
- 现有 7 Beat、两路线、Boss、双结算、存档与浏览器自动路线作为回归基线。

## 当前最短开发路径

产品里程碑改用 `Demo 0.8.x`，工程提交仍沿用 `V2.2.1`：

1. `0.8.0`（已完成）：冻结唯一启动目标，审计旧代码、分支和 worktree，只列清单、不盲删。
2. `0.8.1`（已完成）：交付首个新 Demo Web 画面，可移动、操作机关、开门和重试。
3. `0.8.2`（已完成）：编辑器能新增/摆放六类对象，并安全启动/重载同一 Web Host。
4. `0.8.3`（已完成技术闭环）：在同一场景运行两波敌人、目标和 terminal；真人 8–12 分钟节奏留到战斗调校与最终候选复验。
5. `0.8.4`（已完成技术闭环）：实装一段可选材、失误、雨试、返工和复试的江南伞作技艺；非实现者试玩仍待补。
6. `0.8.5`（下一阶段）：实装有限库存、一个订单、品质/成本、交付和成品后果的小型工坊经营闭环。
7. `0.8.6–0.8.7`：完善战斗手感，并加入沈砚、骆青禾、阿棠三名代表 NPC。
8. `0.8.8–0.8.9`：完成状态恢复、Windows/Web 同包、表现优化和真人 Demo 验收。

不再常驻合同、包体、平台、内容、UI、美术、QA 等多条工作组。新主开发最多同时开两条功能分支；每项任务由一人直接认领，候选出现后再集中 Review 和 QA。

详细排期和验收见 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)；完成历史只在 [`docs/03-发布历史.md`](docs/03-发布历史.md) 维护。

## 文档入口

| 入口 | 用途 |
| --- | --- |
| [`docs/00-项目总纲.md`](docs/00-项目总纲.md) | 产品背景、长期目标、不可变原则与当前 Demo 的关系 |
| [`docs/01-开发者文档.md`](docs/01-开发者文档.md) | 当前实现、未完成模块、架构、工作流和开发准则 |
| [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md) | Demo 0.8.x 小版本、任务资源、精简协作、工期和放行门 |
| [`docs/03-发布历史.md`](docs/03-发布历史.md) | 已完成提交、阶段证据与历史边界 |
| [`docs/04-游戏设计总纲.md`](docs/04-游戏设计总纲.md) | 战斗、工艺、经营、关系、任务与玩法循环 |
| [`docs/05-世界与叙事圣经.md`](docs/05-世界与叙事圣经.md) | 世界观、地区基调、正史、语言与研究边界 |
| [`docs/06-内容生产规范.md`](docs/06-内容生产规范.md) | NPC、敌人、任务、地区、武器和对话模板 |
| [`docs/07-1.0地区与内容蓝图.md`](docs/07-1.0地区与内容蓝图.md) | 江南/龙泉/徽州、24 名 NPC、Boss 与地区内容 |
| [`docs/01-developer/13-编辑器与模板生产.md`](docs/01-developer/13-编辑器与模板生产.md) | 轻量编辑器现状、对象模型、后续功能和验收 |
| [`docs/01-developer/16-测试CI与发布门禁.md`](docs/01-developer/16-测试CI与发布门禁.md) | Native、Web、浏览器、证据和发布规则 |
| [`docs/handovers/2026-07-26-system-demo/README.md`](docs/handovers/2026-07-26-system-demo/README.md) | 可直接复制到新任务的系统型 Demo 主开发启动提示词 |

`docs/` 是唯一正式文档根。`docs/handovers/` 只保存历史交接上下文，不替代当前任务和计划。

## 基本验证

```text
npm test
npm run validate:design
npm run lint:architecture
npm run check:web-abi
npm run test:system-demo-web
npm run test:system-demo-workbench
```

Native、Web Single、浏览器和 Preview 的完整命令按 [`docs/01-developer/16-测试CI与发布门禁.md`](docs/01-developer/16-测试CI与发布门禁.md) 执行。命令行测试、CTest 或 Workbench 下载都不能单独证明“玩家可见 Demo 已完成”。

## 技术基线

- C++20 Gameplay/Runtime 是玩法真相。
- Axmol 2.11.x LTS 承担 2D/2.5D 与平台宿主；Web 由 Emscripten 编译为 wasm32。
- Canvas 承载游戏画面；DOM 只承担文本密集 UI、菜单和辅助功能。
- Windows 与 Web 必须消费同一内容包、同一 Gameplay 规则和同一稳定 ID。
- Presentation、JavaScript、编辑器和存储层都不得保存第二份战斗、任务或奖励真相。
- Stable ID、Schema、包版本、迁移、失败保持和回滚必须可自动验证。
