// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGPlayerController.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "GameplayPlayerController.generated.h"

class UUserWidget;
class UInputMappingContext;
class UInputAction;
class APlayerCharacter;

/**
 * 通用玩法控制器。只接收输入并向下派发，不微操 Character 组件。
 * 大世界/副本专属逻辑在子类扩展。
 */
UCLASS()
class OPENWORLDARPG_API AGameplayPlayerController : public AOpenWorldARPGPlayerController
{
    GENERATED_BODY()

public:
    AGameplayPlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

    // --- 视角输入 ---
    void Input_CameraReset();
    void Input_CameraZoom(const FInputActionValue& Value);
    void Input_Look(const FInputActionValue& Value);  // 含回正打断检测

    // --- 移动输入 ---
    void Input_Move(const FInputActionValue& Value);
    void Input_MoveCompleted(const FInputActionValue& Value);
    void Input_Walk();
    void Input_ShiftAction();
    void Input_ShiftReleased();
    void Input_JumpStart();
    void Input_JumpStop();

    // --- 交互 ---
    void Input_PickUp();

    // --- 队伍切换 ---
    void Input_Switch1() { HandleSwitchCharacterInput(0); }
    void Input_Switch2() { HandleSwitchCharacterInput(1); }
    void Input_Switch3() { HandleSwitchCharacterInput(2); }
    void Input_Switch4() { HandleSwitchCharacterInput(3); }

    // --- 战斗输入 ---
    void Input_NormalAttack();
    void Input_NormalAttackReleased();
    void Input_HeavyAttack();
    void Input_PlungeAttack();
    void Input_AimAttack();
    void Input_AimAttackReleased();
    void Input_Aim();

public:
    /** 全屏 UI 菜单调用，控制大世界 HUD 可见性 */
    void SetMainHUDVisible(bool bIsVisible);

    float GetPlayerDesiredArmLength() const { return PlayerDesiredArmLength; }
    bool IsSprintActionHeld() const { return bIsSprintActionHeld; }
    bool IsNormalAttackHeld() const { return bIsNormalAttackHeld; }

    // --- 角色切换 Server RPC ---

    void HandleSwitchCharacterInput(int32 Index);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SwitchCharacter(int32 TargetIndex);
    void Server_SwitchCharacter_Implementation(int32 TargetIndex);
    bool Server_SwitchCharacter_Validate(int32 TargetIndex);

    /** 编队保存：服务器销毁旧队伍，按 NewTeamTags 重新 Spawn，Possess ActiveIndex */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ApplyTeamChanges(const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex);
    void Server_ApplyTeamChanges_Implementation(const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex);
    bool Server_ApplyTeamChanges_Validate(const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex);

    /** 同步冲刺键状态到服务器 */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetSprintActionHeld(bool bHeld);
    void Server_SetSprintActionHeld_Implementation(bool bHeld);
    bool Server_SetSprintActionHeld_Validate(bool bHeld);

    UFUNCTION(Client, Unreliable)
    void Client_OnCharacterSwitched(int32 NewActiveIndex);

    /** GA_SwapOut 退场完成回调，执行 UnPossess → Possess → GA_SwapIn */
    UFUNCTION()
    void OnSwapOutCompleted(APlayerCharacter* SwappedOutCharacter, FTransform SwapTransform);

protected:
    // --- 输入资产 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Look;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_CameraReset;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Jump;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Sprint;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_Walk;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input")
    TObjectPtr<UInputAction> IA_PickUp;

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
    TObjectPtr<UInputAction> IA_CameraZoom;

    // --- Tags ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag DashEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SprintStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag WalkStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag WalkStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag NormalAttackEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag HeavyAttackEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag PlungeAttackEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimAttackStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimAttackStopEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimStartEventTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Combat")
    FGameplayTag AimStopEventTag;

    /** 目标角色拥有这些标签时禁止切换 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|Switch")
    FGameplayTagContainer PreventSwitchTags;

    // --- 镜头回正 ---

    /** 镜头回正插值速度 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float CameraResetInterpSpeed = 10.0f;

    /** 目标镜头 pitch 角度 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float TargetResetPitch = -15.0f;

    /** 镜头回正中断阈值 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float CameraResetInterruptThreshold = 0.05f;

    /** 镜头回正容差 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Reset")
    float CameraResetTolerance = 1.0f;

private:
    bool bIsWalking = false;
    bool bIsAiming = false;
    bool bIsSprintActionHeld = false;
    bool bIsNormalAttackHeld = false;
    bool bIsResettingCamera = false;

    /** 待切换目标角色索引 */
    int32 PendingSwapTargetIndex = -1;

    /** 出场 Transform，激活 GA_SwapInBase 前设置到角色上 */
    FTransform PendingSwapTransform;

    // --- 镜头缩放 ---

    /** 最小镜头距离 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom")
    float MinCameraDistance = 150.0f;

    /** 最大镜头距离 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom")
    float MaxCameraDistance = 800.0f;

    /** 镜头缩放步长 */
    UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom")
    float CameraZoomStep = 50.0f;

    /** Normal 状态下的目标镜头距离（切换角色后保持不变） */
    UPROPERTY(VisibleAnywhere, Category = "Camera|Zoom")
    float PlayerDesiredArmLength = 400.0f;
};
