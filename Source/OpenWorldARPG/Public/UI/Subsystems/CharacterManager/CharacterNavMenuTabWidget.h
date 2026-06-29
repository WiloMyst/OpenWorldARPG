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
 * 角色界面左侧导航 Tab 按钮（单个页签条目，对应 WBP_CharacterNavMenu_Tab）。
 *
 * 【职责单一原则】
 * - 只负责自身的展示（文本）与点击广播，不参与任何页面切换业务逻辑。
 * - 页面切换、状态互斥由父容器 UCharacterNavMenuWidget 统一管理。
 *
 * 【UMG 绑定】
 * - TabNameText (UTextBlock)：显示页签名称
 * - TabButton (UButton)：点击触发广播
 *
 * 【数据驱动】
 * - TabId：唯一标识符（如 UI.Tab.Attribute / UI.Tab.Weapon），父容器据此路由
 * - DefaultTabName：编辑器配置的默认显示文本
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

    /** 设置 Tab 显示文本 */
    UFUNCTION(BlueprintCallable, Category = "NavTab")
    void SetTabName(const FText& InName);

    /** 获取该 Tab 的唯一标识 */
    UFUNCTION(BlueprintPure, Category = "NavTab")
    FGameplayTag GetTabId() const { return TabId; }

    /** 设置该 Tab 的唯一标识（父容器生成时注入） */
    UFUNCTION(BlueprintCallable, Category = "NavTab")
    void SetTabId(const FGameplayTag& InTabId);

public:
    /** 点击事件广播：向外传递 TabId */
    UPROPERTY(BlueprintAssignable, Category = "NavTab|Events")
    FOnNavTabClickedSignature OnTabClickedDelegate;

protected:
    // --- UMG 绑定控件 ---

    /** 页签名称文本 */
    UPROPERTY(BlueprintReadOnly, Category = "NavTab|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> TabNameText;

    /** 页签点击按钮 */
    UPROPERTY(BlueprintReadOnly, Category = "NavTab|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> TabButton;

    // --- 配置数据 ---

    /** Tab 唯一标识符（区分属性/武器/圣遗物/命之座/天赋/资料） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NavTab|Config")
    FGameplayTag TabId;

    /** 默认显示文本（编辑器配置，用于预览） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NavTab|Config")
    FText DefaultTabName;

    // --- 内部逻辑 ---

    /** TabButton 点击的内部处理：广播 OnTabClickedDelegate */
    UFUNCTION()
    void HandleTabButtonClicked();

    /**
     * 选中状态变化的视觉更新（蓝图实现：改文字颜色 / 换背景图）。
     * 由父容器统一调用，本 C++ 类不维护选中状态。
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "NavTab|Visual")
    void OnSelectionStateChanged(bool bIsSelected);
};
