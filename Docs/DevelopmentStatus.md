# DevelopmentStatus — 项目状态

- 最后更新：**2026-10-30 20:00**
- 当前里程碑：**M3 ✅ 战斗闭环已跑通（2026-10-30）→ M4 技能框架进行中**（GA 基类完成，待负责人建 `GA_Q_Buff` 蓝图）
- 记录人：主 Agent

> ⚠️ **2026-10-24 重大状态变更**：按负责人要求，**C++ 全部回滚到第三人称模板原始状态**（7 个源文件与引擎模板字节级一致）。
> 武器挂载、竖直约束、自动收杖、死亡掉杖、调试按键**均已从代码中删除**。代码存档于 `Docs/Backup_TestSource_20261024/`。
>
> ✅ **2026-10-26 更新**：武器子系统与战斗层**已重新接入并扩展**（AV-034 武器 Actor、AV-035 战斗握持偏移、
> AV-036 普攻三段连击）。上述「已回滚」的功能中，**武器挂载已恢复**；竖直约束 / 自动收杖 / 死亡掉杖 / 调试按键
> **仍未恢复**（代码未重建）。

## 1. 工程可编译状态

| 项 | 值 | 证据 |
|---|---|---|
| 引擎版本 | UE 5.4.4-35576357+++UE5+Release-5.4 | MCP `get_editor_status` |
| 引擎安装 | `C:/Program Files/Epic Games/UE_5.4` | 目录存在 |
| 编译工具链 | **Visual Studio 生成工具 2022，16.11.37327.17**（Build Tools，非完整 IDE） | `vswhere -latest` |
| 构建脚本 | `Engine/Build/BatchFiles/Build.bat` | 目录检查 |
| 模块 | `ArcaneVanguard`（Runtime / Default） | `ArcaneVanguard.uproject` |
| 目标平台 | Win64（Game + Editor 两个 Target） | `Source/*.Target.cs` |
| Target 设置 | `BuildSettingsVersion.V5`、`IncludeOrderVersion.Unreal5_4` | `*.Target.cs` |
| 二进制 BuildId | `33043543` | `Binaries/Win64/UnrealEditor.modules` |
| 最后一次编译 | **通过**（2026-10-27 13:50，常规 `Build.bat`；新增 4 个 UCLASS 后 UHT 重跑 842s，修正 1 处 `TSubclassOf` 类型错误后增量 10.88s） | `Binaries/Win64/UnrealEditor-ArcaneVanguard.dll` |
| 最后一次运行验证 | **通过**（2026-10-27，编辑器内省 + `-game` 实跑） | `[GAS] 属性集已注册：Health=100.0 / MaxHealth=100.0`；GE CDO = Instant / Health / Additive / SetByCaller |

> **工具链坑（已定位并解决，2026-10-26）**：曾出现"所有编译动作在 0.06 秒内全部失败且零编译器输出"。
> 根因是 `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` 里残留
> `<bAllowUBAExecutor>false</bAllowUBAExecutor>`（2026-10-25 为排查别的编译错误临时关闭，忘了恢复）。
> **删掉该元素即恢复**，详见 `KnownIssues.md` KI-018。

## 2.1 玩家角色当前实际配置（运行时读到）

| 项 | 运行时值 | 备注 |
|---|---|---|
| 网格资产 | `/Game/Wizard/Mesh/SK_wizard.SK_Wizard` | 已从模板的 `SKM_Quinn_Simple` 换成巫师网格 |
| 动画蓝图 | **`AnimBP_Wizard_ThirdPerson`** | ✅ AV-011 已切换（KI-010 已关闭） |
| 骨架 | `SKE_Wizard`（70 骨，UE4 Mannequin 命名，1 socket：`StickSocket`） | 详见 `TechnicalDesign.md` §5.6 |
| `cloth_blend_weight` | 1.0 | 布料全程参与 |
| SpringArm 长度 | 400.0 | 与 C++ 默认一致 |
| `max_walk_speed` / `jump_z_velocity` | 500 / 700 | 与 C++ 默认一致 |
| 相机组件 | `FollowCamera` | 正常 |
| **武器** | **已接入**：`BP_AV_Weapon_Staff` 挂在 `StickSocket` | 2026-10-26 重新接入（AV-034），带战斗握持偏移（AV-035） |

## 2. 当前可运行状态

- 编辑器在线（MCP 桥接 37780），引擎 5.4.4，当前关卡 `ThirdPersonMap`。
- 默认 GameMode：`/Script/ArcaneVanguard.ArcaneVanguardGameMode`
- 默认 Pawn：`/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`（父类 `AArcaneVanguardCharacter`）
- 角色可移动 / 镜头正常（AV-027 人工验收通过）
- **战斗层可用**（2026-10-26）：`F` 切战斗姿态、鼠标左键三段普攻连击（`AM_NormalAttack`）
- ~~调试按键 1~5~~ / ~~武器收杖与掉杖~~：**仍随 C++ 回滚移除**（2026-10-24），未重建

