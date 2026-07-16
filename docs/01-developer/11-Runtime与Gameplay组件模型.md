# 11｜Runtime 与 Gameplay 组件模型

> 上级文档：[`../01-开发者文档.md`](../01-开发者文档.md)
> 状态：Accepted Baseline
> 最后更新：2026-07-15
> 维护者角色：C++ 核心负责人；Gameplay、AI、存档与表现 Owner 会签

## 1. 混合组件模型

采用“稳定身份 + 数据组件 + 固定系统阶段 + 少量领域状态机”的 C++ 混合模型。它能组合大量 NPC/Boss/器物，又不为理论上的百万实体引入完整 Archetype ECS 复杂度。

高频战斗数据可使用 SoA/稠密池；任务、Boss 阶段、关系和同步使用可读状态机。Axmol Node/Sprite 是可重建视图，不是实体本身。

## 2. 身份与句柄

每个运行时实体拥有：

- `EntityHandle { index, generation }`：Session 内安全句柄，防止悬空引用。
- `ContentId`：稳定内容原型 ID，例如 `npc_shen_yan`。
- `InstanceId`：需要跨存档/设备存在的实例 ID。
- `PackageId` 与版本来源。

组件池只接受 `EntityHandle`；跨 Tick 不保存裸引用。长期存档/同步只保存 `ContentId/InstanceId` 和状态，重建后可获得新 Handle。

## 3. 组件存储

`EntityRegistry` 管理 generation、签名和组件池。基础接口：

```cpp
template<class T> ComponentPool<T>& pool();
EntityHandle create(EntityPrototypeId prototype);
void destroy_deferred(EntityHandle entity);
bool alive(EntityHandle entity) const;
```

结构变更写入 `CommandBuffer`，在 Cleanup 阶段统一提交，避免遍历时失效。组件优先平凡移动、明确序列化；包含平台句柄、Node 指针、回调闭包的类型禁止进入权威组件。

### 3.1 基础组件

`TransformState`, `LifecycleState`, `TagSet`, `Ownership`, `PersistencePolicy`, `PresentationKey`。

Transform 使用项目定义的标量/向量与单位。Presentation 负责映射 Axmol 坐标和插值。

### 3.2 战斗组件

`Vitality`, `Stamina`, `Poise`, `LanternCharge`, `Combatant`, `WeaponLoadout`, `AbilitySet`, `EffectSet`, `HitShapeState`, `TargetingState`, `ThreatState`, `Faction`。

生命、硬直、失序和证据是不同管线，不能复用一个 `health` 数字。

### 3.3 NPC/世界组件

`Schedule`, `DialogueRole`, `RelationshipParticipant`, `QuestParticipant`, `Profession`, `TradeProvider`, `CompanionCapability`, `TravelCapability`, `WorldStateBinding`。

未使用的能力不挂空组件。核心 NPC 通过模块组合获得功能，不派生 `CombatMerchantQuestNpc`。

### 3.4 AI 组件

`Perception`, `Blackboard`, `Intent`, `NavigationAgentState`, `CombatArchetype`, `UtilityProfile`, `GroupRole`, `LeashPolicy`。

感知事实、决策意图和表现动作分离。场景动画延迟不能改写已提交意图。

## 4. 固定 Tick

每个 1/60 秒 Tick 顺序固定：

```text
1. CollectCommands
2. ValidateCommands
3. UpdateClocksAndSchedules
4. SenseWorld
5. DecideIntent
6. ResolveMovement
7. ResolveAbilitiesAndInteractions
8. ResolveHitsEffectsAndPoise
9. ResolveLanternEvidenceAndSeals
10. AdvanceQuestsRelationshipsAndWorldState
11. CleanupAndSpawn
12. EmitEventsSnapshotAndSyncOps
```

系统注册表以编译期/启动期稳定顺序构建。任何系统不能依赖哈希容器未定义迭代顺序。需要稳定结果时按 Entity/Content ID 排序或使用稳定容器。

## 5. 时间与确定性

- Gameplay 只接受 `TickIndex`、`DurationTicks`，不读 `std::chrono::system_clock`、JS Date 或 Axmol delta。
- 页面后台/原生挂起暂停 Session Clock；世界离线时间由显式 `OfflineProgressPolicy` 计算。
- 随机流按领域/实体分片并序列化状态，不能用全局 `rand()`。
- 浮点结果不承诺跨 CPU 位级一致；需要 Web/Native 校验和的规则使用量化值、固定排序和容差规范。
- 碰撞查询返回结果由适配层规范化、量化和稳定排序后进入 Tick。

目标是同一平台/构建严格回放一致，Web/Native 在权威离散状态上语义一致。

## 6. 命令

命令表达意图：`MoveIntent`, `UseAbility`, `Interact`, `SelectDialogueChoice`, `ApplySeal`, `Craft`, `Travel`。共同头：

```cpp
struct CommandHeader {
  TickIndex tick;
  EntityHandle actor;
  CommandSequence sequence;
  CommandType type;
};
```

输入映射只生成动作命令，不包含键码。Web DOM、Axmol 键鼠/手柄和移动触控都映射到同一 `ActionId`。

命令先校验再执行；拒绝返回稳定错误码和本地化参数。同步服务不接收逐帧战斗命令，只接收已提交的持久领域操作。

## 7. 领域事件与同步操作

领域事件表示已发生事实：`AbilityStarted`, `HitResolved`, `EvidenceDiscovered`, `BossPhaseChanged`, `QuestAdvanced`, `RelationshipChanged`, `WorldFlagChanged`。

并非所有事件都同步：

- 粒子、相机震动、普通命中等短暂事件只供 Presentation。
- 任务完成、奖励领取、关系里程碑、地区归位等在持久事务中生成 `PersistentOperation`。
- `PersistentOperation` 带 `operationId`, `profileId`, `baseRevision`, `domain`, `payloadVersion`, `payload`, `createdLogicalTime`。
- Operation 必须幂等；本地存档和云服务对同一 ID 只应用一次。

操作从领域状态变化生成，不把完整内部事件流上传服务器。

## 8. 快照与 Axmol Presentation

Presentation 每渲染帧获得只读 `PresentationSnapshot` 和 `PresentationEventBatch`：可见实体、位置/姿态、动画标签、UI 摘要、音频/特效事件。Snapshot 使用双/三缓冲或不可变 arena，生命周期清楚。

Axmol View 通过 `EntityHandle` 绑定，不把 Node 指针传回 GameCore。View 被流式卸载、WebGL context 重建或窗口恢复后可从最新快照重建。

DOM 只接收低频 `UiEvent`/小型面板模型。DOM 的按钮产生 `UiCommand`，重新进入 Gameplay 校验。

## 9. 能力执行

能力定义编译成受控执行计划：

```text
Trigger → Cost → TargetQuery → Movement → HitWindow
        → Effects → Evidence/SealHook → FeedbackTags → Recovery
```

节点包含输入标签、前置、资源、Tick 时间轴、取消窗、目标过滤、命中形状、效果、AI 评分和表现标签。动画帧事件不能决定伤害；Runtime 时间轴是权威，Presentation 报告视觉偏差供调试。

## 10. 效果与数值

