// Copyright 2025 WiloMyst. All Rights Reserved.


#include "GAS/AttributeSets/AS_Player.h"

UAS_Player::UAS_Player()
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
    InitStamina(100.0f);
    InitMaxStamina(100.0f);
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
}
