// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/MainGamePlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PlayerStates/MainGamePlayerState.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

AMainGamePlayerController::AMainGamePlayerController()
{
}

void AMainGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 加载输入映射上下文 (Mapping Context)
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    if (MainHUDClass)
    {
        MainHUDInstance = CreateWidget<UUserWidget>(this, MainHUDClass);
        if (MainHUDInstance) MainHUDInstance->AddToViewport();
    }

    // 注意：不再绑定 TeamManager->OnRequestCharacterSwitch
    // 角色切换现在通过 Server RPC 流程：输入 → HandleSwitchCharacterInput → Server_SwitchCharacter
}

void AMainGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // 移动
        if (IA_Move)
        {
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Move);
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Completed, this, &AMainGamePlayerController::Input_MoveCompleted);
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Canceled, this, &AMainGamePlayerController::Input_MoveCompleted);
        }

        // 跳跃
        if (IA_Jump)
        {
            EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_JumpStart);
            EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AMainGamePlayerController::Input_JumpStop);
        }

        // 冲刺/闪避：按下 Shift 触发 GA_Dash，GA_Dash 结束时自动判断是否过渡到 GA_Sprint
        if (IA_Sprint)
        {
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_ShiftAction);
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AMainGamePlayerController::Input_ShiftReleased);
        }

        // 行走 (FlipFlop)
        if (IA_Walk) EnhancedInputComponent->BindAction(IA_Walk, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Walk);
        // 钩索
        if (IA_Hook) EnhancedInputComponent->BindAction(IA_Hook, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Hook);
        // 拾取
        if (IA_PickUp) EnhancedInputComponent->BindAction(IA_PickUp, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_PickUp);
        // 打开背包
        if (IA_ToggleInventory) EnhancedInputComponent->BindAction(IA_ToggleInventory, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_ToggleInventory);
        // 头发布料模拟 (FlipFlop)
        if (IA_ClothSimulation) EnhancedInputComponent->BindAction(IA_ClothSimulation, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_ClothSimulation);

        // 队伍切换 1~4
        if (IA_Switch_1) EnhancedInputComponent->BindAction(IA_Switch_1, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Switch1);
        if (IA_Switch_2) EnhancedInputComponent->BindAction(IA_Switch_2, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Switch2);
        if (IA_Switch_3) EnhancedInputComponent->BindAction(IA_Switch_3, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Switch3);
        if (IA_Switch_4) EnhancedInputComponent->BindAction(IA_Switch_4, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Switch4);

        if (IA_NormalAttack) EnhancedInputComponent->BindAction(IA_NormalAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_NormalAttack);
        if (IA_HeavyAttack)  EnhancedInputComponent->BindAction(IA_HeavyAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_HeavyAttack);
        if (IA_AimAttack)    EnhancedInputComponent->BindAction(IA_AimAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_AimAttack);
        if (IA_Aim)          EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_Aim);
        if (IA_PlungeAttack) EnhancedInputComponent->BindAction(IA_PlungeAttack, ETriggerEvent::Started, this, &AMainGamePlayerController::Input_PlungeAttack);
    }
}

// --- 角色切换 (Server RPC) ---

void AMainGamePlayerController::HandleSwitchCharacterInput(int32 Index)
{
    // 客户端：只发送 RPC 请求到服务器，不做任何本地切换逻辑
    Server_SwitchCharacter(Index);
}

bool AMainGamePlayerController::Server_SwitchCharacter_Validate(int32 TargetIndex)
{
    // 基础验证：索引必须 >= 0（队伍上限由服务器端判定）
    return TargetIndex >= 0 && TargetIndex < 4; // 最多4人队伍
}

