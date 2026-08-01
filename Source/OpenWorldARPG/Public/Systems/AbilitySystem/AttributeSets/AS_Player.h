// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Systems/AbilitySystem/AttributeSets/AS_Base.h"
#include "AS_Player.generated.h"

UCLASS()
class OPENWORLDARPG_API UAS_Player : public UAS_Base
{
	GENERATED_BODY()

public:
	UAS_Player();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- 属性 ---

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAS_Player, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAS_Player, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Stamina", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UAS_Player, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UAS_Player, MaxStamina)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | FlyStamina", ReplicatedUsing = OnRep_FlyStamina)
	FGameplayAttributeData FlyStamina;
	ATTRIBUTE_ACCESSORS(UAS_Player, FlyStamina)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes | FlyStamina", ReplicatedUsing = OnRep_MaxFlyStamina)
	FGameplayAttributeData MaxFlyStamina;
	ATTRIBUTE_ACCESSORS(UAS_Player, MaxFlyStamina)

	// --- 复制回调 ---

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	virtual void OnRep_FlyStamina(const FGameplayAttributeData& OldFlyStamina);

	UFUNCTION()
	virtual void OnRep_MaxFlyStamina(const FGameplayAttributeData& OldMaxFlyStamina);

};
