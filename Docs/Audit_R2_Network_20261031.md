# ArcaneVanguard 第二轮专项技术审计

## 单机战斗系统 → 多人合作 PvE 网络架构改造评估

- 审计日期：2026-10-31
- 审计对象：`D:/UEProject/ArcaneVanguard`（UE 5.4）
- 审计方式：**只读**（未修改任何源码 / 蓝图 / 配置 / 资产）
- 与第一轮的关系：第一轮给的是全景 + 风险分级；本轮**深入代码调用链**，回答「最少改什么才能跑通双客户端战斗」
- 审计范围限制：编辑器未启动，**蓝图 / BT / AnimBP 内部配置无法读取**，相关结论标注「待人工确认」

---

## 1. 审计摘要

### 1.1 当前工程真实状态

**4,190 行 C++ / 28 文件 / 单模块**。战斗闭环在单机下**确实跑通**（玩家普攻 → 敌人掉血 → 受击硬直 → 敌人反击 → 玩家 ragdoll 死亡 → 重生），不是"看起来完成"。

### 1.2 已有功能（有代码证据）

| 系统 | 关键证据 |
|---|---|
| 三段连招 | `OnAttackPressed`(`:672`) / `ArmNextSection`(`:699`) / `PlayComboSection`(`:733`) / `Montage_SetNextSection` 段尾过渡 |
| 攻击窗口 | `UAnimNotifyState_AVAttackWindow::NotifyBegin` → `SetAttackWindowOpen(true)` + `PerformAttackHitCheck()` |
| 命中判定 | `SweepMultiByObjectType(Enemy)` + `TSet<TWeakObjectPtr<AActor>>` 段内去重 |
| 伤害 | `UArcaneVanguardCombatStatics::ApplyDamage`（唯一入口，SetByCaller） |
| GAS | ASC 在 Character，`PossessedBy` 里 `InitAbilityActorInfo(this, this)` + `GiveAbility(DefaultAbilities)` |
| 敌人 AI | BT + Blackboard，`BTService_AVUpdateTarget` 写 `TargetActor`，`BTTask_AVAttack` 出招 |
| 受击硬直 | `HandleHitReact` → 停蒙太奇 / 停移动 / 写 Blackboard `IsStaggered` → BT Selector 分支 |
| 武器 | `SpawnActor` + `AttachToComponent(StickSocket)` + `EnablePhysicsDrop` |

### 1.3 最重要的技术风险（本轮新发现）

| # | 风险 | 证据 | 为什么致命 |
|---|---|---|---|
| **R1** | **攻击动画完全没有复制** | `Character.cpp:794` `PlayAnimMontage(ComboMontage, ...)` 直接调用，无 Multicast、无 GA | 联机时**队友看不到你在挥刀**，只能看到敌人突然掉血 |
| **R2** | **攻击判定在 NotifyBegin 里同步执行** | `AVCombatNotifyStates.cpp:73` `Character->PerformAttackHitCheck()` | 动画在哪个客户端播就在哪算伤害。**而且蒙太奇不复制，服务器上可能根本不播这段动画 → 服务器不会触发 Notify → 判定永远不执行** |
| **R3** | **敌人是关卡里摆的，不是 GameMode Spawn** | `GameMode.cpp` 全文无 `SpawnActor`；`StartPlay` 只打日志 | 联机时**每个客户端各自加载关卡里的敌人** → 一个客户端打死敌人，另一个客户端的敌人还活着 |
| **R4** | **AI 目标选择写死 `GetPlayerPawn(0)`** | `BTService_AVUpdateTarget.cpp:39` `UGameplayStatics::GetPlayerPawn(AIOwner, 0)` | 多玩家时**只追第一个玩家**，且「第一个」在服务器和客户端上指的不是同一个人 |
| **R5** | **武器各自 Spawn，无 Owner 网络关联** | `Character.cpp:532` 设了 `SpawnParams.Owner` 但无 `SetReplicates` | 每个客户端各生成一把法杖 → 队友看到你手上有两把，或你的武器飘在别人手上 |

**R2 是本轮最重要的发现**：它说明**「给 Actor 加 bReplicates」根本解决不了问题**。因为攻击判定的触发源（AnimNotify）本身依赖动画播放，而动画播放没有复制——服务器可能压根不播这段蒙太奇。

### 1.4 是否适合在现有工程上继续改造

**适合，而且应该继续改。** 理由：

- 连招状态机、GAS 分层、BT 结构、碰撞通道**全部可以保留**（见 §9 分级）
- 真正要动的只有 5 个位置：蒙太奇播放方式、Notify 的 Authority 判断、敌人生成点、AI 目标选择、武器复制
- **不需要重写任何系统**

---

## 2. 普通攻击连招专项审计

### 2.1 完整调用链（A1 + A2）

```
【输入层】
IA_Attack (EnhancedInput, ETriggerEvent::Started)
  → AArcaneVanguardCharacter::SetupPlayerInputComponent(`:1084`) 里的 BindAction
  → AArcaneVanguardCharacter::OnAttackPressed()(`:672`)

【状态判定】OnAttackPressed
  ├─ bRequireCombatForAttack && !bInCombat  → 直接 return
  ├─ IsAttacking() == false                 → PlayComboSection(0)   【起手】
  └─ IsAttacking() == true
       ├─ bComboWindowOpen == true  → ArmNextSection()              【窗口内：挂链】
       └─ bComboWindowOpen == false → bAttackBuffered = true         【窗口外：缓存】

【挂链】ArmNextSection(`:699`)
  → Montage_GetCurrentSection / GetSectionIndex
  → 已是最后一段 → return
  → ArmedNextSectionIndex == 下一段 → return（防重复挂）
  → AnimInstance->Montage_SetNextSection(Current, Next, ComboMontage)
  → ArmedNextSectionIndex = 下一段
  ※ 只改播放实例的 NextSections，不写回资产；段尾自然过渡

【播放】PlayComboSection(`:733`)
  → 校验 AnimInstance / ComboMontage / 段索引 / Section 名存在
  → EffectivePlayRate = ComboPlayRate * AttackSpeedMultiplier
  → 若同一蒙太奇已在播 → Montage_JumpToSection + Montage_SetPlayRate
  → 否则 → PlayAnimMontage(ComboMontage, EffectivePlayRate, SectionName)
      → Montage_SetEndDelegate(HandleComboMontageEnded)
      → 攻击期间减速：MaxWalkSpeed = Cached * AttackMoveSpeedScale

【动画事件 → 攻击判定】
UAnimNotifyState_AVComboWindow::NotifyBegin  → SetComboWindowOpen(true)
                                             → 消费 bAttackBuffered（若有）→ ArmNextSection
UAnimNotifyState_AVComboWindow::NotifyEnd    → SetComboWindowOpen(false) + 清缓冲
UAnimNotifyState_AVAttackWindow::NotifyBegin → SetAttackWindowOpen(true)
                                             → PerformAttackHitCheck(DamageOverride)  ← 判定只在这里发生一次
UAnimNotifyState_AVAttackWindow::NotifyEnd   → SetAttackWindowOpen(false)

【状态纠偏】Tick(`:838`)
  → 每帧读 Montage_GetCurrentSection → ComboIndex = 段索引 + 1
  → ArmedNextSectionIndex 已达成 → 清 bAttackBuffered / bComboWindowOpen
  ※ 早退条件清晰，开销可忽略

【结束】HandleComboMontageEnded(`:1053`)
  → 清 ComboIndex / 窗口 / 缓冲 / ArmedNextSectionIndex
  → 恢复 MaxWalkSpeed
```

