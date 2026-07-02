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

/**
 * 物品分类标签页 UI
 */
UCLASS()
class OPENWORLDARPG_API UItemCategoryTabWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|CategoryTab")
    void UpdateTabInfo();

    UFUNCTION(BlueprintCallable, Category = "Inventory|CategoryTab")
    void SetSelected(bool bIsSelected);

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnCategoryTabClicked OnTabClicked;

    // --- 实例数据 (Expose on Spawn) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    FInventoryCategoryTabData CategoryTabData;

protected:
    UFUNCTION()
    void OnTabButtonClicked();

    // --- UI 组件绑定 ---

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> TabButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CategoryNameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CategoryIcon;

private:
    /** 异步加载句柄，用于取消未完成的加载 */
    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

    /** 图标加载完成回调（强类型绑定） */
    void OnCategoryIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);
};
