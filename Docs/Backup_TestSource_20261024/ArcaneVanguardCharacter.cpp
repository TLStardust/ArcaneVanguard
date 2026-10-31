// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcaneVanguardCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AArcaneVanguardCharacter

AArcaneVanguardCharacter::AArcaneVanguardCharacter()
{
	// 武器竖直约束需要在动画求值之后再执行，所以放在 PostUpdateWork
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

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

	// 武器网格：先挂在 Mesh 上，真正的挂点在 BeginPlay 解析
	// （构造期 BP 还没把网格资产设进来，此时查骨骼一定失败，所以不在构造期挂 socket）
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh());
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetCanEverAffectNavigation(false);

	// 巫师骨架没有任何 socket，直接用右手骨骼作为挂点
	WeaponAttachSocket = TEXT("StickSocket");
	WeaponRelativeTransform = FTransform::Identity;

	// 默认开启竖直约束：这套动画的手腕在待机/施法之间差 172°，不约束会翻杖
	bKeepWeaponUpright = true;
	WeaponGripOffset = 96.f;

	// 这套技能动画来自 Mixamo，没有武器；除 Cast_Spell_01 外的施法动作都要收杖
	WeaponHiddenAnimNames.Add(TEXT("Anim_Wizard_Cast_Spell_02"));
	WeaponHiddenAnimNames.Add(TEXT("Anim_Wizard_Magic_Attack"));
	WeaponHiddenAnimNames.Add(TEXT("Anim_Wizard_Magic_Heal"));

	// 死亡时法杖脱手掉落
	WeaponDropAnimNames.Add(TEXT("Anim_Wizard_Death_Forward"));

	// 调试按键默认开启（1~5），发布前在蓝图里关掉
	bEnableDebugAnimKeys = true;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AArcaneVanguardCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	AttachWeapon();
}

void AArcaneVanguardCharacter::AttachWeapon()
{
	USkeletalMeshComponent* OwnerMesh = GetMesh();
	if (!WeaponMesh || !OwnerMesh)
	{
		return;
	}

	// 挂点既可以是 socket，也可以是骨骼：SKE_Wizard 有 0 个 socket，所以这里两种都认
	const bool bHasSocket = WeaponAttachSocket != NAME_None && OwnerMesh->DoesSocketExist(WeaponAttachSocket);
	const bool bHasBone = WeaponAttachSocket != NAME_None && OwnerMesh->GetBoneIndex(WeaponAttachSocket) != INDEX_NONE;
	const bool bResolved = bHasSocket || bHasBone;

	if (!bResolved && WeaponAttachSocket != NAME_None)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Weapon] 挂点 '%s' 在网格 '%s' 上既不是 socket 也不是骨骼，武器退回 Mesh 根节点。"),
			*WeaponAttachSocket.ToString(),
			*GetNameSafe(OwnerMesh->GetSkeletalMeshAsset()));
	}

	WeaponMesh->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		bResolved ? WeaponAttachSocket : NAME_None);

	WeaponMesh->SetRelativeTransform(WeaponRelativeTransform);
}

void AArcaneVanguardCharacter::SetWeaponVisible(bool bVisible)
{
	if (!WeaponMesh)
	{
		return;
	}

	WeaponMesh->SetVisibility(bVisible, true);
	WeaponMesh->SetHiddenInGame(!bVisible, true);

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Weapon] visible = %s"), bVisible ? TEXT("true") : TEXT("false"));
}

bool AArcaneVanguardCharacter::IsWeaponVisible() const
{
	return WeaponMesh && WeaponMesh->IsVisible();
}

void AArcaneVanguardCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateDebugAnimationState();
	UpdateWeaponDropFromAnimation();
	UpdateWeaponVisibilityFromAnimation();
	UpdateWeaponUprightTransform();
}

