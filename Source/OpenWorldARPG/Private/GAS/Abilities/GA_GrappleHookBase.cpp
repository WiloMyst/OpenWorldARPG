// Copyright 2025 WiloMyst. All Rights Reserved.


#include "GAS/Abilities/GA_GrappleHookBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Characters/PlayerCharacter.h"
#include "Components/TargetSelectionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

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

    CachedPlayer = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!CachedPlayer)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 对应蓝图图1：获取最优钩索锚点
    UTargetSelectionComponent* GrappleComp = CachedPlayer->FindComponentByClass<UTargetSelectionComponent>();
    if (GrappleComp)
    {
        CurrentHookTarget = GrappleComp->GetBestTarget();
    }

    if (!CurrentHookTarget)
    {
        // 如果没有目标，技能释放失败
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2. 赋予钩索状态 GE
    if (GrapplingStateEffectClass)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddSourceObject(this);
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GrapplingStateEffectClass, 1.0f, Context);
        GrapplingStateEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }

    // 3. 对应蓝图图5：修正朝向
    OrientToTarget();

    // 4. 对应蓝图图2：播放蒙太奇
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
    if (!CachedPlayer || !CurrentHookTarget) return;

    FRotator CurrentRot = CachedPlayer->GetActorRotation();
    FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(CachedPlayer->GetActorLocation(), CurrentHookTarget->GetActorLocation());

    // 仅修改 Z轴(Yaw)，保留原本的 X 和 Y
    FRotator TargetRot = FRotator(0.0f, LookAtRot.Yaw, 0.0f);
    CachedPlayer->SetActorRotation(TargetRot);
}

void UGA_GrappleHookBase::TriggerGrappleVFX()
{
    if (!CachedPlayer || !RopeVFXTemplate || !CurrentHookTarget) return;

    USkeletalMeshComponent* MeshComp = CachedPlayer->GetMesh();
    if (!MeshComp) return;

    // 生成 Niagara 特效并吸附到手腕插槽
    SpawnedRopeVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
        RopeVFXTemplate,
        MeshComp,
        AttachSocketName,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        true // bAutoDestroy
    );

    if (SpawnedRopeVFX)
    {
        // 设置 Niagara 的终点变量 (End)
        SpawnedRopeVFX->SetVariableVec3(RopeEndParamName, CurrentHookTarget->GetActorLocation());

        // 提前计算总耗时：前摇延迟 + 飞行时间(距离/速度)
        float Distance = FVector::Dist(CachedPlayer->GetActorLocation(), CurrentHookTarget->GetActorLocation());
        float PredictedMoveTime = Distance / GrappleMoveSpeed;

        // 加入极小值保护，防止除以零或时间过短导致特效闪烁
        PredictedMoveTime = FMath::Max(PredictedMoveTime, 0.05f);

        float TotalLifetime = HookDelay + PredictedMoveTime;

        // 设置 Niagara 变量 (浮点) -> Lifetime
        SpawnedRopeVFX->SetVariableFloat(RopeLifetimeParamName, TotalLifetime);
    }
}

void UGA_GrappleHookBase::OnDelayFinished()
{
    if (!CachedPlayer || !CurrentHookTarget) return;

    // 1. 计算移动时间 (Time = Distance / Velocity)
    float Distance = FVector::Dist(CachedPlayer->GetActorLocation(), CurrentHookTarget->GetActorLocation());
    float MoveTime = Distance / GrappleMoveSpeed;

    // 安全保护：防止极近距离导致时间趋近于 0
    MoveTime = FMath::Max(MoveTime, 0.05f);

    // 2. 对应蓝图图4：使用 MoveComponentTo 平滑位移
    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = FName("OnMoveCompleted"); // 移动完成后呼叫这个函数
    LatentInfo.Linkage = 0;
    LatentInfo.UUID = FMath::Rand();

    UKismetSystemLibrary::MoveComponentTo(
        CachedPlayer->GetRootComponent(),
        CurrentHookTarget->GetActorLocation(),
        CachedPlayer->GetActorRotation(), // 保持朝向
        false, false,
        MoveTime,
        false,
        EMoveComponentAction::Move,
        LatentInfo
    );
}

void UGA_GrappleHookBase::OnMoveCompleted()
{
    // 对应蓝图图4后半段：清空速度，停止动画
    if (CachedPlayer)
    {
        if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
        {
            MoveComp->Velocity = FVector::ZeroVector;
        }

        // 停止投掷钩索的蒙太奇
        UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
        if (ASC && GrappleMontage)
        {
            ASC->CurrentMontageStop();
        }
    }

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
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // 1. 对应蓝图图1结尾：移除状态 GE
    if (ASC && GrapplingStateEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(GrapplingStateEffectHandle);
        GrapplingStateEffectHandle.Invalidate();
    }

    // 2. 极致安全防泄漏：销毁特效绳子
    if (SpawnedRopeVFX)
    {
        SpawnedRopeVFX->DestroyComponent();
        SpawnedRopeVFX = nullptr;
    }

    // 3. 杀死所有的异步监听任务
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
    if (DelayTask) { DelayTask->EndTask(); DelayTask = nullptr; }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}