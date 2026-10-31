# Wizard 布料抽搐 —— 诊断报告与修复计划

- 项目：`D:\UEProject\ArcaneVanguard`（UE 5.4.4-35576357）
- 诊断方式：MCP 只读内省（资产属性 / 关卡 Actor / 日志 / 配置），**未修改任何资产**
- 诊断时间：2026-10-21
- 现象（用户描述）：`/Game/Wizard/Mesh` 下的骨骼网格体部分位置衣服抽搐；运行时 `p.ClothPhysics 0` 后抽搐消失

---

## 一、检查结果（已观察到的事实）

### 1.1 资产清单

| 资产 | 路径 | 关键信息 |
|---|---|---|
| 主骨骼网格 | `/Game/Wizard/Mesh/SK_wizard` | 20 个材质段，**仅 1 个 LOD** |
| 骨架 | `/Game/Wizard/Mesh/SKE_Wizard` | 70 根骨骼 |
| 物理资产 | `/Game/Wizard/Mesh/PHY_Wizard` | 约 17 个标准两足骨骼体（pelvis / spine_01-03 / neck / head / clavicle / upperarm / lowerarm / hand / thigh / calf / foot，左右各一） |
| 皮肤变体 | `/Game/Wizard/Mesh/Skin/SK_wizard_Dark` `/SK_wizard_Fire` `/SK_wizard_Ice` | 同骨架、同物理资产，各带 1 份布料 |
| 分件 | `/Game/Wizard/SeparatedMesh/SK_Wizard_*`（armor / mainArmor / pants / hood / boots / face / accessories） | **全部 0 布料** |

### 1.2 布料挂在哪个段上（关键）

`SK_wizard` 只有 1 个布料资产 `SK_wizard_Clothing_0`（内嵌，非独立 .uasset），`is_section_using_cloth()` 只对 **section 10** 返回 true。

| Section | 材质槽 | 材质 | 有布料 |
|---|---|---|---|
| 10 | `armor3_01_defaultMat1` | `M_Wizard_armor3_Inst` | **是** |
| 11 | `armor3_02_defaultMat1` | `M_Wizard_armor3_Inst` | 否 |
| 17 | `cape` | `M_Wizard_armor1_Inst` | **否** |
| 18 | `beard2` | `M_Wizard_armor1_Inst` | 否 |
| 19 | `armor1_main` | `M_Wizard_armor1_Inst` | 否 |

> 推论：**披风（cape）和主体护甲根本不是布料**，是纯蒙皮跟随骨骼。全模型唯一的布料区域是 `armor3_01` 这一块。这直接决定了"抽搐"只可能出现在这一块，也决定了修复不会影响披风（它本来就没有布料动态）。

### 1.3 Chaos 布料配置（4 份内嵌拷贝逐项比对，**完全一致，无差异**）

| 参数 | 值 | 备注 |
|---|---|---|
| `bUseSelfCollisions` | **True** | 自碰撞开启 |
| `SelfCollisionThickness` | **10.0** | 与 `CollisionThickness=1.0` 相差 10 倍 |
| `SelfCollisionFriction` | 0.0 | |
| `bUseSelfIntersections` | False | |
| `CollisionThickness` | 1.0 | |
| `FrictionCoefficient` | 0.5 | |
| `DampingCoefficient` | 0.0669670 | |
| `LocalDampingCoefficient` | 0.0 | |
| `GravityScale` | 1.0 | |
| `TotalMass` / `Density` | 0.5 / 0.35 | |
| `Drag` / `Lift` | 0.07 / 0.035（Low=High） | |
| `AnimDriveStiffness` | Low 0.0 / High 1.0 | |
| `AnimDriveDamping` | Low 0.0 / High 1.0 | |
| `TetherStiffness` / `TetherScale` | 1.0 / 1.0（Low=High） | |
| `SubdivisionCount` | **1** | 求解细分最低档 |

`Config/*.ini` 中**没有任何** Chaos / Cloth / Solver 相关覆盖项 —— 全部走引擎默认。

### 1.4 日志证据（决定性）

