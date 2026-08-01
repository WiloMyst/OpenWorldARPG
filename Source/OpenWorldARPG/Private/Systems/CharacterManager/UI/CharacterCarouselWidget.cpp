// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/UI/CharacterCarouselWidget.h"
#include "Systems/CharacterManager/UI/CharacterCarouselItemWidget.h"
#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Systems/CharacterManager/CharacterRegistrySubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Components/HorizontalBox.h"
#include "Components/WrapBox.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"

UCharacterCarouselWidget::UCharacterCarouselWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCharacterCarouselWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 初始填充角色列表
    RefreshCharacterList();
}

void UCharacterCarouselWidget::RefreshCharacterList()
{
    if (!CharacterListContainer || !CarouselItemClass) return;

    // 清空旧列表
    CharacterListContainer->ClearChildren();
    SelectedItem = nullptr;

    UCharacterManagerSubsystem* SaveDataManager = nullptr;
    UCharacterRegistrySubsystem* Registry = nullptr;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            SaveDataManager = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
        }
        if (UGameInstance* GI = PC->GetGameInstance())
        {
            Registry = GI->GetSubsystem<UCharacterRegistrySubsystem>();
        }
    }

    if (!SaveDataManager) return;

    // 拉取所有已拥有角色
    TArray<FCharacterSaveData> OwnedCharacters = SaveDataManager->GetAllOwnedCharacterSaveData();

    for (const FCharacterSaveData& SaveData : OwnedCharacters)
    {
        if (!SaveData.CharacterTag.IsValid()) continue;

        // 查询 RegistryRow 获取 UI 元数据（名称、头像）
        FCharacterRegistryRow Row;
        bool bHasRow = Registry ? Registry->GetCharacterRegistryRowByTag(SaveData.CharacterTag, Row) : false;

        // 创建头像 Widget
        UCharacterCarouselItemWidget* ItemWidget = CreateWidget<UCharacterCarouselItemWidget>(this, CarouselItemClass);
        if (ItemWidget)
        {
            // 使用 LoadSynchronous() 强制将软引用资源加载进内存
            UTexture2D* HeadIcon = bHasRow ? Row.HeadIcon.LoadSynchronous() : nullptr;
            ItemWidget->InitializeItem(SaveData.CharacterTag, HeadIcon);
            ItemWidget->OnItemSelected.AddDynamic(this, &UCharacterCarouselWidget::HandleItemSelected);

            CharacterListContainer->AddChild(ItemWidget);
        }
    }
}

void UCharacterCarouselWidget::HandleItemSelected(const FGameplayTag& CharacterTag)
{
    // 取消之前选中的项的高亮
    if (SelectedItem)
    {
        SelectedItem->SetSelected(false);
    }

    // 通过 CharacterTag 查找对应的 ItemWidget 并标记选中
    if (CharacterListContainer)
    {
        for (UWidget* Child : CharacterListContainer->GetAllChildren())
        {
            if (UCharacterCarouselItemWidget* Item = Cast<UCharacterCarouselItemWidget>(Child))
            {
                if (Item->GetCharacterTag() == CharacterTag)
                {
                    SelectedItem = Item;
                    Item->SetSelected(true);
                    break;
                }
            }
        }
    }

    // 向上传递选中事件
    OnCharacterSelected.Broadcast(CharacterTag);
}

FGameplayTag UCharacterCarouselWidget::GetFirstCharacterTag() const
{
    if (CharacterListContainer && CharacterListContainer->GetChildrenCount() > 0)
    {
        if (UCharacterCarouselItemWidget* Item = Cast<UCharacterCarouselItemWidget>(CharacterListContainer->GetChildAt(0)))
        {
            return Item->GetCharacterTag();
        }
    }
    return FGameplayTag::EmptyTag;
}
