// Fill out your copyright notice in the Description page of Project Settings.

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
	/**
	 * @brief 获取GameplayTag的最后一部分名称。
	 * 例如，给定Tag "Character.Hero.Warrior"，将返回 "Warrior"。
	 * @param InTag 输入的GameplayTag。
	 * @return 返回Tag的最后一部分字符串。如果Tag无效或为空，则返回空字符串。
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Gameplay Tag")
	static FString GetLastPartOfGameplayTag(const FGameplayTag& InTag);

	/**
	 * @brief 公开引擎的CancelAbilities功能给蓝图。
	 * @param ASC 目标能力系统组件。
	 * @param WithTags 一个Tag容器，所有拥有这些Tag的激活中能力都将被取消。
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	static void CancelAbilitiesWithTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& WithTags);
	
};