### 2.2 攻击判定的实际执行位置（A3）

```cpp
// Animation/AVCombatNotifyStates.cpp:66-87
void UAnimNotifyState_AVAttackWindow::NotifyBegin(...)
{
    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    if (!Owner) return;

    if (AArcaneVanguardCharacter* Character = Cast<AArcaneVanguardCharacter>(Owner))
    {
        Character->SetAttackWindowOpen(true);
        Character->PerformAttackHitCheck(DamageOverride);   // ← 同步、立即、无 Authority 判断
        return;
    }
    if (AArcaneVanguardEnemy* Enemy = Cast<AArcaneVanguardEnemy>(Owner))
    {
        Enemy->PerformAttackHitCheck();
    }
}
```

`PerformAttackHitCheck`(`:886`)：
```cpp
FCollisionObjectQueryParams ObjectParams;
ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);      // Enemy 通道
World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams,
    FCollisionShape::MakeSphere(AttackRadius), QueryParams);
// ... 段内 TSet 去重 ...
const float Applied = UArcaneVanguardCombatStatics::ApplyDamage(HitActor, FinalDamage, this, DamageEffectClass);
```

**去重机制**：`HitActorsThisAttack`（`TSet<TWeakObjectPtr<AActor>>`），在 `SetAttackWindowOpen(true)` 时 `Reset()`，即**每段独立**。

### 2.3 单机能跑、联机会怎样

| 环节 | 单机 | 联机 |
|---|---|---|
| 输入 | 本地 PC 收到 | ✅ 本地 PC 收到（正确） |
| `PlayComboSection` | 本地播蒙太奇 | ❌ **只在本地播**，服务器/其他客户端不播 |
| NotifyBegin | 本地触发 | ❌ 服务器不播动画 → **不触发** |
| `PerformAttackHitCheck` | 本地执行 | ❌ 若只在客户端执行，服务器不知道；若改成只在服务器执行，**服务器没动画就不触发** |
| `ApplyDamage` | 本地施加 | ❌ 客户端改不了服务器属性（GAS 会拒绝） |

**结论：当前连招在联机下会完全失效**，不是"有点不同步"，是"打不出伤害"。

### 2.4 联机改造方案（A4）

| 现有文件 | 类 / 函数 | 当前职责 | 当前问题 | 建议改造 | 优先级 |
|---|---|---|---|---|---|
| `ArcaneVanguardCharacter.cpp` | `OnAttackPressed` | 输入入口 | 无 Authority 判断 | **保留**：输入本来就该只在本地 PC 处理；改的是下游 | — |
| 同上 | `PlayComboSection` | 播蒙太奇 | `PlayAnimMontage` 不复制 | **改成 Multicast**：`ServerPlayComboSection(SectionIndex)` → 服务器 `NetMulticast_PlayComboSection` → 各端 `PlayAnimMontage`。或整体迁进 GA | **P0** |
| 同上 | `ArmNextSection` / `Tick` / `HandleComboMontageEnded` | 连招状态机 | 纯本地状态 | **保留，不复制**。连招是「输入→动画段」的映射，服务器不需要知道你在第几段 | — |
| `AVCombatNotifyStates.cpp` | `AVAttackWindow::NotifyBegin` | 触发判定 | 无 Authority 判断 | 加 `if (!Character->HasAuthority()) return;`（伤害只在服务器算）。**表现层（特效/音效）走 GameplayCue 广播** | **P0** |
| 同上 | `AVComboWindow::NotifyBegin/End` | 开连招窗口 | 纯本地 | **保留**（窗口是输入映射，不需要复制） | — |
| 同上 | `AVAttackWindow::NotifyEnd` | 关窗口 | 纯本地 | **保留** | — |
| `CombatStatics.cpp` | `ApplyDamage` | 施加 GE | 无 Authority 检查 | 加 `if (!SourceASC->IsOwnerActorAuthoritative()) return 0;` | **P0** |

**必须区分**：

- **可以保留**：`ArmNextSection` / `SetComboWindowOpen` / `Tick` 纠偏 / `HandleComboMontageEnded` / 段内去重
- **必须重做**：蒙太奇播放方式（本地 → Multicast）、判定触发点的 Authority 判断

**关于「攻击窗口」的一个重要澄清**：当前实现里 `bAttackWindowOpen` **只是状态标记**，判定只在 `NotifyBegin` 跑一次。所以不存在"窗口持续期间反复判定"的问题——但这也意味着**如果 NotifyBegin 那一刻网络延迟导致动画没同步，判定就丢了**。改造后必须由服务器自己的动画时间驱动。

---

## 3. GAS 专项审计

### 3.1 ASC 初始化（B1）

| 项 | 实现 | 证据 |
|---|---|---|
| 创建位置 | `AArcaneVanguardCharacter` 构造函数 `CreateDefaultSubobject<UAbilitySystemComponent>` | `Character.cpp` |
| 所属 Actor | **Character**（不是 PlayerState） | 同上 |
| `InitAbilityActorInfo` | `PossessedBy`(`:186`)：`InitAbilityActorInfo(this, this)` | 同上 |
| 敌人 | `BeginPlay`(`:117`) + `PossessedBy`(`:146`) 各一次（静态靶子无 Controller 时靠 BeginPlay） | `Enemy.cpp` |
| 重复初始化 | 玩家：`PossessedBy` + `OnRep_PlayerState`（后者是空重写，无副作用）；敌人：两处都会调，但 `InitAbilityActorInfo` 幂等 | 已确认无害 |
| 开发者已知改造点 | `PossessedBy` 注释：**「将来做联机迁到 PlayerState 时，改成 `InitAbilityActorInfo(GetPlayerState(), this)`」** | `Character.cpp:183` |

