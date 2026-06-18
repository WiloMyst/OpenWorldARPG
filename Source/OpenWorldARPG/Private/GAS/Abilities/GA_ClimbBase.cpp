// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_ClimbBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "Characters/PlayerCharacter.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/Character.h"

UGA_ClimbBase::UGA_ClimbBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_ClimbBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    if (!ASC || !CustomMoveComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================
    // 1. 从 TriggerEventData 解析墙壁 HitResult
    // ==========================================
    FHitResult WallHit;
    if (TriggerEventData && TriggerEventData->TargetData.IsValid(0))
    {
        WallHit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(TriggerEventData->TargetData, 0);
    }

    if (!WallHit.bBlockingHit)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================
    // 2. GA 命令 CMC 进入攀爬物理模式
    // ==========================================
    CustomMoveComp->EnterClimb(WallHit);

    // ==========================================
    // 3. 添加 Transition Tag（蒙太奇期间锁定攀爬输入）
    //    攀爬状态 Tag 由 GA 的 AbilityTags 自动管理，无需手动添加。
    // ==========================================
    if (ClimbTransitionTag.IsValid())
    {
        ASC->AddLooseGameplayTag(ClimbTransitionTag);
    }

    // ==========================================
    // 4. 立即监听停止攀爬事件
    //    统一事件源：
    //    - CMC 检测到落地/离开墙壁时发送 StopClimbEventTag
    //    - PlayerCharacter 攀爬中按跳跃时也发送 StopClimbEventTag
    //    两者走同一个事件，GA 收到后统一 EndAbility → ExitClimb
    // ==========================================
    if (StopClimbEventTag.IsValid())
    {
        WaitStopEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopClimbEventTag, nullptr, false, true);
        if (WaitStopEventTask)
        {
            WaitStopEventTask->EventReceived.AddDynamic(this, &UGA_ClimbBase::OnStopClimbEventReceived);
            WaitStopEventTask->ReadyForActivation();
        }
    }

    // ==========================================
    // 5. 设置 Motion Warping Target
    // ==========================================
    ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor);
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(Char);
    UMotionWarpingComponent* MotionWarpingComp = PlayerChar ? PlayerChar->GetMotionWarpingComp() : nullptr;

    if (MotionWarpingComp && ClimbStartWarpTargetName.IsValid())
    {
        const FTransform WarpTarget = CustomMoveComp->CalculateClimbWarpTarget(WallHit);
        MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(ClimbStartWarpTargetName, WarpTarget);
    }

    // ==========================================
    // 6. 播放上墙过渡蒙太奇
    // ==========================================
    bTransitionFinished = false;

    if (TransitionMontage)
    {
        PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, TransitionMontage, 1.0f, NAME_None, false);

        if (PlayMontageTask)
        {
            PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ClimbBase::OnTransitionMontageCompleted);
            PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ClimbBase::OnTransitionMontageInterrupted);
            PlayMontageTask->ReadyForActivation();
        }
        else
        {
            OnTransitionFinished();
        }
    }
    else
    {
        OnTransitionFinished();
    }
}

void UGA_ClimbBase::OnTransitionMontageCompleted()
{
    OnTransitionFinished();
}

void UGA_ClimbBase::OnTransitionMontageInterrupted()
{
    OnTransitionFinished();
}

void UGA_ClimbBase::OnTransitionFinished()
{
    if (bTransitionFinished) return;
    bTransitionFinished = true;

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    // 移除 Transition Tag，允许攀爬输入（ClimbStateTag 保留到 EndAbility）
    if (ASC && ClimbTransitionTag.IsValid())
    {
        ASC->RemoveLooseGameplayTag(ClimbTransitionTag);
    }

    // 过渡完成后才开始扣体力
    if (ClimbStaminaDrainGE && ASC)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ClimbStaminaDrainGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            StaminaDrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void UGA_ClimbBase::OnStopClimbEventReceived(FGameplayEventData Payload)
{
    // CMC 检测到落地/离开墙壁，或玩家按跳跃退出 → 统一结束攀爬
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ClimbBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // GA 结束，物理状态一定退出
    if (CustomMoveComp && CustomMoveComp->IsClimbing())
    {
        CustomMoveComp->ExitClimb();
    }

    // 清理攀爬 Tag（ClimbStateTag 由 GA AbilityTags 自动管理，只需清理 Transition Tag）
    if (ASC && ClimbTransitionTag.IsValid())
    {
        ASC->RemoveLooseGameplayTag(ClimbTransitionTag);
    }

    // 清理体力扣除 GE
    if (ASC && StaminaDrainGEHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(StaminaDrainGEHandle);
        StaminaDrainGEHandle = FActiveGameplayEffectHandle();
    }

    // 停止异步等待任务
    if (PlayMontageTask)
    {
        PlayMontageTask->EndTask();
        PlayMontageTask = nullptr;
    }
    if (WaitStopEventTask)
    {
        WaitStopEventTask->EndTask();
        WaitStopEventTask = nullptr;
    }

    bTransitionFinished = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
