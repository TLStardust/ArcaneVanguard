# TechnicalDesign — 技术设计与现状架构

- 最后更新：2026-10-21
- 性质：**§2 是已核实事实，§3 及之后是目标设计与计划**。两者不得混淆。

---

## 1. 技术栈基线

| 项 | 现状 | 来源 |
|---|---|---|
| 引擎 | UE 5.4.4-35576357 | MCP `get_editor_status` |
| 语言 | C++17（引擎默认）+ Blueprint | 模板 |
| 输入 | Enhanced Input（已启用） | `Build.cs`、`Character.cpp` |
| GAS | **未接入** | `Build.cs` 无 GameplayAbilities/Tags/Tasks；`.uproject` 未启用插件 |
| 任务系统 | **未接入**（插件在另一个工程里） | `find` 结果 |
| 版本控制 | **无** | `git status` 失败 |

---

## 2. 现状架构（已核实）

### 2.1 模块与依赖

```
Source/
├── ArcaneVanguard.Target.cs          (Game)
├── ArcaneVanguardEditor.Target.cs    (Editor)
└── ArcaneVanguard/
    ├── ArcaneVanguard.Build.cs
    ├── ArcaneVanguard.cpp / .h        (IMPLEMENT_PRIMARY_GAME_MODULE)
    ├── ArcaneVanguardCharacter.cpp/.h (ACharacter，第三人称模板原样)
    └── ArcaneVanguardGameMode.cpp/.h  (DefaultPawnClass → BP_ThirdPersonCharacter)
```

`ArcaneVanguard.Build.cs` 的 Public 依赖：
`Core, CoreUObject, Engine, InputCore, EnhancedInput`。

`.uproject` 插件：`ModelingToolsEditorMode`（Editor only）、`PythonScriptPlugin`（MCP 桥接需要）。

### 2.2 类职责（现状）

