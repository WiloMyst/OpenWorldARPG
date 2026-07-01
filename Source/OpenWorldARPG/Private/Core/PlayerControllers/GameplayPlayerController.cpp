// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Core/GameModes/GameplayGameModeBase.h"
#include "Characters/PlayerCharacter.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "UI/Core/WindowWidgetBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"

AGameplayPlayerController::AGameplayPlayerController()
{
}

void AGameplayPlayerController::BeginPlay()
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

    // 订阅 UIManager 的 UI 栈变化广播
    if (UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->OnUIStackChanged.AddDynamic(this, &AGameplayPlayerController::UpdateInputMode);
    }
}

void AGameplayPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // 视角转动
        if (IA_Look) EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AGameplayPlayerController::Input_Look);
        
        // 镜头回正
        if (IA_CameraReset) EnhancedInputComponent->BindAction(IA_CameraReset, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_CameraReset);

        // 移动
        if (IA_Move)
        {
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AGameplayPlayerController::Input_Move);
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Completed, this, &AGameplayPlayerController::Input_MoveCompleted);
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Canceled, this, &AGameplayPlayerController::Input_MoveCompleted);
        }

        // 跳跃
        if (IA_Jump)
        {
            EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_JumpStart);
            EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AGameplayPlayerController::Input_JumpStop);
        }

        // 冲刺/闪避：按下 Shift 触发 GA_Dash，GA_Dash 结束时自动判断是否过渡到 GA_Sprint
        if (IA_Sprint)
        {
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_ShiftAction);
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AGameplayPlayerController::Input_ShiftReleased);
        }

        // 行走 (FlipFlop)
        if (IA_Walk) EnhancedInputComponent->BindAction(IA_Walk, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Walk);
        // 拾取
        if (IA_Interact) EnhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Interact);
        if (IA_Switch_1) EnhancedInputComponent->BindAction(IA_Switch_1, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Switch1);
        if (IA_Switch_2) EnhancedInputComponent->BindAction(IA_Switch_2, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Switch2);
        if (IA_Switch_3) EnhancedInputComponent->BindAction(IA_Switch_3, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Switch3);
        if (IA_Switch_4) EnhancedInputComponent->BindAction(IA_Switch_4, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Switch4);

        if (IA_NormalAttack) 
        {
            EnhancedInputComponent->BindAction(IA_NormalAttack, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_NormalAttack);
            EnhancedInputComponent->BindAction(IA_NormalAttack, ETriggerEvent::Completed, this, &AGameplayPlayerController::Input_NormalAttackReleased);
            EnhancedInputComponent->BindAction(IA_NormalAttack, ETriggerEvent::Canceled, this, &AGameplayPlayerController::Input_NormalAttackReleased);
        }
        if (IA_HeavyAttack)  EnhancedInputComponent->BindAction(IA_HeavyAttack, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_HeavyAttack);
        if (IA_AimAttack)
        {
            EnhancedInputComponent->BindAction(IA_AimAttack, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_AimAttack);
            EnhancedInputComponent->BindAction(IA_AimAttack, ETriggerEvent::Completed, this, &AGameplayPlayerController::Input_AimAttackReleased);
        }
        if (IA_Aim)          EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_Aim);
        if (IA_PlungeAttack) EnhancedInputComponent->BindAction(IA_PlungeAttack, ETriggerEvent::Started, this, &AGameplayPlayerController::Input_PlungeAttack);

        // 镜头缩放（鼠标滚轮）
        if (IA_CameraZoom) EnhancedInputComponent->BindAction(IA_CameraZoom, ETriggerEvent::Triggered, this, &AGameplayPlayerController::Input_CameraZoom);

        // 显示/隐藏光标（Alt键）
        if (IA_ShowCursor)
        {
            EnhancedInputComponent->BindAction(IA_ShowCursor, ETriggerEvent::Started, this, &AGameplayPlayerController::ShowCursorTemporarily);
            EnhancedInputComponent->BindAction(IA_ShowCursor, ETriggerEvent::Completed, this, &AGameplayPlayerController::HideCursorTemporarily);
        }
    }
}

void AGameplayPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (!bIsResettingCamera) return;

    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        bIsResettingCamera = false;
        return;
    }

    // 构造目标旋转：Yaw 对齐角色正前方，Pitch 使用黄金俯角，Roll 归零
    FRotator CurrentRotation = GetControlRotation();
    FRotator TargetRotation(
        TargetResetPitch,
        ControlledPawn->GetActorRotation().Yaw,
        0.0f
    );

    // Normalize 确保取最短旋转路径（防止 170° → -170° 反转 340°）
    CurrentRotation.Normalize();
    TargetRotation.Normalize();

    // 插值逼近目标
    const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, CameraResetInterpSpeed);
    SetControlRotation(NewRotation);

    // 到达容差范围内则精确对齐并结束回正
    if (NewRotation.Equals(TargetRotation, CameraResetTolerance))
    {
        SetControlRotation(TargetRotation);
        bIsResettingCamera = false;
    }
}

