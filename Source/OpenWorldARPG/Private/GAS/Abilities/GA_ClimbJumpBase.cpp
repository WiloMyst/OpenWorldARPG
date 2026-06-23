// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_ClimbJumpBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/PlayerCharacter.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"
#include "GameFramework/Character.h"

UGA_ClimbJumpBase::UGA_ClimbJumpBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_ClimbJumpBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());

    // 安全检查：必须处于攀爬状态
    if (!CustomMoveComp || !PlayerChar || !CustomMoveComp->IsClimbing())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 读取输入意图：InputY > 0 为向前/上（向上冲刺），InputY < 0 为向后/下（脱墙后空翻）
    const float InputY = PlayerChar->GetCurrentInputY();

    // 分支判断
    if (InputY >= DashUpInputThreshold)
    {
        // 分支 A：向上冲刺（保持攀爬状态）
        ExecuteClimbDashUp(Handle, ActorInfo, ActivationInfo);
    }
    else
    {
        // 分支 B：脱墙后空翻（涵盖按 S 或没按方向键）
        ExecuteWallEject(ActorInfo);
    }
}

void UGA_ClimbJumpBase::ExecuteClimbDashUp(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    bIsWallEject = false;

    // 扣除体力：如果不满足体力则取消技能
    if (StaminaCostGE && ASC)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaminaCostGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // 播放向上冲刺蒙太奇（依赖蒙太奇自带的 Root Motion 向上位移）
    if (ClimbDashUpMontage)
    {
        PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ClimbDashUpMontage, 1.0f, NAME_None, false);

        if (PlayMontageTask)
        {
            PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageCompleted);
            PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageBlendOut);
            PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageInterrupted);
            PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageCancelled);
            PlayMontageTask->ReadyForActivation();
        }
        else
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        // 没有配置蒙太奇，直接结束
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_ClimbJumpBase::ExecuteWallEject(const FGameplayAbilityActorInfo* ActorInfo)
{
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    if (!CustomMoveComp || !PlayerChar) return;

    bIsWallEject = true;

    // 获取墙面法线（在 ExitClimb 之前获取，否则法线会被清零）
    const FVector WallNormal = CustomMoveComp->GetClimbWallNormal();

    // 调用 CMC 脱墙跳：退出攀爬 + 赋予反冲速度
    CustomMoveComp->DoWallEject();

    // 发送停止攀爬事件，通知 GA_ClimbBase 结束（GA_ClimbBase EndAbility 会再次调用 ExitClimb，
    // 但 IsClimbing() 此时已返回 false，不会重复执行，只会清理 Tag/GE/Task）
    if (ASC && ClimbStopEventTag.IsValid())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PlayerChar, ClimbStopEventTag, FGameplayEventData());
    }

    // 强制角色面朝墙面法线方向（背对墙面，面向外侧）
    if (!WallNormal.IsNearlyZero())
    {
        const FRotator TargetRotation = FRotationMatrix::MakeFromX(WallNormal).Rotator();
        PlayerChar->SetActorRotation(TargetRotation);
    }

    // 播放后空翻蒙太奇（原地动画，位移由 Velocity 驱动）
    if (WallEjectMontage)
    {
        PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, WallEjectMontage, 1.0f, NAME_None, false);

        if (PlayMontageTask)
        {
            PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageCompleted);
            PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageBlendOut);
            PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageInterrupted);
            PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ClimbJumpBase::OnMontageCancelled);
            PlayMontageTask->ReadyForActivation();
        }
        else
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }
    }
    else
    {
        // 没有配置蒙太奇，直接结束（物理脱墙已执行）
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_ClimbJumpBase::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ClimbJumpBase::OnMontageBlendOut()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ClimbJumpBase::OnMontageInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_ClimbJumpBase::OnMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_ClimbJumpBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 停止异步蒙太奇任务
    if (PlayMontageTask)
    {
        PlayMontageTask->EndTask();
        PlayMontageTask = nullptr;
    }

    bIsWallEject = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
