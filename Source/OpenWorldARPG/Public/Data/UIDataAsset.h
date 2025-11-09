// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UIDataAsset.generated.h"

class UBaseMenuWidget;

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API UUIDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 将UI的GameplayTag映射到对应的BaseMenuWidget蓝图类。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Mapping")
	TMap<FGameplayTag, TSubclassOf<UBaseMenuWidget>> UIMap;

};
