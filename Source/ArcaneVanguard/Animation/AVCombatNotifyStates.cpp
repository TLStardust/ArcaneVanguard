// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AVCombatNotifyStates.h"

#include "ArcaneVanguardCharacter.h"
#include "Enemy/ArcaneVanguardEnemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/** 从 MeshComp 反查角色。Notify 只应该作用在角色身上，其它 Actor 静默忽略 */
	AArcaneVanguardCharacter* GetOwnerCharacter(const USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return nullptr;
		}
		return Cast<AArcaneVanguardCharacter>(MeshComp->GetOwner());
	}
}

//////////////////////////////////////////////////////////////////////////
// UAnimNotifyState_AVComboWindow

void UAnimNotifyState_AVComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (AArcaneVanguardCharacter* Character = GetOwnerCharacter(MeshComp))
	{
		Character->SetComboWindowOpen(true);
	}
}

void UAnimNotifyState_AVComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (AArcaneVanguardCharacter* Character = GetOwnerCharacter(MeshComp))
	{
		Character->SetComboWindowOpen(false);
	}
}

FString UAnimNotifyState_AVComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("AV Combo Window");
}

//////////////////////////////////////////////////////////////////////////
// UAnimNotifyState_AVAttackWindow

void UAnimNotifyState_AVAttackWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}

	// 玩家：开判定窗口 + 球扫
	if (AArcaneVanguardCharacter* Character = Cast<AArcaneVanguardCharacter>(Owner))
	{
		// 顺序不能反：先开窗口（内部清空本段命中记录），再判定。
		// 反过来的话，新一刀会带着上一刀的命中记录，打同一个敌人时直接跳过。
		Character->SetAttackWindowOpen(true);
		Character->PerformAttackHitCheck(DamageOverride);
		return;
	}

	// 敌人：同一个 Notify 复用。敌人没有"窗口"状态，命中记录在每次出招时清空
	if (AArcaneVanguardEnemy* Enemy = Cast<AArcaneVanguardEnemy>(Owner))
	{
		Enemy->PerformAttackHitCheck();
	}
}

void UAnimNotifyState_AVAttackWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (AArcaneVanguardCharacter* Character = GetOwnerCharacter(MeshComp))
	{
		Character->SetAttackWindowOpen(false);
	}
}

FString UAnimNotifyState_AVAttackWindow::GetNotifyName_Implementation() const
{
	return TEXT("AV Attack Window");
}
