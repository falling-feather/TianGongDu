# 11｜Runtime 与 Gameplay 组件模型

> 上级文档：[`../01-开发者文档.md`](../01-开发者文档.md)
> 状态：Accepted Baseline
> 最后更新：2026-07-10

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
- Axmol/Box2D 查询经 `IPhysicsQuery` 适配并稳定排序。
- 命中键 `(attacker, abilityInstance, hitIndex, target)` 防重复。
- 精确格挡是规则窗口、方向和标签交集，不由动画回调判定。
- AI Director 发放进攻令牌，控制横版群战可读性。
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
