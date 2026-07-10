# 11｜Runtime 与 Gameplay 组件模型

> 上级文档：[`../01-开发者文档.md`](../01-开发者文档.md)
> 状态：Accepted Baseline
> 最后更新：2026-07-10

## 1. 为什么是混合组件模型

项目需要成百上千种内容组合，却不需要为了理论上的百万实体引入全量 Archetype ECS 复杂度。采用“稳定实体 ID + 数据组件 + 顺序系统 + 少量领域对象”的混合模型：高频状态连续存储，复杂任务/Boss 用显式状态机，Godot Node 只做表现视图。

目标是让新角色通过组件组合创建，而不是让所有逻辑塞进 `NpcBase` 的深继承树。

## 2. 实体身份

每个运行时实体拥有：

- `EntityId`：本次 Session 内唯一的紧凑 ID，不写入长期内容引用。
- `ContentId`：可选的稳定内容原型 ID，例如 `npc_shen_yan`。
- `InstanceId`：需要持久化的世界实例 ID，例如某扇已修复闸门。
- `PackageId` 与版本来源。

存档保存 `InstanceId`/`ContentId` 和必要状态，不保存内存索引。实体重建后可获得新的 `EntityId`。

## 3. 组件类别

### 3.1 基础组件

`TransformState`, `LifecycleState`, `TagSet`, `Ownership`, `PersistencePolicy`, `PresentationRef`。

核心 Transform 使用引擎无关数值；Godot 适配负责坐标转换和插值。纯静态装饰不进入 Runtime 实体系统。

### 3.2 战斗组件

`Vitality`, `Stamina`, `Poise`, `LanternCharge`, `Combatant`, `WeaponLoadout`, `AbilitySet`, `EffectSet`, `HitShapeState`, `TargetingState`, `ThreatState`, `Faction`。

伤害、硬直、失序和证据是不同管线，禁止一个 `health` 值承担全部含义。

### 3.3 世界与角色组件

`Schedule`, `DialogueRole`, `RelationshipParticipant`, `QuestParticipant`, `Profession`, `TradeProvider`, `CompanionCapability`, `TravelCapability`, `WorldStateBinding`。

NPC 没有用到的功能不挂空组件。核心 NPC 可以同时组合日程、关系、任务、专业和短期同行，不需要专属子类。

### 3.4 AI 组件

`Perception`, `Blackboard`, `Intent`, `NavigationAgentState`, `CombatArchetype`, `UtilityProfile`, `GroupRole`, `LeashPolicy`。

感知事实与决策分离；表现层可延迟动画，不能更改 AI 已提交的意图。

## 4. 系统执行阶段

每个固定 Tick 严格执行：

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
12. EmitEventsAndSnapshot
```

同一阶段内系统顺序由显式注册表固定并有测试。系统不能在迭代组件时立即结构性删除实体；使用命令缓冲在 Cleanup 阶段提交。

## 5. 命令、事件与快照

### 5.1 命令

命令表达玩家/AI 想做什么，例如 `MoveIntent`, `UseAbility`, `Interact`, `SelectDialogueChoice`, `ApplySeal`, `Craft`, `Travel`。命令包含 Tick、发起者、参数和幂等/序列信息。

命令先校验再执行；失败返回结构化拒绝原因，UI 可以解释“体力不足”“目标失效”“证据不足”，而不是猜状态。

### 5.2 领域事件

事件表达已发生事实，例如 `AbilityStarted`, `HitResolved`, `EvidenceDiscovered`, `BossPhaseChanged`, `RelationshipChanged`, `QuestAdvanced`, `WorldFlagChanged`。事件在 Tick 结束发布，处理器不得无限同步递归发事件。

持久事件与纯表现事件分开。火花、相机震动可丢；任务完成和归位裁定必须进入持久事务。

### 5.3 快照

Presentation 获取只读快照和事件流。快照包含当前可见/相关实体的表现状态；不暴露可写组件集合。跨帧插值只作用位置、姿态和连续视觉量，不插值规则状态。

## 6. 能力模型

能力由可复用节点编译成执行计划：

```text
Trigger → Cost → TargetQuery → Movement → HitWindow
        → Effects → Evidence/Seal Hook → FeedbackTags → Recovery
