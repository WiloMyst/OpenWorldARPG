// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AISystem/AIPatrolAreaBase.h"
#include "Characters/AICharacter/EnemyCharacter.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "SGame/GameMessage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

AAIPatrolAreaBase::AAIPatrolAreaBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AAIPatrolAreaBase::BeginPlay()
{
    Super::BeginPlay();

    // 客户端驱动刷怪: BeginPlay 时向服务器上报本巡逻区 (ID + 世界位置).
    // 服务器判断是否允许刷怪, 允许后以 EnemySpawn 下发, 由 HandleServerEnemySpawn 生成敌人.
    // 未登录或实时通道不可用时跳过本次上报 (进入关卡前需先登录).
    if (UGameServerSubsystem* GS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameServerSubsystem>() : nullptr)
    {
        // 携带巡逻半径上报: 服务器以 (位置, PatrolRadius) 为圆心/半径约束刷点,
        // 确保敌人出生即落在巡逻圈内.
        const bool Sent = GS->SendSpawnRequest(AreaId, PatrolRadius, GetActorLocation());
        UE_LOG(LogTemp, Log, TEXT("[AIPatrolArea] 上报刷怪请求 [area_id=%d, patrol_r=%.0f, sent=%d]"),
               AreaId, PatrolRadius, Sent ? 1 : 0);
    }
}

void AAIPatrolAreaBase::HandleServerEnemySpawn(const FGrpcGameEnemySpawn& Spawn)
{
    if (!EnemyClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIPatrolArea] 未配置 EnemyClass, 跳过刷怪 [area_id=%d]"), AreaId);
        return;
    }

    const FVector RawLoc(Spawn.SpawnX, Spawn.SpawnY, Spawn.SpawnZ);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Instigator = GetInstigator();

    AEnemyCharacter* Enemy = GetWorld()->SpawnActor<AEnemyCharacter>(EnemyClass, RawLoc, FRotator::ZeroRotator, Params);
    if (!Enemy)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIPatrolArea] 敌人生成失败 [enemy_id=%llu]"), (uint64)Spawn.EnemyId.Value);
        return;
    }

    // 刷点 z 由服务器按巡逻区圆心下发, 与地形可能偏差 (巡逻区摆放高度 ≠ 地表高度);
    // 生成后向地表追踪对齐, 避免敌人一半身体陷入地面或悬空
    if (UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent())
    {
        const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnGroundTrace), false);
        QueryParams.AddIgnoredActor(Enemy);
        QueryParams.AddIgnoredActor(this);
        FHitResult Hit;
        const FVector TraceStart(RawLoc.X, RawLoc.Y, RawLoc.Z + 5000.0f);
        const FVector TraceEnd(RawLoc.X, RawLoc.Y, RawLoc.Z - 5000.0f);
        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
        {
            Enemy->SetActorLocation(FVector(RawLoc.X, RawLoc.Y, Hit.ImpactPoint.Z + HalfHeight), /*bSweep=*/false);
        }
    }

    // 服务器权威刷怪: 绑定巡逻区 + 写入服务器权威 HP, 敌人归本巡逻区管理
    Enemy->ConfigureFromServer(Spawn.EnemyId.Value, Spawn.MaxHp, this);

    // 登记 enemy_id -> 敌人实体, 供后续 DamageDeal 定位血条/死亡表现
    if (UGameServerSubsystem* GS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameServerSubsystem>() : nullptr)
    {
        GS->RegisterEnemy(Enemy->GetServerEnemyId(), Enemy);
    }

    UE_LOG(LogTemp, Log, TEXT("[AIPatrolArea] 服务器刷怪完成 [enemy_id=%llu, area=%d, type=%d, hp=%d, pos=(%.1f,%.1f,%.1f)]"),
           (uint64)Spawn.EnemyId.Value, Spawn.AreaId, Spawn.EnemyType, Spawn.MaxHp,
           RawLoc.X, RawLoc.Y, RawLoc.Z);
}
