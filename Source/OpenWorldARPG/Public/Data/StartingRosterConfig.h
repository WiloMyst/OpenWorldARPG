// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/CharacterSaveData.h"
#include "GameplayTagContainer.h"
#include "StartingRosterConfig.generated.h"

/**
 * 新建账号初始状态配置。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UStartingRosterConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    TArray<FCharacterSaveData> InitialOwnedCharacters;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    TArray<FGameplayTag> InitialTeamTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    int32 InitialActiveCharacterIndex = 0;
};