// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AbilitySystem/Tasks/AbilityTask_NavMoveTo.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

UAbilityTask_NavMoveTo* UAbilityTask_NavMoveTo::CreateNavMoveToTask(
    UGameplayAbility* OwningAbility, FVector TargetLocation, float AcceptanceRadius)
{
    UAbilityTask_NavMoveTo* Task = NewAbilityTask<UAbilityTask_NavMoveTo>(OwningAbility);
    if (Task)
    {
        Task->TargetLocation = TargetLocation;
        Task->AcceptanceRadius = AcceptanceRadius;
    }
    return Task;
}

void UAbilityTask_NavMoveTo::Activate()
{
    Super::Activate();

    if (!Ability || !Ability->GetActorInfo().AvatarActor.IsValid())
    {
        OnFailed.Broadcast();
        EndTask();
        return;
    }

    AActor* Avatar = Ability->GetActorInfo().AvatarActor.Get();
    APawn* Pawn = Cast<APawn>(Avatar);
    if (!Pawn)
    {
        OnFailed.Broadcast();
        EndTask();
        return;
    }

    AController* Controller = Pawn->GetController();
    if (!Controller)
    {
        OnFailed.Broadcast();
        EndTask();
        return;
    }

    // Early exit：已在可接受半径内，直接广播到达
    const float DistanceToTarget = FVector::Dist(Avatar->GetActorLocation(), TargetLocation);
    if (DistanceToTarget <= AcceptanceRadius)
    {
        OnTargetReached.Broadcast();
        EndTask();
        return;
    }

    // 主动确保 Controller 拥有 UPathFollowingComponent。
    //
    // 问题：UAIBlueprintHelperLibrary::SimpleMoveToLocation 内部通过 InitNavigationControl
    // 自动创建 UPathFollowingComponent，但存在两个坑：
    //   1. PlayerController 在 Possess 后 UPathFollowingComponent 不会自动更新
    //      （GetOnNewPawnNotifier 只在 Server 触发，客户端不触发）
    //   2. SimpleMoveToLocation 的 InitNavigationControl 是异步的，立即 FindComponentByClass
    //      可能返回 null（时序竞态）
    //
    // 解决：在调用 SimpleMoveToLocation 前，主动创建并 Initialize UPathFollowingComponent。
    //       若已存在则强制重新 Initialize 以更新 MovementComponent 引用（修复 Possess 后失效）。
    PathFollowingComp = Controller->FindComponentByClass<UPathFollowingComponent>();
    if (!PathFollowingComp)
    {
        PathFollowingComp = NewObject<UPathFollowingComponent>(Controller);
        PathFollowingComp->RegisterComponentWithWorld(GetWorld());
    }

    // 强制重新初始化，更新对当前 Pawn 的 MovementComponent 引用
    PathFollowingComp->Initialize();

    // 检查寻路是否被允许
    if (!PathFollowingComp->IsPathFollowingAllowed())
    {
        UE_LOG(LogTemp, Warning, TEXT("AbilityTask_NavMoveTo: PathFollowing not allowed! "
            "Check 'Allow Client Side Navigation' in Project Settings > Navigation System."));
        OnFailed.Broadcast();
        EndTask();
        return;
    }

    // 绑定 OnRequestFinished 委托，监听寻路结果
    PathFinishedHandle = PathFollowingComp->OnRequestFinished.AddUObject(
        this, &UAbilityTask_NavMoveTo::OnPathFollowingFinished);

    // 发起基于 NavMesh 的真实寻路
    // 此时 PathFollowingComponent 已就绪，SimpleMoveToLocation 会复用它
    UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, TargetLocation);
}

void UAbilityTask_NavMoveTo::OnPathFollowingFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    // 立即解绑，防止重复触发
    if (PathFollowingComp && PathFinishedHandle.IsValid())
    {
        PathFollowingComp->OnRequestFinished.Remove(PathFinishedHandle);
        PathFinishedHandle.Reset();
    }

    // 根据寻路结果广播对应委托
    if (Result.IsSuccess())
    {
        OnTargetReached.Broadcast();
    }
    else
    {
        OnFailed.Broadcast();
    }

    EndTask();
}

void UAbilityTask_NavMoveTo::OnDestroy(bool bInOwnerFinished)
{
    // 安全解绑 OnRequestFinished
    if (PathFollowingComp && PathFinishedHandle.IsValid())
    {
        PathFollowingComp->OnRequestFinished.Remove(PathFinishedHandle);
        PathFinishedHandle.Reset();
    }

    // 如果寻路仍在进行中，中止它（防止 Ability 被外部取消时角色继续自动行走）
    if (PathFollowingComp && PathFollowingComp->GetStatus() != EPathFollowingStatus::Idle)
    {
        PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished);
    }

    PathFollowingComp = nullptr;
    Super::OnDestroy(bInOwnerFinished);
}
