// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Characters/AI/EnemyCharacter.h"
#include "GAS/AttributeSets/AS_Enemy.h"

AEnemyCharacter::AEnemyCharacter()
{
	// 创建能力系统组件(ASC)。
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 设置ASC的复制模式
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed); // 或 Minimal

	// 创建属性集(AttributeSet)。
	AttributeSet = CreateDefaultSubobject<UAS_Enemy>(TEXT("AttributeSet"));
}
