// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "CharacterCarouselItemWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;

/**
 * 角色头像列表项 (对应 WBP_CharacterCarouselItem)。
 * 使用 UButton 拦截点击，避免被父级 NativeOnMouseButtonDown 拦截。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterCarouselItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterCarouselItemWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "CharacterCarouselItem")
    void InitializeItem(const FGameplayTag& InCharacterTag, UTexture2D* InHeadIcon);

    UFUNCTION(BlueprintPure, Category = "CharacterCarouselItem")
    FGameplayTag GetCharacterTag() const { return CharacterTag; }

    UFUNCTION(BlueprintImplementableEvent, Category = "CharacterCarouselItem")
    void SetSelected(bool bIsSelected);

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemSelectedSignature, const FGameplayTag&, CharacterTag);

    UPROPERTY(BlueprintAssignable, Category = "CharacterCarouselItem|Events")
    FOnItemSelectedSignature OnItemSelected;

protected:
    UFUNCTION()
    void OnItemButtonClicked();

    // --- 绑定控件 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarouselItem|Widgets", meta = (BindWidget))
    TObjectPtr<UImage> HeadIconImage;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarouselItem|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> ItemButton;

    // --- 数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarouselItem|Data")
    FGameplayTag CharacterTag;
};
