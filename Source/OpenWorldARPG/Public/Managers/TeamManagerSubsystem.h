// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "TeamManagerSubsystem.generated.h"

class UCharacterManagerSubsystem;
class APlayerCharacter;

// --- 委托 ---

/** 当前激活角色变化时广播（参数：旧角色 Tag, 新角色 Tag） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterChanged, const FGameplayTag&, OldCharacterTag, const FGameplayTag&, NewCharacterTag);

/** 队伍成员变化时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamMembersChanged);


/**
 * @class UTeamManagerSubsystem
 * @brief 队伍数据管理子系统（本地缓存与 UI 驱动）。
 *
 * 联机架构下的角色定位：
 * - 服务器上的数据是绝对真理（存在 PlayerState 中）
 * - 本地 Subsystem 是"本地缓存"，负责监听 PlayerState 的 OnRep 回调
 * - 一旦 PlayerState 的数据同步到客户端，Subsystem 就发出广播驱动本地 UI 刷新
 * - UI 不能每帧去服务器要数据，所以通过 Subsystem 的广播来驱动
 *
 * 注意：角色切换请求不再通过 OnRequestCharacterSwitch 广播，
 * 而是通过 PlayerController 的 Server RPC 直接发送到服务器。
 */
UCLASS()
class OPENWORLDARPG_API UTeamManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ==========================================
    // 初始化 (从配置数据设置队伍)
    // ==========================================

    UFUNCTION(BlueprintCallable, Category = "Team Management|Initialization")
    void InitializeFromDataObject(UObject* InDataObject);

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex);

    // ==========================================
    // 角色切换请求 (本地便捷方法，内部调用 Controller 的 Server RPC)
    // ==========================================

    /** 通过索引请求切换角色（便捷方法，获取本地 PlayerController 调用 Server RPC） */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SwitchToCharacterByIndex(int32 TeamIndex);

    /** 通过 Tag 请求切换角色 */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SwitchToCharacterByTag(const FGameplayTag& CharacterTag);

    /** 循环切换到下一个可用角色 */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void CycleToNextCharacter();

    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsCharacterSwitchable(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SetActiveCharacterIndex(int32 Index) { ActiveCharacterIndex = Index; }

    // ==========================================
    // PlayerState OnRep 回调入口 (由 MainGamePlayerState 调用)
    // ==========================================

    /** PlayerState 的 ActiveCharacterIndex OnRep 回调时调用此方法，更新本地缓存并广播 UI */
    UFUNCTION(BlueprintCallable, Category = "Team Management|Network")
    void OnRep_ActiveCharacterIndexFromServer(int32 NewActiveIndex);

    /** PlayerState 的 TeamCharacterActors OnRep 回调时调用此方法，更新本地缓存并广播 UI */
    UFUNCTION(BlueprintCallable, Category = "Team Management|Network")
    void OnRep_TeamCharacterActorsFromServer(const TArray<APlayerCharacter*>& NewTeamActors);

    // ==========================================
    // 事件 (驱动本地 UI 刷新)
    // ==========================================

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnActiveCharacterChanged OnActiveCharacterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnTeamMembersChanged OnTeamMembersChanged;

    // ==========================================
    // 查询（本地缓存，无 Actor 引用）
    // ==========================================

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
