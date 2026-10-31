# TaskBoard — 任务板

- 最后更新：2026-10-30
- 状态定义：Backlog / Ready / In Progress / Blocked / Implemented / Verified / Done
- 规则：`Implemented` ≠ `Done`。只有验收条件全部满足且有证据才可标 `Done`。

## 里程碑总览

| 里程碑 | 目标 | 状态 |
|---|---|---|
| M0 工程基线 | 可重复打开 / 编译 / 运行的工程 | **✅ Verified**（2026-10-24） |
| M1 角色框架与输入 | 项目自己的角色扩展点，不破坏模板行为 | **✅ Verified**（2026-10-26） |
| M2 GAS 最小骨架 | ASC 初始化 + 一个属性 + 一个效果跑通 | **✅ Verified**（2026-10-26） |
| M3 伤害、死亡、普攻 | 玩家攻击 → 敌人死亡的完整链路 | **✅ Verified**（2026-10-30，PIE 完整一局跑通） |
| M4 资源、冷却、三技能 | Q / E / R 规则链路 | **进行中**（10-30，GA 基类 + Q 数据层完成，待建 `GA_Q_Buff` 蓝图；CD/Mana 未做） |
| M5 敌人 AI 与战斗场景 | 可重复游玩的遭遇战 | **进行中**（敌人 AI ✅ 10-29；剩 AV-049 收尾） |
| M6 UI、任务插件与交互 | 有目标与结果的体验 | Backlog |
| M7 表现与打磨 | 可读性与展示质量 | Backlog |
| M8 作品集与复盘 | 可展示、可解释、可验证 | Backlog |

## 第一批任务（策划案 §15）

| ID | 任务 | 状态 | 验收要点 | 备注 |
|---|---|---|---|---|
| AV-001 | 确认开发环境与工程基线 | **In Progress** | 环境已记录；工程可编译（已验证）；角色移动/镜头/跳跃正常（自动化已验证，手感待人工）；Git 基线（已豁免）；DevelopmentStatus 存在 | 仅剩人工 PIE 手感确认（AV-010） |
| AV-002 | 扫描模板架构 | **Verified** | 真实类名、蓝图与 C++ 关系、插件与模块依赖、建议扩展点与风险 | 结果见 `TechnicalDesign.md` §2 |
| AV-003 | 建立项目技术骨架 | Backlog | 方案与实际结构一致；说明保留/扩展哪些类；新增文件范围明确；改后仍可编译 | 依赖 M0 验收 |
| AV-004 | 审计任务插件 | Ready | 公共接口、核心类、数据结构、模块依赖；已验证/未验证/未知；最小测试矩阵；进度语义 | 插件源：`D:/UEProject/QuestSystemTest/Plugins/QuestSystem`（v0.1.0，Beta） |
| AV-005 | 建立 GAS 最小验证计划 | Ready | API 与模块依赖按 5.4 核实；只含必要类与资产；明确初始化与测试方式 | 引擎侧 `GameplayAbilities.uplugin` 已确认存在 |

## 近期任务拆解（建议）

