// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_PlungeAttackBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitMovementModeChange.h"
#include "Characters/PlayerCharacter.h"
#include "Components/CharacterWeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UGA_PlungeAttackBase::UGA_PlungeAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_PlungeAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    ExecuteAttack();
}

void UGA_PlungeAttackBase::ExecuteAttack()
{
    if (!CachedPlayer) return;

    bInterruptedByLand = false;

    // 1. Weapon to Hand
    if (UCharacterWeaponComponent* WeaponComp = CachedPlayer->FindComponentByClass<UCharacterWeaponComponent>())
    {
        WeaponComp->WeaponToHand();
    }

    // 2. 滞空时清理速度 (可选：或者赋予一个向下的冲刺力)
    if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
    {
        MoveComp->Velocity = FVector::ZeroVector;
    }

    // 3. 获取蒙太奇数组
    TArray<TSoftObjectPtr<UAnimMontage>> Montages = CachedPlayer->GetPlungeAttackMontages();
    if (Montages.IsEmpty() || !Montages[0].IsValid())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 加载 [0] 号蒙太奇（空中下落循环动画）
    UAnimMontage* FallMontage = Montages[0].LoadSynchronous();
    if (!FallMontage)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    ClearAllTasks();

    // --- 阶段一：多线程并发监听区 ---

    // 任务1：播放空中下落蒙太奇 (通常这是一个Loop动画，不会自然结束)
    FallMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FallMontage, 1.0f, NAME_None, true, 1.0f, 0.0f);
    // 如果下落被打断或取消，结束技能
    FallMontageTask->OnInterrupted.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    FallMontageTask->OnCancelled.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    FallMontageTask->ReadyForActivation();

    // 任务2：监听移动模式变为 "行走(Walking)" 即落地判定
    MovementModeTask = UAbilityTask_WaitMovementModeChange::CreateWaitMovementModeChange(this, MOVE_Walking);
    MovementModeTask->OnChange.AddDynamic(this, &UGA_PlungeAttackBase::OnMovementModeChanged);
    MovementModeTask->ReadyForActivation();

    // 任务3：全局监听伤害判定事件 (等待 [1] 号蒙太奇里的 AnimNotify 触发)
    if (DamageDealEventTag.IsValid())
    {
        DamageTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DamageDealEventTag, nullptr, false, false);
        DamageTask->EventReceived.AddDynamic(this, &UGA_PlungeAttackBase::OnDamageEventReceived);
        DamageTask->ReadyForActivation();
    }
}

void UGA_PlungeAttackBase::OnMovementModeChanged(EMovementMode NewMovementMode)
{
    // --- 阶段二：落地砸地 ---

    // 1. 停止监听落地事件，停止当前的空中下落蒙太奇
    if (MovementModeTask)
    {
        MovementModeTask->EndTask();
        MovementModeTask = nullptr;
    }
    if (FallMontageTask)
    {
        FallMontageTask->EndTask();
        FallMontageTask = nullptr;
    }

    // 强行停止当前的动画，为落地动画腾出轨道
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        ASC->CurrentMontageStop();
    }

    // 2. 获取并播放 [1] 号蒙太奇（落地砸地动画）
    TArray<TSoftObjectPtr<UAnimMontage>> Montages = CachedPlayer->GetPlungeAttackMontages();
    if (Montages.IsValidIndex(1) && Montages[1].IsValid())
    {
        UAnimMontage* LandingMontage = Montages[1].LoadSynchronous();
        if (LandingMontage)
        {
            LandingMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, LandingMontage, 1.0f, NAME_None, true, 1.0f, 0.0f);

            // 绑定落地动画结束的回调（动画播完后技能才算真正结束）
            LandingMontageTask->OnCompleted.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);
            LandingMontageTask->OnBlendOut.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);
            LandingMontageTask->OnInterrupted.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);
            LandingMontageTask->OnCancelled.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);

            LandingMontageTask->ReadyForActivation();
            return; // 成功播放落地动画，跳出函数等待动画结束
        }
    }

    // 兜底：如果没有配置 [1] 号蒙太奇，落地后直接结束技能
    CorrectPawnOrient();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnMontageFinished()
{
    // 这个用于处理“空中阶段”意外结束（比如被击飞打断）
    CorrectPawnOrient();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnLandingMontageFinished()
{
    // 落地砸地动画播放完毕，完美收尾
    CorrectPawnOrient();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnDamageEventReceived(FGameplayEventData Payload)
{
    // 收到 [1] 号蒙太奇里 AnimNotify 发出的 Tag 广播，执行伤害结算
    ApplyDamageToTargets();
}

void UGA_PlungeAttackBase::ApplyDamageToTargets()
{
    if (!CachedPlayer) return;
    if (!GetAvatarActorFromActorInfo()->HasAuthority()) return;

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!SourceASC || !DamageEffectClass) return;

    FVector ActorLoc = CachedPlayer->GetActorLocation();
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);
    TArray<FHitResult> OutHits;

    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        ActorLoc, ActorLoc,
        PlungeDamageRadius,
        TraceObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    TArray<AActor*> HitActors;

    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor && !HitActors.Contains(HitActor))
        {
            HitActors.Add(HitActor);

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            if (TargetASC)
            {
                FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
                Context.AddHitResult(Hit);
                Context.AddSourceObject(this);

                FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
                if (SpecHandle.IsValid())
                {
                    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
                }
            }
        }
    }
}

void UGA_PlungeAttackBase::ClearAllTasks()
{
    if (FallMontageTask) { FallMontageTask->EndTask(); FallMontageTask = nullptr; }
    if (LandingMontageTask) { LandingMontageTask->EndTask(); LandingMontageTask = nullptr; }
    if (MovementModeTask) { MovementModeTask->EndTask(); MovementModeTask = nullptr; }
    if (DamageTask) { DamageTask->EndTask(); DamageTask = nullptr; }
}

void UGA_PlungeAttackBase::CorrectPawnOrient()
{
    if (!CachedPlayer) return;
    FRotator CurrentRot = CachedPlayer->GetActorRotation();
    FRotator CorrectedRot = FRotator(0.0f, CurrentRot.Yaw, 0.0f);
    CachedPlayer->SetActorRotation(CorrectedRot);
}

void UGA_PlungeAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 被其他 GA Cancel 时，武器回到背上
    if (bWasCancelled && CachedPlayer)
    {
        if (UCharacterWeaponComponent* WeaponComp = CachedPlayer->FindComponentByClass<UCharacterWeaponComponent>())
        {
            WeaponComp->WeaponToBack();
        }
    }

    ClearAllTasks();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}