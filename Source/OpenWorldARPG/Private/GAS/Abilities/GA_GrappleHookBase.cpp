// Copyright 2025 WiloMyst. All Rights Reserved.


#include "GAS/Abilities/GA_GrappleHookBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TargetingComponent.h"
#include "Components/WeaponManagerComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
// RootMotionSource 头文件
#include "GameFramework/RootMotionSource.h"

UGA_GrappleHookBase::UGA_GrappleHookBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_GrappleHookBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!CachedCharacter)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 0. 武器立即回到背上（钩索不需要武器在手）
    if (UWeaponManagerComponent* WeaponComp = CachedCharacter->FindComponentByClass<UWeaponManagerComponent>())
    {
        WeaponComp->WeaponToBack();
    }

    // 1. 对应蓝图图1：获取最优钩索锚点
    UTargetingComponent* GrappleComp = nullptr;
    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(CachedCharacter))
    {
        GrappleComp = PlayerChar->GetTargetingComponent();
    }
    if (GrappleComp)
    {
        CurrentHookTarget = GrappleComp->GetBestTarget();
    }

    if (!CurrentHookTarget)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2. 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入

    // 3. 对应蓝图图5：修正朝向
    OrientToTarget();

    // 4. 对应蓝图图2：播放蒙太奇（从 VisualDataAsset->GrappleMontage 加载）
    UAnimMontage* GrappleMontage = nullptr;
    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(CachedCharacter))
    {
        if (UCharacterVisualDataAsset* VisualData = PlayerChar->GetVisualDataAsset_Implementation())
        {
            if (!VisualData->GrappleMontage.IsNull())
            {
                GrappleMontage = VisualData->GrappleMontage.LoadSynchronous();
            }
        }
    }
    if (GrappleMontage)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, GrappleMontage, 1.0f, NAME_None, true, 1.0f, 0.0f);
        MontageTask->OnCompleted.AddDynamic(this, &UGA_GrappleHookBase::OnMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UGA_GrappleHookBase::OnMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UGA_GrappleHookBase::OnMontageFinished);
        MontageTask->ReadyForActivation();
    }

    // 5. 对应蓝图图2与图3：生成绳索特效
    TriggerGrappleVFX();

    // 6. 对应蓝图图4：触发 Hook Delay 延迟
    DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, HookDelay);
    DelayTask->OnFinish.AddDynamic(this, &UGA_GrappleHookBase::OnDelayFinished);
    DelayTask->ReadyForActivation();
}

void UGA_GrappleHookBase::OrientToTarget()
{
    if (!CachedCharacter || !CurrentHookTarget) return;

    FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(CachedCharacter->GetActorLocation(), CurrentHookTarget->GetActorLocation());
    FRotator TargetRot = FRotator(0.0f, LookAtRot.Yaw, 0.0f);
    CachedCharacter->SetActorRotation(TargetRot);
}

void UGA_GrappleHookBase::TriggerGrappleVFX()
{
    if (!CachedCharacter || !RopeVFXTemplate || !CurrentHookTarget) return;

    USkeletalMeshComponent* MeshComp = CachedCharacter->GetMesh();
    if (!MeshComp) return;

    SpawnedRopeVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
        RopeVFXTemplate,
        MeshComp,
        AttachSocketName,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        true
    );

    if (SpawnedRopeVFX)
    {
        SpawnedRopeVFX->SetVariableVec3(RopeEndParamName, CurrentHookTarget->GetActorLocation());

        float Distance = FVector::Dist(CachedCharacter->GetActorLocation(), CurrentHookTarget->GetActorLocation());
        float PredictedMoveTime = FMath::Max(Distance / GrappleMoveSpeed, 0.05f);
        float TotalLifetime = HookDelay + PredictedMoveTime;

        SpawnedRopeVFX->SetVariableFloat(RopeLifetimeParamName, TotalLifetime);
    }
}

void UGA_GrappleHookBase::OnDelayFinished()
{
    if (!CachedCharacter || !CurrentHookTarget) return;

    UCharacterMovementComponent* MoveComp = CachedCharacter->GetCharacterMovement();
    if (!MoveComp) return;

        // 废弃 MoveComponentTo，改用 FRootMotionSource_MoveToForce
    // 优势：完美兼容 CMC 的网络预测与回滚 (Prediction & Rollback)
    // MoveComponentTo 是 Latent Action，游离于 CMC 预测体系之外
    
    float Distance = FVector::Dist(CachedCharacter->GetActorLocation(), CurrentHookTarget->GetActorLocation());
    float MoveTime = FMath::Max(Distance / GrappleMoveSpeed, 0.05f);

    // 1. 创建 RootMotionSource
    TSharedPtr<FRootMotionSource_MoveToForce> RMS = MakeShared<FRootMotionSource_MoveToForce>();
    RMS->InstanceName = FName("GrappleHook");
    RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
    RMS->Priority = 5;
    RMS->StartLocation = CachedCharacter->GetActorLocation();
    RMS->TargetLocation = CurrentHookTarget->GetActorLocation();
    RMS->Duration = MoveTime;
    RMS->bRestrictSpeedToExpected = true;
    // 位移结束后速度归零，防止角色残留惯性
    RMS->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    RMS->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

    // 2. 注册到 CMC 的 CurrentRootMotion
    GrappleRMS_ID = MoveComp->ApplyRootMotionSource(RMS);
    bHasActiveRMS = true;

    // 3. 设置定时器：RMS 完成后触发清理逻辑
    // UE5.2 的 RMS 没有完成回调，使用与 Duration 匹配的定时器检测完成
    FTimerHandle GrappleFinishTimer;
    GetWorld()->GetTimerManager().SetTimer(
        GrappleFinishTimer,
        this,
        &UGA_GrappleHookBase::OnGrappleMoveFinished,
        MoveTime,
        false
    );
}

void UGA_GrappleHookBase::OnGrappleMoveFinished()
{
    if (!CachedCharacter) return;

    // 位移完成：清空速度，停止蒙太奇
    if (UCharacterMovementComponent* MoveComp = CachedCharacter->GetCharacterMovement())
    {
        MoveComp->Velocity = FVector::ZeroVector;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        ASC->CurrentMontageStop();
    }

    bHasActiveRMS = false;

    // 完美闭环：移动完成，结束技能
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_GrappleHookBase::OnMontageFinished()
{
    // 兜底保护：如果动画因为其他原因（受击等）被打断，强行结束技能
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_GrappleHookBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 1. 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 2. 安全移除 RootMotionSource (防止技能被打断时 RMS 残留导致角色持续位移)
    if (bHasActiveRMS && CachedCharacter)
    {
        if (UCharacterMovementComponent* MoveComp = CachedCharacter->GetCharacterMovement())
        {
            MoveComp->RemoveRootMotionSourceByID(GrappleRMS_ID);
        }
        bHasActiveRMS = false;
    }

    // 3. 销毁特效绳子
    if (SpawnedRopeVFX)
    {
        SpawnedRopeVFX->DestroyComponent();
        SpawnedRopeVFX = nullptr;
    }

    // 4. 杀死所有的异步监听任务
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
    if (DelayTask) { DelayTask->EndTask(); DelayTask = nullptr; }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