`Saved/Logs/ArcaneVanguard-backup-2026.10.21-12.25.54.log` 中共 **47 条**布料相关警告，4 个网格全部命中：

```
LogSkeletalMesh: Display: OnRegister[/Game/Wizard/Mesh/SK_wizard.SK_Wizard]:
  [ClothingSimulationFactoryNv] is currently unable to provide a fully functional
  simulation for each of this SkeletalMesh's clothing assets.
LogSkeletalMesh: Display: OnRegister[...]:
  The ClothingSimulationFactory property will now be automatically updated to use
  the most functional simulation that can be found.
```

- `ClothingSimulationFactoryNv` = UE4 时代的 **NvCloth** 工厂，UE5 已移除。说明该资产最后一次保存是在 UE4 管线里。
- 每次 `OnRegister` 引擎都会自动把工厂改成 Chaos（内存中改写，**不回写磁盘**），所以这条警告每次运行都会重现。
- 除这条之外，**没有任何**布料求解错误、NaN、收敛失败类日志。

### 1.5 运行时实例统计（"是否有多个实例"）

同一份 PIE 日志里 `SK_wizard` 的 `OnRegister` 按时间戳分布：

```
 2  12:03:04     2  12:03:05    20  12:03:53     2  12:03:57
22  12:03:58     2  12:04:17     4  12:07:02     4  12:07:13
 4  12:07:18     4  12:07:55     4  12:10:39
```

每 2 条日志对应 1 次注册。即**单次 PIE 内 `SK_wizard` 约有 10~11 个组件实例**，3 个皮肤变体各约 4 次。也就是一屏之内有 10 个以上独立的布料求解实例在跑。

### 1.6 组件级与动画配置

| 项 | 值 |
|---|---|
| 角色 BP `ThirdPersonCharacter`（DemoRoom） | 网格 `SK_wizard`，AnimBP `AnimBP_Wizard_ThirdPerson`，`cloth_blend_weight = 1.0`，**只有 1 个 SkeletalMeshComponent**（`CharacterMesh0`） |
| 组件 `disable_cloth_simulation` | False |
| `enable_update_rate_optimizations`（URO） | False |
| `visibility_based_anim_tick_option` | `ALWAYS_TICK_POSE` |
| `teleport_distance_threshold` | 300 |
| `physics_asset_override` | None（用资产自带的 `PHY_Wizard`） |
| `master_pose_component` | None |
| AnimBP `root_motion_mode` | `ROOT_MOTION_FROM_MONTAGES_ONLY` |
| 蓝图逻辑操作布料 | **无**（`ThirdPersonCharacter` 资产内不存在 `SetCloth*` / `EnableCloth*` 节点） |

### 1.7 一个可能出乎意料的发现：「实际运行角色」并不用巫师网格

- `DefaultEngine.ini`：`GlobalDefaultGameMode = /Script/ArcaneVanguard.ArcaneVanguardGameMode`
- `ArcaneVanguardGameMode` 构造函数：`DefaultPawnClass = /Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`
- 该 BP 的 CDO：网格 = `SKM_Quinn_Simple`，AnimBP = `ABP_Quinn_C`

**即默认可操控角色是 Mannequin，不含任何巫师布料。** 巫师网格在关卡里是**摆放的 `SkeletalMeshActor` 雕像**：

| 地图 | 巫师相关内容 |
|---|---|
| `ThirdPersonMap`（当前打开） | 1 个 `SK_Wizard` Actor，`anim_class = null`（**静态姿态，无 AnimBP**），cloth=1.0 |
| `Map_Wizard_Showcase` | `SK_wizard` + `SK_wizard_Dark` / `Fire` / `Ice` 雕像；**没有** `ThirdPersonCharacter` |
| `Map_Wizard_presentation` | 同上 |

> 这意味着：抽搐发生在**雕像的布料**上（静态姿态下布料仍持续求解），而不是"角色跑动时"。这一点会显著改变候选原因的排序（见 C5）。

---

## 二、诊断报告：候选原因

> 说明：以下每一条都只给「证据 → 假设 → 最小验证 → 判据 → 修复与风险」。**没有一条被认定为已确认原因。**

