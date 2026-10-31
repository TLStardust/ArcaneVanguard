// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ArcaneVanguardAttributeSet.h"

UArcaneVanguardAttributeSet::UArcaneVanguardAttributeSet()
{
	// 基础值直接给默认，避免"没有 GE 就 0 血"的坑。
	// 需要按角色类型给不同初值时，用 Character 的 StartupEffects 施加 GE 覆盖。
	InitHealth(100.0f);
	InitMaxHealth(100.0f);

	// M4：法力。技能消耗走 Cost GE（GE_Cost_Mana），这里只给初值。
	InitMana(100.0f);
	InitMaxMana(100.0f);

	// 攻速倍率：1.0 = 原速。Buff GE 以 Additive 方式叠加（+0.3 = 快 30%），
	// GE 到期移除时自动减回去，不需要手动恢复。
	InitAttackSpeedMultiplier(1.0f);
}

void UArcaneVanguardAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		// 不能低于 0，也不能超过当前上限
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// 上限本身不能为负
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetManaAttribute())
	{
		// 和 Health 同一套规则：钳在 [0, MaxMana]
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetAttackSpeedMultiplierAttribute())
	{
		// 钳在 [0.1, 5.0]：
		//   下限 0.1 —— 再低就是慢动作，而且 0 会让 PlayRate 变成 0（动画卡死）
		//   上限 5.0 —— 再高动画会跳帧到看不出动作
		NewValue = FMath::Clamp(NewValue, 0.1f, 5.0f);
	}
}
