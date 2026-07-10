# ADR-0005｜C++ 网页首发与 Axmol 宿主

> 状态：Accepted
> 日期：2026-07-10
> 决策者：项目负责人、产品/技术基线
> Supersedes：ADR-0001

## 背景与问题

2.0 选择 Godot/.NET Windows 桌面优先，但项目负责人不熟悉 Godot，并明确要求改为 C++ 网页端游戏，产品形态参考《造梦西游》的横版动作、低进入门槛和长期更新，同时考虑多端接续。

旧版曾设想 Axmol/WASM，但问题不是 C++/Web 本身，而是没有先固定内容工具、状态所有权、资源分包、浏览器生命周期和同步边界。新方案必须避免把旧 Demo 原样复活。

## 决策驱动因素

- C++ 是项目要求和长期 Gameplay 主语言。
- 浏览器无需安装、便于发布和快速进入。
- 横版 2D 动作适合 Sprite/骨骼动画和 WebGL2。
- 同一核心可编译 Windows/Android/iOS，支持多端进度接续。
- 大型内容需要宿主之外的模板、内容包和编辑器。
- 浏览器加载、内存、存储、线程和生命周期必须一开始成为硬门。

## 备选方案

### A. Phaser/TypeScript 浏览器原生

2D Web 生态成熟、开发快，但不满足 C++ 核心要求；未来原生端需要新宿主/包装。拒绝作为 GameCore，可参考其 Web UX/测试实践。

### B. Axmol 2.11.x LTS + Emscripten

C++、2D、多平台、WebAssembly/WebGL2 与原生目标契合。风险是官方仍把 WebAssembly 标为 Preview，内容编辑器需自建。选择，但以 F1 为硬门且宿主可替换。

### C. SDL/WebGL2 自建 2D 宿主

控制和可移植性高，但需要自建场景、动画、图集、相机、音频和扩展，当前成本高。作为 Axmol Web 失败时的后备，不作为首选。

### D. Godot Web/C++ 扩展或桌面原生

保留成熟编辑器，但不符合项目负责人熟悉度和 C++ 网页主线决策。2.0 资料作为历史，不继续。

## 决策

游戏采用 C++20 核心，Axmol 2.11.4/2.11.x LTS 为 2D 宿主；Web 通过 Emscripten 输出 wasm32，WebGL2 为 1.0 渲染基线。桌面浏览器是 1.0 必选平台；Windows 原生先做对照/Beta，Android/iOS 后续逐端放行。

Axmol 只在 `presentation/platform`，不拥有 Gameplay、存档和内容契约。Canvas 承载游戏画面，DOM 承载 HUD/菜单/可访问性；PWA/Service Worker/IndexedDB 属于 Web Platform。

## 正面后果

- 满足 C++ 与网页首发要求。
- 横版 2D 使用现有引擎基础，不从零写渲染/音频/输入。
- C++ GameCore 可直接用于原生端和服务器共享 DTO/校验。
- 浏览器发布与 CDN 内容更新迭代快。
- 宿主可替换，内容/玩法不再绑定场景树。

## 负面后果与风险

- Axmol WebAssembly 为 Preview，可能需要维护上游补丁或换宿主。
- WASM 初始体积、线性内存、纹理上传和调试比 TypeScript 复杂。
- 浏览器线程需要 COOP/COEP，必须维护单线程兼容产物。
- DOM 与 Canvas 双层 UI 需要窄桥和焦点/输入协调。
- 自建内容工作台、包、存档和同步服务的工程量真实存在。
- 多端共享核心不自动解决触控、性能和商店审核。

## 验证与放行门

F1 必须完成：

- Axmol 2.11.4 C++20 项目可重现构建 Web Single/Pthreads 与 Windows。
- Chrome/Edge/Firefox 的启动、输入、音频、全屏、隐藏/恢复、WebGL context 恢复。
- 15–30 分钟纵切、固定 60 Hz、DOM UI、内容分包与 IndexedDB。
- WASM ≤12 MiB/首访 ≤35 MiB 等起始预算，长时 heap/纹理回落。
- 同一回放在 Web/Windows 得到一致离散状态。
- Axmol Web 上游风险、需要的补丁和升级策略形成报告。

Axmol Host 不通过时不能靠删掉内容深度来放行。

## 迁移

世界、1.0 内容、Schema、模板和混合组件思想保留。2.0 C# 命名改为 C++ targets；Godot Scene/Resource/PCK 相关设计不迁移。旧 Web 原型只作为交互参考，不复制运行时代码。

## 回滚

如果 Axmol Web Host 在 F1 无法达到兼容、性能或维护硬门，新建 ADR 比较 SDL/WebGL2、其他 C++ 2D 宿主或 TypeScript Presentation；保持 C++ Contracts/Runtime/Gameplay/Content/Sync 不变。

## 官方依据

- [Axmol 官方仓库](https://github.com/axmolengine/axmol)
- [Emscripten 官方文档](https://emscripten.org/docs/)
- [MDN WebAssembly](https://developer.mozilla.org/en-US/docs/WebAssembly)
