// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AbilitySystemInterface.h"
#include "ArcaneVanguardCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UGameplayAbility;
class UAnimMontage;
class UAbilitySystemComponent;
class UArcaneVanguardAttributeSet;
class UGameplayEffect;
class AArcaneVanguardWeapon;
struct FInputActionValue;
struct FOnAttributeChangeData;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AArcaneVanguardCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext 已迁至 AArcaneVanguardPlayerController（ADR-010） */

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Q 技能输入（M4）。在 BP_ThirdPersonCharacter 的 Input 分类里指定 IA_SkillQ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SkillQAction;

	/**
	 * 普攻输入（AV-052）。在 BP_ThirdPersonCharacter 的 Input 分类里指定 IA_Attack。
	 *
	 * [!] 接入后要把蓝图里原来"攻击键 → OnAttackPressed"的节点删掉，
	 *     否则一次按键会同时走 C++ 绑定和蓝图调用，变成一次按出两段连击。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AttackAction;

protected:
	// ---------------------------------------------------------------------
	// 能力系统（GAS，M2 起）
	// 归属决策见 ADR-004：ASC 挂在 Character 上（单人项目的简化选择）。
	// 将来做联机时迁到 PlayerState，届时 InitAbilityActorInfo 的第一个参数改成 PlayerState。
	// ---------------------------------------------------------------------

	/** 能力系统组件。在构造函数里创建，PossessedBy 里初始化 ActorInfo */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * 开局授予的技能列表（M4）。
	 * 在 BP_ThirdPersonCharacter 的 Ability 分类里填 GA_ArcaneBlast 等。
	 * 用 TSubclassOf 而不是硬编码类，是为了让"角色拥有哪些技能"变成纯配置。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 属性集（Health / MaxHealth）。构造函数里创建并注册到 ASC */
	UPROPERTY()
	TObjectPtr<UArcaneVanguardAttributeSet> AttributeSet;

	/**
	 * 启动时施加的 GameplayEffect（可选）。
	 * 用来按角色类型覆盖属性初值、加初始 Buff 等。
	 * 留空也能跑：属性集构造函数里已给了默认 100 血。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

	/** 在 InitAbilityActorInfo 之后施加 StartupEffects */
	void ApplyStartupEffects();

