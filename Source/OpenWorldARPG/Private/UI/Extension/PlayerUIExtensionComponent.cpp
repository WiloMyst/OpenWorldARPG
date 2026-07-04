// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Extension/PlayerUIExtensionComponent.h"
#include "AbilitySystemComponent.h"
#include "Systems/InteractionSystem/Components/InteractionComponent.h"
#include "Systems/AbilitySystem/AttributeSets/AS_Player.h"

UPlayerUIExtensionComponent::UPlayerUIExtensionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerUIExtensionComponent::BindToActor(AActor* InOwner)
{
    if (!InOwner) return;

    UnbindAll();

    if (UAbilitySystemComponent* ASC = InOwner->FindComponentByClass<UAbilitySystemComponent>())
    {
        CachedASC = ASC;

        HealthAttrHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAS_Player::GetHealthAttribute())
            .AddUObject(this, &UPlayerUIExtensionComponent::OnHealthAttributeChanged);

        MaxHealthAttrHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAS_Player::GetMaxHealthAttribute())
            .AddUObject(this, &UPlayerUIExtensionComponent::OnMaxHealthAttributeChanged);

        if (AimingStateTag.IsValid())
        {
            AimingTagHandle = ASC->RegisterGameplayTagEvent(AimingStateTag, EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UPlayerUIExtensionComponent::OnAimingTagChanged);
        }

        CurrentHealth = ASC->GetNumericAttribute(UAS_Player::GetHealthAttribute());
        CurrentMaxHealth = ASC->GetNumericAttribute(UAS_Player::GetMaxHealthAttribute());
        bIsAiming = ASC->HasMatchingGameplayTag(AimingStateTag);

        UpdateHealthBroadcast();
        OnAimingStateChanged.Broadcast(bIsAiming);
    }

    if (UInteractionComponent* InteractComp = InOwner->FindComponentByClass<UInteractionComponent>())
    {
        CachedInteractionComp = InteractComp;
        InteractComp->OnInteractableListChangedDelegate.AddDynamic(this, &UPlayerUIExtensionComponent::HandleInteractionListChanged);
    }
}

void UPlayerUIExtensionComponent::UnbindAll()
{
    if (CachedASC.IsValid())
    {
        if (HealthAttrHandle.IsValid())
        {
            CachedASC->GetGameplayAttributeValueChangeDelegate(UAS_Player::GetHealthAttribute()).Remove(HealthAttrHandle);
            HealthAttrHandle.Reset();
        }
        if (MaxHealthAttrHandle.IsValid())
        {
            CachedASC->GetGameplayAttributeValueChangeDelegate(UAS_Player::GetMaxHealthAttribute()).Remove(MaxHealthAttrHandle);
            MaxHealthAttrHandle.Reset();
        }
        if (AimingTagHandle.IsValid() && AimingStateTag.IsValid())
        {
            CachedASC->UnregisterGameplayTagEvent(AimingTagHandle, AimingStateTag, EGameplayTagEventType::NewOrRemoved);
            AimingTagHandle.Reset();
        }
        CachedASC = nullptr;
    }

    if (CachedInteractionComp.IsValid())
    {
        CachedInteractionComp->OnInteractableListChangedDelegate.RemoveAll(this);
        CachedInteractionComp = nullptr;
    }
}

void UPlayerUIExtensionComponent::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    CurrentHealth = Data.NewValue;
    UpdateHealthBroadcast();
}

void UPlayerUIExtensionComponent::OnMaxHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    CurrentMaxHealth = Data.NewValue;
    UpdateHealthBroadcast();
}

void UPlayerUIExtensionComponent::OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    bIsAiming = (NewCount > 0);
    OnAimingStateChanged.Broadcast(bIsAiming);
}

void UPlayerUIExtensionComponent::HandleInteractionListChanged(const TArray<AActor*>& InteractableActors)
{
    CachedInteractableActors = InteractableActors;
    OnInteractionListChanged.Broadcast(InteractableActors);
}

void UPlayerUIExtensionComponent::UpdateHealthBroadcast()
{
    OnHealthChanged.Broadcast(CurrentHealth, CurrentMaxHealth);
}
