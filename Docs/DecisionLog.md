# DecisionLog — 技术决策记录

格式见 `GameDesign_V1.0.md` §14。状态：Proposed / Accepted / Rejected / Superseded。

---

## ADR-001：引擎版本与工具链

- 日期：2026-10-21
- 状态：**Accepted**（已由实际工程确认）
- 背景：项目需要一个稳定、可复现的开发环境，且素材/插件生态需匹配。
- 决策：**UE 5.4.4-35576357**，Windows 64 位编辑器，C++ + Blueprint 混合。
- 选择理由：工程已在此版本创建并产出二进制；MCP 桥接已在此版本验证可用。
- 影响：所有 API 以 5.4 头文件为准；不得直接粘贴其他版本代码。
- 风险：5.4 无官方 `ModelContextProtocol` 插件，编辑器自动化只能走自建 Python 桥（端口 37780）。
- 验证方式：`get_editor_status` 返回 5.4.4；`GameplayAbilities.uplugin` 存在于 `Engine/Plugins/Runtime/GameplayAbilities`。
- 批准人：项目负责人

---

## ADR-002：版本控制策略（**已决策：暂缓**）

- 日期：2026-10-21
- 状态：**Accepted（暂缓建立版本控制）**
- 背景：工程当前**无 Git 仓库**。`Content` 2.5 GB，其中 `Content/Wizard` 2.1 GB；`Intermediate` 2.2 GB（应排除）。
- 决策：**负责人决定暂时跳过版本控制**，原因是正在修复 `Content/Wizard` 中的问题，此时建立基线会把不稳定的资产固化进历史。
- 选择理由：避免在素材状态不稳定时建立无意义的基线；Wizard 修复完成后再一次性建立干净基线更合理。
- 影响：
  - **改动不可回滚**，风险由负责人明确接受。
  - 补偿措施（已写入 `AgentRules.md` §5）：改动前强制 `.bak-<时间戳>` 文件级备份；禁止 L3 操作；资产改动优先"仅内存验证"。
  - 里程碑 M0 的"基线提交可用于回滚"一项**判定为"经负责人豁免，未执行"**，不得标记为通过。
- 风险：误操作无法撤销；多人/多机协作不可行（当前为个人项目，可接受）。
- 恢复条件：Wizard 修复完成 → 按方案 A（Git + LFS）或 C（排除 Wizard）建立基线。
- 批准人：项目负责人（2026-10-21）

---

## ADR-003：`Content/Wizard/` 的定位（**已决策：作为玩家模型来源**）

- 日期：2026-10-21（首次决策）／2026-10-21 21:20 更新
- 状态：**Accepted（用途已确认：玩家模型；许可仍待确认）**
- 背景：2.1 GB 巫师资产（4 个皮肤、材质、动画、5 把法杖/书）。负责人已明确：**这套巫师模型就是本项目的玩家模型**。
- 决策：
  1. **用途确认**：`SK_wizard`（及 3 个皮肤变体）作为**玩家角色网格**使用。
  2. **资产位置不变**：仍保留在 `Content/Wizard/`，不做迁移（避免破坏已有引用）。
  3. **许可状态不变**：仍为"待确认"，未确认前不得进入对外发布物。
- 选择理由：负责人指定；且该资产已具备完整第三人称动画集（见 `TechnicalDesign.md` §5）。
- 影响（重要，已同步到技术设计）：
  - 玩家角色网格将从 `SKM_Quinn_Simple` 换成 `SK_wizard` 系，AnimBP 从 `ABP_Quinn_C` 换成 `AnimBP_Wizard_ThirdPerson`。
  - **骨骼不兼容**：`SKE_Wizard`（70 骨骼）≠ UE5 Mannequin 骨架 → **模板自带的 `MM_*` 动画不能用于玩家**，只能用 `Content/Wizard/Animations` 下的动画。
  - 布料抽搐（KI-007）从"试验内容问题"升级为**玩家可见的 P1 观感问题**。
  - `Content/Wizard` 的 2.1 GB 成为**交付必需**，仓库策略（ADR-002）恢复时必须走 LFS。
- 风险：许可未确认 → 若最终不可商用，需替换玩家模型，M1/M7 的美术工作会返工。
- 必须给出结论的时间点：**M7（表现与打磨）开工前**，最好是 M1 就确认。
- 批准人：项目负责人（2026-10-21）

---

## ADR-004：ASC 归属与初始化策略（待实现时确认）

- 状态：Proposed
- 计划：单机 Demo，玩家与敌人的 ASC 均挂在 **Character** 上；`InitAbilityActorInfo` 在 `PossessedBy` 与 `OnRep_PlayerState` 各调用一次，用 `bASCInitialized` 防重。
- 理由：V1.0 不做联机，无需 PlayerState 持有 ASC 的复杂度；敌人本来就没有 PlayerState。
- 风险：若未来加联机需迁移到 PlayerState（属重大变更）。

