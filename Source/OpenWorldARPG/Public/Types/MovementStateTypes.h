// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementStateTypes.generated.h"

UENUM(BlueprintType)
enum class ECornerType : uint8
{
	None = 0,
	Convex,  // 阳角 (外拐角)
	Concave  // 阴角 (内拐角)
};

USTRUCT(BlueprintType)
struct FFallingStateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling")
	float AirControl = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling")
	float RotationInterpSpeed = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|ClimbTransition")
	float ClimbInputDotThreshold = -0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|ClimbTransition")
	float WallNormalMaxVerticalAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling|ClimbTransition")
	float ClimbTraceDistance = 80.0f;
};

USTRUCT(BlueprintType)
struct FClimbingStateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float ClimbMoveSpeed = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float RotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float TargetWallDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float GravityScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|GroundTransition")
	float GroundDetectDistance = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Movement")
	float PredictOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Trace")
	float SpaceCheckRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Trace")
	float SpaceCheckHalfHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing|Logic")
	FName UnclimbableActorTag = FName("NotClimbable");
};

USTRUCT(BlueprintType)
struct FCornerTransitionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Detection")
	float SideTraceDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Detection")
	float ForwardTraceDistance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Convex")
	float ConvexArcRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Convex")
	float ConvexPositionInterpSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Convex")
	float ConvexRotationInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Concave")
	float ConcavePositionInterpSpeed = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Concave")
	float ConcaveRotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|Concave")
	float ConcaveSnapDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|General")
	float ArrivalThreshold = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Corner|General")
	float RotationArrivalThreshold = 3.0f;
};

USTRUCT(BlueprintType)
struct FGlidingStateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float GravityScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float AirControl = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	FRotator RotationRate = FRotator(0.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	FVector LaunchVelocity = FVector(0.0f, 0.0f, -200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float MaxHorizontalSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float MinDescentSpeed = -200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding|Physics")
	float MaxDescentSpeed = -50.0f;
};

USTRUCT(BlueprintType)
struct FMovementStateConfigs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grounded")
	float GroundedRotationRate = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Falling")
	FFallingStateConfig FallingConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Climbing")
	FClimbingStateConfig ClimbingConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CornerTransition")
	FCornerTransitionConfig CornerConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gliding")
	FGlidingStateConfig GlidingConfig;
};
