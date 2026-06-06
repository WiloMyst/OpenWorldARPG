// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Types/CustomMovementModeTypes.h"
#include "OpenWorldARPGCharacter.generated.h"

UCLASS(config=Game)
class AOpenWorldARPGCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AOpenWorldARPGCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// 声明一个用于存储队伍ID的属性
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FGenericTeamId TeamId;

	// 声明将要重写的接口函数
	virtual FGenericTeamId GetGenericTeamId() const override;

};

