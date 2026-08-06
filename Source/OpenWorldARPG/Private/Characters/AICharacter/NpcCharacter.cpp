// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Characters/AICharacter/NpcCharacter.h"
#include "Systems/AvatarSystem/AvatarStreamingComponent.h"

#include "Components/WidgetComponent.h"

ANpcCharacter::ANpcCharacter()
{
    AvatarStreamingComponent = CreateDefaultSubobject<UAvatarStreamingComponent>(TEXT("AvatarStreamingComponent"));
}

void ANpcCharacter::BeginPlay()
{
    Super::BeginPlay();
}

bool ANpcCharacter::CanInteract_Implementation(ACharacter* InstigatorCharacter) const
{
    return true;
}

void ANpcCharacter::OnInteract_Implementation(ACharacter* InstigatorCharacter)
{
    // 交互由 AvatarStreamingComponent 的 gRPC 流程接管
}

FTransform ANpcCharacter::GetInteractionTargetTransform_Implementation() const
{
    return GetActorTransform();
}
