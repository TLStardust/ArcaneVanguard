// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcaneVanguardGameMode.h"
#include "GameFramework/DefaultPawn.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAVGameMode, Log, All);

AArcaneVanguardGameMode::AArcaneVanguardGameMode()
{
	// 这里刻意什么都不设。
	//
	// 为什么不用 ConstructorHelpers::FClassFinder 写死资产路径：
	//   硬编码路径一旦与资产实际位置不一致，就会【静默】退回引擎默认 Pawn
	//   （2026-10-27 真实踩过：BP_ThirdPersonCharacter 被移动到 BluePrint/Player/ 后，
	//    玩家变成默认 Mannequin —— 没有武器、没有连击、没有 AnimBP，且不报错）。
	//   改成蓝图配置后，移动资产只需要在编辑器里重指，不用改代码、不用重编译。
	//
	// 配置位置：BP_AV_GameMode → Class Defaults
	//   Default Pawn Class      = BP_ThirdPersonCharacter
	//   Player Controller Class = BP_AV_PlayerController
}

void AArcaneVanguardGameMode::StartPlay()
{
	Super::StartPlay();

	// 到 StartPlay 时 CDO 的蓝图覆盖已经生效，这里读到的是真正在用的类
	UE_LOG(LogAVGameMode, Log, TEXT("[GameMode] Pawn=%s  Controller=%s"),
		*GetNameSafe(DefaultPawnClass), *GetNameSafe(PlayerControllerClass));

	// 退回引擎默认 Pawn 基本只有一个原因：BP_AV_GameMode 没配 DefaultPawnClass
	if (!DefaultPawnClass || DefaultPawnClass == ADefaultPawn::StaticClass())
	{
		UE_LOG(LogAVGameMode, Error,
			TEXT("[GameMode] DefaultPawnClass 仍是引擎默认值 —— 玩家不是巫师，也没有武器与连击。")
			TEXT("请打开 BP_AV_GameMode -> Class Defaults -> Default Pawn Class，指定 BP_ThirdPersonCharacter。"));
	}
}
