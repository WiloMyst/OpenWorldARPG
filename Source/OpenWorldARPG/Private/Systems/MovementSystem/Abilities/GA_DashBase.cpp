// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MovementSystem/Abilities/GA_DashBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Systems/MovementSystem/Components/PlayerCharacterMovementComponent.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/KismetMathLibrary.h"

UGA_DashBase::UGA_DashBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_DashBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 重置上一轮残留的状态标志位，防止第二次激活时被拦截
    bTransitioningToSprint = false;

    // 鸣潮规则：Dash 一次性扣除体力，完全由 GAS 原生 Cost GE 机制处理
    // 策划只需在蓝图默认属性的 Cost Gameplay Effect Class 中配置体力消耗 GE
    // CommitAbility 会自动检查 Cost 并应用，无需手动 GetNumericAttribute 或 ApplyGameplayEffectSpecToSelf
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedPlayer = Cast<APlayerCharacter>(ActorInfo->AvatarActor.Get());
    if (!CachedPlayer)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 状态 Tag 由 ActivationOwnedTags 管理（无敌帧 Tag 等）

    // 计算冲刺方向并应用位移
    ApplyDashMovement();
}

void UGA_DashBase::ApplyDashMovement()
{
    if (!CachedPlayer) return;

    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp) return;

    // 获取移动输入向量
    const FVector InputVector = MoveComp->GetLastInputVector();
    const bool bHasMovementInput = InputVector.SquaredLength() > MovementInputThreshold;

    // 计算冲刺方向
    FVector DashDirection;
    UAnimMontage* MontageToPlay;

    if (bHasMovementInput)
    {
        // 有移动输入：向移动方向冲刺
        DashDirection = InputVector.GetSafeNormal();
        MontageToPlay = DashMontage;
        bIsBackDash = false;

        // 旋转角色朝向冲刺方向
        const FRotator TargetRotation = UKismetMathLibrary::MakeRotFromX(DashDirection);
        CachedPlayer->SetActorRotation(TargetRotation);
    }
    else
    {
        // 无移动输入：向角色后方闪避，不旋转胶囊体，保持原朝向
        DashDirection = -CachedPlayer->GetActorForwardVector();
        MontageToPlay = BackDashMontage;
        bIsBackDash = true;
    }

    // 缓存冲刺方向，供 OnDashFinished 计算惯性速度
    CachedDashDirection = DashDirection;

    // 计算目标位置
    const FVector StartLocation = CachedPlayer->GetActorLocation();
    const FVector TargetLocation = StartLocation + DashDirection * DashDistance;

    // 播放蒙太奇（监听 BlendOut 用于过渡到 Sprint，监听 Completed/Interrupted 用于结束）
    if (MontageToPlay)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f);
        MontageTask->OnBlendOut.AddDynamic(this, &UGA_DashBase::OnMontageBlendOut);
        MontageTask->OnCompleted.AddDynamic(this, &UGA_DashBase::OnMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UGA_DashBase::OnMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UGA_DashBase::OnMontageFinished);
        MontageTask->ReadyForActivation();
    }

    // 创建 RootMotionSource 位移
    TSharedPtr<FRootMotionSource_MoveToForce> RMS = MakeShared<FRootMotionSource_MoveToForce>();
    RMS->InstanceName = FName("Dash");
    RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
    RMS->Priority = 5;
    RMS->StartLocation = StartLocation;
    RMS->TargetLocation = TargetLocation;
    RMS->Duration = DashDuration;
    RMS->bRestrictSpeedToExpected = true;
    // 位移结束后保持当前速度（不归零），由 OnDashFinished 设置惯性速度
    RMS->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;

    DashRMS_ID = MoveComp->ApplyRootMotionSource(RMS);
    bHasActiveRMS = true;

    // 设置定时器：RMS 完成后触发清理逻辑
    FTimerHandle DashFinishTimer;
    GetWorld()->GetTimerManager().SetTimer(
        DashFinishTimer,
        this,
        &UGA_DashBase::OnDashFinished,
        DashDuration,
        false
    );
}

void UGA_DashBase::OnMontageBlendOut()
{
    // 蒙太奇即将结束（BlendOut），检测是否应过渡到 Sprint
    TryTransitionToSprint();
}

void UGA_DashBase::TryTransitionToSprint()
{
    if (!CachedPlayer || bTransitioningToSprint) return;

    // 漏洞一修复：后撤步是绝对的防御动作，绝不能接续疾跑
    // 在触发瞬间（ApplyDashMovement）记录性质，而非 BlendOut 时的摇杆状态
    if (bIsBackDash)
    {
        return;
    }

    // 鸣潮规则：无方向输入 → 不接续疾跑
    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp || MoveComp->GetLastInputVector().SquaredLength() <= MovementInputThreshold)
    {
        return;
    }

    // 鸣潮规则：Sprint 全程不消耗体力，体力已在 Dash 激活时一次性扣除
    // 只要 Dash 成功激活（CommitAbility 通过），即可过渡到 Sprint，无需额外体力检查

    // 所有条件满足，标记过渡中
    bTransitioningToSprint = true;

    // 通过 Controller 的 bIsSprintActionHeld 判断点按/长按
    // EventMagnitude: 1.0 = 长按（持久疾跑），0.0 = 点按（短时疾跑）
    AGameplayPlayerController* PC = CachedPlayer->GetController<AGameplayPlayerController>();
    const bool bIsLongPress = PC ? PC->IsSprintActionHeld() : false;

    if (SprintStartEventTag.IsValid())
    {
        FGameplayEventData EventData;
        EventData.EventMagnitude = bIsLongPress ? 1.0f : 0.0f;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CachedPlayer, SprintStartEventTag, EventData);
    }
}

void UGA_DashBase::OnDashFinished()
{
    if (!CachedPlayer) return;

    // Dash 结束时保留惯性速度，避免速度归零导致的顿感
    // 前冲 Dash：沿冲刺方向施加惯性速度，平滑过渡到 Sprint
    // 后撤步：不施加惯性（防御动作应干净利落地停下）
    if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
    {
        if (!bIsBackDash && DashEndInertiaSpeed > 0.0f && !CachedDashDirection.IsNearlyZero())
        {
            // 仅设置水平速度，保留垂直速度（Z）防止斜坡/悬崖边异常
            MoveComp->Velocity.X = CachedDashDirection.X * DashEndInertiaSpeed;
            MoveComp->Velocity.Y = CachedDashDirection.Y * DashEndInertiaSpeed;
        }
        else
        {
            // 后撤步：清零水平速度
            MoveComp->Velocity.X = 0.0f;
            MoveComp->Velocity.Y = 0.0f;
        }
    }

    bHasActiveRMS = false;

    // 如果正在过渡到 Sprint，不需要在这里 EndAbility（蒙太奇结束时会触发）
    // 否则正常结束
    if (!bTransitioningToSprint)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_DashBase::OnMontageFinished()
{
    // 蒙太奇播放完毕或被打断，结束技能
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_DashBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 安全移除 RootMotionSource（防止技能被打断时 RMS 残留导致角色持续位移）
    if (bHasActiveRMS && CachedPlayer)
    {
        if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
        {
            MoveComp->RemoveRootMotionSourceByID(DashRMS_ID);
        }
        bHasActiveRMS = false;
    }

    // 杀死异步监听任务
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
