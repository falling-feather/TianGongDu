# ADR-0006｜C++ 分层核心与 WASM/Native 平台边界

> 状态：Accepted
> 日期：2026-07-10
> 决策者：项目技术基线
> Supersedes：ADR-0002

## 背景与问题

2.0 的混合组件/分层思想正确，但实现语言与宿主是 C#/Godot。改为 C++/WASM 后，浏览器异步 API、线性内存、JS 桥、单线程/Pthreads 和页面生命周期会诱使平台代码渗入玩法；多端又容易形成大量 `#ifdef` 分叉。

## 驱动因素

- 一个 C++ Gameplay 同时编译 Web/Windows/移动端。
- 渲染、DOM、文件、网络、账号和服务端不拥有权威状态。
- Web 单线程是完整基线，Pthreads 只是增强。
- Native 快速单测、WASM 真实集成和回放一致。
- C++ 所有权、生命周期和外部输入边界清楚。
- Axmol 可替换，不推翻内容和存档。

## 备选方案

### A. Axmol Scene/Node 直接写全部玩法

上手快，但存档、同步、WASM/Native 回放、内容工具和宿主替换困难。拒绝。

### B. 大量平台宏共享代码

短期能编译，长期形成 Web/Native 隐形分支，测试矩阵不可控。拒绝。

### C. 完整 Archetype ECS

高性能但非当前核心问题，复杂任务/Boss/关系仍需领域状态机。暂不选。

### D. 分层 GameCore + 混合组件 + Platform Interface

组合、测试和多端边界平衡。选择。

## 决策

建立 `contracts ← runtime ← gameplay ← presentation_axmol ← platform` 单向依赖。GameCore 固定 60 Hz，使用安全实体句柄、组件池、顺序系统、命令/事件/快照和领域状态机。

Web/Native 通过 `IStorage/INetwork/IIdentity/ILifecycle/IAssetTransport/IPhysicsQuery` 适配。JavaScript 使用窄 C ABI/受控消息桥；DOM 不访问组件。平台宏仅存在 Platform/Build。

Web Single 是功能完整的必选产物；Pthreads 是独立构建，在 COOP/COEP 满足时启用。两者与 Native 使用同一 Gameplay/内容/存档。

## 正面后果

- 纯核心可在原生高速测试，浏览器只测真实边界。
- Axmol/DOM/context 重建不丢权威状态。
- 原生端不会重写战斗、任务和关系。
- 内容模板组合功能，不产生深继承树。
- 同步 Operation 从领域事务生成，不上传逐帧细节。

## 负面后果与风险

- 需要桥、快照、平台接口和适配测试，样板多于直接写 Scene。
- C++ 所有权/异步 generation 需要严格纪律。
- 跨 Web/Native 浮点与物理不天然位级确定。
- 单线程预算要求重任务切片；线程版又有部署头和双产物成本。
- “混合”可能被滥用，必须用层级 lint 和系统阶段守卫。

## 验证

- Gameplay target 不 include Axmol/Emscripten/JS/Drogon。
- 模板创建 NPC/Boss 不新增专属 C++ 类/ID 特判。
- Web Single/Pthreads/Windows 回放的离散状态与持久 Operation 一致。
- 页面隐藏/context lost/Presentation 重建后 Session 正确。
- 两个异步请求跨 Session 返回时，旧 generation 不污染新状态。
- Cell 反复卸载后句柄 generation、组件池和内存稳定。

## 迁移/回滚

F1 先实现最小 EntityRegistry、Tick、命令/事件、快照、Platform 接口和 Axmol View。性能不足先优化组件池/arena/批次；只有实测证明必要时才用新 ADR 更换内部存储，不改变内容/存档 API。
