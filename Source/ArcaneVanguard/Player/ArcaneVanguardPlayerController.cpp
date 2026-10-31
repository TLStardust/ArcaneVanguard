// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcaneVanguardPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAVPlayerController, Log, All);

AArcaneVanguardPlayerController::AArcaneVanguardPlayerController()
	: MappingContextPriority(0)
	, bMappingContextAdded(false)
{
}

void AArcaneVanguardPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 没有配置 IMC 时不报错，只提示——可能是有意为之（例如纯 UI 场景）
	if (!DefaultMappingContext)
	{
		UE_LOG(LogAVPlayerController, Warning,
			TEXT("[Input] 未指定 DefaultMappingContext，玩家将无法操作。请在 BP_AV_PlayerController 里指定 IMC。"));
		return;
	}

	// 防御：理论上本地玩家此时已就绪，但时序异常时不要崩
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogAVPlayerController, Warning,
			TEXT("[Input] BeginPlay 时拿不到 LocalPlayer，跳过 MappingContext 注册。"));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		// 幂等保护：即使重复 BeginPlay 也只注册一次
		if (!bMappingContextAdded)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
			bMappingContextAdded = true;

			UE_LOG(LogAVPlayerController, Log,
				TEXT("[Input] 已注册 MappingContext '%s'（优先级 %d）"),
				*GetNameSafe(DefaultMappingContext), MappingContextPriority);
		}
	}
}

void AArcaneVanguardPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 与 BeginPlay 成对：Controller 销毁时把 MappingContext 摘掉，避免 Subsystem 上残留
	if (bMappingContextAdded && DefaultMappingContext)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);

				UE_LOG(LogAVPlayerController, Log,
					TEXT("[Input] 已移除 MappingContext '%s'"), *GetNameSafe(DefaultMappingContext));
			}
		}
	}

	bMappingContextAdded = false;

	Super::EndPlay(EndPlayReason);
}
