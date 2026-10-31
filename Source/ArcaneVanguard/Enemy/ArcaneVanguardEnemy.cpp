// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/ArcaneVanguardEnemy.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ArcaneVanguardAttributeSet.h"
#include "AbilitySystem/ArcaneVanguardCombatStatics.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "TimerManager.h"

AArcaneVanguardEnemy::AArcaneVanguardEnemy()
{
	// 敌人自己不 Tick：AI 状态机在 AArcaneVanguardEnemyAIController 里跑
	PrimaryActorTick.bCanEverTick = false;

	// 敌人自己用 SetActorRotation 转向目标，不能让 Controller 的 yaw 覆盖它。
	// 如果保持 true，AI 没在转 ControlRotation，SetActorRotation 每帧都会被拉回去。
	bUseControllerRotationYaw = false;

	// ---- 敌人不挡摄像机 ----
	// USpringArmComponent 的 ProbeChannel 默认是 ECC_Camera，而引擎两个预设对 Camera 都是 Block：
	//   CharacterMesh（骨骼网格体）：只忽略 Pawn
	//   Pawn（胶囊体）           ：只忽略 Visibility
	// 结果玩家一靠近敌人（尤其碰到双臂），摄像机会被探针顶回来。
	// 只忽略 Camera 通道：Visibility 仍然响应（瞄准/拾取正常），Pawn 仍然阻挡（玩家撞不进敌人）。
	// 注意：HandleDeath 里切 Ragdoll 预设会重置响应，所以那里还要再设一次。
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		// ---- 敌人用自己的独立通道（DefaultEngine.ini 里 Name="Enemy"）----
		// 玩家的攻击球扫改为按这条通道查（见 AArcaneVanguardCharacter::PerformAttackHitCheck），
		// 所以胶囊体必须挂到 Enemy 通道上，否则打不到。
		//
		// [!] 这里只改 Capsule，不改 Mesh：
		//     球扫命中胶囊体就够了（伤害只需要 Actor），
		//     而 Mesh 死亡时会切 Ragdoll 预设（ObjectType 会被重置成 PhysicsBody），
		//     改它等于每次死亡都要重新处理一遍，没必要。
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel2);

		// 显式 Block 自己这条通道。
		// ini 里 Enemy 的 DefaultResponse=Block 管的是"别的物体对 Enemy 通道"，
		// 而 SetCollisionObjectType 不会动响应表 —— 不补这一条，
		// 球扫按 ObjectType 过滤时会因为这条通道响应是默认值而漏掉自己。
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
	}
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// ---- 移动手感（转向交给 CharacterMovement，不在代码里硬转）----
	// bOrientRotationToMovement：角色自动朝「实际移动方向」平滑转身，
	//   RotationRate 决定转多快。540 度/秒 ≈ 0.17 秒转 90 度，
	//   石头人这种重甲怪有重量感但不迟钝。
	// 以前没有开这个，敌人是「侧着滑过去」+ 动画瞬切，看起来像瞬间转向。
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 540.f, 0.f);

		// 石头人不该像纸片一样瞬间起步/急停：降加速度、加刹车距离
		Movement->MaxAcceleration = 800.f;
		Movement->BrakingDecelerationWalking = 1200.f;
	}

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 和玩家同一套属性集，复用即可（Health / MaxHealth + PreAttributeChange 钳制）
	AttributeSet = CreateDefaultSubobject<UArcaneVanguardAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());

	// ---- 战斗参数默认值（身体侧）----
	// 判定球扫：从胶囊中心向前 2.5 米、半径 1.2 米（形状等价于一根胶囊）
	AttackSweepRadius = 120.f;
	AttackSweepRange = 250.f;
	AttackDamage = 15.f;           // 玩家 100 血 -> 7 下打死

	// 决策参数（追击距离 / 攻击距离 / 出招冷却）不在这里 —— 在 BT 资产的 Service / Decorator 里配

	// ---- 受击硬直默认值（AV-050）----
	// 默认打断：被打中就该中断挥拳，否则会出现"一边挨打一边把你打死"。
	bInterruptOnHit = true;
	// 0.6 秒：比常见的受击动画（0.5~1s）略短，保证动画还没播完就能恢复行动，
	// 避免硬直过长导致敌人像木桩。想让它完全跟着蒙太奇走就填 0。
	StaggerDuration = 0.6f;
	bIsStaggered = false;
	bStaggerPausedAI = false;
	// HitReactMontages 留空，由 BP_AV_Enemy_Golem 的 Combat|HitReact 指定
}