---

## ADR-005：伤害与死亡唯一入口（待实现时确认）

- 状态：Proposed
- 计划：所有伤害走 `GE_AV_Damage` → `UAS_AV_Combat::PostGameplayEffectExecute` 统一钳制与判死 → 调 `HandleDeath()`；`HandleDeath()` 由 `bDeathHandled` 守卫，只执行一次。
- 理由：满足验收"同一次死亡不重复结算"；避免每个技能各写一套。
- 风险：Cue/Notify 若绕过该入口会造成双重结算 → 由 ADR-008 约束。

---

## ADR-006：冷却与资源消耗（待实现时确认）

- 状态：Proposed
- 计划：Cost 与 Cooldown 均用 `UGameplayEffect`（Instant 扣法力 / Duration 加 Cooldown Tag），不用手写 Timer。
- 理由：GAS 原生语义，可被 `CanActivateAbility` 统一检查，蓝图可见。
- 风险：需要正确的 `CooldownGameplayEffectClass` 与 Tag 配置，否则"冷却中仍可施放"。

---

## ADR-007：任务插件接口与防重语义（待 AV-004 后确认）

- 状态：Proposed
- 计划：`TaskIntegration` 适配层唯一依赖 QuestSystem；战斗侧只上报事实（`OnEnemyDefeated(EnemyId, SourceActor)`）；增量上报与绝对同步分开接口；死亡入口保证一次。
- 理由：策划案 §3.7「主动上报模型」。
- 风险：插件 v0.1.0 为 Beta，接口可能变动。

---

## ADR-008：动画 Notify 与 Gameplay Ability 的职责边界（待实现时确认）

- 状态：Proposed
- 计划：Notify 只负责表现时序与"经设计的动作节点"（如放开弹体）；**伤害只在 Ability 内结算一次**，Notify 不得直接 ApplyDamage。
- 理由：策划案 §3.3「不应仅凭播放动画就认定技能成功命中」。

---

## ADR-009：角色框架分层（M1）

- 日期：2026-10-24
- 状态：**Proposed**（待负责人确认后转 Accepted）

### 背景

M1 目标是在不破坏模板行为的前提下建立项目自己的角色扩展点。当前 C++ 只有
`AArcaneVanguardCharacter`（模板）与 `AArcaneVanguardGameMode`，**没有自定义 PlayerController**。
模板把输入绑定（`AddMappingContext` + Action 绑定）全放在 Character 的 `SetupPlayerInputComponent` 里。

### 决策

1. **新建 `AArcaneVanguardPlayerController`**（继承 `APlayerController`），承担：
   输入上下文注册、后续 UI 输入模式切换、技能输入转发。
2. **`AArcaneVanguardCharacter` 保持继承 `ACharacter`，不做组件化重构**。
3. **暂不建敌人基类**。
4. `PlayerControllerClass` 由 C++ 默认指定（`AArcaneVanguardPlayerController::StaticClass()`），
   不强制走蓝图；需要蓝图扩展时再建 BP 子类。

### 理由

- 输入上下文的生命周期应与 **LocalPlayer** 一致，而不是与 Pawn 一致。Pawn 会被销毁/替换
  （死亡、重生、换角色），Controller 不会。放在 Character 上必然要处理"Pawn 换了怎么办"。
- UI 输入模式（Game / UI / GameAndUI）是 Controller 的职责（`SetInputMode`），M6 必然要用。
- 策划案 §4.2 既反对"所有玩法写进一个巨大 Character 类"，也反对"为架构完整而过度泛化"。
  M1 建 Controller 是**拆职责**，不是加抽象层。
- 敌人基类在 M1 没有任何消费者（M1 验收不含敌人），提前建就是空类。

### 备选方案与为何不选

| 方案 | 不选的原因 |
|---|---|
| 不建 Controller，输入继续放 Character | 输入生命周期绑在 Pawn 上，换 Pawn 就出问题；UI 输入模式无处安放 |
| 组件化重构（Input / Camera / Movement 全拆成组件） | M1 目标是"扩展点清晰"不是"重构"；拆了会碰模板行为，违背"M1 不破坏模板" |
| 现在一起建敌人基类 | 无消费者，空类；敌人在 M5 才定设计 |

### 影响

- `GameMode` 需指定 `PlayerControllerClass`。
- 输入绑定从 Character 迁走（见 ADR-010）。
- `DefaultMappingContext` 等输入资产引用需从 Character 搬到 Controller。

### 风险

- 新建 Controller 后若 GameMode 没指定，PIE 会退回默认 `APlayerController`，
  表现是"输入全失效"。**验证方式**：PIE 中读 `GetController()->GetClass()->GetName()`，必须是新类。
