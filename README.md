# 天工渡 / TianGongDu

《天工渡》是一款以“九州百工”为舞台、面向长期更新的单机动作角色扮演游戏。玩家作为年轻的“执灯渡人”，在繁盛而真实运转的市井、工事与祟局之间行走，把失序的器物、工序、关系和记录重新归位。

主分支已经进入 2.0 重构：正式客户端从 Web/WASM 原型转为 Windows 原生端游，采用 Godot 4.7 .NET、C# 和数据驱动内容管线。当前提交是工程与 1.0 内容的“设计基础节点”，不是伪装成成品的技术 Demo。

## 当前成果

- 固定了不超过 10 份的顶层文档体系，以及隶属于 01 开发者文档的 10–19 二级手册。
- 设计了 1.0 的 3 个地区、18 个大型子地区、14 个 Boss、24 个核心 NPC、2 套战斗系统与 2 条武器体系。
- 建立可机器校验的 `v1-content-catalog.json` 与 8 类模板注册表，防止内容数量、引用关系、生产接口与游玩时长在迭代中悄悄缩水。
- 确立 Runtime、Gameplay、Presentation、Editor 与内容契约的边界，以及角色/Boss/任务/地区的模板生产流程。
- 旧版 Web 项目已在 [`codex/archive-legacy-web-v1`](https://github.com/falling-feather/TianGongDu/tree/codex/archive-legacy-web-v1) 分支完整归档；该分支含 V1.1.0 阶段可玩闭环和 52 项自动化测试。

## 从哪里开始

1. 项目负责人先读 [`docs/00-项目总纲.md`](docs/00-项目总纲.md) 与 [`docs/02-版本规划与验收.md`](docs/02-版本规划与验收.md)。
2. 开发者从 [`docs/01-开发者文档.md`](docs/01-开发者文档.md) 进入 10–19 二级手册。
3. 内容与关卡团队读 [`docs/04-游戏设计总纲.md`](docs/04-游戏设计总纲.md)、[`docs/06-内容生产规范.md`](docs/06-内容生产规范.md) 和 [`docs/07-1.0地区与内容蓝图.md`](docs/07-1.0地区与内容蓝图.md)。
4. 世界与叙事团队以 [`docs/05-世界与叙事圣经.md`](docs/05-世界与叙事圣经.md) 为唯一上位依据。

## 仓库布局

```text
apps/                 Godot 客户端与编辑器宿主（Foundation 阶段落地）
src/                  与引擎解耦的 C# Contracts / Runtime / Gameplay
content/              Schema、模板和版本化内容包
assets_src/           DCC 源资产，接入 Git LFS 后启用
assets_runtime/       可发布资源与生成清单
server/               可选控制面：版本清单、热更新、遥测
tools/                内容校验、导入、构建与发布工具
tests/                契约、运行时、内容、场景和性能测试
docs/                 00–09 顶层文档；01-developer/ 下为 10–19
```

## 设计校验

无需安装第三方依赖，只需 Node.js 20 或更高版本：

```bash
npm test
npm run validate:design
```

校验覆盖文档层级、ID 唯一性、跨地区引用、最低内容数量、每地区 5 小时主线预算、Boss 阶段与 NPC 交互深度。

## 当前技术基线

- 客户端/编辑器宿主：Godot 4.7 stable .NET，Windows x64 优先，Forward+ 渲染。
- 核心代码：C#；纯逻辑与 Godot Node 分离，固定 60 Hz 模拟。
- 内容契约：JSON Schema Draft 2020-12；稳定 ID、包清单和显式迁移。
- 离线工具：Node.js 用于轻量验证，Python 用于批量导入与 DCC 处理。
- 资产：Blender/Krita/Aseprite 等源文件，经确定性导入生成运行时资源；大文件使用 Git LFS。
- 发布：完整安装包 + 受签名内容包；1.0 不开放任意 C# 模组执行。

技术版本属于基线而非永久信仰；变更必须通过 ADR、兼容性实验、迁移方案和回滚方案。
