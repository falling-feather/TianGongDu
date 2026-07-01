# Content Layer

> 文档定位：说明文本化内容目录的职责和最低数据约束。  
> 适用范围：`content/**/*.json`、当前 M0 “江南雨巷 + 纸伞门径”内容样例，以及 M1 江南第一版投放的数据扩容计划。  
> 最后更新：2026-07-01  
> 维护者：内容维护者  
> 关联文档：`../doc/01_开发者手册.md`、`../doc/03_项目全局设计规划.md`、`../doc/09_AI协作内容管线.md`、`../doc/12_测试与验收指南.md`

文本化内容是项目对策划、程序和 AI 协作开放的稳定接口。当前 M0 数据仍保留“江南雨巷 + 纸伞门径”的首屏 demo 样例；M1 江南第一版投放已开始按 `../doc/07_江南雨巷纵切规格.md` 拆出子地区、任务、建筑、交互、Boss 阶段、资产和音频对象。

约束：

- 每个内容对象必须有 `schemaVersion`、`id`、`displayNameKey`、`tags`、`designerNote`。
- 交叉引用只使用稳定 ID。
- 中文显示文本进入 `localization/zh-CN.json`。
- 运行 `npm run validate:content` 检查基本结构。

## M1 江南内容包

`regions/jiangnan_rain_alley.json` 是江南雨巷的地区入口。长期加载顺序应从 RegionDef 读取索引，再按 ID 拉取对应集合：

- `subregions/jiangnan_rain_alley_subregions.json`：11 个子地区，含 7 个 M1 核心节点。
- `quests/jiangnan_rain_alley_quests.json`：1 条主线、10 个主线步骤、8 条支线和回访互动。
- `npcs/rain_alley_m1_npcs.json`：15 名关键 NPC，含功能身份、情感身份、行程和四维倾向。
- `buildings/jiangnan_rain_alley_buildings.json`：10 个交互建筑。
- `interactions/jiangnan_rain_alley_interactions.json`：对话、工艺、采集、选择、战斗触发、归位、旅行和调查交互。
- `enemies/rain_alley_m1_enemies.json`：9 类敌人与 6 类机关。
- `bosses/rain_alley_boss.json`：雨巷破伞执念 Boss、4 个阶段和 3 个归位结果。
- `assets/jiangnan_rain_alley_assets.json`：美术资源组和伞面变体。
- `audio/jiangnan_rain_alley_audio.json`：BGM、环境音、音效和混音规则。

M1 集合文件先作为策划/程序/美术/音频/QA 的稳定草案，不要求当前浏览器 demo 立即消费全部对象。新增对象应优先补进集合文件，并把可播放原型需要的最小字段同步到 `prototype/web-demo/`。

当前校验仍是最低结构烟测；跨文件引用、本地化缺失、数值范围和循环引用检查将在后续内容工具链中补齐。
