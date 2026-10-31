// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcaneVanguardWeapon.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ArcaneVanguardCharacter.h"

AArcaneVanguardWeapon::AArcaneVanguardWeapon()
{
	// 需要 Tick 做战斗握持偏移的插值
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	
	// 武器不应该被角色阴影以外的系统拾取
	MeshComponent->SetCanEverAffectNavigation(false);

	// 战斗握持偏移默认值：沿杖身轴（本地 Z）前移 90cm。
	// 正值 = 朝杖尖方向平移 => 手握点相对滑向杖尾 => 前段变长。
	// 法杖网格长 194.4cm、枢轴在杖尾；滑多少合适要在蓝图细节面板里试，改这里需要重编译。
	CombatGripOffset = FVector(0.0f, 0.0f, 90.0f);

	// alpha / 秒。10.0 ≈ 0.1 秒从 0 走到 1。
	// [!] 与 AnimBP 的 CombatAlpha 过渡时长（当前 1 秒）并不一致，两边要同步才有"人杖一体"的观感。
	CombatGripInterpSpeed = 10.0f;
	bAutoTrackOwnerCombat = true;

	// 碰撞配置默认由 C++ 统一处理（见 ApplyWeaponCollisionSetup）。
	// 想自己在蓝图里精调就关掉它。
	bSetupCollisionInConstruction = true;

	// 脱手时的随机翻滚幅度。10 ≈ 轻微打转，调 0 则直直落下。
	// 40 以上会让 194cm 长的法杖在空中明显甩动，不自然。
	DropAngularImpulseRange = 10.0f;

	bBaseCaptured = false;
	CurrentCombatAlpha = 0.0f;
}

void AArcaneVanguardWeapon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 让蓝图里改 MeshAsset 后，组件立刻跟着变
	if (MeshComponent && MeshAsset && MeshComponent->GetStaticMesh() != MeshAsset)
	{
		MeshComponent->SetStaticMesh(MeshAsset);
	}

	// [!] 碰撞必须在这里配，不能等到脱手时才改。
	//     OnConstruction 在 Spawn 流程里早于物理状态创建，
	//     此时设的配置才会被 Chaos 采纳；运行时（死亡时）再改，物理体不会重建。
	ApplyWeaponCollisionSetup();
}

void AArcaneVanguardWeapon::ApplyWeaponCollisionSetup()
{
	if (!MeshComponent)
	{
		return;
	}

	// 关掉开关时完全沿用蓝图里配的碰撞，不覆盖
	if (!bSetupCollisionInConstruction)
	{
		return;
	}

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 武器用自己的独立通道（DefaultEngine.ini 里 Name="Weapon"）。
	// 好处：不再和 UE 默认通道混在一起，也不会因为某个默认通道没被逐个 Ignore
	// 而出现"武器把 ragdoll 尸体撞飞"这类问题（PhysicsBody 通道就是这么漏掉的）。
	MeshComponent->SetCollisionObjectType(ECC_GameTraceChannel1);

	// [!] 用「全 Ignore，只开一个 Block」而不是「全 Block，再逐个 Ignore」。
	//     后者每加一条新通道都得记得补一条 Ignore，漏一个就出事故；
	//     前者默认安全，只在明确需要时才放开。
	//     武器唯一需要碰撞的是静态世界（地面/墙/建筑）—— 脱手落地靠它。
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	MeshComponent->SetGenerateOverlapEvents(false);
}

void AArcaneVanguardWeapon::BeginPlay()
{
	Super::BeginPlay();

	// 注意：SpawnActor 出来的武器，BeginPlay 在 Attach 之前就执行了，
	// 此时相对变换还不是最终值，所以这里不记录基准。
	// 基准由 Character 在 Attach 完成后调 CaptureBaseTransform()，或在首次 Tick 时兜底记录。
}

void AArcaneVanguardWeapon::CaptureBaseTransform()
{
	if (!MeshComponent)
	{
		return;
	}

	BaseMeshRelativeLocation = MeshComponent->GetRelativeLocation();
	bBaseCaptured = true;
	CurrentCombatAlpha = 0.0f;

	// 记录时把偏移清掉，保证起点是干净的校准值
	MeshComponent->SetRelativeLocation(BaseMeshRelativeLocation);
}

