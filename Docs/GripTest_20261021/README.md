# 握持验证（方案 B）—— 2026-10-21

- 目的：确认"法杖没握住"是挂载问题还是动画问题
- 方式：**纯运行时**，把角色切到单节点动画并冻结在指定帧，截图；**未修改任何资产**
- 结论：**这套资产的动画没有握持姿势** → 问题在动画，不在挂载

## 做法（可复现）

在运行的游戏实例里（编辑器桥接 `run_editor_python`）：

```python
mesh.play_animation(anim, False)      # 切到 AnimSingleNodeInstance，覆盖 AnimBP
mesh.set_play_rate(0.0)               # 停止推进
mesh.set_position(t, False)           # 冻在指定秒数
unreal.SystemLibrary.execute_console_command(w, 'HighResShot 1600x900')
```

相机：`SpringArm.target_arm_length = 360`、`do_collision_test = False`（否则会被墙挤进来），
角色 `yaw = 90` 让右手朝向镜头。全部为运行时改动，退出即还原。

## 各动画取样点

| 动画 | 时长 | 取样帧 |
|---|---|---|
| `Anim_Wizard_Cast_Spell_01` | 2.267s | 1.13s |
| `Anim_Wizard_Cast_Spell_02` | 2.167s | 1.08s |
| `Anim_Wizard_Magic_Attack` | 3.333s | 1.67s |
| `Anim_Wizard_Magic_Heal` | 2.667s | 1.33s |

## 观察结果

- **法杖位置是对的**：始终贴着手心，随手臂抬起/前伸而移动，没有漂移。
- **手型是张开的**：手指伸直，**法杖从手心里穿过去**，没有被手指包住（见 `02_cast_spell_01_hand_zoom.png`）。
- 施法时法杖会被手腕带动改变朝向（`Cast_Spell_01` 抬起时法杖斜向上，`Magic_Attack` 前刺时法杖接近水平）——
  这是刚性附着的正常行为，不是 bug。

## 推论

- 换一套**同样没有握持姿势**的动画（A1/A2 的常见情况）解决不了问题，只是换了个姿势穿模。
- 真正能解决的有两条：
  1. 找到/制作**手指弯曲包住杖身**的动画（A3 自己 K 最可控）；
  2. 在 AnimBP 里加**手指弯曲层**（A4），把五指骨骼掰成握姿 —— 法杖位置已经对了，只差手指。

## 截图清单

| 文件 | 内容 |
|---|---|
| `01_cast_spell_01_fullbody.png` | Cast_Spell_01 全身侧视（法杖斜向上举） |
| `02_cast_spell_01_hand_zoom.png` | **手部特写：手指伸直，杖身穿掌** |
| `03_magic_attack_hand_zoom.png` | Magic_Attack 手部特写（前刺，法杖近水平） |
| `04_cast_spell_02.png` | Cast_Spell_02（相机被墙体挤压，构图不可用） |
| `05_magic_heal.png` | Magic_Heal（同上，构图不可用） |
