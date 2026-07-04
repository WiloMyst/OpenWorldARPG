// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "CharacterCarouselWidget.generated.h"

class UCharacterCarouselItemWidget;
class UHorizontalBox;
class UWrapBox;

/**
 * 角色头像轮播面板（对应 WBP_CharacterCarousel）。
 *
 * 水平展示所有已拥有角色的头像，点击头像时通过委托通知主壳子。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterCarouselWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterCarouselWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;

    /** 刷新角色头像列表（从 CharacterManagerSubsystem 拉取数据） */
    UFUNCTION(BlueprintCallable, Category = "CharacterCarousel")
    void RefreshCharacterList();

    /** 获取列表中第一个角色的 Tag（供初始保底选中使用） */
    UFUNCTION(BlueprintPure, Category = "CharacterCarousel")
    FGameplayTag GetFirstCharacterTag() const;

    /** 当前选中角色的头像 Widget */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarousel|State")
    TObjectPtr<UCharacterCarouselItemWidget> SelectedItem;

protected:
    /** 头像容器（蓝图绑定 HorizontalBox 或 WrapBox） */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterCarousel|Panels", meta = (BindWidget))
    TObjectPtr<UPanelWidget> CharacterListContainer;

    /** 头像 Widget 类（蓝图配置） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterCarousel|Config")
    TSubclassOf<UCharacterCarouselItemWidget> CarouselItemClass;

public:
    /** 当角色被选中时触发（向上传递给 MainWidget） */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterSelectedSignature, const FGameplayTag&, CharacterTag);

    UPROPERTY(BlueprintAssignable, Category = "CharacterCarousel|Events")
    FOnCharacterSelectedSignature OnCharacterSelected;

    /** 头像被点击时的回调（由 ItemWidget 或 MainWidget 模拟点击调用） */
    UFUNCTION(BlueprintCallable, Category = "CharacterCarousel")
    void HandleItemSelected(const FGameplayTag& CharacterTag);
};
