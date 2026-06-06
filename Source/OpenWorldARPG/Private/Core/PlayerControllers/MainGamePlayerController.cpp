// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/MainGamePlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Core/GameModes/MainGameGameMode.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h" 
#include "Components/BackpackComponent.h"
#include "Components/ClimbingComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

AMainGamePlayerController::AMainGamePlayerController()
{
    AllowedMovementModes.Add(MOVE_Walking);
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

    if (UTeamManagerSubsystem* TeamManager = GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>())
    {
        TeamManager->OnRequestCharacterSwitch.RemoveDynamic(this, &AMainGamePlayerController::HandleSwitchCharacter);
        TeamManager->OnRequestCharacterSwitch.AddDynamic(this, &AMainGamePlayerController::HandleSwitchCharacter);
    }
}

void AMainGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // 绑定增强输入系统
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
            EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_JumpStart);
            EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AMainGamePlayerController::Input_JumpStop);
        }

        // 冲刺
        if (IA_Sprint)
        {
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_SprintStart);
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AMainGamePlayerController::Input_SprintStop);
        }

        // 行走 (FlipFlop)
        if (IA_Walk) EnhancedInputComponent->BindAction(IA_Walk, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Walk);
        // 滑翔
        if (IA_Glide) EnhancedInputComponent->BindAction(IA_Glide, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Glide);
        // 钩索
        if (IA_Hook) EnhancedInputComponent->BindAction(IA_Hook, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Hook);
        // 拾取
        if (IA_PickUp) EnhancedInputComponent->BindAction(IA_PickUp, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_PickUp);
        // 打开背包
        if (IA_ToggleInventory) EnhancedInputComponent->BindAction(IA_ToggleInventory, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_ToggleInventory);
        // 头发布料模拟 (FlipFlop)
        if (IA_ClothSimulation) EnhancedInputComponent->BindAction(IA_ClothSimulation, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_ClothSimulation);

        // 队伍切换 1~4
        if (IA_Switch_1) EnhancedInputComponent->BindAction(IA_Switch_1, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Switch1);
        if (IA_Switch_2) EnhancedInputComponent->BindAction(IA_Switch_2, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Switch2);
        if (IA_Switch_3) EnhancedInputComponent->BindAction(IA_Switch_3, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Switch3);
        if (IA_Switch_4) EnhancedInputComponent->BindAction(IA_Switch_4, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Switch4);

        if (IA_NormalAttack) EnhancedInputComponent->BindAction(IA_NormalAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_NormalAttack);
        if (IA_HeavyAttack)  EnhancedInputComponent->BindAction(IA_HeavyAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_HeavyAttack);
        if (IA_AimAttack)    EnhancedInputComponent->BindAction(IA_AimAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_AimAttack);
        if (IA_Aim)          EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_Aim);
        if (IA_PlungeAttack) EnhancedInputComponent->BindAction(IA_PlungeAttack, ETriggerEvent::Triggered, this, &AMainGamePlayerController::Input_PlungeAttack);
    }
}

// ==========================================
// 动作绑定实现
// ==========================================

void AMainGamePlayerController::Input_Move(const FInputActionValue& Value)
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    UAbilitySystemComponent* ASC = PC->GetAbilitySystemComponent();
    // 对应蓝图：如果拥有 Uncontrollable 标签，拦截输入
    if (ASC && UncontrollableStateTag.IsValid() && ASC->HasMatchingGameplayTag(UncontrollableStateTag)) return;

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
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    if (JumpStartEventTag.IsValid())
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, JumpStartEventTag, FGameplayEventData());

    // 对应蓝图：检测是否在攀爬，如果是则退出
    if (UAbilitySystemComponent* ASC = PC->GetAbilitySystemComponent())
    {
        if (ClimbingStateTag.IsValid() && ASC->HasMatchingGameplayTag(ClimbingStateTag))
        {
            if (UClimbingComponent* ClimbComp = PC->FindComponentByClass<UClimbingComponent>())
                ClimbComp->ExitClimb();
        }
    }
}

void AMainGamePlayerController::Input_JumpStop()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (JumpStopEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, JumpStopEventTag, FGameplayEventData());
    }
}

void AMainGamePlayerController::Input_SprintStart()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (SprintStartEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, SprintStartEventTag, FGameplayEventData());
    }
}

void AMainGamePlayerController::Input_SprintStop()
{
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

    bIsWalking = !bIsWalking; // FlipFlop 逻辑
    FGameplayTag TagToSend = bIsWalking ? WalkStartEventTag : WalkStopEventTag;

    if (TagToSend.IsValid())
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, TagToSend, FGameplayEventData());
}

