// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/CharacterRegistrySubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Systems/GameFlowManager/GameAssetManagerSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"

void UCharacterRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 尝试预加载，如果此时 DataTable 尚未就绪，会在首次查询时延迟构建
    BuildTagMap();

    UE_LOG(LogTemp, Log, TEXT("CharacterRegistrySubsystem Initialized. TagToRowNameMap 条数=%d"), TagToRowNameMap.Num());
}

void UCharacterRegistrySubsystem::Deinitialize()
{
    TagToRowNameMap.Empty();
    bTagMapBuilt = false;

    Super::Deinitialize();
}

bool UCharacterRegistrySubsystem::GetCharacterRegistryRowByTag(const FGameplayTag& CharacterTag, FCharacterRegistryRow& OutRow) const
{
    EnsureTagMapBuilt();

    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    UDataTable* LoadedTable = AssetManager ? AssetManager->GetCharacterInfoTable() : nullptr;
    const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
    if (!RowNamePtr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Registry] GetCharacterRegistryRowByTag: Tag [%s] 不在 TagToRowNameMap 中！Map 共 %d 条，DataTable=%s"),
            *CharacterTag.ToString(), TagToRowNameMap.Num(), LoadedTable ? TEXT("有效") : TEXT("空"));
        return false;
    }
    if (!LoadedTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[Registry] GetCharacterRegistryRowByTag: DataTable 为空！"));
        return false;
    }

    const FCharacterRegistryRow* FoundRow = LoadedTable->FindRow<FCharacterRegistryRow>(*RowNamePtr, TEXT(""));
    if (FoundRow)
    {
        OutRow = *FoundRow;
        return true;
    }
    return false;
}

FName UCharacterRegistrySubsystem::GetRowNameByTag(const FGameplayTag& CharacterTag) const
{
    EnsureTagMapBuilt();
    const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
    return RowNamePtr ? *RowNamePtr : NAME_None;
}

void UCharacterRegistrySubsystem::BuildTagMap()
{
    UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
    UDataTable* LoadedTable = AssetManager ? AssetManager->GetCharacterInfoTable() : nullptr;
    if (!LoadedTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Registry] BuildTagMap: CharacterInfoDataTable 尚未就绪，将在首次查询时延迟构建。"));
        return;
    }

    const TArray<FName> RowNames = LoadedTable->GetRowNames();
    UE_LOG(LogTemp, Log, TEXT("[Registry] BuildTagMap: DataTable 共 %d 行"), RowNames.Num());

    for (const FName& RowName : RowNames)
    {
        const FCharacterRegistryRow* Row = LoadedTable->FindRow<FCharacterRegistryRow>(RowName, TEXT(""));
        if (Row && Row->CharacterTag.IsValid())
        {
            if (TagToRowNameMap.Contains(Row->CharacterTag))
            {
                UE_LOG(LogTemp, Warning, TEXT("[Registry] 重复 CharacterTag [%s] in DT_CharacterInfo!"), *Row->CharacterTag.ToString());
            }
            TagToRowNameMap.Add(Row->CharacterTag, RowName);
        }
        else if (Row && !Row->CharacterTag.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[Registry] 行名=%s 的 CharacterTag 为空，已跳过！"), *RowName.ToString());
        }
    }

    bTagMapBuilt = true;
    UE_LOG(LogTemp, Log, TEXT("[Registry] TagToRowNameMap 构建完成，共 %d 条映射。"), TagToRowNameMap.Num());
}

void UCharacterRegistrySubsystem::EnsureTagMapBuilt() const
{
    if (bTagMapBuilt) return;
    const_cast<UCharacterRegistrySubsystem*>(this)->BuildTagMap();
}