### C1　自碰撞厚度过大（10 cm）+ 自碰撞开启

**已观察到的证据**
- `bUseSelfCollisions = True`
- `SelfCollisionThickness = 10.0`，而 `CollisionThickness = 1.0`
- `SubdivisionCount = 1`（求解细分最低档，迭代收敛能力弱）
- 4 份布料拷贝参数完全一致，说明这是资产导入时就带的统一设定，不是某次误操作

**尚未确认的假设**
- 布料粒子平均间距远小于 10 cm（推测 1~3 cm 量级），于是每个粒子在 10 cm 半径内有大量邻居，自碰撞每步产生强烈互推；在 1 级细分下求解器无法收敛，表现为高频振荡。
- 该值可能来自 NvCloth 时代的语义（单位/含义与 Chaos 不同），迁移时按数值直接搬运。

**最小化验证步骤**
1. 在编辑器内用 Python 把 `ChaosClothConfig.SelfCollisionThickness` 临时改为 `1.0`（**只在内存改，不 save**，重启编辑器即还原），PIE 观察 section 10。
2. 若仍有轻微抖动，再临时把 `bUseSelfCollisions` 设为 False，PIE 观察。

**预期结果与实际结果的判断方式**
- 预期：改成 1.0 后抖动明显减弱；关掉自碰撞后抖动消失。
- 若**关掉自碰撞后依然抽搐** → C1 被排除，问题不在自碰撞（转向 C2 / C3 / C6）。
- 若改厚度无效但关自碰撞有效 → 说明是"开/关"这一项，而非具体数值。

**修复方案与风险**
- 方案：`SelfCollisionThickness` 降到 1.0~2.0（数值改动，完全可逆）；必要时再关 `bUseSelfCollisions`。
- 风险：厚度过小 → 布料自身穿插（衣摆穿进身体或自己）；完全关闭自碰撞 → 穿插概率显著上升。需要配合 `bUseSelfIntersections=False` 的现状一起评估。

---

### C2　NvCloth → Chaos 迁移遗留

**已观察到的证据**
- 47 条 `ClothingSimulationFactoryNv is currently unable to provide a fully functional simulation` 警告，4 个网格全部命中
- 网格导入源路径为 `C:/Program Files/Zbrush/ZProjects/Projects/Wizard/Rig/SK_wizard.fbx`（UE4 时代资产）
- 资产磁盘上仍序列化为 NvCloth 工厂，每次注册被引擎在内存中改写为 Chaos，**从未回写**

**尚未确认的假设**
- 参数从 NvCloth 迁移到 Chaos 时是按数值搬运的，语义/单位未必等价；尤其自碰撞厚度、阻尼这类"经验值"参数最容易失真。
- 每次启动都重新做一次工厂改写，可能伴随一次内部重建，导致首帧状态不稳定。

**最小化验证步骤**
1. 备份 `SK_wizard` 后，在编辑器里对它执行一次 Save（让 Chaos 工厂写回磁盘），重启编辑器，确认 47 条警告是否消失。
2. 在"已保存为 Chaos 工厂"的状态下 PIE，与保存前的表现对比。

**预期结果与实际结果的判断方式**
- 预期：警告消失。
- 若**警告消失但抽搐依旧** → 说明 C2 只是"脏数据"，不是抖动原因，可从候选里降级。
- 若抽搐程度有变化 → C2 与现象相关，需要继续比对具体参数。

**修复方案与风险**
- 方案：保存一次让工厂固化为 Chaos。属于"元数据清理"，不改变布料参数。
- 风险：Save 会写回资产（必须先备份）；若资产同时在别处被引用，需确认没有并发编辑。

---

### C3　布料与物理资产碰撞体穿插

**已观察到的证据**
- `PHY_Wizard` 只有 17 个标准两足骨骼体，**没有**裙摆/披风/下摆专用碰撞体
- 布料区域是 section 10（`armor3_01`），位置靠近骨盆与大腿
- 组件未设置 `physics_asset_override`，用的是资产自带的 `PHY_Wizard`
- `CollisionThickness = 1.0`，`FrictionCoefficient = 0.5`

