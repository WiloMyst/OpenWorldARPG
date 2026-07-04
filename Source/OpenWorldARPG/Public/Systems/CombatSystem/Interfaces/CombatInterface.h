// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatInterface.generated.h"

class UWeaponManagerComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 战斗实体接口。任何参与战斗的 Actor 都应继承此接口。
 */
class OPENWORLDARPG_API ICombatInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|State")
	void HandleDeath();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|State")
	void HandleRevive();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Weapon")
	UWeaponManagerComponent* GetWeaponManagerComponent() const;
};
