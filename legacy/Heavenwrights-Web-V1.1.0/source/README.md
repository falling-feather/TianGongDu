# Source Layer

> 文档定位：说明未来 C++ / Axmol / Heavenwrights Runtime 的分层入口。  
> 适用范围：后续 `source/` 目录下的 C++ 客户端、框架适配和 Runtime 代码。  
> 最后更新：2026-06-28  
> 维护者：技术维护者  
> 关联文档：`../doc/01_开发者手册.md`、`../doc/03_项目全局设计规划.md`、`../doc/04_技术栈与生产管线.md`、`../doc/adr/ADR-0001-客户端主框架与发布目标.md`

这里保留未来 C++ / Axmol / Heavenwrights Runtime 的分层入口。

当前 M0 尚未接入 Axmol，浏览器 demo 位于 `prototype/web-demo/`。后续接入时保持以下边界：

- `app/`：Axmol 应用入口、平台启动和主循环接入。
- `framework/`：渲染、输入、音频、平台与资源接口适配。
- `runtime/`：固定逻辑帧、实体、碰撞、战斗、技能、工艺、NPC、任务与世界状态。
- `game/`：具体游戏装配、地区加载和表现层 glue code。

硬规则：核心战斗、技能、NPC、工艺和四维状态不要写死在具体场景回调里。
