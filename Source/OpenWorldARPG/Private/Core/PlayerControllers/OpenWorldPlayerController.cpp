// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Core/PlayerControllers/OpenWorldPlayerController.h"

AOpenWorldPlayerController::AOpenWorldPlayerController()
{
}

void AOpenWorldPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // TODO: 大世界专属初始化（如加载大地图 IMC、滑翔伞输入等）
}

void AOpenWorldPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // TODO: 大世界专属输入绑定（如骑乘、传送等）
}
