// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "TeamManagerSubsystem.generated.h"

class UCharacterManagerSubsystem;

// --- 委托 ---

/** 请求切换角色时广播（参数：目标索引） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestCharacterSwitch, int32, TargetIndex);

/** 当前激活角色变化时广播（参数：旧角色 Tag, 新角色 Tag） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterChanged, const FGameplayTag&, OldCharacterTag, const FGameplayTag&, NewCharacterTag);

/** 队伍成员变化时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamMembersChanged);


/**
 * @class UTeamManagerSubsystem
 * @brief 队伍数据管理子系统（纯数据层，不持有 Actor 引用）。
 *
 * 架构原则：GameInstanceSubsystem 管理跨关卡持久数据，不持有关卡内 Actor 引用。
 * Actor 引用由 World 层的 AMainGameGameMode 管理，随关卡销毁自然清理。
 */
UCLASS()
class OPENWORLDARPG_API UTeamManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 核心队伍管理 ---

    UFUNCTION(BlueprintCallable, Category = "Team Management|Initialization")
    void InitializeFromDataObject(UObject* InDataObject);

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex);

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SwitchToCharacterByIndex(int32 TeamIndex);

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SwitchToCharacterByTag(const FGameplayTag& CharacterTag);

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void CycleToNextCharacter();

    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsCharacterSwitchable(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SetActiveCharacterIndex(int32 Index) { ActiveCharacterIndex = Index; }

    // --- 事件 ---

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnRequestCharacterSwitch OnRequestCharacterSwitch;

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnActiveCharacterChanged OnActiveCharacterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnTeamMembersChanged OnTeamMembersChanged;

    // --- 查询（纯数据，无 Actor 引用）---

    UFUNCTION(BlueprintPure, Category = "Team Management")
    TArray<FGameplayTag> GetCurrentTeamCharacterTags() const { return CurrentTeamCharacters; }

    UFUNCTION(BlueprintPure, Category = "Team Management")
    FGameplayTag GetActiveCharacterTag() const { return CurrentTeamCharacters.IsValidIndex(ActiveCharacterIndex) ? CurrentTeamCharacters[ActiveCharacterIndex] : FGameplayTag::EmptyTag; }

    UFUNCTION(BlueprintPure, Category = "Team Management")
    int32 GetActiveCharacterIndex() const { return ActiveCharacterIndex; }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Team Management|Data")
    TObjectPtr<UCharacterManagerSubsystem> CharacterManager;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Management|Data")
    TArray<FGameplayTag> CurrentTeamCharacters;

    int32 ActiveCharacterIndex = -1;
};