# Changelog — 变更记录

## [Unreleased] — 2026-10-30（D5：死亡 / 受击 / 技能框架）

### 背景
5 天战斗闭环冲刺的收尾阶段。目标：补完玩家死亡与重生、敌人受击反馈，并搭好技能框架（GA）。
这一天踩了三个"看起来像代码没写、实际是时序/层级问题"的坑，都记在下面。

### 一、玩家 ragdoll 死亡 + 法杖脱手（AV-047）

**死亡流程**：`Health` 归零 → `HandleDeath`
```
取消技能 → 禁输入 → 停移动 → ragdoll（替代死亡动画）→ 法杖脱手 → 3.8s 后 Respawn
```

**ragdoll 的关键顺序**：
```cpp
PlayerMesh->SetCollisionProfileName(TEXT("Ragdoll"));
PlayerMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);  // 必须在换预设之后
PlayerMesh->SetAllBodiesSimulatePhysics(true);
PlayerMesh->bBlendPhysics = true;
```
`SetCollisionProfileName` 会**重置所有通道响应**，所以 `Ignore Camera` 必须放在它后面——
否则 SpringArm 探针会顶到尸体，镜头被推近。

**保留开关** `bUseRagdollOnDeath`（默认 true），关掉则退回播 `AM_Wizard_Death`。

**重生时的顺序陷阱**：
```cpp
// 先停物理，再还原变换 —— 反了的话物理会在下一帧把变换覆盖回去
PlayerMesh->SetAllBodiesSimulatePhysics(false);
PlayerMesh->SetRelativeLocationAndRotation(InitialMeshRelativeLocation, InitialMeshRelativeRotation);
PlayerMesh->SetCollisionProfileName(TEXT("CharacterMesh"));

// 装武器必须在 Mesh 还原之后 —— 否则插槽世界变换还是倒地的位置
DetachAndDestroyWeapon();
SpawnAndAttachWeapon();
```

### 二、法杖脱手（`AArcaneVanguardWeapon::EnablePhysicsDrop`）

```cpp
SetActorTickEnabled(false);              // 停握持插值，否则和物理打架
SetSimulatePhysics(true);
WakeAllRigidBodies();
AddAngularImpulseInDegrees(随机 ±10);     // 幅度已参数化，见下
```

**`DropAngularImpulseRange`**（`Weapon|Collision` 分类，默认 10.0）：
脱手时的随机翻滚幅度，`0` = 完全不翻滚。之前写死 ±40，法杖长 194cm，
杖尖线速度约 0.97 m/s，看起来像被甩飞。

### 三、★★★ 法杖穿模的真正根因：`SetActorEnableCollision(false)`

**症状**：法杖脱手后穿过地面。`AddAngularImpulse` / `GetMass` 报
"必须将碰撞启用设为查询和物理或纯物理"，日志 `CollisionEnabled=0 Mass=1.00`。

**排查过程（4 轮编译全白费）**：依次试过
1. `SetCollisionEnabled(QueryAndPhysics)` —— 无效
2. `SetCollisionProfileName("PhysicsActor")` + `SetCollisionEnabled` —— 无效
3. 钉死 `"Custom"` + 显式设 enabled/objectType/response —— 无效
4. `FBodyInstance::SetCollisionEnabled` + `UpdatePhysicsFilterData` 兜底 —— 无效
5. 以上 + `RecreatePhysicsState()` —— 无效

**真正根因**：`SpawnAndAttachWeapon` 里有一行
```cpp
NewWeapon->SetActorEnableCollision(false);   // ← 元凶
```
**`SetActorEnableCollision` 是 Actor 级总开关，优先级高于组件的 `SetCollisionEnabled`。**
Spawn 之后调它，会把该 Actor 上所有组件的碰撞查询全部关掉，
之后无论怎么改组件层都读回 0。

**修法**：
- 删掉那行
- "武器不挡角色"改用**通道响应**解决（见下）

**第二个坑（顺带发现）**：`FBodyInstance::GetCollisionEnabled()` 在
`CollisionProfileName != "Custom"` 时会去 `UCollisionProfile` 查预设，
**查不到就返回 `NoCollision`**，组件里实际存的字段值根本不影响返回值。

**最终方案**：碰撞配置放在 `OnConstruction`（Spawn 流程中早于物理状态创建）：
```cpp
MeshComponent->SetCollisionObjectType(ECC_GameTraceChannel1);        // Weapon
MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);        // 默认安全
MeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // 只和地面/墙碰
```
用「全 Ignore 只开一个」而不是「全 Block 再逐个 Ignore」——
后者每加一条新通道都得记得补 Ignore，漏一个就出事故（`PhysicsBody` 就是这么漏掉、
导致武器把 ragdoll 尸体撞飞的）。

### 四、自定义碰撞通道体系（负责人主导设计）

**`Config/DefaultEngine.ini` 新增两条 Object Channel**：
```ini
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Overlap,bTraceType=False,bStaticObject=False,Name="Weapon")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Block,bTraceType=False,bStaticObject=False,Name="Enemy")
```

**`DefaultResponse` 的含义容易误解**：它是"**别的物体对这条通道**的默认响应"，不是"这条通道的物体怎么对待别人"。
- `Weapon` 默认 Overlap → 别人对武器 Overlap（但武器自己 Ignore 一切，所以不碰）
- `Enemy` 默认 **Block** → 别人对 Enemy 通道都 Block，
  这意味着 `SweepMultiByChannel(Enemy)` 会命中**所有东西**（地面/墙/自己）

**三处改动**：
| 位置 | 改动 |
|---|---|
| 武器 `ApplyWeaponCollisionSetup` | ObjectType = `Weapon`，全 Ignore 只 Block `WorldStatic` |
| 敌人 Capsule（构造函数） | ObjectType = `Enemy`，且对 `Enemy` 通道 `Block`（让球扫能查到自己） |
| 玩家攻击球扫 `PerformAttackHitCheck` | 改用 `SweepMultiByObjectType(Enemy)` |

**为什么球扫必须换 API**：`Enemy` 通道默认 Block，按 TraceChannel 扫会命中地面、墙壁、玩家自己——
虽然最后会被"没有 ASC 就跳过"兜住，但会刷一堆无用日志、多做无用计算。
`SweepMultiByObjectType` 只按物体自身 ObjectType 过滤，精确得多。

**敌人球扫保持 `ECC_Pawn` 不动**（玩家仍然是 Pawn 通道）。

**副作用正好**：敌人死亡切 `Ragdoll` 预设会把 ObjectType 重置成 `PhysicsBody`，
尸体自动脱离 `Enemy` 通道 → 玩家球扫打不到尸体，武器也不撞它。

### 五、敌人受击硬直（AV-050）

**触发**：`OnHealthChanged` 里
```cpp
if (Data.NewValue <= 0)          { HandleDeath(); return; }   // 死了不播受击
if (Data.NewValue <  Data.OldValue) { HandleHitReact(); }      // 只有掉血才算挨打
```
用 `<` 而不是 `!=`：初始化时 Health 从 0 拉到满血，那种回调不能算挨打。

**打断分三层，缺一层都会露馅**：
| 层 | 代码 | 少做会怎样 |
|---|---|---|
| 动画 | `AnimInstance->Montage_Stop(0.1f)` | 攻击蒙太奇的 **AnimNotifyState 攻击窗口**会在受击动画下继续结算伤害 |
| 移动 | `Movement->StopMovementImmediately()` | 带着惯性滑一段 |
| AI | 写 Blackboard `IsStaggered` | BT 下一帧继续追过来 |

**架构决策（负责人质疑后改进）**：受击状态**不该用 `PauseLogic` 硬暂停整个 BT**，
而应该放 Blackboard 让 BT 自己切分支。三个理由：
1. `PauseLogic` 粒度太粗——暂停的是整个 BT 的思考，不只是当前动作
2. `ResumeLogic` 漏调用 = AI 永久瘫痪且**零报错**（隐藏的致命 bug）
3. `bIsStaggered` 只在 C++ 里，BT 看不见，无法基于受击状态做决策

**双路径 + 自动降级**：
```cpp
NotifyAIStaggerStart():
  首选  BB->GetKeyID("IsStaggered") != InvalidKey
        -> SetValueAsBool("IsStaggered", true)   // BT 自己 Abort 战斗分支
  兜底  键不存在 -> StopMovement + PauseLogic（旧行为，bStaggerPausedAI = true）
```
`bStaggerPausedAI` 保证 `PauseLogic` / `ResumeLogic` 严格配对。

**BT 侧配置**（`BT_Enemy`）：
```
Selector
├─ [受击] Decorator: IsStaggered / Is True / Aborts = Lower Priority
│         └─ Wait 0.6s
└─ [战斗] Sequence → AV Update Target / Move To / Attack
```
三个必须：父节点是 `Selector`（不是 `Sequence`）、Key Query 是 **`Is True`**（不是 `Is Set`，
Bool 键的 `false` 也算"有值"）、Aborts 是 **`Lower Priority`**（不是 `Self`）。

**踩坑**：`UBlackboardComponent::SetValueAsBool` 在 C++ 里**返回 `void`**（Python 里才是 bool），
且只有 `FName` 重载、没有 `FKey` 版本。判断"键是否存在"必须单独用 `GetKeyID`。

### 六、武器显隐（AV-051）

```cpp
UFUNCTION(BlueprintCallable) void SetWeaponHidden(bool bInHidden);
```
巫师自带的技能动画都不含法杖，施法时必须藏起来。

**三个设计决定**：
1. **只 `SetActorHiddenInGame`，不动碰撞**——武器本来只 Block `WorldStatic`，藏起来不需要关碰撞；
   动 `SetActorEnableCollision` 有"关了恢复不回来"的风险
2. **用 `GetAttachParentActor() != this` 判断"还在手上"**——已脱手掉落的武器不受影响
3. **重生时重置为可见**——`SpawnAndAttachWeapon` 里清 `bWeaponHidden`，避免旧状态继承

**踩坑**：参数不能叫 `bHidden`——`AActor` 已有同名成员，UHT 报 shadowing 错误，改 `bInHidden`。

### 七、普攻接入增强输入（AV-052）

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
UInputAction* AttackAction;

EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started,
    this, &AArcaneVanguardCharacter::OnAttackPressed);
