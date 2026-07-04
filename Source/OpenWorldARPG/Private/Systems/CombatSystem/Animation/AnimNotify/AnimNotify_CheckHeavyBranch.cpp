// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CombatSystem/Animation/AnimNotify/AnimNotify_CheckHeavyBranch.h"
#include "AbilitySystemBlueprintLibrary.h"

UAnimNotify_CheckHeavyBranch::UAnimNotify_CheckHeavyBranch()
{
}

void UAnimNotify_CheckHeavyBranch::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!CheckHeavyBranchEventTag.IsValid()) return;

    if (AActor* Owner = MeshComp->GetOwner())
    {
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, CheckHeavyBranchEventTag, FGameplayEventData());
    }
}

FString UAnimNotify_CheckHeavyBranch::GetNotifyName_Implementation() const
{
    return TEXT("Check Heavy Branch");
}