| ID | 任务 | 状态 | 依赖 |
|---|---|---|---|
| AV-006 | 建立 Git 基线提交 | **Waived**（负责人豁免，ADR-002） | 恢复条件：Wizard 修复完成 |
| AV-007 | 实跑 UBT 编译并记录结果 | **Verified** | clean build 11/11 成功，0 错误 0 警告，75.57s |
| AV-008 | 清理模板残留（ProjectName） | **Done** | 已改为 `Arcane Vanguard`，备份 `DefaultGame.ini.bak-20261021-205004` |
| AV-009 | 建立 `Content/ArcaneVanguard/` 目录骨架 | Backlog | AV-003 |
| AV-010 | PIE 验收：移动 / 镜头 / 跳跃 / 输入不重复绑定 | **Ready（需人工）** | 自动化部分已通过（Pawn 生成/相机/参数/tick），手感需负责人 Play 一次 |
| AV-011 | 确认玩家 AnimBP：`ABP_Quinn_C` → `AnimBP_Wizard_ThirdPerson`？ | **Done**（负责人已切换） | 需实机确认观感 |
| AV-012 | 确认 `Content/Wizard` 许可（现在它是玩家模型） | **Ready** | 见 `AssetRegister.md`；M7 前必须闭合 |
| AV-013 | 玩家武器挂载（法杖/书挂到 `hand_r`） | **已回滚**（2026-10-24） | 当时 Verified（手→杖距离恒定 96.8907）。**2026-10-24 按负责人要求 C++ 全部回滚到模板，武器组件已删除**；代码存档 `Docs/Backup_TestSource_20261024/` |
| AV-015 | 还原 DemoRoom 角色蓝图（用于对照） | **Done** | 已还原到原路径；实测**无武器组件、无相机**；全库确认原始工程从未引用过法杖资产 |
| AV-016 | 方案 B：验证现有动画是否有握持姿势 | **Verified** | 结论：**没有**。手指伸直、杖身穿掌；证据 `Docs/GripTest_20261021/` |
| AV-017 | 解决握持姿势（KI-013） | **In Progress**（2026-10-22，10-26 方案修正） | A4 方案：AnimBP 加 15 个 `Transform (Modify) Bone` 节点。**⚠️ 2026-10-26 方案修正：不要再切 `weapon_r` 挂点** —— 现有校准（`StickSocket` 的 −124cm 偏移 + `CombatGripOffset` 90cm）建立在 `StickSocket` 上，切挂点等于推翻重来。**只做手指弯曲，不动挂点**。旋转值已实测标定；指南 `Docs/KI013_FingerGrip_20261022/README.md`；备份 `.bak-20261022-111500`；验收脚本 `Saved/Scripts/ki013_verify_after.py` |
| AV-018 | **P1 竖直约束**（解决 KI-014 法杖颠倒） | **已回滚**（2026-10-24） | 当时 Verified（三姿势 pitch=0/roll=0）。C++ 回滚后该功能不再存在；证据保留在 `Docs/P1Verify_20261022/` |
| AV-019 | **武器自动显隐**（`Magic_Attack` 等无武器动作） | **已回滚**（2026-10-24） | 当时 Verified。C++ 回滚后该功能不再存在 |
| AV-020 | **资产溯源**（Mixamo / UECandy） | **Done** | 9/19 含 `mixamo.com`，19/19 含 `UECANDY.COM`；已写入 `AssetRegister.md` |
| AV-021 | **调试按键：动画逐个播放** | **已回滚**（2026-10-24） | 当时 Verified（1~5 键播动画）。C++ 回滚后按键功能不再存在；说明保留在 `Docs/DebugKeys_20261022/` |
| AV-022 | **武器规则细化**（隐藏名单 + 死亡掉杖） | **已回滚**（2026-10-24） | 当时 Verified（收杖 + 死亡掉杖）。C++ 回滚后该功能不再存在；证据保留在 `Docs/WeaponHide_20261022/` |
| AV-023 | 寻找"普通攻击"连击动画（3/4/5 连击） | **✅ Done**（2026-10-26） | 负责人自行选定并搭好三段蒙太奇 `AM_NormalAttack`（Section `Combat_01/02/03`，Slot `DefaultSlot`，总长 2.1667s），**未采购** `Essential Magic Staff Animation Pack`。骨架仍为 SKE_Wizard，未切换 Manny | 详见 AV-036 |
| AV-024 | **整理交接文档给下一个 AI** | **Done** | `Docs/HANDOFF_20261022.md`（含完整提示词 + 踩坑清单 + 未完成项） |
| AV-014 | 动画质量提升方案选型（Mixamo 重定向 / 调 AnimBP / 动画包） | **Ready** | 见 `TechnicalDesign.md` §5.7；骨架是 UE4 Mannequin 命名，重定向成本低 |
| AV-025 | **整理美术资产全景需求清单** | **Done** | `Docs/ArtAssetPlan.md`：按 M3–M8 分 P0/P1/P2，含现状统计（敌人/VFX/音频/UI/地图全为 0）、搜集渠道、授权红线、4 批搜集顺序 |
| AV-026 | **Notion 文档库建立与全量同步** | **Done**（2026-10-23） | 数据库 `📚 工程文档库`（`d1cc1ec0b4fe4fb986d3025695766abf`）；22 份文档全部入库；两个旧子页面已移入库内；同步规则写入 `AgentRules.md` §5 |

## M0 收尾 / M1 计划任务（2026-10-24 制定）

详见 `Docs/Plan_M0_M1_20261024.md`