- 若未来做联机，Controller 分层方式需重新评估（V1.0 不做联机）。

### 验证方式

1. 编译通过
2. PIE 中 `GetController()` 返回 `AArcaneVanguardPlayerController`
3. 移动 / 镜头 / 跳跃仍正常

---

## ADR-010：输入绑定生命周期（M1）

- 日期：2026-10-24
- 状态：**Proposed**（待负责人确认）

### 背景

模板在 `AArcaneVanguardCharacter::SetupPlayerInputComponent` 里调用
`AddMappingContext(DefaultMappingContext, 0)`。

`SetupPlayerInputComponent` 在 Pawn 被 Possess 时调用；同一 Pawn 反复 Possess/Unpossess
（或 PIE 反复进出但复用 Pawn）时可能**重复 Add**。

策划案 §4.4 要求"输入绑定初始化应只有明确的入口，避免重复绑定"；M1 验收明确含"输入无重复绑定"。

### 决策

1. **`AddMappingContext` 迁到 `AArcaneVanguardPlayerController::BeginPlay`**，
   `EndPlay` 中成对 `RemoveMappingContext`。
2. Action 绑定（Jump / Move / Look）**仍留在 Character 的 `SetupPlayerInputComponent`**。
   理由：Action 绑定由 `UInputComponent` 持有，Pawn 销毁时组件随之销毁，本身不会累积；
   需要成对管理的只有 Subsystem 级别的 MappingContext。
3. 加防御性检查：`BeginPlay` 时若拿不到 `ULocalPlayer`，打 Warning 并跳过，不崩。

### 理由

- MappingContext 注册在 `UEnhancedInputLocalPlayerSubsystem` 上，**Subsystem 生命周期是 LocalPlayer**，
  比 Pawn 长。在 Pawn 上 Add 而 Pawn 上不 Remove，就是泄漏。
- Controller 的 BeginPlay/EndPlay 与 LocalPlayer 同生命周期，天然成对。
- Action 绑定不需要搬：`UInputComponent` 随 Pawn 走，重复 Possess 时会重新创建。

### 备选方案与为何不选

| 方案 | 不选的原因 |
|---|---|
| 保持在 Character，只在 EndPlay 补 Remove | Character 的 EndPlay 在 Pawn 销毁时才触发，中间若换 Pawn 仍会重复；且无法处理"Controller 已存在但 Pawn 未生成"的时序 |
| 用 `bMappingContextAdded` 布尔防重 | 治标：能防重复，但 Remove 时机仍不明确；状态在 Pawn 上而资源在 Subsystem 上，语义错位 |
| 在 `SetupPlayerInputComponent` 里先 Remove 再 Add | 掩盖问题；Remove 不存在的 context 是空操作，看似安全实则掩盖了生命周期错配 |

### 影响

- `DefaultMappingContext` 属性从 Character 移到 Controller。
- Character 不再需要 `#include "EnhancedInputSubsystems.h"`。

### 风险

- `PlayerController::BeginPlay` 时 `GetLocalPlayer()` 理论上已就绪（本地玩家），
  但若时序异常会拿到 null → 已加 Warning 兜底。
- 若未来有多个 LocalPlayer（分屏），需按 LocalPlayer 分别管理。

### 验证方式（**量化，不接受肉眼**）

1. **改之前先测基线**：连续进出 PIE 5 次，读 Subsystem 已注册的 MappingContext 数量
2. 改动后重复同样测试，数量必须恒为 `1,1,1,1,1`
3. 移动 / 镜头 / 跳跃仍正常

## ADR-011：自定义碰撞通道体系（M3，2026-10-30）

### 背景

武器脱手后会把 ragdoll 尸体撞飞。根因：武器 `ObjectType = PhysicsBody`，且对全通道 Block
（只忽略了 Pawn / Visibility / Camera），而 ragdoll 尸体的 `ObjectType` 也是 `PhysicsBody` ——
两个 PhysicsBody 互相 Block，武器虽然是静态碰撞体（`SimulatePhysics = false`），
但玩家挥武器时它是一个**移动中的静态障碍物**，扫过尸体就把它弹开。

### 决策

在 `Config/DefaultEngine.ini` 新增两条 **Object Channel**，不跟 UE 默认通道混用：

```ini
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Overlap,bTraceType=False,bStaticObject=False,Name="Weapon")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Block,bTraceType=False,bStaticObject=False,Name="Enemy")
```

### 理由