| 类 | 职责 | 关键实现 |
|---|---|---|
| `AArcaneVanguardCharacter` | 第三人称角色 | 胶囊 42/96；`bOrientRotationToMovement=true`，RotationRate 500；JumpZ 700、AirControl 0.35、MaxWalkSpeed 500、Braking 2000/1500；`CameraBoom`（ArmLength 400，`bUsePawnControlRotation=true`）+ `FollowCamera`；`SetupPlayerInputComponent` 里 `AddMappingContext(DefaultMappingContext, 0)` 并绑定 Jump/Move/Look |
| `AArcaneVanguardGameMode` | 默认 Pawn 指定 | 构造函数 `FClassFinder<APawn>` 取 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter` |

### 2.3 资产与内容

| 目录 | 体积 | 内容 | 归属 |
|---|---|---|---|
| `Content/ThirdPerson/` | 3.2 MB | `BP_ThirdPersonCharacter`、`IMC_Default`、`IA_Move/Look/Jump`、`ThirdPersonMap` | UE 模板 |
| `Content/Characters/` | 402 MB | Mannequin 网格、`ABP_Manny` / `ABP_Quinn`、Rigs | UE 模板 |
| `Content/Wizard/` | **2.1 GB** | 巫师骨骼网格（4 个皮肤）、材质、17 个动画、5 把法杖/书、DemoRoom 蓝图 | **玩家模型**（ADR-003 已确认）；许可未登记 |
| `Content/LevelPrototyping/` | 272 KB | 灰盒件 | UE 模板 |
| `Content/__ExternalActors__` / `__ExternalObjects__` | 292 KB | 关卡外部 Actor | 模板 |

- 磁盘上**只有 1 张关卡**：`Content/ThirdPerson/Maps/ThirdPersonMap.umap`。
  （`Content/Wizard/Maps/` 下只剩子目录，无 `.umap`；历史记录里提到的 `Map_Wizard_Showcase` / `Map_Wizard_presentation` 当前**不存在于磁盘**。）

### 2.4 已确认的技术风险（现状层）

1. **无版本控制** → 任何改动都不可回滚，这是当前最高风险。
2. `Content/Wizard` 占工程 84% 体积，且许可未知。
3. `DefaultGame.ini` 的 `ProjectName` 仍是 `Third Person Game Template`。
4. 项目名重定向已配好（`ActiveGameNameRedirects` TP_ThirdPerson → ArcaneVanguard，`ActiveClassRedirects` 同名类），说明是从模板改名而来，**不是全新工程**。
5. 巫师网格资产序列化为已移除的 `ClothingSimulationFactoryNv`，每次注册触发 47 条警告（详见 `Cloth_Jitter_Diagnosis.md`）。

### 2.5 当前代码架构（2026-10-24）

> **完整架构文档见 `Docs/Architecture_20261024.md`**（分层总览、模块依赖、类职责、对象生命周期、
> 输入/动画链路、武器子系统、C++/BP 边界、扩展点、技术债、ADR 索引）。
>
> 本节只保留三句话摘要：
>
> 1. **C++ 侧只有 3 个类**：`AArcaneVanguardCharacter`（模板 + 武器装配）、
>    `AArcaneVanguardGameMode`（指定 DefaultPawnClass）、`AArcaneVanguardWeapon`（AActor 武器载体）。
> 2. **武器用 SpawnActor + AttachToComponent 挂到 `StickSocket`**，生成点取插槽世界变换，
>    因此附着后相对变换为 Identity，校准从零开始。实测武器世界坐标与插槽偏差 `(0,0,0)`。
> 3. **GAS / 自定义 PlayerController / 战斗逻辑 / 敌人基类均不存在**，这是 M1 的起点。

## 3. 目标架构（计划，未实现）

按 `GameDesign_V1.0.md` §4.1 的九层职责划分。**只在类数量或职责边界足以证明需要时才建目录。**

### 3.1 Source 目标结构

```
Source/ArcaneVanguard/
├── Core/             GameMode / GameState / PlayerController / PlayerState 职责分配
├── Character/        玩家与敌人基类、移动与生命状态
├── Combat/           攻击判定、弹体、目标选择、伤害请求、死亡结算边界
├── AbilitySystem/
│   ├── Abilities/    GA_* 对应的 C++ 基类或纯蓝图能力
│   ├── Attributes/   UAS_AV_Combat
│   ├── Effects/      GE_* 的 C++ 侧配置
│   └── Tasks/        AbilityTask（如需要）
├── AI/               AIController、感知、状态决策
├── Interaction/      交互与区域触发
├── TaskIntegration/  QuestSystem 薄适配层（唯一允许依赖插件的位置）
├── UI/               HUD 数据绑定（只读权威状态）
└── Tests/            自动化测试
```

### 3.2 关键设计决策（待 ADR 确认）

| 主题 | 计划 | 状态 |
|---|---|---|
| ASC 归属 | 单机 Demo → ASC 放 Character（玩家与敌人一致），`InitAbilityActorInfo` 在 `PossessedBy` + `OnRep_PlayerState` 各一次，用 bool 防重 | 待 ADR-004 |
| 伤害结算唯一入口 | `UArcaneCombatComponent::ApplyDamage` → `GE_AV_Damage` → AttributeSet 的 `PostGameplayEffectExecute` 判死 → 调 `HandleDeath()` | 待 ADR-005 |
| 死亡防重 | `HandleDeath()` 用 `bDeathHandled` 守卫；`State.Dead` Tag 阻塞后续能力 | 待 ADR-005 |
| 冷却与消耗 | Cost / Cooldown 用 `UGameplayEffect`（Instant + Duration）而非手写计时器 | 待 ADR-006 |
| 任务接口 | 只上报事实（击杀 + 目标 ID），不做反向控制；适配层校验参数 | 待 ADR-007 |
| Notify 与 GA 边界 | Notify 只触发表现与"经设计的动作节点"，伤害只在 Ability 内结算一次 | 待 ADR-008 |

### 3.3 目录/命名

遵循 `GameDesign_V1.0.md` §5：`BP_AV_*`、`GA_AV_*`、`GE_AV_*`、`WBP_AV_*`、`DA_AV_*`。
Content 目标根目录为 `Content/ArcaneVanguard/`（**当前不存在**）。

---

## 4. 与策划案的差距（Gap）

| 策划案要求 | 现状 | 差距等级 |
|---|---|---|
| M0：可编译可运行的基线工程 | 产物存在，未实跑验证；无 Git | 高 |
| M0：`.gitignore` + 基线提交 | 无仓库 | 高 |
| M0：Docs 文档体系（11 份） | 已建 10 份 + 策划案归档 | 已闭合 |
| M1：项目自己的角色类 | 模板原样（类已改名，无项目级扩展点） | 中 |
| M2：GAS 骨架 | 零 | 高 |
| M3–M5：战斗与 AI | 零 | 高 |
| M6：任务插件接入 | 插件不在本工程 | 高 |
| M7–M8：表现与作品集 | 零 | 中 |

> 结论：项目实际处于 **M0 收尾 / M1 起点**，不是"接近完成"。所有 GAS 与战斗内容都还不存在。

---

## 5. 玩家角色美术与动画现状（已核实，2026-10-21）

负责人已确认 **`Content/Wizard` 就是本项目的玩家模型**（ADR-003）。以下是逐项清点结果，直接决定 M1 的可行范围。

### 5.1 网格与骨架

| 资产 | 路径 | 说明 |
|---|---|---|
| 主网格 | `Content/Wizard/Mesh/SK_wizard` | 20 个材质段，**仅 1 个 LOD**；section 10 有布料 |
| 皮肤变体 | `Mesh/Skin/SK_wizard_Dark` / `_Fire` / `_Ice` | 同骨架、同物理资产，各带 1 份独立布料拷贝 |
| 骨架 | `Mesh/SKE_Wizard` | **70 根骨骼** |
| 物理资产 | `Mesh/PHY_Wizard` | 17 个标准两足体，**无裙摆/下摆专用碰撞体** |
| 武器 | `Mesh/Weapon/` | `SM_Wizard_Book`、`SM_Wizard_Staff_basic` / `_Dark` / `_Fire` / `_Ice` |
| 摆姿参考 | `Mesh/StaticMesh/SM_Wizard_pose` | 静态姿态，非运行时资产 |

### 5.2 动画（共 17 个，全部在 `Content/Wizard/Animations`）

| 类别 | 资产 | 覆盖情况 |
|---|---|---|
| 移动 | `ThirdPerson/Anim_Wizard_ThirdPersonIdle`、`_ThirdPersonWalk`、`_ThirdPersonRun`、`_ThirdPerson_Jump`、`_Jump_Start`、`_Jump_Loop`、`_IdleRun_2D`（BlendSpace） | **完整** |
| 四向移动 | `Anim_Wizard_Run_Forward` / `_Back` / `_Left` / `_Right` | 有（但非第三人称八向集） |
| 技能 | `Anim_Wizard_Cast_Spell_01`、`_Cast_Spell_02`、`_Magic_Attack`、`_Magic_Heal` | 4 个施法动作，**足够覆盖 Q/E/R + 普攻** |
| 死亡 | `Anim_Wizard_Death_Forward` | 有 1 个 |
| AnimBP | `ThirdPerson/AnimBP_Wizard_ThirdPerson` | 已存在，绑定该骨架 |

### 5.3 关键结论：与模板动画**完全不兼容**

> ⚠️ **2026-10-23 修正**：本节写于 2026-10-21，当时判断"骨架独立、MM_* 一律不能用"。
> 后续实测（§5.8.2）证明 `SKE_Wizard` 是 UE5 Manny 骨架的**严格命名子集**（70/70 命中），
> 因此 Manny/Mixamo 内容经 IK Retargeter 后**可以**使用。本节结论仅适用于"直接使用"场景。

- `SKE_Wizard` 是独立骨架，**不是** UE5 Mannequin 骨架。
- 因此 `Content/Characters/Mannequins/Animations/` 下的 `MM_Idle` / `MM_Walk_Fwd` / `MM_Run_Fwd` / `MM_Jump` / `MM_Land` / `BS_MM_WalkRun` **一律不能用于玩家角色**。
- `ABP_Quinn` / `ABP_Manny` 及 Rigs 下的 `CR_Mannequin_BasicFootIK` 同样不适用。
- **M1 起玩家必须走 `AnimBP_Wizard_ThirdPerson`**，`BP_ThirdPersonCharacter` 的 `mesh_anim_class` 需要替换。

### 5.4 动画缺口（需在 M7 前补，否则影响手感与表现）

| 缺失类别 | 影响 | 临时方案 |
|---|---|---|
| 受击反应（Hit React / Stagger） | 被打时无反馈 | 用 Montage 做轻微上半身抖动，或用 Niagara + 音效补偿 |
| 落地（Land） | 跳跃落地无过渡 | 直接回 Idle/Run，靠 Montage 混 |
| 转向（Turn L/R） | 急转时脚滑 | 提高 `RotationRate` + 依赖 `bOrientRotationToMovement` |
| 施法起手/收招分段 | 技能节奏单一 | 用 4 个施法动作做 Montage 分段 + Notify 控时序 |

### 5.5 玩家网格的实际挂载状态（运行时读到，2026-10-21）

| 项 | 实际值 | 结论 |
|---|---|---|
| `mesh_asset` | `/Game/Wizard/Mesh/SK_wizard.SK_Wizard` | ✅ 已从模板的 `SKM_Quinn_Simple` 换成巫师网格 |
| `anim_class` | `ABP_Quinn_C` | ⚠️ **仍是 Mannequin 的 AnimBP**，`AnimBP_Wizard_ThirdPerson` 未使用（KI-010） |
| `cloth_blend_weight` | 1.0 | 布料全程参与，KI-007 全程可见 |
| SpringArm / Camera | 400.0 / `FollowCamera` | ✅ 正常 |
| `max_walk_speed` / `jump_z` | 500 / 700 | ✅ 与 C++ 默认一致 |

> 实测骨骼世界坐标在 5 秒内有变化（`hand_r` ≈0.4、`head` ≈0.9），日志无 skeleton mismatch 警告 →
> 动画**确实在评估**。但用 Mannequin 的 AnimBP 驱动巫师骨架，意味着 §5.2 里那 17 个巫师专用动画
> （尤其是 4 个施法动作）**一个都用不上**，M3/M4 做技能表现时必须先切回 `AnimBP_Wizard_ThirdPerson`。

### 5.6 骨架与武器挂载点（2026-10-21 实测）

| 项 | 结论 |
|---|---|
| 骨架命名体系 | **UE4 Mannequin**：`pelvis` / `spine_01..03` / `neck_01` / `head` / `clavicle_l/r` / `upperarm_l/r` / `lowerarm_l/r` / `hand_l/r` / `thigh_l/r` / `calf_l/r` / `foot_l/r` / `ball_l/r` + 五指三节 + `ik_*` |
| 骨骼总数 | 70 |
| **Socket 数量** | **0**（`skm.num_sockets() == 0`；骨架资产二进制里也不存在任何 socket 名） |
| IK 骨 | `ik_foot_root` / `ik_foot_l` / `ik_foot_r` / `ik_hand_root` / `ik_hand_gun` / `ik_hand_l` / `ik_hand_r` |
| 可用挂点 | 直接挂 **`hand_r` 骨骼**（`AttachToComponent` 接受骨骼名，不需要 socket） |

> 推论 1：**没有 socket 不是缺陷**，只是这个资产从来没配过。想精调握持姿势，可以在骨架编辑器里给 `hand_r` 加一个 socket（带偏移与旋转），然后挂到 socket 上。
>
> 推论 2：骨架是 UE4 Mannequin 命名体系 → **UE4 Mannequin 动画包可按骨骼名 1:1 直接使用**；Mixamo 需经 IK Retargeter 映射（骨骼名完全不同），但链路成熟（见 §5.7）。

### 5.6.1 武器挂载实现与实测结果（2026-10-21）

> ⛔ **2026-10-24：本节记录的实现已从代码中全部删除。** 按负责人要求，C++ 已回滚到第三人称模板原始状态，
> `WeaponMesh` 组件、竖直约束、收杖/掉杖规则、调试按键均不存在。
> 以下数据保留为**历史验证记录**（说明当时确实做到过什么、如何测的），重新引入武器时可直接复用。
> 代码存档：`Docs/Backup_TestSource_20261024/`

| 项 | 值 | 证据 |
|---|---|---|
| 组件 | `AArcaneVanguardCharacter::WeaponMesh`（`UStaticMeshComponent`） | `ArcaneVanguardCharacter.h` |
| 挂点 | 骨骼 `hand_r`（非 socket） | 运行时 `get_attach_socket_name()` → `hand_r` |
| 挂载父级 | `CharacterMesh0` | 运行时 `get_attach_parent()` |
| 武器资产 | `SM_Wizard_Staff_basic`（bbox 25×22×**194** cm） | CDO 回读 + 运行时 |
| 相对变换 | loc `(-94.38, 16.23, 14.74)` / rot(pitch 60.83, yaw -88.05, roll 103.05) | `Saved/Logs/av_weapon_transform.json` |
| 挂载精度 | 武器世界坐标与 `hand_r` 骨骼坐标**距离 0.0** | 运行时对比 |
| 视觉结果 | 法杖竖直、水晶过头顶、杖尾接近膝盖高度 | `Saved/Screenshots/weapon_final.png` |

**关于"摆动"的更正（2026-10-21 实测复核）**：
此前记录"手部姿态变化时法杖会摆动、两次运行杖尾 z 分别为 ≈6 与 ≈153"，**这是误判**。
在同一角色位置下间隔 14 秒两次采样：

| 采样 | 角色 z | 法杖 z | 手骨 z | 手→杖距离 |
|---|---|---|---|---|
| T=39.5s | 92.01 | 6.38 | 102.99 | **96.8907** |
| T=53.9s | 92.01 | 6.09 | 102.97 | **96.8907** |

手到法杖的距离**四位小数完全一致**，法杖世界高度仅变化 0.29。即：**刚性附着，不存在漂移**。
早先的 z≈153 是因为那一次角色站在高台上（角色基准高度不同），不是武器在摆。

**真正遗留的问题只有一个**：法杖被正确地"绑"在手上（杖身中点正好在手的位置），
但待机动画的手是自然下垂的张开手型，**没有握持姿势**，所以看起来像"法杖贴在旁边"而不是"握在手里"。
这属于动画问题，不是挂载问题。

**关于"法杖上下颠倒"（2026-10-21 实测）**：
用户观察到施法时水晶朝下、杖头朝上，而待机时水晶朝上。实测确认并定位到根因：

| 姿势 | `hand_r` 世界旋转 | 法杖世界旋转 | 现象 |
|---|---|---|---|
| 待机 | pitch 73.96 / yaw 124.27 / roll 100.07 | pitch 6.10 / yaw -127.58 / **roll -3.87** | 水晶朝上 ✓ |
| `Cast_Spell_01`（t=1.13） | pitch -69.29 / yaw -90.45 / roll 45.68 | pitch 18.87 / yaw -72.96 / **roll -162.54** | 水晶朝下 ✗ |

- **手腕世界旋转差 = 172.44°**（四元数夹角实测）。
- 法杖是 `hand_r` 的刚体子节点，手腕转 172° → 法杖跟着翻 172° → 端对端颠倒。
- **结论：这不是偏移参数设错，而是"没有持杖动画"的必然结果。**
  任何**固定**的相对变换都无法同时让待机和施法都水晶朝上：
  - 保持现值 → 待机正确、施法颠倒（当前状态）
  - 翻转 180° → 施法正确、待机颠倒（同样不可接受）

**三条出路**

| 方案 | 做法 | 成本 | 代价 |
|---|---|---|---|
| **P1 锁定竖直（推荐）** | 每帧重算法杖世界变换：**握点（局部 (0,0,96)）始终贴合 `hand_r`，朝向始终竖直（yaw 跟随角色）**；用 `inverse(socketWorld) * desiredWorld` 求相对变换写回 | ~1 小时 | 前刺类动作法杖也保持竖直，不跟随前倾 |
| P2 按姿势切偏移 | 待机一套、施法一套，靠状态/Notify 切换 | 中 | 状态多易漏，维护差 |
| P3 等握持动画 | 动画里手腕保持合理角度 | 高 | 治本 |

### 5.7 动画质量的可行路线（针对"现有动画不满意"）

**★ 溯源结论（2026-10-22，二进制字符串扫描）**：

| 标记 | 命中 | 含义 |
|---|---|---|
| `mixamo.com` | 9 个：`Cast_Spell_01/02`、`Magic_Attack`、`Magic_Heal`、`Death_Forward`、`Run_四向` | 这些是 **Mixamo 通用人形动画**，录制时**手上没有武器** |
| `UECANDY.COM` | 19 个（全部） | 整包来自 **UECandy 转载站** |

**这条结论一次性解释了三个此前分开记录的问题**：没有握持姿势（KI-013）、
手腕 172° 翻转导致法杖颠倒（KI-014）、`Magic_Attack` 是双手前推而非持杖动作（KI-015）。
**不是参数没调对，是动画本身与法杖无关。**

| 路线 | 成本 | 说明 |
|---|---|---|
| 调现有 AnimBP | 低 | 现有 17 个动画本身质量有限，靠 BlendSpace / 分层 / IK 只能改善过渡，改不了动作本身 |
| **UE4 Mannequin 动画包**（首选） | 低 | 本骨架就是 UE4 Mannequin 命名 → 导入时指定 `SKE_Wizard` 即可按骨骼名 1:1 匹配，**不需要 IK Retargeter** |
| Mixamo 重定向 | 中 | 现有 9 个动作本来就是 Mixamo，说明**重定向链路已被验证可行**；只需挑"持杖"类动作重做一遍 |
| 自研动画 | 高 | 需要 Blender/动捕，个人项目不现实 |

**重定向的固有代价（必须先知道）**：
1. 只搬**骨骼旋转**，不搬比例差异 → 身高/臂长不同会脚滑、手部位置偏移，需要调 retarget pose 与 translation mode。
2. 手部武器握持**不会自动贴合**你的法杖 —— 仍需 socket + 偏移（见 §5.6.1）。
3. 根运动模式需与原动画一致，否则位移会叠加或丢失。

### 5.8 普通攻击连击动画的获取路径（2026-10-22）

**结论：现有 17 个动画全是技能/移动/死亡，没有任何"普通攻击"连发动画。** 这是 M3/M4 做攻击系统前的硬缺口。

本工程的两个有利条件决定了怎么选：

1. **骨架是 UE4 Mannequin 命名**（70 骨骼，名对得上）
2. **Mixamo → `SKE_Wizard` 的重定向链路已被验证可行**（现有 9 个技能动画就是这么来的）

所以两条路都通，按成本排序：

| 路径 | 做法 | 成本 | 说明 |
|---|---|---|---|
| **① Fab / Marketplace 打包动画** | 选**标注 "Epic skeleton" / "UE4 Mannequin" 兼容**的近战连击包，导入时 Skeleton 选 `SKE_Wizard` | 低 | 优先。搜索关键词：`combo`、`attack combo`、`melee animset`、`staff`。搜过的结果里出现过 `Essential Great Sword Animation Pack` 一类包，但**具体内容与当前是否在售需你在 Fab 里确认** |
| ② Mixamo 重定向 | 搜 `staff attack` / `attack combo`，走 IK Rig + Retargeter | 中 | 已有成功先例；但 Mixamo 的"连击"多为单段动作，需自己截段拼 |
| ③ 自己 K | Blender 里做三连击 | 高 | 最可控 |

**重要架构提醒**：连击不是"三个动画挨着播"，需要
`AnimMontage`（含 3 个 Section）+ 输入缓冲 + ComboWindow 判定。
动画资源只是第一步，M3/M4 要做的是连击状态机。
本工程已有 `DebugAnimations` + `DebugPlayAnimation` 可以当验证脚手架。

### 5.8.1 Fab 候选实测调研（2026-10-22，WebSearch/WebFetch，未购买未下载）

| 候选 | 价格 | 引擎版本标注 | 骨架兼容性（页面明示） | 评分 | 备注 |
|---|---|---|---|---|---|
| **Mega Spear Animation Pack**（2018 年老包，Fab 已转免费） | **免费**（页面原文 "is new Free here on FAB"；永久停更） | UE 4.18–4.27 / 5.0–5.1 | **已离线实证（2026-10-24）**：角色动画集引用包内 `UE4_Mannequin_Skeleton`，其 70 骨与 SKE_Wizard **68/70 骨名一致**（唯一差异：包用单骨 `weapon_mount`，我们用 `weapon_l`+`weapon_r`）→ **不需要 IK Retargeter**，duplicate+set skeleton 按骨名复制即可 | 未评分 | **零成本首选，已整包拷入 AV**（`Content/SpearAnimationsV2/`，76MB/401 资产，保持原路径保引用）。**包结构（实测）**：① `Animations/Character/`（247 个，UE4 Mannequin 骨架）= 真正的角色集：10 个持握姿态（1H×4 + 2H×6），每姿态含 Idle + 攻击（Jab/Swing/Thrust × **High/Mid/Low 三向变体**，天生适合连击链）+ Block + 受击（HitHigh/HitLow）+ 移动 + **全姿态互通过渡图**（X_to_Y）；② `Animations/Spear/`（124 个）= 长枪形变/IK 追踪专用小骨架（仅 HandL/HandR/Spear/joint3/joint4 五骨），**对角色无用，重定向时跳过**；③ 包自带 UE4 Mannequin 网格/材质/ABP（BP_SpearFighter，IK 手部追踪系统）——ABP 不迁移，仅取 AnimSequence |
| **Essential Magic Staff Animation Pack**（space.bar.anim） | $37.61 | UE 4.26–4.27 / 5.0–5.7 | 更新日志写明 reposition **`ik_hand_gun`** bone —— **UE4 Mannequin 命名，与本骨架（含 ik_hand_gun）1:1 对应**；另附 UE5 skeleton 重定向版 + IKRig/IKRetargeter | 4.9 (21) | **付费首选**。法杖战斗专用，254+ 动画，含 staff 近战连击（attack_10/11/12）、guard、teleport、knockdown、locomotion；动画清单在卖家 Google Sheet |
| **Mage with Staff Animations**（Excalibur Studios） | $9.66 | UE 4.27 / 5.0–5.7 | 骨架未明示（需进页面确认） | 5.0 (3) | 便宜；含战斗+移动动画，全部动画带镜像版（左右手持杖） |
| **Simple Magic Staff Animation Pack**（CG When Free） | $26.86 | UE 5.2–5.8 | **仅 UE5 Manny 骨架**，页面明示 UE4 mannequin 需重定向 | 5.0 (1) | 需 IK Retargeter；但本骨架与 Manny 是命名子集关系（见 §5.8.2），重定向几乎是恒等映射，不再构成排除理由 |
| **Melee Combo Vol 1 / 2 / 3**（ZzGERTzZ） | $34.99/个 | UE（未逐一核实） | 未明示 | 4.9 / 4.0 / 5.0 | 通用近战连击包，连击结构完整但非法杖专用 |
| **Greatsword Combo Animation Pack**（50 berry） | $15.99 | 未核实 | 未明示 | 4.0 (4) | 大剑连击，可作普攻连击来源 |
| **Sword Animation Pack – 35+ Combat Animations**（Animaster Studio） | $19.99 | 未核实 | 未明示 | — | 35+ 战斗动画 |
| **Mixamo（免费）** | 免费 | — | 需 IK Retargeter（链路本工程已验证可行） | — | 搜 `staff attack` / `attack combo`；多为单段，需自己截段拼连击 |

**补充候选（2026-10-23 关键词实测新发现，未核实详情）**：Combat Magic Animations（SoerGame，魔法战斗动画）、Spear Combat/Fishing/Survival Animations – Mocap Pack（nikoff，$21.48，长柄动捕）、Melee+ (AI+FP/TP+Two Hands)（Affordable Quality，$41.73）、G2: Monk（Quang Phan，武术棍角色）、4 Weapons Essential Animations（Gaming Maestro，$62.60，疑似含杖）。搜索关键词手册（中英双语 + 实测效率速查表）：`Docs/AV023_SearchKeywords.md`。

**结论与建议（2026-10-23 更新）**：**先下载免费的 `Mega Spear Animation Pack`**（零成本、219 动画、UE4 Mannequin 命名与本骨架骨名一致、无版本障碍——详见上表）；到手后逐个数攻击动画的连击段数，不满足 3/4 连击再考虑 `Essential Magic Staff Animation Pack`（$37.61，法杖专用连击最完整）。**下载/购买属 L4，必须由负责人执行**。

**已知集成问题（接入任何攻击动画前必须处理）**：① `bKeepWeaponUpright` 竖直约束会掰直攻击动画里的武器姿态——播放攻击动画时需关闭该约束（按动画类型切换）；② 长枪动画为双手持握，法杖现仅挂 `weapon_r` 单点，左手会握空（先接受，后续可加 `weapon_l` 第二挂点）。

### 5.8.2 骨架决策：不切换到 UE5 Manny（2026-10-23，实测）

负责人曾考虑"为找攻击动画把巫师换成小白人（Manny）骨架"。**结论：不切换，保持 `SKE_Wizard`。**

实测（引擎 `SKM_Manny.uasset` 名称表 vs `SKE_Wizard` 70 骨全量枚举，`Docs/SkeletonDecision_20261023/manny_vs_wizard_bones.json`）：

- **`SKE_Wizard` 是 Manny 骨架的严格命名子集**：70/70 骨骼名全部命中，Manny 仅多 `spine_04`、`spine_05`、`neck_02`；`weapon_l/r` 两边都有。
- 因此 "UE4 Mannequin / UE5 Manny compatible" 的包对本骨架等价，不存在选错骨架的问题。
- Mixamo 是独立骨架，换不换 Manny 都要重定向——换骨架对 Mixamo 零帮助。
- 真正决定观感的是动画**是否持杖**（staff 专用包 > 通用人形包），不是骨架归属。

**可扩展方案**：一次性配置 IK Retargeter（Manny → SKE_Wizard，Humanoid 自动链映射，近乎恒等），之后任何来源的动画都能批量接入。
完整理由与配置步骤见 `Docs/SkeletonDecision_20261023/README.md`。

### 5.9 已存在的技术风险（对玩家模型直接生效）

1. **布料抽搐**（KI-007）：唯一布料区在 `armor3_01`（section 10），披风与主体护甲是纯蒙皮。作为玩家模型后，该问题玩家全程可见 → 优先级升为 P1。
2. **NvCloth 遗留**（KI-005）：4 个网格均序列化为已移除的 `ClothingSimulationFactoryNv`，每次注册触发警告并被内存改写为 Chaos。
3. **单 LOD**：`SK_wizard` 只有 1 级 LOD，10 个实例同时跑布料时开销明显（当前关卡雕像场景）。
4. **骨架 70 骨骼 + 20 材质段**：材质段数量偏多，M7 需合并以降低 draw call。