// --- 角色切换 (Server RPC) ---

void AGameplayPlayerController::HandleSwitchCharacterInput(int32 Index)
{
    // 客户端：只发送 RPC 请求到服务器，不做任何本地切换逻辑
    Server_SwitchCharacter(Index);
}

bool AGameplayPlayerController::Server_SwitchCharacter_Validate(int32 TargetIndex)
{
    // 基础验证：索引必须 >= 0（队伍上限由服务器端判定）
    return TargetIndex >= 0 && TargetIndex < 4; // 最多4人队伍
}

void AGameplayPlayerController::Server_SwitchCharacter_Implementation(int32 TargetIndex)
{
    // 服务器端：通过 GA 流水线执行角色切换
    // 验证逻辑已移入 GA_SwapOutBase/GA_SwapInBase 的 ActivateAbility 中，
    // Controller 不再预检查，GA 验证失败会 Cancel，EndAbility(bWasCancelled=true) 不会触发委托

    AGameplayPlayerState* MyPlayerState = GetPlayerState<AGameplayPlayerState>();
    if (!MyPlayerState) return;

    // 1. 获取当前控制的角色
    APlayerCharacter* OldCharacter = Cast<APlayerCharacter>(GetPawn());
    if (!OldCharacter) return;

    // 2. 从 PlayerState 获取队伍角色实例
    APlayerCharacter* NewCharacter = MyPlayerState->GetTeamCharacterByIndex(TargetIndex);
    if (!NewCharacter || NewCharacter == OldCharacter) return;

    // 2.5. 验证目标角色是否允许切换上场（在触发退场前检查，避免退场后目标不可用导致状态不一致）
    if (PreventSwitchTags.IsValid())
    {
        if (UAbilitySystemComponent* TargetASC = NewCharacter->GetAbilitySystemComponent())
        {
            if (TargetASC->HasAnyMatchingGameplayTags(PreventSwitchTags))
            {
                UE_LOG(LogTemp, Warning, TEXT("Server_SwitchCharacter: 目标角色拥有 PreventSwitchTags，禁止切换！"));
                return;
            }
        }
    }

    // 3. 缓存目标索引，供 GA_SwapOutBase 完成回调使用
    PendingSwapTargetIndex = TargetIndex;

    // 4. 缓存出场 Transform，供 OnSwapOutCompleted 回调使用
    PendingSwapTransform = OldCharacter->GetActorTransform();

    // 5. 绑定旧角色的退场完成委托
    //    GA_SwapOutBase::EndAbility(非Cancel) 会通过 NotifySwapOutCompleted 广播此委托
    OldCharacter->OnSwapOutCompleted.AddDynamic(this, &AGameplayPlayerController::OnSwapOutCompleted);

    // 6. 激活 GA_SwapOutBase（退场技能）
    //    GA 内部会验证 AllowedSwapOutMovementModes，不满足则 Cancel
    if (!OldCharacter->GetSwapOutEventTag().IsValid() || !OldCharacter->GetAbilitySystemComponent())
    {
        UE_LOG(LogTemp, Error, TEXT("Server_SwitchCharacter: SwapOutEventTag 未配置或 ASC 无效，无法切换！"));
        OldCharacter->OnSwapOutCompleted.RemoveDynamic(this, &AGameplayPlayerController::OnSwapOutCompleted);
        PendingSwapTargetIndex = -1;
        return;
    }
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OldCharacter, OldCharacter->GetSwapOutEventTag(), FGameplayEventData());
}

