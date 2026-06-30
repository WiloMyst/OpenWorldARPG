// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/MainMenuGameMode.h"

TSoftObjectPtr<UWorld> AMainMenuGameMode::GetTargetLevelToLoad() const
{
    return TargetLevelToLoad;
}