void AMainGamePlayerController::Server_SwitchCharacter_Implementation(int32 TargetIndex)
{
    // 服务器端：验证 + 通过 GA 流水线执行角色切换

    AMainGamePlayerState* MyPlayerState = GetPlayerState<AMainGamePlayerState>();
    if (!MyPlayerState) return;

    // 1. 获取当前控制的角色
    APlayerCharacter* OldCharacter = Cast<APlayerCharacter>(GetPawn());
    if (!OldCharacter) return;

    // 2. 旧角色状态拦截 (交由 Character 内部判定)
    if (!OldCharacter->CanSwapOut())
    {
        UE_LOG(LogTemp, Log, TEXT("Server_SwitchCharacter: 旧角色状态不允许切换！"));
        return;
    }

    // 3. 从 PlayerState 获取队伍角色实例
    APlayerCharacter* NewCharacter = MyPlayerState->GetTeamCharacterByIndex(TargetIndex);
    if (!NewCharacter || NewCharacter == OldCharacter) return;

    // 4. 新角色状态拦截 (交由 Character 内部判定)
    if (!NewCharacter->CanSwapIn())
    {
        UE_LOG(LogTemp, Log, TEXT("Server_SwitchCharacter: 新角色状态不允许切换！"));
        return;
    }

    // 5. 缓存目标索引，供 GA_SwapOut 完成回调使用
    PendingSwapTargetIndex = TargetIndex;

    // 6. 绑定旧角色的退场完成委托（如果尚未绑定）
    //    GA_SwapOut::EndAbility 会通过 NotifySwapOutCompleted 广播此委托
    OldCharacter->OnSwapOutCompleted.AddDynamic(this, &AMainGamePlayerController::OnSwapOutCompleted);

    // 7. 激活 GA_SwapOut（退场技能）
    //    如果没有配置 GA 类，降级使用旧的直接切换逻辑
    if (OldCharacter->SwapOutAbilityClass && OldCharacter->GetAbilitySystemComponent())
    {
        OldCharacter->GetAbilitySystemComponent()->TryActivateAbilityByClass(OldCharacter->SwapOutAbilityClass);
    }
    else
    {
        // 降级路径：直接执行旧的同步切换
        UE_LOG(LogTemp, Warning, TEXT("Server_SwitchCharacter: SwapOutAbilityClass 未配置，使用降级路径"));

        FTransform SwapTransform;
        OldCharacter->PerformSwapOut(SwapTransform);

        FRotator OldControlRotation = GetControlRotation();
        UnPossess();

        NewCharacter->PerformSwapIn(SwapTransform);
        Possess(NewCharacter);
        SetControlRotation(OldControlRotation);

        MyPlayerState->SetActiveCharacterIndex(TargetIndex);
        Client_OnCharacterSwitched(TargetIndex);
        PendingSwapTargetIndex = -1;
    }
}

void AMainGamePlayerController::OnSwapOutCompleted(APlayerCharacter* SwappedOutCharacter, FTransform SwapTransform)
{
    if (!SwappedOutCharacter) return;

    // 延迟一帧解绑委托，防止重复触发
    GetWorldTimerManager().SetTimerForNextTick([this, SwappedOutCharacter]()
	{
		if (IsValid(SwappedOutCharacter))
		{
			SwappedOutCharacter->OnSwapOutCompleted.RemoveDynamic(this, &AMainGamePlayerController::OnSwapOutCompleted);
		}
	});
    
    AMainGamePlayerState* MyPlayerState = GetPlayerState<AMainGamePlayerState>();
    if (!MyPlayerState || PendingSwapTargetIndex < 0) return;

    // 1. 获取目标角色
    APlayerCharacter* NewCharacter = MyPlayerState->GetTeamCharacterByIndex(PendingSwapTargetIndex);
    if (!NewCharacter)
    {
        PendingSwapTargetIndex = -1;
        return;
    }

    // 2. 解除旧角色控制
    FRotator OldControlRotation = GetControlRotation();
    UnPossess();

    // 3. 写入出场 Transform 供 GA_SwapIn 读取
    NewCharacter->SetPendingSwapInTransform(SwapTransform);

    // 4. 接管新角色
    Possess(NewCharacter);
    SetControlRotation(OldControlRotation);

    // 5. 激活 GA_SwapIn（出场技能）
    if (NewCharacter->SwapInAbilityClass && NewCharacter->GetAbilitySystemComponent())
    {
        NewCharacter->GetAbilitySystemComponent()->TryActivateAbilityByClass(NewCharacter->SwapInAbilityClass);
    }
    else
    {
        // 降级路径：直接执行旧的出场逻辑
        UE_LOG(LogTemp, Warning, TEXT("OnSwapOutCompleted: SwapInAbilityClass 未配置，使用降级路径"));
        NewCharacter->PerformSwapIn(SwapTransform);
    }

    // 6. 更新 PlayerState 的激活索引（触发全网同步）
    MyPlayerState->SetActiveCharacterIndex(PendingSwapTargetIndex);

    // 7. 通知客户端完成切换
    Client_OnCharacterSwitched(PendingSwapTargetIndex);

    // 8. 清理缓存
    PendingSwapTargetIndex = -1;
}