```
之前 `OnAttackPressed` 是在蓝图里被直接调用的（测试用），现在走标准输入。
**用 `Started` 不用 `Triggered`**：连击推进由 `OnAttackPressed` 内部的窗口判定控制，
输入层再 Triggered 会导致按住时一帧推一段。

**接入后必须删掉蓝图里原来的调用节点**，否则一次按键同时触发两次，跳两段连击。

### 八、GA 基类完善 + Q 技能数据层（AV-045 部分）

**架构原则**（回答"GA 该 C++ 还是蓝图"）：
- **C++ 基类 = 通用流程**（提交消耗 / 隐藏武器 / 施加效果 / 播蒙太奇 / 收尾）
- **蓝图子类 = 只填数据**（蒙太奇、GE 列表、CD、消耗），一个节点都不用连
- 参照 Epic 的 Lyra：`ULyraGameplayAbility`(C++) → `GA_WeaponFire`(BP)

**`UArcaneVanguardGameplayAbility` 新增**：
| 成员 | 作用 |
|---|---|
| `AbilityMontage` | 施法动画，留空 = 瞬间生效 |
| `MontagePlayRate` | 动画速率 |
| `bHideWeaponWhileCasting` | 施法期间藏法杖（默认 true） |
| `SelfEffectClasses` | 施加给自己的 GE 列表 |

`ActivateAbility` 流程：`CommitAbility` 失败就 `EndAbility`（否则技能永久挂住）→ 藏武器 →
`ApplySelfEffects` → 有蒙太奇走 `UAbilityTask_PlayMontageAndWait`，没有就立即 `EndAbility`。
`EndAbility` 统一恢复武器——正常结束/被打断/被取消都会走。

**新增属性 `AttackSpeedMultiplier`**（`UArcaneVanguardAttributeSet`）：
基础 1.0，`PreAttributeChange` 钳在 `[0.1, 5.0]`（0 会让 PlayRate=0 动画卡死）。
`PlayComboSection` 里 `EffectivePlayRate = ComboPlayRate * AttackSpeedMultiplier`。
**`Montage_JumpToSection` 不改播放速率**——接段时必须补 `Montage_SetPlayRate`，
否则第二段起沿用起手速率，中途上 Buff 不生效。

**新增两个原生 GE**：
| GE | Duration | Period | Modifier |
|---|---|---|---|
| `UGE_Q_Regen` | 5s | **1s** | `Health +10` Additive |
| `UGE_Q_Haste` | 5s | **无** | `AttackSpeedMultiplier +0.3` Additive |

**★★ 关键约束：GE 的 `Period` 是 GE 级的**，它会让**所有** Modifier 都按周期结算。
所以「每秒回血」和「攻速加成」**必须拆成两个 GE**，否则攻速会每秒 +0.3 累加
（1.0 → 1.3 → 1.6 → 1.9…）。

### 验证

| 项 | 结果 |
|---|---|
| 编译 | 28 个 obj 全通过，DLL 583680 字节 |
| 玩家 ragdoll + 掉武器 | PIE 实测通过（多次死亡重生循环无异常） |
| 碰撞通道 | spawn 实例实测：武器 `ECC_WEAPON`(14)、敌人 Capsule `ECC_ENEMY`(15)、玩家 `ECC_PAWN`(2) |
| 武器质量 | `Mass=24.90`（真实重量，不再是默认 1.0） |
| 敌人受击 | 受击动画 + 硬直 + BT 分支切换，PIE 实测通过 |
| Q 技能 | 数据层就绪，待负责人建 `GA_Q_Buff` 蓝图 |

### 环境问题

**编辑器启动卡在「是否等待 ZenServer？」**：上一次启动的 `zenserver.exe` 没退干净，
端口 8558 不响应。处理：杀掉编辑器和 zenserver 进程，重启编辑器（ZenServer 会自动拉起）。

**`build_manual.sh` 的致命缺陷**：编译前不删旧 obj。上一次编译被中断后，
旧 obj 残留，`link` 报 `LNK2019 无法解析的外部符号`，看起来像函数没实现。
已修：编译前先 `rm -f` 每个同名 obj。排查手法是比对 obj 与源文件时间戳。

### Notes
- 本轮**未改动** GameDesign / AgentRules / KI013 三份文档。
- 碰撞通道由负责人主导设计，AI 负责落地与验证。

## [Unreleased] — 2026-10-27（M2 GAS 骨架 + M3 伤害链路）

### 背景
5 天战斗闭环冲刺的第 1~2 天。目标：把 GAS 立起来，并让普攻真的能扣血。
昨天（10-26）已提前完成 GAS 骨架的代码与运行时验证，今天补伤害链路与敌人基类。

### 零、构建环境修复（KI-018，当日最大收益）

**根因不是编译环境坏了，是一行配置残留**：
`%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` 里有人为的
`<bAllowUBAExecutor>false</bAllowUBAExecutor>` —— 10-25 为排查另一个编译错误临时加的，
注释写着"排查完可删掉"，但一直没恢复。关掉 UBA 后走老的 ParallelExecutor，而它在这台机器上是坏的。

**修复后**：完整构建 7/7 通过（含此前必失败的 `WriteMetadata`），增量 1.29s。
**重启机器对该故障无效** —— 以后遇到"编译秒退且零诊断"，先查这个配置文件。

### 一、GAS 骨架（AV-038 / 039 / 040）

| 项 | 内容 |
|---|---|
| 插件 | `.uproject` 启用 `GameplayAbilities`（该插件 `EnabledByDefault = false`，**必须显式启用**） |
| 模块 | `Build.cs` 加 `GameplayAbilities` / `GameplayTags` / `GameplayTasks` |
| 新增 | `AbilitySystem/ArcaneVanguardAttributeSet.h/.cpp`：`Health` / `MaxHealth` + `PreAttributeChange` 钳制 |
| 角色 | 实现 `IAbilitySystemInterface`；`CreateDefaultSubobject<UAbilitySystemComponent>`；`PossessedBy` 里 `InitAbilityActorInfo(this, this)` |

**三个关键决策**：

1. **属性集必须显式注册**：`UAttributeSet` 是普通 UObject，不是 UActorComponent，ASC 不会自动发现它。
   构造函数里 `AddAttributeSetSubobject(AttributeSet.Get())` —— **必须传 `.Get()`**：
   该函数是模板 `template<class T> AddAttributeSetSubobject(T*)`，`TObjectPtr` 到裸指针的
   隐式转换在**模板类型推导时不生效**，推不出 `T`。
2. **初值写在 AttributeSet 构造函数里**（`InitHealth(100)`），不依赖任何蓝图资产就能跑通。
3. **初始 GE 必须放 `PossessedBy`，不能放 `BeginPlay`**：GameMode 的顺序是
   `SpawnActor`（触发 BeginPlay）→ `Possess`（触发 PossessedBy）。放 BeginPlay 时
   `ActorInfo` 还没初始化，GE 会**静默失效**。

**验证**（`-game` 实跑日志）：
```
[GAS] ActorInfo 已初始化：ASC=AbilitySystemComponent，Owner=BP_ThirdPersonCharacter_C_0
[GAS] 属性集已注册：Health=100.0 / MaxHealth=100.0
```

### 二、伤害链路（AV-041）

**新增 `AbilitySystem/ArcaneVanguardGameplayEffects.h/.cpp` —— `UGE_ArcaneDamage`**

原生 GE，配置全在构造函数里（`DurationPolicy` / `Modifiers` 在 `UGameplayEffect` 的 public 区，可直接改）：

| 项 | 值 |
|---|---|
| DurationPolicy | `Instant`（伤害是瞬时事件，不是持续 buff） |
| Modifier 数 | 1 |
| Attribute / Op | `Health` / `Additive` |
| Magnitude | **`SetByCaller`**，key = `FName("Data.Damage")` |

**为什么 magnitude 走 SetByCaller**：同一个 GE 类能被普攻（30）和技能（50）复用，
不用为每个伤害数值建一个 GE 资产。

**为什么用 FName 而不是 GameplayTag 作 key**：`FSetByCallerFloat` 同时支持
`DataName`(FName) 和 `DataTag` 两种 key。用 FName **不需要在 `DefaultGameplayTags.ini`
里注册 tag**，零配置即可跑通 —— 适合 M3 这种"先打通链路"的阶段。
以后技能系统要按 tag 组织（`Data.Damage.Fire`）时再迁到 DataTag 版本。

**新增 `AbilitySystem/ArcaneVanguardCombatStatics.h/.cpp` —— `ApplyDamage`（ADR-005 唯一入口）**

做成蓝图函数库而不是某个 Actor 的成员，因为玩家打敌人、敌人打玩家、陷阱打人三种情况都要用。

**符号约定**：调用方永远传**正数**伤害，`ApplyDamage` 内部统一 `-FMath::Abs(Damage)`。
避免调用方一会儿传正一会儿传负把符号搞乱。

**Source 退化路径**：GAS 需要一个 Source ASC 来构造 Spec。攻击者不是 GAS Actor
（比如场景陷阱）时退化成"目标自己当 Source"，伤害照样结算，只是 GE 的 Source 信息不准。
⚠️ **联机时这个退化不成立**（客户端不能拿别人的 ASC 造 Spec），到时必须传真实攻击者。

**角色侧判定（`AArcaneVanguardCharacter::PerformAttackHitCheck`）**

| 参数 | 默认 | 说明 |
|---|---|---|
| `AttackRange` | 200 | 从胶囊体中心向前扫的距离（cm） |
| `AttackRadius` | 60 | 判定球半径。**必须 ≥ 敌人胶囊体半径**，否则贴身打不到 |
| `AttackDamage` | 30 | 基础伤害 |
| `DamageEffectClass` | 空 | 空 = 原生 `UGE_ArcaneDamage`；填 BP 子类则覆盖（**A+B 兼容**） |

**三个实现细节**：

1. **起扫点用胶囊体中心**（`GetActorLocation()`），不是脚底 —— 从脚底起扫时球心贴地，
   只能扫到敌人小腿。
2. **必须排除 `SpawnedWeapon`**：武器是独立 Actor、挂在手上、就在扫掠路径里，
   不排除的话每次都会命中自己的法杖。
3. **段内去重**：球扫会为一个目标的多个骨骼/组件返回多条 Hit，不去重会一刀打 3~4 次。
   用 `TSet<TWeakObjectPtr<AActor>>`（弱引用，不拖住 GC），每次 `SetAttackWindowOpen(true)` 清空。

**不做"窗口内每帧检测"**：三段连击的判定窗口只有 0.2s，一次球扫足够；
每帧扫会让同一刀在不同帧命中不同目标，手感不可控。

**Notify 侧**：`UAnimNotifyState_AVAttackWindow` 新增 `DamageOverride`（默认 -1 = 用角色的
`AttackDamage`），按段配不同伤害时不用改代码。`NotifyBegin` 里顺序不能反：
**先 `SetAttackWindowOpen(true)`（清空命中记录）再判定**，否则新一刀会带着上一刀的记录，
打同一个敌人时直接跳过。

**验证（静态）**：
```
duration_policy: INSTANT
modifier 数量: 1
  [0] attribute=Health  op=ADDITIVE  calc=SET_BY_CALLER
        DataName = Data.Damage
