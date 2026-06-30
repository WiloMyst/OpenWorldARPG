// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/CharacterSaveData.h"
#include "GameplayTagContainer.h"
#include "InitialArchiveData.generated.h"

/**
 * 新建账号初始状态配置。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UInitialArchiveData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 初始玩家已获得角色列表 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    TArray<FCharacterSaveData> InitialOwnedCharacters;

    /** 初始编队角色Tag列表 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    TArray<FGameplayTag> InitialTeamTags;

    /** 初始激活编队角色索引 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starting Config")
    int32 InitialActiveCharacterIndex = 0;
};