// Copyright 2025 WiloMyst. All Rights Reserved.

#include "World/Environment/WorldEjectionBoard.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

AWorldEjectionBoard::AWorldEjectionBoard()
{
    // 大厂规范 1：环境死物不需要每帧 Tick，立刻关闭以节省 CPU 性能
    PrimaryActorTick.bCanEverTick = false;

    // 1. 组件创建与层级绑定
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
    BoardMesh->SetupAttachment(RootComponent);
    // 视觉网格体通常只需要阻挡物理，不参与逻辑 Overlap 查询
    BoardMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(RootComponent);
    // 触发器只关心 Pawn 的重叠，忽略其他无关射线和物理对象
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 默认参数初始化
    LaunchVelocity = FVector(0.0f, 0.0f, 3000.0f);
    bXYOverride = true;
    bZOverride = true;
}

void AWorldEjectionBoard::BeginPlay()
{
    Super::BeginPlay();

    // 在 BeginPlay 中绑定委托，比在构造函数中绑定更安全
    if (TriggerBox)
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AWorldEjectionBoard::OnTriggerBoxOverlap);
    }
}

void AWorldEjectionBoard::OnTriggerBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 大厂规范 2：【极其重要】绝对不强转 (Cast) 到特定的 BP_PlayerCharacter
    // 而是强转到更通用的引擎基类 ACharacter。
    // 这样，这个弹射板不仅能弹玩家，还能弹怪物、NPC，乃至联机里的队友。
    ACharacter* OverlappedCharacter = Cast<ACharacter>(OtherActor);
    if (!OverlappedCharacter)
    {
        return;
    }

    // 大厂规范 3：通过 UAbilitySystemGlobals 获取 ASC
    // 彻底解耦，不需要知道对方是玩家还是怪物，只要实现了 IAbilitySystemInterface，就能拿到 ASC
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OverlappedCharacter);
    if (ASC && !TagsToCancelOnLaunch.IsEmpty())
    {
        // 打断指定的 GameplayAbilities
        ASC->CancelAbilities(&TagsToCancelOnLaunch);
    }

    // 弹射角色
    OverlappedCharacter->LaunchCharacter(LaunchVelocity, bXYOverride, bZOverride);
    
    // 【进阶扩展建议】：这里还可以通过 UGameplayStatics::PlaySoundAtLocation 播放 "嗖" 的弹射音效，
    // 或者生成 Niagara 阵风特效。将特效和音效资产也设为 UPROPERTY 让美术在蓝图里配置。
}