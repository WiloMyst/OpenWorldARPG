// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MainWorldHUDLayout.generated.h"

class UBorder;
class UPlayerUIExtensionComponent;
class UTeamListWidget;
class UPlayerCharacterBarWidget;
class UGameplayListWidget; 
class UInteractionListWidget; 
class UPlayerControlButtonWidget;
struct FCharacterRegistryRow;

/**
 * 大世界主界面 HUD 布局类
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UMainWorldHUDLayout : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    // --- 控件绑定 ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTeamListWidget* WBP_TeamList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UGameplayListWidget* WBP_GameplayList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UInteractionListWidget* WBP_InteractionList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UPlayerCharacterBarWidget* WBP_PlayerCharacterBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UPlayerControlButtonWidget* WBP_PlayerControlButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UWidget* WBP_AimStar;

private:
    // --- MVVM 回调 ---

    UFUNCTION()
    void OnHealthChanged(float NewHealth, float NewMaxHealth);

    UFUNCTION()
    void OnAimingStateChanged(bool bIsAiming);

    UFUNCTION()
    void OnInteractionListChanged(const TArray<AActor*>& InteractableActors);

    void BindToExtensionComp(UPlayerUIExtensionComponent* NewExtensionComp);
    void UnbindFromExtensionComp();

    UFUNCTION()
    void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

    // --- 缓存 ---
    TWeakObjectPtr<UPlayerUIExtensionComponent> CachedExtensionComp;

    float CurrentHealth = 0.0f;
    float CurrentMaxHealth = 1.0f; // 避免除以 0
};