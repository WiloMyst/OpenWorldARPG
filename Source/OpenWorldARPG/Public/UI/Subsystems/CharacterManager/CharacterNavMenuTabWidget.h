// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "CharacterNavMenuTabWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * 动态多播委托：Tab 被点击时向外广播自身的 TabId。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavTabClickedSignature, const FGameplayTag&, TabId);

/**
 * 导航 Tab 按钮 (对应 WBP_CharacterNavMenu_Tab)。
 * 只负责展示文本与点击广播，页面切换由父容器统一管理。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterNavMenuTabWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterNavMenuTabWidget(const FObjectInitializer& ObjectInitializer);

    // --- 生命周期 ---

    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;

    // --- 外部接口 ---

    UFUNCTION(BlueprintCallable, Category = "NavTab")
    void SetTabName(const FText& InName);

    UFUNCTION(BlueprintPure, Category = "NavTab")
    FGameplayTag GetTabId() const { return TabId; }

    UFUNCTION(BlueprintCallable, Category = "NavTab")
    void SetTabId(const FGameplayTag& InTabId);

public:
    /** 点击广播：传递 TabId */
    UPROPERTY(BlueprintAssignable, Category = "NavTab|Events")
    FOnNavTabClickedSignature OnTabClickedDelegate;

protected:
    // --- 绑定控件 ---

    UPROPERTY(BlueprintReadOnly, Category = "NavTab|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> TabNameText;

    UPROPERTY(BlueprintReadOnly, Category = "NavTab|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> TabButton;

    // --- 配置 ---

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NavTab|Config")
    FGameplayTag TabId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NavTab|Config")
    FText DefaultTabName;

    UFUNCTION()
    void HandleTabButtonClicked();

    /** 选中状态变化的视觉更新（蓝图实现） */
    UFUNCTION(BlueprintImplementableEvent, Category = "NavTab|Visual")
    void OnSelectionStateChanged(bool bIsSelected);
};
