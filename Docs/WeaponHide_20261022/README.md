# 武器自动显隐（Magic_Attack）—— 2026-10-22

## 背景：为什么这个动作必须收杖

`Anim_Wizard_Magic_Attack` 是"双手向前推"的动作。资产溯源发现它是 **Mixamo 动画**
（资产内含 `mixamo.com` 字符串），作者录制时手上没有任何武器。

用 P1 把法杖强制竖直之后，这个动作就变成了"法杖笔直穿过角色身体"，比穿模更糟。
收起武器比让它插在身体里合理。

## 实现

C++（`AArcaneVanguardCharacter`）：

```cpp
// 可编辑名单：播这些动画时自动收杖
UPROPERTY(EditDefaultsOnly, Category = Weapon)
TArray<FName> WeaponHiddenAnimNames;   // 默认: Anim_Wizard_Magic_Attack

void UpdateWeaponVisibilityFromAnimation();   // Tick 中调用
```

判定逻辑（每帧，状态变化时才真正调用 `SetWeaponVisible`）：

1. `AnimInstance->GetCurrentActiveMontage()` → 取 Montage 名（技能走这条路）
2. 否则 `Cast<UAnimSingleNodeInstance>` → `GetCurrentAsset()`（预览/测试走这条路）
3. 名字在 `WeaponHiddenAnimNames` 里 → 隐藏；否则恢复显示

同时提供了两个动画通知类 `UAnimNotify_HideWeapon` / `UAnimNotify_ShowWeapon`
（`Source/ArcaneVanguard/ArcaneVanguardWeaponNotifies.h/.cpp`），
供以后在动画/蒙太奇的 Notify 轨道上精确控制。

## 实测

| 状态 | `is_visible()` | `hidden_in_game` |
|---|---|---|
| 待机 | true | false |
| 播 `Magic_Attack` | **false** | **true** |
| 切回待机 | true | false |

截图：`magic_attack_before_after.png`

## 备注

- 名单是 **BP 可编辑**的（`Weapon Hidden Anim Names`）。
  **当前值（2026-10-22 按负责人实测调整）**：`Cast_Spell_02` / `Magic_Attack` / `Magic_Heal`。
  **`Cast_Spell_01` 故意不在名单里** —— 它的单手抬杖姿势可接受，保留法杖。
- 死亡动作走另一套：`Weapon Drop Anim Names`（默认 `Anim_Wizard_Death_Forward`）。
  命中后 `DropWeapon()`：脱手 + 开物理模拟 + 自然倒伏；动画结束自动 `RestoreWeapon()` 收回。
  注意掉落的法杖**忽略 ECC_Pawn 碰撞**，否则会被角色胶囊弹开。
- 这套机制按**资产名**匹配。以后换成真正的持杖动画时，把名字从名单里删掉就行。

## 遗留

- **KI-013 未解决**：`Cast_Spell_01` 的杖身穿掌属于"手没握住"，不能靠隐藏解决。
  这是唯一还保留法杖的动作，所以穿模只剩这一处。
- 没有"普通攻击"连击动画（现有 17 个全是技能/移动/死亡）→ 见 `TechnicalDesign.md` §5.8。
