// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ArcaneVanguardCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UAnimSequence;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AArcaneVanguardCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/**
	 * 武器网格。挂在角色 Mesh 的手部骨骼/插槽上。
	 * 巫师骨架（SKE_Wizard）**没有任何 socket**，所以默认直接用骨骼名 hand_r。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WeaponMesh;

	/**
	 * 武器挂点名称。可以是插槽名，也可以是骨骼名（本工程用骨骼名 hand_r）。
	 * 填 None 表示不挂到任何挂点，退回到 Mesh 根节点。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	FName WeaponAttachSocket;

	/**
	 * 武器相对挂点的变换。用来纠正握持姿势（法杖通常是旋转 + 平移）。
	 * 在蓝图里可视化调，不用改代码。
	 * 注意：开启 bKeepWeaponUpright 后，本值不再生效（每帧由 Tick 覆盖）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	FTransform WeaponRelativeTransform;

	/**
	 * 是否强制武器保持竖直（法杖水晶朝上）。
	 *
	 * 背景：这套动画是为空手做的，手腕在待机与施法之间会转 172°，
	 * 刚性挂载会导致法杖端对端翻转。开启本项后，每帧重算：
	 *   - 握点（法杖局部 (0,0,WeaponGripOffset)）始终贴合挂点骨骼
	 *   - 朝向始终竖直，yaw 跟随角色
	 * 代价：前刺类动作法杖不会跟随前倾。拿到持杖动画后可关闭本项。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	bool bKeepWeaponUpright;

	/** 握点距武器原点（杖底）的距离，沿武器局部 +Z。96 约为杖身中点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	float WeaponGripOffset;

	/**
	 * 调试用：按数字键 1~5 依次播放这些动画，播完自动回到 AnimBP 状态机。
	 * 数组顺序对应按键顺序（Index 0 = 键 1）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug|Animation", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UAnimSequence>> DebugAnimations;

	/** 是否启用调试按键（1~5）。发布前记得关掉。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug|Animation", meta = (AllowPrivateAccess = "true"))
	bool bEnableDebugAnimKeys;

	/**
	 * 播放这些动画时自动隐藏武器（按资产名匹配）。
	 *
	 * 背景：本工程的技能动画来自 Mixamo，录制时手上没有武器。
	 * 像 Magic_Attack 这种"双手向前推"的动作，法杖插在手里会明显穿模，
	 * 收起武器比让它穿模更合理。
	 *
	 * 同时支持 Montage 与 SingleNode 播放；动画结束后自动恢复显示。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	TArray<FName> WeaponHiddenAnimNames;

	/**
	 * 播放这些动画时武器脱手掉落（开启物理模拟，自然落地）。
	 * 默认：Anim_Wizard_Death_Forward。动画结束后自动收回手上。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	TArray<FName> WeaponDropAnimNames;

public:
	AArcaneVanguardCharacter();
	

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** 把武器挂到 WeaponAttachSocket 指定的骨骼/插槽；找不到时打警告并退回 Mesh 根 */
	void AttachWeapon();

	/** bKeepWeaponUpright 开启时，每帧把武器摆成「握点贴手 + 朝向竖直」 */
	void UpdateWeaponUprightTransform();

	/** 按当前播放的动画决定武器显隐（见 WeaponHiddenAnimNames） */
	void UpdateWeaponVisibilityFromAnimation();

	/** 按当前播放的动画决定武器是否脱手掉落（见 WeaponDropAnimNames） */
	void UpdateWeaponDropFromAnimation();

	/** 取当前正在播放的动画/Montage 资产名；取不到返回 NAME_None */
	FName GetCurrentAnimationName() const;

	/** 调试动画播完后切回 AnimBP 状态机 */
	void UpdateDebugAnimationState();

	/** 数字键 1~5 的处理函数 */
	void DebugPlayAnim1();
	void DebugPlayAnim2();
	void DebugPlayAnim3();
	void DebugPlayAnim4();
	void DebugPlayAnim5();

private:
	/** 上一帧是否因动画规则隐藏了武器，避免每帧重复 SetVisibility */
	bool bWeaponHiddenByAnimRule = false;

	/** 正在播放的调试动画索引，INDEX_NONE 表示没有 */
	int32 ActiveDebugAnimIndex = INDEX_NONE;

	/** 武器是否已脱手掉落（掉落期间不参与显隐规则与竖直约束） */
	bool bWeaponDropped = false;

public:
	// AActor interface
	virtual void Tick(float DeltaSeconds) override;
			

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay();

public:
	/**
	 * 显示/隐藏武器。供动画通知（UAnimNotify_HideWeapon / ShowWeapon）调用，
	 * 用于在"这套动画本来就没有武器"的动作里把法杖收起来，避免穿模。
	 */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void SetWeaponVisible(bool bVisible);

	/** 武器当前是否可见 */
	UFUNCTION(BlueprintPure, Category = Weapon)
	bool IsWeaponVisible() const;

	/** 调试：播放 DebugAnimations[Index]，播完自动回状态机 */
	UFUNCTION(BlueprintCallable, Category = "Debug|Animation")
	void DebugPlayAnimation(int32 Index);

	/** 调试：立即中止并回到 AnimBP 状态机 */
	UFUNCTION(BlueprintCallable, Category = "Debug|Animation")
	void DebugStopAnimation();

	/** 武器脱手：解除附着并开启物理模拟（掉在地上） */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void DropWeapon();

	/** 把武器收回手上：关闭物理、重新附着到挂点骨骼 */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void RestoreWeapon();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	/** Returns WeaponMesh subobject **/
	FORCEINLINE class UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
};

