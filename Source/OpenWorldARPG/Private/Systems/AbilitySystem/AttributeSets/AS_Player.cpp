// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Systems/AbilitySystem/AttributeSets/AS_Player.h"
#include "Net/UnrealNetwork.h"

UAS_Player::UAS_Player()
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
    InitStamina(100.0f);
    InitMaxStamina(100.0f);
    InitFlyStamina(100.0f);
    InitMaxFlyStamina(100.0f);
}

void UAS_Player::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // 如果修改的是Health，确保它在 [0, MaxHealth] 之间
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
    // 如果修改的是Stamina，确保它在 [0, MaxStamina] 之间
    if (Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
    }
    // 如果修改的是FlyStamina，确保它在 [0, MaxFlyStamina] 之间
    if (Attribute == GetFlyStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxFlyStamina());
    }
}

void UAS_Player::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, Health);
    DOREPLIFETIME(ThisClass, MaxHealth);
    DOREPLIFETIME(ThisClass, Stamina);
    DOREPLIFETIME(ThisClass, MaxStamina);
    DOREPLIFETIME(ThisClass, FlyStamina);
    DOREPLIFETIME(ThisClass, MaxFlyStamina);
}

void UAS_Player::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Player, Health, OldHealth);
}

void UAS_Player::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Player, MaxHealth, OldMaxHealth);
}

void UAS_Player::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Player, Stamina, OldStamina);
}

void UAS_Player::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Player, MaxStamina, OldMaxStamina);
}

void UAS_Player::OnRep_FlyStamina(const FGameplayAttributeData& OldFlyStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Player, FlyStamina, OldFlyStamina);
}

void UAS_Player::OnRep_MaxFlyStamina(const FGameplayAttributeData& OldMaxFlyStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_Player, MaxFlyStamina, OldMaxFlyStamina);
}
