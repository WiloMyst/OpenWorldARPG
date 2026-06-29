// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/MainMenuGameMode.h"
#include "Data/StartingRosterConfig.h"

UStartingRosterConfig* AMainMenuGameMode::GetStartingRosterConfig() const
{
    return StartingRosterAsset.LoadSynchronous();
}

TSoftObjectPtr<UWorld> AMainMenuGameMode::GetTargetLevelToLoad() const
{
    return TargetLevelToLoad;
}
