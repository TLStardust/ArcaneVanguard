// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcaneVanguardCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Weapon/ArcaneVanguardWeapon.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "NativeGameplayTags.h"

/**
 * 技能 Tag（M4）。
 * 用 Native Gameplay Tag 而不是 DefaultGameplayTags.ini：
 * C++ 里直接定义，不用改 ini，也不会因为 ini 漏配导致 TryActivateAbilitiesByTag 静默失效。
 * 技能蓝图（GA_ArcaneBlast）的 AbilityTags 里必须配同一个 Tag。
 */
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Q, "Ability.Skill.Q");
#include "GameplayEffect.h"
#include "AbilitySystem/ArcaneVanguardAttributeSet.h"
#include "AbilitySystem/ArcaneVanguardCombatStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

/**
 * 攻击判定范围的可视化开关（控制台）。
 *
 * 用法：PIE 运行时按 ` 打开控制台，输入
 *   av.DrawAttackHitCheck 1    开启
 *   av.DrawAttackHitCheck 0    关闭
 * 不需要改蓝图、不需要重编译，运行中随时切换。
 *
 * 与蓝图属性 bDrawAttackHitCheck 是「或」的关系：任一为真就画。
 * ECVF_Cheat 表示只在非 Shipping 构建里可用。
 */
static TAutoConsoleVariable<int32> CVarAVDrawAttackHitCheck(
	TEXT("av.DrawAttackHitCheck"),
	0,
	TEXT("绘制攻击判定的球扫范围。\n")
	TEXT("  0 = 关闭（默认）\n")
	TEXT("  1 = 开启"),
	ECVF_Cheat);

//////////////////////////////////////////////////////////////////////////
// AArcaneVanguardCharacter

AArcaneVanguardCharacter::AArcaneVanguardCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	// 武器默认挂点：SKE_Wizard 骨架上的插槽
	WeaponAttachSocket = TEXT("StickSocket");

	// 战斗状态默认关闭
	bInCombat = false;

	// ---- 普攻连击（M1）----
	ComboSectionCount = 3;
	ComboPlayRate = 1.0f;
	bRequireCombatForAttack = true;
	// 1.0 = 攻击期间不减速（当前默认）。想让出招更"拖步"、更有重量感，可降到 0.3 左右
	AttackMoveSpeedScale = 1.0f;
	ComboIndex = 0;
	bComboWindowOpen = false;
	bAttackWindowOpen = false;
	bAttackBuffered = false;
	CachedMaxWalkSpeed = 0.0f;
	bMovementSlowedByAttack = false;
	ArmedNextSectionIndex = INDEX_NONE;

	// ComboMontage 不在这里硬编码路径：由蓝图（BP_ThirdPersonCharacter 的 Class Defaults）
	// 或关卡实例上的 Combat|Combo 分类手动指定。未指定时 OnAttackPressed 会打警告并直接返回。

	// ---- 攻击判定（M3）----
	// 判定球半径默认 60：按"比玩家大的石头人"给的，小体型敌人可降到 40。
	// 半径太小会出现"贴身打不到"——球心从敌人胶囊体边缘掠过但没覆盖到。
	AttackRange = 200.0f;
	AttackRadius = 60.0f;
	AttackDamage = 30.0f;
	// DamageEffectClass 留空 = 用原生 UGE_ArcaneDamage（在 CombatStatics 里兜底）

	// 判定范围可视化默认关闭，用 BP 勾选或控制台 av.DrawAttackHitCheck 1 开启
	bDrawAttackHitCheck = false;
	AttackDebugDrawDuration = 1.0f;

	// ---- 能力系统（M2）----
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 属性集是普通 UObject（不是 UActorComponent），ASC 不会自动发现它，
	// 必须显式注册，否则 GetSet / GE 都拿不到属性。
	// 注意：AddAttributeSetSubobject 是模板函数，参数类型是 T*，
	// TObjectPtr 到裸指针的隐式转换在模板推导时不生效，所以这里必须传 .Get()。
	AttributeSet = CreateDefaultSubobject<UArcaneVanguardAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());

	// ---- 死亡与重生（AV-047）----
	bIsDead = false;
	bUseRagdollOnDeath = true;   // 默认布娃娃，比死亡动画更有重量感
	RespawnDelay = 3.8f;         // 略长于死亡动画，也够看清 ragdoll 落地
	// DeathMontage 留空，仅在关掉 ragdoll 时使用

	// ---- 武器显隐（AV-051）----
	bWeaponHidden = false;
}

UAbilitySystemComponent* AArcaneVanguardCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AArcaneVanguardCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// 记录出生点（AV-047）。重生时传送回这里，不依赖关卡里必须有 PlayerStart。
	// 时序：GameMode 是 SpawnActor(触发本函数) -> Possess，所以此刻位置就是出生点。
	InitialSpawnLocation = GetActorLocation();
	InitialSpawnRotation = GetActorRotation();

	// 记录 Mesh 的初始相对变换。ragdoll 会把它改掉，重生时要还原。
	if (const USkeletalMeshComponent* PlayerMesh = GetMesh())
	{
		InitialMeshRelativeLocation = PlayerMesh->GetRelativeLocation();
		InitialMeshRelativeRotation = PlayerMesh->GetRelativeRotation();
	}

	// 出生即装配武器
	SpawnAndAttachWeapon();
}

void AArcaneVanguardCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// ASC 归属 Character（ADR-004）：OwnerActor 与 AvatarActor 都是自己。
	// 将来做联机迁到 PlayerState 时，改成 InitAbilityActorInfo(GetPlayerState(), this)。
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// [!] 初始 GE 必须在这里施加，不能放 BeginPlay：
		// GameMode 生成 Pawn 的顺序是 SpawnActor(触发 BeginPlay) → Possess(触发本函数)，
		// 放 BeginPlay 时 ActorInfo 还没初始化，GE 会静默失效。
		ApplyStartupEffects();

		// ---- 死亡监听（AV-047）----
		// 注册时机：必须在 InitAbilityActorInfo 之后（委托挂在 ASC 上，
		// ActorInfo 没初始化时 Owner 是空的，回调不一定会触发），
		// 放在 ApplyStartupEffects 之后是为了不让"初始 GE 设血量"触发一次回调。
		//
		// [!] 用 AddUObject 而不是 AddLambda：AddLambda 返回的句柄没人保存，
		//     重生/重复 Possess 时无法移除，会累积成多个回调。
		if (AttributeSet)
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
				AttributeSet->GetHealthAttribute())
				.AddUObject(this, &AArcaneVanguardCharacter::OnHealthChanged);

			UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] Health 变化委托已注册"));
		}

		// ---- 授予开局技能（M4）----
		// 必须在 InitAbilityActorInfo 之后：GiveAbility 需要 ASC 已有 Owner/Avatar。
		// 用 Level 1 / InputID = INDEX_NONE（由输入层按 Tag 触发，不靠 InputID 映射）。
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (!AbilityClass)
			{
				continue;
			}

			FGameplayAbilitySpec Spec(AbilityClass, /*Level=*/1, /*InputID=*/INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);

			UE_LOG(LogTemplateCharacter, Log, TEXT("[GAS] 已授予技能 %s"), *GetNameSafe(AbilityClass));
		}

		if (DefaultAbilities.Num() == 0)
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("[GAS] DefaultAbilities 为空 —— 角色没有任何技能。请在 BP_ThirdPersonCharacter 的 Ability 分类里填 GA_ArcaneBlast。"));
		}

		UE_LOG(LogTemplateCharacter, Log,
			TEXT("[GAS] ActorInfo 已初始化：ASC=%s，Owner=%s，Avatar=%s"),
			*GetNameSafe(AbilitySystemComponent),
			*GetNameSafe(AbilitySystemComponent->GetOwnerActor()),
			*GetNameSafe(AbilitySystemComponent->GetAvatarActor()));

		// 属性集是否真的注册上了：AttributeSet 是普通 UObject，ASC 不会自动发现，
		// 漏了 AddAttributeSetSubobject 时这里会打印 Error，是排查"GE 不生效"的第一现场。
		if (const UArcaneVanguardAttributeSet* RegisteredSet =
				AbilitySystemComponent->GetSet<UArcaneVanguardAttributeSet>())
		{
			UE_LOG(LogTemplateCharacter, Log,
				TEXT("[GAS] 属性集已注册：Health=%.1f / MaxHealth=%.1f"),
				RegisteredSet->GetHealth(), RegisteredSet->GetMaxHealth());
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Error,
				TEXT("[GAS] 属性集未注册！检查构造函数里的 AddAttributeSetSubobject 是否执行"));
		}
	}
}

void AArcaneVanguardCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 客户端路径。单机不会走到，保留是为了明确"ActorInfo 初始化只有两个入口"。
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AArcaneVanguardCharacter::ApplyStartupEffects()
{
	if (!AbilitySystemComponent || StartupEffects.Num() == 0)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (!EffectClass)
		{
			continue;
		}

		const FGameplayEffectSpecHandle Spec =
			AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.0f, Context);

		if (Spec.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			UE_LOG(LogTemplateCharacter, Log, TEXT("[GAS] 已施加启动 GE：%s"), *EffectClass->GetName());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// 死亡与重生（AV-047）

void AArcaneVanguardCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 值没变就直接返回。GAS 在 0 -> 0 时也会回调（死亡期间敌人继续打尸体），
	// 不拦住的话日志会刷屏，而且每帧都在做无意义的判断。
	if (FMath::IsNearlyEqual(Data.OldValue, Data.NewValue))
	{
		return;
	}

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] Health %.1f -> %.1f"),
		Data.OldValue, Data.NewValue);

	// 判 <= 0 而不是 == 0：PreAttributeChange 把 Health 钳在 [0, MaxHealth]，
	// 所以过量伤害在属性层面已经是 0，这里不用自己处理伤害溢出。
	if (Data.NewValue <= 0.0f && !bIsDead)
	{
		HandleDeath();
	}
}

void AArcaneVanguardCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] 玩家死亡（Health 归零）"));

	// 1) 先停技能与连击，再禁输入。
	//    顺序不能反：CancelAllAbilities 会让技能走 EndAbility，而 EndAbility 里
	//    常有"恢复移动速度/解除状态"的收尾逻辑，必须赶在 DisableMovement 之前跑完。
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}
	ResetCombo();

	// 2) 禁输入。用 DisableInput 而不是 SetIgnoreMoveInput：
	//    死亡后 Q、攻击、跳跃都不该响应，只屏蔽移动是不够的。
	//    注意参数是 APlayerController*（不是 AController*），所以要先 Cast。
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	// 3) 停移动。DisableMovement 之后 CharacterMovement 不再接受输入，
	//    也不会因为残留 Velocity 继续滑行。
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 4) 表现死亡：默认布娃娃，关掉 bUseRagdollOnDeath 则退回播蒙太奇。
	if (bUseRagdollOnDeath)
	{
		if (USkeletalMeshComponent* PlayerMesh = GetMesh())
		{
			// 切 Ragdoll 预设：AllBodies 默认 Block，且自身刚体会互相碰撞。
			PlayerMesh->SetCollisionProfileName(TEXT("Ragdoll"));

			// 尸体不挡自己的摄像机。
			// 引擎的 Ragdoll 预设只把 Pawn / Visibility 设成 Ignore，Camera 走默认 = Block，
			// 而 USpringArmComponent 的 ProbeChannel 正是 ECC_Camera ——
			// 不忽略的话，角色一倒地镜头就会被自己的尸体推近（敌人那边已经踩过这个坑）。
			// [!] 必须在 SetCollisionProfileName 之后设置：换预设会重置所有通道响应。
			PlayerMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

			// 全部刚体接管：身体自然瘫软，比固定姿势的死亡动画有说服力
			PlayerMesh->SetAllBodiesSimulatePhysics(true);
			PlayerMesh->SetSimulatePhysics(true);
			PlayerMesh->WakeAllRigidBodies();

			// 让 CharacterMovement 的物理混合接管 Mesh，避免它每帧把 Mesh 拉回胶囊体
			PlayerMesh->bBlendPhysics = true;

			UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] 已切换 ragdoll"));
		}
	}
	else if (DeathMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		PlayAnimMontage(DeathMontage);
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("[Death] 播放死亡蒙太奇 %s"), *DeathMontage->GetName());
	}

	// 5) 法杖脱手。放在 ragdoll 之后：此时手部骨骼已经开始受物理驱动，
	//    脱手瞬间武器会停在"最后一次跟随手"的世界位置，看起来像自然滑落。
	DropWeaponOnDeath();

	// 5) 重生计时
	if (RespawnDelay > 0.0f && GetWorld())
	{
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, this,
			&AArcaneVanguardCharacter::Respawn, RespawnDelay, false);

		UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] %.1f 秒后重生"), RespawnDelay);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] RespawnDelay=0，不自动重生（调试模式）"));
	}
}

void AArcaneVanguardCharacter::Respawn()
{
	// [!] 先清 bIsDead 再改血量。SetHealth 会触发 OnHealthChanged，
	//     虽然血量变正数不会再次触发死亡，但顺序反了以后改回调容易出死循环。
	bIsDead = false;

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	}

	// 1) 回满血
	if (AttributeSet)
	{
		AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
	}

	// 2) 退出 ragdoll。
	//    顺序：先停物理 -> 再还原变换。反过来的话，物理还在跑，
	//    SetRelativeLocation 会被下一帧的物理求解覆盖掉。
	if (USkeletalMeshComponent* PlayerMesh = GetMesh())
	{
		PlayerMesh->bBlendPhysics = false;
		PlayerMesh->SetAllBodiesSimulatePhysics(false);
		PlayerMesh->SetSimulatePhysics(false);

		// 碰撞预设换回 CharacterMesh（Ragdoll 预设的 AllBodies 会挡住 Pawn 通道，
		// 不换回来的话角色会和自己的身体碰撞，表现为原地抖动）
		PlayerMesh->SetCollisionProfileName(TEXT("CharacterMesh"));

		// 还原 Mesh 相对变换。ragdoll 期间物理把它改成了"倒地的姿态"，
		// 不还原的话角色会以躺着的姿势站起来。
		PlayerMesh->SetRelativeLocationAndRotation(
			InitialMeshRelativeLocation, InitialMeshRelativeRotation);
	}

	// 3) 传送回出生点。TeleportPhysics：跳过物理扫描，避免被残留碰撞体卡住
	SetActorLocation(InitialSpawnLocation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(InitialSpawnRotation);

	// 4) 重新装配武器。
	//    必须放在 Mesh 还原之后：SpawnAndAttachWeapon 用的是插槽的世界变换，
	//    Mesh 还躺着的话，武器会被装到倒地的那个位置上。
	//    地上那把直接销毁重建，比"捡起来重新 Attach"省事且不会残留物理状态。
	DetachAndDestroyWeapon();
	SpawnAndAttachWeapon();

	// 5) 恢复移动
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	// 6) 恢复输入
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] 玩家重生，Health=%.1f"),
		AttributeSet ? AttributeSet->GetHealth() : -1.0f);
}

void AArcaneVanguardCharacter::DropWeaponOnDeath()
{
	if (!SpawnedWeapon)
	{
		return;
	}

	// 脱手。用 KeepWorldTransform：武器会停在"最后一次跟随手部插槽"的世界位置，
	// 看起来是从手里滑落，而不是瞬移到角色原点。
	SpawnedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 转成物理体自由落体。
	// 武器构造函数里是 NoCollision（正常游戏时不该挡角色移动），
	// 不开物理的话脱手后它会悬在半空中。
	if (AArcaneVanguardWeapon* DroppedWeapon = Cast<AArcaneVanguardWeapon>(SpawnedWeapon))
	{
		DroppedWeapon->EnablePhysicsDrop();
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Death] 武器不是 AArcaneVanguardWeapon，只做了脱手、不会掉落。"
			     "检查 BP_ThirdPersonCharacter 的 WeaponActorClass 是否继承自它。"));
	}

	// [!] 这里不把 SpawnedWeapon 置空：重生时要用它找到地上那把并销毁。
	UE_LOG(LogTemplateCharacter, Log, TEXT("[Death] 法杖已脱手"));
}

