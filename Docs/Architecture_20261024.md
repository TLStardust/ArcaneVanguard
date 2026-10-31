# 代码架构（2026-10-24 基线，2026-10-27 更新）

- 性质：**现状架构**，只写磁盘上真实存在的东西，不含计划
- 基线：第三人称模板 + 武器 Actor 子系统 + 项目 PlayerController（M1）+ GAS 骨架与伤害链路（M2/M3）
- 2026-10-26 更新：补 §4.4 PlayerController 生命周期、§6.1 战斗动画层、§6.2 普攻连击蒙太奇、
  §7.4 战斗握持偏移；**修正 §7.2 校准方法**；对齐 §3.2 / §4.2 / §4.3 / §6 / §7
- 2026-10-27 更新：新增 **§7.5 伤害链路**（ADR-005 落地）；§2.2 源文件树补 `AbilitySystem/` `Enemy/`；
  §3.1 / §3.2 补 4 个新类；§9 预留表转为"已实装"
- 2026-10-27 16:20 更新：**§7.4 按当前 90cm 偏移重测**（握点 64.1% → 18.2%，前段 69.7 → 158.9cm）；
  全篇清除"可以在蓝图里改 MeshComponent 相对变换来校准"的错误说法（运行时会被 Attach 冲掉）
- 配套：`TechnicalDesign.md`（§2 现状 / §3 目标）、`Plan_M0_M1_20261024.md`

---

## 0. 一句话概括

**第三人称模板骨架 + 独立的武器 Actor + 项目 PlayerController + GAS 属性与伤害链路。**

M1 建立了角色扩展点（Controller 分层 + 输入生命周期）与战斗表现层（武器 + 三段连击）；
M2 接入了 GAS 骨架（ASC + AttributeSet）；M3 打通了「普攻 → 球扫判定 → 唯一入口扣血 → 敌人死亡」。
**尚无 AI、技能、UI。**

---

## 1. 分层总览

```
┌─────────────────────────────────────────────────────────┐
│ 配置层   Config/DefaultEngine.ini / DefaultGame.ini     │
│          （模块、默认 GameMode、渲染、输入默认值）        │
├─────────────────────────────────────────────────────────┤
│ 资产层   Content/                                        │
│          ├── ThirdPerson/     BP_ThirdPersonCharacter    │
│          │                    IMC_Default / IA_*         │
│          │                    ThirdPersonMap             │
│          ├── Wizard/          SK_wizard / SKE_Wizard     │
│          │                    AnimBP_Wizard_ThirdPerson  │
│          │                    17 个动画 / 5 把武器        │
│          ├── ArcaneVanguard/  BP_AV_Weapon_Staff         │
│          ├── Characters/      Mannequin 模板资产（未使用） │
│          └── LevelPrototyping/ 灰盒件                    │
├─────────────────────────────────────────────────────────┤
│ 代码层   Source/ArcaneVanguard/                          │
│          Character / GameMode / PlayerController /       │
│          Weapon / Animation / AbilitySystem / Enemy      │
├─────────────────────────────────────────────────────────┤
│ 引擎层   UE 5.4.4                                        │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 模块与依赖

### 2.1 模块清单

| 项 | 值 |
|---|---|
| 模块名 | `ArcaneVanguard`（Runtime / Default） |
| Build.cs 依赖 | `Core`、`CoreUObject`、`Engine`、`InputCore`、`EnhancedInput`、**`GameplayAbilities`**、**`GameplayTags`**、**`GameplayTasks`** |
| `.uproject` 插件 | `ModelingToolsEditorMode`（Editor only）、`PythonScriptPlugin`（MCP 桥接）、**`GameplayAbilities`**（M2 启用） |
| Target | `ArcaneVanguard`（Game）、`ArcaneVanguardEditor`（Editor），均为 Win64 Development |
| BuildSettings | `V5` / `IncludeOrderVersion = Unreal5_4` |

> **注意**：`GameplayAbilities.uplugin` 的 `EnabledByDefault = false` ——
> 光在 `Build.cs` 加模块名**不够**，必须在 `.uproject` 里显式启用插件，否则编译直接失败。

### 2.2 源文件结构

```
Source/
├── ArcaneVanguard.Target.cs
├── ArcaneVanguardEditor.Target.cs
└── ArcaneVanguard/
    ├── ArcaneVanguard.Build.cs
    ├── ArcaneVanguard.cpp / .h              IMPLEMENT_PRIMARY_GAME_MODULE
    ├── ArcaneVanguardCharacter.h / .cpp     ACharacter 子类（移动/武器/连击/判定）
    ├── ArcaneVanguardGameMode.h / .cpp      AGameModeBase 子类
    ├── ArcaneVanguardPlayerController.h/.cpp APlayerController 子类（M1）
    ├── AbilitySystem/                       （2026-10-26 新增，M2/M3）
    │   ├── ArcaneVanguardAttributeSet.h/.cpp      Health / MaxHealth
    │   ├── ArcaneVanguardGameplayEffects.h/.cpp   UGE_ArcaneDamage（原生 GE）
    │   └── ArcaneVanguardCombatStatics.h/.cpp     ApplyDamage（ADR-005 唯一入口）
    ├── Animation/
    │   └── AVCombatNotifyStates.h / .cpp    连击窗口 + 判定窗口（2026-10-26 新增）
    ├── Enemy/                               （2026-10-27 新增，M3）
    │   └── ArcaneVanguardEnemy.h / .cpp     ACharacter 子类（挨打 / ragdoll 死亡）
    └── Weapon/
        └── ArcaneVanguardWeapon.h / .cpp    AActor 子类（2026-10-24 新增）
