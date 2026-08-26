# 19｜ADR 索引

> 上级文档：[`../01-开发者文档.md`](../01-开发者文档.md)
> 状态：Active
> 最后更新：2026-07-11
> 维护者角色：技术负责人；提案 Owner 与受影响领域负责人会签

## 1. 规则

ADR 记录长期决策的背景、替代方案、后果、验证和回滚。接受后不改写历史结论；新证据用新 ADR `Supersedes`。旧 ADR 只把状态标为 Superseded，正文保留当时理由。

## 2. 当前有效决策

| ADR | 状态 | 决策 | 影响 |
| --- | --- | --- | --- |
| [ADR-0005](adr/ADR-0005-Cpp网页首发与Axmol宿主.md) | Accepted | C++20/Axmol/Emscripten 浏览器首发，原生多端复用 | 00、10、14、17 |
| [ADR-0006](adr/ADR-0006-Cpp分层核心与Wasm平台边界.md) | Accepted | C++ GameCore 与 Axmol/Web/Native 分层，单线程 WASM 基线 | 01、10、11、13、16 |
| [ADR-0007](adr/ADR-0007-浏览器内容包与存档同步契约.md) | Accepted | `.tgdpack`、IndexedDB、版本线与幂等 Operation | 06、12、14、15 |
| [ADR-0008](adr/ADR-0008-本地优先与多端云同步.md) | Accepted | Guest 可玩、本地优先、C++ 同步服务、领域合并 | 12、15、17、18 |
| [ADR-0009](adr/ADR-0009-斗战神优先的2.5D斜向全景.md) | Accepted | 《斗战神》第一指导的 2.5D 斜向全景、地面纵深与受控镜头 | 00、02、04、08、10、11、14 |

## 3. 已取代决策

| ADR | 状态 | 原决策 | 被取代原因 |
| --- | --- | --- | --- |
| [ADR-0001](adr/ADR-0001-桌面原生与Godot宿主.md) | Superseded by 0005 | Godot/.NET Windows 原生优先 | 项目负责人改为 C++ 网页首发 |
| [ADR-0002](adr/ADR-0002-混合组件与分层核心.md) | Superseded by 0006 | C# 分层核心 | 组件思想保留，语言/平台边界重建 |
| [ADR-0003](adr/ADR-0003-内容包与存档版本契约.md) | Superseded by 0007 | Schema/PCK/桌面存档 | Schema 思想保留，改为 Web 包/IndexedDB/同步 Operation |
| [ADR-0004](adr/ADR-0004-离线优先更新与受限模组.md) | Superseded by 0008 | 桌面离线/PCK/模组 | 改为 Web/PWA、本地优先和跨设备云同步 |

更早的旧 Web 原型 ADR 位于 `codex/archive-legacy-web-v1`，属于独立归档时间线。

## 4. 何时必须写 ADR

- 更换 C++ 标准、Axmol/Emscripten、Web 首发或多端顺序。
- 改变单线程/Pthreads、Platform/Presentation、渲染或主循环边界。
- 改变 2.5D 斜向全景、地面纵深、受控镜头或体验参考优先级。
- 改变 `.tgdpack`、IndexedDB、存档、同步 Operation、账号/身份或冲突模型。
- 引入不可逆的编辑器、资产、CDN、数据库或部署基础设施。
- 开放任意代码模组、实时多人、强联网经济或服务端战斗权威。

局部重构、可逆参数和单一内容设计通常不需要；一旦被多个团队长期依赖则升级为 ADR。

## 5. 模板

```text
# ADR-NNNN｜标题

状态：Proposed / Accepted / Rejected / Superseded
日期：YYYY-MM-DD
决策者：角色/团队
关联：文档、Issue、实验、旧 ADR

## 背景与问题
## 决策驱动因素
## 备选方案
## 决策
## 正面后果
## 负面后果与风险
## 验证与放行门
## 迁移与回滚
## 后续行动
```

提案至少包含两个真实替代方案和“不改变”的后果，不能只列优点。

## 6. 生命周期

1. 创建 Proposed ADR 并绑定里程碑。
2. 产品、C++、Web、内容、后端、安全相关负责人收集实验证据。
3. 接受/拒绝并记录未解决风险。
4. 接受后同步 00–18、技术基线、Schema、守卫和迁移。
5. 在 F1/M1 验证；失败则回滚或新 ADR 取代。
6. 发布事实进入 03，ADR 保留原因。

## 7. 编号

ADR 位于 `docs/01-developer/adr/`，四位数字递增。编号一旦占用不复用，Rejected/Superseded 也保留。索引与 ADR 同提交更新。

## 8. 近期候选 ADR

F1 证据可能触发：

- Axmol Web Preview 是否保留，或改用另一 C++ Presentation 宿主。
- Sprite/骨骼、预渲染与有限实时 3D 的混合比例、许可和性能方案。
- Emscripten 模块拆分、异常/RTTI、内存和 Pthreads 参数。
- JS↔WASM ABI major、线程/缓冲模型若改变 0006 的平台边界。
- `.tgdpack`/Snapshot/Operation 的正式编码与签名方案若形成长期外部格式。
- 内容工作台 DOM 框架和本地工作区桥。
- OIDC 提供方、会话/Cookie 与数据驻留。
- Windows/Android/iOS 具体发布顺序和商店 SDK。

实验完成前它们是 09 的待决问题，不提前写成 Accepted。
