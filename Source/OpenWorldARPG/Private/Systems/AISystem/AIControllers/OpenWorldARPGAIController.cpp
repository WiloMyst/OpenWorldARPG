// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AISystem/AIControllers/OpenWorldARPGAIController.h"
#include "Perception/AIPerceptionComponent.h"

AOpenWorldARPGAIController::AOpenWorldARPGAIController()
{
    AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
    SetPerceptionComponent(*AIPerceptionComp);
}