```

> 目录按职责划分，遵循策划案 §5.1「仅在类数量或职责边界足以证明需要时创建」。
> `AbilitySystem/` 是 M2 引入 GAS 时才建的 —— 在此之前只有 1 个类，不值得单独开目录。

---

## 3. 类清单与职责

### 3.1 继承关系

```
AActor
 ├── ACharacter
 │    ├── AArcaneVanguardCharacter        ← 玩家角色
 │    └── AArcaneVanguardEnemy            ← 敌人基类（M3 新增）
 └── AArcaneVanguardWeapon               ← 武器载体

AGameModeBase
 └── AArcaneVanguardGameMode             ← 默认 GameMode

APlayerController
 └── AArcaneVanguardPlayerController     ← 项目 PlayerController（M1 新增）

UGameplayEffect
 └── UGE_ArcaneDamage                    ← 原生伤害 GE（M3 新增）

UBlueprintFunctionLibrary
 └── UArcaneVanguardCombatStatics        ← ApplyDamage 唯一入口（M3 新增）

UAttributeSet
 └── UArcaneVanguardAttributeSet         ← Health / MaxHealth（M2 新增）

UAnimNotifyState
 ├── UAnimNotifyState_AVComboWindow       ← 连击窗口
 └── UAnimNotifyState_AVAttackWindow      ← 判定窗口
```

**GAS 接口**：`AArcaneVanguardCharacter` 与 `AArcaneVanguardEnemy` 都实现
`IAbilitySystemInterface` —— 这是「谁能被 ApplyDamage 打到」的判定条件，
`ApplyDamage` 里 `Cast<IAbilitySystemInterface>` 失败就直接返回 0。

### 3.2 职责表

| 类 | 父类 | 职责 | 关键成员 |
|---|---|---|---|
| `AArcaneVanguardCharacter` | `ACharacter` | 移动 / 镜头 / 输入 / 武器装配 / 普攻连击 / 攻击判定 | `CameraBoom`、`FollowCamera`、`MoveAction`、`LookAction`、`JumpAction`、`WeaponActorClass`、`WeaponAttachSocket`、`SpawnedWeapon`、`ComboMontage`、`ComboIndex`、`bComboWindowOpen`、`bAttackWindowOpen`、`bAttackBuffered`、`ArmedNextSectionIndex`、`AbilitySystemComponent`、`AttributeSet`、`AttackRange`、`AttackRadius`、`AttackDamage`、`DamageEffectClass`、`HitActorsThisAttack` |
| `AArcaneVanguardEnemy` | `ACharacter` | **只管挨打与死亡**，不管追击与出手（那是 AI 的事） | `AbilitySystemComponent`、`AttributeSet`、`DestroyDelayAfterDeath`、`bDisableCapsuleCollisionOnDeath`、`bIsDead` |
| `AArcaneVanguardGameMode` | `AGameModeBase` | 纯逻辑，**不含任何资产路径** | 无（`DefaultPawnClass` / `PlayerControllerClass` 在 `BP_AV_GameMode` 里配） |
| `AArcaneVanguardWeapon` | `AActor` | 武器载体 / 战斗握持偏移 | `MeshComponent`（根）、`MeshAsset`、`CombatGripOffset`、`CombatGripInterpSpeed`、`bAutoTrackOwnerCombat` |
| `AArcaneVanguardPlayerController` | `APlayerController` | 输入上下文注册、后续 UI 输入模式、技能输入转发 | `DefaultMappingContext`、`MappingContextPriority`、`bMappingContextAdded` |
| `UArcaneVanguardAttributeSet` | `UAttributeSet` | 属性容器 + 数值钳制 | `Health`、`MaxHealth`、`PreAttributeChange` |
| `UGE_ArcaneDamage` | `UGameplayEffect` | 原生伤害 GE：Instant + Health Additive + SetByCaller | `Modifiers[0]`（构造里配好，无 UPROPERTY 字段） |
| `UArcaneVanguardCombatStatics` | `UBlueprintFunctionLibrary` | **ADR-005 伤害唯一入口** | `ApplyDamage()`（静态） |
| `UAnimNotifyState_AVComboWindow` | `UAnimNotifyState` | 连击窗口：窗口内按攻击才允许接下一段 | 无（纯事件转发） |
| `UAnimNotifyState_AVAttackWindow` | `UAnimNotifyState` | 判定窗口：`NotifyBegin` 时做一次球扫并结算伤害 | `DamageOverride` |

### 3.3 依赖方向（单向，无环）

```
AArcaneVanguardWeapon          ← 零项目依赖，可独立复用
        ↑ TSubclassOf<AActor>
AArcaneVanguardCharacter
        ↑ 继承
