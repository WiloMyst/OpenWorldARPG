// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/TeamManager/UI/TeamSetupScreenWidget.h"
#include "Systems/TeamManager/UI/TeamSetupStage.h"
#include "Systems/TeamManager/UI/TeamSetupSlotWidget.h"
#include "Systems/TeamManager/UI/TeamSetupOwnedCharacterListWidget.h"
#include "Systems/TeamManager/TeamManagerSubsystem.h"
#include "UI/Core/UIManagerSubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"

UTeamSetupScreenWidget::UTeamSetupScreenWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UTeamSetupScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定按钮事件
    if (SaveButton)
    {
        SaveButton->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::SaveAndExit);
    }
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::OnCloseButtonClicked);
    }

    // 绑定 4 个槽位按钮的点击事件（防崩：逐个判空）
    if (SlotButton_0)
    {
        SlotButton_0->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::OnSlot0Clicked);
    }
    if (SlotButton_1)
    {
        SlotButton_1->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::OnSlot1Clicked);
    }
    if (SlotButton_2)
    {
        SlotButton_2->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::OnSlot2Clicked);
    }
    if (SlotButton_3)
    {
        SlotButton_3->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::OnSlot3Clicked);
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UTeamManagerSubsystem* TeamSubsystem = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
            {
                TeamSubsystem->OnTeamListUpdatedDelegate.AddDynamic(this, &UTeamSetupScreenWidget::HandleTeamListUpdated);
            }
        }
    }

    // 初始化 PendingTeam
    InitializePendingTeam();

    // 生成展台、切换视角、设置输入模式（UI 拥有展台生命周期）
    SpawnStageAndTransition();

    // 接入已拥有角色列表子组件：绑定点击委托 + 刷新列表
    if (OwnedCharacterListWidget)
    {
        OwnedCharacterListWidget->OnOwnedCharacterClicked.AddDynamic(this, &UTeamSetupScreenWidget::HandleCharacterPicked);
        OwnedCharacterListWidget->RefreshList(PendingTeam);
    }

    // 刷新 3D 展台
    RefreshStageDisplay();

    // 默认选中第 0 个槽位（C++ 强制视觉反馈：高亮第 0 个槽位按钮）
    SelectSlot(0);
}

void UTeamSetupScreenWidget::NativeDestruct()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UTeamManagerSubsystem* TeamSubsystem = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
            {
                TeamSubsystem->OnTeamListUpdatedDelegate.RemoveDynamic(this, &UTeamSetupScreenWidget::HandleTeamListUpdated);
            }
        }
    }

    // 销毁展台、恢复视角、恢复输入模式
    DestroyStageAndRestore();

    Super::NativeDestruct();
}

