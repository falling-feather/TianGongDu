# 天工渡 / TianGongDu

《天工渡》是一款以“九州百工”为舞台、面向长期更新的 2.5D 斜向全景动作角色扮演游戏。玩家作为年轻的“执灯渡人”，在市井、工事与祟局之间沿地面纵深探索、交往、战斗并让失序重新归位。

整体体验以《斗战神》为第一指导，重点学习其斜向全景构图、纵深空间、角色与场景尺度、东方材质重量和受控镜头调度；《暖雪》只作为俯斜战斗可读性与输入简洁度的第二参考。项目只借鉴高层设计语言，不复刻任何角色、场景、UI、剧情、动作或资产。

主分支采用 **浏览器首发、C++ 核心、多端复用** 的技术路线：C++20 游戏核心由 Emscripten 编译为 WebAssembly，Axmol 2.11.x LTS 负责 2D/2.5D 表现、输入、音频与跨平台宿主；网页以 Canvas 承载游戏画面，以 DOM 承载 HUD、菜单和可访问性。Windows/Android/iOS 复用同一核心，按里程碑逐端验证，而不是维护互不兼容的多套游戏。

当前工作区是 **2.2 开发对接版设计基础**，不冒充已经完成的正式 WASM 游戏。F1 的硬门是先做出“雨夜试伞”网页纵切：玩家获得控制到任一归位结算首玩不少于 60 分钟，70 分钟端到端预算再覆盖标题/加载、返回、保存、刷新与离线重开；不得以加载、挂机、重复刷怪或失败重试凑时。验证 Axmol 当前仍标记为 Preview 的 WebAssembly 路径后再扩大生产。

## 当前成果

