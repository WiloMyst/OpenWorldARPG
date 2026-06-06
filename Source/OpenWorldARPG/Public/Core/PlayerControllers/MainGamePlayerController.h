// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGPlayerController.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "InputActionValue.h" // 增强输入取值所需
#include "MainGamePlayerController.generated.h"

class UNiagaraSystem;
class UUserWidget;
class UInputMappingContext;
class UInputAction;

UCLASS()
class OPENWORLDARPG_API AMainGamePlayerController : public AOpenWorldARPGPlayerController
{
    GENERATED_BODY()

public:
    AMainGamePlayerController();

protected:
    virtual void BeginPlay() override;

    // 覆盖输入绑定核心函数
    virtual void SetupInputComponent() override;

    UFUNCTION()
    void HandleSwitchCharacter(int32 TargetIndex);

protected:
    // ==========================================
    // 增强输入系统 (Enhanced Input) 绑定回调
    // ==========================================

    void Input_Move(const FInputActionValue& Value);
    void Input_MoveCompleted(const FInputActionValue& Value);

    void Input_JumpStart();
    void Input_JumpStop();

    void Input_SprintStart();
    void Input_SprintStop();

    void Input_Walk();
    void Input_Glide();
    void Input_Hook();
    void Input_PickUp();
    void Input_ToggleInventory();
    void Input_ClothSimulation();

    // 队伍切换
    void Input_Switch1() { HandleSwitchCharacterInput(0); }
    void Input_Switch2() { HandleSwitchCharacterInput(1); }
    void Input_Switch3() { HandleSwitchCharacterInput(2); }
    void Input_Switch4() { HandleSwitchCharacterInput(3); }
    void HandleSwitchCharacterInput(int32 Index);

    void Input_NormalAttack();
    void Input_HeavyAttack();
    void Input_PlungeAttack();
    void Input_AimAttack();
    void Input_Aim();

protected:
    // ==========================================
    // 配置项：输入资产 (Input Actions)
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Jump;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Sprint;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Walk;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Glide;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Hook;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_PickUp;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_ToggleInventory;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_ClothSimulation;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Switch_1;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Switch_2;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Switch_3;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Switch_4;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|Combat")
    TObjectPtr<UInputAction> IA_NormalAttack;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|Combat")
    TObjectPtr<UInputAction> IA_HeavyAttack;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|Combat")
    TObjectPtr<UInputAction> IA_PlungeAttack;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|Combat")
    TObjectPtr<UInputAction> IA_AimAttack;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|Combat")
    TObjectPtr<UInputAction> IA_Aim;

    // ==========================================
    // 配置项：Gameplay Tags (拒绝硬编码)
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag UncontrollableStateTag; // Character.State.Uncontrollable

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ClimbingStateTag;       // Character.State.Climbing

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag GlidingStateTag;        // Character.State.InAir.Gliding

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag InventoryUITag;         // UI.Menu.Inventory

    // --- Events ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag JumpStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag JumpStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SprintStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SprintStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag WalkStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag WalkStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag GlideStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag GlideStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag HookStartEventTag;

    // --- 状态标签 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimingStateTag; // 对应蓝图：Character.State.Aiming

    // --- 事件标签 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag NormalAttackEventTag; // 对应：Input.Action.Attack.Normal

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag HeavyAttackEventTag;  // 对应：Input.Action.Attack.Heavy

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag PlungeAttackEventTag; // 对应：Input.Action.Attack.Plunge

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimAttackEventTag;    // 对应：Input.Action.Attack.AimShot

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimStartEventTag;     // 对应：Input.Action.Aim.Start

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimStopEventTag;      // 对应：Input.Action.Aim.Stop

    // ==========================================
    // 原有的各种配置项保留...
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|UI")
    TSubclassOf<UUserWidget> MainHUDClass;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TObjectPtr<UNiagaraSystem> CharacterSwapFX;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    FVector SwapFXLocationOffset = FVector(0.0f, 0.0f, -100.0f);

    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    FVector SwapFXScale = FVector(0.5f, 0.5f, 0.5f);

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTagContainer PreventSwitchTags;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    TArray<TEnumAsByte<EMovementMode>> AllowedMovementModes;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDInstance;

    // 替代蓝图 FlipFlop 节点的布尔状态
    bool bIsWalking = false;
    bool bIsPhysicsAnimDisabled = false;
};