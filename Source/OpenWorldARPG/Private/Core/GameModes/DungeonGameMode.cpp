// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/DungeonGameMode.h"
#include "Core/PlayerControllers/DungeonPlayerController.h"
#include "Core/PlayerStates/GameplayPlayerState.h"

ADungeonGameMode::ADungeonGameMode()
{
    PlayerControllerClass = ADungeonPlayerController::StaticClass();
    PlayerStateClass = AGameplayPlayerState::StaticClass();
}