`EffectDefinition` 不可变，`EffectInstance` 保存来源、目标、剩余 Tick、叠加、标签和修改器。计算顺序：基础 → 加法 → 乘法 → 上下限 → 最终覆盖。内容加载顺序不影响结果。

热路径避免每 Tick heap allocation：对象池、small-vector/arena 和预分配事件缓冲在压力测试后使用。先保证所有权和测量，再做微优化。

## 11. 护牒兵击

由 `CombatSystem`, `AbilitySystem`, `HitResolutionSystem`, `PoiseSystem`, `EffectSystem` 组合：

- 输入缓冲用 Tick 表达。
- Runtime `ICollisionWorld` 查询经量化并稳定排序；Axmol 物理不进入权威命中，独立碰撞库若使用也只是 Runtime 私有实现。
- 命中键 `(attacker, abilityInstance, hitIndex, target)` 防重复。
- 精确格挡是规则窗口、方向和标签交集，不由动画回调判定。
- `DeterministicEncounterDirector` 的 legacy 路径继续按 `max_simultaneous_attackers` 发放进攻令牌：正在执行非零 `active_ability` 的敌人先占用令牌，剩余候选按玩家距离、再按 Stable Actor Key 排序；Definition 的令牌数不得超过 authored 敌人数。当前 F1 内容仍走此路径并配置为 1。
- 显式职责/租约模式见第 28 节：只有完整 Actor→有限职责绑定与 `attack_token_lease_ticks` 同时存在才启用。令牌池、租约与轮转游标全部归 Encounter Director，敌方 Actor 不能自增令牌；Active Ability 与尚未到期的租约共同占用池，休眠或击败会归还租约。
- Native 用例已证明 legacy 双令牌补位，以及职责模式中压迫者/侧击者双令牌、击败后骚扰者补位、租约期间不超发和 60/30 FPS 固定步分组一致性。视野扇区、遮挡、正式职责数值、空中/远程/控制组合窗口、F1 内容接线与压力调平仍是第 21/28 节规划目标，不得写成现有内容成果。
- Web 后台/掉帧不会改变攻击速度和资源恢复。

## 12. 工灯定印

`EvidenceSystem` 管观察条件和证据；`SequenceSystem` 管环境工序图；`SealSystem` 施加有限、可回滚规则变换；`ResolutionSystem` 提交永久地区结果和持久操作。

工印只执行白名单操作：启停边、改流向、温度档、负载、颜色/文本片段。内容不得注入 C++、Lua、JavaScript 或任意表达式执行。

## 13. Boss 阶段图与重试

Boss 节点定义招式、AI 权重、环境、证据、同步边界和退出条件；边定义优先级、过渡保护和可逆性。生命阈值只是条件之一。

进入章节/地区 Boss 前创建轻量重试快照：玩家/同伴、Boss、场景机关、消耗规则、随机流和已看演出。Web 本地事务完成后才显示“已保存”。刷新/崩溃后回到合法阶段入口，不尝试恢复任意攻击帧。

禁止 `if (bossId == ...)`。专属机制先由能力、环境节点、证据和阶段组合；新增 C++ 扩展必须有第二个可复用用例。

## 14. NPC 日程、关系与任务

流式外 NPC 以抽象日程推进，进入 Cell 时解析到可达锚点。任务覆盖使用有优先级的临时层，结束后恢复基础日程。

关系变化是领域事务：来源、维度、值、理由、公开性。剧情判断使用具名条件而非魔法数字。

任务图节点必须幂等；等待使用 World Clock，不用线程 timer。奖励、重大选择和地区状态生成持久 Operation；同步重复/乱序不能重复奖励或倒退任务。

## 15. 世界流式

实体状态：Active、Abstract、Dormant、Unloaded。持久 NPC/机关离开 Cell 后压缩为抽象状态；临时敌人按生成规则清理。Axmol 资源释放不等于实体死亡。

地区包由 Platform 下载/缓存，Runtime 解包定义，Presentation 分预算上传图集/音频。Cell 进入前确认依赖包可用；弱网时允许停在可玩的安全枢纽，不让半加载关卡运行。

## 16. C++ 错误与所有权

- GameCore 不跨 WASM 边界抛异常；F1 决定核心内部是否完全禁异常。
- 所有异步回调用 RequestId + generation，捕获 `weak` Session 句柄。
- `std::shared_ptr` 只用于确有共享生命周期，不能默认解决所有权问题。
- 组件/命令/事件内禁止 `std::function` 随意捕获大对象。
- 反序列化先进入验证 DTO，再构造领域对象。
- Release 不用 `assert` 代替外部输入检查。

## 17. 测试切面

- 原生纯系统测试：毫秒级固定 Tick、命令、事件和状态。
- WASM 集成：桥、内存增长、浏览器生命周期、IndexedDB、资源加载。
- 回放：Web 单线程/Pthreads/Windows 的离散状态校验。
- Boss 沙盒：阶段可达、失败/刷新/重试、完整归位。
- NPC 长模拟：30 游戏日无日程/任务死锁。
- 同步属性测试：Operation 重放、重复、乱序、双端分叉和迁移。
- 流式压力：地区反复进出后实体、WASM heap、纹理和缓存回落。

新架构原因见 [`adr/ADR-0006-Cpp分层核心与Wasm平台边界.md`](adr/ADR-0006-Cpp分层核心与Wasm平台边界.md)。

## 18. `GameSession` 生命周期

`GameSession` 是一次正在运行的权威游戏实例，唯一拥有 Tick、EntityRegistry、Gameplay 系统、随机流、当前世界和持久事务协调器。Web Shell/Axmol 只能持有 generation-safe handle。

```text
Uninitialized
  → Bootstrapping (合同/包/存档验证)
  → ReadyAtSafePoint
  → Running ↔ Paused
  → TravelCommitting ──success──► Running/ReadyAtSafePoint
                    └─failure──► Paused + recovery UI
  → ShuttingDown
  → Destroyed

Persistence（正交子状态）
Idle → Pending → Committing → Idle
                 └─failure──► FailedRetryable → Pending/Idle
```

- Boot 失败不产生半 Session；加载旧档先在隔离对象中迁移/验证。
- 普通自动保存/奖励提交是正交异步子状态，不把 Running 变成 Saving，也不默认暂停战斗；UI 通过 `PersistenceStatusChanged` 观察 Pending/Committed/Failed。
- Travel/章节/Boss 结算在事务中生成新安全快照；需要跨 Cell/不可逆切换时进入阻塞的 `TravelCommitting`，成功后切 Head/世界，失败回到 Paused 恢复 UI。
- `Paused` 不推进 Tick，但可完成平台 I/O 并排队下一 Tick 命令。
- 受控“返回标题/退出”拒绝新 Gameplay 命令并有界等待已开始的本地提交；浏览器强制关闭/unload 不保证等待，只保上一安全 Head 和已完成事务。
- 销毁递增 generation，所有旧异步回调和 View binding 自动失效。

## 19. 玩家状态机

不要用一个不断增长的 `PlayerState` 枚举表达所有组合。玩家有三个正交域，命令校验读取它们：

