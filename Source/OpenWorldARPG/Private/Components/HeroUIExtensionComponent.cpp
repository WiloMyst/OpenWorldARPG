// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/HeroUIExtensionComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/InteractionComponent.h"
#include "GAS/AttributeSets/AS_Player.h"

UHeroUIExtensionComponent::UHeroUIExtensionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHeroUIExtensionComponent::BindToActor(AActor* InOwner)
{
    if (!InOwner) return;

    // 先解绑旧委托
    UnbindAll();

    // 绑定 ASC
    if (UAbilitySystemComponent* ASC = InOwner->FindComponentByClass<UAbilitySystemComponent>())
    {
        CachedASC = ASC;

        // 血量属性
        HealthAttrHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAS_Player::GetHealthAttribute())
            .AddUObject(this, &UHeroUIExtensionComponent::OnHealthAttributeChanged);

        MaxHealthAttrHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAS_Player::GetMaxHealthAttribute())
            .AddUObject(this, &UHeroUIExtensionComponent::OnMaxHealthAttributeChanged);

        // 瞄准 Tag
        if (AimingStateTag.IsValid())
        {
            AimingTagHandle = ASC->RegisterGameplayTagEvent(AimingStateTag, EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UHeroUIExtensionComponent::OnAimingTagChanged);
        }

        // 初始化当前血量
        CurrentHealth = ASC->GetNumericAttribute(UAS_Player::GetHealthAttribute());
        CurrentMaxHealth = ASC->GetNumericAttribute(UAS_Player::GetMaxHealthAttribute());
        bIsAiming = ASC->HasMatchingGameplayTag(AimingStateTag);

        // 首次广播
        UpdateHealthBroadcast();
        OnAimingStateChanged.Broadcast(bIsAiming);
    }

    // 绑定交互组件
    if (UInteractionComponent* InteractComp = InOwner->FindComponentByClass<UInteractionComponent>())
    {
        CachedInteractionComp = InteractComp;
        InteractComp->OnPickableListChangedDelegate.AddDynamic(this, &UHeroUIExtensionComponent::HandleInteractionListChanged);
    }
}

void UHeroUIExtensionComponent::UnbindAll()
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
        CachedInteractionComp->OnPickableListChangedDelegate.RemoveAll(this);
        CachedInteractionComp = nullptr;
    }
}

void UHeroUIExtensionComponent::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    CurrentHealth = Data.NewValue;
    UpdateHealthBroadcast();
}

void UHeroUIExtensionComponent::OnMaxHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    CurrentMaxHealth = Data.NewValue;
    UpdateHealthBroadcast();
}

void UHeroUIExtensionComponent::OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    bIsAiming = (NewCount > 0);
    OnAimingStateChanged.Broadcast(bIsAiming);
}

void UHeroUIExtensionComponent::HandleInteractionListChanged(const TArray<AActor*>& InteractableActors)
{
    CachedInteractableActors = InteractableActors;
    OnInteractionListChanged.Broadcast(InteractableActors);
}

void UHeroUIExtensionComponent::UpdateHealthBroadcast()
{
    OnHealthChanged.Broadcast(CurrentHealth, CurrentMaxHealth);
}
