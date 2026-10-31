// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ArcaneVanguardGameplayEffects.h"

#include "AbilitySystem/ArcaneVanguardAttributeSet.h"

UGE_ArcaneDamage::UGE_ArcaneDamage()
{
	// 瞬时效果：施加即结算，不产生持续状态
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 唯一的 Modifier：Health += (-Damage)
	// ModifierOp 用 Additive，配合施加方传的负值实现扣血。
	// 不用 Multiplicitive：伤害是绝对值，不是比例。
	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute = UArcaneVanguardAttributeSet::GetHealthAttribute();
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;

	// magnitude 由施加方通过 SetByCaller 传入，GE 本身不写死数值
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = AVGameplayEffectKeys::Damage();
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(ModifierInfo);
}

UGE_Q_Regen::UGE_Q_Regen()
{
	// 持续型：5 秒后自动移除，不需要手动清理
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));

	// 每秒结算一次（不是每帧）。
	// [!] Period 作用于本 GE 的**所有** Modifier —— 所以攻速必须拆到另一个 GE，
	//     否则每秒 +0.3 会累加成 1.3 → 1.6 → 1.9。
	Period = FScalableFloat(1.0f);

	// 每次跳 +10 血。上限钳制由 PreAttributeChange 负责，满血时不会溢出。
	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute = UArcaneVanguardAttributeSet::GetHealthAttribute();
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.0f));

	Modifiers.Add(ModifierInfo);
}

UGE_Q_Haste::UGE_Q_Haste()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));

	// 故意不设 Period：一次性加成，持续整个 Duration。
	// 到期移除时 Additive 的 +0.3 会被自动减回去，不需要手动恢复。

	// 攻速 +0.3（Additive：1.0 → 1.3，即快 30%）
	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute = UArcaneVanguardAttributeSet::GetAttackSpeedMultiplierAttribute();
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.3f));

	Modifiers.Add(ModifierInfo);
}
