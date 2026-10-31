# 调试按键：动画逐个播放 —— 2026-10-22

## 用途

方便逐个人眼观察每个动画的表现（尤其是法杖的握持与朝向），不用改蓝图连状态机。

## 按键映射

| 键 | 动画 | 时长 | 武器行为 |
|---|---|---|---|
| **1** | `Anim_Wizard_Cast_Spell_01` | 2.267s | **保留法杖** |
| **2** | `Anim_Wizard_Cast_Spell_02` | 2.167s | 自动收杖 |
| **3** | `Anim_Wizard_Magic_Attack` | 3.333s | 自动收杖 |
| **4** | `Anim_Wizard_Magic_Heal` | 2.667s | 自动收杖 |
| **5** | `Anim_Wizard_Death_Forward` | — | **脱手掉落**（物理模拟） |

按键顺序 = `BP_ThirdPersonCharacter` 上 `Debug Animations` 数组的顺序（Index 0 → 键 1）。

## 行为

1. 按下数字键 → `SetAnimationMode(AnimationSingleNode)` + `PlayAnimation(seq, false)`，覆盖 AnimBP
2. 每帧 `UpdateDebugAnimationState()` 检查 `GetSingleNodeInstance()->IsPlaying()`
3. 播放结束 → `SetAnimationMode(AnimationBlueprint)`，**自动回到行走状态机**
4. 期间 `UpdateWeaponVisibilityFromAnimation()` 照常工作，命中 `WeaponHiddenAnimNames` 就收杖

## 实测时间线（`Saved/Logs/DebugKeys_20261022.log`）

```
[DebugAnim] 播放 [2] Anim_Wizard_Magic_Attack     02:33:21.422
[Weapon] visible = false                          02:33:21.429
[DebugAnim] 回到 AnimBP 状态机                     02:33:24.760   ← 3.338s ≈ 3.333s 时长
[Weapon] visible = true                           02:33:24.760

[DebugAnim] 播放 [0] Anim_Wizard_Cast_Spell_01    02:34:04.709
[DebugAnim] 回到 AnimBP 状态机                     02:34:06.986   ← 2.277s ≈ 2.267s 时长
（无 [Weapon] 切换 —— 该动画不在收杖名单里，符合预期）
```

## 扩展方法

- 改动画：在 BP 的 `Debug Animations` 数组里换/加（最多 5 个对应 1~5 键）。
- 加更多键：需要改 C++（`SetupPlayerInputComponent` 里加 `BindKey` + 一个 `DebugPlayAnimN()`）。
- **发布前**：把 `Enable Debug Anim Keys` 关掉，否则按键会一直生效。

## 相关属性

| 属性 | 位置 | 默认 |
|---|---|---|
| `Debug Animations` | BP_ThirdPersonCharacter | 5 个动画 |
| `Enable Debug Anim Keys` | 同上 | true |
| `Weapon Hidden Anim Names` | 同上 | `Cast_Spell_02` / `Magic_Attack` / `Magic_Heal` |
| `Weapon Drop Anim Names` | 同上 | `Anim_Wizard_Death_Forward` |
