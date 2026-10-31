// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArcaneVanguard : ModuleRules
{
	public ArcaneVanguard(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// ---- GAS（M2 起）----
		// GameplayAbilities 是引擎插件，且 .uplugin 里 EnabledByDefault = false，
		// 必须同时在 ArcaneVanguard.uproject 的 Plugins 里显式启用，否则这里加模块会编译失败。
		// GameplayTags / GameplayTasks 是 GameplayAbilities 的硬依赖，三个必须一起加。
		PublicDependencyModuleNames.AddRange(new string[] { "GameplayAbilities", "GameplayTags", "GameplayTasks" });

		// ---- AI（D3 起）----
		// AArcaneVanguardEnemyAIController 继承 AAIController，后者在 AIModule 里。
		// 不加这个依赖会报 "fatal error C1083: 无法打开包括文件: AIController.h"。
		PublicDependencyModuleNames.Add("AIModule");

		// 让子目录里的头文件可以用 "Player/XXX.h"、"Weapon/XXX.h" 这种模块根相对路径引用。
		// 不加这一行时，跨子目录引用只能写同目录相对路径或 ../，容易失控。
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
