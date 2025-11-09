// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "TeamManagerSubsystem.generated.h"

class UCharacterManagerSubsystem;
class APlayerCharacter;

// --- 委托定义 ---
// 当当前激活的角色发生变化时广播 (参数：旧角色Tag, 新角色Tag)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveCharacterChanged, const FGameplayTag&, OldCharacterTag, const FGameplayTag&, NewCharacterTag);
// 当队伍成员发生变化时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamMembersChanged);


UCLASS(Blueprintable)
class OPENWORLDARPG_API UTeamManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 核心队伍管理API (蓝图可调用) ---

    /**
     * @brief [核心API] 从一个SaveGame对象初始化整个队伍管理器。
     * 这个函数会清空当前所有队伍数据，然后根据存档信息重新填充队伍。
     * 它应该在加载游戏后、进入主世界前被调用。
     * @param InSaveGameObject 包含玩家所有角色进度数据的有效SaveGame对象。
     */
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Team Management|Initialization")
    void InitializeFromSaveData(USaveGame* InSaveGameObject);

    /**
     * @brief 设置当前的出战队伍
     * @param NewTeamCharacterTags 包含角色唯一Tag的数组
	 * @param NewActiveCharacterIndex 当前激活角色在数组中的索引 (0 to N-1)
     * @return 是否设置成功
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    bool SetCurrentTeam(const TArray<FGameplayTag>& NewTeamCharacterTags, int32 NewActiveCharacterIndex);

    /**
     * @brief 切换到队伍中指定索引的角色
     * @param TeamIndex 队伍中的索引 (0 to N-1)
     */
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Team Management")
    void SwitchToCharacterByIndex(int32 TeamIndex);

    /**
     * @brief 切换到队伍中指定Tag的角色
     * @param CharacterTag 角色的唯一Tag
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SwitchToCharacterByTag(const FGameplayTag& CharacterTag);

    /**
     * @brief 切换到队伍中的下一个角色
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void CycleToNextCharacter();

    /**
     * @brief 检查指定Tag的角色是否可以切换
     * @return 如果角色可以切换则返回true，否则返回false
     */
    UFUNCTION(BlueprintPure, Category = "Team Management")
    bool IsCharacterSwitchable(int32 Index) const;

    /**
     * @brief 设置当前激活的队伍角色Index
     */
    UFUNCTION(BlueprintCallable, Category = "Team Management")
    void SetActiveCharacterIndex(int32 Index) { ActiveCharacterIndex = Index; }

    // --- 事件广播 ---

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnActiveCharacterChanged OnActiveCharacterChanged;

    UPROPERTY(BlueprintAssignable, Category = "Team Management|Events")
    FOnTeamMembersChanged OnTeamMembersChanged;

    // --- 公共Getters (蓝图可读) ---

    UFUNCTION(BlueprintPure, Category = "Team Management")
    TArray<FGameplayTag> GetCurrentTeamCharacterTags() const { return CurrentTeamCharacters; }

    UFUNCTION(BlueprintPure, Category = "Team Management")
    FGameplayTag GetActiveCharacterTag() const { return CurrentTeamCharacters.IsValidIndex(ActiveCharacterIndex) ? CurrentTeamCharacters[ActiveCharacterIndex] : FGameplayTag::EmptyTag; }

    UFUNCTION(BlueprintPure, Category = "Team Management")
    int32 GetActiveCharacterIndex() const { return ActiveCharacterIndex; }

protected:
    /**
     * @brief 用于指定 CharacterManagerSubsystem 的蓝图类。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Management|Config")
    TSubclassOf<UCharacterManagerSubsystem> CharacterManagerClass;

    /**
     * @brief 当前依赖的CharacterManagerSubsystem，用于获取角色数据。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Team Management|Data")
    TObjectPtr<UCharacterManagerSubsystem> CharacterManager;

    /**
     * @brief 当前出战队伍成员GameplayTags。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Management|Data")
    TArray<FGameplayTag> CurrentTeamCharacters;

    /**
     * @brief 当前出战队伍成员的TMap<FGameplayTag, APlayerCharacter*>。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Management|State")
    TMap<FGameplayTag, APlayerCharacter*> TeamCharacterActors;

    /**
     * @brief 当前场上角色在CurrentTeamCharacters数组中的索引 (0~N-1)。
     */
    int32 ActiveCharacterIndex = -1;
};