FName AArcaneVanguardCharacter::GetCurrentAnimationName() const
{
	USkeletalMeshComponent* OwnerMesh = GetMesh();
	UAnimInstance* AnimInstance = OwnerMesh ? OwnerMesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return NAME_None;
	}

	// 技能走 Montage，预览/调试走 SingleNode
	if (const UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage())
	{
		return Montage->GetFName();
	}

	if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		if (const UAnimationAsset* CurrentAsset = SingleNode->GetCurrentAsset())
		{
			return CurrentAsset->GetFName();
		}
	}

	return NAME_None;
}

void AArcaneVanguardCharacter::UpdateWeaponVisibilityFromAnimation()
{
	// 掉落状态由 UpdateWeaponDropFromAnimation 接管，这里不参与
	if (!WeaponMesh || bWeaponDropped || WeaponHiddenAnimNames.Num() == 0)
	{
		return;
	}

	const FName CurrentAnimName = GetCurrentAnimationName();
	const bool bShouldHide = CurrentAnimName != NAME_None && WeaponHiddenAnimNames.Contains(CurrentAnimName);
	if (bShouldHide == bWeaponHiddenByAnimRule)
	{
		return;
	}

	bWeaponHiddenByAnimRule = bShouldHide;
	SetWeaponVisible(!bShouldHide);
}

void AArcaneVanguardCharacter::UpdateWeaponDropFromAnimation()
{
	if (WeaponDropAnimNames.Num() == 0)
	{
		return;
	}

	const FName CurrentAnimName = GetCurrentAnimationName();
	const bool bShouldDrop = CurrentAnimName != NAME_None && WeaponDropAnimNames.Contains(CurrentAnimName);

	if (bShouldDrop && !bWeaponDropped)
	{
		DropWeapon();
	}
	else if (!bShouldDrop && bWeaponDropped)
	{
		RestoreWeapon();
	}
}

void AArcaneVanguardCharacter::DropWeapon()
{
	if (!WeaponMesh || bWeaponDropped)
	{
		return;
	}

	bWeaponDropped = true;

	// 脱手：保留世界变换，避免掉落瞬间瞬移
	WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	// 和地面碰撞，但忽略角色自己（否则会被胶囊弹开）
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WeaponMesh->SetSimulatePhysics(true);

	// 掉落时必须可见
	bWeaponHiddenByAnimRule = false;
	SetWeaponVisible(true);

	const UStaticMesh* StaffMesh = WeaponMesh->GetStaticMesh();
	if (StaffMesh && !StaffMesh->GetBodySetup())
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[Weapon] %s 没有简单碰撞体，物理模拟会穿透地面。请在该静态网格上生成 Simple Collision。"),
			*GetNameSafe(StaffMesh));
	}

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Weapon] 脱手掉落，物理模拟已开启"));
}

void AArcaneVanguardCharacter::RestoreWeapon()
{
	if (!WeaponMesh || !bWeaponDropped)
	{
		return;
	}

	bWeaponDropped = false;

	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	if (USkeletalMeshComponent* OwnerMesh = GetMesh())
	{
		WeaponMesh->AttachToComponent(
			OwnerMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			WeaponAttachSocket);
		WeaponMesh->SetRelativeTransform(WeaponRelativeTransform);
	}

	SetWeaponVisible(true);

	UE_LOG(LogTemplateCharacter, Log, TEXT("[Weapon] 已收回手上"));
}

void AArcaneVanguardCharacter::DebugPlayAnimation(int32 Index)
{
	USkeletalMeshComponent* OwnerMesh = GetMesh();
	if (!OwnerMesh)
	{
		return;
	}

	if (!DebugAnimations.IsValidIndex(Index) || !DebugAnimations[Index])
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[DebugAnim] 索引 %d 无效或动画为空（数组长度 %d）。请在 BP_ThirdPersonCharacter 的 Debug Animations 里填动画。"),
			Index, DebugAnimations.Num());
		return;
	}

	ActiveDebugAnimIndex = Index;

	// 切到 SingleNode 覆盖 AnimBP，播完再由 UpdateDebugAnimationState 切回来
	OwnerMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	OwnerMesh->PlayAnimation(DebugAnimations[Index], false);

	UE_LOG(LogTemplateCharacter, Log, TEXT("[DebugAnim] 播放 [%d] %s"), Index, *GetNameSafe(DebugAnimations[Index]));
}

