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

    // 调用 SimpleMoveToLocation 发起基于 NavMesh 的真实寻路
    // SimpleMoveToLocation 内部会通过 InitNavigationControl 自动创建 UPathFollowingComponent
    UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, TargetLocation);

    // 获取 SimpleMoveToLocation 创建的 PathFollowingComponent
    PathFollowingComp = Controller->FindComponentByClass<UPathFollowingComponent>();
    if (!PathFollowingComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("AbilityTask_NavMoveTo: PathFollowingComponent not found after SimpleMoveToLocation!"));
        OnFailed.Broadcast();
        EndTask();
        return;
    }

    // 绑定 OnRequestFinished 委托，监听寻路结果
    // FMoveComplete 是非动态多播委托，使用 AddUObject 绑定
    PathFinishedHandle = PathFollowingComp->OnRequestFinished.AddUObject(
        this, &UAbilityTask_NavMoveTo::OnPathFollowingFinished);
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
