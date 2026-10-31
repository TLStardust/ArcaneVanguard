// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArcaneVanguardPlayerController.generated.h"

class UInputMappingContext;

/**
 * 项目 PlayerController。
 *
 * 职责（见 ADR-009 / ADR-010）：
 *  - 持有并注册 Enhanced Input 的 MappingContext（生命周期与 LocalPlayer 一致）
 *  - 后续 UI 输入模式切换（M6）
 *  - 后续技能输入转发（M3/M4）
 *
 * 不负责：角色移动/镜头（在 Character）、玩法规则（后续在 Ability）。
 */
UCLASS()
class ARCANEVANGUARD_API AArcaneVanguardPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArcaneVanguardPlayerController();

	/** 默认输入映射上下文。在蓝图子类里指定（BP_AV_PlayerController） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** MappingContext 优先级，数值大的覆盖数值小的 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 MappingContextPriority;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 是否已成功注册 MappingContext，用于保证 Add/Remove 成对，避免重复注册 */
	bool bMappingContextAdded;
};
