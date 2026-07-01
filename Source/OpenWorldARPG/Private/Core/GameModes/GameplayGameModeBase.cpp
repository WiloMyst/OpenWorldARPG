// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/GameModes/GameplayGameModeBase.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Data/CharacterRegistryRow.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Data/CharacterCombatDataAsset.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/GameFlowSubsystem.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

AGameplayGameModeBase::AGameplayGameModeBase()
{
    DefaultPlayerStartTag = FName("PlayerStart");
    AssetCleanupDelay = 3.0f;
}

void AGameplayGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // 仅在服务器执行角色生成
    if (!HasAuthority()) return;

    GeneratePlayerCharacters(NewPlayer);

    // 向 FlowManager 报到：新关卡初始化完毕，可关闭 Loading 屏
    if (UGameFlowSubsystem* FlowManager = GetGameInstance()->GetSubsystem<UGameFlowSubsystem>())
    {
        FlowManager->NotifyNewLevelReady();
    }

    // 只在首次 PostLogin 时设置清理定时器（避免多玩家连入时覆盖）
    if (!CleanupTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &AGameplayGameModeBase::CleanupAfterLoad,
            AssetCleanupDelay,
            false
        );
    }
}

void AGameplayGameModeBase::GeneratePlayerCharacters(APlayerController* PlayerController)
{
    AGameplayPlayerState* PlayerState = PlayerController->GetPlayerState<AGameplayPlayerState>();
    if (!PlayerState)
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: PlayerState 为空或类型不是 AGameplayPlayerState！"));
        return;
    }

    UCharacterManagerSubsystem* CharManager = nullptr;
    UTeamManagerSubsystem* TeamManager = nullptr;

    // TODO: [Network Architecture] 真正的联机模式下，这里应当根据 PlayerController 关联的 UniqueNetId
    // 向专门的 ServerDataManager（或后端数据库）请求当前队伍的 FCharacterSaveData 和 Tags，
    // 而非向客户端实体索要。
    // 当前实现：仅在单机/Host 模式下，从拥有当前 PlayerController 的 LocalPlayer 中获取 Subsystem 作为 Mock 数据源
    if (GetNetMode() == NM_Standalone || GetNetMode() == NM_ListenServer)
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            CharManager = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
            TeamManager = LocalPlayer->GetSubsystem<UTeamManagerSubsystem>();
        }
    }

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
        const FCharacterSaveData* SaveDataPtr = CharManager->GetCharacterSaveData(CharacterTag);
        if (!SaveDataPtr)
        {
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

        // --- 确定要生成的角色蓝图类 ---
        // 完全依赖 VisualData 中按体型配置的蓝图类（数据驱动）
        UClass* ClassToSpawn = nullptr;
        if (VisualData->CharacterBlueprint.IsValid())
        {
            ClassToSpawn = VisualData->CharacterBlueprint.Get();
        }
        else if (!VisualData->CharacterBlueprint.IsNull())
        {
            ClassToSpawn = VisualData->CharacterBlueprint.LoadSynchronous();
        }

        if (!ClassToSpawn)
        {
            UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 角色 Tag=%s 的 VisualData 未配置 CharacterBlueprint！跳过生成。"), *CharacterTag.ToString());
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 尝试生成队伍角色 Tag=%s (索引=%d), 使用蓝图类=%s"), *CharacterTag.ToString(), TeamIndex, *ClassToSpawn->GetName());

        // 生成角色实体
        APlayerCharacter* SpawnedChar = GetWorld()->SpawnActor<APlayerCharacter>(
            ClassToSpawn,
            SpawnTransform,
            SpawnParams
        );

        if (SpawnedChar)
        {
            // 初始化: SaveData + VisualData + CombatData + RegistryRow（三层解耦）
            SpawnedChar->InitializeCharacter(*SaveDataPtr, VisualData, CombatData, RegistryRow);

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

    int32 ActiveIndex = TeamManager->GetActiveCharacterIndex();
    if (TeamActors.IsValidIndex(ActiveIndex) && TeamActors[ActiveIndex])
    {
        APlayerCharacter* ActiveCharacter = TeamActors[ActiveIndex];
        ActiveCharacter->SetStandbyMode(false);
        PlayerController->Possess(ActiveCharacter);

        PlayerState->SetActiveCharacterIndex(ActiveIndex);

        UE_LOG(LogTemp, Log, TEXT("GeneratePlayerCharacters: 激活角色索引 %d 并 Possess。"), ActiveIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GeneratePlayerCharacters: 活跃索引 %d 无效！队伍成员数=%d"), ActiveIndex, TeamActors.Num());
    }
}

void AGameplayGameModeBase::CleanupAfterLoad()
{
    if (UGameAssetManagerSubsystem* AssetManager = GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>())
    {
        AssetManager->CleanupAfterLoad();
        UE_LOG(LogTemp, Log, TEXT("AGameplayGameModeBase: 异步资源已清理完毕。"));
    }
}

// ==========================================
// 编队保存：服务器应用新队伍（实体生成与销毁的工厂方法）
// ==========================================

void AGameplayGameModeBase::ApplyPlayerTeamChanges(AGameplayPlayerController* PlayerController, const TArray<FGameplayTag>& NewTeamTags, int32 ActiveIndex)
{
    if (!HasAuthority() || !PlayerController) return;

    UCharacterManagerSubsystem* CharManager = nullptr;

    // TODO: [Network Architecture] 真正的联机模式下，这里应当根据 PlayerController 关联的 UniqueNetId
    // 向专门的 ServerDataManager（或后端数据库）请求角色的 FCharacterSaveData，
    // 而非向客户端实体索要。
    // 当前实现：仅在单机/Host 模式下，从拥有当前 PlayerController 的 LocalPlayer 中获取 Subsystem 作为 Mock 数据源
    if (GetNetMode() == NM_Standalone || GetNetMode() == NM_ListenServer)
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            CharManager = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
        }
    }

    if (!CharManager)
    {
        UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: CharacterManagerSubsystem 不可用！"));
        return;
    }

    AGameplayPlayerState* PlayerState = PlayerController->GetPlayerState<AGameplayPlayerState>();
    if (!PlayerState)
    {
        UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: PlayerState 无效！"));
        return;
    }

    // --- 1. 记录旧角色的出场 Transform（用于新角色继承位置） ---
    FTransform SpawnTransform = FTransform::Identity;
    if (APlayerCharacter* OldActiveChar = Cast<APlayerCharacter>(PlayerController->GetPawn()))
    {
        SpawnTransform = OldActiveChar->GetActorTransform();
    }
    else
    {
        // 退路：使用 GameMode 自带的 FindPlayerStart 查找出生点
        if (AActor* FallbackStart = FindPlayerStart(PlayerController, DefaultPlayerStartTag.ToString()))
        {
            SpawnTransform = FallbackStart->GetActorTransform();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyPlayerTeamChanges: 未找到 PlayerStart (Tag=%s)，使用 Identity 位置。"), *DefaultPlayerStartTag.ToString());
        }
    }

    // --- 2. 销毁旧队伍所有角色实例 ---
    TArray<APlayerCharacter*> OldTeam;
    PlayerState->GetAllTeamCharacters(OldTeam);
    for (APlayerCharacter* OldChar : OldTeam)
    {
        if (IsValid(OldChar))
        {
            // 先 UnPossess 避免销毁被控 Pawn 产生警告
            if (PlayerController->GetPawn() == OldChar)
            {
                PlayerController->UnPossess();
            }
            OldChar->Destroy();
        }
    }

    // --- 3. 按 NewTeamTags 重新 Spawn 新队伍 ---
    TArray<APlayerCharacter*> NewTeamActors;
    NewTeamActors.SetNum(NewTeamTags.Num());

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = PlayerController;

    for (int32 TeamIndex = 0; TeamIndex < NewTeamTags.Num(); ++TeamIndex)
    {
        const FGameplayTag& CharacterTag = NewTeamTags[TeamIndex];
        if (!CharacterTag.IsValid()) continue;

        // 查询存档数据
        const FCharacterSaveData* SaveDataPtr = CharManager->GetCharacterSaveData(CharacterTag);
        if (!SaveDataPtr)
        {
            UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: Tag=%s 找不到存档数据，跳过。"), *CharacterTag.ToString());
            continue;
        }

        // 查询注册表行（VisualData / CombatData 软引用桥梁）
        FCharacterRegistryRow RegistryRow;
        if (!CharManager->GetCharacterRegistryRowByTag(CharacterTag, RegistryRow))
        {
            UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: Tag=%s 找不到 RegistryRow，跳过。"), *CharacterTag.ToString());
            continue;
        }

        // 同步加载 VisualData 和 CombatData（三层解耦：保持软引用同步加载语义）
        UCharacterVisualDataAsset* VisualData = RegistryRow.VisualData.LoadSynchronous();
        UCharacterCombatDataAsset* CombatData = RegistryRow.CombatData.LoadSynchronous();
        if (!VisualData || !CombatData)
        {
            UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: Tag=%s VisualData/CombatData 加载失败，跳过。"), *CharacterTag.ToString());
            continue;
        }

        // 解析要 Spawn 的角色蓝图类（数据驱动，按体型区分）
        UClass* ClassToSpawn = nullptr;
        if (VisualData->CharacterBlueprint.IsValid())
        {
            ClassToSpawn = VisualData->CharacterBlueprint.Get();
        }
        else if (!VisualData->CharacterBlueprint.IsNull())
        {
            ClassToSpawn = VisualData->CharacterBlueprint.LoadSynchronous();
        }

        if (!ClassToSpawn)
        {
            UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: Tag=%s 未配置 CharacterBlueprint，跳过。"), *CharacterTag.ToString());
            continue;
        }

        APlayerCharacter* SpawnedChar = GetWorld()->SpawnActor<APlayerCharacter>(ClassToSpawn, SpawnTransform, SpawnParams);
        if (!SpawnedChar)
        {
            UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: Tag=%s SpawnActor 失败！"), *CharacterTag.ToString());
            continue;
        }

        // 初始化三层解耦数据：SaveData + VisualData + CombatData + RegistryRow
        SpawnedChar->InitializeCharacter(*SaveDataPtr, VisualData, CombatData, RegistryRow);

        // 非激活角色默认进入待机模式
        SpawnedChar->SetStandbyMode(TeamIndex != ActiveIndex);

        NewTeamActors[TeamIndex] = SpawnedChar;

        UE_LOG(LogTemp, Log, TEXT("ApplyPlayerTeamChanges: 生成角色 Tag=%s (索引=%d) 成功。"), *CharacterTag.ToString(), TeamIndex);
    }

    // --- 4. 写入 PlayerState（触发全网同步） ---
    PlayerState->SetTeamCharacterActors(NewTeamActors);

    // --- 5. Possess 激活索引对应的新角色 ---
    if (NewTeamActors.IsValidIndex(ActiveIndex) && NewTeamActors[ActiveIndex])
    {
        APlayerCharacter* ActiveChar = NewTeamActors[ActiveIndex];
        ActiveChar->SetStandbyMode(false);
        PlayerController->Possess(ActiveChar);

        PlayerState->SetActiveCharacterIndex(ActiveIndex);

        UE_LOG(LogTemp, Log, TEXT("ApplyPlayerTeamChanges: 已 Possess 激活角色索引 %d。"), ActiveIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ApplyPlayerTeamChanges: 激活索引 %d 无效！新队伍大小=%d"), ActiveIndex, NewTeamActors.Num());
    }
}