| ID | 任务 | 状态 | 验收要点 | 依赖 |
|---|---|---|---|---|
| AV-027 | **PIE 手感验收**（M0 唯一未闭合项） | **✅ Done**（2026-10-24） | 负责人人工验收：**移动与镜头正常** | 无 |
| AV-028 | 回滚后文档对齐 | **✅ Done**（2026-10-24） | `TechnicalDesign` §2.5 已更新；`Architecture_20261024` 已建；M0 转 Verified（含 Git 豁免说明） | AV-027 |
| AV-029 | **角色框架与输入架构决策** | **✅ Verified**（2026-10-26） | ADR-009 + ADR-010 已转 **Accepted**（M1 验收通过），含理由 / 备选方案 / 风险 / 验证方式 | 无 |
| AV-030 | 新建 `AArcaneVanguardPlayerController` | **✅ Verified**（2026-10-24） | 编译通过；`BP_AV_PlayerController` 已建（IMC=IMC_Default）；运行时日志 `[Input] 已注册 MappingContext 'IMC_Default'` | AV-029 |
| AV-031 | 统一输入绑定生命周期 | **✅ Verified（结构+日志）**（2026-10-24） | IMC 注册从 Character 迁到 PC::BeginPlay，EndPlay 成对移除；日志实测**启动 1 次注册、退出 1 次移除**。⚠️ 原计划的「连续进出 PIE 5 次」未执行（见备注） | AV-030 |
| AV-032 | ASC 初始化预留点 | **✅ Verified**（2026-10-24） | `PossessedBy` / `OnRep_PlayerState` 空重写 + `TODO(M2)` 注释；未引入 GameplayAbilities 模块 | AV-030 |
| AV-033 | **M1 验收与文档** | **✅ Verified**（2026-10-26） | 四条验收：①移动镜头正常 ✅ ②输入无重复绑定 ✅(日志) ③编译通过 ✅ ④类职责有文档 ✅。2026-10-26 补齐：`Architecture_20261024` 新增 **§4.4 PlayerController 生命周期**，并把 §4.3 Tick / §6 动画链路 / §7 武器子系统 全量对齐到当前实现 | AV-031、AV-032 |
| AV-034 | **武器 Actor 接入**（Spawn + Attach） | **Verified**（2026-10-24） | `AArcaneVanguardWeapon` + `SpawnAndAttachWeapon()`；实测武器世界坐标与 `StickSocket` 偏差 `(0,0,0)`，附着父级 `CharacterMesh0`，碰撞关闭。坐标校准见 AV-035 | — |

## M1 战斗层任务（2026-10-25 ~ 10-26）

| ID | 任务 | 状态 | 验收要点 | 依赖 |
|---|---|---|---|---|
| AV-035 | **战斗握持偏移**（武器沿杖身前伸） | **✅ Verified**（2026-10-26，2026-10-27 参数调整） | 实测根因：`StickSocket` 在 `hand_r` 上带 **−124cm** 偏移，手卡在杖身 62%（前段仅 73.8cm）。实现 `CombatGripOffset` / `CombatGripInterpSpeed` / `bAutoTrackOwnerCombat` + `CaptureBaseTransform()`（**必须在 Attach 之后调**）。<br>**参数已变更**：2026-10-27 把 `CombatGripOffset` 从 `(0,0,25)` 调到 **`(0,0,90)`**、`CombatGripInterpSpeed` 从 1.0 调到 **10.0**。90cm 下实测：待机 64.1%/69.7cm → 战斗 **18.2%/158.9cm**（详见 `Architecture` §7.4） | AV-034 |
| AV-036 | **普攻三段连击**（代码 + Notify + 蒙太奇） | **✅ Verified**（2026-10-26） | 新增 `AVCombatNotifyStates.h/.cpp`（`AVComboWindow` / `AVAttackWindow`）；角色新增 `Combat\|Combo` 状态机（`ComboIndex` / 窗口 / 输入缓冲 / 兜底）。**方案 B**：窗口只收输入，用 `Montage_SetNextSection` 在段尾过渡，后摇完整播完。验证：1→2→3 正常、最后一段忽略、失步场景不往回跳 | AV-023 |
| AV-037 | **第三段攻击动画重做** | **✅ Done**（2026-10-26） | 负责人用 Animation Mode 重调骨骼后烘焙替换。核验：蒙太奇总长仍 2.1667s、三段长度不变、5 个 Notify 位置未变 → 无需返工。资产清理：删除陈旧 `Combat_03` / `Combat_03_Copy`，烘焙产物重命名为 `Combat_03` | AV-036 |

