// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/GameServer/PlayerMovementReportComponent.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"

UPlayerMovementReportComponent::UPlayerMovementReportComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPlayerMovementReportComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UGameServerSubsystem* GameServer = GetGameServer())
    {
        GameServer->OnPositionCorrection.AddDynamic(this, &UPlayerMovementReportComponent::HandlePositionCorrection);

        // 已登录且本地控制：吸附到服务器权威出生点，保证与服务器 AOI 初始位置一致
        APawn* OwnerPawn = Cast<APawn>(GetOwner());
        if (OwnerPawn && OwnerPawn->IsLocallyControlled() && GameServer->IsLoggedIn())
        {
            SnapToServerSpawn(GameServer);
        }
    }
}

void UPlayerMovementReportComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameServerSubsystem* GameServer = GetGameServer())
    {
        GameServer->OnPositionCorrection.RemoveDynamic(this, &UPlayerMovementReportComponent::HandlePositionCorrection);
    }
    Super::EndPlay(EndPlayReason);
}

void UPlayerMovementReportComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 仅本地控制角色上报；待机角色（未 Possess）不参与
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
    {
        return;
    }

    AccumulatedTime += DeltaTime;
    if (AccumulatedTime >= ReportIntervalSec)
    {
        AccumulatedTime = 0.0f;
        ReportMovement();
    }
}

void UPlayerMovementReportComponent::ReportMovement()
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        return;
    }

    const FVector Location = OwnerPawn->GetActorLocation();
    const float Yaw = OwnerPawn->GetActorRotation().Yaw;

    // 位移/朝向未达阈值则跳过，避免静止时刷无效流量
    if (bHasReported)
    {
        const float Moved = FVector::Dist(Location, LastReportedLocation);
        const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(Yaw - LastReportedYaw));
        if (Moved < MinMoveDistance && YawDelta < MinYawDelta)
        {
            return;
        }
    }

    if (UGameServerSubsystem* GameServer = GetGameServer())
    {
        if (GameServer->SendMovementUpdate(Location, Yaw))
        {
            LastReportedLocation = Location;
            LastReportedYaw = Yaw;
            bHasReported = true;
        }
    }
}

void UPlayerMovementReportComponent::HandlePositionCorrection(FVector Location, float Yaw)
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
    {
        return;
    }

    // 服务器权威位置回执：吸附并同步上报基线，避免与服务器位置持续偏离
    OwnerPawn->SetActorLocation(Location);
    OwnerPawn->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
    LastReportedLocation = Location;
    LastReportedYaw = Yaw;
    bHasReported = true;
}

void UPlayerMovementReportComponent::SnapToServerSpawn(UGameServerSubsystem* GameServer)
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        return;
    }

    const FVector SpawnLocation = GameServer->GetSpawnLocation();
    const float SpawnYaw = GameServer->GetSpawnYaw();
    OwnerPawn->SetActorLocation(SpawnLocation);
    OwnerPawn->SetActorRotation(FRotator(0.0f, SpawnYaw, 0.0f));
    LastReportedLocation = SpawnLocation;
    LastReportedYaw = SpawnYaw;
    bHasReported = true;
    UE_LOG(LogTemp, Log, TEXT("[MovementReport] 吸附到服务器出生点 (%.1f,%.1f,%.1f)"),
           SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
}

UGameServerSubsystem* UPlayerMovementReportComponent::GetGameServer() const
{
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }
    const UWorld* World = Owner->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UGameServerSubsystem>() : nullptr;
}
