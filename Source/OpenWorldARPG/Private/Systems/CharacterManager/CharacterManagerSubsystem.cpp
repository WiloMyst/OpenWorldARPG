// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Core/Data/InitialArchiveData.h"

UCharacterManagerSubsystem::UCharacterManagerSubsystem()
{
}

void UCharacterManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("CharacterManagerSubsystem (Client Cache) Initialized."));
}

void UCharacterManagerSubsystem::Deinitialize()
{
	OwnedCharactersSaveData.Empty();

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

	// 单机/ListenServer 场景：从 UInitialArchiveData 直接初始化本地缓存。
	// [SYNC-TODO] 远程客户端应通过服务器 RPC 接收存档数据，而非从本地 UInitialArchiveData 初始化。
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

void UCharacterManagerSubsystem::InitializeFromServerData(const TArray<FCharacterSaveData>& ServerOwned)
{
	OwnedCharactersSaveData.Empty();
	OwnedCharactersSaveData.Reserve(ServerOwned.Num());

	for (const FCharacterSaveData& SaveData : ServerOwned)
	{
		if (SaveData.CharacterTag.IsValid())
		{
			OwnedCharactersSaveData.Add(SaveData.CharacterTag, SaveData);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("InitializeFromServerData: 成功填充 OwnedCharactersSaveData，数量为 %d"), OwnedCharactersSaveData.Num());
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
