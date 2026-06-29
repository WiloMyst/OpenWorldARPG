// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/TeamManager/TeamSetupScreenWidget.h"
#include "UI/Subsystems/TeamManager/TeamSetupStage.h"
#include "UI/Subsystems/TeamManager/TeamSetupSlotWidget.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Data/CharacterRegistryRow.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
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
    if (CancelButton)
    {
        CancelButton->OnClicked.AddDynamic(this, &UTeamSetupScreenWidget::CancelAndExit);
    }

    // 监听 TeamManagerSubsystem 的队伍变化
    if (UTeamManagerSubsystem* TeamSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>() : nullptr)
    {
        TeamSubsystem->OnTeamListUpdatedDelegate.AddDynamic(this, &UTeamSetupScreenWidget::HandleTeamListUpdated);
    }

    // 初始化 PendingTeam
    InitializePendingTeam();

    // 生成展台、切换视角、设置输入模式（UI 拥有展台生命周期）
    SpawnStageAndTransition();

    // 刷新 UI 与 3D 展台
    RefreshOwnedCharacterList();
    RefreshStageDisplay();
}

void UTeamSetupScreenWidget::NativeDestruct()
{
    // 解绑委托避免悬空指针
    if (UTeamManagerSubsystem* TeamSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>() : nullptr)
    {
        TeamSubsystem->OnTeamListUpdatedDelegate.RemoveDynamic(this, &UTeamSetupScreenWidget::HandleTeamListUpdated);
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

    // 2. 记录当前 ViewTarget（通常是大世界主角），关闭时切回去
    if (APlayerController* PC = GetOwningPlayer())
    {
        PreviousViewTarget = PC->GetViewTarget();

        // 3. 平滑切换视角到展台摄像机
        PC->SetViewTargetWithBlend(SpawnedStage, 0.3f);

        // 4. 设置输入模式为 GameAndUI 并显示鼠标
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(nullptr);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = true;
    }
}

void UTeamSetupScreenWidget::DestroyStageAndRestore()
{
    // 1. 恢复视角到打开前的目标
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AActor* PrevTarget = PreviousViewTarget.Get())
        {
            PC->SetViewTargetWithBlend(PrevTarget, 0.3f);
        }

        // 2. 恢复大世界输入模式
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }

    // 3. 销毁展台实例（零内存泄漏）
    if (SpawnedStage)
    {
        SpawnedStage->Destroy();
        SpawnedStage = nullptr;
    }

    PreviousViewTarget = nullptr;
    bIsDragging = false;
}

void UTeamSetupScreenWidget::InitializePendingTeam()
{
    PendingTeam.Reset();

    if (UTeamManagerSubsystem* TeamSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>() : nullptr)
    {
        PendingTeam = TeamSubsystem->GetCurrentTeamCharacterTags();
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

void UTeamSetupScreenWidget::RefreshOwnedCharacterList()
{
    if (!OwnedCharacterList || !SlotItemClass) return;

    OwnedCharacterList->ClearChildren();

    UCharacterManagerSubsystem* CharSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>() : nullptr;
    if (!CharSubsystem) return;

    TArray<FCharacterSaveData> OwnedCharacters = CharSubsystem->GetAllOwnedCharacterSaveData();

    for (const FCharacterSaveData& SaveData : OwnedCharacters)
    {
        if (!SaveData.CharacterTag.IsValid()) continue;

        FCharacterRegistryRow Row;
        const bool bHasRow = CharSubsystem->GetCharacterRegistryRowByTag(SaveData.CharacterTag, Row);

        UTeamSetupSlotWidget* Item = CreateWidget<UTeamSetupSlotWidget>(this, SlotItemClass);
        if (Item)
        {
            UTexture2D* HeadIcon = bHasRow ? Row.HeadIcon.Get() : nullptr;
            const FText DisplayName = bHasRow ? Row.CharacterName : FText::FromName(SaveData.CharacterTag.GetTagName());
            Item->InitializeSlot(SaveData.CharacterTag, DisplayName, HeadIcon, -1);
            Item->OnSlotClicked.AddDynamic(this, &UTeamSetupScreenWidget::HandleCharacterPicked);

            OwnedCharacterList->AddChild(Item);
        }
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

    // 通知蓝图更新 UI
    OnSlotsChanged(SlotIndex);
}

void UTeamSetupScreenWidget::ClearSlot(int32 SlotIndex)
{
    if (!PendingTeam.IsValidIndex(SlotIndex)) return;

    PendingTeam[SlotIndex] = FGameplayTag::EmptyTag;

    RefreshStageDisplay();
    OnSlotsChanged(SlotIndex);
}

void UTeamSetupScreenWidget::SaveAndExit()
{
    if (UTeamManagerSubsystem* TeamSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>() : nullptr)
    {
        // 移除 PendingTeam 末尾的空 Tag，避免空槽位写入 Subsystem
        TArray<FGameplayTag> FinalTeam;
        FinalTeam.Reserve(PendingTeam.Num());
        for (const FGameplayTag& Tag : PendingTeam)
        {
            if (Tag.IsValid())
            {
                FinalTeam.Add(Tag);
            }
        }

        // 写入 Subsystem（保留原激活角色索引 0）
        TeamSubsystem->SetCurrentTeam(FinalTeam, 0);
    }

    // 请求 PlayerController 关闭界面（触发 NativeDestruct 销毁展台并恢复视角）
    if (AGameplayPlayerController* PC = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        PC->CloseTeamSetupScreen();
    }
}

void UTeamSetupScreenWidget::CancelAndExit()
{
    // 不写入 Subsystem，直接关闭界面（NativeDestruct 会销毁展台并恢复视角）
    if (AGameplayPlayerController* PC = Cast<AGameplayPlayerController>(GetOwningPlayer()))
    {
        PC->CloseTeamSetupScreen();
    }
}

void UTeamSetupScreenWidget::HandleTeamListUpdated()
{
    // 外部修改了队伍（如其他系统），同步本地 PendingTeam
    InitializePendingTeam();
    RefreshStageDisplay();
    OnSlotsChanged(-1);
}

void UTeamSetupScreenWidget::HandleCharacterPicked(const FGameplayTag& CharacterTag, int32 SourceSlotIndex)
{
    int32 TargetSlot = -1;

    if (SourceSlotIndex >= 0 && SourceSlotIndex < PendingTeam.Num())
    {
        // 从 DropZone 点击：清空该槽位
        if (PendingTeam[SourceSlotIndex] == CharacterTag)
        {
            ClearSlot(SourceSlotIndex);
            OnCharacterPicked.Broadcast(CharacterTag);
            return;
        }
        TargetSlot = SourceSlotIndex;
    }
    else
    {
        // 从头像列表点击：寻找第一个空槽位
        for (int32 i = 0; i < PendingTeam.Num(); ++i)
        {
            if (!PendingTeam[i].IsValid())
            {
                TargetSlot = i;
                break;
            }
        }

        // 若没有空槽位，覆盖第 0 个槽位
        if (TargetSlot == -1)
        {
            TargetSlot = 0;
        }
    }

    AssignCharacterToSlot(TargetSlot, CharacterTag);

    // 广播事件供蓝图扩展（如关闭头像列表弹窗）
    OnCharacterPicked.Broadcast(CharacterTag);
}

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
