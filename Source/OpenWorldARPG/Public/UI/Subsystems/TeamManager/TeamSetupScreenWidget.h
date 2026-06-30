// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Core/WindowWidgetBase.h"
#include "TeamSetupScreenWidget.generated.h"

class ATeamSetupStage;
class UTeamManagerSubsystem;
class UCharacterManagerSubsystem;
class UButton;
class UHorizontalBox;
class UWrapBox;
class UPanelWidget;
class UTeamSetupSlotWidget;
class UTeamSetupOwnedCharacterListWidget;

/**
 * 编队界面主壳子（对应 WBP_TeamSetupScreen）。
 *
 * 【设计原则：UI 拥有并管理展台】
 * - 透明背景的 UMG，透出背后的 3D 展台 ATeamSetupStage。
 * - 展台在 NativeConstruct 中动态 SpawnActor 生成，在 NativeDestruct 中 Destroy。
 *   不依赖场景中预先放置的实例，跨关卡通用。
 * - 视角切换、输入模式切换全部由 UI 自己管理，PlayerController 只负责调用 UIManager 打开/关闭。
 * - UI 仅负责逻辑排布和调用 Subsystem 更新数据，绝对不生成真实 APlayerCharacter。
 * - 玩家在界面中操作时，先在本地 PendingTeam 数组中调整，
 *   实时调用 ATeamSetupStage::RefreshStage(PendingTeam) 刷新 3D 模型。
 * - 仅当点击"保存"或退出界面时，才调用 UTeamManagerSubsystem::SetCurrentTeam 持久化。
 *
 * 【布局】
 * - 底部：保存按钮（保存后不关闭界面）
 * - 右上角：关闭按钮（走 UIManager 关闭）
 * - 中部：4 个透明可交互 DropZone（位置与展台 4 个槽位大致重合）
 * - 顶部 / 侧边：角色头像列表（已拥有角色，供拖拽上场）
 *
 * 【输入路由】
 * 鼠标左键拖拽时，UI 直接调用 SpawnedStage->RotateCharacters(MouseDelta) 旋转展台角色。
 */
UCLASS()
class OPENWORLDARPG_API UTeamSetupScreenWidget : public UWindowWidgetBase
{
    GENERATED_BODY()

public:
    UTeamSetupScreenWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- 输入处理（拖拽旋转展台） ---

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // --- 外部接口 ---

    /** 获取当前 Pending 队伍（未保存） */
    UFUNCTION(BlueprintPure, Category = "TeamSetup")
    TArray<FGameplayTag> GetPendingTeam() const { return PendingTeam; }

    /** 获取 UI 持有的展台实例（生命周期由 UI 管理） */
    UFUNCTION(BlueprintPure, Category = "TeamSetup")
    ATeamSetupStage* GetSpawnedStage() const { return SpawnedStage; }

    /** 关闭按钮点击回调（与 CharacterScreenMainWidget 一致，统一走 UIManager 关闭） */
    UFUNCTION()
    void OnCloseButtonClicked();

    // --- 槽位操作（供蓝图 DropZone 调用） ---

    /**
     * 将指定角色放入指定槽位。
     * 若该角色已在其他槽位，则交换两个槽位的角色。
     * 若目标槽位已有角色，原角色被替换下场（从 PendingTeam 移除）。
     * 操作后立即刷新 3D 展台。
     *
     * @param SlotIndex 目标槽位 (0~3)
     * @param CharacterTag 要放入的角色 Tag（无效则清空该槽位）
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void AssignCharacterToSlot(int32 SlotIndex, const FGameplayTag& CharacterTag);

    /** 清空指定槽位（角色下场） */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void ClearSlot(int32 SlotIndex);

    /**
     * 选中指定槽位（标记为当前点击列表头像时放入的目标槽位）。
     * 会触发 OnSlotSelected 蓝图事件以更新 4 个槽位的高亮表现框。
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void SelectSlot(int32 SlotIndex);

    /** 获取当前选中的槽位索引 */
    UFUNCTION(BlueprintPure, Category = "TeamSetup")
    int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

