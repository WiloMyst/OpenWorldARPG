// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/CharacterManagerSubsystem.h"
#include "Characters/PlayerCharacter.h"
#include "Data/CharacterInfoRow.h"
#include "Data/StartingRosterConfig.h"
#include "Managers/GameAssetManagerSubsystem.h"

UCharacterManagerSubsystem::UCharacterManagerSubsystem()
{
}

void UCharacterManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 尝试预加载，如果此时 DataTable 尚未就绪，会在首次查询时延迟构建
	PreloadAndProcessCharacterDataTable();

	UE_LOG(LogTemp, Log, TEXT("CharacterManagerSubsystem Initialized. TagToRowNameMap 条数=%d"), TagToRowNameMap.Num());
}

void UCharacterManagerSubsystem::Deinitialize()
{
	LoadBuffer.Empty();
	TagToRowNameMap.Empty();

	Super::Deinitialize();
}

void UCharacterManagerSubsystem::InitializeFromDataObject(UObject* InDataObject)
{
	UStartingRosterConfig* ConfigData = Cast<UStartingRosterConfig>(InDataObject);

	if (!ConfigData)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeFromDataObject: Cast Failed. 输入对象为空或类型不匹配。"));
		return;
	}

	LoadBuffer = ConfigData->InitialOwnedCharacters;

	UE_LOG(LogTemp, Log, TEXT("InitializeFromDataObject: 成功填充 LoadBuffer，数量为 %d"), LoadBuffer.Num());
}

const bool UCharacterManagerSubsystem::GetCharacterInfoRowByTag(const FGameplayTag& CharacterTag, FCharacterInfoRow& OutRow) const
{
	EnsureTagMapBuilt();

	UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
	UDataTable* LoadedTable = AssetManager ? AssetManager->GetCharacterInfoTable() : nullptr;
	const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
	if (!RowNamePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCharacterInfoRowByTag: Tag [%s] 不在 TagToRowNameMap 中！Map 共 %d 条，DataTable=%s"),
			*CharacterTag.ToString(), TagToRowNameMap.Num(), LoadedTable ? TEXT("有效") : TEXT("空"));
		return false;
	}
	if (!LoadedTable)
	{
		UE_LOG(LogTemp, Error, TEXT("GetCharacterInfoRowByTag: DataTable 为空！"));
		return false;
	}

	const FCharacterInfoRow* FoundRow = LoadedTable->FindRow<FCharacterInfoRow>(*RowNamePtr, TEXT(""));
	if (FoundRow)
	{
		OutRow = *FoundRow;
		return true;
	}
	return false;
}

FName UCharacterManagerSubsystem::GetRowNameByTag(const FGameplayTag& CharacterTag) const
{
	EnsureTagMapBuilt();

	const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
	return RowNamePtr ? *RowNamePtr : NAME_None;
}

void UCharacterManagerSubsystem::CollectSaveDataFromCharacters(const TArray<APlayerCharacter*>& CharacterActors, TArray<FCharacterSaveData>& OutSaveData) const
{
	OutSaveData.Empty();
	OutSaveData.Reserve(CharacterActors.Num());

	for (APlayerCharacter* Character : CharacterActors)
	{
		if (IsValid(Character))
		{
			OutSaveData.Add(Character->GetRuntimeData());
		}
	}
}

void UCharacterManagerSubsystem::PreloadAndProcessCharacterDataTable()
{
	UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
	UDataTable* LoadedTable = AssetManager ? AssetManager->GetCharacterInfoTable() : nullptr;
	if (!LoadedTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("PreloadAndProcessCharacterDataTable: CharacterInfoDataTable 尚未就绪，将在首次查询时延迟构建。"));
		return;
	}

	const TArray<FName> RowNames = LoadedTable->GetRowNames();
	UE_LOG(LogTemp, Log, TEXT("PreloadAndProcessCharacterDataTable: DataTable 共 %d 行"), RowNames.Num());

	for (const FName& RowName : RowNames)
	{
		const FCharacterInfoRow* Row = LoadedTable->FindRow<FCharacterInfoRow>(RowName, TEXT(""));
		if (Row && Row->CharacterTag.IsValid())
		{
			if (TagToRowNameMap.Contains(Row->CharacterTag))
			{
				UE_LOG(LogTemp, Warning, TEXT("Duplicate CharacterTag [%s] found in DT_CharacterInfo! Check rows with this tag."), *Row->CharacterTag.ToString());
			}
			TagToRowNameMap.Add(Row->CharacterTag, RowName);
			UE_LOG(LogTemp, Log, TEXT("  行名=%s → Tag=%s"), *RowName.ToString(), *Row->CharacterTag.ToString());
		}
		else if (Row && !Row->CharacterTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("  行名=%s 的 CharacterTag 为空，已跳过！"), *RowName.ToString());
		}
	}

	bTagMapBuilt = true;
	UE_LOG(LogTemp, Log, TEXT("TagToRowNameMap 构建完成，共 %d 条映射。"), TagToRowNameMap.Num());
}

void UCharacterManagerSubsystem::EnsureTagMapBuilt() const
{
	if (bTagMapBuilt) return;

	// const 方法中修改 mutable 成员
	const_cast<UCharacterManagerSubsystem*>(this)->PreloadAndProcessCharacterDataTable();
}
