// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/OpenWorldARPGPlayerController.h"
#include "EnhancedInputSubsystems.h"

void AOpenWorldARPGPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void AOpenWorldARPGPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
}
