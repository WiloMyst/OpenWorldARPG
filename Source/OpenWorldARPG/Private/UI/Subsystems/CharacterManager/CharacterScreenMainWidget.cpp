// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/CharacterManager/CharacterScreenMainWidget.h"
#include "UI/Subsystems/CharacterManager/CharacterShowcaseStage.h"
#include "UI/Subsystems/CharacterManager/CharacterCarouselWidget.h"
#include "UI/Subsystems/CharacterManager/CharacterNavMenuWidget.h"
#include "UI/Subsystems/CharacterManager/CharacterDetailsAttributesWidget.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Managers/UIManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Components/Button.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UCharacterScreenMainWidget::UCharacterScreenMainWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCharacterScreenMainWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定关闭按钮
    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UCharacterScreenMainWidget::OnCloseButtonClicked);
    }

    // 绑定 Carousel 的选中委托
    if (CarouselPanel)
    {
        CarouselPanel->OnCharacterSelected.AddDynamic(this, &UCharacterScreenMainWidget::HandleCharacterSelected);
    }

    // 绑定 NavMenu 的导航切换委托
    if (NavMenuPanel)
    {
        NavMenuPanel->OnNavChanged.AddDynamic(this, &UCharacterScreenMainWidget::HandleNavChanged);
    }

    // 生成展台、切换视角、设置输入模式（UI 拥有展台生命周期）
    SpawnStageAndTransition();

    // 【初始选中逻辑：优先选中当前控制角色，保底选中列表第一个】
    // 延迟一帧触发，确保 Carousel 内部的子 Widget 已全部生成完毕
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (!CarouselPanel) return;

        FGameplayTag TargetTag;
        if (UTeamManagerSubsystem* TeamSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>() : nullptr)
        {
            TargetTag = TeamSubsystem->GetActiveCharacterTag();
        }

        // 【保底机制】：如果没获取到当前控制角色，强制选中列表里的第一个角色
        if (!TargetTag.IsValid())
        {
            TargetTag = CarouselPanel->GetFirstCharacterTag();
        }

        if (TargetTag.IsValid())
        {
            CarouselPanel->HandleItemSelected(TargetTag);
        }
    });
}

void UCharacterScreenMainWidget::OnCloseButtonClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UUIManagerSubsystem* UIManager = PC->GetLocalPlayer()->GetSubsystem<UUIManagerSubsystem>())
        {
            UIManager->CloseTopUI();
        }
    }
}

void UCharacterScreenMainWidget::NativeDestruct()
{
    // 销毁展台、恢复视角、恢复输入模式
    DestroyStageAndRestore();

    Super::NativeDestruct();
}

void UCharacterScreenMainWidget::SpawnStageAndTransition()
{
    if (!ShowcaseStageClass)
    {
        UE_LOG(LogTemp, Error, TEXT("CharacterScreenMainWidget: ShowcaseStageClass 未配置，无法生成展台！"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    // 1. 在地下深处动态生成展台（避免与大世界几何体重叠）
    const FVector SpawnLocation(0.0f, 0.0f, -30000.0f);
    const FRotator SpawnRotation(0.0f, 0.0f, 0.0f);
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transient; // 不参与序列化，避免被保存到关卡

    SpawnedStage = World->SpawnActor<ACharacterShowcaseStage>(ShowcaseStageClass, SpawnLocation, SpawnRotation, SpawnParams);
    if (!SpawnedStage)
    {
        UE_LOG(LogTemp, Error, TEXT("CharacterScreenMainWidget: SpawnActor ACharacterShowcaseStage 失败！"));
        return;
    }

    // 2. 记录当前 ViewTarget（通常是大世界主角），关闭时切回去
    if (APlayerController* PC = GetOwningPlayer())
    {
        PreviousViewTarget = PC->GetViewTarget();

        // 3. 平滑切换视角到展台摄像机
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

void UCharacterScreenMainWidget::DestroyStageAndRestore()
{
    // 1. 恢复视角到打开前的目标
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AActor* PrevTarget = PreviousViewTarget.Get())
        {
            PC->SetViewTarget(PrevTarget);
        }

        // 2. 恢复大世界输入模式
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;

        // 3. 恢复大世界主 HUD 显示（RAII 兜底：无论通过按钮/ESC/死亡任何路径关闭，都能恢复）
        if (AGameplayPlayerController* GameplayPC = Cast<AGameplayPlayerController>(PC))
        {
            GameplayPC->SetMainHUDVisible(true);
        }
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

void UCharacterScreenMainWidget::HandleCharacterSelected(const FGameplayTag& CharacterTag)
{
    SelectedCharacterTag = CharacterTag;

    // 1. 刷新右侧详情面板
    if (DetailsPanel)
    {
        DetailsPanel->RefreshData(CharacterTag);
    }

    // 2. 切换 3D 展台模型
    if (SpawnedStage)
    {
        if (const UCharacterVisualDataAsset* VisualData = ResolveVisualData(CharacterTag))
        {
            SpawnedStage->SwitchDisplayCharacter(VisualData);
        }
    }
}

const UCharacterVisualDataAsset* UCharacterScreenMainWidget::ResolveVisualData(const FGameplayTag& CharacterTag) const
{
    UCharacterManagerSubsystem* Subsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>() : nullptr;
    if (!Subsystem) return nullptr;

    FCharacterRegistryRow Row;
    if (Subsystem->GetCharacterRegistryRowByTag(CharacterTag, Row))
    {
        return Row.VisualData.LoadSynchronous();
    }

    return nullptr;
}

void UCharacterScreenMainWidget::HandleNavChanged(int32 NavIndex)
{
    // 根据导航页签切换右侧面板内容（蓝图侧可重写实现更复杂的页面切换）
    // 0=属性, 1=武器, 2=圣遗物, 3=命之座, 4=天赋, 5=资料
    // C++ 基类仅处理属性面板的可见性，其他页面在蓝图层扩展
    if (DetailsPanel)
    {
        DetailsPanel->SetVisibility(NavIndex == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

// --- UMG 原生鼠标事件：拖拽旋转展台 ---

FReply UCharacterScreenMainWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bIsDragging = true;

        // 捕获鼠标，确保拖拽过程中即使移出 Widget 也能收到 MouseMove
        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    return FReply::Unhandled();
}

FReply UCharacterScreenMainWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsDragging)
    {
        bIsDragging = false;

        // 释放鼠标捕获
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

FReply UCharacterScreenMainWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bIsDragging || !SpawnedStage || !HasMouseCapture())
    {
        return FReply::Unhandled();
    }

    // 获取鼠标 X 轴位移量，旋转展台角色
    const float DeltaX = InMouseEvent.GetCursorDelta().X;

    if (!FMath::IsNearlyZero(DeltaX))
    {
        SpawnedStage->RotateCharacter(DeltaX * RotateSpeed);
    }

    return FReply::Handled();
}