```
`apply_damage` / `perform_attack_hit_check` 已暴露到蓝图；Notify `damage_override` 默认 -1.0。

### 三、敌人基类（AV-042）

**新增 `Enemy/ArcaneVanguardEnemy.h/.cpp`**

职责边界：**只管"自己怎么挨打、怎么死"，不管"怎么追人、怎么出手"**（后者是 AI 的事，AV-043）。

**和玩家的关键差异**：敌人可能是个**没有任何 Controller 的静态靶子**，
`PossessedBy` 根本不会触发，所以 `InitAbilityActorInfo` 放 **`BeginPlay`**；
被 AI 接管后 `PossessedBy` 再补一次。

**死亡用 ragdoll，不用动画**：
```cpp
EnemyMesh->SetCollisionProfileName(TEXT("Ragdoll"));
EnemyMesh->SetAllBodiesSimulatePhysics(true);
EnemyMesh->bBlendPhysics = true;
```
石头人这类重甲怪，物理倒地比播死亡动画更有说服力，而且**不需要死亡动画资产**
（`SpearAnimationsV2` 里没有 Death 类）。

**死亡触发用属性变化委托**，不在 AttributeSet 里 Cast 敌人：
```cpp
ASC->GetGameplayAttributeValueChangeDelegate(HealthAttribute).AddUObject(this, &OnHealthChanged);
```
属性集保持通用，D5 玩家死亡用同样写法。

### 四、资产勘察结论（对冲刺的影响）

- **`Content/Stone_Golem/`**：骨架是 `root/pelvis/spine_01/neck_01/hand_r/...`，
  **没有 spine_04/05、neck_02** → 与 `SKE_Wizard` 同属 UE4 Mannequin 命名体系，
  重定向链路与之前一致。自带 `PA_Stone_Golem_PhysicsAsset`（ragdoll 必需）。
- **`Content/SpearAnimationsV2/`**：402 个资产，含 `UE4_Mannequin_Skeleton`。
  攻击 28 个（1H 13 / 2H 15）、受击 8 个、待机 4 个、格挡 4 个、移动 6 个、过渡 180 个。
  **没有死亡动画**（所以用 ragdoll）。
- **负责人实测**：`AB_1H_*` 里有 2 个重定向后效果不错（右手下砸、右手下冲拳），
  石头人体型大于玩家也没问题。→ 攻击动画方案确定，不用冲撞/跳砸那套备选。

### 编译

| 项 | 结果 |
|---|---|
| 首次（含 UHT 重跑） | **14 动作 / 842 秒**，`ArcaneVanguardCombatStatics.cpp` 1 个错误 |
| 错误 | `无法将参数 1 从 const UClass* 转换为 TSubclassOf<UGameplayEffect>` |
| 修正 | 直接声明 `TSubclassOf<UGameplayEffect> EffectClass`（`TSubclassOf` 构造只接受**非 const** `UClass*`） |
| 增量 | **4/4 通过，10.88 秒** |

### 五、下午：判定可视化 + 硬编码排查 + 注释修正（2026-10-27 16:25）

#### 5.1 攻击判定范围可视化（调试用）

`AArcaneVanguardCharacter` 新增：

| 开关 | 用法 |
|---|---|
| BP 属性 `bDrawAttackHitCheck` | `BP_ThirdPersonCharacter` → `Combat\|Debug` |
| 控制台 `av.DrawAttackHitCheck 1 / 0` | PIE 里按 ` 打开控制台，运行中随时切，不用重编译 |

绘制内容：绿色胶囊（扫掠体积）+ 黄色球心轨迹 + 青色起点 + 红点命中点 + `Range/Radius/Hits` 摘要。

**知识点**：`SweepMultiByChannel + MakeSphere(R)` 的形状 = **胶囊体**（圆柱 + 两端半球），
和 `CapsuleComponent` 同形状，只是轴向由 `Start→End` 决定。

**坑**：`DrawDebugSweptSphere` **在 UE 5.4 里不存在**（只在 UE5 早期版本有）。
改用 `DrawDebugCapsule` + `FQuat::FindBetweenNormals` 把胶囊轴向对齐扫掠方向；
`HalfHeight = 圆柱半长 + Radius`（含半球，与 `UCapsuleComponent` 一致）。

#### 5.2 ★ 资产移动导致 GameMode 失效（再次踩中）

`BP_ThirdPersonCharacter` 被移到 `/Game/ArcaneVanguard/BluePrint/Player/`，
`GameMode.cpp` 的 `FClassFinder` 还指旧路径 → 日志 `Failed to find ...` →
**退回引擎默认 Pawn（不是巫师，没有武器和连击）**。

已修正路径，并把 Warning 升级成 **Error**（带预期路径 + 后果说明）。
> `Architecture` §8 早就警告过，还是踩了。**每次移动这两个蓝图都要检查 GameMode.cpp。**

#### 5.3 硬编码全盘扫描（10 cpp + 10 h）

结果见 `TaskBoard` 与本次会话报告。分类：A 类（会静默失效）/ B 类（注释与实现不一致）/
C 类（数值硬编码）/ D 类（合理内置常量，保留）。

#### 5.4 注释修正（以实际代码为准，只改注释）

| 位置 | 原来写的（错） | 改成 |
|---|---|---|
| `Weapon.cpp` 构造 | "前移 25cm" | 90cm |
| `Weapon.cpp` 构造 | "1 秒走完，与 AnimBP 一致" | 10.0 ≈ 0.1 秒，且与 AnimBP 的 1 秒**不同步** |
| `Weapon.h` 类注释 | "校准在蓝图子类里通过 RelativeTransform 完成" | **这条是错的** —— 运行时会被 Attach 冲掉 |
| `Weapon.h` 类注释 | "与 CombatAlpha 使用同一节奏，因此不会割裂" | "应保持匹配，否则会割裂"（当前值不匹配） |
| `Character.h` `SpawnAndAttachWeapon` | "方便在蓝图里从零开始校准" | 明确警告"不要在蓝图里改相对变换" |

#### 5.5 ★★ 新坑：源码注释里不能用 emoji

给注释加 `⚠️`（U+26A0 + U+FE0F）后，3 个文件编译失败：

```
error C2143: 语法错误: 缺少";"(在"<class-head>"的前面)
error C4430: 缺少类型说明符 - 假定为 int
error C2039: "OnConstruction": 不是 "UObject" 的成员
```

看起来像整个类没定义，实际是注释里的 emoji 让解析器错乱。
**rsp 里已有 `/utf-8` 也救不了**。换成 `[!]` 后立刻通过。
→ 注释一律用 `[!]` 这类 ASCII 标记。

> 附带：`cl.exe` 的中文错误输出是 **GBK**，直接 `grep` 会当二进制跳过，
> 需 `iconv -f GBK -t UTF-8` 转码才看得到真实错误。

#### 5.6 构建环境（KI-018 的第二个成因）

`Total time in Parallel executor: 0.07 seconds`，7 个 action 全失败、零诊断，
但 `BuildConfiguration.xml` 是干净的（`bAllowUBAExecutor` 早已删除），`-NoUBA` 同样失败。
**手动跑同一个 `cl.exe @rsp` 完全成功** → 是 UBT 创建子进程失败，不是编译错误。
处理：手动 `cl.exe` 逐个编译（20 TU ≈ 1m50s）+ `lib.exe` + `link.exe`。
> `ArcaneVanguardWeaponNotifies.cpp.obj.rsp` 是 Intermediate 陈旧残留，跳过即可。

#### 5.7 硬编码收口：A1/A2 + C1（2026-10-27 16:32）

**A1/A2 —— 删掉 GameMode 的两条硬编码资产路径**

`AArcaneVanguardGameMode` 构造函数现在**什么都不设**，`DefaultPawnClass` /
`PlayerControllerClass` 全部由 `BP_AV_GameMode` 的 Class Defaults 提供。

新增 `StartPlay()` 诊断：打印实际生效的 `Pawn=` / `Controller=`；
若 `DefaultPawnClass` 仍是 `ADefaultPawn` 则打 **Error** 并提示去 BP 里配置。

> 为什么必须改：`FClassFinder` 只吃编译期字符串常量，资产一移动就得改代码重编译。
> 这个坑 10-24 和 10-27 踩了两次，第二次直接导致玩家变成默认 Mannequin。

**C1 —— 敌人血量入口**

`AArcaneVanguardEnemy` 新增 `MaxHealthOverride`（`EditDefaultsOnly`，默认 0 = 沿用 100）。
`BeginPlay` 里 `InitAbilityActorInfo` 之后应用。

**顺序必须是「先 MaxHealth 后 Health」**：`PreAttributeChange` 会把 Health 钳到 `[0, MaxHealth]`，
反过来先设 `Health=300` 而 MaxHealth 仍是 100，会被直接钳成 100。

> 背景：玩家和敌人共用同一个 `UArcaneVanguardAttributeSet`，初值写死在构造函数（100/100）。
> 石头人应该更厚，但改 AttributeSet 会同时影响玩家，所以给敌人单独开入口。

#### 5.8 ★★★ 新坑：改 .h 的行数会让编译崩（GENERATED_BODY 行号）

删 GameMode 的 FClassFinder 时在 `UCLASS` 前加了注释块，**又报 C2143**（和 emoji 坑症状一样）。
排查：`minimalapi` 改回 → 仍失败；块注释改行注释 → 仍失败；
而 `Enemy.h` 同样改了却能编译 —— **区别是 Enemy.h 的新内容加在 `GENERATED_BODY()` 之后**。

**根因**：`GENERATED_BODY()` 展开成 `FID_<file>_<__LINE__>_GENERATED_BODY`，
宏名带 **`__LINE__`**；而 `generated.h` 里的宏名是 UHT 按**上次生成时的行号**写死的。
**在 `GENERATED_BODY()` 之前增删任何行，行号一变宏名就对不上，整个类解析失败。**

**解法**：① 重跑 UHT（本机 UBT 坏了，且 Launcher 版 UE 5.4 的 `Engine/Binaries` 下
**没有** `UnrealHeaderTool.exe`，跑不了）；② **把新增内容放在 `GENERATED_BODY()` 之后**（本次采用）。

> 推论：UBT/UHT 修好之前，改 .h 只能"在 GENERATED_BODY 之后追加"。

### Docs
- `TaskBoard`：AV-038/039 → Verified；AV-040 → Implemented；**AV-041/042 → Implemented**；D1 标记完成、D2 标记"代码提前完成"；AV-035 补记参数变更（25 → 90）
- `KnownIssues`：KI-018 → **已解决**（根因是 UBA 配置残留，不是环境损坏）
- `DevelopmentStatus`：M2 标 GAS 骨架完成、M3 标进行中；B4 / B6 / B7 三个阻塞项全部解除
- `Architecture`：§6.1 过渡时长描述修正（武器侧现在 10.0，与 AnimBP 的 1 秒不同步）；§7.4 参数与 90cm 实测数据更新
- `Architecture`：新增 **§7.5 伤害链路**（链路图 / 三类分工 / GE 配置 / 调参指引 / 三个坑 / 已知限制）；
  §2.2 源文件树补 `AbilitySystem/` `Enemy/`；§3.1 / §3.2 补 4 个新类；§9 预留表转"已实装"；
  ADR-004 / ADR-005 转 **Accepted**


