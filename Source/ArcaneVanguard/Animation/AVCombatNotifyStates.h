// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AVCombatNotifyStates.generated.h"

/**
 * 连击窗口 Notify State。
 *
 * 放在每段攻击「判定已结束、收招尚未完成」的区间上。
 * 窗口开启期间按攻击键才会接下一段；窗口外按下会被缓冲，等窗口开启时再消费。
 *
 * 用法：在 Montage 的 Notify Track 上右键 → Add Notify State → AV Combo Window，
 * 拖到目标区间（比如 0.50s – 0.72s），每段各放一个。
 */
UCLASS(meta = (DisplayName = "AV Combo Window"))
class ARCANEVANGUARD_API UAnimNotifyState_AVComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};

/**
 * 攻击判定窗口 Notify State。
 *
 * 判定生效的区间。**窗口开启的瞬间做一次球形 Sweep 并结算伤害**（M3）。
 * 不做"窗口内每帧检测"：三段连击的判定窗口只有 0.2s，一次球扫足够；
 * 每帧扫会让同一刀在不同帧命中不同目标，手感不可控。
 *
 * 三段想给不同伤害时，不用改代码：在每段的这个 Notify 上填 DamageOverride 即可。
 */
UCLASS(meta = (DisplayName = "AV Attack Window"))
class ARCANEVANGUARD_API UAnimNotifyState_AVAttackWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/**
	 * 伤害覆盖。<= 0 表示用攻击者自己的 AttackDamage。
	 * 想给第二段更高伤害（比如 30 / 35 / 50），在对应 Notify 上填即可。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AV")
	float DamageOverride = -1.f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
