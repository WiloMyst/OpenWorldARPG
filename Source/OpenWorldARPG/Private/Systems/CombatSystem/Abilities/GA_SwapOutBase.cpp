// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CombatSystem/Abilities/GA_SwapOutBase.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"

UGA_SwapOutBase::UGA_SwapOutBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 在蓝图子类中配置 ActivationOwnedTags / BlockAbilitiesWithTag / CancelAbilitiesWithTag
    // 例如：BlockAbilitiesWithTag 包含 State.Dead、State.KnockedUp 等
}

void UGA_SwapOutBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // 验证：已死亡的角色必须允许退场，否则检查移动模式
    if (!CachedPlayer->bIsDead && !ValidateSwapOutConditions())
    {
        // 验证失败，取消退场
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 保存当前 Transform 供 GA_SwapInBase 使用
    SaveSwapTransform();

    // 2. 播放退场视觉表现
    PlaySwapOutVisuals();

    // 3. 进入待机休眠状态
    EnterStandbyMode();

    // 4. 退场是瞬时技能，立即结束
    // EndAbility 会触发 PlayerCharacter 上的委托，通知 Controller 执行 Possess
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_SwapOutBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 仅在权威端（服务器）且非取消时，通过角色委托通知 Controller
    // 客户端的 GA 预测执行不应驱动 Possess
    if (!bWasCancelled && CachedPlayer && CachedPlayer->HasAuthority())
    {
        CachedPlayer->NotifySwapOutCompleted(SavedSwapTransform);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_SwapOutBase::ValidateSwapOutConditions() const
{
    // 移动模式拦截：只允许在指定运动模式下切换下场
    if (!AllowedSwapOutMovementModes.IsEmpty())
    {
        if (const UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement())
        {
            if (!AllowedSwapOutMovementModes.Contains(TEnumAsByte<EMovementMode>(MoveComp->MovementMode)))
            {
                return false;
            }
        }
    }

    return true;
}

void UGA_SwapOutBase::SaveSwapTransform()
{
    if (CachedPlayer)
    {
        SavedSwapTransform = CachedPlayer->GetActorTransform();
    }
}

void UGA_SwapOutBase::PlaySwapOutVisuals()
{
    if (!CachedPlayer) return;

    // 在角色位置生成退场特效
    if (SwapOutFX)
    {
        FVector SpawnLocation = CachedPlayer->GetActorLocation() + SwapFXLocationOffset;
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            SwapOutFX,
            SpawnLocation,
            FRotator::ZeroRotator,
            SwapFXScale
        );
    }
}

void UGA_SwapOutBase::EnterStandbyMode()
{
    if (CachedPlayer)
    {
        CachedPlayer->SetStandbyMode(true);
    }
}
