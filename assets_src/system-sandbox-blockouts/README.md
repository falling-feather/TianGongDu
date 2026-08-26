# ART-002 系统型 Demo 可辨识灰盒资产包

本目录是 `ART-002` 的自包含 Presentation Blockout 包。它冻结 12 个系统型 Demo 视觉槽、8 个新增确定性 SVG 源板、12 个 Stable-ID 专属 fallback、六组逐槽元数据、三种验收画幅与 128px 灰阶角色缩略证据。

成熟度固定为 `Blockout / not-integrated`。这里没有 Runtime resolver、Windows/Web 构建产物、Gameplay 绑定、玩家实测或 Preview-ready 证据；预览文件仅是静态评审板。许可证状态固定为 `review-recorded-not-release-cleared`，不得作为正式发布许可。

## 权威边界与自包含性

- Git authoring base：`99a4093c51b99bc8067e10364f591011efe328ab`。
- Sandbox 合同权威根：`27fb87c8af74b48e0bfb8f4ef6da2f8e96d6560e`。
- `f95454c9fa51bdaf593dc354ac21a6ed770ee31c`、tree `e02ff077c0d1d57923064a2273fc9a049ec475eb` 与 `99a4093…` 仅记录项目原创视觉语法和台账来源；本包不读取这些提交中的外部文件。
- 本目录单独 cherry-pick 到权威根后仍包含全部 fallback、source、元数据、预览、生成器、校验器和测试。
- Stable Asset ID 只标识 Presentation 槽，不推导 faction、duty、AI、Ability、Stance、Objective、Wave、碰撞或任务推进真相；所有空间与 Gameplay 权威都由包外合同提供。
- obstacle 的资产 kind 是 `obstacle`；`ground_blocker` 只是 Gameplay 放置域名称，不能成为 `SandboxAssetKind`。

## 12 个冻结槽

| Stable Asset ID | SandboxAssetKind | Source 策略 | 独立 fallback | 主要非颜色辨识 |
|---|---|---|---|---|
| `asset.system_demo.player` | `player` | 项目原创语法重绘 | `fallbacks/player.svg` | 窄直立梯形、灯钉、长对角折伞 |
| `asset.system_demo.enemy.pressure` | `actor` | 项目原创语法重绘 | `fallbacks/enemy-pressure.svg` | 低矮破三角、放射伞骨脚、内压轮廓 |
| `asset.system_demo.enemy.flanker` | `actor` | 新 source SVG | `fallbacks/enemy-flanker.svg` | 高位宽折线、单侧长翼、收紧脚影 |
| `asset.system_demo.enemy.elite` | `actor` | 新 source SVG | `fallbacks/enemy-elite.svg` | 分裂伞冠、配重立柱、三枚坠重 |
| `asset.system_demo.obstacle.tension_gate` | `obstacle` | 新 source SVG | `fallbacks/obstacle-tension-gate.svg` | 实心门槛、交叉绷架、无操作脉冲 |
| `asset.system_demo.interaction.console` | `interaction` | 项目原创语法重绘 | `fallbacks/interaction-console.svg` | 切角台体、凹入手槽、位移短杆 |
| `asset.system_demo.mechanism.gate` | `mechanism` | 项目原创语法重绘 | `fallbacks/mechanism-gate.svg` | 偏置门柱、外露连杆、升降配重 |
| `asset.system_demo.safe_point.lamp_shelter` | `safe_point` | 新 source SVG | `fallbacks/safe-point-lamp-shelter.svg` | 闭合双环、三肋灯棚、上举灯芯 |
| `asset.system_demo.skill.eavesguard.telegraph` | `effect` | 新 source SVG | `fallbacks/skill-eavesguard-telegraph.svg` | 实心撑伞楔、三根内收肋、边缘压缩 |
| `asset.system_demo.skill.eavesguard.hit` | `effect` | 新 source SVG | `fallbacks/skill-eavesguard-hit.svg` | 叠压横条、前向回弹、矩形缺口 |
| `asset.system_demo.skill.flower_turn.telegraph` | `effect` | 新 source SVG | `fallbacks/skill-flower-turn-telegraph.svg` | 断续双弧、斜穿缺口、横扫运动 |
| `asset.system_demo.skill.flower_turn.hit` | `effect` | 新 source SVG | `fallbacks/skill-flower-turn-hit.svg` | 交叉剪切、反向侧移、中心空菱 |

前四个角色在 `previews/actor-thumbnails-128.svg` 中以同一 128px 高灰阶条并列；辨识依赖轮廓、结构、负空间和节奏提示，而不是颜色。

## 坐标、锚点与尺寸

统一单位为毫米：`+x` 向右、`+y` 向视觉前方、`+height` 向上。每槽元数据记录 canonical visual root、脚点或基点、选择/操作/效果锚点以及视觉包围尺寸。锚点只供 Presentation 对齐，绝不写入 Gameplay 碰撞、命中范围或任务状态。

非 effect 槽记录 `visualBoundsMm`；四个 effect 只记录 `nominalPresentationExtentMm`。后者是评审构图的名义展示范围，不是伤害或命中范围，实际几何始终依赖外部 Gameplay 定义。

## 元数据、许可与预算

每个 `asset-metadata.json` 都包含：

- `source`：生成方法、来源、派生关系、AI 辅助披露、文件校验值；
- `import`：planned/not-produced/not-validated、轴、单位、root、anchors 与替换保留规则；
- `license`：项目权利方、外部许可空集、内部用途和 release gate；
- `runtime`：Presentation-only 边界、fallback、Web/Windows 同 ID 且均 not-built/not-integrated；
- `preview`：静态评审板路径与未验证声明；
- `budget`：源/占位字节、传输、GPU 纹理、draw call、透明覆盖与层数目标。

SVG 由项目内确定性脚本生成，没有外部图像输入，也未调用图像生成模型；`aiGenerationDisclosure` 仍记录 Codex 辅助。单项生成 SVG 上限 32 KiB，整个提交上限 1,500,000 bytes；禁止 DCC、GLB、纹理、音频和外链资源。Web 计划上限为每槽 1024 KiB 传输、8 MiB GPU 纹理、3 draw calls、10% 峰值透明覆盖，VFX 最多 2 层。

## 生成与验证

在本目录执行：

```powershell
node --check tools/system-sandbox-blockout-spec.mjs
node --check tools/generate-system-sandbox-blockouts.mjs
node --check tools/validate-system-sandbox-blockouts.mjs
node --test tests/system-sandbox-blockout-contract.test.mjs
node tools/generate-system-sandbox-blockouts.mjs
node tools/generate-system-sandbox-blockouts.mjs --check
node tools/validate-system-sandbox-blockouts.mjs --authoritative-root 27fb87c8af74b48e0bfb8f4ef6da2f8e96d6560e
```

生成器只写本目录。`--check` 逐字节比较所有派生产物；校验器进一步核对权威枚举、精确 ID/kind、独立 fallback、六组元数据、F1 内容 ID 禁入、禁止 Gameplay 字段、许可门、预算门、SVG/XML 静态规则及完整文件集合。

三画幅静态评审板为：

- `previews/readability-1280x720.svg`
- `previews/readability-1920x1080.svg`
- `previews/readability-1230x692.svg`

它们不是 Web/Windows Runtime 截图，也不代表 Preview-ready 或真人可读性门通过。
