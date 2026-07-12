# 11｜Runtime 与 Gameplay 组件模型

> 上级文档：[`../01-开发者文档.md`](../01-开发者文档.md)
> 状态：Accepted Baseline
> 最后更新：2026-07-11
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
- AI Director 按视野扇区、纵深与遮挡发放进攻令牌，控制斜向全景群战可读性。
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
- 群战导演控制同时进攻 token，F1 正式段最多 2 个高威胁者；空中/远程/控制角色不能在无窗口时同时爆发。
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

沈砚训练的 5 项触发依次为檐守重击起手、檐守格挡反制、切换翻花式、翻花轻击起手和翻花闪避反制。Web Shell 只把已经由战斗解析器接受的 `CombatEvent` 映射成 `QuestCombatSignal`；物理键、Presentation 特效和敌人偶然命中不能直接完成任务。新增 Ability/Stance/防御触发种类可复用这一合同，但通用条件表达式、等待/失败替代节点和编辑器预览仍为 Missing/Reserved，不能继续向解析器加入 F1 专属分支。
