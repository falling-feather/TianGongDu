# 天工渡 / TianGongDu

《天工渡》是一款以“九州百工”为舞台、面向长期更新的横版动作角色扮演游戏。玩家作为年轻的“执灯渡人”，在市井、工事与祟局之间探索、交往、战斗并让失序重新归位。

主分支采用 **浏览器首发、C++ 核心、多端复用** 的技术路线：C++20 游戏核心由 Emscripten 编译为 WebAssembly，Axmol 2.11.x LTS 负责 2D 渲染、输入、音频与跨平台宿主；网页以 Canvas 承载游戏画面，以 DOM 承载 HUD、菜单和可访问性。Windows/Android/iOS 复用同一核心，按里程碑逐端验证，而不是维护互不兼容的多套游戏。

当前提交是 2.1 技术设计基础，不冒充已经完成的正式 WASM 游戏。F1 的硬门是先做出 15–30 分钟端到端网页纵切，验证 Axmol 当前仍标记为 Preview 的 WebAssembly 路径，再扩大生产。

## 当前成果

- 固定 00–09 共 10 份顶层文档，以及隶属于 01 的 10–19 开发者二级手册。
- 设计 1.0 的 3 个地区、18 个大型子地区、14 个 Boss、24 个核心 NPC、2 套战斗系统和 2 条武器体系。
- 建立可机器校验的 1.0 内容目录、8 类模板注册表和 C++ Web 技术基线。
- 确立 C++ Contracts / Runtime / Gameplay / Presentation / Platform 分层，玩法状态不归渲染树或 JavaScript 所有。
- 设计 IndexedDB 本地存档、Service Worker/CDN 内容缓存，以及可选的跨设备云同步协议。
- 旧版 Web 原型完整保存在 [`codex/archive-legacy-web-v1`](https://github.com/falling-feather/TianGongDu/tree/codex/archive-legacy-web-v1)，含 V1.1.0 可玩闭环和 52 项自动化测试。

## 从哪里开始

1. 项目负责人先读 [`docs/00-项目总纲.md`](docs/00-项目总纲.md) 与 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)。
2. 开发者从 [`docs/01-开发者文档.md`](docs/01-开发者文档.md) 进入 10–19 二级手册。
3. 技术路线以 [`docs/01-developer/10-技术架构与依赖规则.md`](docs/01-developer/10-技术架构与依赖规则.md) 和 [`content/design/technical-baseline.json`](content/design/technical-baseline.json) 为准。
4. 内容团队读 [`docs/04-游戏设计总纲.md`](docs/04-游戏设计总纲.md)、[`docs/06-内容生产规范.md`](docs/06-内容生产规范.md) 与 [`docs/07-1.0地区与内容蓝图.md`](docs/07-1.0地区与内容蓝图.md)。

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
```

它会检查文档层级、链接、ID、内容数量、地区引用、模板注册表，以及 C++20、Axmol、Emscripten、WebGL2、DOM UI、IndexedDB 和云同步基线。

## 技术基线

- 游戏核心：C++20，固定 60 Hz 模拟，渲染器无权修改持久玩法状态。
- 2D 宿主：Axmol 2.11.4 / 2.11.x LTS；WebAssembly 支持必须先通过 F1 硬门。
- 网页编译：Emscripten SDK，F1 后锁定精确版本；输出 wasm32 + JavaScript loader。
- 网页渲染：WebGL2；首发提供必选单线程构建，多线程构建仅在 COOP/COEP 条件满足时渐进启用。
- 网页外壳：Canvas 游戏画面 + DOM HUD/菜单 + PWA/Service Worker。
- 本地存档：IndexedDB，经异步桥或 Emscripten IDBFS 持久化；浏览器存储不是唯一备份。
- 云同步：可选、访客可玩、本地优先；修订快照 + 幂等操作日志，C++20/Drogon + PostgreSQL 作为参考实现。
- 多端：Web 桌面浏览器为 1.0 必选；Windows 原生在网页纵切后进入 Beta，移动端分别通过触控和平台审核门。
- 内容交付：带哈希的 `.tgdpack`，HTTPS/CDN 分发，Service Worker 与 HTTP Cache 分层缓存。

技术版本不是永久信仰。Axmol Web 路径、线程、内存、加载和移动端适配必须用真实构建验证；失败时替换 Platform/Presentation 宿主，不推翻 C++ Gameplay 与内容契约。
