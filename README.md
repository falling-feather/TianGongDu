# 天工渡 / TianGongDu

《天工渡》是一款以“九州百工”为舞台、面向长期更新的 2.5D 斜向全景动作角色扮演游戏。玩家作为年轻的“执灯渡人”，在市井、工事与祟局之间沿地面纵深探索、交往、战斗并让失序重新归位。

整体体验以《斗战神》为第一指导，重点学习其斜向全景构图、纵深空间、角色与场景尺度、东方材质重量和受控镜头调度；《暖雪》只作为俯斜战斗可读性与输入简洁度的第二参考。项目只借鉴高层设计语言，不复刻任何角色、场景、UI、剧情、动作或资产。

主分支采用 **浏览器首发、C++ 核心、多端复用** 的技术路线：C++20 游戏核心由 Emscripten 编译为 WebAssembly，Axmol 2.11.x LTS 负责 2D/2.5D 表现、输入、音频与跨平台宿主；网页以 Canvas 承载游戏画面，以 DOM 承载 HUD、菜单和可访问性。Windows/Android/iOS 复用同一核心，按里程碑逐端验证，而不是维护互不兼容的多套游戏。

当前工作区是 **2.2 开发对接版设计基础**，不冒充已经完成的正式 WASM 游戏。F1 的硬门是先做出“雨夜试伞”网页纵切：约 27 分钟到归位结算，30 分钟覆盖返回、保存、刷新与离线重开；验证 Axmol 当前仍标记为 Preview 的 WebAssembly 路径后再扩大生产。

## 当前成果

- 固定 00–09 共 10 份顶层文档，以及隶属于 01 的 10–19 开发者二级手册。
- 设计 1.0 的 3 个地区、18 个大型子地区、14 个 Boss、24 个核心 NPC、2 套战斗系统和 2 条武器体系。
- 建立可机器校验的 1.0 内容目录、9 类模板注册表（含独立精英机制战）和 C++ Web 技术基线。
- 确立 C++ Contracts / Runtime / Gameplay / Presentation / Platform 分层，玩法状态不归渲染树或 JavaScript 所有。
- 设计 IndexedDB 本地存档、Service Worker/CDN 内容缓存，以及可选的跨设备云同步协议。
- 旧版 Web 原型完整保存在 [`codex/archive-legacy-web-v1`](https://github.com/falling-feather/TianGongDu/tree/codex/archive-legacy-web-v1)，含 V1.1.0 可玩闭环和 52 项自动化测试。

## 从哪里开始

1. 项目负责人先读 [`docs/00-项目总纲.md`](docs/00-项目总纲.md) 与 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)。
2. 开发者从 [`docs/01-开发者文档.md`](docs/01-开发者文档.md) 进入 10–19 二级手册。
3. 技术路线以 [`docs/01-developer/10-技术架构与依赖规则.md`](docs/01-developer/10-技术架构与依赖规则.md) 和 [`content/design/technical-baseline.json`](content/design/technical-baseline.json) 为准。
4. 内容/叙事/关卡团队依次读 [`docs/04-游戏设计总纲.md`](docs/04-游戏设计总纲.md)、[`docs/05-世界与叙事圣经.md`](docs/05-世界与叙事圣经.md)、[`docs/06-内容生产规范.md`](docs/06-内容生产规范.md)、[`docs/07-1.0地区与内容蓝图.md`](docs/07-1.0地区与内容蓝图.md) 与 [`docs/08-UI-UX与可访问性.md`](docs/08-UI-UX与可访问性.md)。

按开发、美术、叙事、关卡、UI、音频、QA/发布分别交接时，直接使用 00 §1.1 的团队阅读路线；开发能力编号见 01 §15，当前 F1 可复制派工卡见 02 §4.3–4.4。05 是世界/叙事文档，技术栈入口是 01 与 10–19。

开发组开工前先统一四个结论：

- **F1 不是缩小版 1.0**：它只验证一条端到端生产与运行链，原型存档使用独立命名空间，不直接迁入正式档；当前几何启动画面只是宿主探针，不代表最终视角或美术方向。
- **C++ GameCore 是玩法真相**：Axmol、DOM、JavaScript、IndexedDB 和云服务都通过契约观察或提交意图，不能各自保存第二份战斗/任务规则。
- **M1 才是首个五小时地区成品**：F1 未通过兼容、存档、加载、性能和工具门之前，不批量生产最终资产。
- **云同步是可选增强**：访客、离线和本地存档先成立；账号、Pthreads、Windows 与移动端都按证据逐门开放。

开发对接的角色所有权、开工清单和 DoR/DoD 见 [`docs/01-开发者文档.md`](docs/01-开发者文档.md)；F1 唯一纵切、30/60/90 天成果门和风险台账见 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)。

## 当前成熟度

| 领域 | 状态 | 已有 | 仍没有 |
| --- | --- | --- | --- |
| 产品/世界/1.0 范围 | Scope Approved | 三地区、战斗/武器、14 Boss、24 NPC、内容预算 | 全量任务/POI 实例和最终平衡 |
| 技术架构 | In Progress（`F1-DEV-01`） | C++/WASM/Axmol 分层、存档/同步/部署合同；可编译 Native CMake target graph、宿主生命周期 smoke、架构 lint；候选工具链缓存/签名已验证，MSVC/Clang Debug/Release 对照已通过 | Axmol Web Single 与真实浏览器/干净 CI 证据，随后提升工具链锁状态 |
| 可玩纵切 | Scope Approved | F1“雨夜试伞”唯一流程与验收 | 新主线 WASM 纵切代码/资产 |
| 内容工具 | Scope Approved | 9 类模板注册表与工作台范围 | 可用 Workbench/ContentCore/baker |
| 本地存档/云同步 | Accepted Baseline | IndexedDB 主路径、Operation/冲突模型 | 实际 DB migration、API、DDL、OIDC |
| 发布运维 | Accepted Baseline | 渠道、缓存、回滚、证据与灾备门 | CI/CD、正式 origin/CDN/监控 |

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

`toolchains/toolchain-lock.json` 当前是 `candidate-being-validated`，不是 G1 放行结论。下载完候选归档后可运行 `npm run validate:toolchain:cache` 逐一核对大小和 SHA-256；只有 Web Single、MSVC/Clang、浏览器生命周期证据与 build ID 全部齐备，才允许同步提升锁文件和技术基线状态。

Windows x64 可用 `npm run bootstrap:toolchain` 在仓库 `.cache/` 内恢复候选工具链。脚本按锁定字节长度断点下载、逐项校验 SHA-256、验证 LLVM 官方分离签名并无安装解包；它不会写入永久 PATH 或系统级 `AX_ROOT/EMSDK`。所有构建通过 `tools/run-toolchain.mjs` 注入本次进程环境，避免 IDE 私有配置和 Windows `Path/PATH` 重复键污染。

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
