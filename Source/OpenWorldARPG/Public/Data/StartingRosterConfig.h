// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Managers/CharacterManagerSubsystem.h" // 包含 FCharacterSaveData
#include "GameplayTagContainer.h"
#include "StartingRosterConfig.generated.h"

// 专门用于配置玩家“新建账号”时的初始状态
UCLASS(BlueprintType)
class OPENWORLDARPG_API UStartingRosterConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 初始赠送的角色数据 (相当于你原来填在BP_SaveGame里的数据)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    TArray<FCharacterSaveData> InitialOwnedCharacters;

    // 初始上阵的队伍编队
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    TArray<FGameplayTag> InitialTeamTags;

    // 初始操控的角色索引
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    int32 InitialActiveCharacterIndex = 0;
};