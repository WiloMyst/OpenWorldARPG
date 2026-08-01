// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/TeamManager/UI/TeamSetupOwnedCharacterListWidget.h"
#include "Systems/TeamManager/UI/TeamSetupSlotWidget.h"
#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Systems/CharacterManager/CharacterRegistrySubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"

UTeamSetupOwnedCharacterListWidget::UTeamSetupOwnedCharacterListWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UTeamSetupOwnedCharacterListWidget::RefreshList(const TArray<FGameplayTag>& CurrentPendingTeam)
{
    if (!CharacterListContainer || !SlotItemClass) return;

    CharacterListContainer->ClearChildren();

    UCharacterManagerSubsystem* CharSubsystem = nullptr;
    UCharacterRegistrySubsystem* Registry = nullptr;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            CharSubsystem = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
        }
        if (UGameInstance* GI = PC->GetGameInstance())
        {
            Registry = GI->GetSubsystem<UCharacterRegistrySubsystem>();
        }
    }

    if (!CharSubsystem) return;

    TArray<FCharacterSaveData> OwnedCharacters = CharSubsystem->GetAllOwnedCharacterSaveData();

    for (const FCharacterSaveData& SaveData : OwnedCharacters)
    {
        if (!SaveData.CharacterTag.IsValid()) continue;

        FCharacterRegistryRow Row;
        const bool bHasRow = Registry ? Registry->GetCharacterRegistryRowByTag(SaveData.CharacterTag, Row) : false;

        UTeamSetupSlotWidget* Item = CreateWidget<UTeamSetupSlotWidget>(this, SlotItemClass);
        if (!Item) continue;

        // 加载头像贴图（强制同步加载，避免软引用返回 nullptr）
        UTexture2D* HeadIcon = bHasRow ? Row.HeadIcon.LoadSynchronous() : nullptr;
        const FText DisplayName = bHasRow ? Row.CharacterName : FText::FromName(SaveData.CharacterTag.GetTagName());

        // SlotIndex = -1 表示这是头像列表 Item（非 DropZone）
        Item->InitializeSlot(SaveData.CharacterTag, DisplayName, HeadIcon, -1);

        // 绑定子 Item 的点击事件，转发为本组件的对外的委托广播
        Item->OnSlotClicked.AddUniqueDynamic(this, &UTeamSetupOwnedCharacterListWidget::HandleItemClicked);

        // 根据当前 PendingTeam 标记"已上阵"状态（蓝图实现高亮/灰底样式）
        const bool bIsInTeam = CurrentPendingTeam.Contains(SaveData.CharacterTag);
        Item->SetHighlighted(bIsInTeam);

        CharacterListContainer->AddChild(Item);
    }
}

void UTeamSetupOwnedCharacterListWidget::HandleItemClicked(const FGameplayTag& CharacterTag, int32 SlotIndex)
{
    // 子 Item 的 SlotIndex 始终为 -1（仅用于标识来源），忽略该参数，
    // 直接对外广播角色 Tag，由主壳子决定放入哪个槽位。
    OnOwnedCharacterClicked.Broadcast(CharacterTag);
}
