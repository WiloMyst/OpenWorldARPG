// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_SwapIn.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimInstance.h"

UGA_SwapIn::UGA_SwapIn()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_SwapIn::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // 从角色读取 Controller 在激活前写入的 SwapIn Transform
    SwapInTransform = CachedPlayer->GetPendingSwapInTransform();

    // 1. 解除待机模式并设置 Transform
    ExitStandbyAndSetTransform();

    // 2. 赋予无敌 GE
    ApplyInvincibleEffect();

    // 3. 播放出场蒙太奇（如果有）
    if (SwapInMontage)
    {
        PlaySwapInMontage();
    }
    else
    {
        // 没有蒙太奇则直接结束
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_SwapIn::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SwapIn::ExitStandbyAndSetTransform()
{
    if (!CachedPlayer) return;

    // 1. 先解除待机（恢复碰撞和 Tick）
    CachedPlayer->SetStandbyMode(false);

    // 2. 使用 TeleportPhysics 标志设置 Transform，切断基础刚体和物理惯性
    CachedPlayer->SetActorLocationAndRotation(
        SwapInTransform.GetLocation(),
        SwapInTransform.GetRotation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    // 3. 解决布料 (Chaos Cloth) 和 动画物理节点 的残留拉扯
    if (USkeletalMeshComponent* SKMesh = CachedPlayer->GetMesh())
    {
        SKMesh->ForceClothNextUpdateTeleportAndReset();

        if (UAnimInstance* AnimInst = SKMesh->GetAnimInstance())
        {
            AnimInst->ResetDynamics(ETeleportType::TeleportPhysics);
        }
    }

    // 4. 解决武器弹簧臂 (CameraLag) 的拉扯
    // 遍历角色身上的弹簧臂组件，瞬间开关一次 Lag 强制画面硬切
    TArray<USpringArmComponent*> SpringArms;
    CachedPlayer->GetComponents<USpringArmComponent>(SpringArms);
    for (USpringArmComponent* SpringArm : SpringArms)
    {
        if (SpringArm->bEnableCameraLag)
        {
            bool bWasLagEnabled = SpringArm->bEnableCameraLag;
            SpringArm->bEnableCameraLag = false;
            SpringArm->UpdateComponentToWorld(); // 强制在无延迟状态下更新一帧
            SpringArm->bEnableCameraLag = bWasLagEnabled;
        }
    }
}

void UGA_SwapIn::PlaySwapInMontage()
{
    if (!CachedPlayer || !SwapInMontage) return;

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, SwapInMontage, 1.0f, NAME_None, true, 1.0f, 0.0f);

    MontageTask->OnCompleted.AddDynamic(this, &UGA_SwapIn::OnMontageFinished);
    MontageTask->OnBlendOut.AddDynamic(this, &UGA_SwapIn::OnMontageFinished);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_SwapIn::OnMontageFinished);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_SwapIn::OnMontageFinished);

    MontageTask->ReadyForActivation();
}

void UGA_SwapIn::ApplyInvincibleEffect()
{
    if (!CachedPlayer || !InvincibleEffectClass) return;

    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(this);

    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InvincibleEffectClass, GetAbilityLevel(), Context);
    if (SpecHandle.IsValid())
    {
        ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
}

void UGA_SwapIn::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
