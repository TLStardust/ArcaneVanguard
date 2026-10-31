// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ArcaneVanguardWeaponNotifies.generated.h"

/**
 * 动画通知：隐藏武器。
 *
 * 背景：本工程的技能动画来自 Mixamo，作者录动作时手上没有武器，
 * 因此「双手前推」这类动作与法杖完全冲突。在这些片段上挂本通知把法杖收起，
 * 比让法杖插在身体里更合理。
 *
 * 用法：在动画/蒙太奇的 Notify 轨道上添加，开始处放 Hide、结束处放 Show。
 */
UCLASS(meta = (DisplayName = "Weapon: Hide"))
class UAnimNotify_HideWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

/**
 * 动画通知：显示武器。与 UAnimNotify_HideWeapon 配对使用。
 */
UCLASS(meta = (DisplayName = "Weapon: Show"))
class UAnimNotify_ShowWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
