# ART-003 系统 Demo 灰盒运行时导入包

本目录冻结 12 个 `asset.system_demo.*` Presentation Blockout 槽的运行时导入合同。它把 ART-002 的 12 个独立 fallback SVG 中、带专属 `data-stable-asset-id` 的图形组确定性栅格化为 Standard/Low PNG，并由 manifest 以 `Stable Asset ID + Stable Content Key + SandboxAssetKind` 显式索引。

当前事实始终是：`Blockout`、`not-integrated`、`previewReady=false`、`releaseAllowed=false`、`internal-preview`、`review-recorded-not-release-cleared`。本包没有实现 Presentation resolver/renderer，没有写 Gameplay 碰撞、职责、技能、阵营或任务真相，也没有获得 Windows/Web 可见 Preview、三浏览器或真人可读性证据。

## 槽位与独立产物

| Stable Asset ID | SandboxAssetKind | Standard | Low |
| --- | --- | --- | --- |
| `asset.system_demo.player` | `player` | `standard/player.png` | `low/player.png` |
| `asset.system_demo.enemy.pressure` | `actor` | `standard/enemy-pressure.png` | `low/enemy-pressure.png` |
| `asset.system_demo.enemy.flanker` | `actor` | `standard/enemy-flanker.png` | `low/enemy-flanker.png` |
| `asset.system_demo.enemy.elite` | `actor` | `standard/enemy-elite.png` | `low/enemy-elite.png` |
| `asset.system_demo.obstacle.tension_gate` | `obstacle` | `standard/obstacle-tension-gate.png` | `low/obstacle-tension-gate.png` |
| `asset.system_demo.interaction.console` | `interaction` | `standard/interaction-console.png` | `low/interaction-console.png` |
| `asset.system_demo.mechanism.gate` | `mechanism` | `standard/mechanism-gate.png` | `low/mechanism-gate.png` |
| `asset.system_demo.safe_point.lamp_shelter` | `safe_point` | `standard/safe-point-lamp-shelter.png` | `low/safe-point-lamp-shelter.png` |
| `asset.system_demo.skill.eavesguard.telegraph` | `effect` | `standard/skill-eavesguard-telegraph.png` | `low/skill-eavesguard-telegraph.png` |
| `asset.system_demo.skill.eavesguard.hit` | `effect` | `standard/skill-eavesguard-hit.png` | `low/skill-eavesguard-hit.png` |
| `asset.system_demo.skill.flower_turn.telegraph` | `effect` | `standard/skill-flower-turn-telegraph.png` | `low/skill-flower-turn-telegraph.png` |
| `asset.system_demo.skill.flower_turn.hit` | `effect` | `standard/skill-flower-turn-hit.png` | `low/skill-flower-turn-hit.png` |

每槽的两条路径和 artifact identity 均独立；不得由相邻文件名猜测资源，也不得把一个通用图形复用成多个槽。Windows 与 Web 的计划映射可以指向同一份平台中立 PNG 字节，但仍必须通过该槽 manifest 中的显式 artifact ID 查找。

## 尺寸、锚点与格式

- Standard：`256×256`；Low：`128×128`。
- 均为透明 `PNG`、`RGBA8`、`sRGB`，上传时由消费者按合同预乘 alpha；linear filter、clamp、无 mipmap。
- `rootAnchorUv=(0.5, 0.125)`。UV 和 `rootAnchorPx` 均以左下角为原点，因此 Standard 为 `(128,32)`，Low 为 `(64,16)`；PNG 文件行本身仍按规范自上向下存储。
- 人物、场景对象使用 ART-002 的 `visualBoundsMm`；VFX 使用 `nominalPresentationExtentMm`。这些仅是视觉尺度，不是 Gameplay 命中范围。
- 毫米锚点来自 ART-002 台账，全部是 presentation-only。资源导入不得成为碰撞或交互 writer。
- 128 px 的 alpha 轮廓作为灰阶签名；玩家、pressure、flanker、elite 必须互异。每槽另有形状 token 和至少两个非颜色提示通道。

