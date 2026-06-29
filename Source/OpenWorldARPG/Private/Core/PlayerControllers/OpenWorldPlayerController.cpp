// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"

AOpenWorldPlayerController::AOpenWorldPlayerController()
{
}

void AOpenWorldPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // TODO: 大世界专属初始化（如加载大地图 IMC、滑翔伞输入等）
}

void AOpenWorldPlayerController::SetupInputComponent()
{
    // 先调用基类绑定：视角/移动/跳跃/冲刺/战斗/切人/背包/拾取等通用输入
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // 钩索（大世界专属）
        if (IA_Hook) EnhancedInputComponent->BindAction(IA_Hook, ETriggerEvent::Started, this, &AOpenWorldPlayerController::Input_Hook);
    }
}

// --- 大世界专属输入回调 ---

void AOpenWorldPlayerController::Input_Hook()
{
    if (APlayerCharacter* PC = Cast<APlayerCharacter>(GetPawn()))
    {
        if (HookStartEventTag.IsValid())
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PC, HookStartEventTag, FGameplayEventData());
    }
}