## [Unreleased] — 2026-10-26（M1 收尾：战斗握持偏移 + 普攻三段连击 + 第三段动画重做）

### 背景
两件事收口：① 待机时手握在杖身 2/3 处，战斗动画端起法杖后前段只剩约 70cm，长枪感出不来；
② 普攻只有一条蒙太奇、没有连击判定与输入缓冲。另外第三段动画有瑕疵，由负责人重做。

### 一、战斗握持偏移（武器沿杖身前伸）

**实测根因**：`StickSocket` **不在手上**。

| 点 | 世界坐标 |
|---|---|
| `hand_r` | (2.6, 35.7, 29.1) |
| `StickSocket` | (9.2, 37.3, **−95.4**) |

即 socket 在 `hand_r` 上带了约 **−124cm** 的向下偏移，几乎落到地面 → 杖尾贴地、杖尖穿到手上方，
手正好卡在杖身 **62%**。`SM_Wizard_Staff_basic` 包围盒 Z ∈ [3.8, 198.2]，**枢轴在杖尾**，长轴 +Z。

**实现**（`AArcaneVanguardWeapon`）：

| 成员 | 默认 | 作用 |
|---|---|---|
| `CombatGripOffset` | `(0,0,25)` | 战斗握持偏移，网格本地空间，沿杖身改 Z |
| `CombatGripInterpSpeed` | 1.0 | alpha/秒；1.0 = 1 秒，与 AnimBP 的 CombatAlpha 对齐 |
| `bAutoTrackOwnerCombat` | true | 自动读 Owner 的 `IsInCombat()` |
| `CaptureBaseTransform()` | — | 记录基准相对位置，**必须在 Attach 之后调** |

**关键决策**：
- `BeginPlay` 里不能记基准 —— `SpawnActor` 出来的武器 `BeginPlay` 早于 `Attach`，那时相对变换不是终值。
- 每帧只写「基准 + 偏移」，绝不累加。
- 偏移先按 `MeshComponent->GetRelativeRotation()` 旋到父空间再叠加，保证"沿杖身"而不是"沿世界轴"。

**验证**：待机 124.4cm（62%，前段 73.8cm）→ 战斗 99.4cm（49%，前段 **98.8cm**），
偏移 25cm，插值链 0 → 12.5 → 25 → 0 无漂移。

### 二、普攻三段连击

**新增文件** `Source/ArcaneVanguard/Animation/AVCombatNotifyStates.h/.cpp`：

| 类 | 编辑器显示名 | 作用 |
|---|---|---|
| `UAnimNotifyState_AVComboWindow` | AV Combo Window | 连击窗口：窗口内按攻击才允许接下一段 |
| `UAnimNotifyState_AVAttackWindow` | AV Attack Window | 判定窗口：M1 只记状态，M3 在此结算伤害 |

**新增成员**（`AArcaneVanguardCharacter`，`Combat|Combo` 分类）：
`ComboMontage`（`EditAnywhere`，不硬编码路径）、`ComboSectionCount`（3）、`ComboPlayRate`、
`bRequireCombatForAttack`、`AttackMoveSpeedScale`（0.3）、`ComboIndex`、`bComboWindowOpen`、
`bAttackWindowOpen`、`bAttackBuffered`；函数 `OnAttackPressed` / `SetComboWindowOpen` /
`SetAttackWindowOpen` / `ResetCombo` / `IsAttacking` / `GetComboIndex`。

**核心设计 —— 方案 B：窗口只收输入，段尾才切**

窗口内按下**不立刻跳段**，而是用 `Montage_SetNextSection` 把当前段链到下一段。
`FAnimMontageInstance::NextSections` 是**每个播放实例一份**的数组（`AnimMontage.h:368`），
运行时挂链**不写回资产**，引擎在段尾沿链自然过渡 —— 后摇完整播完，衔接是标准 Section 混合。

> 方案 A（按下即 `JumpToSection`）实测会砍掉 Combat_01 的 45%、Combat_02 的 58% 后摇，
> 第二段尤其明显，表现为"点快了动画抽搐"。

**三个踩过的坑**（详见 `KnownIssues.md`）：
1. 蒙太奇资产的 `NextSectionName` 默认指向下一段 → 播完一段自动接下一段，连击窗口形同虚设
2. 窗口终点压在段边界上时，`AnimNotifyState::NotifyEnd` **不会被调用**
3. `ComboIndex` 与画面失步会导致"往回跳一段"

**验证**（编辑器世界实测）：起手 index=1/Combat_01 → 窗口内按 index=2/Combat_02 →
再按 index=3/Combat_03 → 最后一段再按忽略 → `reset_combo` 归零。
失步场景（人为 `Montage_JumpToSection` 后 index 停在 1）按攻击不再往回跳。

### 三、第三段动画重做（负责人手动）

用 Animation Mode 重调骨骼坐标后烘焙新序列替换 `Combat_03`。

**时间轴完好**（已核验）：蒙太奇总长仍 **2.1667s**，三段 0.6667 / 0.6667 / 0.8333，
5 个 Notify 位置一个未变（第三段判定窗口 1.596–1.847 仍落在 1.333–2.167 内）→ 无需返工。

### 环境
- 本次构建期间 **UBT 派生子进程全部失败**（cl/lib/link/dotnet 均秒退且无诊断），
  改用「手动 `cl.exe` + `lib.exe` + `link.exe`」完成构建；`link.exe` 需显式 export
  `WindowsSdkDir` / `WindowsSdkVerBinPath` / `WindowsSDKVersion`，否则报 `LNK1158 无法运行 mt.exe`。
  详见 `KnownIssues.md`。

### Docs
- `TaskBoard`：新增 AV-035 / AV-036 / AV-037；AV-023 转 **Done**；AV-033 转 **Verified**；**M1 转 Verified**
- `Architecture_20261024`：§3.2 职责表、§4.2 武器装配、§4.3 Tick、§6 动画链路、§7 武器子系统全量对齐；
  新增 §4.4 PlayerController 生命周期、§7.4 战斗握持偏移；
  **修正 §7.2 校准方法** —— 原文"在 BP 里改 MeshComponent 相对变换"在运行时会被
  `AttachToComponent(KeepWorldTransform)` 冲掉，是错的
- `KnownIssues`：新增 KI-016（Montage Section 链接）、KI-017（NotifyEnd 不触发）、KI-018（构建环境）；
  KI-013（握持姿势）重新生效


## [Unreleased] — 2026-10-24（**Bugfix：资产移动导致玩家完全无法操作**）

### 现象
负责人报告"无法操控角色，按键都没反应"。

### 根因
负责人在 21:34 把资产整理进新目录 `Content/ArcaneVanguard/BluePrint/`：

```
/Game/ArcaneVanguard/BluePrint/Core/BP_AV_PlayerController
/Game/ArcaneVanguard/BluePrint/Weapons/BP_AV_Weapon_Staff
```

而 `ArcaneVanguardGameMode.cpp` 里 `FClassFinder` 的路径仍是旧的
`/Game/ArcaneVanguard/Core/BP_AV_PlayerController` → 找不到 → **静默退回 C++ PC**。
C++ 类的 `DefaultMappingContext` 为空 → 没有注册任何 MappingContext → **输入全失效**。

> 武器引用没受影响：它是 BP 里的对象引用，移动资产时 UE 自动重定向了
> （实测 `WeaponActorClass` 已指向 `/Game/ArcaneVanguard/BluePrint/Weapons/BP_AV_Weapon_Staff`）。

### Fixed
- `ArcaneVanguardGameMode.cpp`：`FClassFinder` 路径更新为
  `/Game/ArcaneVanguard/BluePrint/Core/BP_AV_PlayerController`。
- **回退时不再静默**：新增 `Error` 级日志，打印预期路径 + 后果（"玩家将无法操作"）+ 修复指引。
  `DefaultPawnClass` 回退同样加了 `Warning`。
- 新增日志分类 `LogAVGameMode`。

### Verified
- 编译通过：4/4，`BUILD=0`（`build_fixpcpath_20261024.log`）。
- commandlet 实测 GameMode CDO：
  ```json
  { "player_controller_class": "BP_AV_PlayerController_C",
    "player_controller_pkg": "/Game/ArcaneVanguard/BluePrint/Core/BP_AV_PlayerController",
    "default_pawn_class": "BP_ThirdPersonCharacter_C" }
  ```

### Docs
- `Architecture_20261024` §8 新增「唯一的例外：GameMode 里的两条硬编码路径」，
  含移动资产后的失败表现对照表与当前三条资产路径。

### 教训
- **C++ 里 `FClassFinder` 的路径是脆弱的**：`ConstructorHelpers` 只接受编译期常量，无法配置化。
  移动蓝图后必须同步改 C++ 并重编译。
- **回退必须有声**：静默 fallback 让"输入失效"这种致命问题看起来像玄学。
  凡是 fallback 到"功能降级"的分支，都要打 Error 并说明后果。

## [Unreleased] — 2026-10-24（战斗状态变量 + 临时调试按键）

### Added（代码）
- `AArcaneVanguardCharacter` 新增战斗状态：
  - `UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly) bool bInCombat`（默认 `false`）
  - `SetInCombat(bool)`（`BlueprintCallable`）—— 带同值短路 + 日志
  - `ToggleCombatState()`（`BlueprintCallable`）—— **临时调试入口**
  - `IsInCombat()`（`BlueprintPure`）—— 给 AnimBP 用的稳定读取入口
- `SetupPlayerInputComponent` 末尾绑定 **`F` 键 → `ToggleCombatState()`**

### 用途
驱动 AnimBP 的 **Layered Blend per Bone**：`CombatAlpha` 从 `bInCombat` 取值，实现"上半身战斗姿态 + 下半身移动动画"。

### Verified
- 编译通过：7/7，`BUILD=0`（`build_combat_20261024.log`）。
- 反射注册确认（`ArcaneVanguardCharacter.gen.cpp`）：
  `NewProp_bInCombat` 标志位 `0x...0815` = `CPF_Edit | CPF_DisableEditOnTemplate | CPF_BlueprintVisible | CPF_BlueprintReadOnly`
  → 即 `VisibleInstanceOnly + BlueprintReadOnly`，**AnimBP 可读**。
- `is_in_combat()` 运行时返回 `False`（构造默认值正确）。
- ⚠️ 注：Python `get_editor_property('in_combat')` 在 **CDO 上读不到**，因为 `VisibleInstanceOnly`
  带 `CPF_DisableEditOnTemplate`。这是预期行为，不影响 AnimBP 读取。

