// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/SelectableTargetActor.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TargetingComponent.h"
#include "Characters/PlayerCharacter.h"

ASelectableTargetActor::ASelectableTargetActor()
{
    PrimaryActorTick.bCanEverTick = false;

    DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
    RootComponent = DefaultRoot;

    DetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectSphere"));
    DetectSphere->SetupAttachment(RootComponent);
    DetectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ASelectableTargetActor::BeginPlay()
{
    Super::BeginPlay();

    if (DetectSphere)
    {
        DetectSphere->OnComponentBeginOverlap.AddDynamic(this, &ASelectableTargetActor::OnDetectSphereBeginOverlap);
        DetectSphere->OnComponentEndOverlap.AddDynamic(this, &ASelectableTargetActor::OnDetectSphereEndOverlap);
    }
}

void ASelectableTargetActor::OnSetAsTarget_Implementation() { }
void ASelectableTargetActor::OnClearAsTarget_Implementation() { }

void ASelectableTargetActor::OrientToScreen(USceneComponent* SceneCompToOrient)
{
    if (!SceneCompToOrient) return;

    if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        SceneCompToOrient->SetWorldRotation(CameraManager->GetCameraRotation());
        SceneCompToOrient->AddLocalRotation(FRotator(180.0f, 0.0f, 180.0f));
    }
}

void ASelectableTargetActor::OnDetectSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor)
    {
        if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(OtherActor))
        {
            if (UTargetingComponent* TargetComp = PlayerChar->GetTargetingComponent())
            {
                TargetComp->AddTarget(this);
            }
        }
    }
}

void ASelectableTargetActor::OnDetectSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor)
    {
        if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(OtherActor))
        {
            if (UTargetingComponent* TargetComp = PlayerChar->GetTargetingComponent())
            {
                TargetComp->RemoveTarget(this);
            }
        }
    }
}