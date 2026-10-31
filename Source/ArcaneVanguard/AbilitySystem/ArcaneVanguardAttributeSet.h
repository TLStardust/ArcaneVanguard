// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ArcaneVanguardAttributeSet.generated.h"

/**
 * 生成属性的标准访问器：GetXxxAttribute() / GetXxx() / SetXxx() / InitXxx()。
 * 引擎只在 AttributeSet.h 的注释里给出示例，没有提供现成宏，所以这里自己定义一份。
 * 用 #ifndef 保护，避免将来别处也定义时重定义报错。
 */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

/**
 * 玩家属性集。
 *
 * Health / MaxHealth（M2）、Mana / MaxMana（M4）。
 *
 * 注意：属性值的**初始化**放在构造函数里（InitHealth/InitMaxHealth/InitMana/InitMaxMana），
 * 这样不依赖任何蓝图资产就能跑通。需要按角色类型区分初值时，
 * 改用 Character 上的 StartupEffects 施加 GE 覆盖即可（敌人就是这么做的：
 * AArcaneVanguardEnemy::MaxHealthOverride）。
 */
UCLASS()
class ARCANEVANGUARD_API UArcaneVanguardAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UArcaneVanguardAttributeSet();

	/** 当前生命值 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UArcaneVanguardAttributeSet, Health)

	/** 生命值上限 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UArcaneVanguardAttributeSet, MaxHealth)

	/** 当前法力（技能消耗） */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UArcaneVanguardAttributeSet, Mana)

	/** 法力上限 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UArcaneVanguardAttributeSet, MaxMana)

	/**
	 * 攻速倍率。1.0 = 正常，1.3 = 快 30%。
	 *
	 * 由 Buff 类 GE 修改（Additive），普攻播放时乘到 PlayRate 上。
	 * 它只影响**动画播放速度**，不影响单刀伤害 —— 伤害仍然由 Attack Window 的 Notify 决定。
	 * 基础值 1.0，钳在 [0.1, 5.0]：太低会变成慢动作，太高动画会跳帧。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UArcaneVanguardAttributeSet, AttackSpeedMultiplier)

protected:
	/**
	 * 属性值即将变化时做钳制。
	 * 这里是唯一保证「Health ∈ [0, MaxHealth]」的地方 ——
	 * 任何 GE、任何代码改这两个属性都会经过它，所以不用在别处重复 clamp。
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
};
