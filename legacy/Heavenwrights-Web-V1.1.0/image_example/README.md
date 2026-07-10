# image_example

> 用途：保存《天工渡 / Heavenwrights》江南区域新版设计的风景与人物参考图、生成 prompt 和后续美术拆图清单。
> 创建日期：2026-07-05
> 关联文档：`doc/13_江南区域新版大地图设计方案.md`

## 文件清单

| 文件 | 用途 | 关联内容 ID | 资源组 |
| --- | --- | --- | --- |
| `jiangnan_scenery_reference.png` | 江南风景四宫格：烟雨竹巷、河埠夜市、青篁湿林、断桥雨幕。 | `large_area.jiangnan.misty_bamboo_alley`、`large_area.jiangnan.river_market`、`large_area.jiangnan.wet_bamboo_forest`、`large_area.jiangnan.wind_bell_old_bridge` | `asset_group.jiangnan.subregion_concepts`、`asset_group.jiangnan.environment_tilesets` |
| `jiangnan_character_reference.png` | 江南人物四宫格：纸伞工艺传承者、夜市跑单人、守桥老人、灯娘。 | `npc.master_shen_yu`、`npc.market_runner`、`npc.mei_bridge_keeper`、`npc.xu_lan_lantern` | `asset_group.jiangnan.npc_portraits` |
| `jiangnan_scenery_reference.prompt.md` | 风景参考图生成 prompt 与拆图说明。 | `large_area_set.jiangnan_chapter_v1` | `asset_group.jiangnan.subregion_concepts` |
| `jiangnan_character_reference.prompt.md` | 人物参考图生成 prompt 与拆图说明。 | `npc_set.jiangnan_rain_alley_m1` | `asset_group.jiangnan.npc_portraits` |

## 美术使用建议

- 风景参考用于拆分环境 tileset、天气层、远景层、建筑体块和地标。
- 人物参考用于确定服装材质、剪影、年龄层和职业道具，不作为最终立绘。
- 所有参考图仅作风格辅助，正式资产必须遵循项目像素风、类 2.5D 判定清晰度和文化表达复核。
- prompt 和参考图不能直接作为运行时资源。正式资源必须进入后续 manifest，并补充文件路径、预览图、帧规格、锚点、碰撞辅助体、压缩格式、授权来源和文化复核状态。
- 当前生产状态为 `draft`。当任一参考图被拆成正式概念或 tileset/sprite 时，需要同步更新 `content/assets/jiangnan_rain_alley_assets.json` 的 `referenceIds` 或后续 `AssetRefDef`。
