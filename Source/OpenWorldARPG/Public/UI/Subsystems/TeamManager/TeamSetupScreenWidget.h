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
 * 透明背景 UMG，透出背后的 3D 展台 ATeamSetupStage。
 * 展台由 UI 在 NativeConstruct 中动态 Spawn，NativeDestruct 中 Destroy，跨关卡通用。
 * 操作先写入本地 PendingTeam，点击保存时才持久化到 TeamManagerSubsystem。
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

    UFUNCTION(BlueprintPure, Category = "TeamSetup")
    TArray<FGameplayTag> GetPendingTeam() const { return PendingTeam; }

    UFUNCTION(BlueprintPure, Category = "TeamSetup")
    ATeamSetupStage* GetSpawnedStage() const { return SpawnedStage; }

    UFUNCTION()
    void OnCloseButtonClicked();

    // --- 槽位操作 ---

    /** 将角色放入指定槽位（已存在则交换，目标有角色则替换下场） */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void AssignCharacterToSlot(int32 SlotIndex, const FGameplayTag& CharacterTag);

    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void ClearSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void SelectSlot(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category = "TeamSetup")
    int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

    /** 保存 PendingTeam 到 TeamManagerSubsystem，保存后不关闭界面 */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup")
    void SaveAndExit();

protected:
    // --- 绑定控件 ---

    /** 4 个槽位 DropZone Button（蓝图命名为 SlotButton_0~3） */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_0;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_1;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_2;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SlotButton_3;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> SaveButton;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> CloseButton;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|Widgets", meta = (BindWidget))
    TObjectPtr<UTeamSetupOwnedCharacterListWidget> OwnedCharacterListWidget;

    // --- 展台配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|Stage")
    TSubclassOf<ATeamSetupStage> StageClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|Stage")
    float RotateSpeed = 0.3f;

    // --- 运行时状态 ---

    UPROPERTY(Transient)
    TObjectPtr<ATeamSetupStage> SpawnedStage;

    UPROPERTY(Transient)
    TArray<FGameplayTag> PendingTeam;

    bool bIsDragging = false;
    FVector2D LastMousePosition = FVector2D::ZeroVector;

    // --- 内部逻辑 ---

    void SpawnStageAndTransition();
    void DestroyStageAndRestore();
    void InitializePendingTeam();
    void RefreshStageDisplay();

    UFUNCTION()
    void HandleTeamListUpdated();

    UFUNCTION()
    void HandleCharacterPicked(const FGameplayTag& CharacterTag);

    // --- 槽位按钮回调 ---

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
    UFUNCTION(BlueprintImplementableEvent, Category = "TeamSetup")
    void OnSlotsChanged(int32 ChangedSlotIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "TeamSetup")
    void OnSlotSelected(int32 SelectedIndex);

    int32 SelectedSlotIndex = 0;
};
