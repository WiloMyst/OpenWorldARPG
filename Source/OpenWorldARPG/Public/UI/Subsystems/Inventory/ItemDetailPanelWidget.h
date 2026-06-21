// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemInstance.h"
#include "ItemDetailPanelWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;
class UInventoryViewModel;
class UItemObject;

/**
 * 物品详情面板 (Dumb View)
 *
 * 【MVVM 架构：纯被动视图】
 * 绑定 ViewModel 的 OnSelectedItemChanged 委托，当选中物品变化时自动更新详情。
 * 不包含任何业务逻辑，仅负责表现层渲染。
 */
UCLASS()
class OPENWORLDARPG_API UItemDetailPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeDestruct() override;

    /** 接收父级传入的 ViewModel 并绑定委托 */
    void SetViewModel(UInventoryViewModel* InViewModel);

private:
    /** 图标加载完成回调（强类型绑定） */
    void OnDetailIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);

    /** 选中物品变化回调 */
    UFUNCTION()
    void OnSelectedItemChanged(UItemObject* SelectedItemObject);

    /** 根据传入的 UItemObject 更新详情面板 */
    void UpdateDetails(UItemObject* ItemObject);

    /** 清空面板内容 */
    void ClearDetails();

    /** ViewModel 引用 (由父级 InventoryWidget 传入) */
    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    /** 异步加载句柄，用于取消未完成的加载 */
    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

protected:
    // --- UI 组件绑定 ---

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