### Notes
- **临时逻辑，M3 需删除**：`ToggleCombatState()`、`F` 键绑定，以及两处 `TODO(M3)` 注释已标注。
- 战斗状态目前只是 bool；M3 接入战斗系统后应改为 `GameplayTag`（`State.Combat`）或 `UCombatComponent` 驱动。

## [Unreleased] — 2026-10-24（M1 代码实装：PlayerController + 输入生命周期 + ASC 预留点）

### Added（代码）
- 新增 `Source/ArcaneVanguard/Player/ArcaneVanguardPlayerController.h/.cpp`
  - `DefaultMappingContext` / `MappingContextPriority`（从 Character 迁来）
  - `BeginPlay` 注册 IMC，`EndPlay` 成对 `RemoveMappingContext`
  - `bMappingContextAdded` 幂等保护；拿不到 LocalPlayer 时打 Warning 不崩
- `AArcaneVanguardCharacter`：
  - **移除** `DefaultMappingContext` 属性与 `AddMappingContext` 调用
  - **新增** `PossessedBy` / `OnRep_PlayerState` 空重写 + `TODO(M2)` 注释（ASC 预留点，不引入 GAS 模块）
- `AArcaneVanguardGameMode`：`PlayerControllerClass` 优先 FClassFinder `BP_AV_PlayerController`，退回 C++ 类
- `Build.cs`：新增 `PublicIncludePaths.Add(ModuleDirectory);` —— 子目录可用 `Player/XXX.h` 这类模块根相对路径

### Added（资产）
- `BP_AV_PlayerController`（`/Game/ArcaneVanguard/Core/`），父类 `AArcaneVanguardPlayerController`，`DefaultMappingContext = IMC_Default`

### Verified（`Saved/Logs/M1Verify_20261024.log`）
```
LogWorld: Bringing World ...ThirdPersonMap up for play
LogAVPlayerController: [Input] 已注册 MappingContext 'IMC_Default'（优先级 0）   ← 1 次
LogTemplateCharacter: [Weapon] 已生成并附着 'BP_AV_Weapon_Staff_C_0' 到插槽 'StickSocket'
...
LogAVPlayerController: [Input] 已移除 MappingContext 'IMC_Default'             ← 退出时 1 次
```
- 启动注册 **1 次**、退出移除 **1 次** → Add/Remove 成对，无重复注册。

### Blocked / 未执行
- **原计划的「连续进出 PIE 5 次读 MappingContext 数量」未执行**：桥接在本 `-game` 实例里拿不到真正的游戏世界
  （`load_object` 返回的 UWorld `get_num_player_controllers()` 恒为 0，`get_game_instance()` 为 None），
  无法在运行时遍历 PlayerController。改用「日志计数 + 结构分析」替代：
  唯一 `AddMappingContext` 调用点在 `PC::BeginPlay`，该函数每个 PC 生命周期只执行一次，且被 `bMappingContextAdded` 守卫。

### Live Coding 结论（教训）
- 先尝试 `LiveCoding.Compile`（负责人要求不关编辑器）→ **失败**：8 分钟后新类仍未注册。
- 根因：Live Coding 使用**现有 makefile**，新增 `.cpp` 文件 UBT 不知道；且新增 UCLASS 需要模块重载。
- **结论：新增源文件 / UCLASS 必须全量编译，Live Coding 无效。**

### ⚠️ 事故记录（工具使用）
- 结束时用 `taskkill /IM UnrealEditor.exe` 想只关 `-game` 实例，但该命令会杀掉**所有** UnrealEditor 进程，
  包括负责人正在使用的编辑器。已确认编辑器已关闭。
- 资产保存时间：`BP_AV_Weapon_Staff.uasset` 仍为 20:43（我创建时），说明**武器位置校准可能未保存**。
- 教训：区分实例要用 PID（`taskkill /PID <pid>`），不要用 `/IM` 通配。

## [Unreleased] — 2026-10-24（武器 Actor 接入：Spawn + Attach）

### Added（代码）
- 新增 `Source/ArcaneVanguard/Weapon/ArcaneVanguardWeapon.h/.cpp`：`AArcaneVanguardWeapon : AActor`
  - `MeshComponent`（`UStaticMeshComponent`，根组件）
  - `MeshAsset`（`EditDefaultsOnly`，`OnConstruction` 同步到组件）
  - 默认 `NoCollision` + 不参与导航
- `AArcaneVanguardCharacter` 新增：
  - `TSubclassOf<AActor> WeaponActorClass`（蓝图指定武器类）
  - `FName WeaponAttachSocket`（默认 `StickSocket`）
  - `TObjectPtr<AActor> SpawnedWeapon`（运行时实例，只读）
  - `SpawnAndAttachWeapon()` / `DetachAndDestroyWeapon()`（`BlueprintCallable`）
  - `BeginPlay` 自动装配

### 装配方式（按要求）
**SpawnActor + AttachActorToComponent**：
1. 取 `StickSocket` 世界变换作为生成点 → 附着后相对变换为 **Identity**，校准从零开始
2. `AlwaysSpawn`（生成点与胶囊重叠，否则会被碰撞拒绝）
3. 关闭武器碰撞
4. `AttachToComponent(GetMesh(), KeepWorldTransform, "StickSocket")`

### Added（资产）
- `BP_AV_Weapon_Staff`（`/Game/ArcaneVanguard/Weapons/`），父类 `AArcaneVanguardWeapon`，`MeshAsset = SM_Wizard_Staff_basic`
- `BP_ThirdPersonCharacter`：`WeaponActorClass = BP_AV_Weapon_Staff_C`、`WeaponAttachSocket = StickSocket`
- 备份：`BP_ThirdPersonCharacter.uasset.bak-20261024-204250`

### Verified
- **编译通过**：4/4，`BUILD=0`（`build_weapon2_20261024.log`）
  - 注：首次编译因 `#include "Weapon/ArcaneVanguardWeapon.h"` 路径未解析报 C1083，
    改为同目录相对引用 `#include "ArcaneVanguardWeapon.h"` 后通过。
- **运行时实测**（`WeaponAttach_20261024.log` + 桥接内省）：
  | 项 | 值 |
  |---|---|
  | 生成对象 | `BP_AV_Weapon_Staff_C_0` |
  | 根组件 | `MeshComponent`（StaticMeshComponent） |
  | 附着父级 | `CharacterMesh0` |
  | 附着插槽 | `StickSocket`（`DoesSocketExist = True`） |
  | 武器世界坐标 vs 插槽世界坐标 | **delta = (0.000, 0.000, 0.000)** |
  | 碰撞 | 关闭 |
- 截图：`Docs/WeaponAttach_20261024/weapon_attached_raw.png`（未校准状态）

### Docs
- `TechnicalDesign.md` 新增 **§2.5 当前代码架构**：模块结构、类职责表、单向依赖图、装配流程、实测结果、已知取舍。

### Notes
- **坐标与旋转校准由负责人执行**（本次只做连接）。
- `StickSocket` 已通过二进制扫描确认存在于 `SKE_Wizard.uasset`（偏移 8129）。
- 编译前用 `save_dirty_packages` 保存了编辑器内未保存改动，再用 `taskkill /IM`（不带 `/F`）优雅关闭，
  避免再触发上次的「恢复包」弹窗。

## [Unreleased] — 2026-10-24（制定 M0/M1 阶段任务计划）

### Docs
- 新增 `Docs/Plan_M0_M1_20261024.md`：基于 `GameDesign_V1.0.md` §8 原始定义重排 M0/M1 计划。
  - **M0 仅剩 1 项未闭合**：AV-027 人工 PIE 手感验收（回滚重编后必须重新确认）。
  - **M1 拆为 5 个任务**：AV-029 架构决策（闸门）→ AV-030 PlayerController → AV-031 输入绑定生命周期 → AV-032 ASC 预留点 → AV-033 验收。
  - 列出 **5 个需负责人拍板的决策**（含建议与理由）。
  - 关键验证点：**输入无重复绑定必须量化**（连续进出 PIE 5 次读 MappingContext 数量），不接受肉眼判断。
- `TaskBoard.md` 新增 AV-027~AV-033。

### Notes
- 计划明确 **不恢复** 已回滚的武器测试代码；M3 做普攻时按正式设计重做。
- 待负责人确认：`BP_ThirdPersonCharacter` 是否重建（建议不重建，M3 顺手清）。

## [Unreleased] — 2026-10-24（**C++ 全部回滚到第三人称模板原始状态**）

### Removed（重大）
按负责人要求，此前所有 C++ 改动均为**测试性质**，全部撤销，恢复成模板刚创建时的状态。

| 文件 | 处理 |
|---|---|
| `ArcaneVanguardCharacter.h` | **重写为模板原文** |
| `ArcaneVanguardCharacter.cpp` | **重写为模板原文** |
| `ArcaneVanguardWeaponNotifies.h` | **删除**（自建文件） |
| `ArcaneVanguardWeaponNotifies.cpp` | **删除**（自建文件） |
| `ArcaneVanguard.Build.cs` / `.cpp` / `.h` / `GameMode.*` | 未改动过，保持原样 |

**已删除的 C++ 功能**（对应任务全部标记「已回滚」）：
- 武器组件 `WeaponMesh` + `AttachWeapon()`（AV-013）
- 竖直约束 `bKeepWeaponUpright` / `WeaponGripOffset` / `UpdateWeaponUprightTransform()`（AV-018）
- 按动画名自动收杖 `WeaponHiddenAnimNames`（AV-019）
- 死亡掉杖 `DropWeapon()` / `RestoreWeapon()` / `WeaponDropAnimNames`（AV-022）
- 调试按键 1~5 `DebugAnimations` / `DebugPlayAnimation()` / `BindKey`（AV-021）
- 动画通知类 `UAnimNotify_HideWeapon` / `ShowWeapon`

### Verified
- **与引擎模板字节级一致**：从 `UE_5.4/Templates/TP_ThirdPerson/Source/TP_ThirdPerson/` 取原文，
  仅把 `TP_ThirdPerson` 替换为 `ArcaneVanguard`，7 个文件 `cmp` 全部相同（含行尾符）。
- **编译通过**：7/7 动作，0 错误，`BUILD=0`（日志 `Saved/Logs/build_revert_template_20261024.log`）。
  注：本次因源文件删除触发了模块整体重编，耗时 16 分 37 秒。
- **运行时验证**（`Saved/Logs/RevertCheck_20261024.log`）：
  - 组件列表**无 `WeaponMesh`**；唯一 StaticMeshComponent 是模板自带的 `MatineeCam_SM`（不可见）
  - 网格 `SK_Wizard` / AnimBP `AnimBP_Wizard_ThirdPerson_C` 保持
  - **0 fatal / 0 ensure / 0 error**，无孤儿属性告警（BP 中旧属性被静默丢弃）

