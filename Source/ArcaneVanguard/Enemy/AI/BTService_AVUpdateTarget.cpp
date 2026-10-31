// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/AI/BTService_AVUpdateTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UBTService_AVUpdateTarget::UBTService_AVUpdateTarget()
{
	NodeName = TEXT("AV Update Target");

	// 0.2s 刷新一次足够：敌人移动速度 250cm/s，0.2s 才走 50cm
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	// 这两个键由用户在 BT 资产里指定，不在这里写死
	bNotifyTick = true;

	// 默认 20 米放弃追击
	LoseTargetDistance = 2000.f;
}

void UBTService_AVUpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (!Blackboard || !AIOwner)
	{
		return;
	}

	APawn* MyPawn = AIOwner->GetPawn();
	APawn* Target = UGameplayStatics::GetPlayerPawn(AIOwner, 0);

	// 没有 Pawn 或没有玩家：清空目标，让 Decorator 的 IsSet 失效
	if (!MyPawn || !Target)
	{
		if (!TargetActorKey.SelectedKeyName.IsNone())
		{
			Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
		}
		return;
	}

	const float CenterDistance = FVector::Dist2D(
		MyPawn->GetActorLocation(), Target->GetActorLocation());

	// 换成胶囊边缘距离
	float EdgeDistance = CenterDistance;
	if (const ACharacter* MyCharacter = Cast<ACharacter>(MyPawn))
	{
		EdgeDistance -= MyCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(Target))
	{
		EdgeDistance -= TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}

	// 距离无条件写：Decorator 可能要用它做阈值判断
	if (!EdgeDistanceKey.SelectedKeyName.IsNone())
	{
		Blackboard->SetValueAsFloat(EdgeDistanceKey.SelectedKeyName, EdgeDistance);
	}

	// 超出放弃距离：清空目标，敌人回到 Idle 分支
	if (EdgeDistance > LoseTargetDistance)
	{
		if (!TargetActorKey.SelectedKeyName.IsNone())
		{
			Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
		}
		return;
	}

	// 在放弃距离内才写目标（Decorator 的 IsSet 靠它）
	if (!TargetActorKey.SelectedKeyName.IsNone())
	{
		Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, Target);
	}
}
