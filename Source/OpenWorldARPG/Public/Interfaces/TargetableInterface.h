// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetableInterface.generated.h"

UINTERFACE(Blueprintable)
class OPENWORLDARPG_API UTargetableInterface : public UInterface
{
    GENERATED_BODY()
};

class OPENWORLDARPG_API ITargetableInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting|Events")
    void OnSetAsTarget();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting|Events")
    void OnClearAsTarget();
};