BP_ThirdPersonCharacter  ──指定──→  BP_AV_Weapon_Staff
```

**为什么武器不继承自角色、也不反向持有角色**：武器需要能被丢弃、拾取、给敌人用、当场景摆设。
一旦它依赖角色，这些场景全部要改。持有者信息通过 `AActor::GetOwner()` 拿，不需要硬引用。

---

## 4. 对象生命周期

### 4.1 启动链路

```
UEngine 启动
  → 加载 ThirdPersonMap
  → 读取 WorldSettings / DefaultEngine.ini 的 GlobalDefaultGameMode
  → AArcaneVanguardGameMode 构造：DefaultPawnClass = BP_ThirdPersonCharacter
  → PlayerStart 处 Spawn 玩家 Pawn
  → APawn::PossessedBy(PlayerController)
  → ACharacter::BeginPlay
       └── AArcaneVanguardCharacter::BeginPlay
              ├── Super::BeginPlay()
              └── SpawnAndAttachWeapon()
```

### 4.2 武器装配时序

```
BeginPlay
  ├─ 检查 SpawnedWeapon 是否已存在（防重复）
  ├─ 检查 WeaponActorClass 是否为空（空则警告并跳过）
  ├─ 取 GetMesh() 与 GetWorld()
  ├─ DoesSocketExist("StickSocket")？
  │    ├─ 是 → 生成点 = GetSocketTransform("StickSocket", RTS_World)
  │    └─ 否 → 生成点 = Mesh 组件变换 + 打警告
  ├─ SpawnActor<AActor>(WeaponActorClass, 生成点, AlwaysSpawn)
  ├─ SetActorEnableCollision(false)
  ├─ AttachToComponent(GetMesh(), KeepWorldTransform, "StickSocket")
  ├─ Cast<AArcaneVanguardWeapon> → CaptureBaseTransform()   ← 2026-10-25 新增，必须在 Attach 之后
  └─ 缓存到 SpawnedWeapon
```

**为什么用 `AlwaysSpawn`**：生成点就在角色胶囊内部，默认的碰撞处理会判定为"空间不足"并拒绝生成。

**为什么相对变换是 Identity**：生成点取自插槽世界变换，`KeepWorldTransform` 附着后
`相对 = 插槽逆 × 当前世界 = 单位矩阵`。这让后续校准可以从零开始，不用反推。

**为什么 `CaptureBaseTransform()` 必须在 Attach 之后**：`SpawnActor` 出来的武器，
`BeginPlay` 早于 `Attach` 执行，那时相对变换还不是终值。记录基准的动作只能由角色在
附着完成后触发（`Tick` 里再兜底一次）。

### 4.3 Tick

| 类 | Tick | 用途 |
|---|---|---|
| `AArcaneVanguardCharacter` | **开启（2026-10-26 起重写）** | 每帧把 `ComboIndex` 同步成蒙太奇实际在播的 Section；检测到切段后清理 `ArmedNextSectionIndex` 与跨段残留的 `bAttackBuffered` / `bComboWindowOpen` |
| `AArcaneVanguardWeapon` | **开启（2026-10-26 起）** | 战斗握持偏移的 alpha 插值（`FInterpConstantTo`），只在 alpha 未到位时才写相对位置 |

两者都在"不需要做事"时提前返回，不做无谓写入。

### 4.4 PlayerController 生命周期（ADR-010）

```
UEngine 启动
  → AArcaneVanguardGameMode 指定 PlayerControllerClass = BP_AV_PlayerController
  → APlayerController::BeginPlay
       └── AArcaneVanguardPlayerController::BeginPlay
              ├── Super::BeginPlay()
              ├── GetLocalPlayer() → UEnhancedInputLocalPlayerSubsystem
              └── AddMappingContext(DefaultMappingContext, Priority)
                     └── bMappingContextAdded = true（幂等保护）
  → ... 游戏运行 ...
  → AArcaneVanguardPlayerController::EndPlay
       ├── RemoveMappingContext(DefaultMappingContext)（仅当 bMappingContextAdded）
       └── bMappingContextAdded = false
```

**为什么 MappingContext 归 PlayerController 管**（而不是 Character）：
`UEnhancedInputLocalPlayerSubsystem` 的生命周期挂在 **LocalPlayer** 上，比 Pawn 长。
若在 Pawn 上 `Add` 而 Pawn 销毁时不移除，每次 Possess 都会累积一份。
由同样长命的 PlayerController 成对 Add/Remove，从根上消除累积。

**实测**：启动日志 `[Input] 已注册 MappingContext 'IMC_Default'` 出现 1 次，
退出时 `[Input] 已移除 MappingContext 'IMC_Default'` 出现 1 次 → 成对。

**职责边界**：PlayerController 只管 **MappingContext 的 Add/Remove**；
**Action 绑定**仍在 `AArcaneVanguardCharacter::SetupPlayerInputComponent`，
因为 `UInputComponent` 随 Pawn 销毁，本身不会累积。

---

## 5. 输入链路

```
玩家按键
  → UEnhancedInputLocalPlayerSubsystem（IMC 由 PlayerController::BeginPlay 注册）
  → UEnhancedInputComponent 的 Action 绑定（在 Character 的 SetupPlayerInputComponent）
  → AArcaneVanguardCharacter::Move / Look / ACharacter::Jump
  → AddMovementInput / AddControllerYawInput / Jump
