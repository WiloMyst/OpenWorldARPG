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
 * - 底部：保存 / 退出按钮
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
     * 保存并退出：将 PendingTeam 写入 TeamManagerSubsystem 并关闭界面。
     * 由蓝图"保存"按钮调用。
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void SaveAndExit();

    /** 不保存直接退出（恢复原队伍显示） */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void CancelAndExit();

protected:
    // --- 绑定控件（蓝图配置） ---

    /** 4 个槽位 DropZone 的 Button（位置与展台槽位对应） */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TArray<TObjectPtr<UButton>> SlotButtons;

    /** 底部"保存"按钮 */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SaveButton;

    /** 底部"取消"按钮 */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> CancelButton;

    /** 已拥有角色头像列表容器（用于拖拽上场） */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UPanelWidget> OwnedCharacterList;

    /** 头像 Item Widget 类（蓝图配置） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|Config")
    TSubclassOf<UTeamSetupSlotWidget> SlotItemClass;

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

    /** 打开界面前摄像机看着的目标（通常是大世界主角），关闭时切回去 */
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> PreviousViewTarget;

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

    /** 刷新已拥有角色头像列表 */
    void RefreshOwnedCharacterList();

    /** Subsystem 队伍变化的回调（外部修改时同步） */
    UFUNCTION()
    void HandleTeamListUpdated();

    /** 头像被点击/拖拽时触发，请求将该角色放入指定槽位（默认放第一个空槽） */
    UFUNCTION()
    void HandleCharacterPicked(const FGameplayTag& CharacterTag, int32 SourceSlotIndex);

public:
    /** 当角色被选中/拖拽时触发（向上传递，蓝图可绑定） */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterPickedSignature, const FGameplayTag&, CharacterTag);

    UPROPERTY(BlueprintAssignable, Category = "TeamSetup|Events")
    FOnCharacterPickedSignature OnCharacterPicked;

protected:
    /** 槽位发生变化的视觉更新（蓝图实现：更新 DropZone 高亮 / 名称） */
    UFUNCTION(BlueprintImplementableEvent, Category = "TeamSetup")
    void OnSlotsChanged(int32 ChangedSlotIndex);
};