| 域 | 典型状态 | 规则 |
| --- | --- | --- |
| Locomotion | Grounded/Airborne/Falling/Climbing/ForcedMove | 决定地面平面移动、独立高度、纵跃/落差、层连接和规则位移 |
| Action | Free/Startup/Active/Recovery/Guard/Evade/Hitstun/Downed | 决定成本提交、命中窗、取消和受击 |
| Interaction | None/WorldInteract/Dialogue/Craft/MajorChoice | 决定输入上下文、暂停/减速和任务命令 |

优先级不是“最后写入者获胜”：Session 强制状态/Defeat > Hit/Downed > 已提交动作不可取消段 > 明确取消窗 > 自由输入。每个能力数据声明可从哪些 Action 状态进入、在哪些 Tick 可取消到哪些标签；Presentation 动画状态只能跟随。

纵跃包括 `jump_pressed` 缓冲、离地宽限、起跳 commit、可变高度保持和落地恢复；它改变独立高度而不把屏幕方向当成世界纵深。闪身是地面平面上的独立 Action，不能共享同一物理键语义。跨越/攀附只发生在烘焙锚点，避免任意墙面导致动画、遮挡和碰撞组合爆炸。

## 20. 世界状态分层与持久事务

| 层 | 示例 | 生命周期/保存 |
| --- | --- | --- |
| Definition | NPC 原型、能力、任务图、Cell、掉落表 | `.tgdpack` 不可变，不进 Snapshot |
| Session transient | 当前命中、AI 临时目标、粒子事件、输入缓冲 | 不保存；从安全点/定义重建 |
| Checkpoint | 玩家状态、Boss 前置、当前任务、随机流、已加载世界摘要 | 自动保存/失败重试 |
| Profile persistent | 解锁、库存、关系、任务里程碑、地区裁定、天工录 | Snapshot + Operation；跨设备 |
| Device local | 键位、画质、音量、缓存、部分辅助 | Platform 设置库；默认不同步 |
| Service authority | cloudRevision、Operation 去重、设备、官方 EntitlementGrant | Sync Service；客户端只缓存镜像 |

一次持久事务先在 GameCore 中验证全部前置，生成新 Profile state、Operation 和本地 `RewardDedupKey`，再交 IStorage 原子提交。官方付费/平台权益只能消费服务端签发的 `EntitlementGrant`。任一部分失败则不对外发布 `Committed` 事件；Presentation 可播放预备演出，但“已获得/已保存”必须等提交成功。

## 21. AI 更新预算与可解释性

AI 分为感知、意图、导航/移动、能力选择和群组协调。高威胁近屏单位每 Tick 更新必要决策；远处/低威胁单位按 2/4/8 Tick 分桶，降频只延迟重新决策，不改变已经提交的动作时间轴。

- 感知事实有来源、可见时间和置信标签，不让 Blackboard 直接读取玩家私有任务状态。
- Utility/状态机输出 `IntentId + reason tags + validUntilTick`，调试器能说明为何选招/放弃。
- 群战导演控制同时进攻 token，Runtime 已证明最多两个并发槽位的确定性占用/补位；F1 正式段目标仍为最多 2 个高威胁者，但当前内容值为 1，空中/远程/控制职责和“无窗口不同时爆发”规则尚未接线。
- Leash、失去目标、导航失败、Cell 卸载、玩家进入对话和地区归位都有明确退场状态。
- NPC 日程使用分钟/时段级模拟，不每 Tick 跑完整 AI；离屏 NPC 只更新抽象日程和持久事件。
- 30 游戏日长模拟检查任务入口、日程锚点、替代交互和关系事件不死锁。

## 22. 模块 Facade 与事件保留

跨层只通过 Facade/DTO，不让调用者拿内部 Registry 引用：

```text
IGameSession::submit(CommandBatch)
IGameSession::advance(FixedTickBudget)
IGameSession::presentation_snapshot()
IGameSession::drain_presentation_events(ackSequence)
IGameSession::begin_persistence_transaction(reason)
IGameSession::platform_completion(Completion)
```

Snapshot 的 previous/current 双缓冲是 F1 基线，Presentation 用 accumulator alpha 插值位置/视觉量；权威 UI/阶段/资源显示 current，不插值。一次性表现事件带单调 sequence，Presentation ack 后释放；掉帧可以延后但不能重复播放关键音效/演出。View/context 重建时丢弃过期纯表现事件，从 current Snapshot 重建；持久结算事件由 UI 模型和事务状态恢复，不依赖一次性粒子事件。

## 23. 命令回放契约

F1 命令回放使用版本化、显式小端的 `CommandReplay` 二进制格式。回放只记录内容指纹、`GameSessionConfig`、最终 Tick、期望校验和及已映射到世界坐标的权威命令；物理键位、浏览器帧时间、Axmol 对象、屏幕坐标和 Presentation 状态不得进入格式。相同回放必须在 Web/Native 以及 30/60/144 Hz 渲染节奏下抵达同一量化 `x/y/height/floorLayer` 和校验和。

解码器先进入受限 DTO，再构造可执行回放；未知 major、超出当前能力的 minor、截断、尾随字节、越界 Tick、非法方向、未排序或重复排序键、单 Tick 命令溢出、总命令/Tick 上限超限以及校验和不符均须失败关闭。回放版本变化若改变已有字节语义或权威结果，必须新增迁移/兼容决策，不能静默重解释历史数据。

## 24. 合格玩法时长审计

`DeterministicPlaytimeAudit` 与 60 Hz Session 同步推进，但不以经过的全部墙钟或模拟 Tick 冒充玩法时长。F1 内容合同固定 180 Tick 活动宽限：移动、纵跃、已被战斗解析器接受的玩家事件和已提交任务进度只负责续期；宽限耗尽后的运行 Tick 进入 `idle_ticks`。当前 Beat 的合格/闲置时间先留在尝试区，安全推进下一 Beat 才提交；`SafePointRetryCommand` 会把当前尝试全部重分类到 `failure_retry_ticks`，不会删掉诊断证据，也不会让失败刷时进入合格总数。

总合格 Tick 达到 60 分钟仍不足以过门；每个 Beat 必须分别达到 `target_minutes`，防止在单段绕圈后快速跳过薄弱内容。只有任务已经归位、合格总数达标且 7 个 Beat 预算全部达标时，`playable_target_met` 才为真。`PlaytimeAuditSnapshot` 是 Presentation、浏览器 QA 与未来盲测导出器的只读边界，包含合格、闲置、失败重试、当前 Beat 和逐 Beat 达标数及确定性校验和；任何 HUD、JavaScript 或测试工具都不得回写这些计数。

该审计器只能拒绝已知伪时长，不能自动判断移动绕圈、原样敌群、文本质量或探索是否真正非重复。正式 1H 验收仍需真人首玩记录、主持规则、路线/事件 trace 和观察结论；正式盲测记录导出器及其证据包格式目前保持 Missing，不得因 HUD 显示 `1H PENDING/PASS` 而冒充已完成内容密度验收。

## 25. 战斗动作驱动的任务目标

