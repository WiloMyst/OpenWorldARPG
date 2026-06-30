// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Managers/TeamManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Data/InitialArchiveData.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

void UTeamManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CharacterManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
    if (!CharacterManager)
    {
        UE_LOG(LogTemp, Error, TEXT("TeamManagerSubsystem::Initialize - Failed to get CharacterManagerSubsystem."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("TeamManagerSubsystem::Initialize - CharacterManagerSubsystem linked."));
    }
}

void UTeamManagerSubsystem::Deinitialize()
{
    CharacterManager = nullptr;
    CurrentTeamCharacters.Empty();

    Super::Deinitialize();
}

void UTeamManagerSubsystem::InitializeFromDataObject(UObject* InDataObject)
{
    UInitialArchiveData* ConfigData = Cast<UInitialArchiveData>(InDataObject);

    if (!ConfigData)
    {
        UE_LOG(LogTemp, Warning, TEXT("TeamManager InitializeFromDataObject: Cast Failed!"));
        return;
    }

    SetCurrentTeam(ConfigData->InitialTeamTags, ConfigData->InitialActiveCharacterIndex);
}

bool UTeamManagerSubsystem::SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex)
{
    if (!CharacterManager) return false;

    int32 SafeIndex = NewTeamCharacterTags.IsValidIndex(NewActiveCharacterIndex) ? NewActiveCharacterIndex : 0;

    if (NewTeamCharacterTags.IsEmpty()) return false;

    CurrentTeamCharacters = NewTeamCharacterTags;
    SetActiveCharacterIndex(SafeIndex);

    UE_LOG(LogTemp, Log, TEXT("Team set with %d members. Active Index: %d"), CurrentTeamCharacters.Num(), SafeIndex);
    OnTeamListUpdatedDelegate.Broadcast();
    return true;
}

// --- 角色切换请求 (通过 PlayerController 的 Server RPC) ---

void UTeamManagerSubsystem::SwitchToCharacterByIndex(int32 TeamIndex)
{
    if (!IsCharacterSwitchable(TeamIndex)) return;

    // 获取本地玩家（而非硬编码 Player 0）的 PlayerController
    // 联机时每个客户端只有自己的 LocalPlayer
    if (UWorld* World = GetWorld())
    {
        if (ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
        {
            if (AGameplayPlayerController* PC = Cast<AGameplayPlayerController>(LocalPlayer->GetPlayerController(World)))
            {
                PC->Server_SwitchCharacter(TeamIndex);
                UE_LOG(LogTemp, Log, TEXT("TeamManager: 通过 Server RPC 请求切换到索引 %d 的角色"), TeamIndex);
                return;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("TeamManager: SwitchToCharacterByIndex - 无法获取本地 PlayerController！"));
}

void UTeamManagerSubsystem::SwitchToCharacterByTag(const FGameplayTag& CharacterTag)
{
    int32 FoundIndex = CurrentTeamCharacters.Find(CharacterTag);
    if (FoundIndex != INDEX_NONE)
    {
        SwitchToCharacterByIndex(FoundIndex);
    }
}

void UTeamManagerSubsystem::CycleToNextCharacter()
{
    if (CurrentTeamCharacters.Num() <= 1) return;

    for (int32 i = 1; i < CurrentTeamCharacters.Num(); ++i)
    {
        const int32 NextIndex = (ActiveCharacterIndex + i) % CurrentTeamCharacters.Num();
        if (IsCharacterSwitchable(NextIndex))
        {
            SwitchToCharacterByIndex(NextIndex);
            return;
        }
    }
}

bool UTeamManagerSubsystem::IsCharacterSwitchable(int32 Index) const
{
    if (!CurrentTeamCharacters.IsValidIndex(Index) || Index == ActiveCharacterIndex) return false;

    const FGameplayTag& CharacterTag = CurrentTeamCharacters[Index];
    if (!CharacterTag.IsValid()) return false;

    return true;
}

// --- PlayerState OnRep 回调 (由 GameplayPlayerState 调用) ---

void UTeamManagerSubsystem::OnRep_ActiveCharacterIndexFromServer(int32 NewActiveIndex)
{
    int32 OldIndex = ActiveCharacterIndex;
    ActiveCharacterIndex = NewActiveIndex;

    // 广播激活角色变化事件，驱动本地 UI 刷新
    FGameplayTag OldTag = CurrentTeamCharacters.IsValidIndex(OldIndex) ? CurrentTeamCharacters[OldIndex] : FGameplayTag::EmptyTag;
    FGameplayTag NewTag = CurrentTeamCharacters.IsValidIndex(NewActiveIndex) ? CurrentTeamCharacters[NewActiveIndex] : FGameplayTag::EmptyTag;

    OnActiveCharacterChanged.Broadcast(OldTag, NewTag);

    UE_LOG(LogTemp, Log, TEXT("TeamManager: OnRep_ActiveCharacterIndex - 索引 %d -> %d"), OldIndex, NewActiveIndex);
}

void UTeamManagerSubsystem::OnRep_TeamCharacterActorsFromServer(const TArray<APlayerCharacter*>& NewTeamActors)
{
    // 从服务器同步来的角色实例中更新本地队伍 Tag 缓存
    TArray<FGameplayTag> UpdatedTags;
    UpdatedTags.Reserve(NewTeamActors.Num());

    for (APlayerCharacter* Character : NewTeamActors)
    {
        if (IsValid(Character))
        {
            UpdatedTags.Add(Character->GetCharacterTag());
        }
    }

    if (UpdatedTags != CurrentTeamCharacters)
    {
        CurrentTeamCharacters = MoveTemp(UpdatedTags);
        OnTeamListUpdatedDelegate.Broadcast();

        UE_LOG(LogTemp, Log, TEXT("TeamManager: OnRep_TeamCharacterActors - 队伍成员已更新，共 %d 个"), CurrentTeamCharacters.Num());
    }
}
