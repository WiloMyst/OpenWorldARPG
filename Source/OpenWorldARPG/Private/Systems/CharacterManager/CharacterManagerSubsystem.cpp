// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Core/Data/InitialArchiveData.h"
#include "Systems/GameFlowManager/GameAssetManagerSubsystem.h"
#include "Engine/LocalPlayer.h"

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
	OwnedCharactersSaveData.Empty();
	TagToRowNameMap.Empty();

	Super::Deinitialize();
}

void UCharacterManagerSubsystem::InitializeFromDataObject(UObject* InDataObject)
{
	UInitialArchiveData* ConfigData = Cast<UInitialArchiveData>(InDataObject);

	if (!ConfigData)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeFromDataObject: Cast Failed. 输入对象为空或类型不匹配。"));
		return;
	}

	// 将初始拥有角色数据存入持久化 Map（以 CharacterTag 为键）
	OwnedCharactersSaveData.Empty();
	OwnedCharactersSaveData.Reserve(ConfigData->InitialOwnedCharacters.Num());

	for (const FCharacterSaveData& SaveData : ConfigData->InitialOwnedCharacters)
	{
		if (SaveData.CharacterTag.IsValid())
		{
			OwnedCharactersSaveData.Add(SaveData.CharacterTag, SaveData);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("InitializeFromDataObject: 跳过无效 CharacterTag 的存档数据。"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("InitializeFromDataObject: 成功填充 OwnedCharactersSaveData，数量为 %d"), OwnedCharactersSaveData.Num());
}

bool UCharacterManagerSubsystem::GetCharacterRegistryRowByTag(const FGameplayTag& CharacterTag, FCharacterRegistryRow& OutRow) const
{
	EnsureTagMapBuilt();

	UGameAssetManagerSubsystem* AssetManager = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
	UDataTable* LoadedTable = AssetManager ? AssetManager->GetCharacterInfoTable() : nullptr;
	const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
	if (!RowNamePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCharacterRegistryRowByTag: Tag [%s] 不在 TagToRowNameMap 中！Map 共 %d 条，DataTable=%s"),
			*CharacterTag.ToString(), TagToRowNameMap.Num(), LoadedTable ? TEXT("有效") : TEXT("空"));
		return false;
	}
	if (!LoadedTable)
	{
		UE_LOG(LogTemp, Error, TEXT("GetCharacterRegistryRowByTag: DataTable 为空！"));
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

FName UCharacterManagerSubsystem::GetRowNameByTag(const FGameplayTag& CharacterTag) const
{
	EnsureTagMapBuilt();

	const FName* RowNamePtr = TagToRowNameMap.Find(CharacterTag);
	return RowNamePtr ? *RowNamePtr : NAME_None;
}

TArray<FCharacterSaveData> UCharacterManagerSubsystem::GetAllOwnedCharacterSaveData() const
{
	TArray<FCharacterSaveData> Result;
	OwnedCharactersSaveData.GenerateValueArray(Result);
	return Result;
}

const FCharacterSaveData* UCharacterManagerSubsystem::GetCharacterSaveData(const FGameplayTag& CharacterTag) const
{
	return OwnedCharactersSaveData.Find(CharacterTag);
}

void UCharacterManagerSubsystem::SetCharacterSaveData(const FGameplayTag& CharacterTag, const FCharacterSaveData& NewData)
{
	FCharacterSaveData* ExistingData = OwnedCharactersSaveData.Find(CharacterTag);
	if (ExistingData)
	{
		*ExistingData = NewData;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCharacterSaveData: Tag [%s] 不存在于 OwnedCharactersSaveData 中，跳过更新。"), *CharacterTag.ToString());
	}
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
	UGameAssetManagerSubsystem* AssetManager = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>();
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
		const FCharacterRegistryRow* Row = LoadedTable->FindRow<FCharacterRegistryRow>(RowName, TEXT(""));
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
