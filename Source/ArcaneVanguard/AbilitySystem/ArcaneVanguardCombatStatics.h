// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcaneVanguardCombatStatics.generated.h"

class UGameplayEffect;

/**
 * 战斗通用静态函数。
 *
 * ADR-005「伤害唯一入口」的落地处：**全项目只有 ApplyDamage 能扣血**。
 * 任何地方想造成伤害都走它，不要在别处直接改 Health 或 ApplyGameplayEffect，
 * 否则伤害统计、免疫、护盾、暴击这些东西以后没有统一的插入点。
 *
 * 之所以做成静态函数库而不是某个 Actor 的成员：
 *   - 玩家打敌人、敌人打玩家、陷阱打任何人，三种情况都要用，不能绑在某一方身上；
 *   - 蓝图里也能直接调，方便调试和做原型。
 */
UCLASS()
class ARCANEVANGUARD_API UArcaneVanguardCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 对目标施加伤害。
	 *
	 * @param Target             受击方。必须实现了 IAbilitySystemInterface 且 ASC 非空
	 * @param Damage             伤害值，传正数。<= 0 直接返回 0（不做治疗，治疗另开函数）
	 * @param DamageCauser       施加方（一般是攻击者 Actor）。用于 GE 的 Source / Instigator，
	 *                           将来做击杀归属、仇恨、伤害回放都要靠它
	 * @param DamageEffectClass  伤害 GE 类。留空 = 用原生 UGE_ArcaneDamage
	 * @return                   实际结算的伤害值；目标无效/无 ASC 时返回 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage", meta = (DefaultToSelf = "DamageCauser"))
	static float ApplyDamage(AActor* Target, float Damage, AActor* DamageCauser = nullptr,
		TSubclassOf<UGameplayEffect> DamageEffectClass = nullptr);
};
