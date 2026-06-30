// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/Subsystems/CharacterManager/CharacterShowcaseStage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Data/CharacterVisualDataAsset.h"
#include "Animation/AnimInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

ACharacterShowcaseStage::ACharacterShowcaseStage()
{
    PrimaryActorTick.bCanEverTick = false;

    // --- 根组件 ---
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // --- 展示网格体 ---
    DisplayMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(Root);
    // 纯展示：禁用碰撞和物理
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayMesh->SetSimulatePhysics(false);
    DisplayMesh->SetCollisionProfileName(TEXT("NoCollision"));
    DisplayMesh->SetComponentTickEnabled(true);

    // --- 相机系统 ---
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Root);
    CameraBoom->TargetArmLength = 300.0f;
    CameraBoom->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
    CameraBoom->bDoCollisionTest = false; // 展台不需要碰撞检测
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 10.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 50.0f);

    ShowcaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ShowcaseCamera"));
    ShowcaseCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    ShowcaseCamera->SetFieldOfView(35.0f); // 窄 FOV，减少透视畸变

    // --- 展台幕布 (Backdrop) ---
    BackdropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropMesh"));
    BackdropMesh->SetupAttachment(Root);

    // 禁用所有碰撞、物理和导航（极致性能，纯视觉）
    BackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BackdropMesh->SetCollisionProfileName(TEXT("NoCollision"));
    BackdropMesh->SetCanEverAffectNavigation(false);

    // 关闭投影和接收贴花，防止展台灯光在幕布上打出奇怪的影子
    BackdropMesh->SetCastShadow(false);
    BackdropMesh->bReceivesDecals = false;

    // 将幕布放置在角色正后方（假设相机在正前方 -X 方向，幕布放在 +X 方向的极远处）
    // 尺寸放大，确保填满摄像机 FOV
    BackdropMesh->SetRelativeLocation(FVector(1000.0f, 0.0f, 0.0f));
    BackdropMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // 面向摄像机
    BackdropMesh->SetRelativeScale3D(FVector(20.0f, 20.0f, 20.0f));

    // --- 打光（三点布光） ---

    // 主光：正面偏左上
    KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(Root);
    KeyLight->SetRelativeLocation(FVector(150.0f, -100.0f, 250.0f));
    KeyLight->SetRelativeRotation(FRotator(-45.0f, 35.0f, 0.0f));
    KeyLight->Intensity = 3000.0f;
    KeyLight->LightColor = FColor(255, 245, 230); // 暖白
    KeyLight->AttenuationRadius = 800.0f;
    KeyLight->InnerConeAngle = 30.0f;
    KeyLight->OuterConeAngle = 45.0f;

    // 补光：右侧柔光
    FillLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(Root);
    FillLight->SetRelativeLocation(FVector(100.0f, 120.0f, 180.0f));
    FillLight->SetRelativeRotation(FRotator(-30.0f, -40.0f, 0.0f));
    FillLight->Intensity = 1200.0f;
    FillLight->LightColor = FColor(220, 235, 255); // 冷白
    FillLight->AttenuationRadius = 600.0f;
    FillLight->InnerConeAngle = 35.0f;
    FillLight->OuterConeAngle = 50.0f;

    // 轮廓光：背后上方
    RimLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("RimLight"));
    RimLight->SetupAttachment(Root);
    RimLight->SetRelativeLocation(FVector(-150.0f, 0.0f, 200.0f));
    RimLight->SetRelativeRotation(FRotator(-40.0f, 180.0f, 0.0f));
    RimLight->Intensity = 2000.0f;
    RimLight->LightColor = FColor(255, 255, 255);
    RimLight->AttenuationRadius = 600.0f;
    RimLight->InnerConeAngle = 30.0f;
    RimLight->OuterConeAngle = 45.0f;
}

void ACharacterShowcaseStage::BeginPlay()
{
    Super::BeginPlay();
}

void ACharacterShowcaseStage::SwitchDisplayCharacter(const UCharacterVisualDataAsset* VisualData)
{
    if (!VisualData || !DisplayMesh) return;

    // 取消上一个未完成的异步加载请求，避免旧请求覆盖新请求（玩家快速切换角色时）
    if (MeshStreamingHandle.IsValid())
    {
        MeshStreamingHandle->CancelHandle();
        MeshStreamingHandle.Reset();
    }

    // 记录当前请求的 VisualData，供异步加载回调使用
    PendingVisualData = VisualData;

    // 1. 异步加载骨骼网格体（避免同步加载造成的卡顿）
    if (!VisualData->CharacterMesh.IsNull())
    {
        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        MeshStreamingHandle = Streamable.RequestAsyncLoad(
            VisualData->CharacterMesh.ToSoftObjectPath(),
            FStreamableDelegate::CreateUObject(this, &ACharacterShowcaseStage::OnMeshLoaded));
    }
    else
    {
        DisplayMesh->SetSkeletalMesh(nullptr);
    }

    // 2. 优先使用展台专用展示动画蓝图，未配置则回退角色动画蓝图
    // 动画蓝图是 Class 资源，体积小且有 Class Cache，保持同步加载即可
    const auto& AnimBPPtr =
        VisualData->ShowcaseAnimationBlueprint.IsNull()
            ? VisualData->AnimationBlueprint
            : VisualData->ShowcaseAnimationBlueprint;

    if (!AnimBPPtr.IsNull())
    {
        TSubclassOf<UAnimInstance> LoadedAnimBP = AnimBPPtr.LoadSynchronous();
        if (LoadedAnimBP)
        {
            DisplayMesh->SetAnimInstanceClass(LoadedAnimBP);
        }
    }
}

void ACharacterShowcaseStage::OnMeshLoaded()
{
    if (!MeshStreamingHandle.IsValid() || !DisplayMesh) return;

    // 校验 PendingVisualData 仍然有效（角色界面可能已被关闭，Actor 即将销毁）
    if (!PendingVisualData.IsValid())
    {
        MeshStreamingHandle.Reset();
        return;
    }

    USkeletalMesh* LoadedMesh = Cast<USkeletalMesh>(MeshStreamingHandle->GetLoadedAsset());
    if (LoadedMesh)
    {
        DisplayMesh->SetSkeletalMesh(LoadedMesh);
    }

    MeshStreamingHandle.Reset();
    PendingVisualData = nullptr;
}

void ACharacterShowcaseStage::RotateCharacter(float DeltaYaw)
{
    if (!DisplayMesh) return;

    // 围绕 Z 轴旋转展示网格体
    const FRotator CurrentRot = DisplayMesh->GetRelativeRotation();
    const FRotator NewRot = FRotator(CurrentRot.Pitch, CurrentRot.Yaw - DeltaYaw, CurrentRot.Roll);
    DisplayMesh->SetRelativeRotation(NewRot);
}