```

**职责划分（ADR-010）**：

| 谁 | 管什么 | 为什么 |
|---|---|---|
| `AArcaneVanguardPlayerController` | **MappingContext 的 Add / Remove** | Subsystem 生命周期 = LocalPlayer，比 Pawn 长，必须由同样长的对象成对管理 |
| `AArcaneVanguardCharacter` | **Action 绑定** | `UInputComponent` 随 Pawn 销毁，不会累积 |

### 5.1 输入资产

| 资产 | 路径 | 说明 |
|---|---|---|
| `IMC_Default` | `Content/ThirdPerson/Input/` | Mapping Context |
| `IA_Move` | 同上 | `Axis2D`，WASD |
| `IA_Look` | 同上 | `Axis2D`，鼠标 |
| `IA_Jump` | 同上 | `bool`，空格 |

### 5.2 已修复：输入绑定生命周期（ADR-010，2026-10-24）

**原问题**：`AddMappingContext` 曾写在 Character 的 `SetupPlayerInputComponent` 里，该函数在 Pawn 被 Possess 时调用，可能重复 Add。

**修复**：迁移到 `AArcaneVanguardPlayerController::BeginPlay`，`EndPlay` 成对 `RemoveMappingContext`，并用 `bMappingContextAdded` 做幂等保护。

**实测**：启动日志 `[Input] 已注册 MappingContext 'IMC_Default'` 出现 **1 次**，退出时 `[Input] 已移除 MappingContext 'IMC_Default'` 出现 **1 次** → 成对。

---

## 6. 动画链路

```
SKE_Wizard（70 骨，UE4 Mannequin 命名）
  └── SK_wizard（4 个皮肤变体）
        └── AnimBP_Wizard_ThirdPerson
              └── BlendSpace Anim_Wizard_ThirdPerson_IdleRun_2D
                    ├── Idle 样本
                    └── Run 样本（Rate Scale 已调，2026-10-24）
```

| 资产 | 说明 |
|---|---|
| `SKE_Wizard` | 70 骨骼，UE4 Mannequin 命名，**有 1 个 socket：`StickSocket`** |
| `AnimBP_Wizard_ThirdPerson` | 目标骨架为 `SKE_Wizard` |
| 动画 17 个 | 移动 7 + 四向跑 4 + 技能 4 + 死亡 1 + BlendSpace |
| 动画来源 | 技能动画为 **Mixamo**（`mixamo.com` 元数据）；移动系不含该标记 |

> **重要**：`Content/Characters/Mannequins/` 下的 `MM_*` 动画和 `ABP_Manny/Quinn`
> **不能直接用于玩家**（骨架链不同），需要 IK Retargeter。

### 6.1 战斗动画层（2026-10-26 补充）

```
AnimBP_Wizard_ThirdPerson
  ├── 状态机（移动：Idle / Walk / Run / Jump / Fall）
  ├── Layered Blend per Bone（按 spine_01 分层）
  │     ├── Base Pose  = 状态机输出（下半身移动）
  │     ├── Blend Pose = 战斗上半身（A_2H_Idle_03）
  │     └── Alpha      = CombatAlpha（由角色 bInCombat 插值而来）
  └── Slot 'DefaultSlot'   ← 蒙太奇在这里注入
        └── Output Pose
