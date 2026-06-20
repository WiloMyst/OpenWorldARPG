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

    // --- Dash/Sprint 动作键状态 ---

    /** 返回玩家是否正在按住 Dash/Sprint 动作键（不关心具体绑定的物理按键） */
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool IsSprintActionHeld() const { return bIsSprintActionHeld; }

    /** Server RPC：客户端同步 bIsSprintActionHeld 到服务器，确保多端一致 */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetSprintActionHeld(bool bHeld);

    void Server_SetSprintActionHeld_Implementation(bool bHeld);
    bool Server_SetSprintActionHeld_Validate(bool bHeld);

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

    virtual void PlayerTick(float DeltaTime) override;

    // --- 镜头平滑回正 ---

    /** 按下镜头回正键时触发 */
    void Input_CameraReset();

    /** 视角输入处理：含回正打断检测 */
    void Input_Look(const FInputActionValue& Value);

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

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_CameraReset;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Look;

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

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimEventTag;

    // --- 配置：镜头回正 ---

    /** 镜头回正插值速度 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float CameraResetInterpSpeed = 10.0f;

    /** 回正时的目标俯角（黄金俯角，避免上下坡时看天/看地） */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float TargetResetPitch = -15.0f;

    /** 用于打断回正的输入死区阈值 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float CameraResetInterruptThreshold = 0.05f;

    /** 到达目标角度的容差阈值（度） */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float CameraResetTolerance = 1.0f;

    // --- 配置：UI ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|UI")
    TSubclassOf<UUserWidget> MainHUDClass;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDInstance;

    bool bIsWalking = false;
    bool bIsPhysicsAnimDisabled = false;

    /** 精准记录玩家是否正在按住 Dash/Sprint 动作键（Enhanced Input 无关物理按键） */
    bool bIsSprintActionHeld = false;

    /** 镜头是否正在自动回正 */
    bool bIsResettingCamera = false;

    /** 缓存的目标切换角色索引，GA_SwapOutBase 完成后使用 */
    int32 PendingSwapTargetIndex = -1;

    /** 缓存的出场 Transform，由 Controller 在激活 GA_SwapInBase 前设置到角色上 */
    FTransform PendingSwapTransform;
};
