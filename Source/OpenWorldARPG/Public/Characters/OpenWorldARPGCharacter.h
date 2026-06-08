// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Types/CustomMovementModeTypes.h"
#include "OpenWorldARPGCharacter.generated.h"

UCLASS(config=Game)
class AOpenWorldARPGCharacter : public ACharacter, public IGenericTeamAgentInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	AOpenWorldARPGCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** ICombatInterface 默认实现：子类可重写 */
	virtual void HandleDeath_Implementation() override;

	/** 修正角色朝向，将 Pitch 和 Roll 归零（修复 Root Motion 造成的异常旋转） */
	UFUNCTION(BlueprintCallable, Category = "Character|Movement")
	void CorrectPawnOrient();

protected:
	// 声明一个用于存储队伍ID的属性
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGenericTeamId TeamId;

	// 声明将要重写的接口函数
	virtual FGenericTeamId GetGenericTeamId() const override;

};