**生命周期判断**：
- 当前 ASC 挂在 Character 上，**Character 重生/换 Pawn 时 ASC 会重建** → 技能授予、GE 状态全丢
- 单机靠 `Respawn()` 里重新 `PossessedBy` 恢复
- **联机时这是 P1 问题**：其他客户端持有的 ASC 引用会失效

**但注意**：不要一刀切迁 PlayerState。**PvE 合作且玩家不换角色**的情况下，Character 挂 ASC 也可以工作（UE 官方 ShooterGame 早期就是这么做的）。**建议：第一阶段先不迁，等做完双客户端验证再评估**。

### 3.2 AttributeSet（B2）

| 项 | 状态 | 证据 |
|---|---|---|
| 属性 | `Health` / `MaxHealth` / `Mana` / `MaxMana` / `AttackSpeedMultiplier` | `AttributeSet.h` |
| 初始化 | 构造函数 `InitHealth(100)` / `InitMaxHealth(100)` / `InitMana(100)` / `InitMaxMana(100)` / `InitAttackSpeedMultiplier(1.0)` | `AttributeSet.cpp:5-16` |
| **Replicated** | **❌ 完全没有** | 无 `GetLifetimeReplicatedProps`、无 `DOREPLIFETIME` |
| **OnRep** | **❌ 完全没有** | — |
| 变化监听 | `GetGameplayAttributeValueChangeDelegate(HealthAttribute).AddUObject(this, &OnHealthChanged)` | `Character.cpp` `PossessedBy` 内 |

**联机后果**：客户端**看不到任何血量变化**。HUD 血条（如果做了）永远是初始值。

**改造要点**（P1）：
```cpp
// UArcaneVanguardAttributeSet
virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
// DOREPLIFETIME_CONDITION_NOTIFY(UArcaneVanguardAttributeSet, Health, COND_None, REPNOTIFY_Always);
// 每个属性配一个 UFUNCTION() OnRep_Health(const FGameplayAttributeData& Old);
```
**注意**：GAS 的 Attribute 复制**不会**自动触发 `OnHealthChanged`（那是 `FOnGameplayAttributeValueChange`，只在本地属性被改时触发）。客户端需要 `OnRep` → 手动调 `GAMEPLAYATTRIBUTE_REPNOTIFY`。

### 3.3 GameplayAbility（B3）

| Ability | 用途 | Instancing | NetExecutionPolicy | 执行端 | 适合联机 | 建议 |
|---|---|---|---|---|---|---|
| `UArcaneVanguardGameplayAbility`（基类，Abstract） | 通用流程 | `InstancedPerActor` | **`LocalPredicted`** | 本地预测 | ⚠️ 需评估 | Buff 类技能建议 `ServerInitiated` 或保留 LocalPredicted + 服务器校正 |
| `GA_ArcaneBlast`（蓝图子类） | Q 增益（回血 + 攻速） | 继承基类 | 继承基类 | 待确认 | ⚠️ 待验证 | 同上；**其 `AbilityTags` / `SelfEffectClasses` 配置需人工打开蓝图确认** |

**基类的关键实现**（`ArcaneVanguardGameplayAbility.cpp`）：
```cpp
if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
{
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);   // ✅ 失败必 EndAbility
    return;
}
// 藏武器 → ApplySelfEffects → PlayMontageAndWait / 立即 EndAbility
```

**`ApplySelfEffects` 用的是 `ApplyGameplayEffectSpecToSelf`** —— 这是**本地应用**，联机时如果客户端先跑，服务器会拒绝（非权威端不能改属性）。`LocalPredicted` 会先本地应用、再等服务器确认，**可能导致"客户端血回满了但服务器没回"的闪回**。

**改造建议（分类型，不要一刀切）**：

| 技能类型 | 建议策略 | 理由 |
|---|---|---|
| **普通近战攻击** | **不做成 GA**，保持现有连招状态机 + Multicast 播动画 + 服务器判定 | 连招是输入映射，做成 GA 反而增加复杂度；UE 官方 Lyra 的近战也是 GA，但那是为了统一 CD/消耗 |
| **瞬发魔法（Q 增益）** | `ServerInitiated` 或 `LocalPredicted` + 服务器校正 | 增益类不需要客户端预测（晚 100ms 无所谓） |
| **需要瞄准的技能** | `LocalPredicted` + 服务器验证方向 | 需要即时反馈，但方向要服务器校验 |
| **持续施法** | `ServerInitiated` | 时长由服务器算 |
| **受击/硬直** | **不是 GA**，用 GE + GameplayCue | 硬直是状态不是技能 |

### 3.4 GameplayEffect 与 GameplayCue（B4）

| 项 | 状态 |
|---|---|
| 伤害 GE | `UGE_ArcaneDamage`（Instant + SetByCaller `Data.Damage`） |
| Buff GE | `UGE_Q_Regen`（5s / Period 1s）、`UGE_Q_Haste`（5s） |
| 冷却 GE | `GE_Cooldown_Q`（资产存在，配置待确认） |
| 消耗 GE | `GE_Cost_Mana`（资产存在，配置待确认） |
| **GameplayCue** | **❌ 完全没有**（无 `UGameplayCueNotify`、无 `GameplayCue.*` Tag） |

**后果**：所有特效/音效**没有挂点**。联机时无法控制"谁播、播给谁"。受击闪光、命中火花这类表现，现在只能靠 Notify 里的本地代码，而 Notify 又不复制。

**改造方向（P1）**：
- 受击表现 → `GameplayCue.Hit`（GE 带 `GameplayCueTag`，`Executed` 时自动在所有客户端播）
- 攻击特效 → 由 Multicast 的攻击动画事件驱动，或 `GameplayCue.Attack`

### 3.5 GAS 改造清单（B5）

| 顺序 | 任务 | 依赖 |
|---|---|---|
| 1 | AttributeSet 加复制 + OnRep | 无 |
| 2 | `ApplyDamage` 加 `IsOwnerActorAuthoritative` 检查 | 无 |
| 3 | 评估 `NetExecutionPolicy`（Q 技能改 `ServerInitiated`） | 1 |
| 4 | 加 `GameplayCue.Hit` + 受击 GE 挂 Cue | 1、2 |
| 5 | 补 CD / Cost GE 的配置并验证 `CommitAbility` 拒绝逻辑 | 3 |