`QuestCombatTriggerDefinition` 把任务目标与稳定的战斗语义连接起来，而不是读取按键或动画帧。F1 当前支持 `player_ability_started`、`player_stance_changed`、`player_hit_guarded` 和 `player_hit_evaded`；Ability 触发器必须提供 `required_ability`，所有触发器都必须提供非空、无重复且不自指的 `prerequisite_objectives`。运行时只在目标处于 Active、姿态/Ability 匹配且全部前置目标已完成时提交进度，错序信号保持无副作用。

当前 F1 schema `1.3.0` 允许战斗触发器同时携带成对的 `required_selection_objective_id` / `required_selection_id`。选择门只能引用先前已发生的 `choose` Objective；同一任务 Objective 可以有多个变体，但不能把无条件与有条件触发器混用，所有变体必须引用同一选择 Objective、option 不重复并完整覆盖 authored options。生成器负责拒绝未来选择与覆盖不全；C++ Resolver 还会拒绝半门、重复 option 和不兼容的同目标定义，并在解析信号时只读取 `IQuestRuntime::selected_option`。Web、Axmol 与输入适配器不得复制“选择→动作”的映射。

沈砚训练保留 5 项触发：檐守重击起手、檐守格挡反制、切换翻花式、翻花轻击起手和翻花闪避反制。返程再增加两个互斥 Definition，共同指向 `f1_objective_demonstrate_rib_calibration`：已选春肋只接受 `stance_eavesguard` + `ability_eavesguard_heavy`，已选冬肋只接受 `stance_flower_turn` + `ability_flower_light`，因此当前总数为 7 项。Web Shell 只把已经由战斗解析器接受的 `CombatEvent` 映射成 `QuestCombatSignal`；物理键、Presentation 特效和敌人偶然命中不能直接完成任务。每次被接受的任务战斗信号后都要重新核对内容化敌群结果，和敌人击败后的核对使用同一 Gameplay Definition；这样即使全部敌人先被清空，随后完成调校动作仍可解锁并收敛 `all_hostiles_defeated`，不会因事件顺序形成软锁。新增 Ability/Stance/防御触发种类可复用这一合同；第 27 节的通用条件求值器已建立最小 Bootstrap，但这些触发 Definition 尚未迁移，等待/失败替代节点和编辑器预览仍为 Missing/Reserved，不能继续向解析器加入 F1 专属分支。

## 26. Beat 边界的敌群激活

F1 的敌对 Actor Definition 全部以 `initially_active=false` 烘焙；`QuestEncounterActivationDefinition` 通过可空 `trigger_objective_id` 为训练、伞巷两轮、调校回程入口/增援和 Boss 指定稳定 Actor Key 组，并必须声明 `EncounterActivationMode`。`replace` 可用于 Beat 入场或同 Beat Objective 完成后的完整切组：恢复玩家、恢复点名敌人并使未点名敌人退出当前组；`reinforce` 必须由同 Beat Objective 触发，只恢复点名的休眠、未击败敌人，保留玩家和既有 Active 敌人的资源、位姿与战斗状态。每个 Actor 都必须有按序对应的 `EncounterActorPlacementDefinition`，包含本次 Home Pose 与 0–14 Formation Slot；缺项、错 Actor、重复/冲突/越界槽位、Active/Defeated 增援目标或返程安全点 aggro 范围外落点均失败关闭。

自 schema `1.2.0` 起，敌群激活可使用成对的 `required_selection_objective_id` / `required_selection_id`。条件激活只能引用已经发生的 `choose` 交互；同一 Beat/触发 Objective 的所有变体必须使用同一选择 Objective、Stable Option 不重复并完整覆盖所有 authored options。`VerticalSliceSession::encounter_activation` 返回 `EncounterActivationMatch`：`boundary_defined=false` 表示没有该边界，存在边界但尚未提交选择时 `activation=nullptr`，唯一匹配时返回 Definition 指针，多个匹配则置 `ambiguous=true`。半配置、未来选择、重复 option、无条件/有条件混合或覆盖不全在初始化前失败关闭。Gameplay 只读取 `IQuestRuntime::selected_option`；Web、Axmol 与浏览器 QA 不得复制选择到激活的映射规则。当前纵切整体 schema 已因第 25 节的战斗动作选择门升至 `1.3.0`，不改变本节既有激活语义。

内容激活使用独立 `EncounterActivationCommand`，以同一 Tick 和全局单调边界序列分别请求 `IEncounterDirector::activate_group` 与 `ICombatResolver::activate_group`；Director 更新归位点/接敌槽并只重置受影响的攻击 Runtime，Combat 按模式恢复/隔离 Actor。`SafePointRetryCommand` 及 `SafePointRetryReason::player_defeated` 只用于真正倒地的玩家，不再承载任务推进。失败重试先恢复当前 Beat 的入场 `replace`，再按 Beat/触发 Objective 对已完成边界去重，交由上述 Gameplay 匹配器重放唯一已选变体，因此返程 `reinforce` 不会丢失或把春冬两个 Definition 都重放。`CombatActorSnapshot` 以独立 `defeated` 位表达生命归零：`active=false && defeated=false` 是休眠/未选中，只有 `defeated=true && health=0` 才可计入击败任务；Active+Defeated、Defeated+非零生命均失败关闭。训练构件继续使用独立 Archetype，避免练习对手进入正式伞偶/纸鹭族群计数。

两个 `activate_group` 接口都接受 placement span，并在写入任何 Runtime 前校验整批 Actor、模式、槽位与 sequence。`tests/native/combat_resolver.cpp` 和 `tests/native/encounter_director.cpp` 现用同一类边界分别证明：两名休眠 Actor 可在一个 `reinforce` 命令中一起提交；若批内较后的 Actor 已 Active，较早 Actor、校验和与单调 sequence 均不改变；合法重试仍可复用该 sequence，而已提交 sequence 的回放无第二次变更。该原子性只属于单个 Resolver/Director。`F1GrayboxLayer::activateEncounterForBeat` 当前仍先调用 Director、再调用 Combat，没有共同的 prepare/commit、补偿或快照回滚；若前者成功而后者因待处理 Combat 命令/位姿等运行条件失败，两个子系统仍可能分叉，因此禁止把这组 Native 证明描述成跨组件事务。

Stage Advance 和 Objective Complete 都可能由场景交互或 `CombatEvent` 产生。Web Shell 只能先记录待激活 Beat/Objective，再在交互解析后或战斗事件发布完成后的 Tick 安全点应用，禁止在 `ICombatEventSink::publish` 回调栈中重入修改 Combat Resolver。沈砚训练用两种单体教学方位、伞巷用 1/5 对向夹击与纵深纸鹭槽、返程用 0/3/6 三角入口组，并在 Slot 5 根据春肋选择追加地面伞偶、根据冬肋选择追加高位纸鹭，共同证明同一内容边界可驱动完整替换、单 Actor 叠加增援和先前选择的可见后果。当前已实现的是 8 项固定 Definition 与“3→4”Bootstrap，以及 Combat/Director 各自的多 Actor 批量增援 Native 边界；F1 JSON 尚未编排一个真正的多 Actor `reinforce` Definition，也没有 Web/浏览器证据或跨组件原子协调。通用条件程序的 C++ 最小合同/求值器已按第 27 节 Bootstrap，职责/令牌租约合同已按第 28 节 Bootstrap；但多个 Objective 会合、职责 Schema/生成器/F1 Definition、Director 令牌状态的快照/回放协议、动态队形、刷出/退场演出、导航避障和 Workbench 条件/编队/时间线仍为 Missing/Reserved。

