// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_SwimBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GAS/ARPGGameplayAbilityActorInfo.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "GameFramework/Character.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"

UGA_SwimBase::UGA_SwimBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 游泳 Tag 由 CMC 在 OnMovementModeChanged 中注入，
    // GA 的 ActivationOwnedTags 不需要再重复添加
}

void UGA_SwimBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    if (!ASC || !CustomMoveComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================
    // 1. CMC 职责：进入游泳物理模式
    // GA 只发送"进入游泳"的意愿，不传递物理参数
    // ==========================================
    CustomMoveComp->EnterSwimMode();

    // ==========================================
    // 2. GA 职责：应用普通游泳体力持续扣除 GE
    // ==========================================
    if (SwimStaminaDrainGE && ASC)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SwimStaminaDrainGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            StaminaDrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // ==========================================
    // 3. 监听事件
    // ==========================================

    // 监听停止游泳事件（离开水体时 CMC 发送）
    if (StopSwimEventTag.IsValid())
    {
        WaitStopEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopSwimEventTag, nullptr, false, true);
        if (WaitStopEventTask)
        {
            WaitStopEventTask->EventReceived.AddDynamic(this, &UGA_SwimBase::OnStopSwimEventReceived);
            WaitStopEventTask->ReadyForActivation();
        }
    }

    // 监听快速游泳开始事件（Shift 按下时由 Controller 发送）
    if (FastSwimStartEventTag.IsValid())
    {
        WaitFastSwimStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FastSwimStartEventTag, nullptr, false, true);
        if (WaitFastSwimStartTask)
        {
            WaitFastSwimStartTask->EventReceived.AddDynamic(this, &UGA_SwimBase::OnFastSwimStartEventReceived);
            WaitFastSwimStartTask->ReadyForActivation();
        }
    }

    // 监听快速游泳结束事件（Shift 释放时由 Controller 发送）
    if (FastSwimStopEventTag.IsValid())
    {
        WaitFastSwimStopTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FastSwimStopEventTag, nullptr, false, true);
        if (WaitFastSwimStopTask)
        {
            WaitFastSwimStopTask->EventReceived.AddDynamic(this, &UGA_SwimBase::OnFastSwimStopEventReceived);
            WaitFastSwimStopTask->ReadyForActivation();
        }
    }

    // ==========================================
    // 4. 启动定时检测：体力耗尽、是否仍在水中
    // ==========================================
    if (ConditionCheckInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(ConditionCheckTimer, this, &UGA_SwimBase::CheckSwimConditions, ConditionCheckInterval, true);
    }
}

