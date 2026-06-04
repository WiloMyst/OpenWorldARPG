// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/MainGameGameMode.h"
#include "Characters/PlayerCharacter.h"
#include "Data/CharacterInfoRow.h"
#include "Data/CharacterDataAsset.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AMainGameGameMode::AMainGameGameMode()
{
    DefaultPlayerStartTag = FName("PlayerStart");
    AssetCleanupDelay = 3.0f;
}

APlayerCharacter* AMainGameGameMode::GetTeamCharacterByTag(const FGameplayTag& CharacterTag) const
{
    if (APlayerCharacter* const* Found = TeamCharacterActors.Find(CharacterTag))
    {
        return IsValid(*Found) ? *Found : nullptr;
    }
    return nullptr;
}

void AMainGameGameMode::GetAllOwnedCharacters(TArray<APlayerCharacter*>& OutCharacters) const
{
    OutCharacters.Empty();
    OutCharacters.Reserve(OwnedCharacters.Num());
    for (const auto& Pair : OwnedCharacters)
    {
        if (IsValid(Pair.Value))
        {
            OutCharacters.Add(Pair.Value);
        }
    }
}

void AMainGameGameMode::GetAllTeamCharacters(TArray<APlayerCharacter*>& OutCharacters) const
{
    OutCharacters.Empty();
    OutCharacters.Reserve(TeamCharacterActors.Num());
    for (const auto& Pair : TeamCharacterActors)
    {
        if (IsValid(Pair.Value))
        {
            OutCharacters.Add(Pair.Value);
        }
    }
}

void AMainGameGameMode::BeginPlay()
{
    Super::BeginPlay();

    GeneratePlayerCharacters();

    GetWorld()->GetTimerManager().SetTimer(
        CleanupTimerHandle,
        this,
        &AMainGameGameMode::CleanupAfterLoad,
        AssetCleanupDelay,
        false
    );
}

void AMainGameGameMode::GeneratePlayerCharacters()
{
    UCharacterManagerSubsystem* CharManager = GetGameInstance()->GetSubsystem<UCharacterManagerSubsystem>();
    UTeamManagerSubsystem* TeamManager = GetGameInstance()->GetSubsystem<UTeamManagerSubsystem>();

    if (!CharManager)
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: CharManager 为空！"));
        return;
    }
    if (!TeamManager)
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: TeamManager 为空！"));
        return;
    }
    if (!PlayerCharacterClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: PlayerCharacterClass 未配置！请在 MainGameGameMode 蓝图中设置。"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: LoadBuffer 数量 = %d"), CharManager->GetLoadBuffer().Num());
    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 队伍成员 = %d, 活跃索引 = %d"), TeamManager->GetCurrentTeamCharacterTags().Num(), TeamManager->GetActiveCharacterIndex());

    // 1. 清空旧引用 (GameMode 随关卡销毁，这里防御性清空)
    OwnedCharacters.Empty();
    TeamCharacterActors.Empty();

    // 2. 获取生成位置
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    AActor* StartSpot = FindPlayerStart(PC, DefaultPlayerStartTag.ToString());
    FTransform SpawnTransform = StartSpot ? StartSpot->GetActorTransform() : FTransform::Identity;

    if (!StartSpot)
    {
        UE_LOG(LogTemp, Warning, TEXT("GeneratePlayerCharacters: 未找到 PlayerStart (Tag=%s)，使用 Identity 位置。"), *DefaultPlayerStartTag.ToString());
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 3. 为所有拥有的角色生成 PlayerCharacter 实体并初始化
    for (const auto& SaveData : CharManager->GetLoadBuffer())
    {
        UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 尝试生成角色 Tag=%s"), *SaveData.CharacterTag.ToString());

        FCharacterInfoRow InfoRow;
        if (CharManager->GetCharacterInfoRowByTag(SaveData.CharacterTag, InfoRow))
        {
            APlayerCharacter* SpawnedChar = GetWorld()->SpawnActor<APlayerCharacter>(
                PlayerCharacterClass,
                SpawnTransform,
                SpawnParams
            );

            if (SpawnedChar)
            {
                // 注册到 GameMode 的本地 Actor 管理
                OwnedCharacters.Add(SaveData.CharacterTag, SpawnedChar);

                // 初始化: SaveData 移入 RuntimeData
                SpawnedChar->InitializeCharacter(SaveData, InfoRow.CharacterDataAsset);

                // 如果是队伍成员，也注册到队伍 Actor 管理
                if (TeamManager->GetCurrentTeamCharacterTags().Contains(SaveData.CharacterTag))
                {
                    TeamCharacterActors.Add(SaveData.CharacterTag, SpawnedChar);
                }

                // 默认进入待机模式
                SpawnedChar->SetStandbyMode(true);

                UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 角色 Tag=%s 生成成功。"), *SaveData.CharacterTag.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s SpawnActor 失败！"), *SaveData.CharacterTag.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s 在 CharacterInfoTable 中未找到！请检查 DataTable 和 TagToRowNameMap。"), *SaveData.CharacterTag.ToString());
        }
    }

    // 4. 所有角色已生成并初始化，清空加载缓冲区
    CharManager->ClearLoadBuffer();

    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 总共生成 %d 个角色，队伍成员 %d 个。"), OwnedCharacters.Num(), TeamCharacterActors.Num());

    // 5. 激活当前激活索引的角色
    int32 ActiveIndex = TeamManager->GetActiveCharacterIndex();
    if (TeamManager->GetCurrentTeamCharacterTags().IsValidIndex(ActiveIndex))
    {
        FGameplayTag ActiveTag = TeamManager->GetCurrentTeamCharacterTags()[ActiveIndex];
        if (APlayerCharacter* ActiveCharacter = GetTeamCharacterByTag(ActiveTag))
        {
            ActiveCharacter->SetStandbyMode(false);
            if (PC) PC->Possess(ActiveCharacter);
            UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 激活角色 Tag=%s 并 Possess。"), *ActiveTag.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 活跃角色 Tag=%s 在 TeamCharacterActors 中未找到！"), *ActiveTag.ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 活跃索引 %d 无效！队伍成员数=%d"), ActiveIndex, TeamManager->GetCurrentTeamCharacterTags().Num());
    }
}

void AMainGameGameMode::CleanupAfterLoad()
{
    if (UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>())
    {
        AssetManager->CleanupAfterLoad();
        UE_LOG(LogTemp, Log, TEXT("AMainGameGameMode: 异步资源已清理完毕。"));
    }
}
