// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_ReviveBase.h"
#include "Interfaces/CombatInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"

UGA_ReviveBase::UGA_ReviveBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_ReviveBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 死亡 GA 的 Cancel 由蓝图配置 CancelAbilitiesWithTag = Ability.Death 自动完成，
    // 无需在此手动调用 CancelAbilities。

    // ==========================================
    // 步骤1：关闭布娃娃物理，恢复动画蓝图控制
    // ==========================================
    StopRagdoll();

    // ==========================================
    // 步骤2：执行底层还原（碰撞恢复、移动恢复、输入恢复）
    // ==========================================
    if (ActorInfo->AvatarActor.IsValid() && ActorInfo->AvatarActor->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_HandleRevive(ActorInfo->AvatarActor.Get());
    }

    // ==========================================
    // 步骤3：应用回血与无敌帧 GE
    // ==========================================
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        if (HealEffectClass)
        {
            FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
            EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
            FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(HealEffectClass, 1.0f, EffectContext);
            if (SpecHandle.IsValid())
            {
                ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }

        if (InvincibleEffectClass)
        {
            FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
            EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
            FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InvincibleEffectClass, 1.0f, EffectContext);
            if (SpecHandle.IsValid())
            {
                ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }

    // ==========================================
    // 步骤4：表现层 —— 播放起身蒙太奇
    // ==========================================
    if (ReviveMontage)
    {
        MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ReviveMontage);

        if (MontageTask)
        {
            // 统一绑定所有结束委托到同一回调
            MontageTask->OnBlendOut.AddDynamic(this, &UGA_ReviveBase::OnReviveMontageFinished);
            MontageTask->OnCompleted.AddDynamic(this, &UGA_ReviveBase::OnReviveMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UGA_ReviveBase::OnReviveMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UGA_ReviveBase::OnReviveMontageFinished);
            MontageTask->ReadyForActivation();
            return;
        }
    }

    // 未配置 ReviveMontage 或任务创建失败，直接走结束流程
    OnReviveMontageFinished();
}

void UGA_ReviveBase::OnReviveMontageFinished()
{
    // 结束复活能力
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ReviveBase::StopRagdoll()
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    USkeletalMeshComponent* MeshComp = AvatarChar->GetMesh();
    if (!MeshComp) return;

    // 关闭全身物理模拟
    MeshComp->SetAllBodiesSimulatePhysics(false);

    // 关闭物理融合
    MeshComp->bBlendPhysics = false;

    // 恢复网格体碰撞配置为角色默认
    MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));

    // 安全重置：将 Mesh 重新贴合到胶囊体，防止布娃娃导致的骨骼漂移
    if (UCapsuleComponent* Capsule = AvatarChar->GetCapsuleComponent())
    {
        MeshComp->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
        MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
        MeshComp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    }
}
