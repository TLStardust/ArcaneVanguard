// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcaneVanguardWeaponNotifies.h"

#include "ArcaneVanguardCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	void ApplyWeaponVisibility(USkeletalMeshComponent* MeshComp, bool bVisible)
	{
		if (!MeshComp)
		{
			return;
		}

		if (AArcaneVanguardCharacter* Character = Cast<AArcaneVanguardCharacter>(MeshComp->GetOwner()))
		{
			Character->SetWeaponVisible(bVisible);
		}
	}
}

FString UAnimNotify_HideWeapon::GetNotifyName_Implementation() const
{
	return TEXT("Weapon: Hide");
}

void UAnimNotify_HideWeapon::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ApplyWeaponVisibility(MeshComp, false);
}

FString UAnimNotify_ShowWeapon::GetNotifyName_Implementation() const
{
	return TEXT("Weapon: Show");
}

void UAnimNotify_ShowWeapon::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ApplyWeaponVisibility(MeshComp, true);
}
