// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/MainGameGameMode.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PlayerStates/MainGamePlayerState.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Data/CharacterCombatDataAsset.h"
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
    // GameInstanceSubsystem，多玩家连入时数据会互相覆盖。
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

    // 获取当前队伍成员 Tag 列表（仅队伍中的角色才生成实体）
    const TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();

    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 玩家拥有角色数 = %d, 队伍成员数 = %d, 活跃索引 = %d"),
        CharManager->GetOwnedCharacterCount(), TeamTags.Num(), TeamManager->GetActiveCharacterIndex());

    // 1. 获取生成位置
    AActor* StartSpot = FindPlayerStart(PlayerController, DefaultPlayerStartTag.ToString());
    FTransform SpawnTransform = StartSpot ? StartSpot->GetActorTransform() : FTransform::Identity;

    if (!StartSpot)
    {
        UE_LOG(LogTemp, Warning, TEXT("GeneratePlayerCharacters: 未找到 PlayerStart (Tag=%s)，使用 Identity 位置。"), *DefaultPlayerStartTag.ToString());
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = PlayerController;

    // 2. 队伍角色实例数组（按队伍顺序存储，索引与 TeamTags 一一对应）
    TArray<APlayerCharacter*> TeamActors;
    TeamActors.SetNum(TeamTags.Num());

    // 3. 仅遍历队伍成员 Tag，按需生成角色实体
    for (int32 TeamIndex = 0; TeamIndex < TeamTags.Num(); ++TeamIndex)
    {
        const FGameplayTag& CharacterTag = TeamTags[TeamIndex];

        // 从 CharacterManagerSubsystem 查询该角色的存档数据
        FCharacterSaveData SaveData;
        if (!CharManager->GetCharacterSaveData(CharacterTag, SaveData))
        {
            // 队伍 Tag 在玩家拥有的角色中找不到，输出 Error 并跳过
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 队伍角色 Tag=%s 在玩家拥有的角色存档中未找到！跳过生成。"), *CharacterTag.ToString());
            continue;
        }

        // 查询角色注册表行（UI 元数据 + VisualData/CombatData 软引用桥梁）
        FCharacterRegistryRow RegistryRow;
        if (!CharManager->GetCharacterRegistryRowByTag(CharacterTag, RegistryRow))
        {
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s 在 CharacterRegistryTable 中未找到！跳过生成。"), *CharacterTag.ToString());
            continue;
        }

        // 解析 TSoftObjectPtr：加载 VisualData 和 CombatData
        UCharacterVisualDataAsset* VisualData = RegistryRow.VisualData.LoadSynchronous();
        UCharacterCombatDataAsset* CombatData = RegistryRow.CombatData.LoadSynchronous();
        if (!VisualData || !CombatData)
        {
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s 的 VisualData 或 CombatData 加载失败！跳过生成。"), *CharacterTag.ToString());
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 尝试生成队伍角色 Tag=%s (索引=%d)"), *CharacterTag.ToString(), TeamIndex);

        // 生成角色实体
        APlayerCharacter* SpawnedChar = GetWorld()->SpawnActor<APlayerCharacter>(
            PlayerCharacterClass,
            SpawnTransform,
            SpawnParams
        );

        if (SpawnedChar)
        {
            // 初始化: SaveData + VisualData + CombatData + RegistryRow（三层解耦）
            SpawnedChar->InitializeCharacter(SaveData, VisualData, CombatData, RegistryRow);

            // 按队伍索引存入 TeamActors
            TeamActors[TeamIndex] = SpawnedChar;

            // 默认进入待机模式
            SpawnedChar->SetStandbyMode(true);

            UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 队伍角色 Tag=%s 生成成功。"), *CharacterTag.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s SpawnActor 失败！"), *CharacterTag.ToString());
        }
    }

    // 4. 将队伍角色存入 PlayerState（Replicated，全网同步）
    PlayerState->SetTeamCharacterActors(TeamActors);

    UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 队伍成员 %d 个已存入 PlayerState。"), TeamActors.Num());

    // 5. 激活当前激活索引的角色
    int32 ActiveIndex = TeamManager->GetActiveCharacterIndex();
    if (TeamActors.IsValidIndex(ActiveIndex) && TeamActors[ActiveIndex])
    {
        APlayerCharacter* ActiveCharacter = TeamActors[ActiveIndex];
        ActiveCharacter->SetStandbyMode(false);
        PlayerController->Possess(ActiveCharacter);

        // 设置 PlayerState 的激活索引（触发全网同步）
        PlayerState->SetActiveCharacterIndex(ActiveIndex);

        UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 激活角色索引 %d 并 Possess。"), ActiveIndex);
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