**尚未确认的假设**
- 静态姿态（当前关卡那个 Actor 是 `anim_class = null`）下，布料粒子初始位置与 `pelvis` / `thigh_l` / `thigh_r` 胶囊体存在穿插，Chaos 每帧把它们推开，形成持续抖动。

**最小化验证步骤**
1. 临时把 `PHY_Wizard` 从网格上解绑（`skm.set_editor_property('physics_asset', None)`，仅内存，不 save），PIE 观察。
2. 或者临时把布料区域的碰撞体（thigh/pelvis）的碰撞响应关掉。

**预期结果与实际结果的判断方式**
- 预期：解绑后抖动消失或大幅减弱 → 指向碰撞穿插。
- 若解绑后**抖动不变** → 排除 C3，问题在布料求解本身（C1 / C6）。

**修复方案与风险**
- 方案：调整 `pelvis` / `thigh` 碰撞体尺寸，或给布料区域加专用碰撞体，或调整 `CollisionThickness`。
- 风险：碰撞体改小 → 布料穿模；改大 → 挤压加剧。这是最需要逐次微调的一项。

---

### C4　多实例 / 多布料拷贝

**已观察到的证据**
- 单次 PIE 内 `SK_wizard` 约 10~11 个组件实例，3 个皮肤各约 4 个
- 4 个网格各自**内嵌**一份同名的 `SK_wizard_Clothing_0`（`outer` 分别是各自的 SkeletalMesh），是 4 份独立拷贝
- 4 份拷贝的参数逐项比对**完全一致**
- 角色 BP 只有 1 个 SkeletalMeshComponent（不存在重复挂载）

**尚未确认的假设**
- 实例数量本身不会产生抖动，但会让"多处抽搐"的观感放大；并且改一份拷贝不会影响其余三份，容易造成"改了没用"的误判。
- 若某些 Actor 的 `cloth_blend_weight` 或 LOD 与其它不同，可能出现"只有某个抽搐"的差异 —— 目前无法在不加载地图的前提下确认每个 Actor 的组件参数。

**最小化验证步骤**
1. 在 Showcase 里临时隐藏除 1 个以外的所有巫师 Actor，只留一个，PIE 观察是否仍抽搐。
2. 逐个切换剩余 Actor，确认是否**所有**实例都抽搐。

**预期结果与实际结果的判断方式**
- 若单个实例也抽搐 → 与实例数量无关，C4 排除（仅剩"改一份不影响其它三份"的工程注意事项）。
- 若只有特定实例抽搐 → 需要单独比对那个 Actor 的组件参数。

**修复方案与风险**
- 方案：无（这是"防止误判"的检查项）。若确认需要统一，可把 4 份内嵌布料替换为**共享的外部布料资产**，但这属于资产结构变更，风险较高，非必要不做。

---

### C5　动画 / 根运动 / 移动耦合

**已观察到的证据**
- 当前关卡的 `SK_Wizard` Actor `anim_class = null`（静态姿态，无 AnimBP）
- 组件 `ALWAYS_TICK_POSE`，URO 关闭，`teleport_distance_threshold = 300`
- AnimBP `root_motion_mode = ROOT_MOTION_FROM_MONTAGES_ONLY`
- 蓝图里没有任何布料相关逻辑

**尚未确认的假设**
- 若用户看到抽搐的实例是带 AnimBP 的（如 DemoRoom 的 `ThirdPersonCharacter`），则动画驱动 + 布料可能互相拉扯；若是静态雕像，则此项可直接排除。

**最小化验证步骤**
1. 静止不动（不做任何输入）时观察是否仍抽搐。
2. 对比"静态雕像"与"带 AnimBP 的角色"两种实例的表现差异。

**预期结果与实际结果的判断方式**
- 静止也抽搐 → 排除动画/根运动，指向 C1 / C3 / C6。
- 只有移动时抽搐 → 指向此项，需要检查根运动与布料重置。

