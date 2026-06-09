// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGPlayerController.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "MainGamePlayerController.generated.h"

class UUserWidget;
class UInputMappingContext;
class UInputAction;
class APlayerCharacter;

/**
 * 轻量化控制器。只接收输入并向下派发请求，不微操 Character 组件。
 */
UCLASS()
class OPENWORLDARPG_API AMainGamePlayerController : public AOpenWorldARPGPlayerController
{
    GENERATED_BODY()

public:
    AMainGamePlayerController();

    // --- 角色切换 (Server RPC) ---

    void HandleSwitchCharacterInput(int32 Index);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SwitchCharacter(int32 TargetIndex);

    void Server_SwitchCharacter_Implementation(int32 TargetIndex);
    bool Server_SwitchCharacter_Validate(int32 TargetIndex);

    UFUNCTION(Client, Unreliable)
    void Client_OnCharacterSwitched(int32 NewActiveIndex);

    /**
     * GA_SwapOut 退场完成回调。由 PlayerCharacter::OnSwapOutCompleted 委托触发。
     * 在服务器端执行 UnPossess → Possess → 激活 GA_SwapIn 的流水线。
     */
    UFUNCTION()
    void OnSwapOutCompleted(APlayerCharacter* SwappedOutCharacter, FTransform SwapTransform);

protected:
    virtual void BeginPlay() override;

    virtual void SetupInputComponent() override;

protected:
    // --- 输入绑定回调 ---

    void Input_Move(const FInputActionValue& Value);
    void Input_MoveCompleted(const FInputActionValue& Value);

    void Input_JumpStart();
    void Input_JumpStop();

    void Input_ShiftAction();
    void Input_ShiftReleased();

    void Input_Walk();
    void Input_Hook();
    void Input_PickUp();
    void Input_ToggleInventory();
    void Input_ClothSimulation();

    // 队伍切换
    void Input_Switch1() { HandleSwitchCharacterInput(0); }
    void Input_Switch2() { HandleSwitchCharacterInput(1); }
    void Input_Switch3() { HandleSwitchCharacterInput(2); }
    void Input_Switch4() { HandleSwitchCharacterInput(3); }

    void Input_NormalAttack();
    void Input_HeavyAttack();
    void Input_PlungeAttack();
    void Input_AimAttack();
    void Input_Aim();

protected:
    // --- 配置：输入资产 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Jump;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Sprint;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Walk;

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

    // --- 配置：Tags ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag InventoryUITag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DashEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SprintStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag WalkStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag WalkStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag HookStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag NormalAttackEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag HeavyAttackEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag PlungeAttackEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimAttackEventTag;

    // --- 配置：UI ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|UI")
    TSubclassOf<UUserWidget> MainHUDClass;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDInstance;

    bool bIsWalking = false;
    bool bIsPhysicsAnimDisabled = false;

    /** 缓存的目标切换角色索引，GA_SwapOut 完成后使用 */
    int32 PendingSwapTargetIndex = -1;
};