- 固定 00–09 共 10 份顶层文档，以及隶属于 01 的 10–19 开发者二级手册。
- 设计 1.0 的 3 个地区、18 个大型子地区、14 个 Boss、24 个核心 NPC、2 套战斗系统和 2 条武器体系。
- 建立可机器校验的 1.0 内容目录、9 类模板注册表（含独立精英机制战）和 C++ Web 技术基线。
- 确立 C++ Contracts / Runtime / Gameplay / Presentation / Platform 分层，玩法状态不归渲染树或 JavaScript 所有。
- 完成 F1 固定 60 Hz `GameSession`、斜向世界输入与版本化命令回放；同一 C++ 黄金夹具已通过 Native 双编译器双配置和 Web Single 三浏览器验证。
- 已实现 Web ABI 1.0、`SaveEnvelopeV1`、Profile 原子提交协调器和 256 KiB 分块存储桥；Guest IndexedDB v1 首次保存与刷新恢复已通过三浏览器阶段门，配额/CAS/损坏导出/离线实现等待下一个阶段门聚合复验，迁移/导入仍保留为缺项。
- 已将 F1 的 5 个 Cell、7 个目标驱动玩法段、斜向相机基和 60/70 分钟口径固化为 JSON→C++ 机器合同，并用 `VerticalSliceSession` 组合既有移动核心。
- 已实现数据驱动的首个战斗遭遇合同与确定性 `ICombatResolver` / `ICombatEventSink` / `IEncounterDirector` 启动边界：当前 Web 灰盒可移动接敌，使用檐守/翻花、轻重击、守势和闪身；两个专用训练构件、两伞偶、纸鹭与 Boss 全部默认休眠，由 6 项 Beat/Objective 激活定义按内容启用，训练和伞巷都可在同 Beat Objective 完成后切换下一波；每次激活还携带逐 Actor Home Pose/Formation Slot，已形成单体教学、伞巷对向夹击与返程三角包围三类落位。`CombatActorSnapshot.defeated` 明确区分休眠与真实击败。Active 敌人会按量化地面坐标追击/归位、占据内容指定的接敌槽位并由单一进攻令牌轮流出招，资源恢复与失败重试已进入同一权威 Tick 边界。
- 已实现确定性 `IQuestRuntime`、目标图快照/事件/校验和、内容驱动的场景交互、战斗信号、敌群结果、稳定选择、Boss 阶段与结算奖励收据解析；玩家可连续完成全部 7 Beat：雨渡、沈砚五步实操训练（檐守重击→格挡反制→切换翻花→翻花轻击→闪避反制）、伞巷清场/路线、工灯调查/调校、回程战/捷径、四时伞祟春夏秋冬四阶段，再选择直接制伏或恢复共同工印并返回沈砚。奖励收据带稳定 `rewardDedupKey`，但写入 Profile/Persistent Operation 的事务提交仍保留在 `ISnapshotContributor` 边界，不冒充已经持久发奖。
- 已设计 Service Worker/CDN 内容缓存，以及可选的跨设备云同步协议；设计不等于运行时已落地。
- 旧版 Web 原型完整保存在 [`codex/archive-legacy-web-v1`](https://github.com/falling-feather/TianGongDu/tree/codex/archive-legacy-web-v1)，含 V1.1.0 可玩闭环和 52 项自动化测试。

当前 Web 灰盒控制为：`WASD/方向键` 移动、`F` 交互、`Space` 纵跃、`J` 轻击、`K` 重击、`Shift` 守势、`C` 闪身、`1/2` 切换檐守/翻花，倒地后按 `R` 从当前 Beat 的内容安全点重试。它已经能直观看到《斗战神》第一指导的斜向纵深、深度排序、遮挡层次与大型 Boss 尺度，并连续操作全部 7 Beat：雨渡开场已从文书/开门两步扩为文书、水痕、系泊、引路灯、作坊铃和渡门 6 节点准备链；沈砚训练是严格有序的“檐守重击→格挡→切翻花→轻击→闪避”五步实操，入场伞偶构件在格挡 Objective 完成后由纸鹭构件接替；正式伞巷再由两只漏雨伞偶首轮切为单纸鹭第二轮，随后调查/调校、回程战/捷径、四季 Boss 换相、双结算选择和返回沈砚均已接通。开场/训练 Wave 1/训练 Wave 2/伞巷 Wave 1/伞巷 Wave 2 Active 敌人数由浏览器固定验证为 0/1/1/2/1，Stage/Objective 的敌群切换只在战斗回调外的 Tick 安全点应用。7 个 authored movement safe point 会在开局前完成碰撞预检并随 Beat 推进，伞巷、工位、Boss 门前和结算段不再一律退回开场。HUD 现同步显示合格玩法、闲置、失败重试和逐 Beat 预算达标数；确定性审计只把 180 Tick 活动窗口计入，失败时把当前尝试整段转入排除桶，并要求 7 个 Beat 分别达标。它仍不是可连续首玩 1 小时的 Demo；六节点准备链、训练和伞巷两轮都尚未填满各 Beat 的分钟预算，审计能拒绝伪时长但不能替代非重复内容填充和真人盲测，真实 60 分钟节奏/盲测、奖励持久事务、正式鼠标/手柄动作适配，以及包含战斗/任务/Profile 的跨域事务快照仍是缺项。

## 从哪里开始

1. 项目负责人先读 [`docs/00-项目总纲.md`](docs/00-项目总纲.md) 与 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)。
2. 开发者从 [`docs/01-开发者文档.md`](docs/01-开发者文档.md) 进入 10–19 二级手册。
3. 技术路线以 [`docs/01-developer/10-技术架构与依赖规则.md`](docs/01-developer/10-技术架构与依赖规则.md) 和 [`content/design/technical-baseline.json`](content/design/technical-baseline.json) 为准。
4. 内容/叙事/关卡团队依次读 [`docs/04-游戏设计总纲.md`](docs/04-游戏设计总纲.md)、[`docs/05-世界与叙事圣经.md`](docs/05-世界与叙事圣经.md)、[`docs/06-内容生产规范.md`](docs/06-内容生产规范.md)、[`docs/07-1.0地区与内容蓝图.md`](docs/07-1.0地区与内容蓝图.md) 与 [`docs/08-UI-UX与可访问性.md`](docs/08-UI-UX与可访问性.md)。

按开发、美术、叙事、关卡、UI、音频、QA/发布分别交接时，直接使用 00 §1.1 的团队阅读路线；开发能力编号见 01 §15，当前 F1 可复制派工卡见 02 §4.3–4.4。05 是世界/叙事文档，技术栈入口是 01 与 10–19。

开发组开工前先统一四个结论：

