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

	virtual void HandleDeath_Implementation() override;
	virtual void HandleRevive_Implementation() override;
	virtual UWeaponManagerComponent* GetWeaponManagerComponent_Implementation() const override { return nullptr; }

	UFUNCTION(BlueprintCallable, Category = "Character|Movement")
	void CorrectPawnOrient();

	/** 是否已经执行过死亡处理，防止重入 */
	bool bIsDead = false;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGenericTeamId TeamId;

	virtual FGenericTeamId GetGenericTeamId() const override;
};

