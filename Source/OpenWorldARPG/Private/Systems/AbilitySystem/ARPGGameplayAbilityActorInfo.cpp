// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AbilitySystem/ARPGGameplayAbilityActorInfo.h"
#include "Systems/MovementSystem/Components/PlayerCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"

void FARPGGameplayAbilityActorInfo::InitFromActor(AActor* InOwnerActor, AActor* InAvatarActor, UAbilitySystemComponent* InAbilitySystemComponent)
{
    Super::InitFromActor(InOwnerActor, InAvatarActor, InAbilitySystemComponent);

    // 在初始化时一次性缓存 CMC 指针，后续 GA 直接 O(1) 读取
    if (ACharacter* Character = Cast<ACharacter>(InAvatarActor))
    {
        // 极致优化：直接获取 Character 原生缓存的 MovementComponent 并强转，绝对的 O(1)
        CustomMovementComponent = Cast<UPlayerCharacterMovementComponent>(Character->GetCharacterMovement());
    }
    else
    {
        CustomMovementComponent = nullptr;
    }
}
