// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_MeleeAttackBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/PlayerCharacter.h"
#include "Components/WeaponManagerComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

UGA_MeleeAttackBase::UGA_MeleeAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_MeleeAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // 1. 状态 Tag 由 ActivationOwnedTags 管理，GA 不再通过 GE 重复注入

    // 2. 初始化连招状态
    ComboIndex = 0;
    bCanTriggerAttack = true;

    // 3. 执行第一次攻击
    ExecuteAttack();
}

void UGA_MeleeAttackBase::ExecuteAttack()
{
    if (!bCanTriggerAttack || !CachedPlayer)
    {
        bool bReplicateEndAbility = true;
        bool bWasCancelled = true;
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
        return;
    }

    // 1. 获取武器并执行 Weapon To Hand (对应图2)
    if (UWeaponManagerComponent* WeaponComp = CachedPlayer->FindComponentByClass<UWeaponManagerComponent>())
    {
        WeaponComp->WeaponToHand();
    }

    // 2. 蓝图事件：攻击转向
    AttackOrientation();

    // 3. 获取蒙太奇并校验安全
    TArray<TSoftObjectPtr<UAnimMontage>> Montages;

    switch (AttackType)
    {
    case EAttackType::Normal:
        Montages = CachedPlayer->GetNormalAttackMontages();
        break;
    case EAttackType::Heavy:
        Montages = CachedPlayer->GetHeavyAttackMontages();
        break;
    default:
        break;
    }

    if (Montages.IsEmpty())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 安全保护：如果下标越界，重置为 0
    if (ComboIndex >= Montages.Num())
    {
        ComboIndex = 0;
    }

    UAnimMontage* MontageToPlay = Montages[ComboIndex].LoadSynchronous();
    if (!MontageToPlay)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

        // 连招核心重置区 (对应图3)
        bCanTriggerAttack = false;
    HitActors.Empty(); // 清空受击数组，新一刀重新判定
    ClearAllTasks();   // 清除上一刀残留的所有监听，防止事件乱窜！

        // 开始监听各种事件
    
    // 任务1：播放蒙太奇
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f);
    MontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->OnBlendOut.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->ReadyForActivation();

    // 任务2：监听伤害判定点 (对应图4)
    if (DamageDealEventTag.IsValid())
    {
        DamageTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DamageDealEventTag, nullptr, false, false);
        DamageTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnDamageEventReceived);
        DamageTask->ReadyForActivation();
    }

    // 任务3：监听连招窗口打开 (对应图7)
    if (ComboWindowOpenTag.IsValid())
    {
        ComboOpenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowOpenTag, nullptr, false, false);
        ComboOpenTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnComboOpenEventReceived);
        ComboOpenTask->ReadyForActivation();
    }

    // 任务4：监听连招窗口关闭 (对应图7)
    if (ComboWindowCloseTag.IsValid())
    {
        ComboCloseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowCloseTag, nullptr, false, false);
        ComboCloseTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnComboCloseEventReceived);
        ComboCloseTask->ReadyForActivation();
    }

    // 任务5：监听玩家下一次点击攻击键 (对应图9)
    if (NextAttackInputTag.IsValid())
    {
        InputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, NextAttackInputTag, nullptr, false, false);
        InputTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnNextAttackInputReceived);
        InputTask->ReadyForActivation();
    }
}

void UGA_MeleeAttackBase::AttackOrientation() {
    if (!CachedPlayer) return;

    FVector ActorLoc = CachedPlayer->GetActorLocation();

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);
    TArray<FHitResult> OutHits;

    // 1. 对应蓝图图1：以角色为中心，执行球体多重追踪
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        ActorLoc, ActorLoc, // Start 和 End 相同，即原地生成一个探测球
        OrientRadius,
        OrientObjectTypes,
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        OutHits,
        true
    );

    AActor* BestTarget = nullptr;
    float MinDistance = OrientRadius; // 初始化最小距离为最大探测半径

    // 2. 对应蓝图图2：遍历命中结果，寻找最近的敌人
    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();

        // 校验有效性及是否带有敌人标签 ("Enemy")
        if (HitActor && HitActor->ActorHasTag(EnemyActorTag))
        {
            float Distance = FVector::Dist(ActorLoc, HitActor->GetActorLocation());

            // 如果比当前记录的最小距离还近，则更新最优目标
            if (Distance < MinDistance)
            {
                MinDistance = Distance;
                BestTarget = HitActor;
            }
        }
    }

    // 3. 对应蓝图图3：如果找到了最优吸附目标，修改角色朝向
    if (BestTarget)
    {
        FRotator CurrentRot = CachedPlayer->GetActorRotation();

        // 获取玩家到敌人的朝向旋转
        FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(ActorLoc, BestTarget->GetActorLocation());

        // 【关键】：保留原本的 Pitch(Y) 和 Roll(X)，仅修改 Yaw(Z)
        FRotator NewRot = FRotator(CurrentRot.Pitch, LookAtRot.Yaw, CurrentRot.Roll);
        CachedPlayer->SetActorRotation(NewRot);
    }
}

