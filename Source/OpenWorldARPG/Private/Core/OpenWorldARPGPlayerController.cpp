// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Core/OpenWorldARPGPlayerController.h"
#include "Managers/UIManagerSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/Core/WindowWidgetBase.h"

void AOpenWorldARPGPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 绑定输入映射上下文
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // 订阅 UIManager 的 UI 栈变化广播
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        // 当收到 UI 发生变化的广播时，触发本类的 UpdateInputMode 进行重新仲裁
        UIManager->OnUIStackChanged.AddDynamic(this, &AOpenWorldARPGPlayerController::UpdateInputMode);
    }
}


//////////////////////////////////////////////////////////////////////////
// Input

void AOpenWorldARPGPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // --- 绑定动作与回调函数 ---

        // 绑定Alt键按下事件
        EnhancedInputComponent->BindAction(IA_ShowCursor, ETriggerEvent::Started, this, &AOpenWorldARPGPlayerController::ShowCursorTemporarily);

        // 绑定Alt键松开事件
        EnhancedInputComponent->BindAction(IA_ShowCursor, ETriggerEvent::Completed, this, &AOpenWorldARPGPlayerController::HideCursorTemporarily);


    }
}

void AOpenWorldARPGPlayerController::ShowCursorTemporarily(const FInputActionValue& Value)
{
    bIsAltKeyDown = true;
    UpdateInputMode(); // 交给仲裁器处理
}

void AOpenWorldARPGPlayerController::HideCursorTemporarily(const FInputActionValue& Value)
{
    bIsAltKeyDown = false;
    UpdateInputMode(); // 交给仲裁器处理
}

void AOpenWorldARPGPlayerController::UpdateInputMode()
{
    UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    
    UWindowWidgetBase* TopUI = UIManager ? UIManager->GetTopWindowWidget() : nullptr; 

    // 优先级 1：当前有 WindowWidgetBase 处于打开状态
    if (TopUI)
    {
        bShowMouseCursor = true;
        
        TSharedPtr<SWidget> SlateWidget = TopUI->GetCachedWidget();

        if (TopUI->InputModeWhenOpen == EWidgetInputMode::UIOnly)
        {
            FInputModeUIOnly InputMode;
            if (SlateWidget.IsValid()) InputMode.SetWidgetToFocus(SlateWidget);
            SetInputMode(InputMode);
        }
        else if (TopUI->InputModeWhenOpen == EWidgetInputMode::GameAndUI)
        {
            FInputModeGameAndUI InputMode;
            if (SlateWidget.IsValid()) InputMode.SetWidgetToFocus(SlateWidget);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);
        }
        
        // 直接呼叫 UMG 层的 SetFocus，确保键盘/手柄事件能直接路由给它
        TopUI->SetFocus();

        // 强制释放底层捕获，防止焦点滞留
        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().ReleaseMouseCapture();
        }
        return;
    }

    // 优先级 2：没有打开的 Window UI，但玩家正在按住 Alt 键
    if (bIsAltKeyDown)
    {
        bShowMouseCursor = true;
        FInputModeGameAndUI InputMode;
        
        // 如果主 HUD 存在，把它设为焦点目标
        // 这样玩家按下 Alt 键时，焦点就已经在 UI 层了，点背包图标只需点【一次】
        if (MainHUDInstance && MainHUDInstance->GetCachedWidget().IsValid())
        {
            InputMode.SetWidgetToFocus(MainHUDInstance->GetCachedWidget());
        }

        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);

        // 强制释放游戏视口的鼠标捕获
        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().ReleaseMouseCapture();
        }
    }
    // 优先级 3：默认战斗探索状态
    else
    {
        bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
    }
}