---

## 4. 敌人 AI 与伤害死亡系统审计

### 4.1 AI 架构与执行流程（C1）

```
【生成】关卡里手工摆放 BP_AV_Enemy_Golem
  ※ GameMode::StartPlay 只打日志，无 SpawnActor —— 敌人是关卡实例

【初始化】Enemy::BeginPlay(`:105`)
  → InitAbilityActorInfo(this, this)
  → AddAttributeSetSubobject
  → ApplyStartupEffects（MaxHealthOverride）
  → 注册 Health 变化委托

【接管】AutoPossessAI / SpawnDefaultController（蓝图配置，待确认）
  → EnemyAIController::OnPossess(`:23`)
      → RunBehaviorTree(BT_Enemy)   ← 无 HasAuthority 判断

【BT 循环】BT_Enemy
  Selector
   ├─ [受击分支] Decorator: IsStaggered Is True, Aborts = Lower Priority
   │     └─ Wait 0.6s
   └─ [战斗 Sequence]
        ├─ Service: BTService_AVUpdateTarget（TickInterval 0.08~0.12）
        │     → UGameplayStatics::GetPlayerPawn(AIOwner, 0)   ← 写死索引 0
        │     → 算 EdgeDistance（扣除双方胶囊半径）
        │     → 超 LoseTargetDistance(2000) → ClearValue(TargetActor)
        │     → 否则 SetValueAsObject(TargetActor, Target)
        ├─ MoveTo(TargetActor)
        └─ [Attack 分支] Cooldown Decorator
              └─ BTTask_AVAttack → Enemy->BeginAttack() → PlayAnimMontage(AttackMontage)
                    → 动画里的 AVAttackWindow Notify → Enemy->PerformAttackHitCheck()
```

### 4.2 伤害与受击（C2）

| 项 | 实现 | 证据 |
|---|---|---|
| 血量存放 | `UArcaneVanguardAttributeSet::Health`（在 Enemy 上） | `AttributeSet.h` |
| 伤害入口 | `UArcaneVanguardCombatStatics::ApplyDamage` | `CombatStatics.cpp` |
| 通过 GAS 接收 | ✅ `ApplyGameplayEffectSpecToTarget` | 同上 |
| 受击触发 | `OnHealthChanged`(`:152`)：`NewValue < OldValue` → `HandleHitReact` | `Enemy.cpp` |
| 硬直管理 | **不是 GameplayTag**，是 `bIsStaggered` 成员 + Blackboard `IsStaggered` | `Enemy.cpp:236-410` |
| 受击动画 | `HitReactMontages` 数组随机播 | 同上 |
| 霸体/无敌 | **❌ 无** | 无相关字段 |
| 重复伤害风险 | `HitActorsThisAttack` 段内去重（玩家侧）；敌人侧靠 `BeginAttack` 里 `HitActorsThisAttack.Reset()` | 已确认 |

### 4.3 死亡处理（C3）

```cpp
// Enemy.cpp:169 HandleDeath()
bIsDead = true;
ClearTimer(StaggerTimerHandle);     // 防尸体 ragdoll 后 EndStagger 还在跑
bIsStaggered = false;
// → 取消技能 / 停 AI / ragdoll / 延迟销毁（DestroyDelayAfterDeath = 6s）
```

| 检查项 | 状态 |
|---|---|
| 死亡条件 | `OnHealthChanged` 里 `NewValue <= 0` |
| 死亡动画 | **不用动画，用 ragdoll**（`bBlendPhysics`） |
| 碰撞关闭 | `bDisableCapsuleCollisionOnDeath`（默认 true） |
| AI 停止 | `Brain->StopLogic` 或类似（待确认具体调用） |
| Actor 销毁 | `SetLifeSpan(6s)` |
| 掉落物 | **❌ 无** |
| 重复触发 | `if (bIsDead) return;` 守卫 ✅ |

### 4.4 多人联机改造（C4）

| 问题 | 现状 | 改造 |
|---|---|---|
| 敌人是否应由服务器生成 | ❌ 关卡摆放 | **必须改成 GameMode/GameState 在服务器 Spawn**，再复制到客户端。关卡实例只能作为"单机调试用" |
| AI 是否应由服务器执行 | ❌ `OnPossess` 无判断 | `if (!HasAuthority()) return;`（UE 的 AIController 默认只在服务器创建，但关卡实例的 `AutoPossessAI` 可能两端都跑，**需实测确认**） |
| 目标选择如何统一 | ❌ `GetPlayerPawn(0)` | 遍历 `PlayerArray`，选最近的**存活**玩家；加仇恨表（谁打我选谁） |
| 敌人移动如何同步 | ❌ 无复制 | `bReplicates` + `bReplicateMovement`（CharacterMovementComponent 自带） |
| 敌人攻击如何同步 | ❌ `PlayAnimMontage` 本地 | Multicast 播动画；判定在服务器 |
| 伤害如何统一结算 | ❌ 客户端可调 `ApplyDamage` | 加 Authority 检查 |
| 死亡状态如何复制 | ❌ | `bIsDead` 改 `Replicated` + `OnRep_Dead` → 各端播 ragdoll |
| 客户端如何播受击/死亡 | 靠本地 Notify | 改 `GameplayCue` 或 Multicast |
| 掉落物 | ❌ 无 | 服务器 Spawn + 复制 |

**特别注意**：`BTService_AVUpdateTarget` 里的 `GetPlayerPawn(AIOwner, 0)` 是**双端各自执行**的——服务器上返回主机玩家，客户端上返回自己。如果 AI 只在服务器跑，这个函数就只在服务器执行，`PlayerIndex 0` 的含义变成"服务器本地的第一个玩家"——**多客户端时永远只追房主**。这是 R4。

---

## 5. 武器系统审计

