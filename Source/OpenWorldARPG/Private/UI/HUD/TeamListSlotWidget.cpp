// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/TeamListSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "AbilitySystemComponent.h"

void UTeamListSlotWidget::InitSlot(
    const FText& InCharacterName,
    TSoftObjectPtr<UTexture2D> InHeadIcon,
    FGameplayTag InCharacterTag,
    int32 InCharacterIndex,
    UAbilitySystemComponent* InASC)
{
    CharacterTag = InCharacterTag;
    CharacterIndex = InCharacterIndex;

    // 1. 设置序号文本 (蓝图逻辑：Index + 1)
    if (Text_CharacterIndex)
    {
        Text_CharacterIndex->SetText(FText::AsNumber(CharacterIndex + 1));
    }

    // 2. 设置角色名称（数据由父容器传入，不再自行查询 Subsystem）
    if (Text_CharacterName)
    {
        Text_CharacterName->SetText(InCharacterName);
    }

    // 3. 触发异步加载头像
    LoadCharacterIcon(InHeadIcon);

    // 4. 绑定死亡状态监听 (ASC 由父容器传入)
    BindCharacterState(InASC);
}

void UTeamListSlotWidget::ResetSlot()
{
    // 清理异步加载
    if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }

    // 安全解绑死亡 Tag 委托
    if (CachedASC.IsValid() && DeadStateTag.IsValid() && DeadTagDelegateHandle.IsValid())
    {
        CachedASC->UnregisterGameplayTagEvent(DeadTagDelegateHandle, DeadStateTag, EGameplayTagEventType::NewOrRemoved);
        DeadTagDelegateHandle.Reset();
    }
    CachedASC = nullptr;

    // 重置 UI 显示
    if (Text_CharacterName) Text_CharacterName->SetText(FText::GetEmpty());
    if (Text_CharacterIndex) Text_CharacterIndex->SetText(FText::GetEmpty());
    if (Image_CharacterIcon) Image_CharacterIcon->SetBrushFromTexture(nullptr);
    if (DeathColor) DeathColor->SetRenderOpacity(0.0f);

    CharacterTag = FGameplayTag();
    CharacterIndex = 0;
}

void UTeamListSlotWidget::LoadCharacterIcon(TSoftObjectPtr<UTexture2D> SoftIcon)
{
    if (SoftIcon.IsNull()) return;

    // 如果资源已经在内存中，直接使用
    if (SoftIcon.IsValid())
    {
        OnIconLoaded(SoftIcon);
        return;
    }

    // 执行异步加载，避免阻塞主线程
    FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();

    // 使用 TWeakObjectPtr 防止加载完成时 UI 已经被销毁导致野指针崩溃
    TWeakObjectPtr<UTeamListSlotWidget> WeakThis = this;
    IconLoadHandle = StreamableManager.RequestAsyncLoad(SoftIcon.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, SoftIcon]()
        {
            if (WeakThis.IsValid())
            {
                WeakThis->OnIconLoaded(SoftIcon);
            }
        })
    );
}

void UTeamListSlotWidget::OnIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon)
{
    if (Image_CharacterIcon && SoftIcon.IsValid())
    {
        Image_CharacterIcon->SetBrushFromTexture(SoftIcon.Get());
    }
}

void UTeamListSlotWidget::BindCharacterState(UAbilitySystemComponent* InASC)
{
    if (!InASC || !DeadStateTag.IsValid()) return;

    // 重置死亡蒙版
    if (DeathColor) DeathColor->SetRenderOpacity(0.0f);

    CachedASC = InASC;

    // 监听死亡 Tag 的状态变化，保存委托句柄用于精准解绑
    DeadTagDelegateHandle = InASC->RegisterGameplayTagEvent(DeadStateTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UTeamListSlotWidget::OnDeadTagChanged);

    // 主动校对一次初始状态 (防角色一出生就是死的)
    if (InASC->HasMatchingGameplayTag(DeadStateTag))
    {
        OnDeadTagChanged(DeadStateTag, 1);
    }
}

void UTeamListSlotWidget::OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (Tag == DeadStateTag && DeathColor)
    {
        DeathColor->SetRenderOpacity(NewCount > 0 ? 1.0f : 0.0f);
    }
}

void UTeamListSlotWidget::NativeDestruct()
{
    // 1. 如果界面被销毁时图片还没加载完，取消加载任务节省内存
    if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
    {
        IconLoadHandle->CancelHandle();
    }

    // 2. 安全解绑 GAS 委托（使用 FDelegateHandle 精准移除，避免误删其他监听者）
    if (CachedASC.IsValid() && DeadStateTag.IsValid() && DeadTagDelegateHandle.IsValid())
    {
        CachedASC->UnregisterGameplayTagEvent(DeadTagDelegateHandle, DeadStateTag, EGameplayTagEventType::NewOrRemoved);
        DeadTagDelegateHandle.Reset();
    }

    Super::NativeDestruct();
}
