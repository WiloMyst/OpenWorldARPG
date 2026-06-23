// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/OpenWorldGameMode.h"
#include "Core/PlayerControllers/OpenWorldPlayerController.h"
#include "Core/PlayerStates/GameplayPlayerState.h"

AOpenWorldGameMode::AOpenWorldGameMode()
{
    PlayerControllerClass = AOpenWorldPlayerController::StaticClass();
    PlayerStateClass = AGameplayPlayerState::StaticClass();
}
