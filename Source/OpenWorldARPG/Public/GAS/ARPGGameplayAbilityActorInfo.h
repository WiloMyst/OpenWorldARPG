// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ARPGGameplayAbilityActorInfo.generated.h"

class UOpenWorldARPGCharacterMovementComponent;

/**
 * 自定义 GameplayAbilityActorInfo，缓存 CMC 指针。
 * GA 高频访问 CMC，通过 ActorInfo 缓存实现 O(1) 访问，替代 FindComponentByClass。
 */
USTRUCT()
struct FARPGGameplayAbilityActorInfo : public FGameplayAbilityActorInfo
{
    GENERATED_USTRUCT_BODY()

    FARPGGameplayAbilityActorInfo()
        : Super()
        , CustomMovementComponent(nullptr)
    {}

    UPROPERTY()
    TObjectPtr<UOpenWorldARPGCharacterMovementComponent> CustomMovementComponent;

    virtual void InitFromActor(AActor* InOwnerActor, AActor* InAvatarActor, UAbilitySystemComponent* InAbilitySystemComponent) override;
};
