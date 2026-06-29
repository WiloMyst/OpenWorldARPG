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
 *
 * 【设计原则：UI 拥有并管理展台】
 * - 透明背景的 UMG，透出背后的 3D 展台 ACharacterShowcaseStage。
 * - 展台在 NativeConstruct 中动态 SpawnActor 生成，在 NativeDestruct 中 Destroy。
 *   不依赖场景中预先放置的实例，跨关卡通用。
 * - 视角切换、输入模式切换全部由 UI 自己管理，PlayerController 只负责调用 UIManager 打开/关闭。
 * - 鼠标拖拽旋转使用 UMG 原生鼠标事件拦截，不影响大世界 Enhanced Input。
 *
 * 【三段式布局】
 * - 顶部：角色头像轮播（Carousel）
 * - 左侧：导航菜单（NavMenu，切换属性/武器/圣遗物/命之座/天赋/资料）
 * - 右侧：详情面板（Details，根据导航选择切换内容）
 * - 中央大部分区域透明，透出背后的 ShowcaseCamera 画面
 *
 * 【数据流向】
 * 1. Carousel 点击头像 → 触发 OnCharacterSelected 委托
 * 2. MainWidget 接收委托 → 通知 SpawnedStage 切换模型 + 通知 DetailsWidget 刷新数据
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

    // --- 子面板引用（蓝图绑定） ---

    /** 关闭按钮 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    /** 顶部角色轮播面板 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Panels", meta = (BindWidget))
    TObjectPtr<UCharacterCarouselWidget> CarouselPanel;

    /** 左侧导航菜单面板 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Panels", meta = (BindWidget))
    TObjectPtr<UCharacterNavMenuWidget> NavMenuPanel;

    /** 右侧详情面板容器（属性页签） */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|Panels", meta = (BindWidget))
    TObjectPtr<UCharacterDetailsAttributesWidget> DetailsPanel;

protected:
    // --- 展台配置（蓝图配置要生成的展台类） ---

    /** 要动态生成的展台 Actor 类（蓝图必须配置，否则无法打开界面） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterScreen|Stage")
    TSubclassOf<ACharacterShowcaseStage> ShowcaseStageClass;

    /** 拖拽旋转速度（鼠标 X 增量乘以该值得到旋转角度） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterScreen|Stage")
    float RotateSpeed = 0.3f;

    // --- 运行时状态 ---

    /** UI 拥有的展台实例（NativeConstruct 生成，NativeDestruct 销毁） */
    UPROPERTY(Transient)
    TObjectPtr<ACharacterShowcaseStage> SpawnedStage;

    /** 打开界面前摄像机看着的目标（通常是大世界主角），关闭时切回去 */
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> PreviousViewTarget;

    /** 当前选中的角色 Tag */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterScreen|State")
    FGameplayTag SelectedCharacterTag;

    /** 是否正在拖拽旋转展台 */
    bool bIsDragging = false;

    // --- 内部逻辑 ---

    /** 生成展台并切换视角 + 输入模式（NativeConstruct 调用） */
    void SpawnStageAndTransition();

    /** 销毁展台并恢复视角 + 输入模式（NativeDestruct 调用） */
    void DestroyStageAndRestore();

    /** Carousel 选中角色时的回调 */
    UFUNCTION(BlueprintCallable, Category = "CharacterScreen")
    void HandleCharacterSelected(const FGameplayTag& CharacterTag);

    /** NavMenu 切换页签时的回调 */
    UFUNCTION(BlueprintCallable, Category = "CharacterScreen")
    void HandleNavChanged(int32 NavIndex);

    /**
     * 根据角色 Tag 查询对应的 VisualDataAsset。
     * 通过 CharacterManagerSubsystem → CharacterRegistryRow → VisualData 桥梁获取。
     */
    const UCharacterVisualDataAsset* ResolveVisualData(const FGameplayTag& CharacterTag) const;
};
