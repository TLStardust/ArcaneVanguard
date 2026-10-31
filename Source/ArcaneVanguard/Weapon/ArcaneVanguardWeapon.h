// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcaneVanguardWeapon.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * 武器 Actor 基类。
 *
 * 设计意图：武器作为独立 Actor 存在（而不是角色的子组件），
 * 由角色 Spawn 出来后 Attach 到骨骼插槽上。这样武器可以被单独
 * 丢弃、拾取、替换、切换皮肤，而不需要改动角色类。
 *
 * [!] 校准位置的限制（2026-10-26 实测）：
 *   角色在**插槽的世界变换处** Spawn 武器，再用 AttachToComponent(KeepWorldTransform)
 *   附着 => 相对变换被强制拉回 Identity。**所以在蓝图里改 MeshComponent 的相对变换
 *   在运行时是无效的**（会被这次 Attach 冲掉）。
 *   校准只有两条路：改骨架上的插槽，或走本类的 CombatGripOffset。
 *
 * 战斗握持偏移（M1）：
 *   长枪/法杖类武器在待机时握在杖身 2/3 处，战斗姿态端起后前段会显得太短。
 *   本类提供「战斗握持偏移」：进入战斗时沿杖身轴把武器整体前移一段，
 *   使手握点相对滑向杖尾，前段随之变长。偏移量按 alpha 插值。
 *   [!] 插值速度（CombatGripInterpSpeed）应与 AnimBP 的 CombatAlpha 过渡时长匹配，
 *      否则会出现"人已经端好姿势、武器还在挪"（或反之）的割裂感。
 */
UCLASS(Blueprintable)
class ARCANEVANGUARD_API AArcaneVanguardWeapon : public AActor
{
	GENERATED_BODY()

public:
	AArcaneVanguardWeapon();

	/** 武器网格组件。根组件，附着到骨骼插槽后由它的相对变换决定最终姿态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 武器网格资产。在蓝图子类里指定；OnConstruction 时同步给 MeshComponent */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMesh> MeshAsset;

	// ---------------------------------------------------------------------
	// 战斗握持偏移
	// ---------------------------------------------------------------------

	/**
	 * 战斗状态下的握持偏移，单位 cm，表达在「网格组件本地空间」。
	 *
	 * 沿杖身滑动请改 Z 分量（法杖网格长轴是 Z，包围盒 25×22×194）：
	 *   正值 = 武器整体朝杖尖方向平移 => 手握点相对滑向杖尾 => 前段变长
	 *   负值 = 反向（前段变短）
	 * 若发现方向反了，把 Z 取负即可；横向分量可用于战斗时稍微避开身体。
	 *
	 * alpha = 0（待机）时偏移为 0，alpha = 1（战斗）时偏移取本值。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|CombatGrip")
	FVector CombatGripOffset;

	/**
	 * 偏移插值速度，单位「alpha / 秒」。
	 * 1.0 = 1 秒走完，3.0 ≈ 0.33 秒，10.0 ≈ 0.1 秒。
	 * 与 AnimBP 的 CombatAlpha 过渡时长对齐时：speed = 1 / 过渡秒数。
	 *
	 * [!] 当前构造函数默认值是 10.0（≈ 0.1 秒走完）。
	 *    如果 AnimBP 的 CombatAlpha 仍是 1 秒过渡，两边会明显不同步 —— 改前先核对。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|CombatGrip",
		meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "20.0"))
	float CombatGripInterpSpeed;

	/** 是否自动跟随 Owner 的战斗状态（AArcaneVanguardCharacter::IsInCombat） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|CombatGrip")
	bool bAutoTrackOwnerCombat;

	// ---------------------------------------------------------------------
	// 碰撞（AV-047 脱手掉落需要）
	// ---------------------------------------------------------------------

	/**
	 * 是否由 C++ 在 OnConstruction 里统一配置武器碰撞。默认开。
	 *
	 * 开启时的配置：QueryAndPhysics + PhysicsBody + 全通道 Block，
	 * 但忽略 Pawn / Visibility / Camera（不卡角色移动、不挡攻击球扫、不推摄像机）。
	 *
	 * **关掉后完全沿用蓝图里 MeshComponent 的碰撞设置**，方便自己在蓝图里精调。
	 * 但必须保证「碰撞启用」是 **查询和物理** 或 **纯物理**，
	 * 否则脱手时开物理模拟会失败、法杖会穿过地面。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Collision")
	bool bSetupCollisionInConstruction;

	/**
	 * 脱手时的随机角冲量范围（度/秒）。三个轴各自在 ±Range 内随机取值。
	 *
	 * 0      = 完全不翻滚，直直落下
	 * 10（默认）= 轻微翻滚，落地姿态不呆板
	 * 40 以上  = 明显在空中打转，法杖长 194cm，杖尖线速度会很夸张
	 *
	 * 只影响旋转，不影响掉落位置；法杖被"甩飞"是另一回事（继承手部速度）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Collision",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float DropAngularImpulseRange;

	/**
	 * 手动设置战斗握持 alpha（0 = 待机握法，1 = 战斗握法）。
	 * bAutoTrackOwnerCombat 为 false 时由外部（AnimBP / 战斗系统）驱动。
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|CombatGrip")
	void SetCombatAlpha(float InAlpha);

	/** 当前战斗握持 alpha（插值后的实际值） */
	UFUNCTION(BlueprintPure, Category = "Weapon|CombatGrip")
	float GetCombatAlpha() const { return CurrentCombatAlpha; }

