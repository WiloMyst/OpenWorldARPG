// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_GlideBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/OpenWorldARPGCharacterMovementComponent.h"
#include "GameFramework/Character.h"

UGA_GlideBase::UGA_GlideBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_GlideBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) return;

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Character ? Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>() : nullptr;
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    if (!Character || !CustomMoveComp || !ASC)
    {
        UE_LOG(LogTemp, Error, TEXT("[GA_Glide] ActivateAbility FAILED: Character=%s, CustomMoveComp=%s, ASC=%s"),
            Character ? TEXT("Valid") : TEXT("NULL"),
            CustomMoveComp ? TEXT("Valid") : TEXT("NULL"),
            ASC ? TEXT("Valid") : TEXT("NULL"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[GA_Glide] ActivateAbility: IsGliding=%d, MovementMode=%d"),
        CustomMoveComp->IsGliding(), (int32)CustomMoveComp->MovementMode.GetValue());

    // ==========================================
    // CMC 职责：物理参数修改 + Launch + 运动模式切换 + Tag 管理
    // GA 只发送"进入滑翔"的意愿，不传递任何物理参数
    // ==========================================
    CustomMoveComp->EnterGlideMode();

    // ==========================================
    // GA 职责：意愿和表现
    // ==========================================

    // 1. 生成滑翔伞 (表现层：视觉外观)
    if (GliderActorClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnedGlider = GetWorld()->SpawnActor<AActor>(GliderActorClass, Character->GetActorTransform(), SpawnParams);
        if (SpawnedGlider)
        {
            SpawnedGlider->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, GliderSocketName);
        }
    }

    // 2. 应用状态 GE (意愿层：Tag 状态标记)
    if (GlideStateEffectClass)
    {
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GlideStateEffectClass, 1.0f, ASC->MakeEffectContext());
        if (SpecHandle.IsValid()) ActiveGlideEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }

    // 3. 发送空中状态变更事件 (意愿层：通知 FSM/其他系统)
    if (AirStateChangedEventTag.IsValid() && Character)
    {
        FGameplayEventData Payload;
        Payload.Instigator = Character;
        Payload.Target = Character;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, AirStateChangedEventTag, Payload);
    }

    // 4. 监听停止事件 (意愿层：等待玩家输入或系统取消)
    WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StopGlideEventTag, nullptr, false, true);
    if (WaitEventTask)
    {
        WaitEventTask->EventReceived.AddDynamic(this, &UGA_GlideBase::OnStopGlideEventReceived);
        WaitEventTask->ReadyForActivation();
    }
}

void UGA_GlideBase::OnStopGlideEventReceived(FGameplayEventData Payload)
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Glide] OnStopGlideEventReceived, ending ability"));
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_GlideBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Glide] EndAbility called, bWasCancelled=%d"), bWasCancelled);

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    UOpenWorldARPGCharacterMovementComponent* CustomMoveComp = Character ? Character->FindComponentByClass<UOpenWorldARPGCharacterMovementComponent>() : nullptr;
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // ==========================================
    // CMC 职责：恢复物理参数 + 运动模式切换 + Tag 清理
    // ==========================================
    if (CustomMoveComp)
    {
        if (CustomMoveComp->IsGliding())
        {
            UE_LOG(LogTemp, Warning, TEXT("[GA_Glide] EndAbility: calling ExitGlideMode"));
            CustomMoveComp->ExitGlideMode();
        }
        else
        {
            // 引擎可能已自动切换模式（如着地），OnMovementModeChanged 已恢复物理参数
            // 但如果 OnMovementModeChanged 未被调用（极端情况），这里做兜底恢复
            UE_LOG(LogTemp, Warning, TEXT("[GA_Glide] EndAbility: NOT gliding, physics should be restored by OnMovementModeChanged"));
        }
    }

    // ==========================================
    // GA 职责：清理意愿和表现
    // ==========================================

    // 1. 销毁滑翔伞 (表现层)
    if (SpawnedGlider)
    {
        SpawnedGlider->Destroy();
        SpawnedGlider = nullptr;
    }

    // 2. 移除状态 GE (意愿层)
    if (ASC && ActiveGlideEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(ActiveGlideEffectHandle);
        ActiveGlideEffectHandle.Invalidate();
    }

    // 3. 发送空中状态变更事件 (意愿层：通知 FSM/其他系统)
    if (AirStateChangedEventTag.IsValid() && Character)
    {
        FGameplayEventData Payload;
        Payload.Instigator = Character;
        Payload.Target = Character;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, AirStateChangedEventTag, Payload);
    }

    // 4. 停止异步等待任务
    if (WaitEventTask)
    {
        WaitEventTask->EndTask();
        WaitEventTask = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