1. **语义清晰**：`Weapon` / `Enemy` 一眼看懂谁是谁，比"ObjectType = PhysicsBody 但 Ignore 一堆通道"好读
2. **默认安全**：新通道默认 Ignore，配置时是"显式开放"而不是"显式禁止"—— 漏配的后果是"不碰撞"而非"乱碰撞"
3. **可扩展**：以后加"投射物""可破坏物"直接加通道，不用在每条已有响应里补规则

### 备选方案与为何不选

| 方案 | 不选的原因 |
|---|---|
| 只给武器加 `Ignore PhysicsBody` | 治标：以后加任何新的物理体类型（载具、掉落物）都会再撞一次 |
| 握持时关碰撞、脱手时开 | 运行时改 `SetCollisionEnabled` 不可靠 —— 物理体在 Spawn 时已按旧配置建好，之后改字段不重建（实测 5 种改法全部失败） |
| 保持默认通道，用 `SetCollisionResponseToAllChannels(Ignore)` + 逐个 Block | 能用，但语义全靠注释；且每加一条新通道都要回来检查，容易漏 |

### 影响

- `DefaultEngine.ini` 两条通道
- 武器 `ApplyWeaponCollisionSetup`：ObjectType = `Weapon`，全 Ignore 只 Block `WorldStatic`
- 敌人 Capsule：ObjectType = `Enemy`，且对 `Enemy` 通道 Block（让球扫能查到自己）
- 玩家攻击球扫：`SweepMultiByChannel(Pawn)` → `SweepMultiByObjectType(Enemy)`

### 风险

- **`Enemy` 通道默认 Block** 意味着 `SweepMultiByChannel(Enemy)` 会命中所有东西。
  必须用 `SweepMultiByObjectType` 按物体类型过滤，不能图省事用 Channel 查询。
- 敌人死亡切 `Ragdoll` 预设会把 ObjectType 重置成 `PhysicsBody`，尸体自动脱离 `Enemy` 通道。
  **当前这是想要的行为**（尸体不该被球扫命中），但若以后要做"鞭尸"需要额外处理。

### 验证方式

在编辑器里 spawn 实例，读 `get_collision_object_type()`：

| 对象 | 期望值 |
|---|---|
| 武器 Mesh | `ECC_WEAPON` (14) |
| 敌人 Capsule | `ECC_ENEMY` (15) |
| 玩家 Capsule | `ECC_PAWN` (2) |

PIE 实测：打敌人正常掉血、尸体不再被武器撞飞。

## ADR-012：GameplayAbility 的分层（M4，2026-10-30）

### 背景

"GA 该用 C++ 还是蓝图？" 这是实现 Q 技能时绕不开的问题。

### 决策

**C++ 基类管流程，蓝图子类只填数据。**

- **C++**：`UArcaneVanguardGameplayAbility` 负责 `CommitAbility` / 隐藏武器 / 施加自身 GE /
  播蒙太奇 / 收尾恢复武器 —— 所有技能共用
- **蓝图**：`GA_Q_Buff` 这类子类只填 `AbilityMontage` / `SelfEffectClasses` / CD / Cost，
  **一个节点都不用连**

### 理由

1. **策划要能改数值**：伤害、CD、持续时间写在 C++ 里，改一次要编译一次，规模化团队不可接受
2. **资产引用**：Montage、Niagara 特效、音效都是 `UObject`，蓝图拖拽比 C++ 写路径（还要处理 `ConstructorHelpers` 和重定向）方便得多
3. **逻辑复用**：加第三个技能时只建蓝图，不复制代码
4. **业界惯例**：Epic 的 Lyra 就是 `ULyraGameplayAbility`(C++) → `GA_WeaponFire`(BP)

### 备选方案与为何不选

| 方案 | 不选的原因 |
|---|---|
| 全 C++ | 策划改数值要编译；资产引用要靠路径字符串，重定向就断；失去"数据驱动"的意义 |
| 全蓝图 | 技能逻辑靠连线，加第三个技能时节点爆炸；面试官会质疑"逻辑复用怎么做" |

### 影响

- `UArcaneVanguardGameplayAbility` 从"只有 `GetAvatarCharacter`"补完为完整基类
- 新增 `AttackSpeedMultiplier` 属性（供攻速 Buff 修改）
- 新增 `UGE_Q_Regen` / `UGE_Q_Haste` 两个原生 GE 作为默认实现

### 风险

- 基类改动影响**所有**蓝图子类 —— 改 `ActivateAbility` / `EndAbility` 需要回归测试全部技能
- 基类暴露的参数越多，蓝图子类的配置越容易配错（`SelfEffectClasses` 填错 GE 不会报错，只会静默无效）

### 验证方式

Q 技能走完整链路：按 Q → `CommitAbility` 通过 → 藏法杖 → 施加两个 GE → 播蒙太奇 →
蒙太奇结束 `EndAbility` → 恢复法杖。观察血量每秒 +10、普攻动画变快。
