// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/AbilitySystem/AttributeSets/AS_Base.h"
#include "AS_Enemy.generated.h"

UCLASS()
class OPENWORLDARPG_API UAS_Enemy : public UAS_Base
{
	GENERATED_BODY()

public:
	UAS_Enemy();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// --- 属性 ---

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAS_Enemy, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAS_Enemy, MaxHealth)
	
};