**修复方案与风险**
- 方案：若确认，考虑关闭根运动对布料的影响，或调整 `teleport_distance_threshold`。
- 风险：改根运动模式会影响角色移动表现，影响面较大，放在最后考虑。

---

### C6　求解细分 / 迭代不足

**已观察到的证据**
- `SubdivisionCount = 1`（最低档）
- 工程 ini 无任何 Chaos/Cloth 覆盖，求解器迭代走引擎默认
- 同时开着自碰撞且厚度 10

**尚未确认的假设**
- 在"自碰撞开启 + 厚度偏大"的组合下，1 级细分不足以收敛，抖动是数值不收敛的表现，而非参数"错误"。

**最小化验证步骤**
1. 临时把 `SubdivisionCount` 提到 2 或 3（仅内存），PIE 观察。
2. 观察是否伴随明显的帧率下降。

**预期结果与实际结果的判断方式**
- 抖动明显减轻 → 指向收敛问题，可作为 C1 的补充手段。
- 抖动不变 → 排除，回到 C1 / C3。

**修复方案与风险**
- 方案：提高细分；或在 `ChaosClothSharedSimConfig` 上提高迭代次数。
- 风险：性能开销上升，且 10 个实例同时跑布料时成本会放大。

---

### 已排除的候选

| 候选 | 排除依据 |
|---|---|
| 蓝图运行时逻辑改动布料 | `ThirdPersonCharacter` 资产内不存在任何 `SetCloth*` / `EnableCloth*` 节点；组件 `disable_cloth_simulation` 恒为 False |
| 角色身上重复挂载多个 SkeletalMeshComponent | 角色 BP 仅 `CharacterMesh0` 一个 SkeletalMeshComponent |
| 多个分件各自带布料互相干扰 | `SeparatedMesh/*` 全部 0 布料资产 |
| 4 份布料配置不一致 | 逐项比对完全一致 |
| 披风/衣摆的布料抽搐 | 披风（section 17）与 `armor3_02`（section 11）**没有布料**，不参与模拟 |

---

### 无法确认的项（如实说明）

以下项目 MCP 的 Python 反射读不到，**未经验证**，不做任何推断：

1. 布料粒子数 / 约束数 / 平均粒子间距（`ClothingAssetCommon.LodData` 未进 Python 反射）—— 这直接决定 C1 的严重程度，是**最该补的一项**。
2. `PHY_Wizard` 各 body 的碰撞类型、是否与布料区域穿插（`PhysicsAsset` 的 body/constraint 属性未反射）。
3. `EdgeSpringStiffness` / `BendingSpringStiffness` 等弹簧刚度（未反射）。
4. 关卡内每个巫师 Actor 的 `cloth_blend_weight` 是否一致（需加载地图，未做）。
5. 用户实际看到抽搐的是哪个地图 / 哪个 Actor —— 从现有数据无法判定，见 C5。

---

## 三、修复计划

**前置约束：在获得授权前，不执行任何修改。** 以下全部是计划。

### 0. 备份（任何改动之前）

- 目录备份：`SK_wizard`、`SK_wizard_Dark`、`SK_wizard_Fire`、`SK_wizard_Ice`、`PHY_Wizard` → 复制到 `/Game/Wizard/_Backup_ClothDiag/`（Content 内副本，可随时回滚），同时把磁盘文件复制一份到工程外。
- 采用"副本资产"而不是覆盖原资产，原资产保留不动。

### 1. 零风险观察（不改任何资产）

| 步骤 | 目的 |
|---|---|
| `p.ClothPhysics 0` / `1` 对照 | 已知有效，作为基准确认"抖动确实来自布料" |
| 静止 vs 移动 | 判定 C5 |
| 单实例 vs 多实例 | 判定 C4 |

### 2. 逐项最小验证（一次只改一个因素，仅内存不存盘）

推荐顺序（可逆性从高到低、侵入性从小到大）：

1. **C1**：`SelfCollisionThickness` 10.0 → 1.0（仅内存）→ PIE
2. **C1 补充**：若无效，`bUseSelfCollisions` True → False（仅内存）→ PIE
3. **C3**：`physics_asset` 临时置 None（仅内存）→ PIE
4. **C6**：`SubdivisionCount` 1 → 2（仅内存）→ PIE
5. **C2**：以上都定位后，再考虑保存一次固化 Chaos 工厂