void UTeamSetupScreenWidget::SpawnStageAndTransition()
{
    if (!StageClass)
    {
        UE_LOG(LogTemp, Error, TEXT("TeamSetupScreenWidget: StageClass 未配置，无法生成展台！"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    // 1. 在地下深处动态生成展台（避免与大世界几何体重叠）
    const FVector SpawnLocation(0.0f, 0.0f, -20000.0f);
    const FRotator SpawnRotation(0.0f, 0.0f, 0.0f);
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transient; // 不参与序列化，避免被保存到关卡

    SpawnedStage = World->SpawnActor<ATeamSetupStage>(StageClass, SpawnLocation, SpawnRotation, SpawnParams);
    if (!SpawnedStage)
    {
        UE_LOG(LogTemp, Error, TEXT("TeamSetupScreenWidget: SpawnActor ATeamSetupStage 失败！"));
        return;
    }

    // 2. 剥夺控制器自动摄像机管理权，防止服务器 Possess 新角色时客户端瞬间抢夺镜头
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bAutoManageActiveCameraTarget = false;

        // 3. 瞬间切换视角到展台摄像机
        PC->SetViewTarget(SpawnedStage);

        // 4. 设置输入模式为 GameAndUI 并显示鼠标
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(nullptr);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = true;

        // 5. 隐藏大世界主 HUD（RAII：HUD 的显隐与展台生命周期严格绑定）
        if (AGameplayPlayerController* GameplayPC = Cast<AGameplayPlayerController>(PC))
        {
            GameplayPC->SetMainHUDVisible(false);
        }
    }
}

void UTeamSetupScreenWidget::DestroyStageAndRestore()
{
    // 1. 动态获取最新的 Pawn 进行过渡。
    //    注意：先设置 ViewTarget，再恢复 bAutoManageActiveCameraTarget，避免自动管理覆盖。
    if (APlayerController* PC = GetOwningPlayer())
    {
        AActor* ViewTarget = nullptr;
        if (APawn* CurrentPawn = PC->GetPawn())
        {
            ViewTarget = CurrentPawn;
        }
        else if (AActor* CurrentView = PC->GetViewTarget())
        {
            // 兜底：极端情况 Pawn 尚未复制到客户端，退回到当前 ViewTarget
            ViewTarget = CurrentView;
        }

        if (ViewTarget) 
        {
            PC->SetViewTarget(ViewTarget);
        }

        // 2. 恢复控制器自动管理摄像机目标（必须在 SetViewTarget 之后）
        PC->bAutoManageActiveCameraTarget = true;

        // 3. 恢复大世界输入模式
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;

        // 4. 恢复大世界主 HUD 显示（RAII 兜底：无论通过按钮/ESC/死亡任何路径关闭，都能恢复）
        if (AGameplayPlayerController* GameplayPC = Cast<AGameplayPlayerController>(PC))
        {
            GameplayPC->SetMainHUDVisible(true);
        }
    }

    // 5. 销毁展台实例（零内存泄漏）
    if (SpawnedStage)
    {
        SpawnedStage->Destroy();
        SpawnedStage = nullptr;
    }

    bIsDragging = false;
}

void UTeamSetupScreenWidget::InitializePendingTeam()
{
    PendingTeam.Reset();

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UTeamManagerSubsystem* TeamSubsystem = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
            {
                PendingTeam = TeamSubsystem->GetCurrentTeamCharacterTags();
            }
        }
    }

    // 保证 PendingTeam 长度为 MaxTeamSize
    constexpr int32 MaxTeamSize = 4;
    while (PendingTeam.Num() < MaxTeamSize)
    {
        PendingTeam.Add(FGameplayTag::EmptyTag);
    }
}

void UTeamSetupScreenWidget::RefreshStageDisplay()
{
    if (SpawnedStage)
    {
        SpawnedStage->RefreshStage(PendingTeam);
    }
}

void UTeamSetupScreenWidget::AssignCharacterToSlot(int32 SlotIndex, const FGameplayTag& CharacterTag)
{
    if (!PendingTeam.IsValidIndex(SlotIndex)) return;
    constexpr int32 MaxTeamSize = 4;
    if (SlotIndex < 0 || SlotIndex >= MaxTeamSize) return;

    // 若该角色已在其他槽位，则交换
    for (int32 i = 0; i < PendingTeam.Num(); ++i)
    {
        if (i == SlotIndex) continue;
        if (PendingTeam[i] == CharacterTag)
        {
            // 交换：目标槽位的角色放到原槽位
            PendingTeam[i] = PendingTeam[SlotIndex];
            break;
        }
    }

    // 写入新角色
    PendingTeam[SlotIndex] = CharacterTag;

    // 刷新展台
    RefreshStageDisplay();

    // 刷新已拥有角色列表（更新"已上阵"高亮状态）
    if (OwnedCharacterListWidget)
    {
        OwnedCharacterListWidget->RefreshList(PendingTeam);
    }

    // 通知蓝图更新 UI
    OnSlotsChanged(SlotIndex);
}

void UTeamSetupScreenWidget::ClearSlot(int32 SlotIndex)
{
    if (!PendingTeam.IsValidIndex(SlotIndex)) return;

    PendingTeam[SlotIndex] = FGameplayTag::EmptyTag;

    RefreshStageDisplay();

    // 刷新已拥有角色列表（更新"已上阵"高亮状态）
    if (OwnedCharacterListWidget)
    {
        OwnedCharacterListWidget->RefreshList(PendingTeam);
    }

    OnSlotsChanged(SlotIndex);
}

