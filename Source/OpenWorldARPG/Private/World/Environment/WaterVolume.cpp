// Copyright 2025 WiloMyst. All Rights Reserved.

#include "World/Environment/WaterVolume.h"
#include "Components/BoxComponent.h"

AWaterVolume::AWaterVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    WaterCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterCollision"));
    WaterCollision->SetupAttachment(RootComponent);
    WaterCollision->SetCollisionProfileName(TEXT("OverlapAll"));
    WaterCollision->SetGenerateOverlapEvents(true);

    // 默认大小
    WaterCollision->SetBoxExtent(FVector(2000.0f, 2000.0f, 500.0f));

    RootComponent = WaterCollision;
}

void AWaterVolume::BeginPlay()
{
    Super::BeginPlay();

    WaterCollision->OnComponentBeginOverlap.AddDynamic(this, &AWaterVolume::HandleWaterBeginOverlap);
    WaterCollision->OnComponentEndOverlap.AddDynamic(this, &AWaterVolume::HandleWaterEndOverlap);
}

float AWaterVolume::GetWaterSurfaceZ() const
{
    if (!WaterCollision) return GetActorLocation().Z;
    return WaterCollision->GetComponentLocation().Z + WaterCollision->GetScaledBoxExtent().Z;
}

float AWaterVolume::GetWaterDepth() const
{
    if (!WaterCollision) return 0.0f;
    return WaterCollision->GetScaledBoxExtent().Z * 2.0f;
}

FVector AWaterVolume::GetCurrentForce() const
{
    if (CurrentDirection.IsNearlyZero() || CurrentStrength <= 0.0f) return FVector::ZeroVector;
    return CurrentDirection.GetSafeNormal() * CurrentStrength;
}

void AWaterVolume::HandleWaterBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor != this)
    {
        OnActorEnterWater.Broadcast(OtherActor);
    }
}

void AWaterVolume::HandleWaterEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor != this)
    {
        OnActorExitWater.Broadcast(OtherActor);
    }
}
