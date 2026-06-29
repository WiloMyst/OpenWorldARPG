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
 * 角色头像列表项（对应 WBP_CharacterCarouselItem）。
 * 单个角色头像 Widget，包含头像图标和选中高亮。
 *
 * 【大厂规范】：UI 交互项使用 UButton 组件拦截点击，
 * 而非重写底层 NativeOnMouseButtonDown，避免被父级 Widget 的鼠标事件拦截。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterCarouselItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterCarouselItemWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;

    /** 初始化头像项数据 */
    UFUNCTION(BlueprintCallable, Category = "CharacterCarouselItem")
    void InitializeItem(const FGameplayTag& InCharacterTag, UTexture2D* InHeadIcon);

    /** 获取此 Item 对应的角色 Tag */
    UFUNCTION(BlueprintPure, Category = "CharacterCarouselItem")
    FGameplayTag GetCharacterTag() const { return CharacterTag; }

    /** 设置选中状态（蓝图侧处理高亮表现） */
    UFUNCTION(BlueprintImplementableEvent, Category = "CharacterCarouselItem")
    void SetSelected(bool bIsSelected);

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemSelectedSignature, const FGameplayTag&, CharacterTag);

    /** 当此头像被点击时触发 */
    UPROPERTY(BlueprintAssignable, Category = "CharacterCarouselItem|Events")
    FOnItemSelectedSignature OnItemSelected;

protected:
    /** 头像按钮点击回调（通过 UButton 标准点击事件触发，避免被父级 NativeOnMouseButtonDown 拦截） */
    UFUNCTION()
    void OnItemButtonClicked();

    // --- 绑定控件 ---

    /** 头像图标 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarouselItem|Widgets", meta = (BindWidget))
    TObjectPtr<UImage> HeadIconImage;

    /** 头像按钮（蓝图侧绑定，用于标准点击事件，替代 NativeOnMouseButtonDown） */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarouselItem|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> ItemButton;

    // --- 数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarouselItem|Data")
    FGameplayTag CharacterTag;
};