void UTeamSetupScreenWidget::SaveAndExit()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UTeamManagerSubsystem* TeamSubsystem = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>())
            {
                TArray<FGameplayTag> FinalTeam;
                FinalTeam.Reserve(PendingTeam.Num());
                for (const FGameplayTag& Tag : PendingTeam)
                {
                    if (Tag.IsValid())
                    {
                        FinalTeam.Add(Tag);
                    }
                }

                TeamSubsystem->SetCurrentTeam(FinalTeam, 0);
            }
        }
    }

    // 通知服务器在世界中真正刷新角色蓝图实体（销毁旧队伍 → Spawn 新队伍 → Possess）
    if (AGameplayPlayerController* PC = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        // 提取最终的纯净队伍（不含空 Tag）
        TArray<FGameplayTag> FinalTeamForServer;
        FinalTeamForServer.Reserve(PendingTeam.Num());
        for (const FGameplayTag& Tag : PendingTeam)
        {
            if (Tag.IsValid())
            {
                FinalTeamForServer.Add(Tag);
            }
        }

        PC->Server_ApplyTeamChanges(FinalTeamForServer, 0);

        // 保存后不关闭界面，玩家可继续查看/调整队伍
    }
}

void UTeamSetupScreenWidget::OnCloseButtonClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
        {
            UIManager->CloseTopUI();
        }
    }
}

void UTeamSetupScreenWidget::HandleTeamListUpdated()
{
    // 外部修改了队伍（如其他系统），同步本地 PendingTeam
    InitializePendingTeam();
    RefreshStageDisplay();

    // 刷新已拥有角色列表（外部队伍变化可能导致"已上阵"状态改变）
    if (OwnedCharacterListWidget)
    {
        OwnedCharacterListWidget->RefreshList(PendingTeam);
    }

    OnSlotsChanged(-1);
}

void UTeamSetupScreenWidget::HandleCharacterPicked(const FGameplayTag& CharacterTag)
{
    // 直接将角色放入当前选中的槽位
    AssignCharacterToSlot(SelectedSlotIndex, CharacterTag);

    // 广播事件供蓝图扩展（如关闭头像列表弹窗）
    OnCharacterPicked.Broadcast(CharacterTag);
}

void UTeamSetupScreenWidget::SelectSlot(int32 SlotIndex)
{
    constexpr int32 MaxTeamSize = 4;
    if (SlotIndex < 0 || SlotIndex >= MaxTeamSize) return;

    SelectedSlotIndex = SlotIndex;

    UButton* Buttons[MaxTeamSize] = { SlotButton_0, SlotButton_1, SlotButton_2, SlotButton_3 };
    for (int32 i = 0; i < MaxTeamSize; ++i)
    {
        if (Buttons[i])
        {
            const FLinearColor Color = (i == SlotIndex)
                ? FLinearColor(1.0f, 1.0f, 1.0f, 0.1f)   // 选中：半透明
                : FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);  // 未选中：透明
            Buttons[i]->SetBackgroundColor(Color);
        }
    }

    // 依然保留通知蓝图的接口以备后用
    OnSlotSelected(SelectedSlotIndex);
}

void UTeamSetupScreenWidget::OnSlot0Clicked() { SelectSlot(0); }
void UTeamSetupScreenWidget::OnSlot1Clicked() { SelectSlot(1); }
void UTeamSetupScreenWidget::OnSlot2Clicked() { SelectSlot(2); }
void UTeamSetupScreenWidget::OnSlot3Clicked() { SelectSlot(3); }

// --- 输入处理：拖拽旋转展台 ---

FReply UTeamSetupScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bIsDragging = true;
        LastMousePosition = InMouseEvent.GetScreenSpacePosition();

        // 捕获鼠标，确保拖拽过程中即使移出 Widget 也能收到 MouseMove
        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    return FReply::Unhandled();
}

FReply UTeamSetupScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDragging)
    {
        bIsDragging = false;

        // 释放鼠标捕获
        if (InMouseEvent.GetCursorDelta().IsNearlyZero())
        {
            // 仅点击未拖拽：不处理
        }

        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

FReply UTeamSetupScreenWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bIsDragging || !SpawnedStage)
    {
        return FReply::Unhandled();
    }

    // 计算鼠标 X 轴增量，旋转展台角色
    const FVector2D CurrentPos = InMouseEvent.GetScreenSpacePosition();
    const float DeltaX = CurrentPos.X - LastMousePosition.X;
    LastMousePosition = CurrentPos;

    if (!FMath::IsNearlyZero(DeltaX))
    {
        SpawnedStage->RotateCharacters(DeltaX * RotateSpeed);
    }

    return FReply::Handled();
}