## 27. 通用任务复合条件最小合同

`QuestConditionProgramDefinition` 用稳定 `ContentId` 命名一段有界后缀布尔程序；当前指令仅允许 `objective_completed`、`selection_equals`、`all`、`any` 和 `negate`。`DeterministicQuestConditionEvaluator::initialize` 在进入 Session 前一次性校验条件库：1–64 个程序、每程序 1–64 条指令、求值栈最多 16 项、`all`/`any` 的 fan-in 为 2–8，并拒绝不稳定/重复条件 ID、未知 Objective、未由 `choose` 交互声明的 option、操作数不足、悬空结果、非法 payload 和未知 opcode。任一错误都失败关闭，不生成部分可用的条件库。

条件状态的唯一 Owner 仍是 `IQuestRuntime`。求值器只调用 `objective_state` 与 `selected_option`，使用固定栈、无分配、无状态写入；未初始化和未知条件返回显式错误，Objective 未完成或选择不匹配只得到 `false`。Presentation、Web Shell、浏览器 QA 与内容工具不得复制求值逻辑或保存第二份条件结果。

形成精确提交 `37f5dd6` 的同一源码由 Native 用例证明两个 Objective 与一个选择的 AND 会合、路线 OR、NOT、嵌套 AND/OR 和替代分支，并覆盖未知引用、空程序、栈上/下溢、fan-in 超限与重复 ID；提交前工作树的 MSVC/Clang Release、目标 CTest 和 Emscripten Web Single Release 均通过。该状态仅为 **Bootstrap Implemented**：`VerticalSliceDefinition` 尚未拥有条件程序 span，JSON Schema/生成器/生成头和现有 Interaction/Combat/Encounter 消费者也尚未接线。旧的 `prerequisite_objectives` 与成对选择门在一次性迁移完成前仍是权威真相；后续迁移必须同时删除被替代字段和专用匹配分支，禁止长期保留两套条件真相。

## 28. 敌人职责与进攻令牌租约最小合同

`EncounterTacticalDuty` 是有限枚举，只包含 `pressure`、`flanker`、`harrier`、`controller`；`EncounterActorDutyDefinition` 以 Stable Actor Key 绑定职责，不接受字符串、Archetype、Objective 或 Boss 名称特判。新模式采用显式迁移：`EncounterDirectorDefinition.actor_duties` 与 `attack_token_lease_ticks` 必须同时为空或同时有效。两者都为空时保留第 11 节的 legacy 距离仲裁与既有校验和；启用时必须恰好覆盖每一名 hostile，Actor 不得重复或指向玩家，职责枚举必须已知，租约必须为 1–600 Tick，令牌池仍不得超过 hostile 数量。半配置、缺绑定、重复 Actor、未知职责、超长租约全部失败关闭。

职责模式的状态 Owner 只有 `DeterministicEncounterDirector`。每个 hostile Runtime 保存职责、租约截止 Tick 与上次获授 Tick，Director 保存有限职责轮转游标；一次获授覆盖当前 Tick 至 `tick + leaseTicks - 1`。非零 Active Ability 或未到期租约各占一个池槽，同一 Actor 不会重复计数；休眠、击败或租约到期会在成功提交下一 Tick 时归还池槽。仲裁从游标职责开始循环寻找候选，优先让不同职责获得槽位；同职责内按“最久未获授、距玩家距离、Stable Actor Key”排序。所有选择先完成快照/Ability 校验，再一起写入租约、攻击计数、冷却和游标；Active+Defeated 快照失败关闭且不消费 Tick。失败重试与 `replace` 重置全部租约/游标，`reinforce` 只重置点名 Actor；职责、租约、上次获授 Tick和游标均进入 Director checksum。

`tests/native/encounter_director.cpp` 证明：双令牌首 Tick 稳定授予压迫者与侧击者；压迫者被击败后立即归还槽位，由等待的骚扰者补位；两项存活租约期间不生成第三条攻击命令；把同一 60 Tick 流按 60 FPS 的 1 Tick/帧或 30 FPS 的 2 Tick/帧调用，并反转输入快照顺序，命令 Tick/Actor/sequence 与最终 checksum 完全一致。MSVC Release 与锁定 Clang 19 Release 完整 CTest 均为 16/16，Emscripten/Axmol Web Single Release 重编通过。

该状态是 **Bootstrap Implemented / Not Authored In F1**。当前 `f1-vertical-slice.json` 仍只有 `maxSimultaneousAttackers: 1`，没有职责/租约字段；JSON Schema、生成器、生成头迁移、F1 双令牌内容、空中/远程/控制行为参数、压力调平、Web/浏览器路线，以及可恢复 Director 租约的跨 Combat/Quest/Profile 快照与回放协议都尚未完成。内容组不得复制单人激活或按敌人名称写分支来冒充本合同。

## 29. Definition 驱动的任务 UI 投影边界

`QuestUiCueDefinition` 以稳定 cue、Beat、source mask 和 Objective 域声明只读任务投影；空 Objective span 表示该 Beat 内的 wildcard。初始化会拒绝空/未知 ID、未知 source bit、重复或跨 Beat Objective、同 Beat/source 下 wildcard 与显式域重叠，以及多个 cue 覆盖同一域；每个 cue 的显式 Objective 与 result selector 各自最多 8 项。`QuestUiResultSelectorDefinition` 只授权 Definition 明确声明的跨 Objective 结果组合，并可为“动作已接受但世界反馈仍为负向”提供 `negative` polarity override；普通 polarity 仍由 secondary（适用时）或 primary 结果状态推导，不能把 `accepted trigger + rejected outcome` 压成一个总状态。

合格时长证据不从 UI fixture、Objective ID 或 `accepted/rejected` 单独推断。每个 cue 必须声明 1–16 条 `QuestUiAttemptEvidenceRuleDefinition`，以 `source + focus Objective + primary result ID/status/reason + secondary result ID/status/reason` 组成无 wildcard 的精确键，并派生唯一 `QuestUiAttemptTimeClassification`。空结果必须精确写成 `not_applicable + empty ID + none`，不是“匹配任意结果”；同键即使输出不同 classification 也属于歧义定义。每个 cue 的所有 source bit 都必须至少被一条规则覆盖，interaction/trigger/outcome 还会按类型、Beat 和既有跨 Objective selector/直接 authored progression 反查。未知枚举/结果、重复键、source/classification 白名单错配或非法 sentinel 在 producer 初始化时失败关闭。

