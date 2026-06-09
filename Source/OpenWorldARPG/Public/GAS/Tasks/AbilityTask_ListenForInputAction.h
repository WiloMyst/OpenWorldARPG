// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "InputAction.h"
#include "AbilityTask_ListenForInputAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputActionUpdatedDelegate, const FVector2D&, ActionValue);

UCLASS()
class OPENWORLDARPG_API UAbilityTask_ListenForInputAction : public UAbilityTask
{
	GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FInputActionUpdatedDelegate OnInputActionUpdated;

    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
    static UAbilityTask_ListenForInputAction* ListenForInputAction(UGameplayAbility* OwningAbility, UInputAction* ActionToListenFor);

protected:
    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;

    void OnActionTriggered(const struct FInputActionInstance& ActionInstance);

private:
    UPROPERTY()
    UInputAction* Action;

    uint32 BindingHandle;
	
};
