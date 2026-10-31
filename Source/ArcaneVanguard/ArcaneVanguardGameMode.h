// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArcaneVanguardGameMode.generated.h"

UCLASS(minimalapi)
class AArcaneVanguardGameMode : public AGameModeBase
{
	GENERATED_BODY()

	// 项目 GameMode（纯逻辑，不含任何资产路径）。
	// Pawn 与 Controller 都在 BP_AV_GameMode 的 Class Defaults 里配置：
	//   DefaultPawnClass      = BP_ThirdPersonCharacter
	//   PlayerControllerClass = BP_AV_PlayerController
	// 2026-10-27：移除了 FClassFinder 硬编码路径 —— 资产一移动就会静默退回默认 Pawn。

public:
	AArcaneVanguardGameMode();

	/** 启动时打印实际生效的 Pawn / Controller，并在退回引擎默认值时报警 */
	virtual void StartPlay() override;
};
