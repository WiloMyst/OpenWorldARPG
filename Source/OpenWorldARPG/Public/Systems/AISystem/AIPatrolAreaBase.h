// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIPatrolAreaBase.generated.h"

class AEnemyCharacter;
struct FGrpcGameEnemySpawn;

/**
 * AI 巡逻区域基类。
 * 定义一个圆形巡逻范围，AI 在此半径内进行巡逻移动。
 *
 * Phase 3 (服务器权威刷怪): 巡逻区绑定 area_id (对齐服务器 EnemySpawn.area_id)，
 * 服务器下发 EnemySpawn 时据此定位巡逻区，在巡逻区旁生成敌人并绑定 PatrolArea。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API AAIPatrolAreaBase : public AActor
{
    GENERATED_BODY()

public:
    AAIPatrolAreaBase();
    virtual void BeginPlay() override;

    /** 巡逻半径 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol", meta = (ClampMin = "0.0"))
    float PatrolRadius = 700.0f;

    /** 巡逻区服务端 ID, 必须与服务器 EnemySpawn.area_id 对齐 (默认见 config.yaml combat.area_id) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
    int32 AreaId = 1;

    /** 本巡逻区生成的敌人蓝图类 (对齐 enemies.yaml enemy_type) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
    TSubclassOf<AEnemyCharacter> EnemyClass;

    /** 是否匹配指定服务端巡逻区 ID */
    bool MatchesAreaId(int32 InAreaId) const { return AreaId == InAreaId; }

    /** 处理服务器刷怪指令 (经 World 双向流下发): 在巡逻区旁生成 count 个敌人, 绑定本巡逻区并写入服务器权威 HP */
    void HandleServerEnemySpawn(const FGrpcGameEnemySpawn& Spawn);
};
