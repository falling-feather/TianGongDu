# 天工渡 / Heavenwrights

> 文档定位：项目入口，负责说明项目是什么、如何启动、关键文档在哪里。  
> 适用范围：当前 M0 浏览器原型、文本化内容数据和长期工程方向。  
> 最后更新：2026-07-01  
> 维护者：项目维护者  
> 关联文档：`doc/README.md`、`doc/01_开发者手册.md`、`doc/02_项目规划与验收.md`、`doc/03_项目全局设计规划.md`、`doc/12_测试与验收指南.md`、`CHANGELOG.md`

《天工渡》是一款 Web 端优先的 2D / 类 2.5D 像素风动作 RPG。当前仓库处于 M0 到 M1 过渡阶段：轻量浏览器 demo 继续验证“江南雨巷 + 纸伞门径”的首屏体验，文档与内容数据侧已按第一版投放规格拆解江南地区的主线、人物、子地区、资源、音频和数据结构。

项目北极星：

> 让“工艺”成为动作，让“师承”成为成长，让“归位”成为战斗终点。

## 当前状态

- 文档集中在 `doc/`，入口见 `doc/README.md`。
- 长期工程边界和目录约定见 `doc/01_开发者手册.md`。
- 短期目标、验收标准和不做事项见 `doc/02_项目规划与验收.md`。
- 全局设计、地区规划、交互模式和基础数值准绳见 `doc/03_项目全局设计规划.md`。
- 江南第一版投放的剧情、人物、子地区、资产和数据结构见 `doc/07_江南雨巷纵切规格.md`。
- 江南 M1 内容数据草案见 `content/regions/jiangnan_rain_alley.json` 及其索引的子地区、任务、NPC、建筑、交互、敌人、Boss、资产和音频集合。
- 回归命令、人工验收和发布检查见 `doc/12_测试与验收指南.md`。
- 已完成事实和剩余风险见 `CHANGELOG.md`。
- 长期技术路线保留为 C++20/23 + Axmol + WebAssembly/WebGL2 + 自研 Runtime + Python 内容工具链。
- 当前 demo 是无外部依赖的浏览器原型，用于快速验证画面、输入、HUD、风息状态和四维选择，不替代最终 Axmol 实现。

## 快速运行

```bash
npm test
npm run validate:content
npm run dev
```

默认本地地址：

```text
http://127.0.0.1:4174/
```

## 目录

```text
doc/                  设计文档、规划、开发者手册、测试指南、ADR 和概念图
content/              文本化内容数据
prototype/web-demo/   当前浏览器 demo
source/               未来 C++ / Axmol / Runtime 分层
tools/                内容校验、开发服务器和自动化工具
tests/                轻量测试
server/               后续运营服务占位
third_party/          后续 Axmol 等外部依赖占位
```

## demo 操作

- `WASD` / 方向键：移动
- `Shift` / `Space`：冲刺
- `Q`：架伞 / 格挡
- `E`：收风
- `R`：借风越行
- `F`：交互 / 归位