void AGameplayPlayerController::OnSwapOutCompleted(APlayerCharacter* SwappedOutCharacter, FTransform SwapTransform)
{
    if (!SwappedOutCharacter) return;

    // 延迟一帧解绑委托，防止重复触发
    GetWorldTimerManager().SetTimerForNextTick([this, SwappedOutCharacter]()
	{
		if (IsValid(SwappedOutCharacter))
		{
			SwappedOutCharacter->OnSwapOutCompleted.RemoveDynamic(this, &AGameplayPlayerController::OnSwapOutCompleted);
		}
	});
    
    AGameplayPlayerState* MyPlayerState = GetPlayerState<AGameplayPlayerState>();
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

    // 3. 设置新角色的出场 Transform（在激活 GA 前完成，GA 从 GetActorTransform 读取）
    NewCharacter->SetActorTransform(PendingSwapTransform, false, nullptr, ETeleportType::TeleportPhysics);

    // 4. 接管新角色
    Possess(NewCharacter);
    SetControlRotation(OldControlRotation);

    // 5. 激活 GA_SwapInBase（出场技能）
    //    GA 内部会验证 PreventSwitchTags，不满足则 Cancel
    if (!NewCharacter->GetSwapInEventTag().IsValid() || !NewCharacter->GetAbilitySystemComponent())
    {
        UE_LOG(LogTemp, Error, TEXT("OnSwapOutCompleted: SwapInEventTag 未配置或 ASC 无效！"));
    }
    else
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(NewCharacter, NewCharacter->GetSwapInEventTag(), FGameplayEventData());
    }

    // 6. 更新 PlayerState 的激活索引（触发全网同步）
    MyPlayerState->SetActiveCharacterIndex(PendingSwapTargetIndex);

    // 7. 通知客户端完成切换
    Client_OnCharacterSwitched(PendingSwapTargetIndex);

    // 8. 清理缓存
    PendingSwapTargetIndex = -1;
}

void AGameplayPlayerController::Client_OnCharacterSwitched_Implementation(int32 NewActiveIndex)
{
    // 客户端收到服务器确认后，执行本地 UI 刷新等操作
    // PlayerState 的 OnRep_ActiveCharacterIndex 会自动触发 TeamManager 广播
    // 这里可以补充客户端专属的本地表现逻辑（如音效、镜头震动等）
    UE_LOG(LogTemp, Log, TEXT("Client_OnCharacterSwitched: 切换完成，新激活索引 = %d"), NewActiveIndex);
}

// ==========================================
// 编队保存：纯 RPC 转发器（工厂职责由 GameMode 承担）
// ==========================================

bool AGameplayPlayerController::Server_ApplyTeamChanges_Validate(const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex)
{
    // 基础验证：激活索引合法 + 队伍不超 4 人
    return ActiveIndex >= 0 && ActiveIndex < 4 && NewTeamTags.Num() <= 4;
}

void AGameplayPlayerController::Server_ApplyTeamChanges_Implementation(const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex)
{
    if (!HasAuthority()) return;

    // 邮局原则：仅向 GameMode 工厂转发请求，不直接 Spawn/Destroy 任何实体
    if (AGameplayGameModeBase* GM = Cast<AGameplayGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->ApplyPlayerTeamChanges(this, NewTeamTags, ActiveIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Server_ApplyTeamChanges: 无法获取 AGameplayGameModeBase！"));
    }
}

void AGameplayPlayerController::Input_Look(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    // 输入打断检测：回正过程中玩家转动鼠标/摇杆则立即中止回正
    if (bIsResettingCamera)
    {
        if (FMath::Abs(LookAxisVector.X) > CameraResetInterruptThreshold ||
            FMath::Abs(LookAxisVector.Y) > CameraResetInterruptThreshold)
        {
            bIsResettingCamera = false;
        }
    }

    // 应用视角输入到控制器
    if (APawn* ControlledPawn = GetPawn())
    {
        ControlledPawn->AddControllerYawInput(LookAxisVector.X);
        ControlledPawn->AddControllerPitchInput(LookAxisVector.Y);
    }
}

void AGameplayPlayerController::Input_CameraReset()
{
    if (GetPawn())
    {
        bIsResettingCamera = true;
    }
}

void AGameplayPlayerController::Input_Move(const FInputActionValue& Value)
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    FVector2D MoveValue = Value.Get<FVector2D>();
    PC->HandleMovementInput(MoveValue.X, MoveValue.Y);
}

void AGameplayPlayerController::Input_MoveCompleted(const FInputActionValue& Value)
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        PC->HandleMovementInputCompleted();
    }
}

void AGameplayPlayerController::Input_JumpStart()
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

void AGameplayPlayerController::Input_JumpStop()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        PC->HandleJumpStopInput();
    }
}

void AGameplayPlayerController::Input_ShiftAction()
{
    // 按下 Dash/Sprint 动作键 → 记录状态 + 触发 GA_Dash
    // GA_Dash 结束时会读取 IsSprintActionHeld() 判断点按/长按，决定接续短疾跑还是长疾跑
    bIsSprintActionHeld = true;
    Server_SetSprintActionHeld(true);

    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (DashEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, DashEventTag, FGameplayEventData());
    }
}

void AGameplayPlayerController::Input_ShiftReleased()
{
    // 释放 Dash/Sprint 动作键 → 仅更新状态，不再发送 SprintStop 事件
    // 鸣潮规则：进入疾跑后松开冲刺键不会打断疾跑，疾跑仅由方向键松开或体力耗尽终止
    bIsSprintActionHeld = false;
    Server_SetSprintActionHeld(false);
}