```

| 变量 / 节点 | 说明 |
|---|---|
| `CombatAlpha` | 0 = 待机姿态，1 = 战斗姿态。由 `IsInCombat()` 驱动插值 |
| `bInCombat` / `IsInCombat` | 角色侧唯一状态源，AnimBP 只读 |
| `Slot 'DefaultSlot'` | 蒙太奇注入点。**蒙太奇资产里的 `SlotName` 必须与它同名**，否则播了但看不见 |

**过渡时长**：`CombatAlpha` 当前 1 秒走完；武器侧的 `CombatGripInterpSpeed` 默认 **10.0**
（≈ 0.1 秒）—— **两者当前并不同步**，表现为"法杖已经端到位、人还在慢慢抬胳膊"。
要统一就设 `CombatGripInterpSpeed = 1.0 / 过渡秒数`（1 秒过渡 → 1.0）。改一边必须同时改另一边。

### 6.2 普攻连击蒙太奇

| 项 | 值 |
|---|---|
| 资产 | `/Game/ArcaneVanguard/ReTarget/Animation/Combat/AM_NormalAttack` |
| Section | `Combat_01` / `Combat_02` / `Combat_03` |
| Slot | `DefaultSlot` |
| 总长 | 2.1667s（三段 0.6667 / 0.6667 / 0.8333） |
| 源序列 | 同目录 `Combat_01/02/03`（骨架 `SKE_Wizard`） |
| 引用方 | `AArcaneVanguardCharacter::ComboMontage`（`EditAnywhere`，**不硬编码路径**） |

**Notify 布局**（每个 Notify State 都是"有起止"的区间）：

| Notify | 时间 | 作用 |
|---|---|---|
| `AV Attack Window` #1 | 0.153 – 0.366 | 判定 |
| `AV Combo Window` #1 | 0.366 – 0.667 | 接续 |
| `AV Attack Window` #2 | 0.814 – 0.948 | 判定 |
| `AV Combo Window` #2 | 0.948 – 1.333 | 接续 |
| `AV Attack Window` #3 | 1.596 – 1.847 | 判定（第三段**不放**连击窗口） |

**两条硬规则**：
1. **资产里三段的 `Next Section` 必须是 None** —— 否则播完一段会自动接下一段，连击窗口形同虚设
2. **连击窗口的终点不要压在段边界上** —— 那种情况下引擎会直接停掉蒙太奇，
   `AnimNotifyState::NotifyEnd` **不会被调用**

**段间过渡方式**：窗口内按下 → `Montage_SetNextSection` 把当前段链到下一段。
改的是 `FAnimMontageInstance::NextSections`（每播放实例一份），**不写回资产**；
引擎在段尾沿链自然过渡，后摇完整播完。详见 `AArcaneVanguardCharacter` 的连击注释块。

---

## 7. 武器子系统

### 7.1 组成

| 层 | 资产 / 类 | 作用 |
|---|---|---|
| 代码 | `AArcaneVanguardWeapon` | 武器 Actor 基类 |
| 资产 | `BP_AV_Weapon_Staff` | 具体武器（指定 `SM_Wizard_Staff_basic`） |
| 挂点 | `SKE_Wizard.StickSocket` | 骨架上的 socket |
| 装配者 | `AArcaneVanguardCharacter::SpawnAndAttachWeapon()` | 生成 + 附着 |

### 7.2 坐标系约定

| 坐标系 | 含义 |
|---|---|
| 插槽空间（socket space） | 武器 Actor 的相对变换基准。**运行时为 Identity** |
| 武器局部空间 | `MeshComponent` 内网格的原始朝向（长轴 +Z，枢轴在杖尾） |
| 世界空间 | 最终渲染位置 |

**⚠️ 校准方法（2026-10-26 修正，原文是错的）**

~~在 `BP_AV_Weapon_Staff` 里改 `MeshComponent` 的 Relative Location / Rotation~~

**这条不成立**：武器由 `SpawnAndAttachWeapon()` 在**插槽世界变换处**生成，
再 `AttachToComponent(KeepWorldTransform)` → 相对变换被强制拉回 Identity。
**蓝图里设的相对变换在运行时会被冲掉。**

正确做法只有两条：

| 做法 | 适用 |
|---|---|
| 改**骨架上的插槽**（`StickSocket` 的位置/朝向） | 静态基准握法 |
| 走 **C++ 偏移**（如 `CombatGripOffset`） | 需要按状态变化的偏移 |

**实测数据**（2026-10-25）：

| 点 | 世界坐标 | 说明 |
|---|---|---|
| `hand_r` | (2.6, 35.7, 29.1) | 手骨 |
| `weapon_r` | (11.2, 36.6, 29.8) | 骨架自带的武器骨，离手约 8.7cm |
| `StickSocket` | (9.2, 37.3, **−95.4**) | 在 `hand_r` 上带约 **−124cm** 偏移 |

`StickSocket` 几乎落到地面 → 杖尾贴地、杖尖穿到手上方，手卡在杖身 **62%**（前段仅 73.8cm）。
这就是"握在 2/3"的几何来源，不是蓝图里调出来的。

`SM_Wizard_Staff_basic` 包围盒 Z ∈ [3.8, 198.2]，长 194.4cm，**枢轴在杖尾**。

### 7.3 扩展点（已预留，未实现）

| 需求 | 建议做法 |
|---|---|
| 武器命中判定 | `MeshComponent` 改 `QueryOnly`，用插槽做 Trace 起点 |
| 武器掉落 | `DetachFromActor(KeepWorldTransform)` + 开物理 + 恢复碰撞 |
| 多武器切换 | 扩展 `WeaponActorClass` 为数组或数据资产 |
| 武器知道持有者 | `GetOwner()` 已经指向角色，无需额外字段 |

### 7.4 战斗握持偏移（2026-10-26 实装，2026-10-27 更新实测数据）

**解决什么问题**：待机时手在杖身 **64%** 处，前段只有 **69.7cm**，
长枪类武器的战斗感出不来。需要在战斗状态下把武器沿杖身往前滑一段。

| 成员 | 当前默认 | 作用 |
|---|---|---|
| `CombatGripOffset` | **`(0,0,90)`** | 偏移量，**网格本地空间**，沿杖身滑就改 Z（正 = 朝杖尖） |
| `CombatGripInterpSpeed` | **10.0** | alpha/秒；10.0 ≈ 0.1 秒走完。**当前与 AnimBP 的 `CombatAlpha`（1 秒）不同步**，改前先核对两边 |
| `bAutoTrackOwnerCombat` | true | 自动读 Owner（`AArcaneVanguardCharacter`）的 `IsInCombat()` |
| `CaptureBaseTransform()` | — | 记录基准相对位置；**必须在 Attach 之后调** |
| `SetCombatAlpha(float)` | — | 外部直接驱动（不插值），给 AnimBP / 战斗系统用 |

**应用公式**：

```cpp
ParentOffset = MeshComponent->GetRelativeRotation().RotateVector(CombatGripOffset * Alpha);
MeshComponent->SetRelativeLocation(BaseMeshRelativeLocation + ParentOffset);
```

先按组件相对旋转把"网格本地偏移"转到父空间，保证是**沿杖身**而不是沿世界轴。
永远写「基准 + 偏移」，**绝不在当前值上累加**。

**实测效果**（2026-10-27，`CombatGripOffset = 90cm`，法杖长 194.4cm、枢轴在杖尾）：

| 状态 | 手→杖尾 | 占杖身比例 | 前段长度 |
|---|---|---|---|
| 待机 (alpha 0) | 124.7cm | 64.1% | **69.7cm** |
| 战斗 (alpha 1) | **35.5cm** | **18.2%** | **158.9cm** |

> **调参提示**：90cm 偏移把握点从 64% 滑到 **18%**，手几乎贴在杖尾末端（杖尾只剩 35cm 在手下）。
> 前段确实拉到了 159cm，但如果挥动时杖尾戳到小臂，把 Z 降到 40~60 会更平衡。
> 参考：25cm 偏移时握点是 49%（前段 98.8cm）。

**注意**：`MeshComponent` 是武器的**根组件**，改它的相对位置等于移动整个武器 Actor。
因为它的相对旋转是 Identity（见 §7.2），偏移在父空间就等于在插槽空间。

---

### 7.5 伤害链路（2026-10-27 实装，M3）

#### 7.5.1 完整链路

```
Montage 播放
  └── UAnimNotifyState_AVAttackWindow::NotifyBegin
        ├── ① Character->SetAttackWindowOpen(true)   ← 先清空本段命中记录
        └── ② Character->PerformAttackHitCheck(DamageOverride)
              ├── SweepMultiByChannel（球扫，起扫点 = 胶囊体中心，半径 AttackRadius）
              ├── 排除自身 + SpawnedWeapon
              ├── 段内去重（TSet<TWeakObjectPtr<AActor>>）
              └── 对每个命中目标 → UArcaneVanguardCombatStatics::ApplyDamage
                    ├── Cast<IAbilitySystemInterface> → 目标 ASC
                    ├── MakeOutgoingSpec(UGE_ArcaneDamage)
                    ├── SetSetByCallerMagnitude("Data.Damage", -|Damage|)
                    └── ApplyGameplayEffectSpecToTarget
                          └── UArcaneVanguardAttributeSet::PreAttributeChange（钳制 [0, MaxHealth]）
                                └── Health 变化 → 属性变化委托
                                      └── AArcaneVanguardEnemy::OnHealthChanged
                                            └── NewValue <= 0 → HandleDeath()（ragdoll）
