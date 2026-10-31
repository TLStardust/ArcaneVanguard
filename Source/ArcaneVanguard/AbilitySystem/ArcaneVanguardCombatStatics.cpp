// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ArcaneVanguardCombatStatics.h"

#include "AbilitySystem/ArcaneVanguardGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogAVCombat, Log, All);

float UArcaneVanguardCombatStatics::ApplyDamage(AActor* Target, float Damage, AActor* DamageCauser,
	TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	if (!IsValid(Target) || Damage <= 0.f)
	{
		return 0.f;
	}

	// ---- 受击方必须有 ASC，否则这个 Actor 根本不参与 GAS ----
	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(Target);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		UE_LOG(LogAVCombat, Verbose, TEXT("[Damage] %s 没有 ASC，忽略"), *GetNameSafe(Target));
		return 0.f;
	}

	// 直接用 TSubclassOf 而不是 UClass*：MakeOutgoingSpec 的参数就是 TSubclassOf，
	// 而 TSubclassOf 的构造函数只接受**非 const** 的 UClass*，写成 const UClass* 会编译失败。
	TSubclassOf<UGameplayEffect> EffectClass = DamageEffectClass;
	if (!EffectClass)
	{
		EffectClass = UGE_ArcaneDamage::StaticClass();
	}

	// ---- Source 侧 ----
	// GAS 需要一个 Source ASC 来构造 Spec。正常情况就是攻击者的 ASC；
	// 攻击者不是 GAS Actor（比如场景陷阱）时退化成"目标自己当 Source"，
	// 这样伤害照样能结算，只是 GE 的 Source 信息不准。
	// ⚠️ 联机时这个退化不成立（客户端不能拿别人的 ASC 造 Spec），到时必须传真实攻击者。
	AActor* SourceActor = IsValid(DamageCauser) ? DamageCauser : Target;
	IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
	UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
	if (!SourceASC)
	{
		SourceASC = TargetASC;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(SourceActor);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, /*Level=*/1.f, Context);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogAVCombat, Warning, TEXT("[Damage] 无法创建 Spec（GE=%s）"), *GetNameSafe(EffectClass.Get()));
		return 0.f;
	}

	// Health 是"当前值"，扣血 = 负增量。调用方永远传正数，取负在这里统一做，
	// 避免调用方一会儿传正一会儿传负，把符号搞乱。
	SpecHandle.Data->SetSetByCallerMagnitude(AVGameplayEffectKeys::Damage(), -FMath::Abs(Damage));

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogAVCombat, Verbose, TEXT("[Damage] %s → %s : %.1f"),
		*GetNameSafe(SourceActor), *GetNameSafe(Target), Damage);

	return Damage;
}
