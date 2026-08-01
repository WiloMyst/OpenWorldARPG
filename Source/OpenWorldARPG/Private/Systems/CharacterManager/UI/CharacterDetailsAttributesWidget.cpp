// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/UI/CharacterDetailsAttributesWidget.h"
#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Systems/CharacterManager/CharacterRegistrySubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/GameInstance.h"

UCharacterDetailsAttributesWidget::UCharacterDetailsAttributesWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCharacterDetailsAttributesWidget::RefreshData(const FGameplayTag& CharacterTag)
{
    CurrentCharacterTag = CharacterTag;

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

    // 拉取存档数据
    const FCharacterSaveData* SaveDataPtr = SaveDataManager->GetCharacterSaveData(CharacterTag);
    if (!SaveDataPtr)
    {
        return;
    }
    CurrentSaveData = *SaveDataPtr;

    // 更新基础文本控件
    if (LevelText)
    {
        LevelText->SetText(FText::Format(FText::FromString(TEXT("{0}")), FText::AsNumber(CurrentSaveData.CharacterLevel)));
    }

    // 查询 RegistryRow 获取角色名称
    FCharacterRegistryRow Row;
    if (Registry && Registry->GetCharacterRegistryRowByTag(CharacterTag, Row))
    {
        if (CharacterNameText)
        {
            CharacterNameText->SetText(Row.CharacterName);
        }
    }

    // 经验进度条（占位：需要配合等级表计算实际进度百分比）
    if (ExperienceBar)
    {
        ExperienceBar->SetPercent(0.5f);
    }

    // 绑定属性快照数据到文本（数据源：FCharacterSaveData 中的 Attribute Snapshots）
    if (MaxHPText)
    {
        MaxHPText->SetText(FText::AsNumber(FMath::RoundToInt(CurrentSaveData.MaxHealth)));
    }

    if (AttackText)
    {
        AttackText->SetText(FText::AsNumber(FMath::RoundToInt(CurrentSaveData.Attack)));
    }

    if (DefenseText)
    {
        DefenseText->SetText(FText::AsNumber(FMath::RoundToInt(CurrentSaveData.Defense)));
    }

    OnDataRefreshed();
}
