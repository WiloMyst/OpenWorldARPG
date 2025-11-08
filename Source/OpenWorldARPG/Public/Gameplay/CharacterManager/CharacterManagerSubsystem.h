// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CharacterData.h"
#include "GameplayTagContainer.h"
#include "CharacterManagerSubsystem.generated.h"

class USaveGame;
class APlayerCharacter;
class UDataTable;
struct FCharacterInfoRow;

/**
 * @class UCharacterManagerSubsystem
 * @brief 角色系统的“大脑中枢”和数据中心。
 * 负责管理玩家拥有的所有角色数据，并提供统一的数据访问接口。
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API UCharacterManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- 核心API (可在C++和蓝图中调用) ----

	/**
	 * @brief [核心API] 从一个SaveGame对象初始化整个角色管理器。
	 * 这个函数会清空当前所有角色数据，然后根据存档信息重新填充角色仓库。
	 * 它应该在加载游戏后、进入主世界前被调用。
	 *
	 * @param InSaveGameObject 包含玩家所有角色进度数据的有效SaveGame对象。
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "CharacterManager|Initialization")
	void InitializeFromSaveData(USaveGame* InSaveGameObject);

	/**
	 * @brief [蓝图可调用] 通过Tag获取已拥有的角色数据。
	 * @return 返回UCharacterData的对象指针，如果玩家不拥有该角色则返回nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Data")
	ACharacterData* GetOwnedCharacterDataByTag(const FGameplayTag& CharacterTag) const;

	/**
	 * @brief [蓝图可调用] 获取已拥有的所有角色数据。
	 * @return 返回TArray<UCharacterData*>。
	 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Data")
	void GetAllOwnedCharacterData(TArray<ACharacterData*>& CharacterDataArray) const;

	/**
	 * @brief [蓝图可调用] 获取角色在DT_CharacterInfo中的完整数据行。
	 * @param CharacterTag 要查找的角色Tag。
	 * @param OutRow [输出参数] 如果找到，这里会填充找到的数据行拷贝。
	 * @return 如果成功找到并返回了数据，则返回true；否则返回false。
	 */
	UFUNCTION(BlueprintCallable, Category = "CharacterManager|Data")
	const bool GetCharacterInfoRowByTag(const FGameplayTag& CharacterTag, FCharacterInfoRow& OutRow) const;

private:
	/**
	 * @brief 加载并预处理角色数据表，构建TagToRowNameMap。
	 * 这是一个内部函数，在Initialize时调用。
	 */
	void PreloadAndProcessCharacterDataTable();

protected:
	// ---- 核心数据容器 ----

	/**
	 * @brief [核心] 角色存档数据列表。存储玩家拥有的所有角色的动态进度数据。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterManager|Data")
	TArray<FCharacterSaveData> OwnedCharactersSaveData;

	/**
	 * @brief [核心] 角色仓库。存储玩家拥有的所有ACharacterData实例。
	 * Key是角色的唯一Tag, Value是对应的ACharacterData对象。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterManager|Data")
	TMap<FGameplayTag, TObjectPtr<ACharacterData>> OwnedCharacterData;


	// ---- 配置与预处理数据 ----

	/**
	 * @brief 指向角色信息数据表的硬指针。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterManager|Config")
	TSoftObjectPtr<UDataTable> CharacterInfoDataTable;

	/**
	 * @brief 用于通过Tag快速查找DataTable行名的映射表。
	 * 在Initialize时一次性生成，避免后续运行时的全表遍历。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CharacterManager|Config")
	TMap<FGameplayTag, FName> TagToRowNameMap;
};