- **F1 不是缩小版 1.0**：它只验证一条端到端生产与运行链，原型存档使用独立命名空间，不直接迁入正式档；当前代码绘制的雨夜战斗灰盒用于验证《斗战神》优先的斜向全景构图、纵深、尺度和控制，不代表最终美术、动作或 UI。
- **C++ GameCore 是玩法真相**：Axmol、DOM、JavaScript、IndexedDB 和云服务都通过契约观察或提交意图，不能各自保存第二份战斗/任务规则。
- **M1 才是首个五小时地区成品**：F1 未通过兼容、存档、加载、性能和工具门之前，不批量生产最终资产。
- **云同步是可选增强**：访客、离线和本地存档先成立；账号、Pthreads、Windows 与移动端都按证据逐门开放。

开发对接的角色所有权、开工清单和 DoR/DoD 见 [`docs/01-开发者文档.md`](docs/01-开发者文档.md)；F1 唯一纵切、30/60/90 天成果门和风险台账见 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)。

## 当前成熟度

| 领域 | 状态 | 已有 | 仍没有 |
| --- | --- | --- | --- |
| 产品/世界/1.0 范围 | Scope Approved | 三地区、战斗/武器、14 Boss、24 NPC、内容预算 | 全量任务/POI 实例和最终平衡 |
| 技术架构 | In Progress（`F1-GAME-01`，并行收口 `F1-DEV-03` 证据） | 精确工具链与分层；60 Hz Session、量化回放；Web ABI/Profile；F1 Definition Provider、组合纵切会话、确定性任务/交互/战斗解析、事件与权威位姿批次 | IndexedDB 异常/离线聚合复验、迁移/导入、完整玩法纵切和真实设备性能证据 |
| 可玩纵切 | In Progress（7/7 Beat 功能闭环 + 可操作战斗/调查/Boss/结算灰盒） | F1 唯一流程、5 Cell/7 Beat 机器合同；代码绘制的 2.5D 斜向全景雨夜场景；雨渡交互、沈砚五步有序双架势训练、失败重试、伞巷 2→1 两轮读招清场、路线选择、三证据工灯调查、双调校、回程战/捷径、四季 Boss 四阶段、双结算和返沈砚；7 个内容驱动移动安全点；合格/闲置/失败重试与逐 Beat 预算审计；资源恢复、任务选择和已提交进度保持 | 真实 60 分钟非重复内容密度/盲测、奖励 Profile 事务、复杂 AI/掉落、真实 5 Cell、跨域事务快照和正式资产 |
| 内容工具 | In Progress（Bootstrap） | 9 类模板注册表、工作台范围、F1 纵切、17 项场景交互、7 项安全点、5 项带 Objective 前置的战斗任务触发器、3 项敌群结果、6 项含 Home Pose/Formation Slot 的 Beat/Objective 敌群激活、4 项 Boss 阶段、2 项结算奖励映射及 7 实体/17 能力战斗包的 JSON/Schema/确定性 C++ 生成 | 可用 Workbench、复合波次/叠加组、动态队形/多令牌协同、通用 ContentCore/baker、迁移、资源预览与错误定位 UI |
| 本地存档/云同步 | In Progress（`F1-DEV-03`） | IndexedDB v1 六 store、`SaveEnvelopeV1`、Profile Head CAS/C++ 异步桥、Guest 首存与刷新恢复 | 配额/冲突/损坏/离线证据、迁移/导入、多标签主动接管、云 API/DDL/OIDC |
| 发布运维 | Accepted Baseline | 渠道、缓存、回滚、证据与灾备门；F1 Windows 2022 干净 CI 已落地 | CD、正式 origin/CDN/监控与演练 |

`Scope Approved` 或 `Accepted Baseline` 都不等于 `Implemented`。状态词统一定义在 [`docs/09-术语与索引.md`](docs/09-术语与索引.md)。

## 目标仓库布局