## 🚀 冲刺：5 天做出基本战斗闭环（2026-10-27 ~ 10-31）

**目标（10-31 结束时的验收）**：能完整玩一局 ——
玩家普攻/技能打敌人 → 敌人受伤死亡 → 敌人 AI 追击反击 → 玩家受伤 → 可重开。

**压缩范围**：M2（GAS 骨架）+ M3（伤害死亡）+ M4（资源冷却技能）+ M5（敌人 AI）四合一。

### ⚠️ 三个前置约束（状态已更新）

| # | 约束 | 决策 |
|---|---|---|
| C1 | ~~敌人资产为 0~~ → **已解决**（2026-10-27） | 负责人导入 `Content/Stone_Golem/`（UE4 Mannequin 命名，与 `SKE_Wizard` 同体系）+ `Content/SpearAnimationsV2/`（402 资产）。**石头人无死亡动画 → 用 ragdoll**；攻击用 `AB_1H_*`（负责人实测 2 个重定向后可用：右手下砸、右手下冲拳） |
| C2 | **5 天做不出完整 3 技能** | 技能按"1 个完整 + 2 个复用变体"做，动画映射现有 5 个 Mixamo 技能序列 |
| C3 | ~~构建环境不稳定~~ → **已解决**（2026-10-26） | 根因是 UBT `BuildConfiguration.xml` 里残留 `<bAllowUBAExecutor>false</bAllowUBAExecutor>`，删除后 7/7 通过。**重启机器无效** |

### 日程（每天必须产出可验证的东西）

| 日期 | 主线 | 任务 ID | 当天验收（做不到就算没过） |
|---|---|---|---|
| **10-27 (D1)** | 修环境 + GAS 骨架 | AV-038 / 039 / 040 | ✅ **已完成**：KI-018 修好（UBA 配置残留）、ASC 可取、`Health=100`。GE 扣血延到 D2 一起验 |
| **10-28 (D2)** | 普攻接伤害 + 敌人靶子 | AV-041 / 042 | 普攻打到敌人 → 敌人血量下降 → 归零后 ragdoll 倒地并销毁。**代码已提前完成（10-27），待 PIE 行为验证** |
| **10-29 (D3)** | 敌人 AI + 反击 | AV-043 / 044 | 敌人会追、会打、玩家会掉血（**闭环在这一天合拢**） |
| **10-30 (D4)** | 技能 + 冷却资源 | AV-045 / 046 | 按 Q 能放技能并造成伤害；CD 期间按不出；Mana 不够放不出 |
| **10-31 (D5)** | 收尾与可玩性 | AV-047 / 048 / 049 | 死亡→重生可循环；双方血条可见；能完整打一局并录屏 |

### 任务清单

