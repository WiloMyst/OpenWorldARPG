// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MainWorldHUDLayout.generated.h"

class UBorder;
class UHeroUIExtensionComponent;
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

    // ==========================================
    // 控件绑定 (变量名必须与蓝图中完全一致)
    // ==========================================

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
    // --- MVVM 回调（监听 ExtensionComponent 的统一事件） ---

    UFUNCTION()
    void OnHealthChanged(float NewHealth, float NewMaxHealth);

    UFUNCTION()
    void OnAimingStateChanged(bool bIsAiming);

    UFUNCTION()
    void OnInteractionListChanged(const TArray<AActor*>& InteractableActors);

    /** 绑定/解绑 UI 扩展组件（在初始化和 Pawn 切换时复用） */
    void BindToExtensionComp(UHeroUIExtensionComponent* NewExtensionComp);
    void UnbindFromExtensionComp();

    /** Pawn 切换回调（由 PlayerController->OnPossessedPawnChanged 触发） */
    UFUNCTION()
    void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

    // --- 缓存指针 ---
    TWeakObjectPtr<UHeroUIExtensionComponent> CachedExtensionComp;

    float CurrentHealth = 0.0f;
    float CurrentMaxHealth = 1.0f; // 避免除以 0
};