    /**
     * 保存：将 PendingTeam 写入 TeamManagerSubsystem 并通知服务器 Spawn 新队伍。
     * 注意：保存后不关闭界面，玩家可继续查看/调整。
     * 由蓝图"保存"按钮调用。
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void SaveAndExit();

protected:
    // --- 绑定控件（蓝图配置） ---

    /**
     * 4 个槽位 DropZone 的 Button（位置与展台槽位对应）。
     * 蓝图侧在 Widget 中放置 4 个 Button，分别命名为 SlotButton_0 ~ SlotButton_3 即可自动绑定。
     */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_0;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_1;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_2;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_3;

    /** 底部"保存"按钮 */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SaveButton;

    /** 右上角"关闭"按钮（与 CharacterScreenMainWidget 行为一致：仅调用 UIManager->CloseTopUI） */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    /** 已拥有角色列表子组件（管理头像生成/排布，职责单一） */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UTeamSetupOwnedCharacterListWidget> OwnedCharacterListWidget;

    // --- 展台配置（蓝图配置要生成的展台类） ---

    /** 要动态生成的展台 Actor 类（蓝图必须配置，否则无法打开界面） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|Stage")
    TSubclassOf<ATeamSetupStage> StageClass;

    /** 拖拽旋转速度（鼠标 X 增量乘以该值得到旋转角度） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|Stage")
    float RotateSpeed = 0.3f;

    // --- 运行时状态 ---

    /** UI 拥有的展台实例（NativeConstruct 生成，NativeDestruct 销毁） */
    UPROPERTY(Transient)
    TObjectPtr<ATeamSetupStage> SpawnedStage;

    /** 本地 Pending 队伍（未保存） */
    UPROPERTY(Transient)
    TArray<FGameplayTag> PendingTeam;

    /** 是否正在拖拽旋转展台 */
    bool bIsDragging = false;

    /** 上一帧鼠标位置（用于计算增量） */
    FVector2D LastMousePosition = FVector2D::ZeroVector;

    // --- 内部逻辑 ---

    /** 生成展台并切换视角 + 输入模式（NativeConstruct 调用） */
    void SpawnStageAndTransition();

    /** 销毁展台并恢复视角 + 输入模式（NativeDestruct 调用） */
    void DestroyStageAndRestore();

    /** 从 Subsystem 初始化 PendingTeam 并刷新展台 */
    void InitializePendingTeam();

    /** 刷新 3D 展台显示 */
    void RefreshStageDisplay();

    /** Subsystem 队伍变化的回调（外部修改时同步） */
    UFUNCTION()
    void HandleTeamListUpdated();

    /**
     * 已拥有角色列表点击回调（由 OwnedCharacterListWidget 委托触发）。
     * 直接将角色放入当前选中的槽位 SelectedSlotIndex。
     */
    UFUNCTION()
    void HandleCharacterPicked(const FGameplayTag& CharacterTag);

    // --- 槽位按钮点击回调（4 个无参函数，供 NativeConstruct 绑定） ---

    UFUNCTION() void OnSlot0Clicked();
    UFUNCTION() void OnSlot1Clicked();
    UFUNCTION() void OnSlot2Clicked();
    UFUNCTION() void OnSlot3Clicked();

public:
    /** 当角色被选中/拖拽时触发 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterPickedSignature, const FGameplayTag&, CharacterTag);

    UPROPERTY(BlueprintAssignable, Category = "TeamSetup|Events")
    FOnCharacterPickedSignature OnCharacterPicked;

protected:
    /** 槽位发生变化的视觉更新（蓝图实现：更新 DropZone 高亮 / 名称） */
    UFUNCTION(BlueprintImplementableEvent, Category = "TeamSetup")
    void OnSlotsChanged(int32 ChangedSlotIndex);

    /** 选中槽位变化的视觉更新（蓝图实现：更新 4 个槽位的高亮表现框） */
    UFUNCTION(BlueprintImplementableEvent, Category = "TeamSetup")
    void OnSlotSelected(int32 SelectedIndex);

    // --- 选中槽位状态 ---

    /** 当前选中的槽位索引（默认 0，点击列表头像时放入此槽位） */
    int32 SelectedSlotIndex = 0;
};