### Added
- 测试代码存档：`Docs/Backup_TestSource_20261024/`（4 个文件的完整副本，可随时取回重做）

### Notes
- **蓝图 `BP_ThirdPersonCharacter` 未改动**（负责人只要求回滚 C++）。BP 中残留的武器属性与配置已失效，
  加载时被静默丢弃，不影响运行。
- 相关验证证据（`P1Verify_20261022/`、`WeaponHide_20261022/`、`DebugKeys_20261022/`、`KI013_FingerGrip_20261022/`）
  **全部保留**，作为「做过什么、怎么验证的」记录；重新引入武器时可直接复用标定数据。

## [Unreleased] — 2026-10-23（KI-010 关闭 + DevelopmentStatus 全量更新）

### Fixed
- **KI-010 关闭**：`KnownIssues.md` 中该行原写"待负责人确认是否切换到 `AnimBP_Wizard_ThirdPerson`"，
  但 AV-011 早已 Done（负责人已在 BP 中切换）。已改为 **已解决**，级别划除。
  本地文件与 Notion 条目同步修改。

### Changed
- **`DevelopmentStatus.md` 全量重写**（原内容停留在 2026-10-21）：
  - §2.1 玩家配置：AnimBP 更正为 `AnimBP_Wizard_ThirdPerson`；补充骨架 `SKE_Wizard` 与武器挂载信息
  - §3 已完成任务：补入 AV-018~AV-026（P1 约束 / 自动收杖 / 资产溯源 / 调试按键 / 死亡掉杖 / 交接文档 / 资产清单 / Notion 库）
  - §5 阻塞项：新增 **B5 无普通攻击连击动画**（AV-023）
  - §9 下一步：明确 AV-010 为唯一优先项，并列两项可并行的无外部依赖任务
  - **新增 §10「已闭环的历史风险」**：KI-010 / KI-014 / 武器穿身 / 死亡握杖 / 骨骼兼容 五项均已解决
- Notion 条目「DevelopmentStatus — 项目状态」状态由 **待更新 → 有效**，内容同步为最新版。

## [Unreleased] — 2026-10-23（Notion 文档库去重）

### Fixed
- 文档库出现重复条目：5 份文档各有 2~4 个副本（标题不同、内容略有差异，`本地路径` 指向同一文件）。
  根因是多轮导入叠加，每轮都新建条目而非更新既有条目。
- **删除 11 个重复条目**（已进 Notion 回收站，可 `restore_pages` 恢复）：
  | 文档 | 原条目数 | 保留 | 删除 |
  |---|---|---|---|
  | `KnownIssues.md` | 4 | 完整版 | 3 |
  | `TaskBoard.md` | 3 | 完整版 | 2 |
  | `DevelopmentStatus.md` | 3 | 完整版 | 2 |
  | `DecisionLog.md` | 3 | 完整版 | 2 |
  | `TestPlan.md` | 3 | 完整版 | 2 |
- 保留原则：**留内容最完整的版本**（含全部表格与细节），删精简版；同时修正保留版的「分类 / 里程碑」错填
  （`TaskBoard` 与 `DevelopmentStatus` 原先被填成「策划设计」，已改回「技术设计」）。
- 去重后校验：**22 条 = 22 份本地文档**，`本地路径` 全部唯一。

### Notes
- 教训：**更新文档时应当修改既有条目，不要新建**。新建会绕过「本地路径唯一」的约束。

## [Unreleased] — 2026-10-23（Notion 文档库建立 + 全量文档同步）

### Docs
- 在 Notion 页面「2 项目 · 作品集 / Arcane Vanguard-奥术先锋」下建立数据库
  **📚 工程文档库**（`d1cc1ec0b4fe4fb986d3025695766abf`，data source `collection://e7add142-2cbd-4131-8523-63e9a8e47163`）。
- 四个管理维度：**分类**（策划设计/技术设计/资产管理/问题跟踪/测试验证/决策记录/交接归档）、
  **状态**（有效/待更新/已归档）、**里程碑**（M0–M8）、**本地路径**（双向核对）。
- **22 份文档全部入库**：16 份根目录 md + 6 份验证目录 README。
- 原本平铺的两个子页面（`完整项目策划案`、`ArtAssetPlan`）**已移动进数据库**，避免内容重复。
- `ArtAssetPlan` 页面内容已更新到最新版（含 Dark Knight 敌人采购、现状统计修正）。

### Added（强制规范）
- `AgentRules.md` §5 新增 **Notion 文档同步**硬约束：
  新建 md → 必须建库条目；修改 md → 必须同步更新条目；本地路径不得留空；过时标「待更新」不删除。

### Verified
- 库内条目数与本地文档数一致（22 = 22）。

## [Unreleased] — 2026-10-23（骨架兼容性决策：不切换 Manny；KI-013 补 weapon_r 握点）

### 骨架决策：不把巫师切换成 UE5 Manny 骨架（AV-023 相关，实测）

- 实测（引擎 `SKM_Manny.uasset` 名称表 vs `SKE_Wizard` 70 骨全量枚举）：**`SKE_Wizard` 是 Manny 骨架的严格命名子集**（70/70 命中，Manny 仅多 `spine_04/05`、`neck_02`；`weapon_l/r` 两边都有）。
- 结论：保持 `SKE_Wizard`。UE4 Mannequin / UE5 Manny compatible 的包对本骨架等价；Mixamo 是独立骨架，换不换 Manny 都要重定向。
- 可扩展方案：一次性 IK Retargeter（Manny → SKE_Wizard，Humanoid 自动链映射，近乎恒等），之后任何来源动画批量接入。
- 决策文档：`Docs/SkeletonDecision_20261023/README.md`；`TechnicalDesign.md` §5.8.2。

### KI-013：新增 weapon_r 握点优化（零风险，1 分钟改一个字段）

- 实测发现 `SKE_Wizard` 有 `weapon_r`/`weapon_l` 骨骼（索引 46/24，父骨 hand_r/hand_l）。
- `weapon_r` 相对 `hand_r` 局部偏移 (-8.32, +2.55, +0.60)cm；运行时两帧冻帧验证**随 hand_r 刚性运动（局部漂移 0.00cm）**。
- 效果：法杖轴从"距手指 8~11cm"变为"距指根 4.5~6cm"（每根手指近 3~5cm），显著改善"没握住"观感，且为手指层提供正确目标。
- 操作：BP_ThirdPersonCharacter → Class Defaults → WeaponAttachSocket 改 `weapon_r`（回滚即改回）。
- 验收脚本已更新（挂点变更自动要求重测基线，防假 PASS）。证据在 `Docs/KI013_FingerGrip_20261022/`。

## [Unreleased] — 2026-10-22（KI-013 实测标定 + 修复指南；AV-023 候选调研）

### KI-013：Cast_Spell_01 杖身穿掌（进行中，等编辑器手动操作）

- **实测标定**（`-game` 实例 + 桥接，只读，未改任何资产）：
  - `SKE_Wizard` 二进制名称表扫描：70 个目标骨骼名全部存在，UE4 Mannequin 命名（`index_01_r`…`thumb_03_r`）。
  - 手指骨局部坐标系：指向指梢方向 ≈ 局部 **-X**；弯曲轴实测（局部系）：index≈-Y、middle/ring/pinky≈+Y、thumb≈-Y/-Z。
  - **待机 vs Cast_Spell_01@1.13s 的手指局部轴点积 0.96~1.00 → 动画不驱动手指骨骼**，一组固定旋转值适用所有姿势。
  - 各手指弯向法杖轴的符号差异经几何核实为正确（五指分布在法杖轴四周，绕杖手势）。
- **产出**：
  - 修复指南：`Docs/KI013_FingerGrip_20261022/README.md`（15 个 Transform (Modify) Bone 节点的完整步骤 + 实测旋转值表 + 微调/回滚说明）。
  - 验收脚本：`Saved/Scripts/ki013_verify_after.py`（指尖到法杖轴距离 vs 基线，量化判定）。
  - 证据：`Docs/KI013_FingerGrip_20261022/`（3 个测量 JSON + 施法手部截图/放大图）。
- **备份**：`AnimBP_Wizard_ThirdPerson.uasset.bak-20261022-111500`（改前已完成）。
- **待办**：需负责人在编辑器里按指南手动加节点（AnimBP 图无法脚本编辑），完成后由 AI 跑验收脚本。

### AV-023：连击动画候选调研（等负责人选型，未购买/下载）

- Fab 实搜候选（价格/兼容性见 `TechnicalDesign.md` §5.8）：
  **Essential Magic Staff Animation Pack**（$37.61，UE4.26-4.27/5.0-5.7，更新日志提及 `ik_hand_gun` 骨骼 —— 与本骨架命名一致，最契合）、
  **Mage with Staff Animations**（$9.66，UE4.27/5.0-5.7）、
  **Simple Magic Staff Animation Pack**（$26.86，仅 UE5 Manny 骨架，需重定向）、
  **Melee Combo Vol 1/2/3**（$34.99/个）、**Greatsword Combo Animation Pack**（$15.99）、
  **Sword Animation Pack 35+**（$19.99）；免费备选 Mixamo（重定向链路已验证）。
- 购买/下载属 L4，必须由负责人执行。

### 环境备忘（新增踩坑）

- **-game 实例里取世界的正确方法**：`unreal.load_asset("/Game/ThirdPerson/Maps/ThirdPersonMap")` 直接返回 UWorld，
  再 `unreal.GameplayStatics.get_all_actors_of_class(world, ...)`。传 `None` 当 world context 会静默返回空（日志有 RuntimeWarning）。
- `SkeletalMeshComponent` Python 侧可用：`get_socket_location/quaternion/rotation`（骨骼名可当 socket 名用，有 fallback）、
  `get_bone_transform`、`transform_to/from_bone_space`；但 `Skeleton`/`SkeletalMesh` 资产对象**没有**骨骼列表 API（`num_bones`/`get_bone_name` 均不存在）。
- commandlet 里 `print()` 输出不进日志也不进 stdout —— 结果一律写 JSON 文件。

## [Unreleased] — 2026-10-23（敌人资产采购登记）

### Added
- `AssetRegister.md` 新增「已采购资产登记」：`Dark Knight - Male and Female`
  （<https://fab.com/s/b03de798c900>，敌人 Arcane Guard）。
  登记了骨架兼容性（Mannequin/Quinn）、52 个动画清单、面数、材质能力、集成要点与已知取舍。
- `ArtAssetPlan.md` §1.1 更新：敌人从 ❌ 改为 ✅ 已采购，附集成待办（AnimBP / socket / 材质实例）。
- 明确记录：该包 Attack×3 为**剑系**动画，**不可用于玩家**；玩家连击仍走
  `Mega Spear Animation Pack`（免费）或 `Essential Magic Staff Animation Pack`（$37.61）。

