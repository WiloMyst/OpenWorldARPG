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

/**
 * @class AMainGamePlayerController
 * @brief 轻量化控制器 (邮局原则 + Server RPC)。
 *
 * 职责边界：
 * - 接收输入并向下派发请求 (发信号)
 * - 角色切换通过 Server RPC 请求，服务器执行 Possess/UnPossess
 * - 管理输入映射上下文和 HUD
 *
 * 绝对禁止：
 * - FindComponentByClass 微操 Character 的私有组件
 * - HasMatchingGameplayTag 越权判断能否执行某动作
 * - 处理视觉表现层逻辑 (特效、动画)
 */
UCLASS()
class OPENWORLDARPG_API AMainGamePlayerController : public AOpenWorldARPGPlayerController
{
    GENERATED_BODY()

public:
    AMainGamePlayerController();

    // ==========================================
    // 角色切换 (Server RPC) - 供 TeamManagerSubsystem 等外部调用
    // ==========================================

    /** 客户端输入触发：请求切换角色（发送到服务器） */
    void HandleSwitchCharacterInput(int32 Index);

    /**
     * Server RPC：请求服务器执行角色切换。
     * 客户端只负责发送请求，服务器负责验证和执行。
     */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SwitchCharacter(int32 TargetIndex);

    /** 服务器执行角色切换的实际逻辑 */
    void Server_SwitchCharacter_Implementation(int32 TargetIndex);
    bool Server_SwitchCharacter_Validate(int32 TargetIndex);

    /** 客户端回调：服务器完成角色切换后，客户端执行本地表现 */
    UFUNCTION(Client, Unreliable)
    void Client_OnCharacterSwitched(int32 NewActiveIndex);

protected:
    virtual void BeginPlay() override;

    virtual void SetupInputComponent() override;

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
    // 配置项：Controller 专属 Gameplay Tags
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag InventoryUITag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag SprintStartEventTag;

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

    // ==========================================
    // 配置项：UI
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|UI")
    TSubclassOf<UUserWidget> MainHUDClass;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDInstance;

    // 替代蓝图 FlipFlop 节点的布尔状态
    bool bIsWalking = false;
    bool bIsPhysicsAnimDisabled = false;
};
