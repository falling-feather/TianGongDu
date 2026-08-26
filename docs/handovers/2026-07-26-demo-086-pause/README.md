# 《天工渡》系统型 Demo 0.8.6 暂停与恢复交接

> 状态：Paused snapshot
>
> 职责：记录 2026-07-26 项目暂停时唯一可恢复的 Git、构建、验证与后续入口；任务状态仍以 [`02`](../../02-版本规划与验收.md) 为权威
>
> 最后更新：2026-07-26
>
> 维护者角色：新主开发

## 1. 暂停结论

项目应用户要求暂停，不再继续扩展 `DEMO-086`。暂停不是版本完成：

- 稳定集成基线为根 `主开发` 的 `e62a1785e793b95866ebbb3ad6e9243a62b3ac31`，对应已验收的 Demo 0.8.5。
- 在制品保存在 `codex/demo/086-combat-feel` 的 clean 检查点 `d94d97f8fc27feb186ae1535a49006d535bbff68`。
- 该检查点未合入 `主开发`，未推送 `main`，未形成 Demo 0.8.6 候选。
- 没有第二条并行功能分支，没有运行中的项目构建、Web 服务、浏览器验收或 QA。
- 全部 16 个 worktree 在收束复查时均为 clean；任何旧分支/worktree 都未删除。

## 2. Git 与工作树快照