`DeterministicQuestUiProjectionProducer` 只读取当前 `IQuestRuntime`、移动安全点、Combat Actor 快照和已验证的低频结果信号，输出固定容量 `QuestUiProjectionSnapshot`。`QuestUiProjectionSignal` 只携带 source、focus Objective 和双结果槽，没有 attempt classification 覆盖入口；producer 在结果语义验证后查找 Definition 精确规则，缺规则返回 `missing_attempt_evidence`，派生 classification 会进入整体 checksum。`DeterministicQuestRuntime` 的 stage 语义会把当前 Beat 内所有未完成 Objective 报告为 Active；producer 接受这些同 Beat 事实并存，但每条投影只有一个由 `signal.objective` 指定的只读焦点，不会选择、拥有或压扁其余 Objective。`pending_objective` 只在该焦点 Active 时等于焦点；焦点 Completed/Locked 时为 0，不能按 author order 猜测“下一个目标”。choice/recovery 要求焦点 Active 且 pending 与焦点一致；跨 Objective selector 继续要求结果 Owner 已完成、焦点 Active 并满足直接 authored progression。快照还包含单调 projection sequence、Quest 权威 checksum、cue/Beat/Objective、安全点、已提交选择、已完成 Objective、排序后的 Active/Defeated hostile、两个各自拥有 ID/Objective/status/reason 的结果槽、稳定 attempt-time classification 与整体 checksum。Actor 身份由 Definition 决定：player key 只能携带 player faction，Encounter/Boss 授权的 hostile key 只能携带 hostile faction，未知、重叠或阵营伪装都失败关闭。attempt-time classification 按 source 使用固定白名单，choice 不可映射为 combat proof、combat feedback 不可映射为 craft decision；`ignored_repeat` 唯一合法 reason 是 `selection_already_committed`。任何其他 Beat 出现 Active、未知/重复/非法 Actor、未知或错类型 interaction/trigger/outcome、跨 Beat 引用、选择/完成事实不一致、缺少精确 evidence rule 或任一容量溢出时均失败关闭，且不会替换上一份有效快照或消费 sequence。

choice 面板的 options 只按 Definition 中当前 Active `choose` Objective 的 authored interaction 顺序完整派生，最多 8 项，重复 selection 或缺失 interaction 在初始化前失败关闭。`QuestUiSelectionIntent` 必须精确引用 projection sequence/checksum、Objective、interaction 与 selection；提交前重新核对当前 Quest 仍属于同一 Definition/Beat、Quest checksum 未变化、Objective 仍 Active、selection 尚未提交，并确认 interaction/selection 对仍是该投影的完整 authored option。UI Action 因而只表达意图，不拥有 Objective、Combat、选择或奖励真相。

通用 Native 目录以 16 个彼此独立的权威语义快照样本覆盖互斥选择、accepted-positive、accepted-negative override、rejected、trigger-only、`trigger accepted + outcome rejected`、檐守/翻花各自成对的恢复 offer/resume 和两套 choice options，并逐项断言 Definition 派生的 attempt classification；同一 combat result IDs 的 accepted/accepted 变体还证明 polarity 可变而分类规则仍按完整 status 精确选择。负例覆盖重复精确键、未知结果/枚举、source/class 不兼容、非空 `not_applicable` sentinel、缺规则且上一 snapshot/sequence/checksum 不变；编译期断言保证调用信号不存在 UI override 字段。另直接启动真实 `DeterministicQuestRuntime`，证明当前 Beat 多个 stage-active Objective 并存时 choice projection/intent 可用，并用独立快照覆盖 objective/combat 多 Active、跨 Beat Active、locked choice focus、stale intent、定义歧义、结果引用、Actor 身份/阵营、cue Objective/selector/rule 的 8/8/16 上限及 choice/selected/Actor/retained 的 8/16/16/64 容量和失败不变性。

该状态是 **Implemented / 1.6 Content Authored**。为兼容旧 C++ 聚合，`VerticalSliceDefinition.quest_ui_cues` 与 cue 尾随 `attempt_evidence_rules` 都有空 span 默认值；pre-1.5 Definition 留空时旧消费者继续编译，而主动初始化投影 producer 会失败关闭。当前根 schema/generated 已为 1.6.0，八个 cue、16 条规则与两路线 checksum 已接入；Stage② 已按第 30 节接通 Platform/Web envelope、正式 Presentation renderer、Action 与 Native fallback，Stage③ 再按第 33 节从权威 Quest/Combat/Recovery 事实组装原 14 类 raw signal 与两个 paired recovery 变体。fixture、prototype flow/autoAdvance、计时分类或 F1 Objective ID 均不能成为运行时派生源。

## 30. 任务 UI Platform、Action 与 Presentation 边界

Web ABI 1.2 在既有 Profile/存储消息之上增加三类同版本信封：JS→C++ 的 `quest_ui_selection_intent`、C++→JS 的 `quest_ui_event` 与 `quest_ui_close_ack`。投影保留 producer sequence/checksum，信封头另有同 session generation 内严格递增的 wire `messageSequence`；二者不能互相替代。`WebPlatformBridge` 只接受固定容量、尾部归零且枚举/结果/选项/Actor/retained Objective 形态合法的投影，并单独保存当前 choice 的 generation + projection sequence + checksum。错误 generation、错误 sequence/checksum、非 choice 身份、旧确认均失败关闭且不替换 pending 消息；相同确认幂等，新权威投影优先并抑制旧 choice close。JS bridge 再按 wire sequence 拒绝重复或乱序消息，renderer 还会核对同一面板身份与 `close.messageSequence > panel.messageSequence`。

DOM renderer 是 Presentation consumer，不是 Quest Runtime。它从稳定 cue、focus Objective、source 与 raw result 槽选择玩家文案；允许在 Presentation 目录登记 Stable F1 ID，但不得从 attempt-time classification、CSS、按钮或 fixture 推断路线、进度或结果。classification 可以保留在 decoded bridge QA 对象，却不得进入 renderer model、copy、Action 或 close。choice options 必须逐项保持 projection authored 顺序；按钮 Promise resolve 只进入“等待权威确认”，不本地隐藏或推进，等待期间保持原焦点并使用 `aria-busy`/`aria-disabled` 阻止重复提交，技术异常只进入 lifecycle/QA。只有更高 wire sequence 的权威 replace/close 才改变 DOM，关闭后焦点回到可聚焦 Canvas，重复 close 不再次恢复焦点。

应用层 `has_authored_cue(beat, objective, source)` 是只读能力查询，不产生投影、不消费 sequence，也不验证选择本身。只有当前 Objective 被 `choice_available` cue 覆盖时，`F1GrayboxLayer` 才强制进入 projection → choice state → `validate_choice_intent` → `complete_objective`；投射失败保持失败关闭。未被 cue 覆盖的 legacy `choose` Objective 继续在具体世界交互上提交 interaction 自带的 authored selection，既不默认第一个选项，也不由 UI 保存第二份选择真相。无 Web consumer 时，同一 App 层 choice state 显示按投影顺序排列的 `1..N` 英文调试标签，数字键构造完全相同的 intent；越界键无副作用，成功后退出输入门控。标签只属于 `apps/web-shell` Presentation fallback，不得扩散到 Gameplay/Definition，也不是 P01–P05 正式玩家候选。

当前根 F1 schema/generated 已为 1.6.0，八个 cue / 16 条 evidence rule 与两路线 checksum 已接入。抵达、系泊、训练道三个真实 choice 会激活本节的 Platform/Action/renderer 或 Native fallback；后五 Beat 未投射选择继续走显式世界交互。第 33 节已把其余 11 类 Interaction/Combat/Recovery raw signal 与两个 paired recovery 变体接到同一 producer/Platform/renderer 通道；10 个 Stable Asset ID resolver、训练安全点 Encounter+Session+Combat 完整快照/原子重建、正式 recovery Action、无开发界面玩家构建、200%/辅助完整矩阵及真人盲测仍未完成。

