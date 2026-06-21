// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_DieBase.h"
#include "Interfaces/CombatInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
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

    // 只要不 EndAbility，蓝图中配置的 ActivationOwnedTags (如 Character.State.Dead) 就会一直存在，
    // 死亡 GA 将在 GA_ReviveBase 激活时通过 CancelAbilities 被显式打断，从而自动剥离死亡 Tag。
}

void UGA_DieBase::StartRagdoll()
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    USkeletalMeshComponent* MeshComp = AvatarChar->GetMesh();
    if (!MeshComp) return;

    // ==========================================
    // 关键1：彻底停止 CharacterMovementComponent
    // CMC 每帧会覆盖 Mesh 位置，导致物理模拟失效、重力丢失
    // ==========================================
    if (UCharacterMovementComponent* MoveComp = AvatarChar->GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->DisableMovement();
        MoveComp->SetComponentTickEnabled(false);
    }

    // ==========================================
    // 关键2：将 Mesh 从 Capsule 分离（保持世界位置）
    // 否则 Capsule 的移动会拖拽 Mesh，物理模拟被覆盖
    // ==========================================
    MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

    // ==========================================
    // 关键3：关闭 Capsule 碰撞，避免与 Ragdoll 物理体冲突
    // ==========================================
    if (UCapsuleComponent* Capsule = AvatarChar->GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    // ==========================================
    // 步骤4：配置 Ragdoll 物理模拟
    // ==========================================
    // 设置碰撞为 QueryAndPhysics，使用 Ragdoll 碰撞配置
    MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 开启全身物理模拟
    MeshComp->SetAllBodiesSimulatePhysics(true);

    // 确保物理重力开启（防止飘浮）
    MeshComp->SetEnableGravity(true);

    // 唤醒所有刚体，避免静止不动
    MeshComp->WakeAllRigidBodies();

    // 开启物理融合，使动画与物理平滑过渡
    MeshComp->bBlendPhysics = true;

    // ==========================================
    // 关键5：将 Actor 根组件移至 Mesh 位置，避免 Actor 原点与 Ragdoll 脱节
    // ==========================================
    AvatarChar->SetActorLocation(MeshComp->GetComponentLocation(), false);
}