## [Unreleased] — 2026-10-23（美术资产全景需求清单）

### Docs
- 新增 `Docs/ArtAssetPlan.md`：V1.0 前期美术资产全景清单。
  - 现状实测：`Content/` 下**敌人 0 / VFX 0 / 音频 0 文件 / 项目 UI 0 / 仅模板地图**，
    玩家角色与武器已齐，动画只缺普通攻击连击。
  - 按 M3–M8 分 **P0 阻断 / P1 必要 / P2 增强** 三级，每项含用途、规格、来源建议。
  - 附搜集渠道速查（Fab / Mixamo / Kenney CC0 / Sonniss / Freesound / Poly Haven）、
    授权红线、**4 批搜集顺序**（每批到货先登记 AssetRegister 再买下一批）。

## [Unreleased] — 2026-10-22（交接文档）

### Docs
- 新增 `Docs/HANDOFF_20261022.md`：本对话中所有模型/动画改动的完整交接，
  含环境与工具、改动清单、实测数据、**10 条踩坑记录**、未完成项（KI-013 / AV-023 / 授权风险）
  与一段**可直接复制的交接提示词**。

## [Unreleased] — 2026-10-22（武器规则细化：隐藏名单 + 死亡掉杖）

### Changed
- `WeaponHiddenAnimNames` 改为 3 个：`Cast_Spell_02` / `Magic_Attack` / `Magic_Heal`
  （**`Cast_Spell_01` 保留法杖**，按负责人实测结论）。
- 新增 `WeaponDropAnimNames`（默认 `Death_Forward`）。

### Added
- `DropWeapon()` / `RestoreWeapon()`（`BlueprintCallable`）：
  - 掉落：`DetachFromComponent(KeepWorldTransform)` → 开 `QueryAndPhysics` + `PhysicsActor` 碰撞档
    + `SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore)`（不与角色胶囊互碰，否则会被弹开）
    → `SetSimulatePhysics(true)`
  - 回收：关物理 → 关碰撞 → 重新 `AttachToComponent(hand_r)` → 恢复可见
- `UpdateWeaponDropFromAnimation()` + `GetCurrentAnimationName()`（抽出公共函数，显隐与掉落共用）
- `UpdateWeaponUprightTransform()` 增加 `bWeaponDropped` 早退——**掉落期间绝不能再用附着约束覆盖物理**

### Verified（`Saved/Logs/DropWeapon_20261022.log`）
```
[Weapon] 脱手掉落，物理模拟已开启
  attach parent = NONE | simulating = True | collision = QUERY_AND_PHYSICS
  rot: (0, 90, 0) → (45, 146.9, 90)   ← 已自然倒伏
  loc: z = 8.3（躺在地面）
[Weapon] 已收回手上
  parent = CharacterMesh0 | socket = hand_r | simulating = False
[DebugAnim] 播放 [1] Cast_Spell_02 → [Weapon] visible = false → 2.167s 后回到状态机 → visible = true
```

## [Unreleased] — 2026-10-22（调试按键：动画逐个播放）

### Added
- `AArcaneVanguardCharacter`：
  - `TArray<TObjectPtr<UAnimSequence>> DebugAnimations`（BP 可编辑，已填 5 个）
  - `bool bEnableDebugAnimKeys`（默认 true，**发布前需关闭**）
  - `DebugPlayAnimation(int32)` / `DebugStopAnimation()`（`BlueprintCallable`，便于自动化验证）
  - `UpdateDebugAnimationState()`：每帧检查 `GetSingleNodeInstance()->IsPlaying()`，播完自动切回 AnimBP
  - `SetupPlayerInputComponent` 里 `BindKey` 绑定 **1~5** 键
- BP 配置：`Debug Animations` = Cast_Spell_01 / Cast_Spell_02 / Magic_Attack / Magic_Heal / Death_Forward

### Verified（日志时间线）
```
[DebugAnim] 播放 [2] Magic_Attack        02:33:21.422
[Weapon] visible = false                 02:33:21.429
[DebugAnim] 回到 AnimBP 状态机            02:33:24.760   ← 3.338s ≈ 3.333s
[Weapon] visible = true                  02:33:24.760

[DebugAnim] 播放 [0] Cast_Spell_01       02:34:04.709
[DebugAnim] 回到 AnimBP 状态机            02:34:06.986   ← 2.277s ≈ 2.267s
（无 [Weapon] 切换，该动画不在收杖名单，符合预期）
```
- 编译通过（8/8，`BUILD=0`）。
- 说明文档：`Docs/DebugKeys_20261022/README.md`

## [Unreleased] — 2026-10-22（资产溯源 + 武器自动显隐）

### Found（资产溯源，重要）
- 对 19 个资产做二进制字符串扫描：
  - **9 个含 `mixamo.com`**：`Cast_Spell_01/02`、`Magic_Attack`、`Magic_Heal`、`Death_Forward`、`Run_Forward/Back/Left/Right`
  - **19 个全部含 `.!VISIT HTTPS://WWW.UECANDY.COM!`**
- 结论：**技能动画是 Mixamo 通用人形动画**，录制时手上没有武器；整包来自 **UECandy** 转载站。
  这解释了此前所有症状（无握持、172° 手腕翻转、`Magic_Attack` 是双手前推）。
- 已同步 `AssetRegister.md`（含授权风险）与 `TechnicalDesign.md` §5.7。

### Added
- `SetWeaponVisible(bool)` / `IsWeaponVisible()`（`BlueprintCallable`）
- `UAnimNotify_HideWeapon` / `UAnimNotify_ShowWeapon`
  （`Source/ArcaneVanguard/ArcaneVanguardWeaponNotifies.h/.cpp`）——供以后在 Notify 轨道精确控制
- `TArray<FName> WeaponHiddenAnimNames`（BP 可编辑，默认 `Anim_Wizard_Magic_Attack`）
  + `UpdateWeaponVisibilityFromAnimation()`：每帧按当前动画名判定，**Montage 与 SingleNode 都覆盖**

### Verified
- 编译通过（4/4，`BUILD=0`）。
- 运行时实测：
  | 状态 | `is_visible()` | `hidden_in_game` |
  |---|---|---|
  | 待机 | true | false |
  | 播 `Magic_Attack` | **false** | **true** |
  | 切回待机 | true | false |
- 截图：`Docs/WeaponHide_20261022/magic_attack_before_after.png`

### Blocked / Notes
- 尝试把通知直接写进动画资产失败：`AnimationBlueprintLibrary` 在 commandlet 与编辑器里**都未暴露给 Python**，
  `UAnimSequenceBase::Notifies` 又是 `protected` 不可写。故改用「按动画名判定」的运行时方案。
- **KI-013 未解决**：`Cast_Spell_01` 的杖身穿掌属于"手没握住"，不能靠隐藏解决。

## [Unreleased] — 2026-10-22（P1 视觉验收通过）

### Verified
- `-game` 实跑 + 运行时冻帧，三姿势全部通过：

| 姿势 | `hand_r` 世界旋转 | 法杖世界旋转 | 判定 |
|---|---|---|---|
| 待机 | 75.86 / 1.70 / 118.70 | **0 / 90 / 0** | 竖直 ✓ |
| `Cast_Spell_01` | -69.29 / -90.45 / 45.68 | **0 / 90 / 0** | 竖直 ✓（修复前 roll -162.54） |
| `Magic_Attack` | -34.70 / 68.12 / 104.38 | **0 / -94.16 / 0** | 竖直 ✓ |

- 握点校验：`staff_origin - hand_r = (0, 0, -96)`，与 `WeaponGripOffset` 完全一致，X/Y 偏差 0。
- 证据：`Docs/P1Verify_20261022/`（README + 三姿势并排图 + 前后对比图）。

### Result
- **KI-014（法杖颠倒）已解决。**
- **KI-013（手指没握住）仍在**——P1 只解决朝向，不解决手指姿态。

## [Unreleased] — 2026-10-21（第七轮：实现 P1 竖直约束）

### Added（代码）
- `AArcaneVanguardCharacter` 新增：
  - `bool bKeepWeaponUpright`（默认 **true**）—— 每帧强制武器竖直
  - `float WeaponGripOffset`（默认 **96**）—— 握点距杖底的距离，沿武器局部 +Z
  - `Tick()`（`TG_PostUpdateWork`，确保在动画求值之后执行）+ `UpdateWeaponUprightTransform()`
- 算法：握点 `(0,0,GripOffset)` 贴合 `hand_r`，朝向 `(0, ActorYaw, 0)`；
  反推组件原点 `GripWorld - RotateVector(GripLocal)`，再写
  `WeaponMesh->SetRelativeTransform(DesiredWorld.GetRelativeTransform(SocketWorld))`。
- 关闭 `bKeepWeaponUpright` 即回到 `WeaponRelativeTransform` 的静态偏移（P3 到位后可关）。

### Verified
- **全量编译通过**：clean build 11/11，0 错误，`BUILD=0`。日志 `Saved/Logs/build_p1_20261021.log`。
- **无头校验通过**（commandlet）：
  - `keep_weapon_upright = true`
  - `weapon_grip_offset = 96.0`
  - `weapon_attach_socket = hand_r`
  - `weapon_static_mesh = SM_Wizard_Staff_basic`
  - 数学复核：给定手位 `(100,200,50)`、握点偏移 96，反推原点 `(100,200,-46)`，回代握点 = `(100,200,50)` ✓
- **视觉验证未执行**：执行时用户屏幕处于锁屏（`LogonUI` 运行中），
  带渲染截图会出黑图或触发 `DXGI_ERROR_DEVICE_HUNG`，故推迟到解锁后。

### Fixed（工具链）
- 增量编译曾出现"4 个编译动作在 0.06 秒内全部失败且无任何编译器输出"。
  根因是 PCH/依赖图状态失配（UHT 重生成后 makefile 未正确失效）。
  **`-Clean` 后重建即恢复**，与代码无关。

## [Unreleased] — 2026-10-21（第六轮：法杖颠倒根因定位）

### Found（实测，非推断）
- 用户指出施法时法杖上下颠倒，实测确认：
  - 待机：`hand_r` 旋转 (73.96, 124.27, 100.07)，法杖世界 roll **-3.87** → 水晶朝上 ✓
  - `Cast_Spell_01`(t=1.13)：`hand_r` 旋转 (-69.29, -90.45, 45.68)，法杖世界 roll **-162.54** → 水晶朝下 ✗
  - **手腕世界旋转差 = 172.44°**（四元数夹角）
- 结论：法杖是 `hand_r` 刚体子节点，手腕转 172° 法杖必然跟着翻。
  **任何固定偏移都无法同时让两个姿势水晶朝上**——保持现值则施法倒，翻转 180° 则待机倒。
