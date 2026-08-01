// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Systems/AbilitySystem/AttributeSets/AS_Enemy.h"
#include "Net/UnrealNetwork.h"

UAS_Enemy::UAS_Enemy()
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
}

void UAS_Enemy::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // 如果修改的是Health，确保它在 [0, MaxHealth] 之间
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
}

void UAS_Enemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, Health);
    DOREPLIFETIME(ThisClass, MaxHealth);
}

void UAS_Enemy::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Enemy, Health, OldHealth);
}

void UAS_Enemy::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Enemy, MaxHealth, OldMaxHealth);
}
