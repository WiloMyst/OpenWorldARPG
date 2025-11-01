// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/CharacterManager/CharacterManagerSubsystem.h"
#include "Data/CharacterData.h"
#include "Data/CharacterInfoRow.h"
#include "GameFramework/SaveGame.h"

void UCharacterManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 预加载和处理TagToRowName映射表
    PreloadAndProcessCharacterDataTable();

    UE_LOG(LogTemp, Log, TEXT("CharacterManagerSubsystem Initialized."));
    OnSubsystemInitialized();
}

void UCharacterManagerSubsystem::Deinitialize()
{
    OwnedCharacterData.Empty();
    TagToRowNameMap.Empty();

    Super::Deinitialize();
}

ACharacterData* UCharacterManagerSubsystem::GetOwnedCharacterDataByTag(const FGameplayTag& CharacterTag) const
{
    const TObjectPtr<ACharacterData>* FoundData = OwnedCharacterData.Find(CharacterTag);
    return FoundData ? *FoundData : nullptr;
}

void UCharacterManagerSubsystem::GetAllOwnedCharacterData(TArray<ACharacterData*>& CharacterDataArray) const
{
    CharacterDataArray.Empty();
    CharacterDataArray.Reserve(OwnedCharacterData.Num()); // 预分配内存提升性能

    for (const TPair<FGameplayTag, TObjectPtr<ACharacterData>>& Pair : OwnedCharacterData)
    {
        if (Pair.Value)
        {
            CharacterDataArray.Add(Pair.Value);
        }
    }
}

const bool UCharacterManagerSubsystem::GetCharacterInfoRowByTag(const FGameplayTag& CharacterTag, FCharacterInfoRow& OutRow) const
{
    UDataTable* LoadedTable = CharacterInfoDataTable.LoadSynchronous();
    // 使用预处理的映射表，通过Tag高效地找到RowName
    const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
    if (!RowNamePtr || !LoadedTable)
    {
        return false;
    }

    // 使用找到的RowName从DataTable中获取数据行
    const FCharacterInfoRow* FoundRow = LoadedTable->FindRow<FCharacterInfoRow>(*RowNamePtr, TEXT(""));
    if (FoundRow)
    {
        OutRow = *FoundRow;
        return true;
    }
    return false;
}


void UCharacterManagerSubsystem::PreloadAndProcessCharacterDataTable()
{
    UDataTable* LoadedTable = CharacterInfoDataTable.LoadSynchronous();
    if (!LoadedTable)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load CharacterInfoDataTable."));
        return;
    }

    // 构建TagToRowName映射
    const TArray<FName> RowNames = LoadedTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        const FCharacterInfoRow* Row = LoadedTable->FindRow<FCharacterInfoRow>(RowName, TEXT(""));
        if (Row && Row->CharacterTag.IsValid())
        {
            // 如果映射表中已经有这个Tag，说明数据表配置有重复，发出警告
            if (TagToRowNameMap.Contains(Row->CharacterTag))
            {
                UE_LOG(LogTemp, Warning, TEXT("Duplicate CharacterTag [%s] found in DT_CharacterInfo! Check rows with this tag."), *Row->CharacterTag.ToString());
            }
            TagToRowNameMap.Add(Row->CharacterTag, RowName);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("TagToRowName Map Loaded Successfully!"));
}