public:
	AArcaneVanguardCharacter();

	// ---------------------------------------------------------------------
	// IAbilitySystemInterface
	// ---------------------------------------------------------------------

	/** GAS 通过这个接口从任意 Actor 拿到 ASC；返回空表示该 Actor 不参与 GAS */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 给 UI / 调试用的属性集访问器 */
	UFUNCTION(BlueprintPure, Category = "Ability")
	UArcaneVanguardAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// ---------------------------------------------------------------------
	// 武器
	// ---------------------------------------------------------------------

	/**
	 * 要生成的武器类。必须是 AArcaneVanguardWeapon 的子类（通常是它的蓝图子类）。
	 *
	 * 在 BP_ThirdPersonCharacter 的 Weapon 分类里指定。
	 * 类型收窄到 AArcaneVanguardWeapon（原来是 TSubclassOf<AActor>）：
	 * 既能防选错类，也让蓝图里能直接访问武器的全部属性。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (AllowAbstract = "false"))
	TSubclassOf<AArcaneVanguardWeapon> WeaponActorClass;

	/** 武器附着的骨骼插槽名。SKE_Wizard 上已存在 "StickSocket" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponAttachSocket;

	/** 运行时生成的武器实例（只读，供调试与后续逻辑读取） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AActor> SpawnedWeapon;

	/**
	 * 生成武器并附着到 WeaponAttachSocket。
	 * 生成点取插槽的世界变换，因此附着后武器相对插槽的变换为 Identity。
	 *
	 * [!] 不要用「在蓝图里改 MeshComponent 的相对变换」来校准武器位置 ——
	 *    运行时会被这次 Attach(KeepWorldTransform) 冲掉，改了等于没改。
	 *    校准只有两条路：改骨架上的插槽，或走 AArcaneVanguardWeapon::CombatGripOffset。
	 *
	 * @return 生成的武器 Actor；失败返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AActor* SpawnAndAttachWeapon();

	/** 从插槽上卸下并销毁当前武器 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DetachAndDestroyWeapon();

	// ---------------------------------------------------------------------
	// 武器显隐（AV-051）
	// ---------------------------------------------------------------------

	/**
	 * 显示 / 隐藏手上的武器。
	 *
	 * 巫师自带的技能动画都不含法杖，施法时必须藏起来，
	 * 否则会出现"施法动作里插着一根棍子"。
	 * 典型用法：技能 ActivateAbility 里 SetWeaponHidden(true)，
	 *          EndAbility 里 SetWeaponHidden(false)（EndAbility 在被打断时也会调，所以不会漏恢复）。
	 *
	 * 只在武器还 Attach 在手上时生效 —— 已脱手掉落的武器不受影响。
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponHidden(bool bInHidden);

	/** 武器当前是否被隐藏 */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWeaponHidden() const { return bWeaponHidden; }

	/** 武器是否处于隐藏状态（施法期间） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	bool bWeaponHidden;

	// ---------------------------------------------------------------------
	// 战斗状态（M1 临时实现）
	// ---------------------------------------------------------------------

	/**
	 * 是否处于战斗状态。
	 * AnimBP 读取此值驱动上半身分层混合（Layered Blend per Bone 的 alpha）。
	 * 当前由调试按键切换；M3 接入战斗系统后改由 GameplayTag / CombatComponent 驱动。
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
	bool bInCombat;

	/** 设置战斗状态（后续由战斗系统调用） */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInCombat(bool bNewInCombat);

	/** 切换战斗状态。临时调试入口，M3 接入战斗系统后删除 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ToggleCombatState();

	/** 查询当前是否处于战斗状态 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInCombat() const { return bInCombat; }

	// ---------------------------------------------------------------------
	// 普通攻击三段连击（M1）
	// ---------------------------------------------------------------------

	/**
	 * 三段普攻蒙太奇。**不硬编码路径，由蓝图手动指定**：
	 *   - 统一配置：BP_ThirdPersonCharacter → Class Defaults → Combat|Combo
	 *   - 单实例覆盖：关卡里选中角色 → Details → Combat|Combo
	 * 指向的蒙太奇里 Section 名必须为 Combat_01 / Combat_02 / Combat_03（与 ComboSectionCount 对应）。
	 * 留空时 OnAttackPressed 会打警告并直接返回，不会崩。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Combo")
	TObjectPtr<UAnimMontage> ComboMontage;

	/** 连击段数，必须与蒙太奇里的 Section 数量一致 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Combo",
		meta = (ClampMin = "1", ClampMax = "8"))
	int32 ComboSectionCount;

	/** 蒙太奇播放速率。1.0 = 原始速度，调大 = 更快的连击 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Combo",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float ComboPlayRate;

	/** 攻击是否要求处于战斗状态。调试时可关掉，方便不进战斗直接测 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Combo")
	bool bRequireCombatForAttack;

	/** 攻击期间的移动速度倍率。1 = 不减速，0.3 = 降到 30%，0 = 完全锁住 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Combo",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackMoveSpeedScale;

	/** 当前连击段：0 = 未在攻击，1..ComboSectionCount = 正在播第几段 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Combo")
	int32 ComboIndex;

	/** 连击窗口是否开启（由 UAnimNotifyState_AVComboWindow 驱动） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Combo")
	bool bComboWindowOpen;

	/** 攻击判定窗口是否开启（由 UAnimNotifyState_AVAttackWindow 驱动，实际判定见 PerformAttackHitCheck） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Combo")
	bool bAttackWindowOpen;

	/** 窗口外按下的攻击输入，暂存到窗口开启时消费 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Combo")
	bool bAttackBuffered;

	/**
	 * 攻击输入入口。按一次推进一段：
	 *   未在攻击 → 起手第一段
	 *   连击窗口内 → 立刻接下一段
	 *   连击中但不在窗口 → 缓存输入，等窗口开启再消费
	 * 蓝图里把攻击键直接接到这个函数即可。
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
	void OnAttackPressed();

	/** 由 UAnimNotifyState_AVComboWindow 调用，不要在蓝图里手动开 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
	void SetComboWindowOpen(bool bOpen);

	/** 由 UAnimNotifyState_AVAttackWindow 调用。开窗口时清空本段的命中记录 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
	void SetAttackWindowOpen(bool bOpen);

	/** 是否正在播放连击 */
	UFUNCTION(BlueprintPure, Category = "Combat|Combo")
	bool IsAttacking() const { return ComboIndex > 0; }

	/** 连击窗口是否开启 */
	UFUNCTION(BlueprintPure, Category = "Combat|Combo")
	bool IsComboWindowOpen() const { return bComboWindowOpen; }

	/** 当前连击段（0 = 未在攻击） */
	UFUNCTION(BlueprintPure, Category = "Combat|Combo")
	int32 GetComboIndex() const { return ComboIndex; }

	/** 中断并复位连击（同时停掉蒙太奇、恢复移动速度） */
	UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
	void ResetCombo();

	// ---------------------------------------------------------------------
	// 攻击判定（M3）
	// 由 UAnimNotifyState_AVAttackWindow 的 NotifyBegin 驱动，每个判定窗口做一次。
	// 不做"窗口内每帧检测"：三段连击的判定窗口只有 0.2s，一次球扫足够，
	// 每帧扫反而会让同一刀在不同帧命中不同目标，手感不可控。
	// ---------------------------------------------------------------------

	/**
	 * 判定球从角色胶囊体中心向前扫的距离（cm）。
	 * 起扫点用 GetActorLocation()（胶囊体中心）而不是脚底 ——
	 * 从脚底起扫时球心在地面，够不到站着的敌人躯干。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitCheck", meta = (ClampMin = "0.0"))
	float AttackRange;

	/**
	 * 判定球半径（cm）。
	 * 必须 ≥ 敌人的胶囊体半径，否则贴身打不到（球从中心扫过，横向覆盖不够）。
	 * 默认 60 是按"比玩家大的石头人"给的，小体型敌人可以降到 40。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitCheck", meta = (ClampMin = "1.0"))
	float AttackRadius;

	/** 每段攻击的基础伤害。Notify 上的 DamageOverride > 0 时以 Notify 为准 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitCheck", meta = (ClampMin = "0.0"))
	float AttackDamage;

	/**
	 * 伤害 GE 类。留空 = 用原生 UGE_ArcaneDamage（数值走 SetByCaller）。
	 * 想做元素类型、暴击、伤害曲线时，建 BP 子类填到这里。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitCheck")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 做一次球形 Sweep，对命中的、有 ASC 的目标施加伤害。
	 * 每个判定窗口调一次（NotifyBegin）。同一段内同一目标只结算一次。
	 *
	 * @param DamageOverride <= 0 时用 AttackDamage；> 0 时覆盖（方便给不同段配不同伤害）
	 * @return 实际结算伤害的目标数
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|HitCheck")
	int32 PerformAttackHitCheck(float DamageOverride = -1.f);

	// ---------------------------------------------------------------------
	// 死亡与重生（AV-047）
	//
	// 触发链：ASC 的 Health 变化委托 -> OnHealthChanged -> HandleDeath
	//         -> 定时器 -> Respawn
	//
	// 为什么用属性委托而不是 Tick 轮询血量：
	//   轮询要么漏掉"一帧内从 20 掉到 -10"的致死，要么多写一套状态；
	//   委托是 GAS 的标准出口，GE 结算完立刻回调，时序确定。
	// ---------------------------------------------------------------------

	/**
	 * 是否用布娃娃（ragdoll）表现死亡。默认开启。
	 *
	 * ragdoll 需要骨骼网格上配好 PhysicsAsset（SKE_Wizard 已配 PHY_Wizard）。
	 * 关掉则退回"播 DeathMontage"的老方案，两种二选一。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death")
	bool bUseRagdollOnDeath;

	/**
	 * 死亡时播放的蒙太奇。**仅在 bUseRagdollOnDeath = false 时生效**。
	 * 现成素材：AM_Wizard_Death（由 Anim_Wizard_Death_Forward 生成）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death",
		meta = (EditCondition = "!bUseRagdollOnDeath"))
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 死亡后多久重生（秒）。填 0 = 不重生，停在死亡状态（调试用） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Death", meta = (ClampMin = "0.0"))
	float RespawnDelay;

	/** 是否已死亡。蓝图 / AnimBP 可读，用来切死亡姿势或隐藏 HUD */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Death")
	bool bIsDead;

	/** 是否处于死亡状态 */
	UFUNCTION(BlueprintPure, Category = "Combat|Death")
	bool IsDead() const { return bIsDead; }

	/**
	 * 死亡处理：取消技能与连击 -> 禁用输入 -> 停移动 -> 播死亡动画 -> 启动重生计时。
	 * 幂等：重复调用只生效一次（bIsDead 保护）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void HandleDeath();

	/**
	 * 重生：回满血 -> 恢复 ragdoll -> 传送回出生点 -> 重新装配武器 -> 恢复移动与输入。
	 * 出生点用 BeginPlay 时记录的初始位置，不依赖关卡里必须有 PlayerStart。
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void Respawn();

	/**
	 * 让手上的武器脱手掉落（脱 Attach + 开物理自由落体）。
	 * 由 HandleDeath 调用；掉落的那把武器在重生时销毁并重新生成。
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Death")
	void DropWeaponOnDeath();

	// ---------------------------------------------------------------------
	// 攻击判定可视化（调试）
	//
	// SweepMultiByChannel + MakeSphere 的几何本质：
	//   半径 R 的球从 Start 沿直线移到 End，扫过的体积 = 圆柱 + 两端半球，
	//   也就是一个「胶囊体」—— 和 CapsuleComponent 同形状，
	//   只是轴向由 Start→End 决定、长度 = AttackRange。
	//
	// 两种开关方式：
	//   1) 蓝图里勾 bDrawAttackHitCheck（BP_ThirdPersonCharacter → Combat|Debug）
	//   2) PIE 里按 ~ 打开控制台，输入 av.DrawAttackHitCheck 1 / 0
	//      第二种不用改蓝图、不用重编译，运行中随时切
	// ---------------------------------------------------------------------

	/** 是否绘制攻击判定范围（球扫管道 + 命中点 + 参数摘要） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
	bool bDrawAttackHitCheck;

	/** 每次判定后绘制持续多少秒。默认 1.0，略大于单段时长（0.667），能连续看到 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug",
		meta = (ClampMin = "0.1", ClampMax = "10.0", EditCondition = "bDrawAttackHitCheck"))
	float AttackDebugDrawDuration;

protected:

	// ---------------------------------------------------------------------
	// 死亡与重生（内部状态）
	// ---------------------------------------------------------------------

	/** Health 属性变化回调。降到 0 时触发 HandleDeath */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** 重生定时器句柄。重生前再次死亡会先清掉它，避免重复重生 */
	FTimerHandle RespawnTimerHandle;

	/** 出生点。BeginPlay 时记录，重生时传送回去 */
	FVector InitialSpawnLocation;
	FRotator InitialSpawnRotation;

	/**
	 * Mesh 的初始相对变换。
	 * ragdoll 会把 Mesh 的相对变换改成一团（物理接管后不再跟随胶囊体），
	 * 重生时必须手动还原，否则角色会以"躺在地上的姿势"站起来。
	 * 用缓存而不是硬编码 (0,0,-90)/(0,-90,0)，这样 BP 里调过 Mesh 偏移也不会被冲掉。
	 */
	FVector InitialMeshRelativeLocation;
	FRotator InitialMeshRelativeRotation;

	/**
	 * 本次判定窗口内已经命中过的目标。
	 * 用弱引用：不持有目标，目标被销毁后自然失效，不会拖住 GC。
	 * 每次 SetAttackWindowOpen(true) 时清空。
	 */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	/**
	 * 只清状态、不碰蒙太奇。
	 * 蒙太奇结束回调里必须用这个：那时蒙太奇正在被引擎停用，
	 * 再调 Montage_Stop 会重入。
	 */
	void ClearComboState();

	/** 播放第 ZeroBasedIndex 段（0-based）。已在播则用 JumpToSection，不重启蒙太奇 */
	void PlayComboSection(int32 ZeroBasedIndex);

	/**
	 * 把当前段的 Next Section 挂到下一段。
	 *
	 * 关键：`Montage_SetNextSection` 只在**段尾**生效，所以"按下时就挂"和
	 * "窗口结束时才挂"的过渡时机完全一样 —— 后摇照样播完。
	 * 不能等窗口 NotifyEnd 再挂：窗口终点和段尾撞在同一帧时，
	 * 引擎会直接停掉蒙太奇，NotifyEnd 不一定会被调用。
	 */
	void ArmNextSection();

	/** 第 ZeroBasedIndex 段对应的 Section 名：Combat_01 / Combat_02 / ... */
	static FName GetComboSectionName(int32 ZeroBasedIndex);

	/** 蒙太奇播放结束（含被打断）时复位连击 */
	UFUNCTION()
	void HandleComboMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 攻击开始前的 MaxWalkSpeed，用于结束后恢复 */
	float CachedMaxWalkSpeed;

	/** 是否已经为攻击降过速（避免重复缓存） */
	bool bMovementSlowedByAttack;

	/**
	 * 已经"武装"好的下一段（0-based），INDEX_NONE 表示没有。
	 *
	 * 连击窗口只负责接收输入，真正切段发生在**窗口结束**：那时把当前段的
	 * Next Section 链到下一段（`Montage_SetNextSection` 改的是当前播放实例的
	 * `FAnimMontageInstance::NextSections`，不写回资产），引擎就会在段尾自然过渡，
	 * 后摇完整播完、衔接用引擎标准的 Section 混合。
	 *
	 * 这个值只用于兜底：万一链接没生效、蒙太奇直接结束，就在结束回调里接着播下一段。
	 */
	int32 ArmedNextSectionIndex;

	/**
	 * 每帧把 ComboIndex 同步成蒙太奇实际在播的段。
	 * 索引永远以画面为准，就不会出现"状态停在第一段、画面已经到第三段"的失步。
	 */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Q 技能：按 Ability.Skill.Q 这个 Tag 激活（技能蓝图里要配同一个 Tag） */
	void Input_SkillQ();
			

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay();

	// ---------------------------------------------------------------------
	// GAS 预留点（M2 实装，当前只留空重写，不引入 GameplayAbilities 模块）
	// ---------------------------------------------------------------------

	/** 控制器接管时的初始化时机。M2 将在此调用 InitAbilityActorInfo（ADR-004） */
	virtual void PossessedBy(AController* NewController) override;

	/** PlayerState 复制到客户端时的初始化时机。M2 在此补一次 InitAbilityActorInfo（ADR-004） */
	virtual void OnRep_PlayerState() override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