UAbilitySystemComponent* AArcaneVanguardEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AArcaneVanguardEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[Enemy] %s 没有 ASC，伤害系统无法工作"), *GetName());
		return;
	}

	// ⚠️ 敌人可能没有任何 Controller（D2 的静态靶子），PossessedBy 不会触发，
	// 所以 ActorInfo 必须在这里初始化。被 AI 接管后 PossessedBy 会再补一次。
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// ---- 血量覆盖 ----
	// 必须在 InitAbilityActorInfo 之后：属性集要挂到 ASC 上才能读写。
	// 顺序必须是「先 MaxHealth 后 Health」：
	//   PreAttributeChange 会把 Health 钳到 [0, MaxHealth]，
	//   反过来先设 Health=300 而 MaxHealth 仍是 100，会被直接钳成 100。
	if (AttributeSet && MaxHealthOverride > 0.0f)
	{
		AttributeSet->SetMaxHealth(MaxHealthOverride);
		AttributeSet->SetHealth(MaxHealthOverride);

		UE_LOG(LogTemp, Log, TEXT("[Enemy] %s 血量覆盖为 %.0f / %.0f"),
			*GetName(), AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
	}

	// 监听自己的 Health，降到 0 就死。
	// 用委托而不是在 AttributeSet 里 Cast 敌人 —— 属性集保持通用，
	// 玩家将来（D5）用同样的方式监听自己的死亡。
	AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UArcaneVanguardAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AArcaneVanguardEnemy::OnHealthChanged);
}

void AArcaneVanguardEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 被 AIController 接管后重新初始化（Owner/Avatar 不变，只是保证时机在 Possess 之后）
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AArcaneVanguardEnemy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f)
	{
		HandleDeath();
		return;
	}

	// 只有「掉血」才触发受击。
	// 用 NewValue < OldValue 而不是 !=：初始化时 Health 会被从 0 拉到满血，
	// 那种回调不能算挨打，否则敌人一出生就抽一下。
	if (Data.NewValue < Data.OldValue)
	{
		HandleHitReact();
	}
}

void AArcaneVanguardEnemy::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// 清掉硬直状态与计时器。
	// 否则尸体 ragdoll 之后 EndStagger 还会到点执行，去 ResumeLogic 一个已经瘫掉的 AI。
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(StaggerTimerHandle);
	}
	bIsStaggered = false;

	UE_LOG(LogTemp, Log, TEXT("[Enemy] %s 死亡"), *GetName());

	// 1) 切 ragdoll
	//    重甲怪物理倒地比播死亡动画更有说服力，也省掉一套死亡动画资产。
	//    物理资产（PA_*）在骨骼网格上配好即可，这里不用指定。
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		EnemyMesh->SetCollisionProfileName(TEXT("Ragdoll"));

		// 尸体不挡摄像机。
		// 引擎的 Ragdoll 预设只把 Pawn / Visibility 设成 Ignore，其余通道走"默认"= Block；
		// 而 USpringArmComponent 的 ProbeChannel 正是 ECC_Camera，
		// 于是玩家一走到尸体旁，摄像机臂被 ragdoll 挡住就往回缩（实测症状：镜头被推近）。
		// 必须在 SetCollisionProfileName 之后设置 —— 换预设会重置所有通道响应。
		// 只忽略 Camera：WorldStatic 仍然 Block，尸体照样躺在地上不会穿地。
		EnemyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		EnemyMesh->SetAllBodiesSimulatePhysics(true);
		EnemyMesh->SetSimulatePhysics(true);
		EnemyMesh->WakeAllRigidBodies();
		EnemyMesh->bBlendPhysics = true;
	}

	// 2) 停移动、停 AI，避免尸体继续追人
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
	}

	// 3) 关掉胶囊体碰撞（网格的 ragdoll 碰撞独立，不受影响）
	if (bDisableCapsuleCollisionOnDeath)
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4) 延迟销毁
	if (DestroyDelayAfterDeath > 0.0f)
	{
		SetLifeSpan(DestroyDelayAfterDeath);
	}
}

