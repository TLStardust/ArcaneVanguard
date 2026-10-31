// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_AVUpdateTarget.generated.h"

/**
 * 每 Interval 秒刷新一次 Blackboard：
 *   - TargetActorKey   ← 0 号本地玩家（Object 类型键）
 *   - EdgeDistanceKey  ← 双方胶囊**边缘**距离，单位 cm（Float 类型键）
 *
 * 为什么用边缘距离而不是中心距：
 *   石头人胶囊半径 90、玩家 42，中心距天然比"看起来的距离"多 132cm。
 *   用中心距配阈值，会显得"隔空开打"。
 *
 * 用法：挂在 BT 的 Selector 或 Sequence 上作为 Service，Interval 默认 0.2s。
 */
UCLASS()
class ARCANEVANGUARD_API UBTService_AVUpdateTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_AVUpdateTarget();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** 把目标 Actor 写到这个 Blackboard 键（需要在 BB 里建一个 Object 类型的键） */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** 把边缘距离写到这个 Blackboard 键（需要在 BB 里建一个 Float 类型的键） */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector EdgeDistanceKey;

	/**
	 * 放弃距离（cm，胶囊边缘）。
	 * 超过这个距离就清空 TargetActorKey —— BT 里带「TargetActor Is Set」的 Decorator
	 * 会因此失效，敌人自然回到 Idle 分支，不需要额外写代码。
	 */
	UPROPERTY(EditAnywhere, Category = "Target", meta = (ClampMin = "0.0"))
	float LoseTargetDistance;
};
