// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/UIManagerSubsystem.h"
#include "UI/BaseMenuWidget.h"
#include "Data/UIDataAsset.h"
#include "Kismet/GameplayStatics.h"

UBaseMenuWidget* UUIManagerSubsystem::ShowUIByTag(FGameplayTag UITag)
{
	if (!UITag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowUIByTag called with an invalid tag!"));
		return nullptr;
	}
	if (!UIMapDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("UI Map Data Asset is not set in UIManagerSubsystem!"));
		return nullptr;
	}

	if (const TSubclassOf<UBaseMenuWidget>* WidgetClassPtr = UIMapDataAsset->UIMap.Find(UITag))
	{
		if (*WidgetClassPtr)
		{
			return OpenUI(*WidgetClassPtr);
		}
	}
	return nullptr;
}

UBaseMenuWidget* UUIManagerSubsystem::OpenUI(TSubclassOf<UBaseMenuWidget> WidgetClass)
{
	if (!WidgetClass) return nullptr;

	// 检查是否已经存在一个同类的UI在堆栈中，防止重复打开
	for (UBaseMenuWidget* OpenWidget : UIStack)
	{
		if (OpenWidget && OpenWidget->GetClass() == WidgetClass)
		{
			return OpenWidget;
		}
	}

	// 创建Widget实例
	UBaseMenuWidget* NewWidget = CreateWidget<UBaseMenuWidget>(GetGameInstance(), WidgetClass);
	if (!NewWidget) return nullptr;

	// 将新Widget推入堆栈顶部
	UIStack.Add(NewWidget);
	NewWidget->AddToViewport(UIStack.Num() - 1); // Z-Order根据堆栈深度设置
	NewWidget->OnOpened(); // 调用新Widget的蓝图事件

	// 根据新的顶层UI，更新输入模式
	UpdateInputMode();

	return NewWidget;
}

void UUIManagerSubsystem::CloseTopUI()
{
	if (UIStack.IsEmpty()) return;

	// 弹出最顶层的Widget
	UBaseMenuWidget* TopWidget = UIStack.Pop();
	if (TopWidget)
	{
		TopWidget->OnClosed(); // 调用顶层Widget的蓝图事件
		TopWidget->RemoveFromParent();
	}

	// 根据新的顶层UI（或空堆栈），更新输入模式
	UpdateInputMode();
}

bool UUIManagerSubsystem::IsAnyUIOpen() const
{
	return !UIStack.IsEmpty();
}

void UUIManagerSubsystem::UpdateInputMode()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	if (UIStack.IsEmpty())
	{
		// 堆栈为空，恢复到纯游戏模式
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
	else
	{
		// 堆栈不为空，根据最顶层UI的设置来决定输入模式
		UBaseMenuWidget* TopWidget = UIStack.Last();

		switch (TopWidget->InputModeWhenOpen)
		{
		case EWidgetInputMode::UIOnly:
		{
			PC->bShowMouseCursor = true;
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(TopWidget->TakeWidget()); // 将焦点设置给顶层UI
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
			break;
		}
		case EWidgetInputMode::GameAndUI:
		{
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(TopWidget->TakeWidget());
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			break;
		}
		case EWidgetInputMode::GameOnly:
		{
			PC->bShowMouseCursor = false;
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			break;
		}
		case EWidgetInputMode::UIOnlyNoCursor:
		{
			PC->bShowMouseCursor = false;
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(TopWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
			break;
		}
		}
	}
}