void UGA_SwimBase::OnStopSwimEventReceived(FGameplayEventData Payload)
{
    // 离开水体，结束游泳
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SwimBase::OnFastSwimStartEventReceived(FGameplayEventData Payload)
{
    if (bIsFastSwimming || bHasDrowned) return;

    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(CurrentActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!CustomMoveComp || !ASC) return;

    // 检查体力是否足够进入快速游泳
    const float CurrentStamina = ASC->GetNumericAttribute(UAS_Player::GetStaminaAttribute());
    if (CurrentStamina <= DrowningStaminaThreshold) return;

    bIsFastSwimming = true;

    // CMC 职责：切换快速游泳物理参数
    CustomMoveComp->EnterFastSwimMode();

    // GA 职责：替换为消耗更大的体力扣除 GE
    if (StaminaDrainGEHandle.IsValid() && ASC)
    {
        ASC->RemoveActiveGameplayEffect(StaminaDrainGEHandle);
        StaminaDrainGEHandle = FActiveGameplayEffectHandle();
    }

    if (FastSwimStaminaDrainGE && ASC)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(FastSwimStaminaDrainGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            StaminaDrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void UGA_SwimBase::OnFastSwimStopEventReceived(FGameplayEventData Payload)
{
    if (!bIsFastSwimming || bHasDrowned) return;

    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(CurrentActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!CustomMoveComp || !ASC) return;

    bIsFastSwimming = false;

    // CMC 职责：恢复普通游泳物理参数
    CustomMoveComp->ExitFastSwimMode();

    // GA 职责：替换回普通游泳体力扣除 GE
    if (StaminaDrainGEHandle.IsValid() && ASC)
    {
        ASC->RemoveActiveGameplayEffect(StaminaDrainGEHandle);
        StaminaDrainGEHandle = FActiveGameplayEffectHandle();
    }

    if (SwimStaminaDrainGE && ASC)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SwimStaminaDrainGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            StaminaDrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void UGA_SwimBase::CheckSwimConditions()
{
    if (bHasDrowned) return;

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return;

    // 检测体力是否耗尽
    const float CurrentStamina = ASC->GetNumericAttribute(UAS_Player::GetStaminaAttribute());
    if (CurrentStamina <= DrowningStaminaThreshold)
    {
        HandleDrowning();
    }
}

// ==========================================
// 溺水处理逻辑
// 体力归零时的惩罚机制：
// 1. 发送溺水事件 Tag（触发溺水动画/特效）
// 2. 应用溺水伤害 GE（扣除一定比例最大生命值）
// 3. 强制退出快速游泳
// 4. 将角色 Teleport 到 CMC 记录的 LastSafeLocation
// 5. 结束游泳技能
// ==========================================
void UGA_SwimBase::HandleDrowning()
{
    if (bHasDrowned) return;
    bHasDrowned = true;

    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(CurrentActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;

    if (!Character || !ASC) return;

    // 1. 发送溺水事件 Tag（用于触发溺水动画/特效/音效）
    if (DrowningEventTag.IsValid())
    {
        FGameplayEventData Payload;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, DrowningEventTag, Payload);
    }

    // 2. 应用溺水伤害 GE（扣除一定比例最大生命值）
    if (DrowningDamageGE && ASC)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DrowningDamageGE, GetAbilityLevel(), ASC->MakeEffectContext());
        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // 3. 强制退出快速游泳
    if (bIsFastSwimming && CustomMoveComp)
    {
        CustomMoveComp->ExitFastSwimMode();
        bIsFastSwimming = false;
    }

    // 4. 传送到最后安全位置
    if (CustomMoveComp)
    {
        const FVector& SafeLocation = CustomMoveComp->GetLastSafeLocation();
        if (!SafeLocation.IsNearlyZero())
        {
            Character->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
        }
    }

    // 5. 结束游泳技能（会触发 EndAbility 清理）
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SwimBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    const FARPGGameplayAbilityActorInfo* ARPGActorInfo = StaticCast<const FARPGGameplayAbilityActorInfo*>(ActorInfo);
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = ARPGActorInfo ? ARPGActorInfo->CustomMovementComponent : nullptr;
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // CMC 职责：退出游泳物理模式
    if (CustomMoveComp && CustomMoveComp->IsSwimming())
    {
        CustomMoveComp->ExitSwimMode();
    }

    // GA 职责：清理意愿和表现

    // 移除体力扣除 GE
    if (ASC && StaminaDrainGEHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(StaminaDrainGEHandle);
        StaminaDrainGEHandle = FActiveGameplayEffectHandle();
    }

    // 停止状态检测定时器
    GetWorld()->GetTimerManager().ClearTimer(ConditionCheckTimer);

    // 停止异步等待任务
    if (WaitStopEventTask)
    {
        WaitStopEventTask->EndTask();
        WaitStopEventTask = nullptr;
    }
    if (WaitFastSwimStartTask)
    {
        WaitFastSwimStartTask->EndTask();
        WaitFastSwimStartTask = nullptr;
    }
    if (WaitFastSwimStopTask)
    {
        WaitFastSwimStopTask->EndTask();
        WaitFastSwimStopTask = nullptr;
    }

    // 重置状态
    bIsFastSwimming = false;
    bHasDrowned = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
