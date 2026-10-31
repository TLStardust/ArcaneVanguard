// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ArcaneVanguardGameplayAbility.h"

#include "ArcaneVanguardCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameplayEffect.h"

UArcaneVanguardGameplayAbility::UArcaneVanguardGameplayAbility()
{
	// 每个角色一份实例：技能里可以存 per-actor 状态（比如连招段数）
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 单机项目其实用不上，但保持预测执行的默认策略，将来上联机不用改
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	MontagePlayRate = 1.0f;
	bHideWeaponWhileCasting = true;
	bWeaponHiddenByThisAbility = false;
}

AArcaneVanguardCharacter* UArcaneVanguardGameplayAbility::GetAvatarCharacter() const
{
	return Cast<AArcaneVanguardCharacter>(GetAvatarActorFromActorInfo());
}

void UArcaneVanguardGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 提交消耗与冷却。CD 中 / Mana 不够时这里返回 false，
	// 必须自己 EndAbility，否则技能会一直挂在激活状态、后续再也放不出来。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 1) 藏武器
	//    放在播蒙太奇之前：第一帧就是"空手施法"，不会闪一下法杖。
	if (bHideWeaponWhileCasting)
	{
		if (AArcaneVanguardCharacter* Avatar = GetAvatarCharacter())
		{
			Avatar->SetWeaponHidden(true);
			bWeaponHiddenByThisAbility = true;
		}
	}

	// 2) 施加自身效果（Buff / 治疗 / 属性加成）
	//    注意：GE 有自己的 Duration，技能结束后它们会继续跑完。
	ApplySelfEffects();

	// 3) 播蒙太奇；没有动画就直接收尾
	if (AbilityMontage)
	{
		// 用 AbilityTask 而不是直接 PlayAnimMontage：
		// 它处理了技能被打断时的清理，还提供 OnCompleted / OnInterrupted / OnCancelled 三个回调，
		// 保证武器一定会在正确的时机恢复。
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, AbilityMontage, MontagePlayRate);

		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);

		MontageTask->ReadyForActivation();
	}
	else
	{
		// 无动画技能：效果已经施加，立刻收尾。
		// 比如纯 Buff 类技能不需要施法动作，按下就生效。
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

void UArcaneVanguardGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 恢复武器。不管技能是正常结束、被打断还是被取消，这里都会执行 ——
	// 所以不会出现"技能中断了法杖永远消失"。
	if (bWeaponHiddenByThisAbility)
	{
		if (AArcaneVanguardCharacter* Avatar = GetAvatarCharacter())
		{
			Avatar->SetWeaponHidden(false);
		}

		bWeaponHiddenByThisAbility = false;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UArcaneVanguardGameplayAbility::ApplySelfEffects()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : SelfEffectClasses)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		// 把技能对象挂到 Context 上，便于日后追踪"这个 Buff 是谁给的"
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle SpecHandle =
			ASC->MakeOutgoingSpec(EffectClass, GetAbilityLevel(), Context);

		if (SpecHandle.IsValid())
		{
			// 施加给自己：Buff 类技能都是"给自己上状态"
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void UArcaneVanguardGameplayAbility::HandleMontageCompleted()
{
	// 动画播完 → 正常收尾。GE 继续按自己的 Duration 跑，不受影响。
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UArcaneVanguardGameplayAbility::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}
