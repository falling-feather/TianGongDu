# 《天工渡 / Heavenwrights》发布历史

> 文档定位：记录已完成事实、验证证据和剩余风险。  
> 适用范围：项目原型、内容数据、文档体系和后续版本记录。  
> 最后更新：2026-07-10
> 维护者：项目维护者  
> 关联文档：`README.md`、`doc/02_项目规划与验收.md`、`doc/12_测试与验收指南.md`

## V1.1.0 - 江南雨巷阶段可玩归档（2026-07-10）

### 已完成

- 将旧版 Canvas 白盒整理为有明确起点和终点的阶段路线：入镇验货、架伞学习、竹林采集、伞面工艺、夜市选择、风铃铆、旧桥战斗、断桥 Boss 归位与《天工录》落款。
- 新增五段阶段清单和动态目标；内容包加载不再用“大地图/编辑器介绍”覆盖当前玩法目标。
- 生机可以真实归零；新增“伞骨失稳”失败状态、阶段据点整备和进度保留。
- 新增阶段完成覆盖层，Boss 归位并完成落款后给出明确的成果节点与自由探索入口。
- 将章节内容库存与当前可玩切片显式分离；湖田外圩等草案步骤继续保留，但不再阻塞本切片完成。
- 背包、人物、大地图、编辑器、江南 M1 和记录面板打开时暂停战斗；普通入口默认隐藏开发捷径，`?debug=1` 可启用。
- 修正染纸晒场探索入口：学会架伞后开放竹林，采得青篾后开放染纸晒场，不再依赖隔空背包制作或开发按钮。
- 将 HUD 高频更新与抽屉面板结构更新拆分签名，避免地图按钮随时辰刷新被反复销毁，改善键盘焦点与自动化稳定性。

### 验证

- `npm test`：52 个测试通过。
- `npm run validate:content`：22 个内容 JSON 文件通过校验。
- `node --check prototype/web-demo/src/main-v2.js`：语法检查通过。
- 浏览器人工验收通过：首屏、阶段路线、失败覆盖层、据点重试、深层菜单暂停、开发流程到 Boss、Boss 归位、最终落款与完成覆盖层；控制台无错误或警告。

### 仍需后续

- 本版本仍是无正式美术/音频资产的 Canvas 白盒，不等同于端游 Runtime。
- “推进 M1 流程”只用于 `?debug=1` 白盒验收，正式游玩必须走自然触发。
- 旧版技术路线已冻结；长期架构、内容 Schema 和生产编辑器在新版主线重建。

## 未发布 - 江南新版四维大地图设计方案（2026-07-05）

### 已完成

- 新增 `doc/13_江南区域新版大地图设计方案.md`，在不直接覆盖 07 号文档的前提下，将江南从纵切型内容扩展为不少于 5 小时可玩性的正式章节方案。
- 按百工、战斗、剧情、探索四维重构江南地区，设计 `烟雨竹巷`、`河埠水市`、`青篁湿林`、`染纸晒场`、`风铃旧桥`、`湖田外圩` 6 个大型子区域，并为每区补充 NPC、建筑、采集点、敌人、小 Boss、支线与长期回访循环。
- 新增 `image_example/`，保存江南风景与人物参考图、生成 prompt 和美术拆图说明。
- 更新根 README 与 `doc/README.md`，补入 13 号文档入口，保持 07 号文档作为当前第一版投放规格不变。

### 验证

- `npm test` 通过：3 个测试套件、32 个测试用例全部通过。
- `npm run validate:content` 通过：17 个内容 JSON 文件通过结构校验。
- 文档搜索检查未发现临时占位标记或过时的“PNG 未返回”说明。

### 仍需后续

- 若确认采用本方案，再按 13 号文档建议将 `07_江南雨巷纵切规格.md` 改写为 6 大子区域版本，并把新版区域点拆入正式 `content/**/*.json`。
- 后续正式美术阶段需要将 `image_example/` 的参考图拆分为像素风 tileset、NPC 立绘、地标和天气层资产。

