// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OpenWorldARPGPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class OPENWORLDARPG_API AOpenWorldARPGPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;
};
