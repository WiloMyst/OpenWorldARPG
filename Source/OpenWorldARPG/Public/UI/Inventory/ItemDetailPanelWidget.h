// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemInstance.h"
#include "ItemDetailPanelWidget.generated.h"

class UTextBlock;
class UImage;

UCLASS()
class OPENWORLDARPG_API UItemDetailPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 对应蓝图的 "Update Details" 自定义事件
    UFUNCTION(BlueprintCallable, Category = "Inventory|Details")
    void UpdateDetails(const FItemInstance& ItemInstance);

    // 异步图标加载完成回调
    UFUNCTION()
    void OnDetailIconLoaded(FSoftObjectPath LoadedPath);

protected:
    // ==========================================
    // UI 组件绑定 (变量名必须与蓝图中完全一致)
    // ==========================================

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> AvatarImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CategoryText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> AmountText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> FunctionText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DetailText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> SourceText;
};