AActor* AArcaneVanguardCharacter::SpawnAndAttachWeapon()
{
	// 已经有武器就不重复生成
	if (SpawnedWeapon)
	{
		return SpawnedWeapon;
	}

	if (!WeaponActorClass)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Weapon] 未指定 WeaponActorClass，跳过武器生成。请在 BP_ThirdPersonCharacter 的 Weapon 分类里指定武器蓝图。"));
		return nullptr;
	}

	USkeletalMeshComponent* OwnerMesh = GetMesh();
	UWorld* World = GetWorld();
	if (!OwnerMesh || !World)
	{
		return nullptr;
	}

	// 插槽缺失时退回 Mesh 自身变换，并给出可诊断的警告
	const bool bSocketExists = OwnerMesh->DoesSocketExist(WeaponAttachSocket);
	if (!bSocketExists)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Weapon] 骨架 %s 上找不到插槽 '%s'，将附着到 Mesh 根节点。请检查 SKE_Wizard 的 Socket 配置。"),
			*GetNameSafe(OwnerMesh->GetSkeletalMeshAsset()), *WeaponAttachSocket.ToString());
	}

	// 生成点 = 插槽的世界变换。这样附着后相对变换为 Identity，是校准的零点。
	// [!] 但不要在蓝图里改 MeshComponent 的相对变换来校准 —— 下面那次 Attach 会把它冲掉。
	const FTransform SpawnTransform = bSocketExists
		? OwnerMesh->GetSocketTransform(WeaponAttachSocket, RTS_World)
		: OwnerMesh->GetComponentTransform();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	// 生成点可能和角色胶囊重叠，必须 AlwaysSpawn，否则会被碰撞检测拒绝
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AArcaneVanguardWeapon* NewWeapon = World->SpawnActor<AArcaneVanguardWeapon>(
		WeaponActorClass,
		SpawnTransform.GetLocation(),
		SpawnTransform.Rotator(),
		SpawnParams);

	if (!NewWeapon)
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("[Weapon] 生成武器失败，WeaponActorClass = %s"), *GetNameSafe(WeaponActorClass));
		return nullptr;
	}

	// [!] 这里**不能**调 SetActorEnableCollision(false)。
	//     它是 Actor 级的总开关，优先级高于组件上的 SetCollisionEnabled ——
	//     Spawn 之后一旦关掉，之后无论怎么设 BodyInstance / RecreatePhysicsState
	//     都读回 NoCollision，武器脱手后直接穿过地面（2026-10-30 排查了很久，根因就是这行）。
	//     不挡角色移动这件事改由武器自己解决：
	//     AArcaneVanguardWeapon::ApplyWeaponCollisionSetup 里把 Pawn 通道设为 Ignore。

	// 附着：KeepWorldTransform + 生成点就是插槽点 => 相对变换归零
	NewWeapon->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::KeepWorldTransform,
		bSocketExists ? WeaponAttachSocket : NAME_None);

	// 附着完成后再记录战斗握持偏移的基准位置。
	// 顺序很重要：武器 Actor 的 BeginPlay 在 SpawnActor 时就已执行，那时还没 Attach，
	// 相对变换不是最终值，所以在角色这边补一次。
	NewWeapon->CaptureBaseTransform();

	SpawnedWeapon = NewWeapon;

	// 新武器默认可见。重生时会先销毁上一把再重新生成，
	// 如果不重置，上一把在施法期间被隐藏的状态会"继承"到新武器上。
	bWeaponHidden = false;
	NewWeapon->SetActorHiddenInGame(false);

	UE_LOG(LogTemplateCharacter, Log,
		TEXT("[Weapon] 已生成并附着 '%s' 到插槽 '%s'"),
		*GetNameSafe(NewWeapon), *WeaponAttachSocket.ToString());

	return NewWeapon;
}

void AArcaneVanguardCharacter::DetachAndDestroyWeapon()
{
	if (!SpawnedWeapon)
	{
		return;
	}

	SpawnedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SpawnedWeapon->Destroy();
	SpawnedWeapon = nullptr;

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Weapon] 已卸下并销毁武器"));
}

void AArcaneVanguardCharacter::SetWeaponHidden(bool bInHidden)
{
	if (!SpawnedWeapon)
	{
		return;
	}

	// 武器已经脱手（死亡掉落）就不管 —— 那是地上的道具，不该被施法逻辑影响。
	// 用 GetAttachParentActor 判断"还在手上"而不是看 bIsDead，
	// 这样以后加别的脱手场景（缴械 / 换武器）也能自动跳过。
	if (SpawnedWeapon->GetAttachParentActor() != this)
	{
		return;
	}

	// [!] 参数名不能叫 bHidden —— AActor 已经有同名成员，UHT 会报 shadowing 错误。
	bWeaponHidden = bInHidden;

	SpawnedWeapon->SetActorHiddenInGame(bInHidden);

	// [!] 只改可见性，**不动碰撞**。
	//     武器的碰撞在 OnConstruction 里就配成"只 Block WorldStatic，其余全 Ignore"
	//     （见 AArcaneVanguardWeapon::ApplyWeaponCollisionSetup），
	//     所以它本来就不会挡住角色、敌人或攻击球扫 —— 藏起来不需要关碰撞。
	//     反过来动 SetActorEnableCollision 会有"关了之后恢复不回来"的风险
	//     （Spawn 之后再改 Actor 级开关，会盖住组件的碰撞配置）。
	UE_LOG(LogTemplateCharacter, Verbose, TEXT("[Weapon] 法杖%s"),
		bInHidden ? TEXT("已隐藏") : TEXT("已显示"));
}

