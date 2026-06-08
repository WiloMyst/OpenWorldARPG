// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/MainGameGameMode.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PlayerStates/MainGamePlayerState.h"
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

void AMainGameGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // 仅在服务器执行角色生成
    if (!HasAuthority()) return;

    GeneratePlayerCharacters(NewPlayer);

    // 只在首次 PostLogin 时设置清理定时器（避免多玩家连入时覆盖）
    if (!CleanupTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &AMainGameGameMode::CleanupAfterLoad,
            AssetCleanupDelay,
            false
        );
    }
}

void AMainGameGameMode::GeneratePlayerCharacters(APlayerController* PlayerController)
{
    // TODO [联机架构缺陷]: CharacterManagerSubsystem 和 TeamManagerSubsystem 是全局共享的
    // GameInstanceSubsystem，多玩家连入时 LoadBuffer 和 TeamTags 会互相覆盖。
    // 联机时需要改为按玩家隔离的数据源（如从 PlayerState 或存档系统按玩家 ID 加载）。
    // 当前单机/Listen Server 场景下只有 Host 一个玩家，暂时安全。

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

    AMainGamePlayerState* PlayerState = PlayerController->GetPlayerState<AMainGamePlayerState>();
    if (!PlayerState)
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: PlayerState 为空或类型不是 AMainGamePlayerState！"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: LoadBuffer 数量 = %d"), CharManager->GetLoadBuffer().Num());
    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 队伍成员 = %d, 活跃索引 = %d"), TeamManager->GetCurrentTeamCharacterTags().Num(), TeamManager->GetActiveCharacterIndex());

    // 1. 获取生成位置
    AActor* StartSpot = FindPlayerStart(PlayerController, DefaultPlayerStartTag.ToString());
    FTransform SpawnTransform = StartSpot ? StartSpot->GetActorTransform() : FTransform::Identity;

    if (!StartSpot)
    {
        UE_LOG(LogTemp, Warning, TEXT("GeneratePlayerCharacters: 未找到 PlayerStart (Tag=%s)，使用 Identity 位置。"), *DefaultPlayerStartTag.ToString());
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = PlayerController; // 设置 Owner 为 PlayerController，便于权限判定

    // 2. 队伍角色实例数组（按队伍顺序存储）
    TArray<APlayerCharacter*> TeamActors;
    TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();

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
                // 初始化: SaveData 移入 RuntimeData
                SpawnedChar->InitializeCharacter(SaveData, InfoRow.CharacterDataAsset);

                // 如果是队伍成员，按队伍顺序加入 TeamActors
                int32 TeamIndex = TeamTags.Find(SaveData.CharacterTag);
                if (TeamIndex != INDEX_NONE)
                {
                    // 确保数组足够大
                    if (TeamActors.Num() <= TeamIndex)
                    {
                        TeamActors.SetNum(TeamIndex + 1);
                    }
                    TeamActors[TeamIndex] = SpawnedChar;
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
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s 在 CharacterInfoTable 中未找到！"), *SaveData.CharacterTag.ToString());
        }
    }

    // 4. 所有角色已生成并初始化，清空加载缓冲区
    CharManager->ClearLoadBuffer();

    // 5. 将队伍角色存入 PlayerState（Replicated，全网同步）
    PlayerState->SetTeamCharacterActors(TeamActors);

    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 队伍成员 %d 个已存入 PlayerState。"), TeamActors.Num());

    // 6. 激活当前激活索引的角色
    int32 ActiveIndex = TeamManager->GetActiveCharacterIndex();
    if (TeamActors.IsValidIndex(ActiveIndex))
    {
        APlayerCharacter* ActiveCharacter = TeamActors[ActiveIndex];
        if (ActiveCharacter)
        {
            ActiveCharacter->SetStandbyMode(false);
            PlayerController->Possess(ActiveCharacter);

            // 设置 PlayerState 的激活索引（触发全网同步）
            PlayerState->SetActiveCharacterIndex(ActiveIndex);

            UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 激活角色索引 %d 并 Possess。"), ActiveIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 活跃索引 %d 无效！队伍成员数=%d"), ActiveIndex, TeamActors.Num());
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