| # | 问题 | 证据 |
|---|---|---|
| 1 | 武器 Actor 在哪创建 | `AArcaneVanguardCharacter::SpawnAndAttachWeapon`(`:494`) |
| 2 | 用什么 Spawn | `World->SpawnActor<AArcaneVanguardWeapon>(WeaponActorClass, ...)`(`:537`) |
| 3 | 是否设置复制 | **❌ 无 `SetReplicates`** |
| 4 | 由谁生成 | **角色自己**（`SpawnAndAttachWeapon` 在 `BeginPlay` / `Respawn` 里调） |
| 5 | 如何挂载 | `AttachToComponent(OwnerMesh, KeepWorldTransform, "StickSocket")` |
| 6 | 挂载关系是否在客户端正确显示 | **❌ 每个客户端各自 Spawn 一把，各自 Attach** —— 视觉上"看起来对"，但队友看不到你的武器 |
| 7 | 掉落由谁执行 | `DropWeaponOnDeath`(`:465`) → `EnablePhysicsDrop` |
| 8 | 掉落物是否有复制 | **❌ 无** |
| 9 | 拾取逻辑 | **❌ 不存在**（`DetachAndDestroyWeapon` 是销毁，不是拾取） |
| 10 | 客户端能否改武器归属 | **❌ 能**（`SpawnedWeapon` 是普通 UPROPERTY，无 Authority 守卫） |

**概念区分（重要）**：

| 概念 | 当前实现 | 联机建议 |
|---|---|---|
| **武器数据**（攻击力、模型、握持偏移） | 混在 `AArcaneVanguardWeapon` 的 UPROPERTY 里 | **保持**，数据不需要复制 |
| **武器 Actor**（场景实体） | 每个客户端各 Spawn 一把 | **两种方案二选一**：<br>A. 服务器 Spawn + 复制（一把武器，多端看到）<br>B. 每端本地 Spawn 纯表现版（不进复制列表，`bOnlyRelevantToOwner=false`） |
| **武器视觉表现** | Mesh + Attach | 跟随 Actor 复制 |
| **挂载关系** | `AttachToComponent` | 复制后自动跟随（Attach 是复制属性） |
| **掉落物** | 无（脱手后就是物理 Actor） | 服务器 Spawn + 复制 |
| **拾取与归属** | 无 | 服务器决定归属，`ServerRPC_RequestPickup` |

**对当前规模的建议**：**方案 B（每端本地 Spawn 表现版）更省事**。理由：
- 武器不需要被其他玩家交互（没有拾取系统）
- 武器不参与战斗判定（判定用球扫，武器已排除）
- 本地 Spawn 意味着**零网络开销**，且天然避免"两把武器"问题

**唯一要改的**：`DropWeaponOnDeath` 时，如果要做"别人能看到地上的武器"，才需要服务器 Spawn 一个复制版。

---

## 6. 网络基础设施审计（E）

| 网络机制 | 是否存在 | 所在文件 | 实际用途 | 是否正确 | 后续处理 |
|---|---|---|---|---|---|
| `bReplicates` | **❌ 0 处** | — | — | — | 三个 Actor 需加 |
| `bReplicateMovement` | **❌ 0 处** | — | — | — | Character/Enemy 需加 |
| `GetLifetimeReplicatedProps` | **❌ 0 处** | — | — | — | AttributeSet 需加 |
| `DOREPLIFETIME*` | **❌ 0 处** | — | — | — | 同上 |
| Server RPC | **❌ 0 处** | — | — | — | 攻击/拾取需加 |
| Client RPC | **❌ 0 处** | — | — | — | 一般不需要 |
| NetMulticast | **❌ 0 处** | — | — | — | 攻击动画/受击表现需加 |
| `HasAuthority` | **❌ 0 处** | — | — | — | 判定/AI/伤害需加 |
| `IsLocallyControlled` | **❌ 0 处** | — | — | — | 输入/相机需加（目前靠 PC 天然本地） |
| `GetNetMode` | **❌ 0 处** | — | — | — | 可选 |
| `SetOwner` | **❌ 0 处** | — | — | — | 武器需要 |
| RepNotify | **❌ 0 处** | — | — | — | 死亡状态需要 |
| GameMode 基类 | `AGameModeBase` | `ArcaneVanguardGameMode.h:10` | 只重写 `StartPlay` 打日志 | ❌ 单机 | 改 `AGameMode` 或补 `PostLogin` |
| GameState | **❌ 无自定义类** | — | — | — | 需要（副本进度/剩余敌人） |
| PlayerController | `AArcaneVanguardPlayerController` | `Player/ArcaneVanguardPlayerController.cpp` | IMC 注册/移除（幂等 + 成对） | ✅ **正确** | 保持 |
| PlayerState | **❌ 未使用** | `OnRep_PlayerState` 空重写 | — | — | 第一阶段可不迁 |
| CharacterMovement | 默认 `UCharacterMovementComponent` | `ACharacter` 自带 | 移动 | ✅ **正确**（自带网络预测） | 只需 `bReplicateMovement = true` |

**关于 `DefaultEngine.ini`**：
- `bAllowNetworkConnection=True`（引擎默认值，**不代表项目支持联机**）
- 无 `[/Script/Engine.GameNetworkManager]` 配置
- 无 `NetServerMaxTickRate` 等设置

**GameInstance**：`Source/` 下**无自定义 GameInstance**（用引擎默认）。**这是好事**——没有把网络职责塞进 GameInstance。

---

## 7. 蓝图与 C++ 边界审计（F）

### 7.1 已在 C++ 的部分（可读）

| 逻辑 | 位置 |
|---|---|
| 连招状态机 | `ArcaneVanguardCharacter` 全部 |
| 攻击判定 + 伤害 | `PerformAttackHitCheck` + `ApplyDamage` |
| 受击硬直 | `AArcaneVanguardEnemy::HandleHitReact` |
| 死亡/重生 | `HandleDeath` / `Respawn` |
| 武器生成/挂载/掉落 | `AArcaneVanguardCharacter` 武器区 + `AArcaneVanguardWeapon` |
| GAS 初始化 | `PossessedBy` / `BeginPlay` |
| GA 基类流程 | `UArcaneVanguardGameplayAbility` |
| AI 启动 | `EnemyAIController::OnPossess` |
| BT Service/Task | `BTService_AVUpdateTarget` / `BTTask_AVAttack` |

### 7.2 在蓝图的部分（**无法读取，需人工确认**）

