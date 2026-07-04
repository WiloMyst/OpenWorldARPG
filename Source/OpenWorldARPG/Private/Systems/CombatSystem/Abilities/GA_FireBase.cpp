// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CombatSystem/Abilities/GA_FireBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Systems/CombatSystem/Components/WeaponManagerComponent.h"
#include "Systems/CombatSystem/Weapons/GunBase.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

UGA_FireBase::UGA_FireBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_FireBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 缓存角色与武器
    CachedPlayer = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!CachedPlayer)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (UWeaponManagerComponent* WeaponComp = CachedPlayer->FindComponentByClass<UWeaponManagerComponent>())
    {
        CachedWeapon = WeaponComp->CharacterWeapon;
    }

    if (!CachedWeapon)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2. 开启"停止事件"的异步监听
    // 等待玩家松开按键 (控制器在 Completed 时发送该 Tag)
    if (StopFireEventTag.IsValid())
    {
        StopEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopFireEventTag, nullptr, false, false);
        StopEventTask->EventReceived.AddDynamic(this, &UGA_FireBase::OnStopFireEventReceived);
        StopEventTask->ReadyForActivation();
    }

    // 3. 立即执行第一发子弹射击
    PerformSingleShot();

    // 4. 如果是全自动武器 (FireRate > 0)，开启连发定时器
    if (FireRate > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UGA_FireBase::PerformSingleShot, FireRate, true);
    }
    else
    {
        // 如果是半自动单发武器 (FireRate <= 0)，射完一发直接结束，或者等待动画结束
        // 这里可以直接结束，因为动画是由原生 API 播放的
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_FireBase::PerformSingleShot()
{
    if (!CachedPlayer) return;

    // 播放蒙太奇：
    // 在超高射速(如0.1s一发)的情况下，如果用 AbilityTask_PlayMontageAndWait 会瞬间产生大量 Task 导致卡顿和泄漏。
    // 大厂对于这种高频循环动作，通常直接调用角色原生的 PlayAnimMontage (Fire&Forget 模式)。
    if (FireMontage)
    {
        CachedPlayer->PlayAnimMontage(FireMontage, MontagePlayRate);
    }

    AGunBase* Gun = Cast<AGunBase>(CachedWeapon);
    if (Gun)
    {
        // 枪口火光、抛壳、开火音效
        Gun->PlayShootFX();
    }

    // 结算射线判定与伤害
    ApplyDamage();
}

void UGA_FireBase::ApplyDamage()
{
    if (!CachedPlayer || !CachedWeapon) return;

    // LocalPredicted GA：伤害应用必须只在权威端（服务器）执行
    if (!GetAvatarActorFromActorInfo()->HasAuthority()) return;

    AGunBase* Gun = Cast<AGunBase>(CachedWeapon);
    UCameraComponent* FollowCamera = CachedPlayer->GetFollowCamera();

    if (!Gun || !FollowCamera) return;

    float CurrentFireRange = Gun->GetFireRange();
    FVector MuzzleLoc = Gun->GetMuzzleTransform().GetLocation();

    FVector CameraLoc = FollowCamera->GetComponentLocation();
    FVector CameraForward = FollowCamera->GetForwardVector();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(CachedPlayer);
    Params.AddIgnoredActor(CachedWeapon);

    // 第一步：摄像机射线 (寻找准心在世界中的落点)
    FVector CamTraceEnd = CameraLoc + CameraForward * (CurrentFireRange + 500.0f);

    FHitResult CamHitResult;
    bool bCamHit = GetWorld()->LineTraceSingleByChannel(CamHitResult, CameraLoc, CamTraceEnd, TraceChannel, Params);

    // 目标落点：如果摄像机打到东西了，就是撞击点；如果没打到，就是射线的尽头
    FVector TargetAimPoint = bCamHit ? CamHitResult.ImpactPoint : CamTraceEnd;

    // 第二步：枪口射线 (真实弹道计算)
    FVector ShootDirection = (TargetAimPoint - MuzzleLoc).GetSafeNormal();
    FVector MuzzleTraceEnd = MuzzleLoc + ShootDirection * CurrentFireRange;

    FHitResult MuzzleHitResult;
    bool bMuzzleHit = GetWorld()->LineTraceSingleByChannel(MuzzleHitResult, MuzzleLoc, MuzzleTraceEnd, TraceChannel, Params);

    // 第三步：结算伤害 (GAS 核心流程)
    if (bMuzzleHit)
    {
        AActor* HitActor = MuzzleHitResult.GetActor();
        if (HitActor)
        {
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

            if (TargetASC && SourceASC && DamageEffectClass)
            {
                FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
                Context.AddHitResult(MuzzleHitResult);
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

void UGA_FireBase::OnStopFireEventReceived(FGameplayEventData Payload)
{
    // 收到控制器发来的“松开鼠标”事件，结束开火
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_FireBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 清理连发定时器，停止自动射击
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FireTimerHandle);
    }

    // 清理异步监听任务
    if (StopEventTask)
    {
        StopEventTask->EndTask();
        StopEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}