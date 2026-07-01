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
    if (!UITag.IsValid()) return nullptr;

    UGameAssetManagerSubsystem* AssetManager = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    UUIDataAsset* LoadedUIData = AssetManager ? AssetManager->GetUIMapDataAsset() : nullptr;
    if (!LoadedUIData) return nullptr;

    if (const TSubclassOf<UWindowWidgetBase>* WidgetClassPtr = LoadedUIData->UIMap.Find(UITag))
    {
        if (*WidgetClassPtr)
        {
            UWindowWidgetBase* NewWidget = OpenUI(*WidgetClassPtr);
            if (NewWidget)
            {
                ActiveUIs.Add(UITag, NewWidget);
            }
            return NewWidget;
        }
    }

    return nullptr;
}

UWindowWidgetBase* UUIManagerSubsystem::OpenUI(TSubclassOf<UWindowWidgetBase> WidgetClass)
{
    if (!WidgetClass) return nullptr;

    for (UWindowWidgetBase* OpenWidget : UIStack)
    {
        if (OpenWidget && OpenWidget->GetClass() == WidgetClass)
        {
            return OpenWidget;
        }
    }

    UWindowWidgetBase* NewWidget = CreateWidget<UWindowWidgetBase>(GetLocalPlayer()->GetGameInstance(), WidgetClass);
    if (!NewWidget) return nullptr;

    UIStack.Add(NewWidget);
    NewWidget->AddToViewport(UIStack.Num() - 1);
    NewWidget->OnOpened();

    OnUIStackChanged.Broadcast();

    return NewWidget;
}

void UUIManagerSubsystem::CloseTopUI()
{
    if (UIStack.IsEmpty()) return;

    UWindowWidgetBase* TopWidget = UIStack.Pop();
    if (TopWidget)
    {
        for (auto It = ActiveUIs.CreateIterator(); It; ++It)
        {
            if (It.Value() == TopWidget)
            {
                It.RemoveCurrent();
                break;
            }
        }

        TopWidget->OnClosed();
        TopWidget->RemoveFromParent();
    }

    OnUIStackChanged.Broadcast();
}

void UUIManagerSubsystem::CloseUIByTag(const FGameplayTag& UITag)
{
    if (!UITag.IsValid()) return;

    TObjectPtr<UWindowWidgetBase>* FoundPtr = ActiveUIs.Find(UITag);
    if (!FoundPtr || !*FoundPtr) return;

    UWindowWidgetBase* WidgetToClose = *FoundPtr;

    UIStack.Remove(WidgetToClose);
    ActiveUIs.Remove(UITag);

    WidgetToClose->OnClosed();
    WidgetToClose->RemoveFromParent();

    OnUIStackChanged.Broadcast();
}

bool UUIManagerSubsystem::IsUIOpen(const FGameplayTag& UITag) const
{
    if (!UITag.IsValid()) return false;

    const TObjectPtr<UWindowWidgetBase>* Found = ActiveUIs.Find(UITag);
    if (!Found || !*Found) return false;

    return (*Found)->IsInViewport();
}

void UUIManagerSubsystem::ShowLoadingScreen()
{
    if (LoadingScreenInstance) return;

    const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
    TSubclassOf<ULoadingScreenWidget> WidgetClass = Settings.LoadingScreenWidgetClass;
    if (!WidgetClass) return;

    UGameInstance* GI = GetLocalPlayer()->GetGameInstance();

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
