// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "TeamManagerSubsystem.generated.h"

class UCharacterManagerSubsystem;
class APlayerCharacter;
class AGameplayPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterChanged, const FGameplayTag&, OldCharacterTag, const FGameplayTag&, NewCharacterTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamListUpdatedSignature);

/**
 * 队伍管理子系统。缓存 PlayerState 同步数据，通过广播驱动 UI 刷新。
 */
UCLASS()
class OPENWORLDARPG_API UTeamManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 初始化 ---

    void InitializeFromDataObject(UObject* InDataObject);

    // 服务器权威初始化: 以登录响应下发的配队列表 + 上场 index 覆盖本地配置
    void InitializeFromServerData(const TArray<FGameplayTag>& TeamTags, int32 InActiveCharacterIndex);

    // --- 队伍设置 ---

    bool SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex);

    // --- 角色切换 ---

    void SwitchToCharacterByIndex(int32 TeamIndex);
    void SwitchToCharacterByTag(const FGameplayTag& CharacterTag);
    void CycleToNextCharacter();
    bool IsCharacterSwitchable(int32 Index) const;
    void SetActiveCharacterIndex(int32 Index) { ActiveCharacterIndex = Index; }

    // --- 网络回调 ---

    void OnRep_ActiveCharacterIndexFromServer(int32 NewActiveIndex);
    void OnRep_TeamCharacterActorsFromServer(const TArray<APlayerCharacter*>& NewTeamActors);

    // --- 事件绑定 ---

    void BindToPlayerStateEvents(AGameplayPlayerState* PlayerState);

    UFUNCTION()
    void HandleActiveCharacterIndexChanged(int32 OldIndex, int32 NewIndex);

    UFUNCTION()
    void HandleTeamCharacterActorsChanged();

    // --- 查询 ---

    TArray<FGameplayTag> GetCurrentTeamCharacterTags() const { return CurrentTeamCharacters; }
    FGameplayTag GetActiveCharacterTag() const { return CurrentTeamCharacters.IsValidIndex(ActiveCharacterIndex) ? CurrentTeamCharacters[ActiveCharacterIndex] : FGameplayTag::EmptyTag; }
    int32 GetActiveCharacterIndex() const { return ActiveCharacterIndex; }

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnActiveCharacterChanged OnActiveCharacterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnTeamListUpdatedSignature OnTeamListUpdatedDelegate;

protected:
    // --- 缓存指针 ---

    UPROPERTY(BlueprintReadOnly, Category = "Team Management|Data")
    TObjectPtr<UCharacterManagerSubsystem> CharacterManager;

    TWeakObjectPtr<AGameplayPlayerState> BoundPlayerState;

    // --- 数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "Team Management|Data")
    TArray<FGameplayTag> CurrentTeamCharacters;

    int32 ActiveCharacterIndex = -1;
};
