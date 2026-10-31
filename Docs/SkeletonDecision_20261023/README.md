# 骨架决策：是否把巫师切换成 UE5 Manny（小白人）骨架？

**日期**：2026-10-23 ｜ **状态**：已决策（不切换）｜ **触发问题**：AV-023 要采购攻击动画，负责人怀疑"换成小白人骨架或 Mixamo 才最好配"

## 结论（TL;DR）

**不要切换。保持 `SKE_Wizard` 不动。** 实测证明本骨架与 UE5 Manny 骨架是「命名子集」关系（70/70 骨骼名全部命中），市面上标注 "UE4 Mannequin / UE5 Manny compatible" 的动画包对本骨架完全等价——不存在"选错骨架"的问题。小白人/Mixamo 内容通过一次性 **IK Retargeter** 即可批量接入，不需要拿主角网格去迁就骨架。

## 实测数据（非记忆，来自引擎资产二进制名称表扫描）

方法：解析 `UE_5.4/Engine/Plugins/Experimental/MoverExamples/Content/Characters/Mannequins/Meshes/SKM_Manny.uasset` 的 name table，与人形骨骼词汇表求交，与 `SKE_Wizard` 的 70 骨完整骨骼树（组件实测 `get_bone_name(i)` 全量枚举）逐骨对比。

| 项 | 数量 | 说明 |
|---|---|---|
| `SKE_Wizard` 骨骼 | **70** | 完整树已枚举（见 `ki013_weapon_bone_probe2.json`） |
| Manny 命中人形词汇表骨骼 | **73** | 名称表扫描 |
| 共有骨骼 | **70** | **SKE_Wizard 的每一个骨骼名都能在 Manny 里找到** |
| 仅 Manny 有 | 3 | `spine_04`、`spine_05`、`neck_02`（Manny 的延展脊柱/颈椎） |
| 仅 SKE_Wizard 有 | **0** | — |

即：**`SKE_Wizard ⊂ Manny skeleton`，严格子集**。连 `weapon_l` / `weapon_r`（武器挂载骨，父骨分别为 `hand_l` / `hand_r`）在 Manny 上都存在——本工程实测（`ki013_weapon_bone_probe2.json`：`parent_weapon_r = hand_r`）。

原始数据：`manny_vs_wizard_bones.json`。

## 为什么不切换（三条理由）

1. **命名已完全兼容，切换收益≈0**。动画包标注的 "Epic skeleton / UE4 Mannequin / UE5 Manny compatible" 三种说法，对本骨架是同一件事。UE4 Mannequin 命名正是 Marketplace  legacy 包最大的兼容面，而本骨架就是 UE4 Mannequin 命名（+ `weapon_l/r`，Manny 也有）。
2. **切换成本与风险很高**：
   - `SK_wizard` 网格的蒙皮权重绑定在 `SKE_Wizard` 骨骼上，换骨架 = 重新导入/重新绑定；
   - `AnimBP_Wizard_ThirdPerson`（目标骨架 SKE_Wizard）、现有 17 个动画资产全部受牵连；
   - KI-013 的握持标定（`weapon_r` 挂点、手指弯曲层旋转值）是按当前骨架实测的，需全部重做；
   - Manny 多的 `spine_04/05`、`neck_02` 在巫师网格上**没有蒙皮**，这三根骨上的动画轨会被丢弃或错位。
3. **换骨架并不能解决"找动画"的真正问题**：
   - **Mixamo 是独立骨架**（Hips/Spine/… 自有命名），对任何目标骨架都要重定向——换成 Manny 不会让 Mixamo 变即插即用，对 Mixamo 零帮助；
   - 真正决定"巫师拿着法杖像不像"的是**动画内容是否持杖**，不是骨架是谁。小白人通用近战包挥拳踢腿都没有杖；staff 专用包（如 `Essential Magic Staff Animation Pack`，已实测其 `ik_hand_gun` 命名与本骨架 1:1）才是对口的。

## 正确的基础设施：一次性 IK Retargeter（而非换骨架）

UE5 的 IK Retargeter 是**基于骨骼链**的映射（不是逐骨名），Manny → SKE_Wizard 的映射几乎是恒等映射（本骨架是 Manny 的命名子集，链结构一致），一次性配置后：

- 任何 UE5-Manny-native 包 → 批量重定向进 SKE_Wizard；
- 任何 Mixamo 动画 → 同一条路；
- 未来买任何骨架的包都不再发愁。

**一次性设置步骤（约 15 分钟，需负责人在编辑器执行）**：

1. 右键 `SKE_Wizard` → Create IK Rig，Retargeting 模式选 **Humanoid**，点 Auto Map Chains（应能自动识别全部链）；
2. 对 Manny 侧 IK Rig 重复（或直接用引擎自带的）；
3. 新建 IK Retargeter：Source = Manny IKRig，Target = SKE_Wizard IKRig；
4. 用 `ThirdPersonMap` 里的角色 Preview 验证（待机/施法姿势无扭曲）；
5. 之后买到动画：右键动画 → Retarget → 选该 Retargeter，批量导出到 `/Game/Wizard/Animations/`。

> 若某包的骨架恰好是 UE4 Mannequin 命名（如 Essential Magic Staff 包），甚至可以**导入时直接选 SKE_Wizard 为 Skeleton**（骨骼名一致，缺的轨保持参考姿势）——比重定向还省一道工序。混合包（自带 .uasset + 独立骨架）走重定向器。

## 对 AV-023 采购结论的修正

`TechnicalDesign.md` §5.8.1 候选表里 "仅 UE5 Manny 骨架需重定向" 不再构成排除理由：

- **首选不变**：`Essential Magic Staff Animation Pack`（$37.61，UE4 Mannequin 命名，最接近即插即用，staff 连击对口）；
- **解禁**：`Simple Magic Staff Animation Pack`（$26.86，仅 UE5 Manny）——配置好重定向器后可用；
- Mixamo 仍作为免费备选，同样走重定向器。

## 相关文档

- 骨骼对比数据：`manny_vs_wizard_bones.json`
- KI-013 握持修复（含 `weapon_r` 握点优化）：`../KI013_FingerGrip_20261022/README.md`
- 动画采购候选：`../TechnicalDesign.md` §5.8.1
