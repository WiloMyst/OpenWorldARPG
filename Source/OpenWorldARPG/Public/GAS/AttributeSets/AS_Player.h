// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/AttributeSets/AS_Base.h"
#include "AS_Player.generated.h"

UCLASS()
class OPENWORLDARPG_API UAS_Player : public UAS_Base
{
	GENERATED_BODY()

public:
	UAS_Player();

	// 当属性被修改前调用
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// --- 属性定义 ---

	// 生命值
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAS_Player, Health)

	// 最大生命值
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAS_Player, MaxHealth)

	// 耐力值
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Stamina")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UAS_Player, Stamina)

	// 最大耐力值
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Stamina")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UAS_Player, MaxStamina)

};
