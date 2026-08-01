// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "CharacterRegistrySubsystem.generated.h"

class UDataTable;
struct FCharacterRegistryRow;

/**
 * 角色注册表子系统（全局静态配置层）。
 *
 * 【职责】承载 DT_CharacterInfo 的 Tag→RowName 索引，提供全局共享的注册表查询。
 * 本子系统数据对所有玩家完全相同，无 per-玩家状态，故用 UGameInstanceSubsystem。
 *
 * 【设计原则】
 * - 只读不写：DataTable 是策划配置的静态数据，运行时不修改
 * - 全局唯一：所有玩家共用同一份索引，不区分 LocalPlayer
 * - 轻量常驻：只缓存 Tag→RowName 映射，不持有 DataAsset 硬引用
 */
UCLASS()
class OPENWORLDARPG_API UCharacterRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 注册表查询 ---

    /** 按 Tag 查询注册表行。内部会确保索引已构建。 */
    bool GetCharacterRegistryRowByTag(const FGameplayTag& CharacterTag, FCharacterRegistryRow& OutRow) const;

    /** 按 Tag 查询行名。内部会确保索引已构建。 */
    FName GetRowNameByTag(const FGameplayTag& CharacterTag) const;

private:
    /** 从 DataTable 构建 Tag→RowName 映射。 */
    void BuildTagMap();

    /** 延迟构建索引（首次查询时触发）。 */
    void EnsureTagMapBuilt() const;

    UPROPERTY()
    TMap<FGameplayTag, FName> TagToRowNameMap;

    mutable bool bTagMapBuilt = false;
};