```

#### 7.5.2 三个类的分工（为什么拆成三个）

| 类 | 只做一件事 | 为什么不能合并 |
|---|---|---|
| `UGE_ArcaneDamage` | 描述"伤害长什么样"（改哪个属性、怎么改） | GE 是数据，蓝图可继承覆盖；写进代码就没有可视化调参入口 |
| `UArcaneVanguardCombatStatics::ApplyDamage` | 描述"怎么施加"（造 Spec、传值、应用） | 玩家打敌人 / 敌人打玩家 / 陷阱打人三方共用，不能绑在某一方 |
| `AArcaneVanguardCharacter::PerformAttackHitCheck` | 描述"打到谁"（几何判定） | 判定方式会因角色而异（近战球扫 / 技能锥形 / 投射物），不该塞进通用入口 |

**ADR-005 的落地含义**：全项目只有 `ApplyDamage` 能扣血。任何地方想造成伤害都走它，
否则伤害统计、免疫、护盾、暴击这些东西以后没有统一的插入点。

#### 7.5.3 GE 配置（全在构造函数里，无 UPROPERTY 字段）

| 项 | 值 | 原因 |
|---|---|---|
| `DurationPolicy` | `Instant` | 伤害是瞬时事件，不是持续 buff |
| Modifier 数 | 1 | 只动 Health，MaxHealth 不变 |
| Attribute / Op | `Health` / `Additive` | 伤害是绝对值，不是比例 |
| Magnitude | `SetByCaller`，key = `FName("Data.Damage")` | 同一个 GE 类复用于不同伤害数值 |

**为什么 key 用 FName 而不是 GameplayTag**：`FSetByCallerFloat` 同时支持 `DataName`(FName)
和 `DataTag`。FName 版本**不需要在 `DefaultGameplayTags.ini` 注册 tag**，零配置即可跑通。
以后技能系统按 tag 组织（`Data.Damage.Fire`）时再迁。

**符号约定**：调用方永远传正数，`ApplyDamage` 内部统一取负。
`Health` 是当前值，扣血 = 负增量。

#### 7.5.4 判定参数与调参指引

| 参数 | 默认 | 调错的症状 |
|---|---|---|
| `AttackRange` | 200 | 太短 → 明明贴着敌人却打不到 |
| `AttackRadius` | 60 | 太小 → 贴身打不到（球从胶囊体边缘掠过）；太大 → 隔空打到人 |
| `AttackDamage` | 30 | — |
| `DamageEffectClass` | 空 | 空 = 原生 GE；填 BP 子类可改元素/暴击 |

**`AttackRadius` 必须 ≥ 敌人胶囊体半径**。石头人体型大于玩家，建 BP 时把胶囊体调大后，
这个值要跟着调。

**按段配不同伤害**：不用改代码，在蒙太奇每段的 `AV Attack Window` 上填 `DamageOverride` 即可
（默认 -1 = 用角色的 `AttackDamage`）。

#### 7.5.5 三个实现坑

1. **起扫点必须用胶囊体中心**（`GetActorLocation()`），不能从脚底起扫 ——
   球心贴地时只能扫到敌人小腿。
2. **必须排除 `SpawnedWeapon`**：武器是独立 Actor、挂在手上、正好在扫掠路径里，
   不排除的话每次都会命中自己的法杖（而且法杖没有 ASC，只会刷 Verbose 日志）。
3. **`NotifyBegin` 里顺序不能反**：先 `SetAttackWindowOpen(true)`（清空命中记录）再判定。
   反过来的话，新一刀会带着上一刀的命中记录，打同一个敌人时直接跳过。

#### 7.5.6 已知限制

| 项 | 说明 | 何时处理 |
|---|---|---|
| Source 退化路径 | 攻击者无 ASC 时退化成"目标自己当 Source"，联机下不成立 | 做联机时 |
| 无伤害数字 / 受击反馈 | 目前只有血量和日志 | AV-048（UI） |
| 无无敌帧 | 同一段内去重，但相邻两段可以连打同一目标 | 属设计选择，暂不改 |
| 不检测朝向 | 球扫从中心向正前方，身后打不到，但侧后方 60cm 内能打到 | 手感问题再调半径 |

---

## 8. C++ 与 Blueprint 的边界

按策划案 §4.2 划分：

| 内容 | 放在 | 现状 |
|---|---|---|
| 角色移动/镜头参数 | C++ 构造函数 + BP 可覆盖 | ✅ |
| 输入绑定 | C++ `SetupPlayerInputComponent` | ✅ |
| 武器生成与附着逻辑 | C++ | ✅ |
| 武器具体网格 | BP（`BP_AV_Weapon_Staff.MeshAsset`） | ✅ |
| 武器坐标校准 | **骨架插槽** 或 **C++ 偏移**（`CombatGripOffset`） | ✅ 见 §7.2（BP 相对变换在运行时无效） |
| 角色网格 / AnimBP | BP（`BP_ThirdPersonCharacter`） | ✅ |
| 输入资产引用 | BP（`DefaultMappingContext` 等） | ✅ |

**原则**：C++ 不直接引用 Content 资产路径（避免硬编码），所有资产引用走 BP 的
`EditDefaultsOnly` 属性。

### 曾经的唯一例外：GameMode 里的硬编码路径（2026-10-27 已消除）

~~`ArcaneVanguardGameMode.cpp` 用 `FClassFinder` 写死两条资产路径~~

**现状：两条硬编码已全部删除。** `AArcaneVanguardGameMode` 构造函数现在什么都不设，
`DefaultPawnClass` / `PlayerControllerClass` 全部在 **`BP_AV_GameMode` 的 Class Defaults** 里配置。

**为什么要改**：`ConstructorHelpers::FClassFinder` 只接受编译期字符串常量，资产一移动就必须改代码重编译。
这个坑**踩了两次**：
- **2026-10-24**：资产整理进 `BluePrint/` 子目录 → WASD / 镜头 / 跳跃全失效
- **2026-10-27**：`BP_ThirdPersonCharacter` 移到 `BluePrint/Player/` → 玩家变成引擎默认 Mannequin（没武器、没连击）

**现在的诊断**：`StartPlay()` 里打印实际生效的 `Pawn=` / `Controller=`，
若 `DefaultPawnClass` 仍是 `ADefaultPawn` 则打 **Error** 并提示去 BP 里配置 —— 不会再静默失效。

**当前资产路径**（2026-10-27）：

| 资产 | 路径 |
|---|---|
| `BP_ThirdPersonCharacter` | `/Game/ArcaneVanguard/BluePrint/Player/` |
| `BP_AV_PlayerController` | `/Game/ArcaneVanguard/BluePrint/Core/` |
| `BP_AV_GameMode` | `/Game/ArcaneVanguard/BluePrint/Core/` |
| `BP_AV_Weapon_Staff` | `/Game/ArcaneVanguard/BluePrint/Weapons/` |
| `BP_AV_Enemy_Golem` | `/Game/ArcaneVanguard/BluePrint/Enemy/` |
| `ABP_Enemy_Stone` | `/Game/ArcaneVanguard/BluePrint/Enemy/` |

> 注意：`DefaultEngine.ini` 的 `GlobalDefaultGameMode` 也必须指向 `BP_AV_GameMode`（2026-10-27 负责人修改），
> 否则**新建关卡**（不继承当前 WorldSettings）会用到没有 Pawn 配置的 C++ GameMode。

---

## 9. 为后续里程碑预留的位置

| 里程碑 | 需要的扩展点 | 当前状态 |
|---|---|---|
| **M2 GAS** | ASC 初始化时机 | ✅ **已实装**（玩家 `PossessedBy` / 敌人 `BeginPlay`，见 §3.1 与 AV-039/042） |
| **M3 战斗** | 伤害唯一入口 | ✅ **已实装**（`UArcaneVanguardCombatStatics::ApplyDamage`，ADR-005） |
| **M3 战斗** | 攻击命中判定 | ✅ **已实装**（`PerformAttackHitCheck` 球扫，见 §7.5） |
| **M3 战斗** | 敌人基类 | ✅ **已实装**（`AArcaneVanguardEnemy`：挨打 + ragdoll 死亡），BP 与动画待负责人 |
| M4 技能 | Ability 基类 + 输入绑定 | ⏳ 无（AV-045；`AbilitySystemComponent` 已是原生类型，到时决定是否派生子类加 `AbilityInputID`） |
| M4 技能 | 资源 / 冷却 | ⏳ 属性集里还没有 `Mana`（AV-046） |
| M5 AI | AI 控制器 | ⏳ 无（AV-043；计划 C++ 状态机，不用 Behavior Tree） |
| M6 UI | 权威状态读取源 | ⏳ 无（属性系统已有，缺 UI 绑定；用属性变化委托推给 UMG） |

---

## 10. 已知限制与技术债

| 项 | 说明 | 影响 |
|---|---|---|
| ~~无自定义 PlayerController~~ | **已解决**（AV-030） | — |
| ~~输入可能重复绑定~~ | **已解决**（AV-031，ADR-010） | — |
| BP 里残留失效属性 | 上一版武器组件的配置还在 `BP_ThirdPersonCharacter` | 无功能影响，M3 顺手清 |
| 无 Git | 改动不可回滚 | 靠 `.bak` 补偿 |
| `Content/Wizard` 授权未闭合 | 来自 UECandy 转载站 | M7 前必须解决 |
| 单 LOD | `SK_wizard` 只有 1 级 LOD | M7 优化 |
| 20 个材质段 | draw call 偏多 | M7 合并 |
| 武器相对变换在运行时恒为 Identity | `KeepWorldTransform` 附着所致，蓝图里设的相对变换无效 | 校准只能做在插槽或走 C++ 偏移（§7.2） |
| 连击窗口终点不能压段边界 | 否则 `NotifyEnd` 不触发 | 摆 Notify 时留 0.05s 余量 |
| 第三段攻击无连击窗口 | 收招不可取消，属设计选择 | 需要时再补 |

---

## 11. 架构决策索引

| ADR | 主题 | 状态 |
|---|---|---|
| ADR-001 | 引擎版本 UE 5.4.4 | Accepted |
| ADR-002 | 版本控制暂缓 | Accepted |
| ADR-003 | `Content/Wizard` 作为玩家模型 | Accepted |
| ADR-004 | ASC 归属（Character） | **Accepted**（M2 实装，2026-10-26；联机时迁 PlayerState） |
| ADR-005 | 伤害与死亡唯一入口 | **Accepted**（M3 实装，2026-10-27；`ApplyDamage` 是唯一入口） |
| ADR-006 | 冷却与资源消耗用 GE | Proposed |
| ADR-007 | 任务插件适配层 | Proposed |
| ADR-008 | Notify 与 Ability 边界 | Proposed |
| **ADR-009** | **角色框架分层**（PlayerController 新建 / Character 保持继承） | **Accepted**（M1 验收通过，2026-10-26） |
| **ADR-010** | **输入绑定生命周期** | **Accepted**（M1 验收通过，2026-10-26） |
| **ADR-011** | **自定义碰撞通道体系**（`Weapon` / `Enemy` 独立 Object Channel） | **Accepted**（M3 实装，2026-10-30；详见 `DecisionLog.md`） |
| **ADR-012** | **GameplayAbility 分层**（C++ 基类管流程 + 蓝图子类填数据） | **Accepted**（M4 实装，2026-10-30；详见 `DecisionLog.md`） |

## 12. 2026-10-30 架构增量（M3 收尾 / M4 起步）

> 本章只记**结构性变化**，细节见 `Changelog.md` 与 `DecisionLog.md` 对应 ADR。

### 12.1 碰撞通道分层

```
Weapon（ECC_GameTraceChannel1）  —— 武器：全 Ignore，只 Block WorldStatic
Enemy （ECC_GameTraceChannel2）  —— 敌人胶囊：对 Enemy 通道 Block（供球扫命中）
Pawn   （引擎默认）              —— 玩家：保持不变，敌人球扫仍查 Pawn
```

**配置位置**：`Config/DefaultEngine.ini` 的 `[/Script/Engine.CollisionProfile]`。

**关键约束**：`Enemy` 通道的 `DefaultResponse = Block`，意味着**所有物体**对 Enemy 通道都 Block。
所以球扫不能用 `SweepMultiByChannel(Enemy)`（会命中地面/墙/自己），
必须用 `SweepMultiByObjectType` 按物体类型过滤。

### 12.2 技能层（M4）

```
UGameplayAbility（引擎）
  └─ UArcaneVanguardGameplayAbility（C++ 基类）
        ├─ CommitAbility → 隐藏武器 → 施加 SelfEffectClasses → 播蒙太奇 → EndAbility 恢复
        └─ GA_Q_Buff（蓝图子类，只填数据，零连线）
```

**新增属性**：`AttackSpeedMultiplier`（攻速倍率，`PlayComboSection` 里乘到 PlayRate 上）。

### 12.3 敌人受击的状态归属

受击状态**不在 C++ 里硬暂停 AI**，而是写 Blackboard 的 `IsStaggered`，
由 BT 的 `Selector` + `Decorator(Aborts = Lower Priority)` 自动切换分支。

```
C++ HandleHitReact()  →  BB.IsStaggered = true  →  BT Selector 切受击分支（Wait 0.6s）
                                                            ↓ 自动 Abort 战斗分支的 MoveTo
C++ EndStagger()      →  BB.IsStaggered = false →  BT 回战斗分支
```

**降级路径**：BB 里没有 `IsStaggered` 键时，退回 `StopMovement + PauseLogic`（旧行为），
保证 BT 资产未更新时功能不失效。