//////////////////////////////////////////////////////////////////////////
// 战斗状态（M1 临时实现）

void AArcaneVanguardCharacter::SetInCombat(bool bNewInCombat)
{
	if (bInCombat == bNewInCombat)
	{
		return;
	}

	bInCombat = bNewInCombat;

	// AnimBP 通过轮询 IsInCombat()/bInCombat 读取，这里只做状态记录与日志
	UE_LOG(LogTemplateCharacter, Log, TEXT("[Combat] 战斗状态 -> %s"),
		bInCombat ? TEXT("ON") : TEXT("OFF"));
}

void AArcaneVanguardCharacter::ToggleCombatState()
{
	// TODO(M3): 接入战斗系统后删除本函数与 F 键绑定
	SetInCombat(!bInCombat);
}

//////////////////////////////////////////////////////////////////////////
// 普通攻击三段连击（M1）
//
// 状态流转：
//   Idle --OnAttackPressed--> Combo_01
//   Combo_0N 的连击窗口内按下 --> ArmNextSection()：把当前段链到下一段
//   Combo_0N 的连击窗口外按下 --> 只置 bAttackBuffered，窗口开启时再挂链
//   段尾 --> 引擎沿已挂的链自然过渡到下一段（后摇完整播完）
//   最后一段播完 / 蒙太奇被打断 --> ClearComboState()
//
// 为什么"按下时挂链"就等于"窗口结束时挂链"：
//   Montage_SetNextSection 只在段尾生效，挂早了也不会提前切。
//   而窗口终点常常正好压在段尾，那时引擎会直接停掉蒙太奇，NotifyEnd 未必被调用，
//   所以不能把挂链放在 NotifyEnd 里（实测整段播完一条日志都没有）。
//
// 资产里三段的 Next Section 必须是 None（否则会自己连播）；
// 段间链接只在运行时由代码临时挂上，且只作用于当前播放实例，不写回资产。

FName AArcaneVanguardCharacter::GetComboSectionName(int32 ZeroBasedIndex)
{
	return FName(*FString::Printf(TEXT("Combat_%02d"), ZeroBasedIndex + 1));
}

void AArcaneVanguardCharacter::OnAttackPressed()
{
	if (bRequireCombatForAttack && !bInCombat)
	{
		return;
	}

	if (IsAttacking())
	{
		// 窗口内按下：立刻挂链。注意挂链只影响"段尾往哪走"，
		// 不会立刻切段，所以当前段的后摇照样完整播完。
		if (bComboWindowOpen)
		{
			ArmNextSection();
		}
		// 窗口外按下（提前按）：先缓存，窗口开启时再挂
		else
		{
			bAttackBuffered = true;
		}
		return;
	}

	// 空闲：起手第一段
	PlayComboSection(0);
}

void AArcaneVanguardCharacter::ArmNextSection()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ComboMontage || !AnimInstance->Montage_IsPlaying(ComboMontage))
	{
		return;
	}

	const FName CurrentSectionName = AnimInstance->Montage_GetCurrentSection(ComboMontage);
	const int32 CurrentSectionIndex = ComboMontage->GetSectionIndex(CurrentSectionName);

	// 找不到当前段，或已经是最后一段：不接
	if (CurrentSectionIndex == INDEX_NONE || CurrentSectionIndex + 1 >= ComboSectionCount)
	{
		return;
	}

	// 这一段已经挂过了就不重复挂
	if (ArmedNextSectionIndex == CurrentSectionIndex + 1)
	{
		return;
	}

	const FName NextSectionName = GetComboSectionName(CurrentSectionIndex + 1);

	// 改的是当前播放实例的 FAnimMontageInstance::NextSections，不写回资产。
	// 引擎会在段尾沿这条链接自然过渡。
	AnimInstance->Montage_SetNextSection(CurrentSectionName, NextSectionName, ComboMontage);
	ArmedNextSectionIndex = CurrentSectionIndex + 1;

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Combo] 挂链 %s -> %s"),
		*CurrentSectionName.ToString(), *NextSectionName.ToString());
}

