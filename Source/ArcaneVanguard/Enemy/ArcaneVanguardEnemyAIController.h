// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ArcaneVanguardEnemyAIController.generated.h"

class AArcaneVanguardEnemy;
class UBehaviorTree;

/**
 * 敌人 AI 控制器（D3，行为树版）。
 *
 * 职责：**只负责跑行为树**。所有决策（追谁、什么时候打、距离阈值、冷却）
 * 都在 BT 资产里，通过 Service / Decorator / Task 表达，不写在 C++ 里。
 *
 * 架构：
 *   AArcaneVanguardEnemyAIController
 *     ├─ BehaviorTreeAsset ──> BT_Enemy（在 BP_AV_EnemyAIController 里指定）
 *     │                          ├─ Service: UBTService_AVUpdateTarget（Enemy/AI/）
 *     │                          │     写 Blackboard: TargetActor / EdgeDistanceKey
 *     │                          ├─ BTTask_MoveTo（引擎内置，走 NavMesh）
 *     │                          └─ Task: UArcaneVanguardBTTask_Attack
 *     │                                转向目标 + 让 Pawn 播攻击蒙太奇
 *     └─ Pawn = AArcaneVanguardEnemy（只负责身体：判定、蒙太奇、死亡）
 *
 * 为什么决策参数不在 C++ 里：
 *   BT 的 Decorator / Service 可以在编辑器里直接调，不用重编译；
 *   同一个 Pawn 换一棵 BT 就是另一种敌人行为（远程 / 近战 / 飞行）。
 */
UCLASS()
class ARCANEVANGUARD_API AArcaneVanguardEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AArcaneVanguardEnemyAIController();

	virtual void OnPossess(APawn* InPawn) override;

	/** GetPawn 的强类型版本；不是敌人类就返回 null */
	UFUNCTION(BlueprintPure, Category = "AI")
	AArcaneVanguardEnemy* GetControlledEnemy() const;

	// Blackboard Key 名（必须与 BB_Enemy 资产里的大小写完全一致）
	static const FName Key_TargetActor;
	static const FName Key_DistanceToTarget;

protected:
	/**
	 * 行为树资产。在 BP_AV_EnemyAIController → AI → Behavior Tree Asset 里指定。
	 * 不指定的话敌人完全不会行动（OnPossess 会打 Error 日志）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;
};
