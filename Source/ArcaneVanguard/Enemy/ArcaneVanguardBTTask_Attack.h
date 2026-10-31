// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "ArcaneVanguardBTTask_Attack.generated.h"

/**
 * 敌人攻击节点：转向目标 → 让 Pawn 播攻击蒙太奇 → 立即返回 Succeeded。
 *
 * 为什么是「立即返回」而不是等蒙太奇播完：
 *   BT 的设计哲学是「节点小而快，等待交给 Wait」。
 *   在 BT 里给 Attack 后面接一个 Wait（约等于蒙太奇长度）即可，
 *   不需要在 Task 里管委托和 latent 生命周期，节点也就无状态、可复用。
 *
 * 出招频率由 BT 上的 Cooldown Decorator 控制，不在 C++ 里写冷却计时。
 */
UCLASS()
class ARCANEVANGUARD_API UArcaneVanguardBTTask_Attack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UArcaneVanguardBTTask_Attack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 攻击前转向的目标（通常是 TargetActor） */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;
};