| ID | 任务 | 状态 | 验收要点 | 依赖 |
|---|---|---|---|---|
| AV-038 | **启用 GAS 模块**（L2 变更） | **✅ Verified**（2026-10-26） | `.uproject` 启用 `GameplayAbilities`（已确认其 `.uplugin` 的 `EnabledByDefault = false`，必须显式启用）；`Build.cs` 加 `GameplayAbilities`/`GameplayTags`/`GameplayTasks`；编译 7/7 通过、UHT 重新生成 `ArcaneVanguardAttributeSet.gen.cpp` | D1 |
| AV-039 | **ASC 挂到 Character** | **✅ Verified**（2026-10-26） | 实现 `IAbilitySystemInterface`；`CreateDefaultSubobject<UAbilitySystemComponent>`；`PossessedBy` 补 `InitAbilityActorInfo(this, this)`。**实测日志**：`[GAS] ActorInfo 已初始化：ASC=AbilitySystemComponent，Owner/Avatar=BP_ThirdPersonCharacter_C_0` | AV-038 |
| AV-040 | **AttributeSet + 初始血量** | **Implemented（属性已验证，GE 待测）**（2026-10-26） | `UArcaneVanguardAttributeSet`（`Health`/`MaxHealth` + `PreAttributeChange` 钳制），构造函数 `InitHealth(100)`；**必须显式 `AddAttributeSetSubobject`**（UAttributeSet 是普通 UObject，ASC 不会自动发现）。**实测日志**：`[GAS] 属性集已注册：Health=100.0 / MaxHealth=100.0`。⏳ 剩余：创建测试 GE（-30）验证扣血到 70 | AV-039 |
| AV-041 | **普攻接入伤害** | **Implemented（待 PIE 行为验证）**（2026-10-27） | 新增 `UGE_ArcaneDamage`（原生 GE：Instant + Health Additive + **SetByCaller `Data.Damage`**）+ `UArcaneVanguardCombatStatics::ApplyDamage`（**ADR-005 唯一入口**）。`UAnimNotifyState_AVAttackWindow::NotifyBegin` → `SetAttackWindowOpen(true)`（清空命中记录）→ `PerformAttackHitCheck`（胶囊体中心起扫的球形 Sweep，`ECC_Pawn`，段内去重）。**A+B 兼容**：`DamageEffectClass` 留空走原生 GE，填 BP 子类则覆盖；Notify 上 `DamageOverride` 可按段配不同伤害。**已验**：GE CDO = Instant / 1 Modifier / Health / Additive / SetByCaller `Data.Damage`；`apply_damage` 与 `perform_attack_hit_check` 已暴露到蓝图。**待验**：PIE 里打石头人掉血 | AV-040 |
| AV-042 | **敌人 Actor（靶子）** | **Implemented（代码完成，BP 待负责人）**（2026-10-27） | `AArcaneVanguardEnemy`：ActorInfo 在 **BeginPlay** 初始化（静态靶子无 Controller）；Health 变化委托 → 归零切 **ragdoll** → 延迟销毁。<br>**新增 `MaxHealthOverride`**（2026-10-27）：玩家和敌人共用同一套 AttributeSet（初值写死 100），给敌人单独开了血量入口 —— BP 里填 300 就是 300 血，先设 MaxHealth 再补满 Health（顺序反了会被 `PreAttributeChange` 钳回 100）。<br>**负责人建 BP**：Mesh=`SKM_Stone_Golem`、PhysicsAsset=`PA_Stone_Golem_PhysicsAsset`、胶囊体 134/90、`MaxWalkSpeed` 600→**待降到 250**、Anim Class=`ABP_Enemy_Stone` | AV-040 |
| AV-043 | **敌人 AI** | **✅ 代码完成（BT 版）**（2026-10-27） | `AArcaneVanguardEnemyAIController` + **Behavior Tree**。决策全在 BT 资产：`BTService_AVUpdateTarget`（写 `TargetActor` / `EdgeDistanceKey`）+ `BTTask_AVAttack` + 引擎内置 `MoveTo`。距离/冷却用 Service / Cooldown Decorator 配，**不在 C++ 里**。转向交给 `bOrientRotationToMovement`（540°/s），不手写 `SetActorRotation` | AV-042 |
| AV-044 | **敌人攻击玩家** | **✅ Verified**（2026-10-30） | 敌人攻击动画 + Attack Window → 对玩家施加伤害；玩家受击血量下降、归零后进入死亡流程。PIE 实测：被石头人打死 → ragdoll → 3.8s 重生，循环多次无异常 | AV-043、AV-041 |
| AV-045 | **技能框架 + Q 技能** | **In Progress（仅基类）**（2026-10-30 核实） | `AArcaneVanguardGameplayAbility` 基类已存在（注释里写了 `ActivateAbility` / `PlayMontageAndWait` 的设计），**但没有任何具体技能子类**。按 Q 目前只有输入日志、无实际效果。<br>**待办**：写 1 个完整技能（Q），动画用现有序列 | AV-041 |
| AV-046 | **冷却与资源** | **In Progress（仅属性）**（2026-10-30 核实） | `Mana` / `MaxMana` 已在 `UArcaneVanguardAttributeSet` 里（构造函数 `InitMana`），**但没有 Cost / Cooldown 的 GE**，也没接进任何技能。验收标准不变：CD 中按不出、Mana 不足放不出 | AV-045 |
| AV-047 | **死亡与重生** | **✅ Verified**（2026-10-30） | Health 委托 → `HandleDeath`（取消技能 / 禁输入 / 停移动 / **ragdoll**）→ 3.8s 后 `Respawn`（回满血 + 传送回出生点 + 恢复输入 + 重新装配武器）。出生点在 `BeginPlay` 记录，不依赖 PlayerStart。<br>**增强（2026-10-30）**：`bUseRagdollOnDeath` 开关（默认开，关掉退回播 `AM_Wizard_Death`）；法杖脱手掉落（`EnablePhysicsDrop`，碰撞在 `OnConstruction` 配好）；自定义碰撞通道 `Weapon`/`Enemy` 避免武器撞飞 ragdoll 尸体 | AV-044 |
| AV-048 | **最小 UI** | Ready | 玩家血条 + 敌人血条（UMG 最简版）。**砍单优先级最低**，时间不够就用屏幕调试数字代替 | AV-044 |
| AV-049 | **战斗场景与收尾** | Ready | 一个可反复游玩的遭遇战；完整一局录屏；文档与 TaskBoard 同步 | 全部 |
| AV-050 | **敌人受击硬直** | **✅ Verified**（2026-10-30） | `OnHealthChanged` 检测到掉血（`NewValue < OldValue`，过滤初始化加血）→ `HandleHitReact`：<br>① `Montage_Stop`（**必须显式停**，否则攻击蒙太奇的 NotifyState 窗口会在受击动画下继续结算伤害）② `StopMovementImmediately` ③ 写 Blackboard `IsStaggered=true` → 随机播受击蒙太奇 → 定时器。<br>BT 侧：`Selector` 高优先级受击分支（Decorator `Is True` + **`Aborts Lower Priority`**）自动 Abort 战斗分支，`Wait` 0.6s。<br>**双路径**：BB 里没有 `IsStaggered` 键时自动降级为 `PauseLogic`（`bStaggerPausedAI` 保证 Pause/Resume 严格配对）。死亡时清计时器避免恢复尸体 | AV-044 |
| AV-051 | **武器显隐（施法时收杖）** | **Implemented（待 PIE 验证）**（2026-10-30） | 巫师自带的技能动画都不含法杖，施法时必须把法杖藏起来。<br>`AArcaneVanguardCharacter::SetWeaponHidden(bool)` / `IsWeaponHidden()`：**只做 `SetActorHiddenInGame`，不动碰撞** —— 武器碰撞在 `OnConstruction` 里配成"只 Block `WorldStatic`、其余全 Ignore"，本来就不会挡人；动 `SetActorEnableCollision` 有"关了恢复不回来"的风险。用 `GetAttachParentActor() != this` 判断"还在手上"，已脱手掉落的武器不受影响 | AV-045 |
| AV-052 | **普攻接入增强输入** | **Implemented（待负责人配 IA 资产）**（2026-10-30） | 之前普攻是蓝图里的测试函数，没走 Enhanced Input。<br>C++ 新增 `AttackAction`（`UInputAction*`，蓝图指定）+ `SetupPlayerInputComponent` 里 `BindAction(ETriggerEvent::Started)` → `OnAttackPressed`（复用已有的三段连击状态机）。用 `Started` 不用 `Triggered`：连击推进由内部窗口判定控制。<br>**接入后必须删掉蓝图里原来的调用节点**，否则一次按键触发两次、跳两段连击 | AV-029 |

