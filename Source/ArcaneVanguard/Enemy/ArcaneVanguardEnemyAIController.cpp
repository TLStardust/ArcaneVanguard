// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/ArcaneVanguardEnemyAIController.h"

#include "Enemy/ArcaneVanguardEnemy.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAVEnemyAI, Log, All);

const FName AArcaneVanguardEnemyAIController::Key_TargetActor(TEXT("TargetActor"));
const FName AArcaneVanguardEnemyAIController::Key_DistanceToTarget(TEXT("DistanceToTarget"));

AArcaneVanguardEnemyAIController::AArcaneVanguardEnemyAIController()
{
	// 决策全在 BT 里；Controller 的 Tick 保留给将来可能的表现层逻辑，开销可忽略
	PrimaryActorTick.bCanEverTick = true;
}

void AArcaneVanguardEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!BehaviorTreeAsset)
	{
		UE_LOG(LogAVEnemyAI, Error,
			TEXT("[EnemyAI] %s 没有指定 BehaviorTreeAsset —— 敌人不会行动。")
			TEXT("请在 BP_AV_EnemyAIController -> AI -> Behavior Tree Asset 里指定 BT_Enemy。"),
			*GetName());
		return;
	}

	if (!RunBehaviorTree(BehaviorTreeAsset))
	{
		UE_LOG(LogAVEnemyAI, Error,
			TEXT("[EnemyAI] %s RunBehaviorTree 失败（BT=%s）。检查 BT 是否设置了 Blackboard。"),
			*GetName(), *GetNameSafe(BehaviorTreeAsset));
		return;
	}

	UE_LOG(LogAVEnemyAI, Log, TEXT("[EnemyAI] %s 已启动行为树 %s"),
		*GetName(), *GetNameSafe(BehaviorTreeAsset));
}

AArcaneVanguardEnemy* AArcaneVanguardEnemyAIController::GetControlledEnemy() const
{
	return Cast<AArcaneVanguardEnemy>(GetPawn());
}
