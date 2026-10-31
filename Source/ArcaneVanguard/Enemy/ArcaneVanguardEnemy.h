// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ArcaneVanguardEnemy.generated.h"

class UAbilitySystemComponent;
class UArcaneVanguardAttributeSet;
class UAnimMontage;
struct FOnAttributeChangeData;

/**
 * 敌人基类（M3）。
 *
 * 职责边界（重构后，2026-10-27）：
 *   - 本类只管「身体」：挨打、攻击判定、播蒙太奇、死亡表现；
 *   - 「什么时候追、什么时候打」是 AArcaneVanguardEnemyAIController 的事。
 *   - 本类只提供 AI 需要的身体参数（攻击蒙太奇、判定半径/距离/伤害）和执行接口
 *     （BeginAttack / PerformAttackHitCheck）。
 *
 * 和玩家的差异（重要）：
 *   - 玩家必定被 Controller 接管，所以 ActorInfo 初始化放 PossessedBy；
 *   - 敌人可能是个**没有任何 Controller 的静态靶子**（D2 就要这种），
 *     PossessedBy 根本不会触发，所以初始化放 BeginPlay，被 AI 接管后再补一次。
 *
 * 死亡表现：切 ragdoll。
 *   石头人这类重甲怪，物理倒地比播死亡动画更有说服力，而且不需要额外动画资产
 *   （SpearAnimationsV2 里没有死亡动画）。
 */