void AMainGamePlayerController::Input_Glide()
{
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    UAbilitySystemComponent* ASC = PC->GetAbilitySystemComponent();
    if (!ASC) return;

    // 滑翔只能在空中下落时启动，地面按空格不应触发滑翔
    if (UCharacterMovementComponent* MoveComp = PC->GetCharacterMovement())
    {
        bool bIsGliding = GlidingStateTag.IsValid() && ASC->HasMatchingGameplayTag(GlidingStateTag);

        // 如果当前不在滑翔状态，且不在空中（下落），则不允许启动滑翔
        if (!bIsGliding && !MoveComp->IsFalling())
        {
            return;
        }
    }

    // 对应蓝图：检测是否在滑翔，进行状态翻转
    bool bIsGliding = GlidingStateTag.IsValid() && ASC->HasMatchingGameplayTag(GlidingStateTag);
    FGameplayTag TagToSend = bIsGliding ? GlideStopEventTag : GlideStartEventTag;

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

    if (UBackpackComponent* Backpack = PC->FindComponentByClass<UBackpackComponent>())
    {
        Backpack->PickUpItem(); // 注意：请确保你的组件方法名叫这个
    }
}

void AMainGamePlayerController::Input_ToggleInventory()
{
    // 对应蓝图：打开 UI
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

    bIsPhysicsAnimDisabled = !bIsPhysicsAnimDisabled; // FlipFlop 逻辑
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
    // 对应图2：判断是否正在瞄准，然后分支发送事件
    APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn());
    if (!PC) return;

    UAbilitySystemComponent* ASC = PC->GetAbilitySystemComponent();
    if (!ASC) return;

    // 检测角色是否拥有瞄准标签
    bool bIsAiming = AimingStateTag.IsValid() && ASC->HasMatchingGameplayTag(AimingStateTag);

    // 如果正在瞄准就发送 Stop，否则发送 Start
    FGameplayTag TagToSend = bIsAiming ? AimStopEventTag : AimStartEventTag;

    if (TagToSend.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, TagToSend, FGameplayEventData());
    }
}

void AMainGamePlayerController::HandleSwitchCharacterInput(int32 Index)
{
    // 将玩家键盘切人输入，转发给 TeamManager
    if (UTeamManagerSubsystem* TeamManager = GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>())
    {
        // 注意：请确保你的 TeamManager 里面切人接口的方法名与这里一致
        TeamManager->SwitchToCharacterByIndex(Index);
    }
}

void AMainGamePlayerController::HandleSwitchCharacter(int32 TargetIndex)
{
    UTeamManagerSubsystem* TeamManager = GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>();
    AMainGameGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainGameGameMode>();
    if (!TeamManager || !GameMode) return;

    // 1. 获取当前控制的角色
    APlayerCharacter* OldCharacter = Cast<APlayerCharacter>(GetPawn());
    if (!OldCharacter) return;

    // ------------------------------------------
    // 拦截点 1：运动模式拦截（检查当前角色）
    // ------------------------------------------
    if (UCharacterMovementComponent* MoveComp = OldCharacter->GetCharacterMovement())
    {
        if (!AllowedMovementModes.Contains(MoveComp->MovementMode))
        {
            UE_LOG(LogTemp, Log, TEXT("HandleSwitchCharacter: 当前运动模式禁止切换角色！"));
            return;
        }
    }

    // 2. 从 TeamManager 获取纯数据 (Tags)，从 GameMode 获取 Actor 引用
    TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();
    if (!TeamTags.IsValidIndex(TargetIndex)) return;

    FGameplayTag TargetTag = TeamTags[TargetIndex];

    // 从 GameMode (World层) 获取角色实例，而非从 GameInstanceSubsystem
    APlayerCharacter* NewCharacter = GameMode->GetTeamCharacterByTag(TargetTag);

    if (!NewCharacter || NewCharacter == OldCharacter) return;

    // ------------------------------------------
    // 拦截点 2：GAS 状态标签拦截（检查目标角色）
    // ------------------------------------------
    UAbilitySystemComponent* TargetASC = NewCharacter->GetAbilitySystemComponent();
    if (TargetASC && TargetASC->HasAnyMatchingGameplayTags(PreventSwitchTags))
    {
        UE_LOG(LogTemp, Log, TEXT("HandleSwitchCharacter: 目标角色状态（Tag）禁止切换！"));
        return;
    }

    // ==========================================
    // 执行角色切换流水线
    // ==========================================

    // 4. 保存旧角色的现场数据
    FTransform OldTransform = OldCharacter->GetActorTransform();
    FRotator OldControlRotation = GetControlRotation();

    // 5. 在旧位置生成切换特效
    if (CharacterSwapFX)
    {
        FVector SpawnFXLocation = OldTransform.GetLocation() + SwapFXLocationOffset;

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            CharacterSwapFX,
            SpawnFXLocation,
            FRotator::ZeroRotator,
            SwapFXScale
        );
    }

    // 6. 让旧角色进入待机休眠状态
    OldCharacter->SetStandbyMode(true);

    // 7. 更新队伍管理器的当前激活索引
    TeamManager->SetActiveCharacterIndex(TargetIndex);

    // 8. 装配新角色并唤醒
    NewCharacter->SetActorTransform(OldTransform);
    NewCharacter->SetStandbyMode(false);

    // 9. 显式解除旧角色控制
    UnPossess();

    // 10. 接管新角色并恢复摄像机视角
    Possess(NewCharacter);
    SetControlRotation(OldControlRotation);
}