## 3. 已完成任务及证据

| 任务 | 状态 | 证据 |
|---|---|---|
| MCP 桥接接入（Python 桥 + stdio server） | Verified | `.workbuddy-ai/mcp/ue_bridge.py`；端口 37780 LISTENING |
| 巫师布料抽搐诊断与修复 | Verified | `Cloth_Jitter_Diagnosis.md`（关闭自碰撞） |
| 策划案归档为玩法基线 | Done | `GameDesign_V1.0.md` |
| 文档骨架建立 + Notion 文档库 | Done | `Docs/` 22 份文档，全部同步至 Notion `📚 工程文档库` |
| 工程现状扫描（AV-002） | Verified | `TechnicalDesign.md` §2 |
| 模板残留清理（ProjectName） | Done | `Config/DefaultGame.ini` |
| 版本控制策略决策 | Done | ADR-002（暂缓，.bak 补偿） |
| Wizard 资产定位决策 | Done | ADR-003（玩家模型） |
| C++ 全量编译验证 | **Verified** | clean build 11/11，0 错误 |
| 运行时冒烟验证 | **Verified** | Pawn 生成、58 Actor、世界 tick |
| 玩家 AnimBP 切换（AV-011） | **Done** | `AnimBP_Wizard_ThirdPerson` |
| 玩家武器挂载（AV-013） | **已回滚** | 当时 Verified（96.8907 无漂移）；C++ 回滚后功能不存在 |
| 握持姿势验证（AV-016） | **Verified** | 结论：动画无握持姿势，`GripTest_20261021/` |
| **P1 竖直约束（AV-018）** | **已回滚** | 当时 Verified（三姿势 pitch=0/roll=0）；C++ 回滚后功能不存在 |
| **武器自动显隐（AV-019）** | **已回滚** | 当时 Verified；C++ 回滚后功能不存在 |
| **资产溯源（AV-020）** | **Done** | 9/19 含 `mixamo.com`，19/19 含 `UECANDY.COM` |
| **调试按键（AV-021）** | **已回滚** | 当时 Verified（1~5 键）；C++ 回滚后功能不存在 |
| **死亡掉杖（AV-022）** | **已回滚** | 当时 Verified；C++ 回滚后功能不存在 |
| 交接文档（AV-024） | **Done** | `HANDOFF_20261022.md` |
| 美术资产全景清单（AV-025） | **Done** | `ArtAssetPlan.md` |
| Notion 文档库（AV-026） | **Done** | 22 条，已去重 |
| 角色框架与输入（AV-029 ~ AV-033） | **✅ Verified**（2026-10-26） | ADR-009 / ADR-010 转 Accepted；`Architecture` §4.4 已补 PlayerController 生命周期 |
| **武器 Actor 接入（AV-034）** | **Verified** | 武器世界坐标与 `StickSocket` 偏差 `(0,0,0)` |
| **战斗握持偏移（AV-035）** | **✅ Verified**（2026-10-26） | 待机 62%/73.8cm → 战斗 49%/**98.8cm**，插值无漂移 |
| **普攻三段连击（AV-036）** | **✅ Verified**（2026-10-26） | 1→2→3 正常、最后一段忽略、失步不回跳；方案 B（段尾过渡） |
| **第三段动画重做（AV-037）** | **✅ Done**（2026-10-26） | 时间轴未变（2.1667s / 5 个 Notify 位置未动） |
| 普通攻击连击动画（AV-023） | **✅ Done**（2026-10-26） | 负责人自制三段蒙太奇，**未采购** |
| **GAS 模块启用（AV-038）** | **✅ Verified**（2026-10-26） | `.uproject` + `Build.cs`；编译通过、编辑器能开 |
| **ASC 挂 Character（AV-039）** | **✅ Verified**（2026-10-26） | `-game` 日志：`[GAS] ActorInfo 已初始化`，Owner = Avatar = 角色 |
| **AttributeSet（AV-040）** | **✅ Verified**（2026-10-26） | 日志：`[GAS] 属性集已注册：Health=100.0 / MaxHealth=100.0` |
| **普攻接入伤害（AV-041）** | **Implemented**（2026-10-27） | GE 配置静态验证通过（Instant / Health / Additive / SetByCaller `Data.Damage`）；待 PIE 行为验证 |
| **敌人基类（AV-042）** | **Implemented**（2026-10-27） | 代码完成，BP（Mesh / PhysicsAsset / 胶囊体 / AnimBP）由负责人建 |

## 4. 正在进行的任务

- **M0 ✅ Verified**（AV-027 负责人人工验收，2026-10-24）。
- **M1 ✅ Verified**（2026-10-26）：AV-029 ~ AV-033 全部闭合，ADR-009 / ADR-010 转 **Accepted**。
  M1 实际交付超出原计划：除角色框架与输入外，还完成了**武器接入 + 战斗握持偏移 + 普攻三段连击**（AV-034 ~ AV-037）。
- **M2 ✅ GAS 骨架**（2026-10-26）：ASC 挂到 Character、`Health/MaxHealth` 属性集注册、运行时日志实证
  `Health=100.0 / MaxHealth=100.0`（AV-038 ~ AV-040）。ADR-004 转 Accepted。
- **M3 进行中**（2026-10-27）：伤害链路代码全部完成 —— 原生 `UGE_ArcaneDamage`（Instant + SetByCaller）、
  `ApplyDamage` 唯一入口（ADR-005 转 Accepted）、`PerformAttackHitCheck` 球扫、`AArcaneVanguardEnemy` 基类（ragdoll 死亡）。
  **已静态验证 GE 配置与接口暴露；待 PIE 行为验证**（需要负责人的敌人 BP）。
- **M3 ✅ 闭环合拢**（2026-10-30）：PIE 实测跑通完整一局 —— 玩家普攻打敌人 → 敌人掉血 → 受击硬直 →
  敌人反击 → 玩家掉血 → 玩家 ragdoll 死亡 + 法杖脱手 → 3.8s 重生 → 循环无异常。
  同日补齐：自定义碰撞通道（`Weapon` / `Enemy`）、敌人受击硬直（Blackboard + BT 分支）、
  武器显隐、普攻接入增强输入。详见 `Changelog.md` 2026-10-30 条目。
- **M4 进行中**（2026-10-30）：技能框架 —— `UArcaneVanguardGameplayAbility` 基类补完
  （提交消耗 / 隐藏武器 / 施加自身 GE / 播蒙太奇 / 收尾恢复武器），
  新增 `AttackSpeedMultiplier` 属性、`UGE_Q_Regen`（每秒回血）/ `UGE_Q_Haste`（攻速 +30%）两个 Buff GE。
  **待负责人建 `GA_Q_Buff` 蓝图并加进 Default Abilities**（AV-045）。
  **冷却与 Mana 消耗（AV-046）未做** —— `Mana` 属性存在，但没有 Cost / Cooldown GE。

## 5. 阻塞项

| ID | 阻塞内容 | 影响 | 解除条件 |
|---|---|---|---|
| B1 | ~~工程无 Git 仓库~~ → **已由负责人豁免**（ADR-002） | 改动不可回滚，已用 `.bak` 备份 + 禁用 L3 操作作为补偿 | 负责人决定恢复版本控制时 |
| B2 | `Content/Wizard/` 来源与许可未登记（**已知来自 UECandy 转载站**） | 不能作为对外交付内容 | 负责人确认许可（AV-012，M7 前必须闭合） |
| B3 | QuestSystem 插件仅存在于 `D:/UEProject/QuestSystemTest/Plugins/QuestSystem` | M6 任务系统无法开工 | AV-004 审计通过后再决定接入方式 |
| ~~B4~~ | ~~GAS 模块与插件均未启用~~ | **已解除**（2026-10-26） | `GameplayAbilities` 插件已启用（该插件 `EnabledByDefault=false`，必须显式启用），ASC + AttributeSet 运行时验证通过 |
| ~~B5~~ | ~~无普通攻击连击动画~~ | **已解除**（2026-10-26） | 负责人自行搭好三段蒙太奇 `AM_NormalAttack`，**未采购**，AV-023 转 Done |
| ~~B6~~ | ~~敌人资产为 0~~ | **已解除**（2026-10-27） | 负责人导入 `Content/Stone_Golem/`（UE4 Mannequin 命名，与 `SKE_Wizard` 同体系）+ `Content/SpearAnimationsV2/`（402 资产，含 1H/2H 攻击 28 个、受击 8 个）。**无死亡动画 → 用 ragdoll** |
| ~~B7~~ | ~~**构建环境故障**（KI-018）：UBT 派生子进程秒退~~ | **已解除**（2026-10-26 20:48） | 根因是 UBT 的 `BuildConfiguration.xml` 里残留 `<bAllowUBAExecutor>false</bAllowUBAExecutor>`，删除后 7/7 通过 |

## 6. 已知缺陷

见 `KnownIssues.md`。当前**无阻断主流程的缺陷**（因为主流程尚未实现）。
KI-010 已关闭；**KI-013（握持姿势）2026-10-26 重新生效**（武器已重新接入），
建议只做手指弯曲、不动挂点；KI-016 / KI-017 为连击相关踩坑，均已规避。

## 7. 当前 Git 提交

**无仓库**（`git status` → `fatal: not a git repository`）。已由负责人决定暂缓（ADR-002）。
补偿措施：改动前 `.bak-<时间戳>` 备份，禁止 L3 操作。

## 8. M0 验收对照

| M0 验收条件 | 状态 | 证据 / 说明 |
|---|---|---|
| 干净启动工程成功 | **通过** | 运行时实跑：关卡加载 0.54s，`Game class is 'ArcaneVanguardGameMode'` |
| C++ 编译成功 | **通过** | clean build 11/11，0 错误 0 警告 |
| PIE 中角色移动与镜头正常 | **通过** | 自动化已确认 Pawn/相机/参数/tick；**2026-10-24 负责人人工验收：移动与镜头正常**（AV-027） |
| 基线提交可用于回滚 | **经负责人豁免**（ADR-002） | 不得标记为通过 |
| 所有实际环境信息已记录 | **通过** | 本文件 §1 |

## 9. 下一步：5 天战斗闭环冲刺（2026-10-27 ~ 10-31）

详见 `TaskBoard.md` 的「🚀 冲刺：5 天做出基本战斗闭环」章节（AV-038 ~ AV-049）。

**目标**：10-31 结束时能完整玩一局 —— 普攻/技能打敌人 → 敌人死亡 → 敌人 AI 反击 → 玩家受伤 → 可重开。

| 日期 | 主线 | 当天验收 |
|---|---|---|
| 10-27 | 修环境 + GAS 骨架（AV-038~040） | ✅ **已完成**：KI-018 修好、ASC 可取、`Health=100` |
| 10-28 | 普攻接伤害 + 敌人靶子（AV-041/042） | ✅ **已完成**：打到敌人 → 掉血 → ragdoll 销毁 |
| 10-29 | 敌人 AI + 反击（AV-043/044） | ✅ **已完成**：敌人会追会打，玩家掉血 |
| 10-30 | 技能 + 冷却资源（AV-045/046） | ⚠️ **部分完成**：GA 基类 + Q 技能数据层（GE / 属性）✅，**待建 `GA_Q_Buff` 蓝图**；CD/Mana 未做 |
| 10-31 | 死亡重生 + UI + 场景（AV-047~049） | ⚠️ **死亡重生已提前完成（10-30）**；剩 **UI 血条** 与 **收尾录屏** |

**10-30 额外完成（原计划外）**：
- 玩家 ragdoll 死亡 + 法杖物理脱手 + 重生重新装配（AV-047 增强）
- 自定义碰撞通道 `Weapon` / `Enemy`（负责人主导设计），解决"武器撞飞 ragdoll 尸体"
- 敌人受击硬直（AV-050）：Blackboard 状态 + BT Selector 受击分支
- 武器显隐（AV-051）、普攻接入增强输入（AV-052）

**三个前置约束（状态已更新）**：
- ~~敌人资产为 0~~ → **已解决**：`Content/Stone_Golem/`（UE4 Mannequin 命名）+ `Content/SpearAnimationsV2/`
  （402 资产）。**石头人无死亡动画 → 用 ragdoll**；攻击动画用 `AB_1H_*`（负责人实测 2 个可用）
- **5 天做不出完整 3 技能** → 1 个完整 + 2 个复用变体，必要时砍到 1 个
- ~~构建环境不稳定~~ → **已解决**（KI-018 根因是 UBA 配置残留）

**绝对不能砍**：GAS 骨架、普攻伤害链路、敌人能死、敌人能打人。缺一块就没有闭环。

并行任务（不阻塞主线）：
- **AV-017 / KI-013 手指弯曲**：负责人加 15 个 `Transform (Modify) Bone` 节点。
  **只做手指，不要切 `weapon_r` 挂点**（会推翻 AV-035 的校准）。冲刺期间可暂停。
- **AV-012** `Content/Wizard` 许可闭合（M7 前必须完成）。

## 10. 已闭环的历史风险

| 原风险 | 现状 |
|---|---|
| KI-010 玩家用错 AnimBP | **已解决**（AV-011） |
| KI-014 法杖施法颠倒 | 当时已解决（AV-018）→ **代码已回滚** |
| 武器穿身体（Magic_Attack 双手前推） | 当时已解决（AV-019）→ **代码已回滚** |
| 死亡时法杖还握在手里 | 当时已解决（AV-022）→ **代码已回滚** |
| 骨骼与动画包不兼容 | **已澄清**：`SKE_Wizard` 是 Manny 命名子集，兼容包等价可用（`SkeletonDecision_20261023/`） |