## 未发布 - 江南 M1 染纸晒场工艺白盒（2026-07-04）

### 已完成

- 更新 `prototype/web-demo/` 的背包与场景逻辑，新增染纸晒场工艺白盒：`伞面吃风`、`蓝染稳色`、`急单省工` 三种配方会消耗材料、写入 `world.craft`、更新工艺四项指标、四维反馈、NPC 评价和资源事件 ID。
- 将工艺结果接入真实玩法参数：架伞回风、收风伤害、借风风息消耗和雨幕湿滞承受会读取 `world.craft.bonuses`；蓝染稳色会把玩家伞面和晒场预览切到蓝色变体。
- 新增染纸晒场 Canvas 白盒区域、工艺热点和 `F` 上下文入口；玩家从 M1 推进到竹林选材后，可在地图进入染纸晒场，靠近后按 `F` 聚焦工艺面板。
- 更新 `content/assets/jiangnan_rain_alley_assets.json`，将 `tile.dye_paper_court` 纳入环境 tileset 需求，并让生产面板白盒接入计数达到 `7/7`。
- 更新 `doc/01_开发者手册.md` 和 `doc/12_测试与验收指南.md`，沉淀 `CraftRecipeDef`、`world.craft` 消费规则、材料 adapter、资源 manifest 边界和浏览器验收路径。
- 更新 `tools/dev-server.mjs`，对 `/favicon.ico` 返回 204，避免浏览器默认资源请求污染 QA console。
- 更新 `tests/web-demo.test.mjs`，覆盖工艺面板、空间交互热点、染纸晒场绘制、蓝染变体和染纸 tileset 资源 ID。

### 验证

- `npm run validate:content` 通过：17 个内容 JSON 文件通过结构校验。
- `npm test` 通过：3 个测试套件、32 个测试用例全部通过。
- 本地 Web 服务 `http://127.0.0.1:4175/` 已启动并使用系统 Edge + Playwright 完成最终浏览器 QA：桌面 1280x720 与移动 390x844 均走通“开始巡巷 -> 推进 M1 至竹林选材 -> 地图进入染纸晒场 -> 按 F -> 选择蓝染稳色”。
- 浏览器 QA 断言通过：`trait.umbrella.stable_blue_dye` 生效，桐油和雨纹纸消耗为 0，`quest_step.jiangnan_rain_alley.paper_and_oil` 完成，风铃巷解锁，资源事件更新到 `asset.variant.umbrella.blue_lantern / trait.umbrella.stable_blue_dye / interaction.jiangnan_rain_alley.blue_pattern`，环境 tileset 显示 `7/7`，console error 为空。
- 截图证据：桌面截图保存到 `C:\Users\niu-h\AppData\Local\Temp\heavenwrights-m1-craft-desktop.png`，移动截图保存到 `C:\Users\niu-h\AppData\Local\Temp\heavenwrights-m1-craft-mobile.png`；移动端 `craftHotbarOverlap=false`、`productionHotbarOverlap=false`。

### 仍需后续

- 将当前 `CRAFT_RECIPES` 提升为正式内容数据或生成型 `CraftRecipeDef`，并接入统一物品 ID adapter。
- 正式资源阶段需要为 `tile.dye_paper_court`、`building.jiangnan_rain_alley.dye_court`、`asset.variant.umbrella.blue_lantern` 补贴图、帧规格、锚点、碰撞辅助体和 manifest 路径。
- 若后续用 Three.js 或 Axmol 提高染纸晒场、雨幕和伞面的表现精度，必须继续通过稳定内容 ID 与资源服务读取表现层，不改变任务、工艺和战斗判定。

## 未发布 - 江南 M1 资源生产面板与事件接口（2026-07-04）

### 已完成

