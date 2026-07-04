// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CustomMovementModeTypes.generated.h"

UENUM(BlueprintType)
enum class ECustomMovementMode : uint8
{
	None = 0,
	Climbing = 1,                  // 攀爬模式
	ClimbingCornerTransition = 2,  // 墙角过渡 (阳角/阴角平滑过渡中)
	ClimbUp = 3,                   // 翻越 (从墙沿翻上)
	Gliding = 4,                   // 滑翔模式
	Swimming = 5                   // 游泳模式
};
