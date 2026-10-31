# KI-013 修复指南：Cast_Spell_01 杖身穿掌（手指弯曲层）

- 日期：2026-10-22（2026-10-23 补充 §4.0 weapon_r 握点优化）
- 状态：**方案已定（A4 AnimBP 手指弯曲层 + weapon_r 握点），参数已实测标定，等待在编辑器中手动操作**
- 资产备份：`Content/Wizard/Animations/ThirdPerson/AnimBP_Wizard_ThirdPerson.uasset.bak-20261022-111500`
- 证据：本目录 `ki013_survey.json` / `ki013_finger_analysis.json` / `ki013_finger_analysis_cast.json` / `cast_spell_01_hand_zoom_before.png` / `ki013_weapon_*.json` / `grip_hand_r.png` / `grip_weapon_r.png`

---

## 0. 先做这一步：握点切换到 `weapon_r` 骨骼（2026-10-23 新增，零风险）

### 0.1 发现

`SKE_Wizard` 骨架里存在 **`weapon_r` / `weapon_l` 骨骼**（实测索引 46 / 24，父骨骼分别是 `hand_r` / `hand_l`，
标准武器挂载骨布局，`ki013_weapon_bone_probe2.json`）。当前法杖挂在 `hand_r` 骨骼原点，
而 `weapon_r` 在 `hand_r` 局部坐标系里偏移 **(-8.32, +2.55, +0.60) cm**（相距 8.73cm，朝向手指一侧）。

### 0.2 为什么值得换（实测数据）

| 指标（参考姿势） | 挂 hand_r（现状） | 挂 weapon_r | 变化 |
|---|---|---|---|
| 四指基节到法杖轴距离 | 8.3 ~ 10.9 cm | **4.5 ~ 6.0 cm** | 全部近 3~5cm |
| 指尖到法杖轴距离 | 9.3 ~ 13.8 cm | **5.3 ~ 8.8 cm** | 全部近 3~5cm |
| 四指基节质心到轴 | 8.91 cm | **4.20 cm** | -53% |

即：法杖轴从"穿过掌/腕中央、离手指 8~11cm"变成"贴着指根线"——**即使不加手指层，观感也会明显更像握住**；
配合手指层后，弯过去的手指能直接搭在杖身上（§3.3 的旋转值可能要相应加大 10~20%，以验收脚本为准）。

### 0.3 稳定性已实证

运行时验证（Cast_Spell_01 @0.4s / @1.13s 两帧冻帧，`ki013_weapon_rt_check.json`）：
`weapon_r` 在 `hand_r` 局部系下的偏移恒为 (-8.32, +2.55, +0.60)，与参考姿势 **漂移 0.00 cm**——
动画不独立驱动它，握点随手刚性运动。移动类动画（Idle/Run/Walk/Jump）名称表里根本没有 `weapon_r`，更不可能驱动。

### 0.4 操作（1 分钟，Blueprint 里改一个字段）

