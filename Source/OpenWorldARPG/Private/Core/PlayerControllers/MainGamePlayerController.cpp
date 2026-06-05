// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/MainGamePlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Core/GameModes/MainGameGameMode.h"
#include "Managers/TeamManagerSubsystem.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

AMainGamePlayerController::AMainGamePlayerController()
{
    // 默认只允许在行走状态切人
    AllowedMovementModes.Add(MOVE_Walking);
}

void AMainGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 1. 创建并添加 Main HUD
    if (MainHUDClass)
    {
        MainHUDInstance = CreateWidget<UUserWidget>(this, MainHUDClass);
        if (MainHUDInstance)
        {
            MainHUDInstance->AddToViewport();
        }
    }

    // 2. 绑定切换角色的委托
    if (UTeamManagerSubsystem* TeamManager = GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>())
    {
        TeamManager->OnRequestCharacterSwitch.RemoveDynamic(this, &AMainGamePlayerController::HandleSwitchCharacter);
        TeamManager->OnRequestCharacterSwitch.AddDynamic(this, &AMainGamePlayerController::HandleSwitchCharacter);
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