## 31. 交互尝试只读判定边界

`DeterministicQuestInteractionResolver::resolve` 继续只返回 Objective Active、选择门吻合、前置完成且在范围内的可用交互，既有 span 初始化和“距离平方 → StableContentKey”排序不变。新增 `resolve_attempt` 是纯加法诊断查询：调用方必须用完整 `VerticalSliceDefinition` 初始化 resolver，使其能核对 Quest ID、当前 Beat 与 Objective 归属；旧 span 初始化仍可使用 `resolve`，但不能调用 attempt 路径。

attempt 先按与 `resolve` 相同的 Cell、floor、选择门、半径和候选排序查找 `eligible`，任何可用候选都优先于不可用候选。仅在没有可用候选时，当前 Beat 的 Active Objective 可因前置未完成返回 `prerequisite_incomplete`；当前 Beat 已完成的 `choose` Objective 只有在 `IQuestRuntime::selected_option` 仍指向同一 Objective 的作者化 option 时才可返回 `selection_already_committed`，即使玩家触碰的是该 Objective 的另一项作者化交互。已完成 inspect/operate/talk、未来或过去 Beat、required-selection 不符、越层、越距、未知 Quest/Beat 都不会冒充这两类结果。

该查询为 `const`，只读取 `IQuestRuntime::snapshot/objective_state/selected_option`，不完成 Objective、不消费 Quest command sequence、不改 tick、选择或 checksum。Native 正负用例使用实际 1.6 Definition 覆盖正常候选、未读码鸣铃、已提交线索的另一选项、读码后鸣铃、1 mm 越距、未来 Beat、路线不符、完成非 choose、可用候选压过同位不可用候选、重复查询与 Definition/Quest 上下文错配。第 33 节的 App emitter 已按通用 disposition 映射 raw result status/reason：每次世界 F 只查询一次，拒绝/重复不提交命令，eligible 只有在 Quest command accepted 后才发布。resolver 仍不得出现 F1 ID，查询本身仍不能推进 Quest。

## 32. 战斗目标尝试只读判定边界

`DeterministicQuestCombatOutcomeAttemptResolver` 是现有 trigger 与 outcome resolver 之间的纯加法观察层。它必须用完整 `VerticalSliceDefinition` 初始化，并复用 `DeterministicQuestCombatTriggerResolver` / `DeterministicQuestCombatOutcomeResolver` 的 Definition 校验；此外还检查 Beat ID、Objective 唯一归属，以及 trigger 的 Objective、选择门、前置和 outcome Objective 都能在同一 Definition 中唯一反查。初始化失败不保存 Definition 指针，调用方可修正后重试；成功后再次初始化返回 `invalid_lifecycle`。

`evaluate_attempt` 的输入是已经由 trigger resolver 接受的 `QuestCombatTriggerResult`、产生该信号的原始 `CombatEvent`、只读 `CombatActorSnapshot` span 与当前 `IQuestRuntime`。它先反查 trigger ID/Objective，再按通用事件语义核对身份：`player_ability_started` 要求玩家 source 与精确 Ability，`player_stance_changed` 要求玩家 source、零 target 与精确 Stance，`player_hit_guarded` / `player_hit_evaded` 要求 Definition 已作者化的 hostile source 和玩家 target。随后核对 Quest ID、Beat index/ID、stage count、trigger 属于当前 Beat且其 Objective 已完成。缺失、拒绝、错 ID、错事件类型、错 source/Ability 或未完成 trigger 都返回显式错误，不能伪造成 accepted history。

候选只从 trigger Objective 在当前 Beat author order 的下一项产生；该 Objective 必须恰好绑定一个当前 Active `hostile_archetype_defeated` outcome。不存在下一项、下一项没有 outcome、kind 为 `all_hostiles_defeated` 或 Objective 已非 Active 时返回 `no_candidate`。这一步先于 Actor snapshot 校验，因此檐守反制后下一项仍是动作 Objective时，即使事件 target 是玩家且未传 hostile snapshots，也只能得到 `no_candidate`，不会误报 `invalid_actor_snapshot`；禁止按 trigger/outcome 名称越过中间 Objective 猜测候选。

只有候选存在时才读取 event target。target 必须非零，在 Encounter activation 或 Boss phase 中已作者化，且在输入 span 中恰好出现一次；其 faction 必须为 hostile，当前 Active、未击败、生命为正且 Archetype 非零。与 outcome Archetype 相同返回 `target_matches_pending`，不同返回 `wrong_target`；未知、未授权、重复、非 hostile、休眠、击败或零生命 target 均失败关闭。两种 disposition 都不会完成 Outcome，也不会修改 Actor、Quest command sequence/checksum、选择、奖励或 accepted trigger 结果。只有 `DeterministicQuestCombatOutcomeResolver::resolve` 可在真实击败快照上报告条件满足，调用方随后仍须提交显式 Quest command。

Native 用例以实际 1.6 Definition 的独立训练状态覆盖檐守无候选、翻花重击命中训练架的 wrong target、命中证明靶但仍 pending，以及重复结果完全一致；负例覆盖 trigger/result/event 身份、target 形态、Definition/Quest context 与失败初始化。第 33 节的 App emitter 已把 `wrong_target` 通用映射为 secondary outcome `rejected/wrong_target`，与 accepted primary trigger 共同交给第 29 节 producer；`target_matches_pending` 不产生失败投影，resolver 内没有 F1 ID、fixture 序号或计时分类分支。

## 33. 首两拍权威任务反馈 App 接线

`apps/web-shell` 的 `F1QuestUiSignalEmitter` 是 App 边界组装器，不是第二套 Quest Runtime。它只接受已经由 Definition/Runtime 验证的 `QuestInteractionResult`、`QuestCombatTriggerResult`、`QuestCombatOutcomeAttemptResult`、`QuestEvent` 与只读 Quest snapshot，输出 raw `QuestUiProjectionSignal`；类本身不提交命令、不读取 cue/evidence rule、Presentation copy、fixture 或 attempt-time classification，也不含 F1 Objective/interaction/trigger/outcome ID 分支。所有信号仍必须经过第 29 节 producer 才能获得 cue、polarity、classification、Actor、sequence 与 checksum。

世界交互按一次调用边界使用完整 Definition `resolve_attempt`。`eligible` 复用原完成路径，只有 `complete_objective` 真实 accepted 后才映射 accepted；`prerequisite_incomplete` 与 `selection_already_committed` 分别映射 rejected/prerequisite_incomplete 和 ignored_repeat/selection_already_committed，且不提交 Quest command。已投射 choice 在精确 intent 验证和完成后，用作者化 interaction 作为 result owner，并只在 author order 紧邻 Objective 当前 Active 时把投影 focus 迁移过去；未投射 legacy choose 仍走具体世界交互，不默认首项。