## 确定性重建

从仓库根运行以下命令；输出只进入被 Git 忽略的 `build/generated-assets/system-demo-blockouts/**`，PNG 不提交：

```powershell
node tools/run-toolchain.mjs node assets_src/system-sandbox-blockouts/tools/generate-system-sandbox-runtime-assets.mjs --verify-determinism
node tools/run-toolchain.mjs node assets_src/system-sandbox-blockouts/tools/generate-system-sandbox-runtime-assets.mjs --check
node tools/run-toolchain.mjs node assets_src/system-sandbox-blockouts/tools/validate-system-sandbox-runtime-assets.mjs --authoritative-root cb7d5ac347e6581d187db1bfb2ff05e8997b97fd
node tools/run-toolchain.mjs node --test assets_src/system-sandbox-blockouts/tests/system-sandbox-runtime-assets.test.mjs
```

生成器不新增 npm 依赖，并锁定：

- Node `20.18.0`
- ART-003 generator contract `1.0.0`；manifest 同时记录 LF 规范化后的 generator source SHA-256
- Playwright `1.61.1`
- pngjs `7.0.0`
- Chromium revision `1223` / version `148.0.7778.96`
- Chromium executable SHA-256 `290fa7018fda22c52ada5eddb0113baf3ebc41fd0fde6085eddb19793606c635`

默认从 `%LOCALAPPDATA%/ms-playwright/chromium-1223/chrome-win64/chrome.exe` 取锁定浏览器。CI 可用 `SYSTEM_SANDBOX_CHROMIUM_EXECUTABLE` 指向同一字节的 executable；SHA、revision 或 version 不符时必须阻断。manifest 只记录版本与 SHA，不记录机器绝对路径。`--verify-determinism` 连续生成两遍并逐字节比较 24 个 PNG；`--check` 同样双遍生成，再与 manifest 和忽略目录产物逐字节比较。

## 容量门

| 项 | 上限 | 当前生成证据 |
| --- | ---: | ---: |
| Standard transfer（12 个） | 512 KiB | 111,179 B |
| Low transfer（12 个） | 256 KiB | 53,361 B |
| Standard decoded | 3 MiB | 3,145,728 B |
| Low decoded | 0.75 MiB | 786,432 B |
| manifest | 64 KiB | 62,656 B |

单个 Standard/Low 还分别受 48 KiB/24 KiB 上限约束。validator 对表容量、字段形状、路径、artifact/source hash、PNG 尺寸与 alpha、唯一 identity、许可和预算全部 fail closed；验证失败不得暴露部分 resolver 表。

## 后续接线顺序与 Remaining Open

1. 本包先通过 generator、runtime validator 和旧 ART-002 文件集 validator；ART/技术美术 Owner，DEV/Presentation、CONTENT、GAME、QA、许可/PLATFORM 必审。
2. CONTENT-002 authored definitions 只提供外部内容/状态引用，不从资源文件名推导 Gameplay；本包也不复制 F1 内容 ID。
3. DEV/Presentation 后续实现按 `Stable Asset ID + Stable Content Key + SandboxAssetKind` 的 resolver/import；unknown、key mismatch、wrong kind、缺 artifact 一律关闭，Standard 只能显式回落到同一 entry 的 Low。
4. resolver 稳定后，Presentation renderer 才可做 Native/Web texture upload 与锚点 smoke；TOOLS Preview 再消费同一显式表。
5. 待执行的视觉证据为 Windows/Web × `1280×720`、`1920×1080`、`1230×692` × Standard/Low 的截图矩阵，以及 128 px 灰阶角色辨识复核。当前均为 `not-produced/not-validated`，不能据此声明 Preview-ready。
6. 许可未升级为 release-cleared 前，所有产物只允许 internal preview；任何发布或最终美术替换必须重新走来源、许可、预算和平台验收。