- 更新 `prototype/web-demo/` 的“江南M1”面板，新增资源生产面板，运行时展示 M1 美术资源组、音频项、需求数量、白盒接入状态、Boss 阶段 `requiredState` 和当前资源事件 ID。
- 新增轻量资源事件层：架伞、纸刃、伞旋、收风、借风、风铃机关、Boss 显形、Boss 阶段伤害和 Boss 归位会更新对应 VFX/SFX/BGM/资产组 ID，先验证触发契约，不播放不存在的正式资源。
- 将断桥 Boss Canvas placeholder 与 `asset_group.jiangnan.boss_sprites` 的 `phase_1`、`phase_2`、`phase_3`、`restoration` 状态对齐，为后续 Three.js、Axmol 或资源 manifest 替换保留边界。
- 更新 `doc/01_开发者手册.md` 和 `doc/12_测试与验收指南.md`，明确 `AssetPackDef` / `AudioPackDef` 字段契约、真实资源 manifest 边界、资源事件层职责和浏览器验收路径。
- 更新 `tests/web-demo.test.mjs`，覆盖资源生产面板容器、渲染函数、资源事件函数、Boss 资源状态函数和样式类。

### 验证

- `npm test` 通过：3 个测试套件、31 个测试用例全部通过。
- `npm run validate:content` 通过：17 个内容 JSON 文件通过结构校验。
- 本地 Web 服务 `http://127.0.0.1:4174/` 已启动并完成浏览器 QA：内容包状态显示“已载入 11 个子地区、15 名 NPC、9 类敌人”，资源生产面板生成 28 行生产数据，Boss 从显形、P1/P3 到归位时资源事件分别更新到 `bgm.jiangnan_rain_alley.boss`、`bossphase.*`、`sfx.boss.core_restore` 和 `mix.jiangnan_rain_alley.boss_to_restoration`，console 无错误。
- 移动视口 390x844 下复验：生产面板、底部热键和操作提示不重叠，长资源 ID 不再溢出；截图保存到 `C:\Users\niu-h\AppData\Local\Temp\heavenwrights-m1-production-mobile.png`，桌面截图保存到 `C:\Users\niu-h\AppData\Local\Temp\heavenwrights-m1-production-desktop.png`。

### 仍需后续

- 将资源生产面板接入真实资源 manifest，补正式文件路径、缩略图、帧规格、锚点、碰撞辅助体、压缩格式和音频混音参数。
- 后续如正式接入 Three.js 或 Axmol 高精度渲染，需要让表现层通过资源服务读取同一组 ID，不改变任务、Boss 和技能规则。

## 未发布 - 江南 M1 Web 原型接入（2026-07-04）

### 已完成

- 扩展 `tools/dev-server.mjs`，为 Web demo 增加 `/content/...` 只读路由，使浏览器原型能实际读取根目录内容数据。
- 更新 `prototype/web-demo/`，新增“江南M1”面板，运行时加载 M1 内容包并展示 11 个子地区、15 名 NPC、9 类敌人、主线步骤、Boss 状态和内容生产摘要。
- 将 M1 主线推进接入可交互流程：推进按钮会解锁子地区、发放风铃铆、打开断桥雨幕并启动 Boss 验收。
- 新增可玩的“雨巷破伞执念”Boss 闭环：纸刃、伞旋、收风和借风可削减 Boss 失序，归零后按 `F` 触发归位并进入天工录落款步骤。
- 强化断桥雨幕 Canvas 2.5D 绘制，加入多层雨帘、裂伞核、伞骨弧线和 Boss 阶段视觉反馈；本轮未引入 Three.js 网络依赖，保留后续高精度渲染替换边界。
- 更新 `tests/web-demo.test.mjs`，覆盖 M1 面板、内容包加载函数、Boss 函数和 `/content` 路由。

### 验证

