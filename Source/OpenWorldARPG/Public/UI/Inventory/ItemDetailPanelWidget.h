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
    UFUNCTION(BlueprintCallable, Category = "Inventory|Details")
    void UpdateDetails(const FItemInstance& ItemInstance);

    UFUNCTION()
    void OnDetailIconLoaded(FSoftObjectPath LoadedPath);

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