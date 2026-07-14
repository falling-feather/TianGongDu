# 11｜Runtime 与 Gameplay 组件模型

> 上级文档：[`../01-开发者文档.md`](../01-开发者文档.md)
> 状态：Accepted Baseline
> 最后更新：2026-07-14
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