> 关键技术点：`set_editor_property` **不调用 `save_asset`** 时只改内存，PIE 会使用内存中的资产对象，**重启编辑器即自动还原**。这让每一轮验证都是零风险、可重复的。

### 3. 保留衣摆与披风的动态效果

- 现状：披风和衣摆**本来就没有布料模拟**（是纯蒙皮）。所以调整 section 10 的布料参数**不会削弱任何现有动态效果** —— 目前全模型只有 `armor3_01` 这一块有布料。
- 修复目标是"让这一块稳定"，而不是"去掉动态"。因此**优先调数值（厚度/阻尼/细分），不优先关闭自碰撞**；只有在数值调整无法收敛时才考虑关自碰撞。
- 若后续希望披风也有动态，那属于**新增功能**（给 section 17 加布料资产），不在本次修复范围。

### 4. 每次修改后的验证要求

- 必须在 UE 5.4 编辑器中实际 PIE 观察，不接受"看参数觉得没问题"。
- 记录：改前/改后的参数值、PIE 时长、抖动是否可见、帧率变化。
- **若无法实际验证（例如当前无法打开对应地图），明确标注"未验证"，不宣称修复成功。**

### 5. 落盘与回滚

- 验证通过后才 `save_asset`，且一次只保存一个网格。
- 4 份内嵌布料需要**分别**应用（同一份参数改 4 次），或先改 `SK_wizard` 验证通过后再批量同步到 3 个皮肤。
- 任一环节表现变差 → 直接丢弃内存修改 / 从 `_Backup_ClothDiag` 恢复。

---

## 四、需要用户确认的三件事

1. **抽搐具体出现在哪个地图、哪个 Actor 上？**（ThirdPersonMap 的雕像 / Showcase 的雕像 / DemoRoom 的可操控角色）
2. 是**持续抖动**，还是只在特定动作（行走、转身、施法）时抖动？
3. 是否可以授权我按上面的顺序做**内存级（不存盘）**验证？这一步不改任何磁盘资产，重启编辑器即完全还原。

---

# 五、验证结果与最终结论（2026-10-21 21:04 补记）

## 5.1 用户补充的关键前提

- 抽搐对象：**`ThirdPersonMap` 中摆放的 `SK_Wizard`（`SkeletalMeshActor`，`anim_class = null` 静态姿态）**，持续抖动。
- Showcase / DemoRoom 两张地图已被用户删除（相关 BP 资产仍在）。
- **未来这个骨骼网格体会作为玩家操控的角色** → 性能为第一优先。

## 5.2 C1 已确认（A/B 对照，非经验判断）

在同一 PIE 会话内、只切换 `bUseSelfCollisions`、其余参数全部不变：

| 状态 | 观察结果 |
|---|---|
| `bUseSelfCollisions = False` | **不抖** |
| `bUseSelfCollisions = True`（厚度 10cm） | **抖** |

→ C1（自碰撞厚度过大导致求解不收敛）确认为根因。C2/C3/C4/C5/C6 未再单独验证，因为根因已定位。

## 5.3 量化数据（补上第一版报告中"无法确认"的缺口）

通过 `ClothingSimulationInteractor` 在 PIE 中实测：

| 指标 | 值 |
|---|---|
| 布料实例数 | 1 |
| 动态粒子 | **959** |
| 运动学粒子 | 226 |
| 求解迭代 | 3~4 |
| 子步数 | 1 |
| 布料段 | 仅 section 10（`armor3_01`） |

由 959 个动态粒子推算平均间距约 **1.5–2 cm**；自碰撞厚度 **10 cm** 意味着每个粒子的碰撞球内有上百个邻居，而求解器只有 3–4 次迭代、1 个子步 → 数值上无法收敛。这是抖动的直接机制。

## 5.4 参数扫描结果

**第一轮（自碰撞开关）**：绿/红标签交替，用户确认「关→不抖，开→抖」。

