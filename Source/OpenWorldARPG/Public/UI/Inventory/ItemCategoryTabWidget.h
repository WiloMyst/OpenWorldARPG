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

    // 对应蓝图的 "更新 Tab信息"
    UFUNCTION(BlueprintCallable, Category = "Inventory|CategoryTab")
    void UpdateTabInfo();

    // 供主面板调用，控制选中状态的高亮图
    void SetTabSelectedState(bool bIsSelected);

    // 异步图标加载完成回调
    UFUNCTION()
    void OnCategoryIconLoaded(FSoftObjectPath LoadedPath);

    // 暴露给外部系统的点击事件分发器
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnCategoryTabClicked OnTabClicked;

    // ==========================================
    // 暴露给蓝图的实例数据 (Expose on Spawn)
    // 请确保 FInventoryCategoryTabData 结构体名字与你项目一致
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Data", meta = (ExposeOnSpawn = "true"))
    FInventoryCategoryTabData CategoryTabData;

protected:
    // 按钮交互的回调函数
    UFUNCTION()
    void OnTabButtonClicked();

    UFUNCTION()
    void OnTabButtonHovered();

    UFUNCTION()
    void OnTabButtonUnhovered();

protected:
    // ==========================================
    // UI 组件绑定
    // ==========================================

    // 对应你截图里的 Button
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> TabButton;

    // 对应你截图里的 Category Name Text
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CategoryNameText;

    // 对应你截图里的 Category Icon
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CategoryIcon;

    // 对应你截图里的 Image Mouse Hovered (悬停高亮背景图)
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ImageMouseHovered;

    // 对应蓝图里的 Image Mouse Clicked
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ImageMouseClicked;
};