void AArcaneVanguardCharacter::PlayComboSection(int32 ZeroBasedIndex)
{
	USkeletalMeshComponent* OwnerMesh = GetMesh();
	UAnimInstance* AnimInstance = OwnerMesh ? OwnerMesh->GetAnimInstance() : nullptr;

	if (!AnimInstance || !ComboMontage)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Combo] 无法播放：AnimInstance=%s，ComboMontage=%s。请在 BP 的 Combat|Combo 分类里指定 ComboMontage。"),
			(AnimInstance ? TEXT("OK") : TEXT("NULL")), *GetNameSafe(ComboMontage));
		return;
	}

	if (ZeroBasedIndex < 0 || ZeroBasedIndex >= ComboSectionCount)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Combo] 段索引 %d 越界（ComboSectionCount=%d）"), ZeroBasedIndex, ComboSectionCount);
		return;
	}

	const FName SectionName = GetComboSectionName(ZeroBasedIndex);

	// Section 名拼错是静默失败，这里显式拦一道，避免"卡在第一段"这种难查的问题
	if (ComboMontage->GetSectionIndex(SectionName) == INDEX_NONE)
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("[Combo] 蒙太奇 %s 上找不到 Section '%s'，请检查命名是否为 Combat_01/02/03"),
			*ComboMontage->GetName(), *SectionName.ToString());
		return;
	}

	ComboIndex = ZeroBasedIndex + 1;
	bAttackBuffered = false;

	// 攻速加成：从属性读倍率，乘到播放速率上。
	// Buff 到期后 GE 会自动移除，倍率回到 1.0，这里不需要任何恢复逻辑。
	float EffectivePlayRate = ComboPlayRate;
	if (AttributeSet)
	{
		EffectivePlayRate *= AttributeSet->GetAttackSpeedMultiplier();
	}

	if (AnimInstance->Montage_IsPlaying(ComboMontage))
	{
		// 已经在同一段上就不要再跳，重复 JumpToSection 会把动画重置回段首
		const FName CurrentSectionName = AnimInstance->Montage_GetCurrentSection(ComboMontage);
		if (CurrentSectionName == SectionName)
		{
			ComboIndex = ZeroBasedIndex + 1;
			return;
		}

		// 已在播同一条蒙太奇：用 JumpToSection 接下一段，不重启、不重新混合
		AnimInstance->Montage_JumpToSection(SectionName, ComboMontage);

		// 接段时也要刷新播放速率：JumpToSection 不会改速率，
		// 不设的话第二段起会沿用起手时的速率，中途上 Buff 就不生效了。
		AnimInstance->Montage_SetPlayRate(ComboMontage, EffectivePlayRate);
	}
	else
	{
		PlayAnimMontage(ComboMontage, EffectivePlayRate, SectionName);

		// 绑定结束回调（必须绑，否则连击计数不清零，下次攻击会从第三段开始）
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AArcaneVanguardCharacter::HandleComboMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, ComboMontage);

		// 攻击期间减速，避免边打边滑步
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			if (!bMovementSlowedByAttack)
			{
				CachedMaxWalkSpeed = Movement->MaxWalkSpeed;
				bMovementSlowedByAttack = true;
			}
			Movement->MaxWalkSpeed = CachedMaxWalkSpeed * AttackMoveSpeedScale;
		}
	}

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Combo] -> 第 %d 段 (%s)"),
		ComboIndex, *SectionName.ToString());
}

void AArcaneVanguardCharacter::SetComboWindowOpen(bool bOpen)
{
	bComboWindowOpen = bOpen;

	if (bOpen)
	{
		// 窗口开启：把"提前按"的那次输入消费掉（挂链，不切段）
		if (bAttackBuffered)
		{
			bAttackBuffered = false;
			ArmNextSection();
		}
		return;
	}

	// 窗口关闭：没消费掉的输入作废。
	// 这里不做挂链 —— 窗口终点常常和段尾撞在同一帧，NotifyEnd 未必会被调用，
	// 挂链的时机统一放在"按下时"和"窗口开启时"两处。
	bAttackBuffered = false;
}

void AArcaneVanguardCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ComboMontage || !AnimInstance->Montage_IsPlaying(ComboMontage))
	{
		return;
	}

	// ComboIndex 永远以蒙太奇实际在播的段为准，杜绝状态和画面失步
	const int32 CurrentSectionIndex =
		ComboMontage->GetSectionIndex(AnimInstance->Montage_GetCurrentSection(ComboMontage));

	if (CurrentSectionIndex == INDEX_NONE)
	{
		return;
	}

	ComboIndex = CurrentSectionIndex + 1;

	// 已经真的切到目标段了，兜底标记可以清掉
	if (ArmedNextSectionIndex != INDEX_NONE && CurrentSectionIndex >= ArmedNextSectionIndex)
	{
		ArmedNextSectionIndex = INDEX_NONE;

		// 新段开始了：上一段残留的缓冲和窗口状态都作废。
		// 否则把连击窗口调早之后，玩家在"窗口结束~段尾"这段空隙里多按的一下
		// 会留到下一段，白送一次连击。
		bAttackBuffered = false;
		bComboWindowOpen = false;
	}
}

void AArcaneVanguardCharacter::SetAttackWindowOpen(bool bOpen)
{
	bAttackWindowOpen = bOpen;

	// 开窗口 = 新的一刀开始。清空命中记录，允许重新命中上一段打过的目标。
	// （每段是独立的一刀，同一个敌人当然可以被第二段再打一次）
	if (bOpen)
	{
		HitActorsThisAttack.Reset();
	}

	UE_LOG(LogTemplateCharacter, Verbose, TEXT("[Combo] 判定窗口 %s"), bOpen ? TEXT("开") : TEXT("关"));
}

int32 AArcaneVanguardCharacter::PerformAttackHitCheck(float DamageOverride)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// 起扫点用胶囊体中心（GetActorLocation），不是脚底。
	// 从脚底起扫时球心贴地，只能扫到敌人小腿；从中心起扫正好覆盖躯干。
	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorForwardVector() * AttackRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AVAttackHitCheck), /*bTraceComplex=*/false, this);
	QueryParams.AddIgnoredActor(this);
	// 武器是独立 Actor，挂在手上、就在扫掠路径里，必须排除，否则每次都会命中自己的法杖
	if (IsValid(SpawnedWeapon))
	{
		QueryParams.AddIgnoredActor(SpawnedWeapon);
	}

	// 只查 Enemy 通道的物体（DefaultEngine.ini 里 Name="Enemy"）。
	//
	// [!] 这里必须用 ObjectType 查询，不能再用 SweepMultiByChannel(Enemy)：
	//     Enemy 通道在 ini 里 DefaultResponse=Block，含义是"所有物体对 Enemy 通道都 Block"。
	//     如果按 TraceChannel 扫，地面、墙壁、玩家自己全都会命中 ——
	//     虽然最后会被"没有 ASC 就跳过"兜住，但会刷一堆无用日志、多做无用计算。
	//     SweepMultiByObjectType 只按物体自身的 ObjectType 过滤，精确得多。
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(AttackRadius),
		QueryParams);

