// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Core/OpenWorldARPGPlayerController.h"
#include "Managers/UIManagerSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"


void AOpenWorldARPGPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 绑定输入映射上下文
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}


//////////////////////////////////////////////////////////////////////////
// Input

void AOpenWorldARPGPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // --- 绑定动作与回调函数 ---

		//Looking
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOpenWorldARPGPlayerController::Look);

        // 绑定Alt键按下事件
        EnhancedInputComponent->BindAction(ShowCursorAction, ETriggerEvent::Started, this, &AOpenWorldARPGPlayerController::ShowCursorTemporarily);

        // 绑定Alt键松开事件
        EnhancedInputComponent->BindAction(ShowCursorAction, ETriggerEvent::Completed, this, &AOpenWorldARPGPlayerController::HideCursorTemporarily);


    }
}

void AOpenWorldARPGPlayerController::Look(const FInputActionValue& Value)
{
    // input is a Vector2D
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (APawn* ControlledPawn = GetPawn())
    {
        // add yaw and pitch input to controller
        ControlledPawn->AddControllerYawInput(LookAxisVector.X);
        ControlledPawn->AddControllerPitchInput(LookAxisVector.Y);
    }
}

void AOpenWorldARPGPlayerController::ShowCursorTemporarily(const FInputActionValue& Value)
{
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        if (UIManager->IsAnyUIOpen())
        {
            return;
        }
    }

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
}

void AOpenWorldARPGPlayerController::HideCursorTemporarily(const FInputActionValue& Value)
{
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        if (UIManager->IsAnyUIOpen())
        {
            return;
        }
    }

    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}


