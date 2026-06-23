// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/DungeonPlayerController.h"

ADungeonPlayerController::ADungeonPlayerController()
{
}

void ADungeonPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // TODO: 副本专属初始化（如加载副本 IMC、屏蔽大世界功能等）
}

void ADungeonPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // TODO: 副本专属输入绑定（如副本结算、投票等）
}
