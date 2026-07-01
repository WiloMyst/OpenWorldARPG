// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Types/CustomMovementModeTypes.h"
#include "OpenWorldARPGCharacter.generated.h"

/**
 * 角色基类。提供队伍 ID 和死亡处理等通用逻辑。
 */
UCLASS(config=Game)
class AOpenWorldARPGCharacter : public ACharacter, public IGenericTeamAgentInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	AOpenWorldARPGCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// --- 接口实现 (IGenericTeamAgentInterface / ICombatInterface) ---

	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void HandleDeath_Implementation() override;
	virtual void HandleRevive_Implementation() override;
	virtual UWeaponManagerComponent* GetWeaponManagerComponent_Implementation() const override { return nullptr; }

	// --- 移动修正 ---

	void CorrectPawnOrient();

public:
	// --- 状态 ---

	bool bIsDead = false;

protected:
	// --- AI ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGenericTeamId TeamId;
};
