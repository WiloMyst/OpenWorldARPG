// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Core/WindowWidgetBase.h"
#include "CharacterScreenMainWidget.generated.h"

class ACharacterShowcaseStage;
class UCharacterCarouselWidget;
class UCharacterNavMenuWidget;
class UCharacterDetailsAttributesWidget;
class UCharacterVisualDataAsset;
class UButton;

/**
 * 角色界面主壳子（对应 WBP_CharacterScreen_Main）。
 * 透明背景 UMG，透出背后的 3D 展台 ACharacterShowcaseStage。
 * 展台由 UI 动态 Spawn / Destroy，跨关卡通用。
 * 三段式布局：顶部 Carousel + 左侧 NavMenu + 右侧 Details。
 * 数据流：Carousel 点击 → 切换展台模型 + 刷新 Details。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterScreenMainWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

public:
    UCharacterScreenMainWidget(const FObjectInitializer& ObjectInitializer);

    // --- 生命周期 ---

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- UMG 原生鼠标事件（拖拽旋转展台） ---

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // --- 外部接口 ---

    /** 获取 UI 持有的展台实例（生命周期由 UI 管理） */
    UFUNCTION(BlueprintPure, Category = "CharacterScreen")
    ACharacterShowcaseStage* GetSpawnedStage() const { return SpawnedStage; }

    /** 关闭按钮点击回调 */
    UFUNCTION()
    void OnCloseButtonClicked();

    // --- 子面板引用 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Panels", meta = (BindWidget))
    TObjectPtr<UCharacterCarouselWidget> CarouselPanel;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Panels", meta = (BindWidget))
    TObjectPtr<UCharacterNavMenuWidget> NavMenuPanel;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Panels", meta = (BindWidget))
    TObjectPtr<UCharacterDetailsAttributesWidget> DetailsPanel;

protected:
    // --- 展台配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterScreen|Stage")
    TSubclassOf<ACharacterShowcaseStage> ShowcaseStageClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterScreen|Stage")
    float RotateSpeed = 0.3f;

    // --- 运行时状态 ---

    UPROPERTY(Transient)
    TObjectPtr<ACharacterShowcaseStage> SpawnedStage;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> PreviousViewTarget;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|State")
    FGameplayTag SelectedCharacterTag;

    bool bIsDragging = false;

    // --- 内部逻辑 ---

    void SpawnStageAndTransition();
    void DestroyStageAndRestore();

    UFUNCTION(BlueprintCallable, Category = "CharacterScreen")
    void HandleCharacterSelected(const FGameplayTag& CharacterTag);

    UFUNCTION(BlueprintCallable, Category = "CharacterScreen")
    void HandleNavChanged(int32 NavIndex);

    const UCharacterVisualDataAsset* ResolveVisualData(const FGameplayTag& CharacterTag) const;
};