#if ENABLE_DRAW_DEBUG
	// ---------------------------------------------------------------------
	// 判定范围可视化
	// 两种开关：BP 属性 bDrawAttackHitCheck，或控制台 av.DrawAttackHitCheck 1
	// ---------------------------------------------------------------------
	const bool bDrawDebug = bDrawAttackHitCheck || CVarAVDrawAttackHitCheck.GetValueOnGameThread() != 0;
	if (bDrawDebug)
	{
		const float LifeTime = AttackDebugDrawDuration;

		// 扫掠体积 = 胶囊体：圆柱长 AttackRange，两端半球半径 AttackRadius。
		// [!] UE 5.4 没有 DrawDebugSweptSphere（只在 UE5 早期版本有），
		//    改用 DrawDebugCapsule + 手动把胶囊轴向对齐扫掠方向。
		const FVector Center = (Start + End) * 0.5f;
		const FVector SweepDir = End - Start;
		const float HalfLen = SweepDir.Size() * 0.5f;
		FQuat CapsuleRot = FQuat::Identity;
		if (!SweepDir.IsNearlyZero())
		{
			// DrawDebugCapsule 的默认轴向是 +Z，转到扫掠方向
			CapsuleRot = FQuat::FindBetweenNormals(FVector::UpVector, SweepDir.GetSafeNormal());
		}
		// HalfHeight 含半球（与 UCapsuleComponent 一致）= 圆柱半长 + 半径
		DrawDebugCapsule(World, Center, HalfLen + AttackRadius, AttackRadius,
			CapsuleRot, FColor::Green, false, LifeTime, 0, 2.0f);

		// 球心轨迹（黄色），起点用青色点标出
		DrawDebugLine(World, Start, End, FColor::Yellow, false, LifeTime, 0, 2.0f);
		DrawDebugPoint(World, Start, 10.0f, FColor::Cyan, false, LifeTime);

		// 命中点（红点）+ 从球心到命中点的连线（看清是哪个位置被扫到）
		for (const FHitResult& Hit : Hits)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 14.0f, FColor::Red, false, LifeTime);
			DrawDebugLine(World, Start, Hit.ImpactPoint, FColor::Red, false, LifeTime, 0, 1.0f);
		}

		// 参数摘要，贴在起点上方，方便截图对照
		const FString Info = FString::Printf(
			TEXT("Range %.0f | Radius %.0f | Hits %d"),
			AttackRange, AttackRadius, Hits.Num());
		DrawDebugString(World, Start + FVector(0.0f, 0.0f, AttackRadius + 30.0f),
			Info, nullptr, FColor::White, LifeTime, true);
	}
#endif

	const float FinalDamage = (DamageOverride > 0.f) ? DamageOverride : AttackDamage;

	int32 HitCount = 0;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || HitActor == this)
		{
			continue;
		}

		// 同一段内同一目标只结算一次：
		// 球扫会为一个目标的多个骨骼/组件返回多条 Hit，不去重会打 3~4 次。
		const TWeakObjectPtr<AActor> HitKey(HitActor);
		if (HitActorsThisAttack.Contains(HitKey))
		{
			continue;
		}
		HitActorsThisAttack.Add(HitKey);

		// ADR-005：伤害唯一入口。这里只负责"打到谁"，扣多少血由 CombatStatics 结算。
		const float Applied = UArcaneVanguardCombatStatics::ApplyDamage(
			HitActor, FinalDamage, this, DamageEffectClass);

		if (Applied > 0.f)
		{
			++HitCount;
			UE_LOG(LogTemplateCharacter, Log, TEXT("[Hit] 第 %d 段命中 %s，伤害 %.1f"),
				ComboIndex, *GetNameSafe(HitActor), Applied);
		}
		else
		{
			// 扫到了但没有 ASC（比如场景道具），说明判定半径可能过大，值得看一眼
			UE_LOG(LogTemplateCharacter, Verbose, TEXT("[Hit] 扫到 %s 但不是 GAS Actor，跳过"),
				*GetNameSafe(HitActor));
		}
	}

	return HitCount;
}

void AArcaneVanguardCharacter::ResetCombo()
{
	// 手动复位（死亡、受击硬直等）时把还在播的蒙太奇一起停掉，
	// 避免"状态说没在攻击、画面还在挥杖"。
	if (ComboMontage)
	{
		if (USkeletalMeshComponent* OwnerMesh = GetMesh())
		{
			if (UAnimInstance* AnimInstance = OwnerMesh->GetAnimInstance())
			{
				if (AnimInstance->Montage_IsPlaying(ComboMontage))
				{
					AnimInstance->Montage_Stop(0.15f, ComboMontage);
				}
			}
		}
	}

	ClearComboState();
}