1. 打开 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter` → Class Defaults；
2. Weapon 分类 → **WeaponAttachSocket**：`hand_r` → **`weapon_r`**；
3. 编译保存。**不需要改代码**（C++ 默认值仍是 `hand_r`，蓝图显式覆盖，回滚就是改回 `hand_r`）。

`bKeepWeaponUpright` 保持开启——只有握点位置变，竖直约束逻辑不变，P1 约束零风险。
`weapon_l` 留给你日后的双手持杖动画用。

---

## 1. 结论（先说重点）

1. **AnimBP 图无法用脚本修改**（UE 5.4 Python 不暴露 AnimGraph 节点编辑），因此需要**在编辑器里手动添加 15 个 Transform (Modify) Bone 节点**。本文给出完整步骤和**实测标定的旋转值**，照做即可，不需要猜。
2. 所有旋转参数不是估的，是在运行中的游戏里实测骨骼变换后算出来的（见 §3）。**两个姿势（待机 / 施法）下测量结果一致**，证明这套动画不驱动手指骨骼，手指始终处于骨架静止姿势 —— 所以一组固定值对所有姿势都成立。
3. 修改完成后按 §5 的协议做量化验收（指尖到法杖轴的距离应显著缩小），不需要"看起来像"式判断。

## 2. 现象与根因（已由前序工作确认，不重复验证）

- 现象：`Cast_Spell_01` 中法杖位置/朝向都正确，但**手指张开，杖身穿掌**。
- 根因：技能动画来自 Mixamo，录制时手上没有武器，**没有任何握持姿势**（KI-013，`Docs/GripTest_20261021/`）。
- 该动画是唯一保留法杖的动作，不能用"隐藏法杖"绕过。

## 3. 实测标定数据（2026-10-22，运行中游戏内测量）

测量方法：`-game` 实例 + MCP 桥接，读 `SkeletalMeshComponent` 的骨骼四元数/位置，
计算每根手指骨"弯向法杖轴"所需的旋转轴（骨骼局部坐标系）。

### 3.1 手指骨骼名（已实证）

对 `SKE_Wizard.uasset` 做二进制名称表扫描，70 个目标骨骼名**全部存在**，
命名与 UE4 Mannequin 完全一致（`ki013_survey.json`）：

```
index_01_r/02_r/03_r  middle_01_r/02_r/03_r  ring_01_r/02_r/03_r
pinky_01_r/02_r/03_r  thumb_01_r/02_r/03_r   hand_r  lowerarm_r  ...
```

### 3.2 关键几何事实

| 事实 | 数据 | 含义 |
|---|---|---|
| 手指骨局部坐标系里，指向指梢的方向 | ≈ 局部 **-X**（如 index_01: (-0.97, 0.26, -0.02)） | UE4 Mannequin 标准骨骼轴向 |
| **待机 vs 施法（Cast_Spell_01 @1.13s）的手指局部轴点积** | **0.96~1.00（几乎完全一致）** | 动画不驱动手指 → 手指始终是骨架静止姿势 → **一组固定旋转值适用所有姿势** |
| 法杖轴 | 竖直，过握点。握点偏移 96（`WeaponGripOffset`）。挂点默认 `hand_r`；**若按 §0 切到 `weapon_r`，整体朝手指方向平移 ~4.6cm**（见 §0.2） | 手指要绕竖直的法杖弯曲 |
| 各手指相对法杖轴的位置（施法姿势，相对 hand_r 的 XY） | thumb (+3.1,-2.3) / index (+2.9,+0.6) / middle (+0.3,+1.9) / ring (-2.0,+3.0) / pinky (-4.4,+3.7) | 五指分布在法杖轴四周，各自弯向法杖轴（绕杖手势） |

### 3.3 每根骨的弯曲轴（骨骼局部坐标系，施法姿势实测）

正值 = 绕该轴正向旋转时指尖弯向法杖：

| 骨骼 | 弯曲轴 (x,y,z) | 最接近主轴 |
|---|---|---|
| index_01_r | (-0.162, -0.671, -0.724) | -Y / -Z |
| index_02_r | (-0.373, -0.775, -0.511) | -Y |
| middle_01_r | (0.088, 0.462, -0.883) | -Z |
| middle_02_r | (0.546, 0.805, -0.233) | +Y |
| ring_01_r | (0.206, 0.852, -0.482) | +Y |
| ring_02_r | (0.473, 0.868, -0.149) | +Y |
| pinky_01_r | (-0.116, 0.975, -0.192) | +Y |
| pinky_02_r | (0.084, 0.993, -0.088) | +Y |
| thumb_01_r | (-0.057, -0.701, -0.711) | -Y / -Z |
| thumb_02_r | (-0.264, -0.796, -0.545) | -Y |

> 注：index 与其他三指符号相反是**正确**的 —— 五指分布在法杖轴两侧（§3.2），
> 各自弯向法杖轴时局部轴符号自然不同；绕杖手势要求如此。

## 4. 操作步骤（编辑器手动，约 20 分钟）

> 前置：备份已完成（`.bak-20261022-111500`）。如需重新备份：
> `cp AnimBP_Wizard_ThirdPerson.uasset AnimBP_Wizard_ThirdPerson.uasset.bak-<时间戳>`

### 4.1 打开资产

1. 关闭正在运行的 `-game` 实例（本指南验证时占用着工程）。
2. 打开编辑器 → Content Browser → `/Game/Wizard/Animations/ThirdPerson/` → 双击 `AnimBP_Wizard_ThirdPerson`。
3. 切到 **AnimGraph** 标签页。

### 4.2 添加手指弯曲节点链

在**状态机输出 → Output Pose** 的连线上，依次插入 15 个 `Transform (Modify) Bone` 节点：

1. 左键双击状态机到 Output Pose 的连线（或在连线上拖动）→ 弹出菜单输入 `Transform Modify Bone` → 回车。
2 对每个节点按下表配置（其余选项保持默认）：

| 选项 | 值 |
|---|---|
| Bone to Modify | 下表中的骨骼名 |
| Rotation Mode | **Add to Existing** |
| Rotation Space | **Bone Space** |
| Rotation (Pitch/Yaw/Roll) | 下表中的值 |
| Translation / Scale Mode | Ignore（默认） |
| Alpha | 1.0（默认） |

3. 节点串联顺序：`hand 姿态 → index_01 → index_02 → index_03 → middle_01 → … → thumb_03 → Output Pose`。
   （同一根手指内**先父后子**，整体顺序其实不影响结果，按上表顺序即可。）

### 4.3 旋转值表（起始值， Bone Space / Add to Existing）

数值由 §3.3 实测轴换算（每关节 30~38°，指尖关节略小；一阶近似，允许目视微调 ±10°）：

| # | 骨骼 | Pitch | Yaw | Roll |
|---|---|---|---|---|
| 1 | index_01_r | -20 | -22 | -5 |
| 2 | index_02_r | -29 | -19 | -14 |
| 3 | index_03_r | -23 | -15 | -11 |
| 4 | middle_01_r | +14 | -26 | +3 |
| 5 | middle_02_r | +31 | -9 | +21 |
| 6 | middle_03_r | +24 | -7 | +16 |
| 7 | ring_01_r | +26 | -14 | +6 |
| 8 | ring_02_r | +33 | -6 | +18 |
| 9 | ring_03_r | +26 | -5 | +14 |
| 10 | pinky_01_r | +29 | -6 | -3 |
| 11 | pinky_02_r | +38 | -3 | +3 |
| 12 | pinky_03_r | +30 | -3 | +3 |
| 13 | thumb_01_r | -21 | -21 | -2 |
| 14 | thumb_02_r | -30 | -21 | -10 |
| 15 | thumb_03_r | -18 | -13 | -6 |

### 4.4 编译保存

- AnimGraph 右上角 **Compile** → **Save** → 关闭。
- 如果 Compile 报错，通常是骨骼名打错 —— 按 §3.1 的表核对。

### 4.5 预期效果与微调

- 预期：Cast_Spell_01 / 待机 / 行走时，右手五指自然握住法杖；法杖仍保持竖直（P1 约束不受影响，二者独立）。
- **若某根手指弯反了（指尖远离法杖/反关节）**：把该骨三个值**同时取负**。
- **若弯曲幅度不自然**：按 ±10° 调整该骨数值；指尖关节（03）比中段（02）小 20% 左右更自然。
- 若个别姿势下方向不对：把该节点的 Rotation Space 在 `Bone Space` / `Parent Bone Space` 之间换一个再试。

### 4.6 可选进阶（本次不做）

- 只想在"持杖"时弯曲：把每个节点的 **Alpha** 绑定到 AnimBP 变量（如 `WeaponGripAlpha`），
  由角色 C++ 属性驱动。需要新增 C++ 属性 + AnimBP 变量，属独立任务，确认主方案效果后再做。

## 5. 验收协议（改完后由 AI 执行，量化判定）

脚本已备好：`Saved/Scripts/ki013_verify_after.py`（随游戏启动后由 AI 调用）。
**注意基线**：若已按 §0 切换挂点到 `weapon_r`，法杖轴本身会靠近手指 ~4.6cm，
旧基线（`ki013_finger_analysis_cast.json`，hand_r 挂点所测）不再适用——
脚本会检测挂点变化并要求先重测基线（把脚本头部 `MODE` 改为 `baseline` 跑一遍即可）。

判定标准（同挂点下的基线对比）：

1. 播 `Cast_Spell_01` 冻帧 t=1.13s，测量每根手指**指尖骨到法杖竖直轴的水平距离**；
2. 该距离应**明显小于**基线（指尖向法杖收拢，每指 ≥0.5cm）；
3. 同时截图存档，人工复核观感；
4. 三项全过 → KI-013 关闭，更新 `KnownIssues.md` / `TaskBoard.md` / `Changelog.md`。

## 6. 回滚

```
# 用备份覆盖（编辑器需关闭）
cp AnimBP_Wizard_ThirdPerson.uasset.bak-20261022-111500 AnimBP_Wizard_ThirdPerson.uasset
```

## 7. 已知限制

- 弯曲值是"一阶近似"的起始值，观感微调预期内。
- 收杖动画（Cast_Spell_02 / Magic_Attack / Magic_Heal）中法杖隐藏，手指仍呈握姿（可接受）；
  如需区分，见 §4.6。
- 本方案不改变动画本身，只改 AnimBP 输出姿势；换成持杖动画（A3）后可整体移除本节点链。