void AArcaneVanguardCharacter::DebugStopAnimation()
{
	if (USkeletalMeshComponent* OwnerMesh = GetMesh())
	{
		if (OwnerMesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			OwnerMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			UE_LOG(LogTemplateCharacter, Log, TEXT("[DebugAnim] 回到 AnimBP 状态机"));
		}
	}
	ActiveDebugAnimIndex = INDEX_NONE;
}

void AArcaneVanguardCharacter::UpdateDebugAnimationState()
{
	if (ActiveDebugAnimIndex == INDEX_NONE)
	{
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetMesh();
	if (!OwnerMesh)
	{
		return;
	}

	// 单节点播放结束（IsPlaying 变 false）就切回状态机
	const UAnimSingleNodeInstance* SingleNode = OwnerMesh->GetSingleNodeInstance();
	if (!SingleNode || !SingleNode->IsPlaying())
	{
		DebugStopAnimation();
	}
}

void AArcaneVanguardCharacter::DebugPlayAnim1() { DebugPlayAnimation(0); }
void AArcaneVanguardCharacter::DebugPlayAnim2() { DebugPlayAnimation(1); }
void AArcaneVanguardCharacter::DebugPlayAnim3() { DebugPlayAnimation(2); }
void AArcaneVanguardCharacter::DebugPlayAnim4() { DebugPlayAnimation(3); }
void AArcaneVanguardCharacter::DebugPlayAnim5() { DebugPlayAnimation(4); }

void AArcaneVanguardCharacter::UpdateWeaponUprightTransform()
{
	// 掉落期间武器已在物理模拟中，不能再用附着约束覆盖它
	if (bWeaponDropped || !bKeepWeaponUpright || !WeaponMesh)
	{
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetMesh();
	if (!OwnerMesh || WeaponAttachSocket == NAME_None)
	{
		return;
	}

	// 挂点必须真的存在，否则不去干扰 AttachWeapon 已经设好的相对变换
	const bool bSocketOk = OwnerMesh->DoesSocketExist(WeaponAttachSocket)
		|| OwnerMesh->GetBoneIndex(WeaponAttachSocket) != INDEX_NONE;
	if (!bSocketOk)
	{
		return;
	}

	// 目标：握点（武器局部 (0,0,WeaponGripOffset)）落在挂点骨骼上，朝向竖直（yaw 跟随角色）。
	// 武器组件的原点在杖底，所以要先反推原点位置，否则会把握点从手上甩开。
	const FVector GripLocal(0.f, 0.f, WeaponGripOffset);
	const FVector GripWorld = OwnerMesh->GetSocketLocation(WeaponAttachSocket);
	const FRotator DesiredRotation(0.f, GetActorRotation().Yaw, 0.f);
	const FVector DesiredOrigin = GripWorld - DesiredRotation.RotateVector(GripLocal);

	const FTransform DesiredWorld(DesiredRotation, DesiredOrigin, FVector::OneVector);
	const FTransform SocketWorld = OwnerMesh->GetSocketTransform(WeaponAttachSocket, RTS_World);

	// 直接写相对变换，比 SetWorldTransform 更稳（不跟附着系统抢）
	WeaponMesh->SetRelativeTransform(DesiredWorld.GetRelativeTransform(SocketWorld));
}

//////////////////////////////////////////////////////////////////////////
// Input

void AArcaneVanguardCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AArcaneVanguardCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AArcaneVanguardCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	// 调试按键：1~5 播放 DebugAnimations[0..4]，播完自动回状态机
	if (bEnableDebugAnimKeys && PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AArcaneVanguardCharacter::DebugPlayAnim1);
		PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AArcaneVanguardCharacter::DebugPlayAnim2);
		PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AArcaneVanguardCharacter::DebugPlayAnim3);
		PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AArcaneVanguardCharacter::DebugPlayAnim4);
		PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AArcaneVanguardCharacter::DebugPlayAnim5);
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