- 这不是参数设错，是"没有持杖动画"的必然结果。

### Proposed
- **P1 锁定竖直**：每帧重算法杖变换，握点 `(0,0,96)` 贴合 `hand_r`、朝向竖直（yaw 跟角色）。
  相对变换 = `inverse(socketWorld) * desiredWorld`。约 1 小时，代价是前刺动作不跟随前倾。
- P2 按姿势切偏移（维护差，不推荐）；P3 等持杖动画（治本）。

## [Unreleased] — 2026-10-21（第五轮：方案 B 握持验证）

### Verified（纯运行时，未改任何资产）
- 用 `AnimSingleNodeInstance` + 冻结帧的方式，把 4 个施法动画各截一帧：
  `Cast_Spell_01`(t=1.13) / `Cast_Spell_02`(t=1.08) / `Magic_Attack`(t=1.67) / `Magic_Heal`(t=1.33)。
- **结论：这套资产的动画没有握持姿势。** 手指伸直，法杖从掌心穿过；
  法杖位置本身正确（贴手心、随手臂移动、无漂移）。
- 施法时法杖会被手腕带动改变朝向（抬起时斜向上、前刺时近水平），属刚性附着的正常行为。
- 证据与复现步骤：`Docs/GripTest_20261021/README.md` + 5 张截图。

### 方法记录
- 相机需 `do_collision_test = False`，否则弹簧臂被墙体挤进角色内部，构图不可用。
- `play_animation` 会切到 `AnimSingleNodeInstance`，覆盖 AnimBP；`set_play_rate(0)` + `set_position(t)` 可稳定冻帧。

### 结论对方案的影响
- A1/A2（换动画包 / Mixamo）**不保证**解决问题——多数通用动画同样是空手手型。
- 可控解法只剩两条：**A3 自制握持动画**，或 **A4 在 AnimBP 加手指弯曲层**。

## [Unreleased] — 2026-10-21（第四轮：还原 DemoRoom 蓝图 + 挂载复核）

### Restored
- 按原路径还原 `Content/Wizard/Maps/DemoRoom/ThirdPersonBP/Blueprints/ThirdPersonCharacter.uasset`（121,305 B）。
  来源备份：`Docs/AssetBackup_2026-10-21/Wizard_Maps_to_delete/`（162 文件 / 245 MB）。
  对应资产路径：`/Game/Wizard/Maps/DemoRoom/ThirdPersonBP/Blueprints/ThirdPersonCharacter`。

### Found（关键，推翻了原假设）
- 该蓝图**没有任何武器组件**。组件只有 `CollisionCylinder`(Capsule 96/42)、`Arrow`、
  `CharMoveComp`(walk 600 / jump 600)、`CharacterMesh0`(`SK_wizard` + `AnimBP_Wizard_ThirdPerson`)。
  **连相机和 SpringArm 都没有** —— 它是展示用 NPC，不是可操作角色。
- 全库搜索 `SM_Wizard_Staff`：**只有我改过的 `BP_ThirdPersonCharacter` 引用过它**，
  其余命中都是法杖资产自身。→ **原始工程从未把武器挂到任何角色上，不存在可参照的"正确实现"**。
- `SM_Wizard_Book` 同样只被自身引用。

### Corrected
- 撤回"法杖会摆动"的结论。间隔 14 秒两次采样：手→杖距离均为 **96.8907**（四位小数一致），
  法杖世界高度仅变 0.29 → **刚性附着，无漂移**。早先 z≈153 是角色站在高台上所致。
- 遗留问题重新定性：挂载正确，缺的是**握持姿势**（待机动画是张开下垂的手型）。

### Known Issue（新增）
- 运行时偶发 `DXGI_ERROR_DEVICE_HUNG`（GPU 5s 超时，TDR），发生在 Lumen 的
  `FilterRadianceWithGather` / `ShortRangeAO_ScreenSpace` pass，显存仅用 397MB / 7249MB。
  与武器挂载无关，4 次运行中 1 次触发，重试即恢复。日志 `Saved/Logs/WeaponDiag_20261021.log`。

## [Unreleased] — 2026-10-21（第三轮：玩家武器挂载）

### Added（代码）
- `AArcaneVanguardCharacter` 新增武器组件：
  - `UStaticMeshComponent* WeaponMesh`（`VisibleAnywhere`，挂在角色 Mesh 上）
  - `FName WeaponAttachSocket`（默认 `hand_r`；因为 `SKE_Wizard` 有 0 个 socket，直接用骨骼名）
  - `FTransform WeaponRelativeTransform`（蓝图里可视化调握持偏移）
  - `AttachWeapon()`：`BeginPlay` 时解析挂点，**socket 与骨骼两种都接受**；找不到时打 Warning 并退回 Mesh 根
  - `GetWeaponMesh()` 访问器
- 文件：`Source/ArcaneVanguard/ArcaneVanguardCharacter.h` / `.cpp`

### Changed（资产，可回滚）
- `BP_ThirdPersonCharacter`：给 `WeaponMesh` 指定 `SM_Wizard_Staff_basic`，
  写入握持变换（loc `-94.38/16.23/14.74`，rot `60.83/-88.05/103.05`），挂点 `hand_r`。
  - 备份：`Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset.bak-20261021-213907`
  - 回滚：把备份覆盖回原文件名（同时会丢掉武器配置）

### Verified
- **编译通过**：7/7 动作，0 错误 0 警告，3.90s（VS 2022 14.38.33145）。
- **运行时挂载验证通过**：武器组件父级 `CharacterMesh0`、socket `hand_r`、
  武器世界坐标与 `hand_r` 骨骼坐标**距离 0.0**、`is_visible() = True`。
- **视觉验证**：法杖竖直、水晶过头顶、杖尾接近膝盖（`Saved/Screenshots/weapon_final.png`）。
- **UHT 通过**：`Parsing headers ... Total of 3 written`。

### Found
- `SKE_Wizard` **socket 数为 0**；骨架是 **UE4 Mannequin 命名体系**（70 骨骼，含 `ik_hand_gun` 等 IK 骨）
  → 武器可直接挂 `hand_r` 骨骼；**UE4 Mannequin 动画包可按骨骼名 1:1 使用**，
  而 **Mixamo 骨骼名不匹配，必须走 IK Retargeter**（此前"Mixamo 可直接重定向"的说法已更正）。
- **握持摆动**：因挂点是骨骼 + 95cm 杠杆臂，手部姿态变化时法杖会随之摆动
  （两次运行杖尾 z 分别为 ≈6 与 ≈153）。根治需正确的握持姿势动画。

## [Unreleased] — 2026-10-21（第二轮：M0 编译与运行时验证）

### Verified
- **C++ 全量编译通过**：`Build.bat ArcaneVanguardEditor Win64 Development -Clean` + 增量重建，
  11/11 动作成功，0 错误 0 警告，75.57s，退出码 0。
  工具链 `VS 2022 14.38.33145` + `Windows SDK 10.0.26100.0`。
  日志：`Saved/Logs/UBT_clean_build_20261021-2122.log`
- **运行时冒烟通过**：`-game` 带渲染实跑 110s，玩家 Pawn 生成、GameMode 正确、58 Actor、世界持续 tick、无 fatal/ensure。
  日志：`Saved/Logs/GameSmoke_RHI_20261021.log`
- **玩家角色配置已内省**：网格 `SK_wizard`、相机 `FollowCamera`、SpringArm 400、walk 500、jump 700、cloth_blend 1.0。

### Found
- **KI-010（P1）**：玩家网格已是 `SK_wizard`，但 `anim_class` 仍是 `ABP_Quinn_C`（Mannequin AnimBP），
  项目自带的 `AnimBP_Wizard_ThirdPerson` 未使用 → 巫师自带的 17 个动画（含 4 个施法动作）全部用不上。
- **KI-011（P2）**：`-nullrhi` 在本工程会导致静态网格就绪等待卡死，运行时冒烟必须带渲染。

### Changed（文档）
- `DevelopmentStatus.md`：新增 §2.1 玩家角色实际配置、M0 验收对照改为实测结论。
- `TechnicalDesign.md`：新增 §5.5 玩家网格实际挂载状态；§5 补充动画清单与缺口。
- `TestPlan.md`：新增 4 条实测记录与 §3.1「本工程踩到的测试方法坑」。
- `KnownIssues.md`：新增 KI-010 / KI-011。
- `TaskBoard.md`：AV-007 转 Verified，新增 AV-011（AnimBP 决策）、AV-012（Wizard 许可）。
- `AssetRegister.md` / `DecisionLog.md`：ADR-003 更新为「Wizard = 玩家模型」。

### Notes
- 本轮**未修改任何代码、蓝图或资产**。唯一落盘写入是编译产物与日志。
- 编译前已备份二进制到 `Saved/_BinBackup_20261021/`。

## [Unreleased] — 2026-10-21（第一轮：文档基线）

### Changed
- `Config/DefaultGame.ini`：`ProjectName` 由 `Third Person Game Template` 改为 `Arcane Vanguard`。
  - 备份：`Config/DefaultGame.ini.bak-20261021-205004`
  - 回滚：把备份覆盖回 `Config/DefaultGame.ini`
  - 影响：仅影响项目显示名/打包名，不影响代码与资产引用

### Decided
- ADR-002：版本控制**暂缓**（负责人决定，正在修复 Wizard 资产）。补偿措施：`.bak` 备份 + 禁止 L3 操作。
- ADR-003：`Content/Wizard/` **暂不动**，仅登记为"待确认"，M5 前必须给出结论。

### Added
- `Docs/GameDesign_V1.0.md`：V1.0 策划案归档，作为玩法与验收的唯一权威源。
- `Docs/ProjectBrief.md`：项目目标、范围、不做项、质量优先级。
- `Docs/AgentRules.md`：Agent 行为规范（十条规则、权限等级 L0–L4、标准流程 A–E、重大变更清单、本工程硬约束）。
- `Docs/TechnicalDesign.md`：现状架构（已核实）+ 目标架构（计划）+ 与策划案的差距分析。
- `Docs/DevelopmentStatus.md`：环境、编译状态、阻塞项、M0 验收对照、下一步唯一优先任务。
- `Docs/TaskBoard.md`：M0–M8 里程碑与 AV-001–AV-010 任务状态。
- `Docs/AssetRegister.md`：资产来源与许可登记（含 `Content/Wizard` 待确认项）。
- `Docs/DecisionLog.md`：ADR-001（已接受）与 ADR-002–008。
- `Docs/TestPlan.md`：测试分层、最低测试矩阵、证据要求。
- `Docs/KnownIssues.md`：KI-001–KI-009。

### Notes
- 本批次**未修改任何 C++ 代码、蓝图或资产**，仅新增文档 + 1 处配置文本。
- 未建立 Git 仓库（ADR-002）。