```

节点类型数量受控；新节点必须服务多个内容或解决无法组合的问题。能力定义包含：输入标签、前置、资源、时间轴、取消窗、目标过滤、命中形状、效果、工灯钩子、AI 评分、反馈标签和测试种子。

动画事件不能决定伤害真相。Runtime 时间轴决定窗口，Presentation 对齐动画并报告可观测偏差。

## 7. 效果与数值

效果采用显式 `EffectDefinition` + `EffectInstance`：来源、目标、持续 Tick、叠加规则、标签、修改器和触发器。修改器按固定顺序：基础值 → 加法 → 乘法 → 上下限 → 最终覆盖。禁止内容作者依赖 JSON 加载顺序。

持续伤害、温度、湿润、失衡、固色等共享效果框架，但领域语义用标签/组件区分。每 Tick 大量更新的效果使用批处理；低频复杂效果可用领域对象。

## 8. 护牒兵击实现边界

战斗权威由 `CombatSystem`、`AbilitySystem`、`HitResolutionSystem`、`PoiseSystem` 和 `EffectSystem` 组合。

- 输入缓冲在 Runtime，以 Tick 表达。
- 碰撞查询通过 `IPhysicsQuery` 抽象请求 Godot 物理世界；结果在同一 Tick 确定化排序。
- 命中判定保存 attacker/ability/hitIndex，防止同一窗口重复命中。
- 精确格挡是两个时间窗与方向/标签的规则交集，不由动画回调直接触发。
- 群战威胁由 AI Director 控制进攻令牌，不靠每个敌人互相猜。

## 9. 工灯定印实现边界

`EvidenceSystem` 管理可观察工痕、观察条件、已验证证据和错误假设；`SequenceSystem` 管理环境工序图；`SealSystem` 验证证据和资源后，对图施加有期限、可回滚的规则变换；`ResolutionSystem` 在归位窗口提交永久地区结果。

工印只通过受限操作修改世界，例如启用/禁用边、改变流向、调整温度档、转移负载、恢复文本片段。内容不能执行任意脚本。

## 10. Boss 阶段图

Boss 使用有向阶段图：节点定义招式集、AI 权重、环境状态、证据和退出条件；边定义优先级、可逆性、过渡保护和存档策略。生命阈值只是允许的条件之一。

地区 Boss 在阶段开始前保存轻量重试快照；重试恢复权威状态、场景机关、同伴和消耗规则。过场观看状态独立保存，重复重试可跳过。

Boss 专属机制先尝试组合通用能力、环境节点、证据和阶段条件。确需新 Runtime 扩展时，必须写可复用接口与第二个测试用例，禁止 `if bossId == ...`。

## 11. NPC 日程与关系

日程由高层活动块和地点锚点构成，不存逐帧路线。流式外 NPC 以抽象状态推进；进入活动 Cell 时生成可达的具体路径。任务覆盖以有优先级的临时层叠加，结束后回到基础日程。

关系变化是领域事务：来源、维度、增量/设置、理由 key、公开性和最大影响。剧情判断读取具名条件，如 `TrustAtLeast(2) && Disagreement("water_priority")`，不散落魔法数字。

## 12. 任务与世界状态

任务图节点必须幂等；事件重复投递不能重复奖励。长时等待使用世界时钟条件，不用真实线程定时器。任务状态与场景表现分离，目标 NPC 缺席时由任务提供代理位置或替代入口。

世界状态分三层：

- `GlobalState`：跨地区记录原则、主线阶段、解锁。
- `RegionState`：设施、声誉、经济和地区归位。
- `InstanceState`：门、宝箱、机关、一次性遭遇。

状态键来自注册契约，不能由脚本拼字符串。

## 13. 世界流式

`WorldStreamer` 根据玩家、相机、任务和预加载门管理 Cell。Runtime 实体可处于 Active、Abstract、Dormant、Unloaded 四态。持久实体离开 Cell 后压缩为抽象状态；普通临时敌人按生成规则和清理策略处理。

Cell 卸载前生成持久差异，提交到内存世界状态；存档只在安全点或事务边界写盘。Presentation 资源释放不等于实体死亡。

## 14. 测试切面

- 纯系统测试：固定 Tick、命令序列、预期事件/快照。
- 属性测试：ID、效果叠加、任务幂等、迁移和随机种子。
- 战斗回放：同一命令流在不同渲染帧率得到相同校验和。
- Boss 沙盒：阶段可达性、失败重试、证据不足/完整归位。
- NPC 模拟：30 个游戏日无死锁，任务覆盖结束后恢复日程。
- 流式测试：反复进出 Cell 后实体、内存和存档状态稳定。

架构原因见 [`adr/ADR-0002-混合组件与分层核心.md`](adr/ADR-0002-混合组件与分层核心.md)。