void AMainGamePlayerController::Client_OnCharacterSwitched_Implementation(int32 NewActiveIndex)
{
    // 客户端收到服务器确认后，执行本地 UI 刷新等操作
    // PlayerState 的 OnRep_ActiveCharacterIndex 会自动触发 TeamManager 广播
    // 这里可以补充客户端专属的本地表现逻辑（如音效、镜头震动等）
    UE_LOG(LogTemp, Log, TEXT("Client_OnCharacterSwitched: 切换完成，新激活索引 = %d"), NewActiveIndex);
}

// 输入回调实现 (邮局原则：只转发，不拦截)
//
// 注意 [联机安全]: SendGameplayEventToActor 在客户端本地执行。
// 如果触发的技能是 LocalPredicted，GAS 会自动与服务器同步，无需额外 RPC。
// 但如果是 ServerInitiated 技能，客户端调用 SendGameplayEventToActor 不会触发服务器执行，
// 需要改为 Server RPC 调用 TryActivateAbility。
// 请确保所有战斗技能的 NetExecutionPolicy 设置正确。

void AMainGamePlayerController::Input_Move(const FInputActionValue& Value)
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    FVector2D MoveValue = Value.Get<FVector2D>();
    PC->HandleMovementInput(MoveValue.X, MoveValue.Y);
}

void AMainGamePlayerController::Input_MoveCompleted(const FInputActionValue& Value)
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        PC->HandleMovementInputCompleted();
    }
}

void AMainGamePlayerController::Input_JumpStart()
{
    UE_LOG(LogTemp, Warning, TEXT("[Jump] Input_JumpStart called from Controller"));

    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("[Jump] Controller has no PlayerCharacter pawn!"));
        return;
    }

    PC->HandleSpacebarInput();
}

void AMainGamePlayerController::Input_JumpStop()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        PC->HandleJumpStopInput();
    }
}

void AMainGamePlayerController::Input_ShiftAction()
{
    // 按下 Shift → 无脑触发 GA_Dash
    // GA_Dash 结束时会检测 Shift 是否仍按住 + 移动输入 + 体力，决定是否过渡到 GA_Sprint
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (DashEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, DashEventTag, FGameplayEventData());
    }
}

void AMainGamePlayerController::Input_ShiftReleased()
{
    // 释放 Shift → 发送 SprintStop 事件，GA_Sprint 内部监听此事件后结束
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (SprintStopEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, SprintStopEventTag, FGameplayEventData());
    }
}

void AMainGamePlayerController::Input_Walk()
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    bIsWalking = !bIsWalking;
    FGameplayTag TagToSend = bIsWalking ? WalkStartEventTag : WalkStopEventTag;

    if (TagToSend.IsValid())
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, TagToSend, FGameplayEventData());
}

void AMainGamePlayerController::Input_Hook()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (HookStartEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, HookStartEventTag, FGameplayEventData());
    }
}

void AMainGamePlayerController::Input_PickUp()
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    PC->HandleInteractInput();
}

void AMainGamePlayerController::Input_ToggleInventory()
{
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        if (InventoryUITag.IsValid())
            UIManager->ShowUIByTag(InventoryUITag);
    }
}

void AMainGamePlayerController::Input_ClothSimulation()
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    bIsPhysicsAnimDisabled = !bIsPhysicsAnimDisabled;
    if (bIsPhysicsAnimDisabled)
    {
        PC->ClearPhysicsAnimLayers();
    }
    else
    {
        PC->SetupPhysicsAnimLayers();
    }
}

void AMainGamePlayerController::Input_NormalAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (NormalAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, NormalAttackEventTag, FGameplayEventData());
        }
    }
}

void AMainGamePlayerController::Input_HeavyAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (HeavyAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, HeavyAttackEventTag, FGameplayEventData());
        }
    }
}

void AMainGamePlayerController::Input_AimAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (AimAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, AimAttackEventTag, FGameplayEventData());
        }
    }
}

void AMainGamePlayerController::Input_PlungeAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (PlungeAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, PlungeAttackEventTag, FGameplayEventData());
        }
    }
}

void AMainGamePlayerController::Input_Aim()
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    PC->ToggleAim();
}
