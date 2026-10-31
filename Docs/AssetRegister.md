# AssetRegister — 资产来源与许可登记

- 最后更新：2026-10-21
- 规则：**未登记的资产不得进入交付版本**。许可不明确的资产必须标"待确认"，不得假设可用。

## 登记表

| 资产 / 目录 | 类型 | 来源 | 许可证 | 商用 | 需署名 | 依赖插件 | 状态 |
|---|---|---|---|---|---|---|---|
| `Content/ThirdPerson/` | 蓝图 / 输入 / 关卡 | UE 5.4 Third Person 模板（Epic 随引擎提供） | Epic Content License（随引擎） | 允许 | 否 | 无 | 已登记 |
| `Content/Characters/Mannequins/` | 骨骼网格 / 动画 / Rigs | UE 5.4 Mannequin（Epic） | Epic Content License | 允许 | 否 | 无 | 已登记 |
| `Content/LevelPrototyping/` | 灰盒静态网格 | UE 5.4 模板 | Epic Content License | 允许 | 否 | 无 | 已登记 |
| `Content/Wizard/`（2.1 GB） | 骨骼网格 / 材质 / 动画 / 法杖 / DemoRoom 蓝图 | 未知；网格导入源路径为 `C:/Program Files/Zbrush/ZProjects/Projects/Wizard/Rig/SK_wizard.fbx`（本地制作痕迹） | **未知** | **未知** | **未知** | 无（但含 NvCloth 时代数据） | **玩家模型（用途已确认）／许可待确认** |

## `Content/Wizard/` 说明

**用途已确认**（ADR-003，2026-10-21）：这是本项目的**玩家角色模型**，不是可选素材。包含：
`SK_wizard` + 3 个皮肤变体（Dark / Fire / Ice）、`SKE_Wizard`（70 骨骼）、`PHY_Wizard`、
17 个动画（含 `AnimBP_Wizard_ThirdPerson`）、5 把法杖/书（`SM_Wizard_Staff_*`、`SM_Wizard_Book`）。

### ⚠️ 来源溯源（2026-10-22 实测，重要）

对 19 个资产做了二进制字符串扫描，结果：

| 标记 | 命中资产 | 说明 |
|---|---|---|
| `mixamo.com` | **9 个**：`Cast_Spell_01/02`、`Magic_Attack`、`Magic_Heal`、`Death_Forward`、`Run_Forward/Back/Left/Right` | 这些动作动画是 **Mixamo 动画**，导入时保留了来源元数据 |
| `UECANDY.COM` | **19 个（全部）** | 全部资产带 `.!VISIT HTTPS://WWW.UECANDY.COM!` 字符串 —— 来自 **UECandy** 这个资源分发站 |

**含义（直接影响设计决策）**：

1. Mixamo 是**通用人形动画库**，录制时手上没有武器。这解释了全部症状：
   没有握持姿势（KI-013）、手腕在待机与施法之间转 172° 导致法杖颠倒（KI-014）、
   `Magic_Attack` 是"双手向前推"而非持杖动作。
2. 第三人称移动那 7 个（`ThirdPersonIdle/Walk/Run/Jump`）**不含** `mixamo.com`，
   来源与前者不同，可能是作者自录或另一来源。
3. **UECandy 是资源转载站**，不是原始发行渠道。原始作者与授权链条目前无法从资产内确认。

### 仍待确认（许可层面）

1. 巫师模型/贴图/动画是**自己制作**、**购买**还是**免费下载**？
2. 若来自 UECandy，**原始授权**是什么？UECandy 通常只做转载，不提供授权凭证。
3. 9 个 Mixamo 动画：Mixamo 本身允许商用，但**转载链条**是否合规需确认。
4. 由于它是玩家模型，**必须**在作品集公开前确认授权；否则需替换模型（M1/M7 美术工作会返工）。

## 引擎与工具

| 项 | 版本 / 许可 |
|---|---|
| Unreal Engine | 5.4.4-35576357（Epic EULA） |
| Blender（素材处理，如使用） | 5.2.2 LTS，GPL（仅用于制作，不随工程分发） |
| QuestSystem 插件 | 自研，位于 `D:/UEProject/QuestSystemTest/Plugins/QuestSystem`，v0.1.0 |

---

## 已采购资产登记（2026-10-23）

### ① Dark Knight - Male and Female（敌人）

| 项 | 值 |
|---|---|
| Fab 链接 | https://fab.com/s/b03de798c900 （listing `d237534b-9ba7-4c48-8cd9-8a1fd1965119`） |
| 用途 | **敌人：Arcane Guard 奥术守卫**（M5） |
| 采购状态 | ✅ **负责人已购买**（2026-10-23） |
| 价格 | 待填（见 Fab 订单页） |
| 许可 | 待填 —— 需在 Fab 订单页/商品页确认具体条款（Fab 付费包通常允许用于自研项目，禁止再分发） |
| 引擎版本 | UE 4.24–4.27 / 5.0–5.8 ✅（本工程 5.4） |
| 骨架兼容 | **Mannequin Skeleton for UE4** / Quinn Skeleton for UE5（页面明示） |
| 面数 | 男/女各 34,519 tris / 22,424 verts；剑 4,268 tris |
| 内容 | 模块化分件（头盔/盔甲/裤子/头/手套/靴子）+ 男/女双版本 + 剑 |
| 动画 | **52 个**：Idle×2、Walk×16、Run×6、Attack×3、Hit Reactions×8、Death×1、Crouch×16 |
| 材质 | 可定制 color / roughness / metallic / **emissive** |
| 分发形式 | Asset package（Unreal Engine / Unity / fbx / obj） |

**集成要点**：
1. FBX 导入时 **Skeleton 选 `SKE_Wizard`，不要新建骨架**（骨名对上即可，`SKE_Wizard` 是 Mannequin 命名子集）
2. 导入后检查有无 skeleton mismatch 警告
3. 待补：敌人 AnimBP（可基于 `AnimBP_Wizard_ThirdPerson` 改造）、骨骼 socket（`Muzzle` / `Hit`）、奥术蓝紫 + emissive 材质实例
4. Walk×16 / Crouch×16 为方向化动画，需 BlendSpace 或挑 4~6 个用

**已知取舍**：
- 风格为"暗黑重甲骑士"，与"奥术守卫"设定有差距，靠材质改色 + emissive + Niagara 符文弥补
- 包内 Attack×3 为**剑系**动画，**不可用于玩家**（玩家为法杖）；玩家连击另走 `Mega Spear Animation Pack`（免费）或 `Essential Magic Staff Animation Pack`（$37.61）
- 附带好处：敌人持剑 / 玩家持杖，敌我视觉对比清晰