//////////////////////////////////////////////////////////////////////////
// 受击硬直（AV-050）

void AArcaneVanguardEnemy::HandleHitReact()
{
	if (bIsDead)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Enemy] %s 受击"), *GetName());

	// ---- 1) 打断当前动作 ----
	if (bInterruptOnHit)
	{
		// 1a) 停攻击蒙太奇。
		//     必须显式 Stop，不能指望 PlayAnimMontage 覆盖：
		//     攻击蒙太奇里带 AnimNotifyState 的攻击窗口，不 Stop 的话
		//     它会在受击动画底下继续结算伤害（表现为"明明被打断了还打中你"）。
		if (USkeletalMeshComponent* EnemyMesh = GetMesh())
		{
			if (UAnimInstance* AnimInstance = EnemyMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.1f);
			}
		}

		// 1b) 停移动，清掉残留 Velocity（否则会带着惯性滑一段）
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		// 1c) 通知 AI 进入硬直。
		//     优先写 Blackboard（BT 自己 Abort 战斗分支，规范做法）；
		//     Blackboard 里还没配 IsStaggered 键时退回 PauseLogic（兜底）。
		//     详见 NotifyAIStaggerStart 的注释。
		NotifyAIStaggerStart();
	}

	// ---- 2) 随机播一个受击蒙太奇 ----
	float MontageLength = 0.0f;

	if (HitReactMontages.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, HitReactMontages.Num() - 1);

		if (UAnimMontage* Montage = HitReactMontages[Index])
		{
			MontageLength = PlayAnimMontage(Montage);

			UE_LOG(LogTemp, Log, TEXT("[Enemy] %s 播放受击蒙太奇 %s（%.2fs）"),
				*GetName(), *Montage->GetName(), MontageLength);
		}
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Enemy] %s 未配置受击蒙太奇，只做硬直。"
			     "在 BP_AV_Enemy_Golem 的 Combat|HitReact 里指定。"), *GetName());
	}

	// ---- 3) 进入硬直 ----
	bIsStaggered = true;

	// StaggerDuration <= 0 表示跟随蒙太奇实际长度
	const float Duration = StaggerDuration > 0.0f ? StaggerDuration : MontageLength;

	if (Duration > 0.0f && GetWorld())
	{
		// SetTimer 对同一句柄是"覆盖"语义：连续挨打时硬直从最后一次命中重新计时，
		// 不会叠加成越来越长的眩晕。
		GetWorldTimerManager().SetTimer(StaggerTimerHandle, this,
			&AArcaneVanguardEnemy::EndStagger, Duration, false);
	}
	else
	{
		// 既没配硬直时长、也没配蒙太奇：立刻结束，避免永久卡在硬直里
		EndStagger();
	}
}

void AArcaneVanguardEnemy::EndStagger()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(StaggerTimerHandle);
	}

	bIsStaggered = false;

	// 恢复 AI：进入时走的是哪条路径，这里就按哪条收尾（见 NotifyAIStaggerEnd）
	NotifyAIStaggerEnd();

	UE_LOG(LogTemp, Log, TEXT("[Enemy] %s 硬直结束"), *GetName());
}

