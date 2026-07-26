# DEMO-083｜Demo 0.8.3 两波目标战斗场验收

- **任务**：`DEMO-083`
- **产品版本**：`Demo 0.8.3`
- **Owner / 集成负责人**：新主开发
- **实现提交**：`484e7e5e87a3814ea57f545e46bb1878ba75a024`
- **证据提交绑定**：[`evidence.json`](evidence.json) 的 `commit` 精确等于上述实现提交
- **浏览器**：Edge `150.0.4078.99`

## 1. 包与生成物身份

| 项目 | 验收值 |
| --- | --- |
| `.tgdsbx` 格式 / 作者 Schema / Service ABI | `1.2` / `1.2.0` / `0x00010002` |
| canonical 作者源 | 10,439 bytes；SHA-256 `07fa5537fc617e0686605a4b7eaeb20ceea24ab7696f5db63e42b23e68e0bb69` |
| canonical package | 2,960 bytes；SHA-256 `a09ce35e58a48c01b15d4946de8e380d752760142ddbefee7bce7f7476b0252e` |
| provider checksum | `0cfd8c17732278074deb08dccf5f1eb5d4abf1f59ffbaa1de610eeac7d87e2e9` |
| Workbench generated module / WASM | `456ac7f83d1864928540e4d2a811eed0ac7c058ee99e5f39c7a8ea42174fa2a2` / `b8c47d9da59a7ca9ab91ad4b3bf643b7af7d22deb7bedfe56bc1d19528bad145` |

## 2. 玩家路线验收

| 验收项 | 实测结果 | 结论 |
| --- | --- | --- |
| 关闭机关碰撞 | 玩家在 `Y=350` 被门阻挡，`blockedMoveCount=28` | 通过 |
| 开门与首波 | `F` 操作后门开启，第一目标完成，2 名敌人激活 | 通过 |
| 重复触发幂等 | 再次触发后仍为 2 名敌人，`repeatedTriggerCount=1` | 通过 |
| 真实敌击倒与死亡 | 玩家由敌人自然攻击降至 `HP=0`，不是测试直接改状态 | 通过 |
| 死亡后重试 | `R` 恢复至 `(-1250,-3000)`、`HP=160`、关门且波次/目标/敌人计数归零 | 通过 |
| 两波 terminal | 轻击/重击完成两波，最终 `waves=2`、`objectives=2`、`defeated=4`、`acceptedAttacks=59` | 通过 |
| terminal 后重试 | 再次 `R` 后全部局部遭遇状态恢复初始值，`retryCount=2` | 通过 |
| 浏览器错误 | console/page/request error 均为 0 | 通过 |

完整结构化数据见 [`evidence.json`](evidence.json)。

## 3. 自动验证

- MSVC Debug 全量构建通过，CTest `30/30`。
- Web Release Single 真实 `.html/.js/.wasm` 构建通过。
- Web Asset Resolver probe 通过；Web Runtime Coordinator probe 通过，trace 为 `4c373193845dda96`。
- 根 `npm test` 通过：根 Node `73/73`；Workbench `59 pass + 2 expected environment skips`；系统 Demo 证据合同通过。
- 在真实 generated module/WASM 环境关闭条件 skip 后，Workbench `61/61`。
- Workbench 真实 Edge 浏览器套件 `3/3`，console/page/request/unexpected HTTP error 均为 0。
- Workbench 的 system-demo Launch/Safe Reload 路线与独立 System Demo Host 路线均使用真实 Edge 和真实生成模块完成。
- `npm run lint:architecture`、`npm run validate:design` 与 `git diff --check` 通过。

## 4. 画面证据

- [初始关闭门与零激活敌人](01-initial-closed-gate.png)
- [首波激活且重复触发不重复刷怪](02-wave-one-active-repeat-safe.png)
- [敌人自然击倒玩家](03-player-defeated.png)
- [死亡后局部路线恢复](04-death-retry-restored.png)
- [两波、两目标和 terminal 完成](05-two-waves-terminal-complete.png)

## 5. 放行边界

- 本证据放行的是目标驱动的两波技术闭环，不把自动快跑冒充真人 `8–12` 分钟节奏证明；战斗节奏、职责可读性和真人时长在 `DEMO-086/089` 调校并复验。
- 本版只有最小轻击/重击和两种普通敌人 profile，不声明两种架势、4–6 个数据化技能、精英、完整受击/防守/闪避手感已经完成。
- Workbench 的 Wave/Objective 是 **update-only** 面板；新增、复制和删除仍未开放为完整 CRUD。
- Windows 可见窗口、三浏览器矩阵、工艺/工坊经营、三名江南 NPC、聚合离开返回与最终表现均未放行。
- 外部 Edge/Chrome 扩展后端在本机未安装或未启用；验收使用仓库真实 Edge Playwright Host，不把该环境缺口写成浏览器连接成功。
