// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "ItemDetailPanelWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;
class UInventoryViewModel;
class UItemObject;

/**
 * 物品详情面板 (Dumb View)
 * 绑定 ViewModel 的 OnSelectedItemChanged 委托自动更新，不含业务逻辑。
 */
UCLASS()
class OPENWORLDARPG_API UItemDetailPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeDestruct() override;

    void SetViewModel(UInventoryViewModel* InViewModel);

private:
    void OnDetailIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);

    UFUNCTION()
    void OnSelectedItemChanged(UItemObject* SelectedItemObject);

    void UpdateDetails(UItemObject* ItemObject);
    void ClearDetails();

    UPROPERTY()
    TObjectPtr<UInventoryViewModel> ViewModel;

    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

protected:
    // --- 控件绑定 ---

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