| 资产 | 需要确认的内容 | 为什么重要 |
|---|---|---|
| `BP_ThirdPersonCharacter` | `DefaultAbilities` 是否填了 `GA_ArcaneBlast`；`ComboMontage` / `AttackRadius` / `AttackRange` / `AttackDamage` 的实际值；`WeaponActorClass` 指向哪把武器 | 决定 Q 技能能否激活、攻击数值 |
| `BP_AV_GameMode` | `DefaultPawnClass` / `PlayerControllerClass` 是否配了（C++ 里刻意留空） | 配错会退回默认 Mannequin |
| `BP_AV_Enemy_Golem` | `HitReactMontages` 填了几个；`AttackMontage` 是哪个；`AutoPossessAI` 设置；`AIControllerClass` | 决定受击/攻击表现 |
| `BP_AV_EnemyAIController` | `BehaviorTreeAsset` 指向 `BT_Enemy`？ | 不配则敌人不行动 |
| `BT_Enemy` | Selector 结构、受击分支的 Decorator（`Is True` + `Aborts = Lower Priority`）、战斗分支的 Cooldown 参数 | 决定受击能否打断攻击 |
| `BB_Enemy` | 是否有 `IsStaggered` 键 | 没有则硬直退回 `PauseLogic` 降级路径 |
| `ABP_Enemy_Stone` / `AnimBP_Wizard_ThirdPerson` | 是否有 `DefaultSlot` 节点 | 没有则 `PlayAnimMontage` 播了不显示 |
| `GA_ArcaneBlast` | `AbilityTags` 是否有 `Ability.Skill.Q`；`SelfEffectClasses` 是否填两个 GE；CD/Cost GE 是否挂上 | 决定 Q 技能是否真的能跑 |
| `GE_Cooldown_Q` / `GE_Cost_Mana` | Duration / Modifier 配置 | 决定 CD 与耗蓝是否生效 |
| `AM_NormalAttack` | Section 名是否为 `Combat_01/02/03`；`AVComboWindow` / `AVAttackWindow` Notify 的位置 | 决定连招窗口与判定时机 |
| `IMC_Default` | 5 个 IA 是否都映射了按键 | 决定输入是否完整 |

**当前无法判断的**：蓝图中**是否存在攻击判定/伤害计算/状态管理**（理论上不应该有，但需要打开确认——如果蓝图的 AnimBP 里也挂了 Notify 逻辑，会和 C++ 的重复）。

---

## 8. 完整 Demo 缺口分析（G）

| 系统 | 当前情况 | 缺失内容 | 是否依赖网络架构 | 推荐开发顺序 |
|---|---|---|---|---|
| **玩家战斗** | ✅ 三段连招 + 技能框架 | 闪避、格挡、更多技能 | 是（判定要服务器） | 1（先网络化现有） |
| **敌人** | ✅ 一种石头人 + BT | 精英、Boss、多类型 | 是 | 2 |
| **关卡流程** | ❌ 只有一个 ThirdPersonMap | 副本入口、区域切换、触发器 | 是（关卡加载要服务器协调） | 3 |
| **任务系统** | ❌（插件在别的工程） | 接入 + 任务 UI | 否（可后做） | 5 |
| **交互系统** | ❌ | 拾取、开门、机关 | 部分（拾取要服务器） | 4 |
| **奖励/掉落** | ❌ | 掉落表、结算 | 是 | 4 |
| **背包/装备** | ❌ | 全部 | 部分 | 6 |
| **HUD** | ❌ 零 UMG | 血条、技能栏、CD、伤害数字 | 是（读复制属性） | 2（跟属性复制一起） |
| **菜单** | ❌ | 主菜单、暂停、设置 | 否 | 6 |
| **动画** | ⚠️ 基础可用 | 玩家受击、闪避、IK 手指、上下半身分层 | 否 | 5 |
| **特效** | ❌ 零 Niagara | 技能、命中、受击 | 是（GameplayCue） | 5 |
| **音效** | ❌ 零 | 全部 | 部分 | 5 |
| **存档** | ❌ | 全部 | 是（服务器权威） | 6 |
| **网络** | ❌ 零 | 全部 | — | **0（最优先）** |

**核心判断**：**网络架构是"地基"，必须在内容量起来之前完成**。现在只有 1 种敌人、1 个技能，是网络化成本最低的时候。

---

## 9. 代码复用等级（I）

| 文件 | 类 | 主要职责 | 复用等级 | 判断依据 | 需要做什么 |
|---|---|---|---|---|---|
| `AbilitySystem/ArcaneVanguardCombatStatics.cpp` | `UArcaneVanguardCombatStatics` | 伤害唯一入口 | **B** | 逻辑纯函数、无状态、SetByCaller 设计正确 | 加 `IsOwnerActorAuthoritative` 检查 |
| `AbilitySystem/ArcaneVanguardAttributeSet.cpp` | `UArcaneVanguardAttributeSet` | 属性 + 钳制 | **B** | 钳制逻辑完美、属性定义清晰 | 加 `GetLifetimeReplicatedProps` + `OnRep` |
| `AbilitySystem/ArcaneVanguardGameplayAbility.cpp` | `UArcaneVanguardGameplayAbility` | GA 基类流程 | **B** | 流程完整、`CommitAbility` 失败必 End | 评估 `NetExecutionPolicy` |
| `AbilitySystem/ArcaneVanguardGameplayEffects.cpp` | 三个 GE | 数据容器 | **A** | 纯配置，与网络无关 | 无 |
| `Animation/AVCombatNotifyStates.cpp` | 两个 NotifyState | 窗口 + 判定触发 | **C** | 判定逻辑要移到服务器、表现要拆出 | 加 Authority 判断 + 表现层拆到 Cue |
| `ArcaneVanguardCharacter.cpp` | `AArcaneVanguardCharacter` | 角色主体（1,219 行） | **C** | 连招状态机可留，蒙太奇播放/武器 Spawn/死亡重生需改 | 蒙太奇改 Multicast、武器改本地表现、死亡加 RPC |
| `ArcaneVanguardGameMode.cpp` | `AArcaneVanguardGameMode` | GameMode | **C** | 基类是 `AGameModeBase`，无登录流程 | 换基类或补 `PostLogin` |
| `Player/ArcaneVanguardPlayerController.cpp` | PC | IMC 注册 | **A** | 幂等 + 成对移除，标准做法 | 无 |
| `Enemy/ArcaneVanguardEnemy.cpp` | `AArcaneVanguardEnemy` | 敌人主体（478 行） | **B** | 受击/死亡逻辑清晰，硬直设计优秀 | 加 `bReplicates`、死亡状态复制 |
| `Enemy/ArcaneVanguardEnemyAIController.cpp` | AIController | 启动 BT | **B** | 逻辑简单 | `OnPossess` 加 `HasAuthority` |
| `Enemy/AI/BTService_AVUpdateTarget.cpp` | Service | 目标选择 | **C** | `GetPlayerPawn(0)` 写死索引 | 改遍历 `PlayerArray` + 仇恨表 |
| `Enemy/ArcaneVanguardBTTask_Attack.cpp` | Task | 出招 | **B** | 逻辑清晰、有 `IsDead` 守卫 | 无（判定在 Enemy 侧改） |
| `Weapon/ArcaneVanguardWeapon.cpp` | `AArcaneVanguardWeapon` | 武器 Actor | **B** | 碰撞/握持/掉落逻辑好 | 决定"服务器 Spawn"还是"本地表现版" |
| `ArcaneVanguard.h/.cpp`（模块入口） | — | 模块注册 | **A** | 空实现 | 无 |