**第二轮（厚度扫描，自碰撞始终开）**：`10 / 5 / 3 / 2 / 1 / 0.5 cm`，用户判定 **0.5 最好**。

**第三轮（动画 + 位移条件下复测）**：给 PIE 中的网格挂 `AnimBP_Wizard_ThirdPerson`、再改用单节点播 `Anim_Wizard_ThirdPersonRun`，并以 ±150cm / 3s（峰值约 3 m/s）往返位移，三档 `OFF / 0.5 / 1.5` 对比。

**第四轮（CollisionThickness 扫描，自碰撞关）**：`1.0 / 2.0 / 3.5`，用于评估能否在不恢复自碰撞的前提下减少穿模。

## 5.5 方案取舍（性能优先）

| 方案 | 稳定性 | 自穿插防护 | 性能 | 结论 |
|---|---|---|---|---|
| **关闭自碰撞** | 好 | 无 | **最低** | **采纳** |
| 厚度 0.5 cm | 好 | ≈无 | 仍付自碰撞开销 | 否决：付了开销拿不到防护 |
| 厚度 1.5–2.0 + 提高迭代 | 待验证 | 有 | 明显上升 | 否决：玩家角色不划算 |
| 提高 `SubdivisionCount` | 好 | 有 | 最差（粒子数成倍增长） | 否决 |

用户确认：**穿模可接受**，以性能为第一优先。因此最终只做一项改动。

## 5.6 已实施的修复

**唯一改动：`bUseSelfCollisions` 由 `True` 改为 `False`。其余所有布料参数保持原值**（含 `SelfCollisionThickness` 仍为 10.0、`CollisionThickness` 仍为 1.0、`DampingCoefficient`、`GravityScale`、`TotalMass` 等均未动）。

应用到 4 个网格（参数完全一致的内嵌布料拷贝）：

- `/Game/Wizard/Mesh/SK_wizard`
- `/Game/Wizard/Mesh/Skin/SK_wizard_Dark`
- `/Game/Wizard/Mesh/Skin/SK_wizard_Fire`
- `/Game/Wizard/Mesh/Skin/SK_wizard_Ice`

## 5.7 备份

原始资产已复制到 `Docs/AssetBackup_2026-10-21/`（4 个 .uasset，保留 2023 年原始时间戳）。回滚方式：把备份文件覆盖回 `Content/Wizard/Mesh/` 对应路径。

## 5.8 修复后的验证

| 验证项 | 结果 |
|---|---|
| 保存后新起 PIE 的日志中 `ClothingSimulationFactoryNv` 警告数 | **0 条**（修复前每次注册 2 条/网格）→ 证明磁盘上的工厂已固化为 Chaos |
| 布料是否仍在模拟 | 是，959 动态粒子 + 226 运动学粒子 |
| 资产完整性 | 20 个材质段 / 1 个 LOD / 1 个布料资产 / 布料段仍为 section 10 / 骨架 `SKE_Wizard` / 物理资产 `PHY_Wizard` 均未变 |
| 文件体积 | 34.3MB → 7.9MB（UE5 以 Oodle 重新压缩，非数据丢失；材质、骨骼、布料、物理资产引用均已核对） |

## 5.9 已知代价与遗留事项

1. **失去衣料自穿插防护**（用户已确认可接受）。大幅折叠或卷动时，衣料可能穿过自身。
2. **网格只有 1 个 LOD** → 布料永远按满分辨率模拟，没有降级余地。若作为玩家角色长期使用，建议后续补 LOD（超出本次范围）。
3. **`CollisionThickness` 保持 1.0 未调**。若日后觉得布料穿身体明显，可单独调这一项（成本远低于自碰撞）。
4. **尚未验证磁盘内容**：编辑器未重启，无法通过重新加载确认落盘值。已通过「NvCloth 警告消失」间接证明保存生效（该警告只取决于序列化的工厂类）。严格验证需重启编辑器后复查。
5. **未做 CPU 开销实测**。编辑器内帧时间噪声远大于布料差异，`stat unit` / Insights 需独立进程，本次未执行。

