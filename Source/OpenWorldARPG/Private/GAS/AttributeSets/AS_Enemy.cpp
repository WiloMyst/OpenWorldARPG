// Copyright 2025 WiloMyst. All Rights Reserved.


#include "GAS/AttributeSets/AS_Enemy.h"

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
