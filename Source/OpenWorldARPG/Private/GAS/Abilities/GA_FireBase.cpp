// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_FireBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/PlayerCharacter.h"
#include "Components/CharacterWeaponComponent.h"
#include "Weapons/GunBase.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"

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

    // 1. 缓存角色 (告别 BP_PlayerCharacter 强转)
    CachedPlayer = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!CachedPlayer)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2. 缓存武器 (告别 BP_Gun 强转)
    if (UCharacterWeaponComponent* WeaponComp = CachedPlayer->FindComponentByClass<UCharacterWeaponComponent>())
    {
        CachedWeapon = WeaponComp->CharacterWeapon;
    }

    if (!CachedWeapon)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 3. 执行攻击
    ExecuteAttack();
}

void UGA_FireBase::ExecuteAttack()
{
    if (!CachedPlayer || !FireMontage)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 1. 对应蓝图图2：播放开火蒙太奇
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, FireMontage, MontagePlayRate, NAME_None, true, 1.0f, 0.0f);

    MontageTask->OnCompleted.AddDynamic(this, &UGA_FireBase::OnMontageFinished);
    MontageTask->OnBlendOut.AddDynamic(this, &UGA_FireBase::OnMontageFinished);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_FireBase::OnMontageFinished);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_FireBase::OnMontageFinished);

    // 激活任务
    MontageTask->ReadyForActivation();

    AGunBase* Gun = Cast<AGunBase>(CachedWeapon);
    if (Gun)
    {
        // 命令枪械播放自己的声光电表现
        Gun->PlayShootFX();
    }
    ApplyDamage();
}

void UGA_FireBase::ApplyDamage()
{
    if (!CachedPlayer || !CachedWeapon) return;

    // 强转为我们的枪械基类，获取准确的枪械独立数据
    AGunBase* Gun = Cast<AGunBase>(CachedWeapon);
    UCameraComponent* FollowCamera = CachedPlayer->GetFollowCamera();

    if (!Gun || !FollowCamera) return;

    // 向枪械动态索要射程和真实的枪口位置 (彻底摆脱硬编码与找组件的开销)
    float CurrentFireRange = Gun->GetFireRange();
    FVector MuzzleLoc = Gun->GetMuzzleTransform().GetLocation();

    FVector CameraLoc = FollowCamera->GetComponentLocation();
    FVector CameraForward = FollowCamera->GetForwardVector();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(CachedPlayer);
    Params.AddIgnoredActor(CachedWeapon);

    // ==========================================
    // 第一步：摄像机射线 (寻找准心在世界中的落点)
    // ==========================================
    FVector CamTraceEnd = CameraLoc + CameraForward * (CurrentFireRange + 500.0f);

    FHitResult CamHitResult;
    bool bCamHit = GetWorld()->LineTraceSingleByChannel(CamHitResult, CameraLoc, CamTraceEnd, TraceChannel, Params);

    // 目标落点：如果摄像机打到东西了，就是撞击点；如果没打到，就是射线的尽头
    FVector TargetAimPoint = bCamHit ? CamHitResult.ImpactPoint : CamTraceEnd;

    // ==========================================
    // 第二步：枪口射线 (真实弹道计算)
    // ==========================================
    // 计算从枪口指向准心落点的标准化方向向量
    FVector ShootDirection = (TargetAimPoint - MuzzleLoc).GetSafeNormal();

    // 根据动态射程，计算真实的枪口射线终点
    FVector MuzzleTraceEnd = MuzzleLoc + ShootDirection * CurrentFireRange;

    FHitResult MuzzleHitResult;
    bool bMuzzleHit = GetWorld()->LineTraceSingleByChannel(MuzzleHitResult, MuzzleLoc, MuzzleTraceEnd, TraceChannel, Params);

    // ==========================================
    // 第三步：结算伤害 (GAS 核心流程)
    // ==========================================
    if (bMuzzleHit)
    {
        AActor* HitActor = MuzzleHitResult.GetActor();
        if (HitActor)
        {
            // 通过 GAS 库安全地获取对方的 ASC
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

            if (TargetASC && SourceASC && DamageEffectClass)
            {
                FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
                Context.AddHitResult(MuzzleHitResult); // 传入真实的枪口 Hit 结果
                Context.AddSourceObject(this);

                FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
                SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
            }
        }
    }
}

void UGA_FireBase::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_FireBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}