void AArcaneVanguardWeapon::EnablePhysicsDrop()
{
	if (!MeshComponent)
	{
		return;
	}

	// 1) 停 Tick。握持插值每帧调 ApplyCombatAlpha 改 Mesh 的相对位置，
	//    和物理模拟同时作用会互相打架（表现为落地瞬间抖动或瞬移）。
	SetActorTickEnabled(false);

	// 2) 碰撞已经在 OnConstruction 里配好了（QueryAndPhysics + PhysicsBody）。
	//    [!] 不能在这里才改碰撞 —— 2026-10-30 实测：Spawn 之后物理体已经按 NoCollision
	//        建好，此时无论 SetCollisionEnabled / SetCollisionProfileName / RecreatePhysicsState
	//        怎么组合，GetCollisionEnabled() 都返回 0、GetMass() 停在默认 1.0，法杖穿过地面。
	//        正确做法是在 OnConstruction（物理状态创建之前）配好，这里只负责开模拟。
	//    保留兜底分支：万一某个生成路径没走到 OnConstruction，补设一次再重建。
	if (MeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Weapon] %s 脱手时碰撞仍未启用（预期 QueryAndPhysics），补设一次"),
			*GetName());

		ApplyWeaponCollisionSetup();
		MeshComponent->RecreatePhysicsState();
	}

	// 3) 开模拟并唤醒（脱离 Attach 后刚体可能处于睡眠状态）
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();

	// 4) 诊断：CollisionEnabled 必须是 3，Mass 应该是法杖真实重量（不是默认 1.0）
	UE_LOG(LogTemp, Log,
		TEXT("[Weapon] %s 脱手：CollisionEnabled=%d SimulatePhysics=%d Mass=%.2f Mesh=%s BodyInstance=%s"),
		*GetName(),
		static_cast<int32>(MeshComponent->GetCollisionEnabled()),
		MeshComponent->IsSimulatingPhysics() ? 1 : 0,
		MeshComponent->GetMass(),
		MeshComponent->GetStaticMesh() ? *MeshComponent->GetStaticMesh()->GetName() : TEXT("NULL"),
		MeshComponent->GetBodyInstance() ? TEXT("OK") : TEXT("NULL"));

	// 5) 随机角速度：否则每次都以完全相同的姿态落地，看起来像"摆上去的"。
	//    幅度由 DropAngularImpulseRange 控制（蓝图可调），填 0 就完全不翻滚。
	if (DropAngularImpulseRange > 0.0f)
	{
		const FVector RandomAngularImpulse(
			FMath::FRandRange(-DropAngularImpulseRange, DropAngularImpulseRange),
			FMath::FRandRange(-DropAngularImpulseRange, DropAngularImpulseRange),
			FMath::FRandRange(-DropAngularImpulseRange, DropAngularImpulseRange));

		MeshComponent->AddAngularImpulseInDegrees(RandomAngularImpulse, NAME_None, /*bVelChange=*/true);
	}

	UE_LOG(LogTemp, Log, TEXT("[Weapon] %s 已脱手，进入自由落体"), *GetName());
}

void AArcaneVanguardWeapon::SetCombatAlpha(float InAlpha)
{
	// 外部直接设定（不做插值），用于 AnimBP/战斗系统驱动
	CurrentCombatAlpha = FMath::Clamp(InAlpha, 0.0f, 1.0f);
	ApplyCombatAlpha(CurrentCombatAlpha);
}

void AArcaneVanguardWeapon::ApplyCombatAlpha(float InAlpha)
{
	if (!MeshComponent)
	{
		return;
	}

	if (!bBaseCaptured)
	{
		CaptureBaseTransform();
	}

	// CombatGripOffset 表达在网格本地空间，转到父空间（插槽空间）后再叠加到相对位置上。
	// 始终用「基准 + 偏移」，绝不在当前值上累加，否则每帧都会越跑越远。
	const FVector LocalOffset = CombatGripOffset * InAlpha;
	const FVector ParentOffset = MeshComponent->GetRelativeRotation().RotateVector(LocalOffset);

	MeshComponent->SetRelativeLocation(BaseMeshRelativeLocation + ParentOffset);
}

void AArcaneVanguardWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 首次 Tick 时兜底记录基准（正常情况下 Character 在 Attach 后已经记录过）
	if (!bBaseCaptured)
	{
		CaptureBaseTransform();
	}

	if (!bAutoTrackOwnerCombat)
	{
		return; // alpha 由外部驱动，不需要自己插值
	}

	// 从 Owner 读战斗状态：Owner 是生成武器的 AArcaneVanguardCharacter
	const AArcaneVanguardCharacter* OwnerCharacter = Cast<AArcaneVanguardCharacter>(GetOwner());
	const float TargetAlpha = (OwnerCharacter && OwnerCharacter->IsInCombat()) ? 1.0f : 0.0f;

	if (!FMath::IsNearlyEqual(CurrentCombatAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
	{
			CurrentCombatAlpha = FMath::FInterpConstantTo(
        		CurrentCombatAlpha, TargetAlpha, DeltaSeconds, CombatGripInterpSpeed);
        
        	ApplyCombatAlpha(CurrentCombatAlpha);
	}


}