| 用途 | 分支 / worktree | HEAD | 状态 | 恢复动作 |
| --- | --- | --- | --- | --- |
| 稳定集成 | `主开发` / 根工作区 | `e62a1785e793b95866ebbb3ad6e9243a62b3ac31` | clean；相对上游 ahead 56 | 只承担文档与后续 Review/集成 |
| D86 在制品 | `codex/demo/086-combat-feel` / `.tmp/worktrees/demo-086` | `d94d97f8fc27feb186ae1535a49006d535bbff68` | clean；未集成 | 从该提交继续，不 reset、不重建同名树 |
| D81–D85 历史 | `codex/demo/081-*`—`codex/demo/085-*` | 见 `git worktree list --porcelain` | clean | 保留实现、证据与 cherry-pick 历史 |
| 更早实验线 | 见 [`02 §0.4`](../../02-版本规划与验收.md#04-git-与历史工作树) | 各自原 HEAD | clean | 继续按保留/隔离/可删除但未授权分类；本次不清理 |

## 3. D86 检查点包含什么

检查点以 `DEMO-086` 的一个可回滚意图保存以下在制品：

1. `.tgdsbx` format、authoring schema 与 compiler service ABI 暂升至 `1.5 / 1.5.0 / 0x00010005`，append-only sections 25–28 表达 Combatant、Ability、Skill Binding 和 Elite Encounter。
2. 唯一作者源 [`system-demo.sandbox.json`](../../../content/design/system-demo.sandbox.json) 暂存 4 个 combatant、19 个 ability、6 个 skill binding 和 1 个 elite encounter；对象使用正式 `weapon_umbrella`、两种江南普通敌人与 `jn_boss_rain_market_bell` 机器身份。
3. Combat Resolver 暂接精准防守、体力消耗、定力/踉跄、打断与命中硬直；Encounter Session 暂接输入缓冲、连段窗口、冷却、架势/技能和精英阶段/证据结算。
4. Encounter Director 暂按 pressure、flanker、controller 使用不同队形、攻击节奏与技能行为。
5. 唯一 [`apps/system-demo-web`](../../../apps/system-demo-web/) Host 暂接 `L` 防守、`E` 闪避、`Q` 切架势、`Z/X` 技能和战斗资源/精英 HUD；Presentation 只读取 Gameplay 导出。
6. Workbench authoring/transport、唯一 C++ producer/decoder/validator、C ABI/JS client、Native/Node/Web 静态合同同步到了同一 1.5 候选。

暂停时生成身份：

| 项目 | 值 |
| --- | --- |
| 作者源 SHA-256 | `05f8db6e65144ac698391b0ce7229f8f0b240a2d9b969eb023a17ea52f730a68` |
| canonical package bytes | `8448` |
| package SHA-256 | `1fe90fb73d5d00add13479f0db60d4901e6e10ef1e5f494a3026d3150683e4bc` |
| provider checksum | `ccae6c1b7e0d0a097fc4454df38e1e52654fcda7793f02392204c71629401a75` |

## 4. 验证状态

| 门 | 暂停时结果 | 结论 |
| --- | --- | --- |
| 根 Node | `76/76` | Pass |
| Workbench | `66 pass + 2 expected environment skips` | Pass |
| 证据合同、设计、架构、diff | 全部通过 | Pass |
| 关键 Native | package contract、combat resolver、encounter session、package、compiler、service ABI 编译并执行成功 | Pass |
| Web/Wasm Host | Emscripten 3.1.73 / Axmol 2.11.4 Web Debug 完整链接 | Pass，但不是浏览器验收 |
| 全量 MSVC workflow | MSBuild 资产生成步骤取得 Node `22.20.0`，锁要求 `20.18.0` | Open；失败关闭，禁止跳过 |
| D86 真实 Edge 路线 | 未执行 | Open |
| 人工战斗手感 | 未执行 | Open |
| `docs/evidence/demo-086/` | 尚未建立 | Open |
| Windows 可见窗口 / 三浏览器 / QA | 未执行 | Open；不是本检查点完成条件 |

## 5. 恢复后的第一条路线

1. 读取 [`00`](../../00-项目总纲.md)、[`01`](../../01-开发者文档.md)、[`02 §0.1、§4.7`](../../02-版本规划与验收.md#01-当前状态) 和本文件，确认用户仍要继续 Demo 0.8.x。
2. 运行 `git status --short --branch`、`git worktree list --porcelain` 与项目进程检查；根工作区和 `demo-086` 必须仍可解释。发现漂移时先对账，不覆盖。
3. 在 `.tmp/worktrees/demo-086` 确认 `codex/demo/086-combat-feel` 与 `d94d97f8fc27feb186ae1535a49006d535bbff68`，再开始新修改。
4. 先修复 MSVC workflow 的锁定 Node 传播，使 `npm run test:cpp:msvc` 使用 `20.18.0`；不得降低版本锁或绕过资产生成。
5. 为 authored `SandboxEncounterSession` 增加直接覆盖 1.5 profile/ability/elite 的 Native 测试；Review authored initialize 的重复构建路径，并让 stance 数量检查在 Session 边界失败关闭。
6. 把 [`tests/browser/system-demo-web-host.mjs`](../../../tests/browser/system-demo-web-host.mjs) 升到 `DEMO-086`，真实操作防守、闪避、切架势、双技能、两类普通敌人与精英阶段/结算。
7. 按机器本地规则使用外部 Edge/Chrome 扩展后端执行真实 Edge 路线；记录 console/page/request error、输入、状态、截图和结构化 JSON 到 `docs/evidence/demo-086/`。
8. 完成代码 Review、全量自动门和人工手感后，才把 1.5 接口同步进 [`11`](../../01-developer/11-Runtime与Gameplay组件模型.md)、[`12`](../../01-developer/12-内容存档与版本契约.md)、[`13`](../../01-developer/13-编辑器与模板生产.md)、[`16`](../../01-developer/16-测试CI与发布门禁.md)、[`17`](../../01-developer/17-平台构建部署与运维.md) 与 01/02/03。
9. 由唯一集成 Owner Review 后集成到 `主开发`；禁止直接推送 `main`。验收失败时继续留在隔离分支，不修改稳定 0.8.5。

## 6. 风险与回退

- 1.5 的作者源、package sections、compiler/provider、C ABI/JS client、Gameplay、Host 和 Workbench 是一个整体候选，不得只挑一层合入。
- `SandboxEncounterSession` 的 authored initialize 当前先走 legacy 初始化再重建 authored runtime；恢复后必须 Review 是否消除重复构建。
- authored profile 的 stance 数量目前依赖 package validator；恢复后应在 Session 接线边界补显式容量检查，避免未来非包调用绕过。
- 全量 MSVC 红门是锁定 Node 传播问题；修复目标是让生成步骤使用既有锁定 `20.18.0`，不是接受 `22.20.0`。
- Web 构建成功只证明产物可链接，不证明输入可用性、敌人职责可读性、精英阶段、真实节奏或人工手感。
- 放弃候选时保留 `d94d97f8fc27feb186ae1535a49006d535bbff68` 供审计，通过新 revert/替代提交处理；禁止 reset、强制删除分支或清理 worktree。
- 稳定回退点是根 `主开发` 的 `e62a1785e793b95866ebbb3ad6e9243a62b3ac31`。构建目录是可再生缓存，不属于产品完成证据。
