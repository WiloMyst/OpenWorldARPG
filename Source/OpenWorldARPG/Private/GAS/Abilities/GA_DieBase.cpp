// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_DieBase.h"
#include "Interfaces/CombatInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"

UGA_DieBase::UGA_DieBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_DieBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================
    // 步骤1：强制打断角色正在进行的其他所有技能（攀爬、攻击等）
    // ==========================================
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->CancelAllAbilities(this);
    }

    // ==========================================
    // 步骤2：执行底层清理（碰撞剥离、移动停止、禁用输入等）
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入
    // ==========================================
    if (ActorInfo->AvatarActor.IsValid() && ActorInfo->AvatarActor->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_HandleDeath(ActorInfo->AvatarActor.Get());
    }

    // ==========================================
    // 步骤3：表现层 —— 播放死亡蒙太奇
    // ==========================================
    if (DeathMontage)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, DeathMontage);

        if (MontageTask)
        {
            // 统一绑定所有结束委托到同一回调，无论正常结束、混出、打断还是取消
            MontageTask->OnBlendOut.AddDynamic(this, &UGA_DieBase::OnDeathMontageFinished);
            MontageTask->OnCompleted.AddDynamic(this, &UGA_DieBase::OnDeathMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UGA_DieBase::OnDeathMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UGA_DieBase::OnDeathMontageFinished);
            MontageTask->ReadyForActivation();
            return;
        }
    }

    // 未配置 DeathMontage 或任务创建失败，直接走结束流程
    OnDeathMontageFinished();
}

void UGA_DieBase::OnDeathMontageFinished()
{
    // 步骤4：开启布娃娃物理（若启用）
    if (bEnableRagdollOnDeath)
    {
        StartRagdoll();
    }

    // 步骤5：结束死亡能力
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_DieBase::StartRagdoll()
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    USkeletalMeshComponent* MeshComp = AvatarChar->GetMesh();
    if (!MeshComp) return;

    // 设置碰撞为 QueryAndPhysics，使用 Ragdoll 碰撞配置
    MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 开启全身物理模拟
    MeshComp->SetAllBodiesSimulatePhysics(true);

    // 唤醒所有刚体，避免静止不动
    MeshComp->WakeAllRigidBodies();

    // 开启物理融合，使动画与物理平滑过渡
    MeshComp->bBlendPhysics = true;
}