void UGA_MeleeAttackBase::OnDamageEventReceived(FGameplayEventData Payload)
{
    ApplyDamageToTargets();
}

void UGA_MeleeAttackBase::ApplyDamageToTargets()
{
    if (!CachedPlayer) return;

    // LocalPredicted GA：伤害应用必须只在权威端（服务器）执行
    // 客户端调用 ApplyGameplayEffectSpecToTarget 无权修改目标属性，会被回滚
    if (!GetAvatarActorFromActorInfo()->HasAuthority()) return;

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!SourceASC || !DamageEffectClass) return;

    // 对应图5：计算起始和结束点
    FVector ActorLoc = CachedPlayer->GetActorLocation();
    FVector ForwardDir = CachedPlayer->GetActorForwardVector();
    FVector StartLoc = ActorLoc + ForwardDir * TraceForwardOffset1;
    FVector EndLoc = ActorLoc + ForwardDir * TraceForwardOffset2;

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);

    TArray<FHitResult> OutHits;

    // 执行球体追踪
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        StartLoc, EndLoc, TraceRadius,
        TraceObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    // 对应图6：排除重复受击并施加伤害
    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();

        if (HitActor && !HitActors.Contains(HitActor))
        {
            HitActors.Add(HitActor); // 加入排重名单

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            if (TargetASC)
            {
                // 严谨做法：把 HitResult 塞进 Context，方便伤害计算（Execution Calculation）
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

void UGA_MeleeAttackBase::OnComboOpenEventReceived(FGameplayEventData Payload)
{
    // 对应图8上部：更新连招索引
    bCanTriggerAttack = true;
    ComboIndex++;
}

void UGA_MeleeAttackBase::OnComboCloseEventReceived(FGameplayEventData Payload)
{
    // 对应图8下部：关闭连招窗口
    ComboIndex = 0;
    bCanTriggerAttack = false;
}

void UGA_MeleeAttackBase::OnNextAttackInputReceived(FGameplayEventData Payload)
{
    // 如果窗口是开着的，直接开启新一轮的 ExecuteAttack (形成闭环)
    if (bCanTriggerAttack)
    {
        ExecuteAttack();
    }
}

void UGA_MeleeAttackBase::OnMontageFinished()
{
    CorrectPawnOrient();

    bCanTriggerAttack = true;
    ComboIndex = 0;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 状态 Tag 由 ActivationOwnedTags 管理，GA 不再负责 GE 的移除

    // 被其他 GA Cancel 时，武器回到背上
    if (bWasCancelled && CachedPlayer)
    {
        if (UWeaponManagerComponent* WeaponComp = CachedPlayer->FindComponentByClass<UWeaponManagerComponent>())
        {
            WeaponComp->WeaponToBack();
        }
    }

    ClearAllTasks();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttackBase::ClearAllTasks()
{
    // GAS 极其重要的内存防泄漏操作：在重置或者退出时手动杀死所有监听任务
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
    if (DamageTask) { DamageTask->EndTask();  DamageTask = nullptr; }
    if (ComboOpenTask) { ComboOpenTask->EndTask(); ComboOpenTask = nullptr; }
    if (ComboCloseTask) { ComboCloseTask->EndTask(); ComboCloseTask = nullptr; }
    if (InputTask) { InputTask->EndTask();   InputTask = nullptr; }
}

void UGA_MeleeAttackBase::CorrectPawnOrient() {
    if (!CachedPlayer) return;
    FRotator CurrentRot = CachedPlayer->GetActorRotation();
    FRotator CorrectedRot = FRotator(0.0f, CurrentRot.Yaw, 0.0f);
    CachedPlayer->SetActorRotation(CorrectedRot);
}