// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ArcaneVanguardGameplayAbility.generated.h"

class AArcaneVanguardCharacter;
class UAnimMontage;
class UGameplayEffect;

/**
 * 项目技能基类（D4 / M4）。
 *
 * **分层原则（大厂标准做法）**：
 *   - 本类（C++）：技能的**通用流程** —— 提交消耗、隐藏武器、施加自身效果、
 *     播蒙太奇、等动画结束、收尾恢复。所有技能共用，写一次即可。
 *   - 蓝图子类：只填**数据** —— 用哪个蒙太奇、挂哪些 GE、CD / 消耗 GE。
 *     一个节点都不用连。
 *
 * 这样加新技能不用写 C++（策划能自己配），但流程本身可复用、可测试、可维护。
 *
 * 用法：
 *   1. 内容浏览器右键 → Blueprint Class → 父类选本类 → 命名 GA_Q_Buff
 *   2. AbilityMontage 填施法动画（可留空 = 瞬间生效）
 *   3. SelfEffectClasses 填要施加给自己的 GE（比如回血 + 攻速）
 *   4. CooldownGameplayEffectClass / CostGameplayEffectClass 挂 CD 与消耗
 *   5. AbilityTags 里加 Ability.Skill.Q（和输入触发用的 tag 一致）
 */
UCLASS(Abstract)
class ARCANEVANGUARD_API UArcaneVanguardGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UArcaneVanguardGameplayAbility();

	/** Avatar 的强类型版本；不是玩家角色就返回 null */
	UFUNCTION(BlueprintPure, Category = "AV|Ability")
	AArcaneVanguardCharacter* GetAvatarCharacter() const;

	// ---------------------------------------------------------------------
	// 可配置项（蓝图子类里填）
	// ---------------------------------------------------------------------

	/**
	 * 施法蒙太奇。**留空 = 技能瞬间生效、不播动画**。
	 *
	 * 有蒙太奇时，技能会等它播完才 EndAbility ——
	 * 否则武器会提前显示出来，出现"动画还在放、法杖已经回到手上"的穿帮。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AV|Animation")
	TObjectPtr<UAnimMontage> AbilityMontage;

	/** 蒙太奇播放速率。1.0 = 原速 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AV|Animation",
		meta = (ClampMin = "0.1", ClampMax = "3.0", EditCondition = "AbilityMontage != nullptr"))
	float MontagePlayRate;

	/**
	 * 施法期间是否隐藏武器。
	 *
	 * 巫师自带的技能动画都不含法杖，不藏会出现"施法动作里插着一根棍子"。
	 * 隐藏与恢复由本类统一处理（ActivateAbility 藏、EndAbility 恢复），
	 * 子类不需要自己管 —— 而且 EndAbility 在技能被打断 / 取消时也会调，
	 * 所以不会出现"技能中断了法杖永远消失"。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AV|Animation")
	bool bHideWeaponWhileCasting;

	/**
	 * 技能激活时施加给**自己**的 GameplayEffect 列表。
	 *
	 * 例：Q 增益技能填 GE_Q_Regen（每秒回血）+ GE_Q_Haste（攻速 +30%）。
	 *
	 * [!] 这些是「持续型 Buff」，时长由 GE 自己的 Duration 决定，
	 *     **和技能的存活时间无关** —— 技能播完动画就 EndAbility，
	 *     GE 会继续按自己的 Duration 跑完。这正是 Buff 类技能想要的行为。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AV|Effects")
	TArray<TSubclassOf<UGameplayEffect>> SelfEffectClasses;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** 把 SelfEffectClasses 逐个施加到自己身上 */
	void ApplySelfEffects();

	/** 蒙太奇正常播完 → 收尾 */
	UFUNCTION()
	void HandleMontageCompleted();

	/** 蒙太奇被打断 / 被取消 → 收尾（标记为 cancelled） */
	UFUNCTION()
	void HandleMontageInterrupted();

private:
	/**
	 * 记录「武器是这次技能藏起来的」。
	 * EndAbility 只在真的藏过时才恢复 —— 避免把别的来源隐藏的武器错误显示出来。
	 */
	bool bWeaponHiddenByThisAbility;
};