### 砍单优先级（时间不够时按此顺序砍）

| 顺序 | 砍什么 | 替代方案 |
|---|---|---|
| 1 | AV-048 血条 UI | 用 `AddOnScreenDebugMessage` 打数字 |
| 2 | E / R 两个技能 | 只留 Q，把"三技能"记为 M4 未完成 |
| 3 | 敌人 AI 的转向/闪避 | 直线追击 + 固定攻击间隔 |
| 4 | 玩家受击动画 | 只扣血，不播受击 |

**绝对不能砍**：GAS 骨架（AV-038~040）、普攻伤害链路（AV-041）、敌人能死（AV-042）、敌人能打人（AV-043/044）。这四块缺一块就没有"闭环"。

## M1 收尾后的其他待办

| 顺序 | 事项 | 说明 |
|---|---|---|
| 1 | **AV-017 手指弯曲**（只做一半） | 15 个 `Transform (Modify) Bone` 节点解决"手指穿过杖身"。**建议不要再切 `weapon_r` 挂点** —— 现有校准（−124cm 偏移 + 90cm 战斗偏移）是基于 `StickSocket` 的，切挂点等于推翻重来。**冲刺期间可暂停** |
| 2 | AV-012 `Content/Wizard` 许可闭合 | M7 前必须完成 |

## 优先级说明

按 `ProjectBrief.md` §5：正确性与稳定性 > 核心战斗闭环 > 架构可解释性 > 表现 > 内容数量 > 非必要优化。
