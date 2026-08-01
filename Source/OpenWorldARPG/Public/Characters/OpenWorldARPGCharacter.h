// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Systems/CombatSystem/Interfaces/CombatInterface.h"
#include "Systems/MovementSystem/Types/CustomMovementModeTypes.h"
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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- 接口实现 (IGenericTeamAgentInterface / ICombatInterface) ---

	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void HandleDeath_Implementation() override;
	virtual void HandleRevive_Implementation() override;
	virtual UWeaponManagerComponent* GetWeaponManagerComponent_Implementation() const override { return nullptr; }

	// --- 移动修正 ---

	void CorrectPawnOrient();

	// --- 死亡/复活的客户端视觉应用，子类可覆盖以追加表现 ---

	UFUNCTION()
	virtual void OnRep_IsDead(bool bOldIsDead);

public:
	// --- 状态 ---

	// 死亡状态由服务器权威设置并复制，客户端通过 OnRep 应用视觉（胶囊/移动等）
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "Character|State")
	bool bIsDead = false;

protected:
	// --- 死亡/复活视觉实现 ---

	virtual void ApplyDeathState();
	virtual void ApplyReviveState();

	// --- AI ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGenericTeamId TeamId;
};