**统计**：A 级 4 个、B 级 7 个、C 级 4 个、D 级 0 个、U 级 0 个。

**结论：没有需要"重写"的模块**，最差的是 C 级（局部重构）。

---

## 10. 最小多人联机改造任务表（H）

### H1. 双客户端基础

| 任务 | 修改文件 | 类/函数 | 前置依赖 | 验收方法 | 失败定位 |
|---|---|---|---|---|---|
| H1.1 Character 开复制 | `ArcaneVanguardCharacter.cpp` 构造函数 | `bReplicates = true` `bReplicateMovement = true` | 无 | PIE 设 2 客户端，B 客户端能看到 A 的角色 | B 端看不到 → 检查 `NetCullDistance` / `bAlwaysRelevant` |
| H1.2 GameMode 换基类 | `ArcaneVanguardGameMode.h` | `AGameModeBase` → `AGameMode` | 无 | 能正常登录，`PostLogin` 被调用 | 无 Pawn → 检查 `DefaultPawnClass` |
| H1.3 PlayerController 补登录 | `PlayerController.cpp` | 重写 `PostLogin` / `Logout` | H1.2 | 日志打出 `PostLogin: <PlayerName>` | — |
| H1.4 输入只在本端 | 已有（PC 天然本地） | — | — | B 端不能控制 A 的角色 | 若 A 动了 → 检查输入绑定是否走了 Pawn 而非 PC |
| H1.5 相机只在本端 | `ArcaneVanguardCharacter.cpp` | `BeginPlay` 里 `if (IsLocallyControlled())` 才设 SpringArm 目标 | H1.1 | 每个客户端只看自己的角色 | 看到别人的视角 → SpringArm 没判断 |

### H2. 玩家攻击同步

| 任务 | 修改文件 | 类/函数 | 前置依赖 | 验收方法 | 失败定位 |
|---|---|---|---|---|---|
| H2.1 攻击动画改 Multicast | `ArcaneVanguardCharacter.cpp` | `PlayComboSection` → 新增 `ServerPlayComboSection` + `NetMulticast_PlayComboSection` | H1.1 | A 攻击，B 端能看到 A 的挥刀动画 | B 端无动画 → Multicast 没写 / `bReplicates` 没开 |
| H2.2 判定加 Authority | `AVCombatNotifyStates.cpp` | `AVAttackWindow::NotifyBegin` 加 `if (!HasAuthority()) return;` | H2.1 | A 攻击，敌人掉血；B 端看到敌人掉血（属性复制后） | 敌人不掉血 → 服务器没播动画 → 检查 H2.1 |
| H2.3 伤害加 Authority | `CombatStatics.cpp` | `ApplyDamage` 加 `if (!SourceASC->IsOwnerActorAuthoritative()) return 0;` | 无 | 客户端调 `ApplyDamage` 无效 | 客户端能改血 → 检查 `IsOwnerActorAuthoritative` |
| H2.4 连招状态不复制 | — | 保持 `ArmNextSection` / `Tick` / 窗口全本地 | — | 各客户端连招独立，不影响他人 | 若不同步 → 这是**预期行为**，不需要修 |
| H2.5 攻击表现（特效） | 新增 `GameplayCue` 或 Multicast | — | H2.1 | B 端看到 A 的攻击特效 | 无特效 → Cue 没挂 |

### H3. GAS 同步

| 任务 | 修改文件 | 类/函数 | 前置依赖 | 验收方法 | 失败定位 |
|---|---|---|---|---|---|
| H3.1 AttributeSet 复制 | `AttributeSet.h/.cpp` | `GetLifetimeReplicatedProps` + 每属性 `OnRep` | H1.1 | B 端能看到 A 的血量变化 | 血条不动 → `DOREPLIFETIME` 没写 |
| H3.2 OnRep 触发通知 | 同上 | `OnRep` 里调 `GAMEPLAYATTRIBUTE_REPNOTIFY` | H3.1 | 客户端 HUD 跟随 | — |
| H3.3 Q 技能策略调整 | `ArcaneVanguardGameplayAbility.cpp` | `NetExecutionPolicy` 改 `ServerInitiated`（仅 Q） | H3.1 | B 端放 Q，A 端能看到 Buff 生效 | 客户端血没回 → 策略错 |
| H3.4 受击 Cue | 新增 `GameplayCue.Hit` + 受击 GE 挂 Cue | — | H3.1 | B 端看到 A 被打的闪光 | — |

### H4. 敌人同步

| 任务 | 修改文件 | 类/函数 | 前置依赖 | 验收方法 | 失败定位 |
|---|---|---|---|---|---|
| H4.1 敌人改服务器 Spawn | `GameMode.cpp` 新增 Spawner | `PostLogin` 或 `StartPlay` 里 Spawn 敌人 | H1.2 | 关卡里不再手摆敌人，服务器生成 | 客户端有 2 份敌人 → 关卡实例没删 |
| H4.2 敌人开复制 | `Enemy.cpp` 构造函数 | `bReplicates = true` `bReplicateMovement = true` | H4.1 | B 端能看到敌人移动 | 敌人不动 → `bReplicateMovement` 没开 |
| H4.3 AI 只在服务器 | `EnemyAIController.cpp` | `OnPossess` 加 `if (!HasAuthority()) return;` | H4.1 | 客户端不跑 BT | 客户端也在跑 → 检查 `AutoPossessAI` |
| H4.4 目标选择支持多玩家 | `BTService_AVUpdateTarget.cpp` | 遍历 `PlayerArray` 选最近存活玩家 | H4.3 | 两个玩家分散，敌人追最近的 | 只追房主 → 还是 `GetPlayerPawn(0)` |
| H4.5 敌人攻击 Multicast | `Enemy.cpp` | `BeginAttack` → `Server` + `Multicast_PlayAttackMontage` | H4.2 | B 端看到敌人挥拳 | — |
| H4.6 死亡状态复制 | `Enemy.cpp` | `bIsDead` 改 `Replicated` + `OnRep_Dead` 播 ragdoll | H4.2 | A 打死敌人，B 端看到尸体倒下 | B 端敌人站着 → `OnRep` 没播 ragdoll |
| H4.7 敌人伤害 Authority | `Enemy.cpp` | `PerformAttackHitCheck` 加 `HasAuthority` | H4.3 | 敌人只在服务器判定 | — |

