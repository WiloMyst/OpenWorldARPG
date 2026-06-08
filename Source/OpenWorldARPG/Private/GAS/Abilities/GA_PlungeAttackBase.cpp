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
    // 对应图6：设置高级默认项
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

    // 1. 对应图1：赋予下落攻击状态 GE
    if (PlungeStateEffectClass)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddSourceObject(this);
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(PlungeStateEffectClass, 1.0f, Context);
        PlungeStateEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }

    // 2. 对应图2：开始执行攻击流
    ExecuteAttack();
}

void UGA_PlungeAttackBase::ExecuteAttack()
{
    if (!CachedPlayer) return;

    // 1. 对应图2：SET Interrupted by Land = false
    bInterruptedByLand = false;

    // 2. 对应图2：Weapon to Hand
    if (UCharacterWeaponComponent* WeaponComp = CachedPlayer->FindComponentByClass<UCharacterWeaponComponent>())
    {
        WeaponComp->WeaponToHand();
    }

    // 3. 对应图2：清理惯性速度 (SET Velocity 0,0,0)
    if (UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
    {
        MoveComp->Velocity = FVector::ZeroVector;
    }

    // 4. 对应图2：获取下落蒙太奇并提取首个动画
    TArray<TSoftObjectPtr<UAnimMontage>> Montages = CachedPlayer->GetPlungeAttackMontages();
    if (Montages.IsEmpty())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    UAnimMontage* MontageToPlay = Montages[0].LoadSynchronous();
    if (!MontageToPlay)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    ClearAllTasks();

    // ==========================================
    // 多线程并发监听区
    // ==========================================

    // 任务1：播放蒙太奇
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f);
    MontageTask->OnCompleted.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    MontageTask->OnBlendOut.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    MontageTask->ReadyForActivation();

    // 任务2：监听移动模式变为 "行走(Walking)" 即落地判定 (对应图3的 WaitMovementModeChange)
    MovementModeTask = UAbilityTask_WaitMovementModeChange::CreateWaitMovementModeChange(this, MOVE_Walking);
    MovementModeTask->OnChange.AddDynamic(this, &UGA_PlungeAttackBase::OnMovementModeChanged);
    MovementModeTask->ReadyForActivation();

    // 任务3：监听伤害判定事件 (对应图4)
    if (DamageDealEventTag.IsValid())
    {
        DamageTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DamageDealEventTag, nullptr, false, false);
        DamageTask->EventReceived.AddDynamic(this, &UGA_PlungeAttackBase::OnDamageEventReceived);
        DamageTask->ReadyForActivation();
    }
}

void UGA_PlungeAttackBase::OnMovementModeChanged(EMovementMode NewMovementMode)
{
    // 对应图3：落地触发 -> 设置打断标志位并结束技能
    bInterruptedByLand = true;

    // 当调用 EndAbility 时，底层的 MontageTask 会自动执行 Cancelled 回调。
    // 但是因为我们设置了 bInterruptedByLand = true，它不会执行 CorrectPawnOrient。
    // 这完美复刻了你的蓝图逻辑闭环！
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnMontageFinished()
{
    // 对应图3：蒙太奇自然结束 / 被打断 时的分支
    if (!bInterruptedByLand)
    {
        // 如果不是因为落地打断的，执行朝向修正
        CorrectPawnOrient();
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_PlungeAttackBase::OnDamageEventReceived(FGameplayEventData Payload)
{
    // 收到 Tag 广播，执行伤害结算
    ApplyDamageToTargets();
}

void UGA_PlungeAttackBase::ApplyDamageToTargets()
{
    // 对应图4中你自定义的 Apply Damage 事件
    if (!CachedPlayer) return;

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!SourceASC || !DamageEffectClass) return;

    // 以玩家为中心，执行球体范围追踪 (模拟下落震地 AoE 伤害)
    FVector ActorLoc = CachedPlayer->GetActorLocation();

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);
    TArray<FHitResult> OutHits;

    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        ActorLoc, ActorLoc, // Start 和 End 相同，原地范围检测
        PlungeDamageRadius,
        TraceObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    // 防重复受击缓存 (对于 AoE 很重要)
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
                SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
            }
        }
    }
}

void UGA_PlungeAttackBase::ClearAllTasks()
{
    // 强行清理异步任务，避免内存泄漏或逻辑冲突
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
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
    // 对应图1结尾：移除下落状态 GE
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (ASC && PlungeStateEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(PlungeStateEffectHandle);
        PlungeStateEffectHandle.Invalidate();
    }

    ClearAllTasks();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}