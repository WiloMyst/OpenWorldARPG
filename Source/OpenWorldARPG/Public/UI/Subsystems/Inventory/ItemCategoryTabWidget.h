// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Types/ItemTypes.h"
#include "Types/InventoryUITypes.h"
#include "ItemCategoryTabWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UItemCategoryTabWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCategoryTabClicked, UItemCategoryTabWidget*, NewCategoryTab, EItemCategory, NewTabCategory);

UCLASS()
class OPENWORLDARPG_API UItemCategoryTabWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|CategoryTab")
    void UpdateTabInfo();

    void SetTabSelectedState(bool bIsSelected);

    UFUNCTION()
    void OnCategoryIconLoaded(FSoftObjectPath LoadedPath);

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnCategoryTabClicked OnTabClicked;

    // --- 实例数据 (Expose on Spawn) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    FInventoryCategoryTabData CategoryTabData;

protected:
    UFUNCTION()
    void OnTabButtonClicked();

    UFUNCTION()
    void OnTabButtonHovered();

    UFUNCTION()
    void OnTabButtonUnhovered();

protected:
    // --- UI 组件绑定 ---

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> TabButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CategoryNameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CategoryIcon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ImageMouseHovered;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ImageMouseClicked;
};