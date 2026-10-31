// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/ArcaneVanguardBTTask_Attack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/ArcaneVanguardEnemy.h"

UArcaneVanguardBTTask_Attack::UArcaneVanguardBTTask_Attack()
{
	NodeName = TEXT("Attack (AV)");
}

EBTNodeResult::Type UArcaneVanguardBTTask_Attack::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AArcaneVanguardEnemy* Enemy = Cast<AArcaneVanguardEnemy>(AIController->GetPawn());
	if (!Enemy || Enemy->IsDead())
	{
		return EBTNodeResult::Failed;
	}

	// 攻击前先停下：CharacterMovement 开了 bOrientRotationToMovement，
	// 如果还在移动，它会持续把朝向拉向移动方向，覆盖下面这次转向。
	AIController->StopMovement();

	// 攻击前必须面向目标，否则球扫会朝着旧方向打空
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		if (AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)))
		{
			const FVector ToTarget =
				(Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				// Enemy 构造函数里已关掉 bUseControllerRotationYaw，这里不会被覆盖
				Enemy->SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
			}
		}
	}

	if (!Enemy->AttackMontage)
	{
		// 没配蒙太奇就不算"能攻击" —— 返回 Failed 让 BT 走别的分支
		return EBTNodeResult::Failed;
	}

	// 执行交给 Pawn：清空本刀命中记录 + PlayAnimMontage
	Enemy->BeginAttack();

	return EBTNodeResult::Succeeded;
}

FString UArcaneVanguardBTTask_Attack::GetStaticDescription() const
{
	return FString::Printf(TEXT("转向 %s 并播放攻击蒙太奇\n（出招频率由 Cooldown Decorator 控制）"),
		*TargetActorKey.SelectedKeyName.ToString());
}