	/**
	 * 重新记录"基准相对位置"。
	 * 武器被 Attach 到插槽后调用（Character 在附着完成后会自动调一次）；
	 * 运行时若手动移动了武器需要重新基准，也可调用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|CombatGrip")
	void CaptureBaseTransform();

	// ---------------------------------------------------------------------
	// 脱手掉落（AV-047 玩家死亡）
	// ---------------------------------------------------------------------

	/**
	 * 让武器变成可自由落体的物理体。
	 *
	 * 调用前必须已经 `DetachFromActor(KeepWorldTransform)` —— 本函数不负责脱手，
	 * 只负责「从被握持的装饰物」切换成「有重量的物体」。
	 *
	 * 做三件事：
	 *   1. 关掉 Tick：握持插值每帧会改 Mesh 的相对变换，和物理模拟打架会导致抖动/瞬移；
	 *   2. 开 QueryAndPhysics + PhysicsActor 预设：构造函数里是 NoCollision（防止挡角色），
	 *      不开碰撞的话脱手后武器会悬在空中不掉；
	 *   3. 加一点随机角速度，避免每次都以完全相同的姿态落地（看着像"摆上去的"）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EnablePhysicsDrop();

protected:
	/** 在编辑器与运行时都会执行，用于把 MeshAsset 同步到组件上 */
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 配置武器碰撞：QueryAndPhysics + PhysicsBody + 全通道 Block，
	 * 但忽略 Pawn / Visibility / Camera（不挡角色、不挡攻击判定、不挡摄像机）。
	 *
	 * [!] 为什么必须在 OnConstruction 里做、而不是等到脱手时才改：
	 *     组件的物理体在 Spawn 时就按当时的碰撞配置建好了，
	 *     之后再调 SetCollisionEnabled / RecreatePhysicsState 都不一定能让它重生
	 *     （2026-10-30 实测：死亡时改碰撞，GetCollisionEnabled() 始终返回 0，
	 *       GetMass() 停在默认 1.0，法杖穿过地面）。
	 *     OnConstruction 在物理状态创建之前执行，此时配置才会被真正采纳。
	 */
	void ApplyWeaponCollisionSetup();

	/** 把 alpha 应用到 MeshComponent 的相对位置 */
	void ApplyCombatAlpha(float InAlpha);

	/** 基准相对位置（CaptureBaseTransform 时记录，偏移始终叠加在它上面，避免逐帧累加） */
	FVector BaseMeshRelativeLocation;

	/** 是否已经记录过基准 */
	bool bBaseCaptured;

	/** 插值后的战斗握持 alpha */
	float CurrentCombatAlpha;
};