- `npm test` 通过：3 个测试套件、30 个测试用例全部通过。
- `npm run validate:content` 通过：17 个内容 JSON 文件通过结构校验。
- 本地 Web 服务 `http://127.0.0.1:4174/` 已启动并完成浏览器 QA：页面加载非空、内容包状态显示正常、console 无错误、M1 主线推进到 Boss、技能将 Boss 失序从 100% 降到 80%、继续战斗后 Boss 成功归位，移动视口 390x844 下 M1 面板和控制区可读。

### 仍需后续

- 将当前 M1 推进按钮逐步替换为关卡内自然任务触发、对白和场景门槛。
- 将 Boss 数值、碰撞、阶段机制和归位结果继续从原型函数外提到可校验数据规则。
- 若正式引入 Three.js 或 Axmol 高精度渲染，需要新增 ADR，明确依赖来源、许可、资源管线、性能预算和与 Runtime 判定层的边界。

## 未发布 - 江南 M1 内容对象落地（2026-07-01）

### 已完成

- 将 `doc/07_江南雨巷纵切规格.md` 中的 M1 内容规划拆入 `content/` 数据层，新增子地区、任务、NPC、建筑、交互、敌人、Boss、资产和音频集合。
- 扩展 `content/regions/jiangnan_rain_alley.json`，使 RegionDef 成为江南内容包入口，索引 11 个子地区、7 个核心节点、主线/支线、15 名 NPC、敌人、Boss、建筑交互、资产包和音频包。
- 新增 `tests/content-m1.test.mjs`，覆盖 RegionDef 索引、M1 核心内容数量、本地化 key、Boss 阶段、归位结果和美术/音频生产接口。
- 更新 `doc/01_开发者手册.md`、`content/README.md`、`doc/02_项目规划与验收.md`、`doc/12_测试与验收指南.md`、根 README 和文档索引，沉淀内容接口、集合边界、四维选择反馈和测试口径。

### 验证

- `npm test` 通过：3 个测试套件、29 个测试用例全部通过。
- `npm run validate:content` 通过：17 个内容 JSON 文件通过结构校验。

### 仍需后续

- 将当前浏览器 demo 从局部硬编码逐步改为消费 M1 内容数据包。
- 将 Node 测试中的本地化 key 检查并入 Python 内容校验器，并继续补跨文件 ID、数值范围、子地区连接图和 BossPhase 转换条件校验。
- 按 `08_资料索引与调研计划.md` 对纸伞工序、江南水镇、灯市、行会账房和旧桥表达进行正式资料核验。

## 未发布 - 江南第一版投放设计书（2026-07-01）

### 已完成

- 将 `doc/07_江南雨巷纵切规格.md` 扩展为江南第一版投放设计书，保留原 M0 纵切骨架，并新增 M1 投放目标、主线剧情、11 个子地区、15 名关键 NPC、支线任务、工艺门径、敌人/Boss、建筑交互、数据结构、资产需求和团队分工。
- 回写 `doc/03_项目全局设计规划.md`，把江南第一版投放的 11 个子地区和新增内容对象类型纳入全局设计基线。
- 更新 `doc/04_技术栈与生产管线.md`、`doc/09_AI协作内容管线.md` 与 `content/README.md`，补充 `SubregionDef`、`QuestDef`、`BuildingDef`、`InteractionDef`、`BossPhaseDef`、`AssetDef`、`BgmDef`、`CinematicDef` 等内容生产对象。
- 同步根 README、文档地图和项目规划，使新协作者能从入口文档找到江南第一版投放设计。

### 验证

- `npm test` 通过：2 个测试套件、12 个测试用例全部通过。
- `npm run validate:content` 通过：8 个内容 JSON 文件通过结构校验。
- 文档搜索检查未发现未完成状态占位、临时任务标记或旧版 07 号定位残留。

### 仍需后续

