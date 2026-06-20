// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UIDataAsset.generated.h"

class UWindowWidgetBase;

/**
 * UI Tag → Widget 映射资产。
 */
UCLASS()
class OPENWORLDARPG_API UUIDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** UI Tag → Widget 蓝图类映射。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Mapping")
	TMap<FGameplayTag, TSubclassOf<UWindowWidgetBase>> UIMap;

};
