// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterGeneralDataAsset.generated.h"

class UGameplayAbility;

/**
 * 角色共有配置资产。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterGeneralDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 角色共有的GameplayAbility */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色共有GA"))
	TArray<TSubclassOf<UGameplayAbility>> GeneralAbilityClasses;
	
};