void AArcaneVanguardEnemy::NotifyAIStaggerStart()
{
	bStaggerPausedAI = false;

	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC)
	{
		return;
	}

	// ---- 首选：写 Blackboard，让 BT 自己切到受击分支 ----
	// BT 里配了 Decorator(IsStaggered == true) + Observer Aborts = Lower Priority 时，
	// 正在跑的 MoveTo / Attack 会被自动 Abort —— 不需要手动 StopMovement，
	// 也不需要 PauseLogic（后者会把整个 BT 的思考都停掉，粒度太粗）。
	if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
	{
		// [!] SetValueAsBool 在 C++ 里返回 void（Python 里才是 bool），
		//     且只有 FName 重载、没有 FKey 版本 ——
		//     所以判断"键是否存在"必须单独用 GetKeyID 查。
		const FBlackboard::FKey KeyID = BB->GetKeyID(TEXT("IsStaggered"));
		if (KeyID != FBlackboard::InvalidKey)
		{
			BB->SetValueAsBool(TEXT("IsStaggered"), true);

			UE_LOG(LogTemp, Verbose,
				TEXT("[Enemy] %s 已写入 Blackboard IsStaggered=true，交给 BT 处理"), *GetName());
			return;
		}
	}

	// ---- 兜底：BT 还没加 IsStaggered 键 ----
	// 退回"暂停整个 BT"的老办法，保证受击期间 AI 不会继续追人。
	// 一旦你在 BB_Enemy 里加上这个 Bool 键，就会自动走上面的首选路径，这段不再执行。
	bStaggerPausedAI = true;

	AIC->StopMovement();

	if (UBrainComponent* Brain = AIC->GetBrainComponent())
	{
		Brain->PauseLogic(TEXT("HitReact"));
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("[Enemy] %s 的 Blackboard 里没有 IsStaggered 键，退回 PauseLogic。"
		     "建议在 BB_Enemy 加一个 Bool 键 IsStaggered，让 BT 接管硬直状态。"), *GetName());
}

void AArcaneVanguardEnemy::NotifyAIStaggerEnd()
{
	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC)
	{
		return;
	}

	if (bStaggerPausedAI)
	{
		// 兜底模式：恢复被暂停的 BT。
		// 只在真的暂停过时调 ResumeLogic —— 对没暂停的 Brain 反复 Resume 会打乱内部计数。
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->ResumeLogic(TEXT("HitReact"));
		}

		bStaggerPausedAI = false;
	}
	else
	{
		// Blackboard 模式：清键即可，BT 下一帧自己会回到战斗分支。
		// 键不存在时 SetValueAsBool 返回 false，这里不关心结果。
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
		{
			BB->SetValueAsBool(TEXT("IsStaggered"), false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// AI 执行层
// 「什么时候追、什么时候打」在 AArcaneVanguardEnemyAIController 里，
// 这里只负责「怎么打」和「怎么记状态」。

void AArcaneVanguardEnemy::BeginAttack()
{
	if (!AttackMontage)
	{
		return;
	}

	// 新的一刀：清空命中记录，允许重新命中上次打过的目标
	HitActorsThisAttack.Reset();

	PlayAnimMontage(AttackMontage);
}

int32 AArcaneVanguardEnemy::PerformAttackHitCheck()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// 与玩家一致：从胶囊体中心起扫（不是脚底），球扫形状等价于一根胶囊
	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorForwardVector() * AttackSweepRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AVEnemyAttackHitCheck), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(AttackSweepRadius),
		QueryParams);

	int32 HitCount = 0;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || HitActor == this)
		{
			continue;
		}

		// 同一刀内同一目标只结算一次
		const TWeakObjectPtr<AActor> HitKey(HitActor);
		if (HitActorsThisAttack.Contains(HitKey))
		{
			continue;
		}
		HitActorsThisAttack.Add(HitKey);

		// ADR-005：伤害唯一入口。敌人打玩家和玩家打敌人走同一条路
		const float Applied = UArcaneVanguardCombatStatics::ApplyDamage(HitActor, AttackDamage, this, nullptr);
		if (Applied > 0.f)
		{
			++HitCount;
			UE_LOG(LogTemp, Log, TEXT("[Enemy] %s 命中 %s，伤害 %.1f"),
				*GetName(), *GetNameSafe(HitActor), Applied);
		}
	}

	return HitCount;
}
