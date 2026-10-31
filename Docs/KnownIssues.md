# KnownIssues — 已知问题

- 最后更新：2026-10-27
- 分级：**P0 阻断** / P1 严重 / P2 一般 / P3 轻微
- ⚠️ **2026-10-24：C++ 已全部回滚到第三人称模板**。依赖已删除代码的修复项（KI-013 / KI-014 / KI-015 中的收杖规则）**当时不再生效**。
- ⚠️ **2026-10-26：武器子系统已重新接入**（AV-034 / AV-035）。KI-013（握持姿势）**重新生效**；
  KI-014（法杖颠倒）与 KI-015 中的"自动收杖"**仍未恢复**（对应代码未重建）。

| ID | 级别 | 问题 | 证据 | 影响 | 处理 |
|---|---|---|---|---|---|
| KI-001 | **P0** | 工程无 Git 仓库 | `git status` → `fatal: not a git repository` | 任何改动不可回滚；AgentRules §5 已冻结 L2+ 改动 | 待 ADR-002 决策后建立基线 |
| KI-002 | P1 | `Content/Wizard/` 2.1 GB，来源与许可未知 | `Docs/AssetRegister.md` | 不能进入对外交付；占工程 84% 体积 | 待 ADR-003 决策 |
| KI-003 | P1 | GAS 完全未接入 | `Build.cs` 无 GameplayAbilities；`.uproject` 未启用插件 | M2 起全部阻塞 | AV-005 计划后按 L2 启用 |
| KI-004 | P1 | QuestSystem 插件不在本工程 | `Plugins/` 不存在；插件在 `D:/UEProject/QuestSystemTest/` | M6 阻塞 | 先做 AV-004 审计 |
| KI-005 | P2 | 47 条 `ClothingSimulationFactoryNv` 警告 | `Saved/Logs/*.log` | 日志噪声；资产每次注册被内存改写为 Chaos | 见 `Cloth_Jitter_Diagnosis.md` C2；不影响玩法 |
| KI-006 | ~~P2~~ **已解决** | `DefaultGame.ini` 的 `ProjectName` 曾是 `Third Person Game Template` | `Config/DefaultGame.ini` | — | **已修复**（AV-008，已改为 `Arcane Vanguard`） |
| KI-007 | P2 | 巫师布料在 `armor3_01` 区域抽搐（`p.ClothPhysics 0` 后消失） | `Cloth_Jitter_Diagnosis.md` | 若使用巫师网格则影响观感 | 诊断完成，修复待授权 |
| KI-008 | P3 | 当前磁盘只有 1 张关卡，历史提到的 Wizard 展示关卡已不存在 | `find Content -iname "*.umap"` | 与旧记录不一致，需以磁盘为准 | 已在 `TechnicalDesign.md` §2.3 更正 |
| KI-009 | P3 | 工程由模板改名而来，残留 `ActiveGameNameRedirects` / `ActiveClassRedirects` | `Config/DefaultEngine.ini` | 无害，但需保留以免旧资产引用断裂 | 不动 |
| KI-010 | ~~P1~~ **已解决** | **玩家网格是 `SK_wizard`，但动画蓝图仍是 `ABP_Quinn_C`（Mannequin 的 AnimBP）**；项目自带的 `AnimBP_Wizard_ThirdPerson` 完全未被使用 | 运行时读组件：`mesh_asset = /Game/Wizard/Mesh/SK_wizard.SK_Wizard`，`anim_class = ABP_Quinn_C` | — | **已修复**（AV-011，负责人已在 BP 中切换到 `AnimBP_Wizard_ThirdPerson`）。遗留观感需实机确认 |
| KI-011 | P2 | `-nullrhi` 无法用于本工程运行时冒烟 | `GameSmoke_20261021.log` 卡在静态网格就绪 | 影响自动化验证方法选择 | 已改用带渲染运行，写入 `TestPlan.md` §3.1 |
| KI-012 | P2 | 运行时偶发 `DXGI_ERROR_DEVICE_HUNG`（GPU 5s 超时 / TDR） | `Saved/Logs/WeaponDiag_20261021.log`：Lumen `FilterRadianceWithGather` pass，显存 397MB/7249MB | 4 次运行触发 1 次；崩溃后需 taskkill 残留进程 | 重试即恢复；若频繁复现再考虑降 Lumen 或换 DX11 排查 |
| KI-013 | P1 **（2026-10-26 重新生效）** | **玩家握持法杖时没有握持姿势**：所有动画的手指都是伸直的，**杖身从掌心穿过** | `Docs/GripTest_20261021/02_cast_spell_01_hand_zoom.png` | 观感"没握住"；M7 前需解决 | **处理中**：采用 A4（AnimBP 手指弯曲层）。已完成：骨骼名实证（70 骨 UE4 Mannequin 命名）、弯曲轴实测标定、AnimBP 备份、分步指南 `Docs/KI013_FingerGrip_20261022/README.md`、验收脚本 `Saved/Scripts/ki013_verify_after.py`。**待负责人在编辑器操作**：按指南加 15 个 `Transform (Modify) Bone` 节点（AnimBP 图无法脚本编辑）。<br />**⚠️ 2026-10-26 方案修正**：**不要再切 `weapon_r` 挂点**。原计划那条已不适用 —— 当前握持校准（`StickSocket` 的 −124cm 偏移 + `CombatGripOffset` 90cm，见 AV-035）建立在 `StickSocket` 上，切到 `weapon_r` 会让杖尾贴到手心、握点从 62% 变 0%，等于推翻整套校准重来。**只做手指弯曲，不动挂点** |
| KI-015 | P2 | 技能动画为 Mixamo 通用动作，部分与持杖完全冲突（如 `Magic_Attack` 是双手前推） | `Content/Wizard/Animations/*` 含 `mixamo.com` 字符串 | 动作语义不对，不只是穿模 | `Magic_Attack` 当时已用 `WeaponHiddenAnimNames` 自动收杖（**该机制已随 C++ 回滚删除**）；其余动作待评估 |
| KI-014 | ~~P1~~ **已解决** | **施法时法杖上下颠倒**：待机水晶朝上（roll -3.87），`Cast_Spell_01` 水晶朝下（roll -162.54） | 实测 `hand_r` 世界旋转在两种姿势间相差 **172.44°** | — | **当时已修复**（2026-10-22 验收）：`bKeepWeaponUpright` 每帧锁竖直 + 握点贴手，三姿势实测 `pitch=0/roll=0`。证据 `Docs/P1Verify_20261022/`。**⚠️ 2026-10-24：C++ 回滚后该约束已删除，修复不再生效**（当前无武器，问题不会显现） |
| KI-016 | P2 | **蒙太奇 Section 链接导致"自动播完三段"**：只按一次攻击，三段连续播完，连击窗口形同虚设 | 负责人实测 + 日志只有 `-> 第 1 段` 与 `结束`，中间无任何输入事件 | 连击系统完全失效 | **已解决**（2026-10-26）：把 `Combat_01` / `Combat_02` 的 `Next Section` 设为 `None`（负责人手动）。**该字段在引擎里是 private，Python / 蓝图 / `Montage_SetNextSection` 都改不了**，只能人工在 Montage 编辑器改 |
| KI-017 | P2 | **连击窗口终点压在段边界上时，`AnimNotifyState::NotifyEnd` 不会被调用** | 第一版方案 B 把挂链放在 NotifyEnd 里 → 整段播完一条日志都没有 | 任何依赖 NotifyEnd 的逻辑静默失效 | **已规避**（2026-10-26）：挂链时机改到"按下时"和"窗口开启时"（`Montage_SetNextSection` 只在段尾生效，时机等价）。**摆 Notify 时窗口终点至少离段尾留 0.05s** |
| KI-018 | ~~P1~~ **已解决** | **构建环境故障：UBT 派生的任何子进程秒退（退出码 1，无任何诊断）** | `Total time in Parallel executor: 0.06 seconds`；cl / lib / link / dotnet 全部如此 | 无法用常规 `Build.bat` 编译，阻塞所有代码改动 | **✅ 已解决**（2026-10-26 20:48）：根因是 `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` 里被人为加了 `<bAllowUBAExecutor>false</bAllowUBAExecutor>` —— 2026-10-25 为排查另一个编译错误临时关闭 UBA，注释写着"排查完可删掉"，但一直没恢复。**删掉该元素恢复默认后，7/7 动作全部通过（含此前必失败的 `WriteMetadata`），增量编译 1.29s 正常**。⚠️ **重启机器无效**。**2026-10-27 复现**：配置文件干净时同样症状又出现（`0.07s` / 7 个动作全失败 / 零诊断），`-NoUBA` 也无效，但**手动 `cl.exe @rsp` 完全成功** → 说明还有第二个未定位的成因（UBT 创建子进程失败），此时仍需手动 `cl+lib+link` 绕过 |
| KI-019 | ~~P2~~ **已解决** | **敌人 ragdoll 死亡后，玩家走到尸体旁摄像机会被推近** | PIE 实测（2026-10-27）。引擎 `BaseEngine.ini` 的 `Ragdoll` 预设只有 `CustomResponses=((Pawn,Ignore),(Visibility,Ignore))`，**其余通道走"默认" = Block** | 第三人称视角突然拉近，观感差；玩家会以为撞到了什么 | **✅ 已解决**（2026-10-27）：`HandleDeath` 里在 `SetCollisionProfileName("Ragdoll")` **之后**补 `SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore)`。`USpringArmComponent` 的 `ProbeChannel` 正是 `ECC_Camera`，所以之前被挡。**顺序不能反** —— 换碰撞预设会重置所有通道响应 |

## 说明

- 当前**没有**"主流程崩溃"类缺陷，因为主流程（战斗闭环）尚未实现。
- 本表只记录**已观察到**的问题。未验证的猜测一律写入 `Cloth_Jitter_Diagnosis.md` 的候选原因，不进入本表。