void AGameplayPlayerController::Server_SetSprintActionHeld_Implementation(bool bHeld)
{
    bIsSprintActionHeld = bHeld;
}

bool AGameplayPlayerController::Server_SetSprintActionHeld_Validate(bool bHeld)
{
    return true;
}

void AGameplayPlayerController::Input_Walk()
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    bIsWalking = !bIsWalking;
    FGameplayTag TagToSend = bIsWalking ? WalkStartEventTag : WalkStopEventTag;

    if (TagToSend.IsValid())
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, TagToSend, FGameplayEventData());
}

void AGameplayPlayerController::Input_Interact()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        PC->HandleInteractInput();
    }
}

void AGameplayPlayerController::Input_NormalAttack()
{
    bIsNormalAttackHeld = true;

    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (NormalAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, NormalAttackEventTag, FGameplayEventData());
        }
    }
}

void AGameplayPlayerController::Input_NormalAttackReleased()
{
    bIsNormalAttackHeld = false;
}

void AGameplayPlayerController::Input_HeavyAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (HeavyAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, HeavyAttackEventTag, FGameplayEventData());
        }
    }
}

void AGameplayPlayerController::Input_AimAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (AimAttackStartEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, AimAttackStartEventTag, FGameplayEventData());
        }
    }
}

void AGameplayPlayerController::Input_AimAttackReleased()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (AimAttackStopEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, AimAttackStopEventTag, FGameplayEventData());
        }
    }
}

void AGameplayPlayerController::Input_PlungeAttack()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (PlungeAttackEventTag.IsValid())
        {
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, PlungeAttackEventTag, FGameplayEventData());
        }
    }
}

void AGameplayPlayerController::Input_Aim()
{
	APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
	if (!PC) return;

	bIsAiming = !bIsAiming;
	FGameplayTag TagToSend = bIsAiming ? AimStartEventTag : AimStopEventTag;

	if (TagToSend.IsValid())
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, TagToSend, FGameplayEventData());
}

void AGameplayPlayerController::Input_CameraZoom(const FInputActionValue& Value)
{
    const float ZoomValue = Value.Get<float>();
    if (FMath::IsNearlyZero(ZoomValue)) return;

    // 滚轮向上（ZoomValue > 0）拉近，滚轮向下（ZoomValue < 0）拉远
    PlayerDesiredArmLength -= (ZoomValue * CameraZoomStep);
    PlayerDesiredArmLength = FMath::Clamp(PlayerDesiredArmLength, MinCameraDistance, MaxCameraDistance);
}

void AGameplayPlayerController::SetMainHUDVisible(bool bIsVisible)
{
    if (MainHUDInstance)
    {
        MainHUDInstance->SetVisibility(bIsVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void AGameplayPlayerController::ShowCursorTemporarily(const FInputActionValue& Value)
{
    bIsAltKeyDown = true;
    UpdateInputMode();
}

void AGameplayPlayerController::HideCursorTemporarily(const FInputActionValue& Value)
{
    bIsAltKeyDown = false;
    UpdateInputMode();
}

void AGameplayPlayerController::UpdateInputMode()
{
    UUIManagerSubsystem* UIManager = GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>();
    
    UWindowWidgetBase* TopUI = UIManager ? UIManager->GetTopWindowWidget() : nullptr; 

    if (TopUI)
    {
        bShowMouseCursor = true;
        
        TSharedPtr<SWidget> SlateWidget = TopUI->GetCachedWidget();

        if (TopUI->InputModeWhenOpen == EWidgetInputMode::UIOnly)
        {
            FInputModeUIOnly InputMode;
            if (SlateWidget.IsValid()) InputMode.SetWidgetToFocus(SlateWidget);
            SetInputMode(InputMode);
        }
        else if (TopUI->InputModeWhenOpen == EWidgetInputMode::GameAndUI)
        {
            FInputModeGameAndUI InputMode;
            if (SlateWidget.IsValid()) InputMode.SetWidgetToFocus(SlateWidget);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);
        }
        
        TopUI->SetFocus();

        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().ReleaseMouseCapture();
        }
        return;
    }

    if (bIsAltKeyDown)
    {
        bShowMouseCursor = true;
        FInputModeGameAndUI InputMode;
        
        if (MainHUDInstance && MainHUDInstance->GetCachedWidget().IsValid())
        {
            InputMode.SetWidgetToFocus(MainHUDInstance->GetCachedWidget());
        }

        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);

        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().ReleaseMouseCapture();
        }
    }
    else
    {
        bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
    }
}