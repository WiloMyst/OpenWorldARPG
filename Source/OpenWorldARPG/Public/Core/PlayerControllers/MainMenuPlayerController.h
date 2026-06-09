// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "MainMenuPlayerController.generated.h"

UCLASS()
class OPENWORLDARPG_API AMainMenuPlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleOnLoginButtonClicked();

    UFUNCTION()
    void HandleOnStartButtonClicked();

protected:
    // --- UI 配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag LoginUITag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI Config")
    FGameplayTag StartGameUITag;
};