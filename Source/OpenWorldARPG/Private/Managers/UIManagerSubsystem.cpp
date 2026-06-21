// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/UIManagerSubsystem.h"
#include "UI/Core/WindowWidgetBase.h"
#include "Data/UIDataAsset.h"
#include "Engine/LocalPlayer.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Core/OpenWorldARPGPlayerController.h"

UWindowWidgetBase* UUIManagerSubsystem::ShowUIByTag(FGameplayTag UITag)
{
    if (!UITag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowUIByTag called with an invalid tag!"));
        return nullptr;
    }

    // 从 GameAssetManagerSubsystem 获取 UI 映射数据资产
    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    UUIDataAsset* LoadedUIData = AssetManager ? AssetManager->GetUIMapDataAsset() : nullptr;
    if (!LoadedUIData)
    {
        UE_LOG(LogTemp, Error, TEXT("UI Map Data Asset failed to load in UIManagerSubsystem!"));
        return nullptr;
    }

    if (const TSubclassOf<UWindowWidgetBase>* WidgetClassPtr = LoadedUIData->UIMap.Find(UITag))
    {
        if (*WidgetClassPtr)
        {
            return OpenUI(*WidgetClassPtr);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowUIByTag: Cannot find UI Class for Tag [%s] in DA_UIMap."), *UITag.ToString());
    }

    return nullptr;
}

UWindowWidgetBase* UUIManagerSubsystem::OpenUI(TSubclassOf<UWindowWidgetBase> WidgetClass)
{
    if (!WidgetClass) return nullptr;

    // 检查是否已经存在一个同类的UI在堆栈中，防止重复打开
    for (UWindowWidgetBase* OpenWidget : UIStack)
    {
        if (OpenWidget && OpenWidget->GetClass() == WidgetClass)
        {
            return OpenWidget;
        }
    }

    // 创建Widget实例
    UWindowWidgetBase* NewWidget = CreateWidget<UWindowWidgetBase>(GetGameInstance(), WidgetClass);
    if (!NewWidget) return nullptr;

    // 将新Widget推入堆栈顶部
    UIStack.Add(NewWidget);
    NewWidget->AddToViewport(UIStack.Num() - 1); // Z-Order根据堆栈深度设置
    NewWidget->OnOpened(); // 调用新Widget的蓝图事件

    // 触发 UI 栈改变的全局广播，不再直接调用 PC
    OnUIStackChanged.Broadcast();

    return NewWidget;
}

void UUIManagerSubsystem::CloseTopUI()
{
    if (UIStack.IsEmpty()) return;

    // 弹出最顶层的Widget
    UWindowWidgetBase* TopWidget = UIStack.Pop();
    if (TopWidget)
    {
        TopWidget->OnClosed(); // 调用顶层Widget的蓝图事件
        TopWidget->RemoveFromParent();
    }

    // 触发 UI 栈改变的全局广播
    OnUIStackChanged.Broadcast();
}

bool UUIManagerSubsystem::IsAnyUIOpen() const
{
    return !UIStack.IsEmpty();
}

UWindowWidgetBase* UUIManagerSubsystem::GetTopWindowWidget() const
{
    return UIStack.IsEmpty() ? nullptr : UIStack.Last();
}