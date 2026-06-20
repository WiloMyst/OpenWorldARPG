// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGGameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UStartingRosterConfig;

UCLASS()
class OPENWORLDARPG_API AMainMenuGameMode : public AOpenWorldARPGGameModeBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
    void HandleStartGameRequest();

protected:
    UFUNCTION()
    void OnLevelPreloadFinished();

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UStartingRosterConfig> StartingRosterAsset;

    UPROPERTY(EditDefaultsOnly, Category = "Config")
    TSoftObjectPtr<UWorld> TargetLevelToLoad;
};