`objective_state` 只从真实 `QuestEvent::objective_completed` 的同 Beat author-order 紧邻 Active Objective 产生；App 再以 `has_authored_cue` 做能力过滤，不扫描 cue/evidence rule 反推玩法阶段。Combat trigger 完成后，outcome attempt 必须在原始 `CombatEvent` target 尚未被 Encounter replace 的 Actor snapshot 上立即求值；raw signal 随后进入容量 8 的固定队列，只有 `applyPendingEncounterActivation` 成功后才 flush。`no_candidate` 发布 trigger accepted + secondary N/A，`wrong_target` 发布 accepted primary + rejected secondary，`target_matches_pending` 不发布失败；投射失败不回滚已经 accepted 的 Quest，也不追加第二条命令。

恢复焦点按当前 Beat author order 找首个未完成 Active frontier。`recovery_offer` 只在玩家击败事件已成为 Combat 真相后请求；App 只从该倒地后的 `CombatActorSnapshot` 暂存非零权威 stance。Session retry、Encounter restore、Combat restore 和移动安全点全部成功后，若 Combat 初始 stance 与暂存值不同，App 只排入既有 `switch_stance` 命令，不直接写 Actor；`F1QuestUiRecoveryResumeGate` 此时必须观察实际 retry Tick 之后的匹配 `stance_changed`。若初始 stance 已匹配则不要求该事件；两种路径都必须等后续 Session/Encounter/Combat 同步 Tick，并重新读取 Active/non-defeated/matching-stance 玩家 snapshot。只有 Quest/Beat/frontier、completed_total、selection_count 和 checksum 均未漂移时，才可投射 `recovery_resume`。倒地 Tick 只是 retry Tick 的下界，不能要求玩家立刻重试。

当前 1.6 内容已为檐守和翻花两个精确 frontier 各作者化 offer/resume；cue 能力过滤只决定真实 focus 能否投射，绝不能反过来挑 Objective。此 gate 是 App freshness 边界，CombatResolver 仍独占 stance 真相，QuestRuntime 仍独占进度/选择/checksum。任一恢复子步骤或身份核对失败都不发 resume，但既有三个组件之间还没有 prepare/commit/rollback；因此失败后的部分组件回滚、跨组件 defeated history、离开返回、刷新恢复和完整 Encounter+Session+Combat 安全点快照仍为 Open。正式 recovery DOM Action、offer 首焦 retry、leave 可达也尚未由本节的 Canvas `R` 路径关闭。

Native actual-Definition 用例以彼此独立的 Quest 快照覆盖线索重复、交叉系缆/快速结、鸣铃错序/接受、两训练阶段、檐守 trigger-only、翻花 accepted+wrong-target 双槽及两个 frontier 的 paired recovery，并断言 read-only disposition、重复 wrong-target、投射失败均无额外推进。额外 gate/Combat 正负例覆盖：实际 retry Tick 晚于倒地 Tick、早于倒地的非法 retry、重复 mark、三组件 Tick 任一不一致、非法/未生效 stance、需要切换却缺少后续 stance event、Quest/Objective 漂移，以及无需切姿态的檐守恢复不要求 event、也不会被翻花逻辑污染。浏览器高水痕/迎风路线在檐守 frontier、循钟声/背风路线在翻花 frontier 各走一次真实倒地→Canvas R→三组件重试→fresh resume，并在安全点正常移动回训练线标后看到敌人重新攻击；两者的 source/focus/classification、wire/projection sequence、Quest completed/selection/checksum 与 7 Beat 结算均受断言，fresh resume 后的重复 R 还必须在权威审计 Tick 推进后保持 retry count、消息与 Quest 身份不变。报告位于 `.tmp/f1-recovery-02-precommit-canopy/report.json` 与 `.tmp/f1-recovery-02-precommit-drain/report.json`；它们是 dirty-worktree pre-commit Chrome 候选，不是 exact-commit、远端三浏览器或真人时长证据。

## 34. 数据化主动技能最小切片

Action Registry 已有的 `weapon_skill` 是输入入口，但 Platform/Presentation 只提交稳定 Action sample，不提交 Ability ID、键码或技能归属。`DeterministicCombatActionIntentMapper` 接受 Gameplay 附加的 Actor 与 target context，把非重复 press edge 和单调 Platform sequence 映射为 `CombatSkillSlot::primary`；首切片只消费 primary，secondary/utility/special 仅冻结枚举与容量。未知 Action、release/repeat、空身份和重放 sequence 都不会替换上一份有效映射状态。`CombatCommand` 只携带 slot，不能携带调用方选择的 Ability。

`CombatActorConfig` 以尾随默认字段保存固定容量 16 的 authored `skill_loadout` 与 count，对应最多 4 stance × 4 slot。空 loadout 保持 legacy 聚合初始化；有效项必须绑定 Actor 自己的 stance、合法且唯一的 `(stance, slot)`、存在的 `weapon_skill` Ability 和一致的 `required_stance`，同一 Actor 不能重复 Ability，count 之后必须全零。Resolver 只规范化私有配置副本，不改调用者输入；不同 Actor 可以共享同一 Definition，但拥有独立 cooldown。

状态 Owner 只有 `DeterministicCombatResolver`。玩家与敌人沿同一路径，在命令执行 Tick 使用 Actor 当前 Combat-owned stance + slot 查找 owned Ability，再校验 target faction/active、stamina、active timeline 与 absolute ready Tick；未装备 slot、其他 Actor 的 Ability、资源或状态失败只产生 `command_ignored`，不回退全局 Ability 查找，也不扣资源、写 cooldown 或造成伤害。sequence 非零只约束 `weapon_skill`，legacy command 不在本切片收紧。

`query_skill_cooldown` 返回结构化 `CombatSkillQueryError + ability + ready_tick`，明确区分 unknown actor、invalid slot、slot unbound、ability not owned 与合法 `ready_tick == 0`。运行时只为有效 binding 保存 ready Tick；有 binding 时 checksum 按稳定 Actor、规范化 binding、被引用 Definition 与对应 ready Tick进入独立 hash 域，全部 loadout 为空时不增加任何 legacy checksum 字节。

Actor 倒地取消正在进行的技能但保留已经提交的 cooldown；retry、Encounter replace/reinforce 只为实际恢复的 Actor 清除瞬时技能状态，并从当前边界 Tick 重建其 authored initial cooldown，其他 Actor 的 ready Tick 不漂移。`opposing_actor` 继续进入既有命中/格挡/闪避/硬直管线；`self_actor` 与 `no_target` 当前只允许零直接伤害的受控时间线。

`tests/native/combat_skill.cpp` 覆盖 Action→primary slot、同 stance 玩家/敌人归属隔离、共享 Definition 的 Actor-local cooldown、切 stance 后同 slot 解析变化、结构化查询、未装备与错误配置失败不变性、loadout/Definition 顺序归一、空 loadout legacy checksum、倒地保留以及 retry/reinforce 定向重建；旧 `combat_resolver`、`encounter_director` 与固定内容回放继续作为兼容门。该状态是 **Bootstrap Implemented / Not Authored In System Demo**：被动效果、增减益/持续效果、取消窗、连段、升级替换、AI 选槽、存档序列化、HUD、正式 Sandbox Definition/pack、平台玩家接线与手感验收仍为 Open。
