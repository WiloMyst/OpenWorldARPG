// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "GASBlueprintLibrary.generated.h"

class UAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API UGASBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 获取 GameplayTag 的最后一部分名称（如 "Character.Hero.Warrior" → "Warrior"）。 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Gameplay Tag")
	static FString GetLastPartOfGameplayTag(const FGameplayTag& InTag);

	/** 取消拥有指定 Tags 的所有激活中能力。 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	static void CancelAbilitiesWithTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& WithTags);
	
};