UCLASS()
class ARCANEVANGUARD_API AArcaneVanguardEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArcaneVanguardEnemy();

	// ---------------------------------------------------------------------
	// IAbilitySystemInterface
	// ---------------------------------------------------------------------

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Ability")
	UArcaneVanguardAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const { return bIsDead; }

	/**
	 * 死亡处理：切 ragdoll → 停移动 → 延迟销毁。幂等，重复调用无副作用。
	 * 由 Health 降到 0 自动触发；也可以从蓝图/调试手动调。
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void HandleDeath();

	// ---------------------------------------------------------------------
	// 受击硬直（AV-050）
	//
	// 触发链：ASC 的 Health 变化委托 -> OnHealthChanged（掉血且没死）
	//         -> HandleHitReact（打断动作 + 播受击蒙太奇 + 进入硬直）
	//         -> 定时器 -> EndStagger（恢复 AI）
	//
	// 「打断」包含三层，缺一层都会出现"被打断了但还在走/还在挥拳"：
	//   1. 蒙太奇：Montage_Stop，停掉攻击动画
	//   2. 移动：StopMovementImmediately，清残留 Velocity
	//   3. AI：StopMovement + Brain->PauseLogic，让 BT 不再下达新的追击/攻击
	// ---------------------------------------------------------------------

	/**
	 * 受击蒙太奇池。每次受击**随机**播一个 —— 同一个动作连播会显得机械。
	 * 留空则不播动画，只做硬直（仍然会打断当前动作）。
	 *
	 * 现成素材：Content/SpearAnimationsV2/ 里有 8 个受击动画，重定向后加进这个数组。
	 * [!] 蒙太奇里要有 DefaultSlot，且 ABP_Enemy_Stone 里有同名 Slot 节点，否则播了没反应。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReactMontages;

	/**
	 * 硬直持续时间（秒）。填 0 表示跟随受击蒙太奇的实际长度。
	 * 硬直期间敌人不移动、不出招；结束后 AI 自动恢复。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float StaggerDuration;

	/** 受击时是否打断正在进行的攻击 / 移动。关掉则只在空闲时播受击动画 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact")
	bool bInterruptOnHit;

	/** 是否正在硬直中（蓝图 / AnimBP 可读，用来切受击姿势或禁用 UI） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|HitReact")
	bool bIsStaggered;

	/** 是否处于受击硬直 */
	UFUNCTION(BlueprintPure, Category = "Combat|HitReact")
	bool IsStaggered() const { return bIsStaggered; }

	/**
	 * 受击反馈：打断当前动作 -> 随机播一个受击蒙太奇 -> 进入硬直。
	 * 由 OnHealthChanged 在「掉血且没死」时自动调用；也可以在蓝图里手动调（比如格挡弹反）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|HitReact")
	void HandleHitReact();

	// ---------------------------------------------------------------------
	// 供 AIController 驱动（执行层）
	// ---------------------------------------------------------------------

	/**
	 * 敌人版攻击判定：球扫 + 对命中目标施加伤害（走 ApplyDamage 唯一入口）。
	 * 由 UAnimNotifyState_AVAttackWindow::NotifyBegin 调用 —— 所以必须是 public。
	 * @return 命中并成功结算的目标数
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	int32 PerformAttackHitCheck();

	/**
	 * 开始一次攻击：清空本刀的命中记录 + 播放攻击蒙太奇。
	 * 由 BT 的 UArcaneVanguardBTTask_Attack 调用（它只决定"什么时候打"）。
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void BeginAttack();

	// ---------------------------------------------------------------------
	// 战斗参数（身体侧）
	//
	// 决策参数（追击距离、攻击距离、出招冷却）**不在这里** —— 它们属于 AI 决策，
	// 放在 BT 资产的 Service / Decorator 里配置。这样调数值不用重编译，
	// 也不用改 C++；同一个 Pawn 换一棵 BT 就是另一种行为。
	// ---------------------------------------------------------------------

	/** 攻击蒙太奇（里面要放 AV Attack Window Notify 来触发判定） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 攻击判定球扫的半径（cm） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy")
	float AttackSweepRadius;

	/** 攻击判定球扫从胶囊中心向前的距离（cm） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy")
	float AttackSweepRange;

	/** 攻击伤害（走 ApplyDamage 唯一入口，内部会取负） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy")
	float AttackDamage;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	/** ASC 里 Health 变化时触发；降到 0 就死 */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UArcaneVanguardAttributeSet> AttributeSet;

	/**
	 * 生命值上限覆盖。<= 0 表示沿用 AttributeSet 的默认值（100）。
	 *
	 * 为什么需要它：玩家和敌人共用同一个 UArcaneVanguardAttributeSet，
	 * 初值写死在 AttributeSet 构造函数里（100/100）。石头人这类大体型怪
	 * 应该比玩家厚，但改 AttributeSet 会同时影响玩家 —— 所以给敌人单独开一个入口。
	 *
	 * 用法：BP_AV_Enemy_Golem → Combat → Max Health Override 填 300。
	 * 注意这不是"加成"而是"直接设成这个值"，同时会把当前 Health 补满。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float MaxHealthOverride = 0.0f;

	/** 死亡后延迟销毁的秒数；<= 0 表示不销毁（调试时方便观察尸体） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float DestroyDelayAfterDeath = 6.0f;

	/** 死亡后关掉胶囊体碰撞，避免尸体挡路（网格自己的 ragdoll 碰撞不受影响） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	bool bDisableCapsuleCollisionOnDeath = true;

private:
	bool bIsDead = false;

	/** 硬直计时器。再次受击时会重置，避免硬直被拆成几段 */
	FTimerHandle StaggerTimerHandle;

	/** 结束硬直：解除 AI 暂停，恢复 BT 逻辑 */
	void EndStagger();

	/**
	 * 通知 AI 进入硬直。
	 *
	 * 首选路径：写 Blackboard 的 IsStaggered 键 —— BT 里配了
	 * Decorator(IsStaggered==true) + Observer Aborts=Lower Priority 时，
	 * 正在跑的战斗分支（MoveTo / Attack）会被自动 Abort，不需要手动停 AI。
	 *
	 * 兜底路径：Blackboard 里还没有 IsStaggered 键时（BT 资产未更新），
	 * 退回 StopMovement + PauseLogic，保证受击期间 AI 至少不会继续追人。
	 */
	void NotifyAIStaggerStart();

	/** 通知 AI 硬直结束。与 NotifyAIStaggerStart 成对，按进入时走的是哪条路径决定怎么收尾 */
	void NotifyAIStaggerEnd();

	/**
	 * 兜底模式下是否由本类暂停了 AI。
	 * 只有进入时走了 PauseLogic，结束时才需要 ResumeLogic ——
	 * 否则会对没暂停过的 Brain 反复 Resume，或漏掉一次 Resume 让 AI 永久瘫痪。
	 */
	bool bStaggerPausedAI;

	/** 本次攻击已命中的目标，防止球扫一刀打多次。每次 BeginAttack 时清空 */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;
};
