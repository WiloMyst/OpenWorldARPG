// Copyright 2025 WiloMyst. All Rights Reserved.

#include "World/Environment/WorldAccelerationBoard.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

AWorldAccelerationBoard::AWorldAccelerationBoard()
{
    // 大厂规范：死物坚决关闭 Tick
    PrimaryActorTick.bCanEverTick = false;

    // 1. 组件创建与层级
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    // 实例化箭头组件，方便关卡策划在场景里旋转板子对齐方向
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(RootComponent);
    DirectionArrow->ArrowColor = FColor::Cyan;
    DirectionArrow->ArrowSize = 2.0f;

    BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
    BoardMesh->SetupAttachment(RootComponent);
    BoardMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(RootComponent);
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 2. 初始化你蓝图里的默认数值
    ForwardAccelerationSpeed = 1000.0f;
    UpwardBoostSpeed = 1000.0f; 
    bXYOverride = true;
    bZOverride = true;
}

void AWorldAccelerationBoard::BeginPlay()
{
    Super::BeginPlay();

    if (TriggerBox)
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AWorldAccelerationBoard::OnTriggerBoxOverlap);
    }
}

void AWorldAccelerationBoard::OnTriggerBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 使用通用的 ACharacter 而不是特定的 BP_PlayerCharacter
    ACharacter* OverlappedCharacter = Cast<ACharacter>(OtherActor);
    if (!OverlappedCharacter)
    {
        return;
    }

    // 通过通用全局接口获取 ASC，并尝试打断状态
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OverlappedCharacter);
    if (ASC && !TagsToCancelOnLaunch.IsEmpty())
    {
        ASC->CancelAbilities(&TagsToCancelOnLaunch);
    }

    // ==========================================
    // 核心物理运算：提取自你的蓝图数学节点
    // ==========================================
    // 1. 获取箭头的世界空间前向向量 (归一化方向)
    FVector ForwardDir = DirectionArrow->GetForwardVector();
    
    // 2. 方向 乘以 基础弹射速度，再加上 Z 轴的浮空补偿
    FVector FinalLaunchVelocity = (ForwardDir * ForwardAccelerationSpeed) + FVector(0.0f, 0.0f, UpwardBoostSpeed);

    // 3. 执行弹射
    OverlappedCharacter->LaunchCharacter(FinalLaunchVelocity, bXYOverride, bZOverride);
}