**最小可运行版本 = H1 + H2 + H3.1 + H4.1~H4.3**。这一组做完就能实现「两个客户端看到彼此 + 一起打同一个敌人 + 敌人会追最近的玩家」。

---

## 11. 开发优先级

### P0（不做就跑不起来双客户端）

| # | 项 | 位置 |
|---|---|---|
| P0-1 | Character / Enemy 开 `bReplicates` + `bReplicateMovement` | 两个构造函数 |
| P0-2 | 攻击蒙太奇改 Multicast（否则服务器不播动画 → Notify 不触发 → 判定不执行） | `PlayComboSection` |
| P0-3 | `AVAttackWindow::NotifyBegin` 加 `HasAuthority` | `AVCombatNotifyStates.cpp` |
| P0-4 | `ApplyDamage` 加 `IsOwnerActorAuthoritative` | `CombatStatics.cpp` |
| P0-5 | 敌人改服务器 Spawn（关卡实例会在每个客户端各生成一份） | `GameMode` |
| P0-6 | `AIController::OnPossess` 加 `HasAuthority` | `EnemyAIController.cpp` |

### P1（基础多人体验必需）

| # | 项 |
|---|---|
| P1-1 | AttributeSet 复制 + OnRep |
| P1-2 | 敌人目标选择改遍历 `PlayerArray` |
| P1-3 | 敌人死亡状态复制 + `OnRep` 播 ragdoll |
| P1-4 | 受击/命中表现走 `GameplayCue` |
| P1-5 | `GameMode` 换 `AGameMode` 或补登录流程 |
| P1-6 | Q 技能 `NetExecutionPolicy` 调整 |

### P2（玩法扩展时处理）

| # | 项 |
|---|---|
| P2-1 | 武器改「本地表现版」或「服务器 Spawn + 复制」 |
| P2-2 | 玩家死亡/重生走 RPC + Multicast |
| P2-3 | GameState 承载副本进度 |
| P2-4 | 掉落物生成与同步 |
| P2-5 | 拾取与归属 |

---

## 12. 尚需人工确认的问题

| # | 问题 | 为什么必须确认 | 怎么确认 |
|---|---|---|---|
| 1 | `BP_ThirdPersonCharacter` 的 `DefaultAbilities` 是否含 `GA_ArcaneBlast` | 决定 Q 技能是否被授予 | 打开 BP 看 Ability 分类 |
| 2 | `GA_ArcaneBlast` 的 `AbilityTags` 是否有 `Ability.Skill.Q` | 没有则按 Q 静默失败 | 打开 GA 看 Tags 面板 |
| 3 | `GA_ArcaneBlast` 的 `SelfEffectClasses` 是否填了 `GE_Q_Regen` + `GE_Q_Haste` | 没有则技能无效果 | 同上 |
| 4 | `GA_ArcaneBlast` 的 CD/Cost GE 是否挂上 | 决定 `CommitAbility` 是否检查 | 同上 |
| 5 | `BB_Enemy` 是否有 `IsStaggered` 键 | 没有则硬直走 `PauseLogic` 降级 | 打开 BB |
| 6 | `BT_Enemy` 受击分支的 Decorator 是否为 `Is True` + `Aborts = Lower Priority` | 配错则受击不打断攻击 | 打开 BT |
| 7 | `ABP_Enemy_Stone` / `AnimBP_Wizard_ThirdPerson` 是否有 `DefaultSlot` | 没有则蒙太奇播了不显示 | 打开 AnimBP 看 AnimGraph |
| 8 | `BP_AV_Enemy_Golem` 的 `AutoPossessAI` 设置 | 决定客户端是否也会生成 AIController | 打开 BP 看 Pawn 分类 |
| 9 | 关卡里是否有手工摆放的敌人实例 | 联机前必须删掉或改成服务器 Spawn | 打开 ThirdPersonMap 看 Outliner |
| 10 | `AM_NormalAttack` 的 Section 名与 Notify 位置 | 决定连招窗口与判定时机 | 打开蒙太奇 |
| 11 | 是否接受「服务器权威、无客户端预测」 | PvE 可接受 100~150ms；PvP 不行 | 需要你决策 |
| 12 | 目标联机形态：Listen Server / Dedicated / 单机+联机双模式 | 决定 GameMode 与网络配置 | 需要你决策 |

---

## 附：与第一轮审计的差异说明

| 第一轮结论 | 本轮修正 / 深化 |
|---|---|
| 「攻击判定在客户端本地结算」 | **深化**：不只是"在客户端"，而是「判定由 AnimNotify 驱动，而蒙太奇**不复制**」——所以**服务器可能压根不触发判定**。这是比"客户端作弊"更根本的问题 |
| 「AI 无 Authority 判断」 | **深化**：具体到 `OnPossess` 没有 `HasAuthority`；且 `BTService` 用 `GetPlayerPawn(0)` 写死索引 |
| 「GameMode 是 AGameModeBase」 | **深化**：且 `StartPlay` 里**没有任何 SpawnActor**——敌人是关卡摆的，联机时会双份 |
| 「武器各自 Spawn」 | **深化**：给出了「本地表现版 vs 服务器复制版」两种方案，并建议前者（当前规模更省事） |
| 「建议迁 PlayerState」 | **修正**：本轮明确**不建议第一阶段就迁**。PvE 合作、玩家不换角色的情况下，Character 挂 ASC 可以工作；先跑通双客户端再评估 |
| 「不要给所有 Actor 加 bReplicates」 | **遵循**：报告里明确说「加了也解决不了 R2」，必须先改蒙太奇复制 |

---

## 审计范围声明

- **已读取**：`Source/` 全部 28 个文件（含逐函数追踪）、`Config/DefaultEngine.ini`、`Content/` 目录结构与 `.uasset` 清单、`Docs/` 全部文档。
- **未能读取**：所有 Blueprint 内部连线与默认值、`BT_Enemy` 节点参数、`BB_Enemy` 键列表（仅从代码引用推断有 `SelfActor` / `TargetActor` / `DistanceToTarget`）、AnimBP 图表、Montage 的 Section/Notify 配置、关卡摆放。
- **未执行**：编译、运行、PIE、多人测试、性能采样。
- **置信度**：C++ 层为「已确认」；蓝图/资产层为「待人工确认」，已在 §7.2 与 §12 逐条列出。
