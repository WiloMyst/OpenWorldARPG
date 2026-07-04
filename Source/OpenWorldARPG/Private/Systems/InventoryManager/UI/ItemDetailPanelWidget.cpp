// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InventoryManager/UI/ItemDetailPanelWidget.h"
#include "Systems/InventoryManager/UI/InventoryViewModel.h"
#include "Systems/InventoryManager/UI/ItemObject.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Systems/InventoryManager/InventoryManagerSubsystem.h"

void UItemDetailPanelWidget::NativeDestruct()
{
    // 取消未完成的异步加载，防止界面销毁后回调野指针崩溃
    if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
    {
        IconLoadHandle->CancelHandle();
        IconLoadHandle.Reset();
    }

    // 生命周期安全：解绑 ViewModel 委托，防止野指针崩溃
    if (ViewModel)
    {
        ViewModel->OnSelectedItemChanged.RemoveDynamic(this, &UItemDetailPanelWidget::OnSelectedItemChanged);
    }

    Super::NativeDestruct();
}

void UItemDetailPanelWidget::SetViewModel(UInventoryViewModel* InViewModel)
{
    // 如果已有旧 ViewModel，先解绑
    if (ViewModel)
    {
        ViewModel->OnSelectedItemChanged.RemoveDynamic(this, &UItemDetailPanelWidget::OnSelectedItemChanged);
    }

    ViewModel = InViewModel;

    if (ViewModel)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryDebug] DetailPanel 成功接收到 ViewModel 并绑定委托！"));
        
        // 监听 VM 的选中物品变化广播
        ViewModel->OnSelectedItemChanged.AddDynamic(this, &UItemDetailPanelWidget::OnSelectedItemChanged);

        // 立即应用当前选中状态
        OnSelectedItemChanged(ViewModel->GetSelectedItemObject());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryDebug] DetailPanel 接收到的 ViewModel 为 NULL！"));
    }
}

void UItemDetailPanelWidget::OnSelectedItemChanged(UItemObject* SelectedItemObject)
{
    UE_LOG(LogTemp, Warning, TEXT("[InventoryDebug] DetailPanel 接收到了选中变化广播！"));

    // 如果没有选中物品，清空并隐藏面板
    if (!SelectedItemObject)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryDebug] 传入的 SelectedItemObject 为空，即将隐藏详情面板。"));
        ClearDetails();
        SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[InventoryDebug] 传入对象有效，准备更新详情面板 UI (ItemID: %d)。"), SelectedItemObject->ItemInstance.ItemID);
    // 有选中物品，显示面板并更新详情
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UpdateDetails(SelectedItemObject);
}

void UItemDetailPanelWidget::UpdateDetails(UItemObject* ItemObject)
{
    if (!ItemObject) 
    {
        ClearDetails();
        return;
    }

    // 直接从 ViewModel 的数据载体中提取安全拷贝
    const FItemInstance& ItemInstance = ItemObject->ItemInstance;
    FItemData StaticData = ItemObject->GetItemStaticData();

    // --- 赋值：物品名称 ---
    if (NameText)
    {
        NameText->SetText(StaticData.ItemName);
    }

    // --- 赋值：物品图标 (异步加载) ---
    if (AvatarImage)
    {
        if (!StaticData.ItemIcon.IsNull())
        {
            if (UTexture2D* LoadedIcon = StaticData.ItemIcon.Get())
            {
                // 已加载：直接使用
                AvatarImage->SetBrushFromTexture(LoadedIcon);
                AvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            }
            else
            {
                // 未加载：异步请求，使用强类型绑定并缓存 Handle
                // 如果有旧的加载任务，先取消
                if (IconLoadHandle.IsValid() && IconLoadHandle->IsActive())
                {
                    IconLoadHandle->CancelHandle();
                }

                TSoftObjectPtr<UTexture2D> SoftIcon = StaticData.ItemIcon;
                FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UItemDetailPanelWidget::OnDetailIconLoaded, SoftIcon);
                IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftIcon.ToSoftObjectPath(), Delegate);

                // 加载期间先隐藏图标
                AvatarImage->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        else
        {
            AvatarImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- 赋值：物品分类 ---
    if (CategoryText)
    {
        if (const UEnum* EnumPtr = StaticEnum<EItemCategory>())
        {
            FText CategoryTextName = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(StaticData.ItemCategory));
            CategoryText->SetText(CategoryTextName);
        }
    }

    // --- 赋值：拥有数量 ---
    if (AmountText)
    {
        FText AmountFormat = NSLOCTEXT("InventoryUI", "ItemAmountText", "拥有 {0}");
        FText FinalAmountText = FText::Format(AmountFormat, FText::AsNumber(ItemInstance.Count));
        AmountText->SetText(FinalAmountText);
    }

    // --- 赋值：功能描述 ---
    if (FunctionText)
    {
        FunctionText->SetText(StaticData.ItemFunctionDescription);
    }

    // --- 赋值：详细描述 ---
    if (DetailText)
    {
        DetailText->SetText(StaticData.ItemDescription);
    }

    // --- 赋值：来源说明 ---
    if (SourceText)
    {
        // TODO: 如果 StaticData 包含来源字段，在此赋值
    }
}

void UItemDetailPanelWidget::ClearDetails()
{
    if (NameText) NameText->SetText(FText::GetEmpty());
    if (AvatarImage) AvatarImage->SetVisibility(ESlateVisibility::Collapsed);
    if (CategoryText) CategoryText->SetText(FText::GetEmpty());
    if (AmountText) AmountText->SetText(FText::GetEmpty());
    if (FunctionText) FunctionText->SetText(FText::GetEmpty());
    if (DetailText) DetailText->SetText(FText::GetEmpty());
    if (SourceText) SourceText->SetText(FText::GetEmpty());
}

void UItemDetailPanelWidget::OnDetailIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon)
{
    if (!AvatarImage) return;

    if (SoftIcon.IsValid())
    {
        AvatarImage->SetBrushFromTexture(SoftIcon.Get());
        AvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}
