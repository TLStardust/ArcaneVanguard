// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArcaneVanguardGameplayEffects.generated.h"

/**
 * SetByCaller 的 key 名。
 *
 * 为什么用 FName 而不是 GameplayTag：
 *   FSetByCallerFloat 同时支持 DataName(FName) 和 DataTag 两种 key。
 *   用 FName 不需要在 DefaultGameplayTags.ini 里注册 tag，零配置就能跑通，
 *   适合 M3 这种"先把链路打通"的阶段。
 *   以后技能系统要按 tag 组织时（比如 Data.Damage.Fire），再迁到 DataTag 版本。
 *
 * 用函数而不是全局常量：避免跨编译单元的静态初始化顺序问题。
 */
namespace AVGameplayEffectKeys
{
	/** 伤害量。施加方传正数，内部取负（Health 是当前值，扣血 = 负增量） */
	FORCEINLINE FName Damage() { return FName(TEXT("Data.Damage")); }
}

/**
 * 原生伤害 GameplayEffect（ADR-005：伤害唯一入口的载体）。
 *
 * 设计取舍：
 *   - **Instant**：伤害是瞬时事件，不是持续 buff。不需要 Duration / Period。
 *   - **只有一个 Modifier**：Health 加负值。MaxHealth 不动。
 *   - **magnitude 走 SetByCaller**：同一个 GE 类能被普攻（30）和技能（50）复用，
 *     不用为每个伤害数值建一个 GE 资产。
 *   - 数值钳制交给 UArcaneVanguardAttributeSet::PreAttributeChange，这里不管。
 *
 * 想改数值/加元素类型时：建 BP 子类覆盖，然后填到 Character 的 DamageEffectClass。
 */
UCLASS(meta = (DisplayName = "GE Arcane Damage"))
class ARCANEVANGUARD_API UGE_ArcaneDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ArcaneDamage();
};

/**
 * Q 增益之一：持续回血（Heal over Time）。
 *
 * 5 秒内每秒回 10 点血。
 *   - HasDuration + DurationMagnitude=5：到时自动移除，不需要手动清理
 *   - Period=1：每 1 秒结算一次（不是每帧）
 *   - Health +10 Additive：每次跳回 10 点
 *
 * [!] Period 是 GE 级的 —— 它会让**所有** Modifier 都按周期结算。
 *     所以「回血」和「攻速加成」必须拆成两个 GE，
 *     否则攻速会每秒 +0.3 累加（1.0 → 1.3 → 1.6 → 1.9 …）。
 */
UCLASS(meta = (DisplayName = "GE Q Regen"))
class ARCANEVANGUARD_API UGE_Q_Regen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Q_Regen();
};

/**
 * Q 增益之二：攻速加成。
 *
 * 5 秒内 AttackSpeedMultiplier +0.3（1.0 → 1.3，即快 30%）。
 *   - 无 Period：一次性加成，持续整个 Duration
 *   - 到期自动移除时，Additive 的 +0.3 会被减回去，不需要手动恢复
 */
UCLASS(meta = (DisplayName = "GE Q Haste"))
class ARCANEVANGUARD_API UGE_Q_Haste : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Q_Haste();
};