- 将 `07_江南雨巷纵切规格.md` 中的 M1 对象拆成实际 `content/**/*.json` 草案。
- 为江南纸伞、江南水镇、灯市、行会账房和旧桥空间补正式资料来源与文化表达风险审查。
- 增加跨文件 ID 引用、本地化 key、数值范围、子地区连接图和 BossPhase 转换条件校验。

## 未发布 - 全局设计规划与文档顺延（2026-06-30）

### 已完成

- 新增 `doc/03_项目全局设计规划.md`，汇总基础设定、主线结构、12 个地区包、核心交互、门径体系、基础数值参考、内容管线、工程边界和验收标准。
- 将原 `03` 到 `11` 号文档顺延为 `04` 到 `12`，为全局设计规划腾出 `03` 号位。
- 同步 README、文档索引、ADR、分区 README 和专项文档中的编号引用。

### 验证

- `npm test` 通过：2 个测试套件、12 个测试用例全部通过。
- `npm run validate:content` 通过：8 个内容 JSON 文件通过结构校验。
- Codex Security 标准扫描完成：基于 ASCII 临时快照审查文档与工具链边界，0 个可报告安全发现，报告位于 `C:\Users\niu-h\AppData\Local\Temp\codex-security-scans-6Zfojg\heavenwrights-security-snapshot\unversioned_20260630T101306Z_vvsa2o62\report.md`。

### 仍需后续

- 按实机验证调整 `03_项目全局设计规划.md` 中标注为初始参考的数值区间。
- 对 12 个地区包逐个补资料来源、文化表达风险和内容 Schema。
- 继续补 Axmol WebAssembly 技术冒烟清单。

## 未发布 - 文档体系整理（2026-06-28）

### 已完成

- 梳理 README、`doc/README.md`、规划、开发者手册、测试指南、发布历史和 ADR 入口的职责边界。
- 将“未来计划、长期工程规则、测试验收、已完成事实”拆到不同生命周期文档中。
- 按阅读优先级重排文档编号：`01_开发者手册.md` 为最重要入口，`02_项目规划与验收.md` 次之。
- 对齐 `npm run dev` 的默认项目入口地址为 `http://127.0.0.1:4174/`。

### 验证

- 文档入口可从根 README 导航到规划、开发者手册、测试指南和发布历史。
- `npm test` 通过：2 个测试套件、11 个测试用例全部通过。
- `npm run validate:content` 通过：7 个内容 JSON 文件通过结构校验。

### 仍需后续

- 补充更完整的内容 Schema 文档。
- 为 Axmol WebAssembly 技术冒烟补具体测试清单。
- 按下一阶段实现进度补充发布记录。
- 浏览器人工冒烟仍需在发布前按 `doc/12_测试与验收指南.md` 执行。

## v0.0.1 - M0 浏览器原型与内容骨架（2026-06-23）

### 已完成

- 建立《天工渡 / Heavenwrights》根 README、前期设计文档和江南雨巷纵切方向。
- 创建无外部依赖的 `prototype/web-demo/`，验证江南雨巷首屏、纸伞门径 HUD、简化战斗、风息状态、夜市选择和归位反馈。
- 建立 `content/` 文本化内容样例，覆盖地区、门径、技能、NPC、敌人、世界事件和本地化。
- 建立 `tools/python/content_validate.py` 内容校验脚本。
- 建立 Node 测试，覆盖 Web demo 静态入口和内容 JSON 必填字段。

### 验证

- 可运行 `npm test` 进行 Node 静态烟测。
- 可运行 `npm run validate:content` 进行内容结构校验。
- 可运行 `npm run dev` 打开本地浏览器 demo。

### 仍需后续

- 当前 demo 只验证首屏和基础交互，不是完整 15-25 分钟纵切。
- 当前内容校验只覆盖基本字段，尚未覆盖跨文件引用、本地化 key、数值范围和可达性。
- Axmol/C++/WebAssembly 技术链路仍待单独冒烟。
