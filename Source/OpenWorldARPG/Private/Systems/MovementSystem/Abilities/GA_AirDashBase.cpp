// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MovementSystem/Abilities/GA_AirDashBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitMovementModeChange.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Systems/MovementSystem/Components/PlayerCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/KismetMathLibrary.h"

UGA_AirDashBase::UGA_AirDashBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_AirDashBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 鸣潮规则：AirDash 一次性扣除体力，完全由 GAS 原生 Cost GE 机制处理
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

    // 安全检查：确保角色处于滞空状态
    // 正常情况下由 ActivationRequiredTags=FallingTag 在 GAS 层面拦截，此处为兜底
    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp || !MoveComp->IsFalling())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 状态 Tag 由 ActivationOwnedTags 管理（无敌帧 Tag 等）

    // 计算冲刺方向并应用位移
    ApplyAirDashMovement();
}

void UGA_AirDashBase::ApplyAirDashMovement()
{
    if (!CachedPlayer) return;

    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp) return;

    // 获取移动输入向量
    const FVector InputVector = MoveComp->GetLastInputVector();
    const bool bHasMovementInput = InputVector.SquaredLength() > MovementInputThreshold;

    // 计算冲刺方向（仅水平面，空中冲刺不改变垂直运动）
    FVector DashDirection;
    UAnimMontage* MontageToPlay;

    if (bHasMovementInput)
    {
        // 有移动输入：向移动方向空中冲刺
        // 投影到水平面，确保 Z 分量为 0
        DashDirection = InputVector.GetSafeNormal2D();
        MontageToPlay = AirDashMontage;
        bIsBackDash = false;

        // 旋转角色朝向冲刺方向
        const FRotator TargetRotation = UKismetMathLibrary::MakeRotFromX(DashDirection);
        CachedPlayer->SetActorRotation(TargetRotation);
    }
    else
    {
        // 无移动输入：向角色后方空中闪避，不旋转胶囊体，保持原朝向
        // 同样投影到水平面
        DashDirection = (-CachedPlayer->GetActorForwardVector()).GetSafeNormal2D();
        MontageToPlay = AirBackDashMontage;
        bIsBackDash = true;
    }

    // 缓存冲刺方向，供 OnAirDashFinished 计算惯性速度
    CachedDashDirection = DashDirection;

    // 计算目标位置（Z 轴保持不变，形成短暂滞停感）
    const FVector StartLocation = CachedPlayer->GetActorLocation();
    const FVector TargetLocation = FVector(
        StartLocation.X + DashDirection.X * AirDashDistance,
        StartLocation.Y + DashDirection.Y * AirDashDistance,
        StartLocation.Z  // Z 不变：RMS Override 期间角色悬停
    );

    // 播放蒙太奇（纯表现层，位移由 RMS 驱动）
    if (MontageToPlay)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f);
        MontageTask->OnCompleted.AddDynamic(this, &UGA_AirDashBase::OnMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UGA_AirDashBase::OnMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UGA_AirDashBase::OnMontageFinished);
        MontageTask->ReadyForActivation();
    }

    // 创建 RootMotionSource 位移（水平移动，Z 锁定）
    TSharedPtr<FRootMotionSource_MoveToForce> RMS = MakeShared<FRootMotionSource_MoveToForce>();
    RMS->InstanceName = FName("AirDash");
    RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
    RMS->Priority = 5;
    RMS->StartLocation = StartLocation;
    RMS->TargetLocation = TargetLocation;
    RMS->Duration = AirDashDuration;
    RMS->bRestrictSpeedToExpected = true;
    // 位移结束后保持当前速度（不归零），由 OnAirDashFinished 精确设置惯性速度
    RMS->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;

    AirDashRMS_ID = MoveComp->ApplyRootMotionSource(RMS);
    bHasActiveRMS = true;

    // 监听落地事件：空中冲刺期间角色可能落地，需立即清理 RMS
    LandingTask = UAbilityTask_WaitMovementModeChange::CreateWaitMovementModeChange(this, MOVE_Walking);
    LandingTask->OnChange.AddDynamic(this, &UGA_AirDashBase::OnLanded);
    LandingTask->ReadyForActivation();

    // 设置定时器：RMS 完成后触发清理逻辑
    FTimerHandle AirDashFinishTimer;
    GetWorld()->GetTimerManager().SetTimer(
        AirDashFinishTimer,
        this,
        &UGA_AirDashBase::OnAirDashFinished,
        AirDashDuration,
        false
    );
}

void UGA_AirDashBase::OnAirDashFinished()
{
    if (!CachedPlayer) return;

    // AirDash 结束时设置惯性速度
    // 前冲 AirDash：沿冲刺方向施加水平惯性速度，Z 速度归零（重力自然接管恢复下落）
    // 后撤步：水平速度清零，Z 速度归零（防御动作应干净利落地停下，垂直方向也重置）
    if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
    {
        if (!bIsBackDash && AirDashEndInertiaSpeed > 0.0f && !CachedDashDirection.IsNearlyZero())
        {
            // 前冲：保留水平惯性，Z 归零让重力重新接管
            MoveComp->Velocity.X = CachedDashDirection.X * AirDashEndInertiaSpeed;
            MoveComp->Velocity.Y = CachedDashDirection.Y * AirDashEndInertiaSpeed;
            MoveComp->Velocity.Z = 0.0f;
        }
        else
        {
            // 后撤步：水平清零，Z 归零
            MoveComp->Velocity.X = 0.0f;
            MoveComp->Velocity.Y = 0.0f;
            MoveComp->Velocity.Z = 0.0f;
        }
    }

    bHasActiveRMS = false;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AirDashBase::OnMontageFinished()
{
    // 蒙太奇播放完毕或被打断，结束技能
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AirDashBase::OnLanded(EMovementMode NewMovementMode)
{
    // 角色在空中冲刺期间落地，立即清理并结束技能
    // 落地后由正常的地面移动逻辑接管，不播放特殊落地蒙太奇
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AirDashBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 安全移除 RootMotionSource（防止技能被打断时 RMS 残留导致角色持续位移）
    if (bHasActiveRMS && CachedPlayer)
    {
        if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
        {
            MoveComp->RemoveRootMotionSourceByID(AirDashRMS_ID);
        }
        bHasActiveRMS = false;
    }

    // 杀死异步监听任务
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
    if (LandingTask) { LandingTask->EndTask(); LandingTask = nullptr; }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
