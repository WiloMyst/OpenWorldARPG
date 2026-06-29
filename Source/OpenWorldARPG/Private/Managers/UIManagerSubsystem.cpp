// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/UIManagerSubsystem.h"
#include "UI/Core/WindowWidgetBase.h"
#include "UI/Screens/LoadingScreenWidget.h"
#include "Data/UIDataAsset.h"
#include "Core/OpenWorldARPGSettings.h"
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
    UGameAssetManagerSubsystem* AssetManager = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
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
            UWindowWidgetBase* NewWidget = OpenUI(*WidgetClassPtr);
            if (NewWidget)
            {
                // 记录 Tag → Widget 映射，供 IsUIOpen / CloseUIByTag 查询
                // 即使 OpenUI 因重复打开返回了已存在的 Widget，这里也用最新 Tag 覆盖写入
                ActiveUIs.Add(UITag, NewWidget);
            }
            return NewWidget;
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
    UWindowWidgetBase* NewWidget = CreateWidget<UWindowWidgetBase>(GetLocalPlayer()->GetGameInstance(), WidgetClass);
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
        // 同步从 ActiveUIs 映射中移除对应的 Tag 条目（按值查找）
        for (auto It = ActiveUIs.CreateIterator(); It; ++It)
        {
            if (It.Value() == TopWidget)
            {
                It.RemoveCurrent();
                break;
            }
        }

        TopWidget->OnClosed(); // 调用顶层Widget的蓝图事件
        TopWidget->RemoveFromParent();
    }

    // 触发 UI 栈改变的全局广播
    OnUIStackChanged.Broadcast();
}

void UUIManagerSubsystem::CloseUIByTag(const FGameplayTag& UITag)
{
    if (!UITag.IsValid()) return;

    TObjectPtr<UWindowWidgetBase>* FoundPtr = ActiveUIs.Find(UITag);
    if (!FoundPtr || !*FoundPtr)
    {
        return;
    }

    UWindowWidgetBase* WidgetToClose = *FoundPtr;

    // 从 UIStack 中移除（允许非栈顶精确关闭）
    UIStack.Remove(WidgetToClose);

    // 从 Tag 映射中移除
    ActiveUIs.Remove(UITag);

    WidgetToClose->OnClosed();
    WidgetToClose->RemoveFromParent();

    // 触发 UI 栈改变的全局广播
    OnUIStackChanged.Broadcast();
}

bool UUIManagerSubsystem::IsAnyUIOpen() const
{
    return !UIStack.IsEmpty();
}

bool UUIManagerSubsystem::IsUIOpen(const FGameplayTag& UITag) const
{
    if (!UITag.IsValid()) return false;

    const TObjectPtr<UWindowWidgetBase>* Found = ActiveUIs.Find(UITag);
    if (!Found || !*Found)
    {
        return false;
    }

    // 双重校验：Widget 必须仍然在 Viewport 中才算"打开"
    return (*Found)->IsInViewport();
}

UWindowWidgetBase* UUIManagerSubsystem::GetTopWindowWidget() const
{
    return UIStack.IsEmpty() ? nullptr : UIStack.Last();
}

// --- 加载界面管理 ---

void UUIManagerSubsystem::ShowLoadingScreen()
{
    // 已存在则不重复创建
    if (LoadingScreenInstance) return;

    // 从项目设置读取加载界面 Widget 类
    const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
    TSubclassOf<ULoadingScreenWidget> WidgetClass = Settings.LoadingScreenWidgetClass;
    if (!WidgetClass) return;

    UGameInstance* GI = GetLocalPlayer()->GetGameInstance();

    // 创建并添加到 Viewport 最顶层（Z-Order=100，覆盖所有游戏 UI）
    LoadingScreenInstance = CreateWidget<ULoadingScreenWidget>(GI, WidgetClass);
    if (LoadingScreenInstance)
    {
        GI->GetGameViewportClient()->AddViewportWidgetContent(
            LoadingScreenInstance->TakeWidget(),
            100
        );
    }
}

void UUIManagerSubsystem::HideLoadingScreen()
{
    if (!LoadingScreenInstance) return;

    UGameInstance* GI = GetLocalPlayer()->GetGameInstance();
    if (GI && GI->GetGameViewportClient())
    {
        GI->GetGameViewportClient()->RemoveViewportWidgetContent(
            LoadingScreenInstance->TakeWidget()
        );
    }

    LoadingScreenInstance->MarkAsGarbage();
    LoadingScreenInstance = nullptr;
}