void AArcaneVanguardCharacter::ClearComboState()
{
	ComboIndex = 0;
	bComboWindowOpen = false;
	bAttackWindowOpen = false;
	bAttackBuffered = false;
	ArmedNextSectionIndex = INDEX_NONE;

	if (bMovementSlowedByAttack)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = CachedMaxWalkSpeed;
		}
		bMovementSlowedByAttack = false;
	}
}

void AArcaneVanguardCharacter::HandleComboMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ComboMontage)
	{
		return;
	}

	// 兜底：窗口结束时已经武装了下一段，但链接没生效（蒙太奇在段尾直接结束了）。
	// 这时不要回待机，直接接着播下一段，连击不会断。
	if (!bInterrupted && ArmedNextSectionIndex != INDEX_NONE)
	{
		const int32 NextIndex = ArmedNextSectionIndex;

		UE_LOG(LogTemplateCharacter, Log,
			TEXT("[Combo] 链接未生效，兜底接第 %d 段"), NextIndex + 1);

		ClearComboState();
		PlayComboSection(NextIndex);
		return;
	}

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Combo] 结束（%s）"),
		bInterrupted ? TEXT("被打断") : TEXT("正常播完"));

	// 蒙太奇正在被引擎停用，这里只清状态，不要再调 Montage_Stop
	ClearComboState();
}

//////////////////////////////////////////////////////////////////////////
// Input

void AArcaneVanguardCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 注意：MappingContext 的注册已迁至 AArcaneVanguardPlayerController（ADR-010）。
	// 原因：MappingContext 注册在 UEnhancedInputLocalPlayerSubsystem 上，其生命周期是 LocalPlayer，
	// 比 Pawn 长。若在 Pawn 上 Add 而 Pawn 上不 Remove，会随 Possess 次数累积。

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AArcaneVanguardCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AArcaneVanguardCharacter::Look);

		// Q 技能（M4）：用 Started 而不是 Triggered —— 按住不该连续放技能，
		// 连续释放由技能的 Cooldown GE 控制，不靠输入层的重复触发。
		if (SkillQAction)
		{
			EnhancedInputComponent->BindAction(SkillQAction, ETriggerEvent::Started, this, &AArcaneVanguardCharacter::Input_SkillQ);
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("[GAS] SkillQAction 未指定 —— Q 技能无法触发。请在 BP_ThirdPersonCharacter 的 Input 分类里指定 IA_SkillQ。"));
		}

		// 普攻（AV-052）：同样用 Started。
		// 连击推进完全由 OnAttackPressed 内部的窗口判定控制（起手 / 挂链 / 缓存），
		// 输入层再用 Triggered 会导致按住时一帧推一段。
		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AArcaneVanguardCharacter::OnAttackPressed);
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("[Input] AttackAction 未指定 —— 普攻无法触发。请在 BP_ThirdPersonCharacter 的 Input 分类里指定 IA_Attack。"));
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	// ---------------------------------------------------------------------
	// 临时调试：F 键切换战斗状态
	// TODO(M3): 接入战斗系统后删除这段绑定与 ToggleCombatState()
	// ---------------------------------------------------------------------
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AArcaneVanguardCharacter::ToggleCombatState);
	}
}

void AArcaneVanguardCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AArcaneVanguardCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AArcaneVanguardCharacter::Input_SkillQ()
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("[GAS] Q 技能：AbilitySystemComponent 为空，输入到了但组件没初始化"));
		return;
	}

	// 用 Tag 激活（而不是按类）：换技能只要改 BP 里授予的 GA 类，
	// 输入层完全不用动。
	// [!] GA 蓝图的 AbilityTags 里必须配同一个 Tag，否则这里会静默什么都不发生。
	FGameplayTagContainer SkillTags;
	SkillTags.AddTag(TAG_Ability_Skill_Q);

	// 先统计「带这个 Tag 的技能有几个」——这是按了没反应最常见的两个原因：
	//   TotalAbilities == 0  -> BP 的 DefaultAbilities 没填（技能没授予）
	//   Matching == 0        -> 技能授予了，但 GA 蓝图的 AbilityTags 没配 Ability.Skill.Q
	// 这两个数字能直接区分，不用靠猜。
	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	AbilitySystemComponent->GetActivatableGameplayAbilitySpecsByAllMatchingTags(SkillTags, MatchingSpecs);

	const int32 TotalAbilities = AbilitySystemComponent->GetActivatableAbilities().Num();

	const bool bActivated = AbilitySystemComponent->TryActivateAbilitiesByTag(SkillTags);

	// 用 Log 而不是 Verbose：Verbose 在默认日志窗口里被过滤掉，
	// 会让人误以为「代码没跑到」。这条是排查技能链路的唯一入口，必须可见。
	UE_LOG(LogTemplateCharacter, Log,
		TEXT("[GAS] Q 技能激活请求 -> %s（已授予 %d 个技能，带 Ability.Skill.Q 的有 %d 个）"),
		bActivated ? TEXT("成功") : TEXT("失败"), TotalAbilities, MatchingSpecs.Num());

	if (!bActivated)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[GAS] Q 技能激活失败。按下面顺序排查：\n")
			TEXT("  1) 已授予 0 个技能 -> BP_ThirdPersonCharacter 的 Default Abilities 没填\n")
			TEXT("  2) 带 Tag 的 0 个   -> GA_ArcaneBlast 的 Ability Tags 里没加 Ability.Skill.Q\n")
			TEXT("  3) 两者都有         -> 冷却中 / Mana 不足 / 技能本身 Blocked"));
	}
}
