// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "TeamManagerSubsystem.generated.h"

class UCharacterManagerSubsystem;
class APlayerCharacter;

// --- 委托 ---

/** 当前激活角色变化时广播（参数：旧角色 TAG, 新角色 TAG） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterChanged, const FGameplayTag&, OldCharacterTag, const FGameplayTag&, NewCharacterTag);

/** 队伍成员变化时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamListUpdatedSignature);


/**
 * 队伍数据管理子系统。本地缓存 PlayerState 同步数据，通过广播驱动 UI 刷新。
 * 角色切换请求通过 PlayerController 的 Server RPC 发送到服务器。
 */
UCLASS()
class OPENWORLDARPG_API UTeamManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 初始化 ---

    UFUNCTION(BlueprintCallable, Category = "Team Management|Initialization")
    void InitializeFromDataObject(UObject* InDataObject);

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex);

    // --- 角色切换 (内部调用 Controller 的 Server RPC) ---
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

    // --- PlayerState OnRep 回调入口 ---
    UFUNCTION(BlueprintCallable, Category = "Team Management|Network")
    void OnRep_ActiveCharacterIndexFromServer(int32 NewActiveIndex);

    UFUNCTION(BlueprintCallable, Category = "Team Management|Network")
    void OnRep_TeamCharacterActorsFromServer(const TArray<APlayerCharacter*>& NewTeamActors);

    // --- 事件 ---

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnActiveCharacterChanged OnActiveCharacterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnTeamListUpdatedSignature OnTeamListUpdatedDelegate;

    // --- 查询 ---

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
