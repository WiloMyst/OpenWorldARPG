// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Characters/AICharacter/NpcCharacter.h"
#include "Systems/DialogueSystem/Components/DialogueComponent.h"
#include "Systems/AvatarSystem/AvatarStreamingComponent.h"

#include "Components/WidgetComponent.h"

ANpcCharacter::ANpcCharacter()
{
    // 创建对话组件
    DialogueComponent = CreateDefaultSubobject<UDialogueComponent>(TEXT("DialogueComponent"));

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
    // 交互时通过对话组件启动对话
    if (DialogueComponent)
    {
        DialogueComponent->StartDialogue(InstigatorCharacter);
    }
}

FTransform ANpcCharacter::GetInteractionTargetTransform_Implementation() const
{
    return GetActorTransform();
}