```text
apps/
  web-shell/             HTML/DOM、PWA、Service Worker、JS↔WASM 桥
  native-shell/          Windows/Android/iOS 平台入口与商店适配
  content-workbench/     面向设计者的网页内容编辑器
src/
  contracts/             稳定 ID、命令、事件、存档与同步契约
  runtime/               时钟、实体、世界、内容包、存档与流式
  gameplay/              战斗、AI、任务、关系、工艺
  presentation-axmol/    Axmol 场景、动画、音频和输入适配
  platform/              Web/Native 文件、网络、账号与生命周期适配
content/                 Schema、模板、源内容和版本化目录
assets_src/              DCC 源资产，Git LFS 管理
assets_runtime/          图集、音频、字体和可发布内容包
server/sync-service/     C++20/Drogon 云存档同步服务
tools/                   内容编译、构建、发布与迁移工具
tests/                   原生、WASM、浏览器、同步与内容测试
docs/                    00–09；01-developer/ 下为 10–19 与 ADR
```

## 设计校验

当前 Foundation 校验无需第三方依赖，只需 Node.js 20 或更高版本：

```bash
npm test
npm run validate:design
npm run validate:toolchain
```

它会检查文档层级/元信息/链接、稳定 ID、21 章与分钟、18 支线与 24 NPC 参与关系、25 敌人家族、F1 纵切引用、9 类模板、Action/Context/默认输入映射、14 组开发对接合同，以及 C++20、Axmol、Emscripten、WebGL2、DOM UI、IndexedDB 主路径和云同步基线。工具链校验还会拒绝浮动版本、非官方来源、越出工作区的缓存路径，以及没有 SHA-256 却声称已验证的产物。

`toolchains/toolchain-lock.json` 已由 `g1-gh-29152364317-1` 提升为 `locked-and-validated`。`npm run validate:toolchain:cache` 会继续逐一核对大小、SHA-256、LLVM 官方分离签名和安装态版本；该状态只放行 `F1-DEV-01` 的构建/宿主工具链，不等于 G1 整体或正式可玩纵切已经通过。

Windows x64 可用 `npm run bootstrap:toolchain` 在仓库 `.cache/` 内恢复锁定工具链。脚本按锁定字节长度断点下载、逐项校验 SHA-256、验证 LLVM 官方分离签名并完成无安装解包；它不会写入永久 PATH 或系统级 `AX_ROOT/EMSDK`。所有构建通过 `tools/run-toolchain.mjs` 注入本次进程环境，避免 IDE 私有配置和 Windows `Path/PATH` 重复键污染。

当前脚本执行项目语义守卫并解析 Schema；完整 JSON Schema Draft 2020-12 validator、pack/save/API 机器合同与负向 fixture 属于 G1/G3 工具交付，限制见 [`docs/01-developer/16-测试CI与发布门禁.md`](docs/01-developer/16-测试CI与发布门禁.md)。

## 技术基线

- 游戏核心：C++20，固定 60 Hz 模拟，渲染器无权修改持久玩法状态。
- 2D/2.5D 宿主：Axmol 2.11.4 / 2.11.x LTS；WebAssembly 支持必须先通过 F1 硬门，最终 Sprite、骨骼、预渲染与有限实时 3D 组合由代表样片放行。
- 网页编译：Emscripten SDK，G1 宿主门结束前锁定精确版本；输出 wasm32 + JavaScript loader。
- 网页渲染：WebGL2；首发提供必选单线程构建，多线程构建仅在 COOP/COEP 条件满足时渐进启用。
- 网页外壳：Canvas 游戏画面 + DOM HUD/菜单 + PWA/Service Worker。
- 本地存档：Profile 只经直接异步桥事务写入 IndexedDB；IDBFS 仅兼容非 Profile 文件，不构成第二份真相；浏览器存储不是唯一备份。
- 云同步：可选、访客可玩、本地优先；修订快照 + 幂等操作日志，C++20/Drogon + PostgreSQL 作为参考实现。
- 多端：Web 桌面浏览器为 1.0 必选；Windows 原生在网页纵切后进入 Beta，移动端分别通过触控和平台审核门。
- 内容交付：带哈希的 `.tgdpack`，HTTPS/CDN 分发，Service Worker 与 HTTP Cache 分层缓存。

技术版本不是永久信仰。Axmol Web 路径、线程、内存、加载和移动端适配必须用真实构建验证；失败时替换 Platform/Presentation 宿主，不推翻 C++ Gameplay 与内容契约。
