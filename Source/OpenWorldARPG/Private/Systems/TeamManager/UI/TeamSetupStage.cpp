// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/TeamManager/UI/TeamSetupStage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Systems/CharacterManager/CharacterManagerSubsystem.h"
#include "Characters/PlayerCharacter/Data/CharacterRegistryRow.h"
#include "Characters/PlayerCharacter/Data/CharacterVisualDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

ATeamSetupStage::ATeamSetupStage()
{
    PrimaryActorTick.bCanEverTick = false;

    // --- 根组件 ---
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // --- 展台摄像机（正对 4 个角色，距离适中） ---
    StageCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("StageCamera"));
    StageCamera->SetupAttachment(Root);
    StageCamera->SetRelativeLocation(FVector(450.0f, 0.0f, 120.0f));
    StageCamera->SetRelativeRotation(FRotator(-5.0f, -90.0f, 0.0f)); // 朝向 -Y 方向看向角色
    StageCamera->SetFieldOfView(35.0f); // 窄 FOV，减少透视畸变

    // --- 初始化 4 个槽位 ---
    InitializeSlots();

    // --- 打光（三点布光，照亮整个展台） ---

    // 主光：正面偏左上
    KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(Root);
    KeyLight->SetRelativeLocation(FVector(300.0f, -200.0f, 350.0f));
    KeyLight->SetRelativeRotation(FRotator(-45.0f, 35.0f, 0.0f));
    KeyLight->Intensity = 5000.0f;
    KeyLight->LightColor = FColor(255, 245, 230); // 暖白
    KeyLight->AttenuationRadius = 1500.0f;
    KeyLight->InnerConeAngle = 40.0f;
    KeyLight->OuterConeAngle = 60.0f;

    // 补光：右侧柔光
    FillLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(Root);
    FillLight->SetRelativeLocation(FVector(300.0f, 200.0f, 250.0f));
    FillLight->SetRelativeRotation(FRotator(-30.0f, -40.0f, 0.0f));
    FillLight->Intensity = 2000.0f;
    FillLight->LightColor = FColor(220, 235, 255); // 冷白
    FillLight->AttenuationRadius = 1200.0f;
    FillLight->InnerConeAngle = 45.0f;
    FillLight->OuterConeAngle = 65.0f;

    // 轮廓光：背后上方
    RimLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("RimLight"));
    RimLight->SetupAttachment(Root);
    RimLight->SetRelativeLocation(FVector(-200.0f, 0.0f, 300.0f));
    RimLight->SetRelativeRotation(FRotator(-40.0f, 180.0f, 0.0f));
    RimLight->Intensity = 3000.0f;
    RimLight->LightColor = FColor(255, 255, 255);
    RimLight->AttenuationRadius = 1200.0f;
    RimLight->InnerConeAngle = 40.0f;
    RimLight->OuterConeAngle = 60.0f;

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

    // 将幕布放置在 4 个角色正后方（摄像机朝 -Y 方向，幕布放在 -Y 方向的极远处）
    // 横向居中（X=0，覆盖 Slot0~Slot3 的 X 范围），尺寸放大确保填满摄像机 FOV
    BackdropMesh->SetRelativeLocation(FVector(0.0f, -1000.0f, 0.0f));
    BackdropMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f)); // 面向 +Y（摄像机方向）
    BackdropMesh->SetRelativeScale3D(FVector(20.0f, 20.0f, 20.0f));
}

void ATeamSetupStage::InitializeSlots()
{
    SlotAnchors.SetNum(MaxTeamSize);
    SlotMeshes.SetNum(MaxTeamSize);

    // 4 个槽位横向排开，间距 180 单位，居中（X 轴方向，因摄像机朝 -Y）
    const float SlotSpacing = 180.0f;
    const float StartX = -SlotSpacing * (MaxTeamSize - 1) * 0.5f;

    for (int32 i = 0; i < MaxTeamSize; ++i)
    {
        const FName AnchorName = *FString::Printf(TEXT("Slot%d_Anchor"), i);
        const FName MeshName = *FString::Printf(TEXT("Slot%d_Mesh"), i);

        // 锚点
        SlotAnchors[i] = CreateDefaultSubobject<USceneComponent>(AnchorName);
        SlotAnchors[i]->SetupAttachment(Root);
        SlotAnchors[i]->SetRelativeLocation(FVector(StartX + i * SlotSpacing, 0.0f, 0.0f));

        // 展示网格体（纯渲染，禁用碰撞和物理，不挂载任何逻辑组件）
        SlotMeshes[i] = CreateDefaultSubobject<USkeletalMeshComponent>(MeshName);
        SlotMeshes[i]->SetupAttachment(SlotAnchors[i]);
        SlotMeshes[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SlotMeshes[i]->SetSimulatePhysics(false);
        SlotMeshes[i]->SetCollisionProfileName(TEXT("NoCollision"));
        SlotMeshes[i]->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        //SlotMeshes[i]->SetVisibility(false); // 默认隐藏，待 RefreshStage 唤醒
    }
}

USkeletalMeshComponent* ATeamSetupStage::GetSlotMesh(int32 SlotIndex) const
{
    if (SlotMeshes.IsValidIndex(SlotIndex))
    {
        return SlotMeshes[SlotIndex];
    }
    return nullptr;
}

void ATeamSetupStage::RotateCharacters(float DeltaYaw)
{
    // 所有槽位锚点围绕 Z 轴同步旋转，摄像机和灯光保持不动
    for (USceneComponent* Anchor : SlotAnchors)
    {
        if (Anchor)
        {
            const FRotator CurrentRot = Anchor->GetRelativeRotation();
            Anchor->SetRelativeRotation(FRotator(CurrentRot.Pitch, CurrentRot.Yaw + DeltaYaw, CurrentRot.Roll));
        }
    }
}

void ATeamSetupStage::RefreshStage(const TArray<FGameplayTag>& TeamTags)
{
    UCharacterManagerSubsystem* Subsystem = nullptr;

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                Subsystem = LocalPlayer->GetSubsystem<UCharacterManagerSubsystem>();
            }
        }
    }

    if (!Subsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("ATeamSetupStage::RefreshStage: CharacterManagerSubsystem 不可用"));
        return;
    }

    for (int32 i = 0; i < MaxTeamSize; ++i)
    {
        USkeletalMeshComponent* SlotMesh = SlotMeshes[i];
        if (!SlotMesh) continue;

        // 槽位无 Tag 或超出数组范围：隐藏并清空
        if (!TeamTags.IsValidIndex(i) || !TeamTags[i].IsValid())
        {
            SlotMesh->SetVisibility(false);
            SlotMesh->SetSkeletalMesh(nullptr);
            SlotMesh->SetAnimInstanceClass(nullptr);
            continue;
        }

        const FGameplayTag& CharacterTag = TeamTags[i];

        // 通过 CharacterManagerSubsystem 桥梁查询 RegistryRow → VisualDataAsset
        FCharacterRegistryRow Row;
        if (!Subsystem->GetCharacterRegistryRowByTag(CharacterTag, Row))
        {
            UE_LOG(LogTemp, Warning, TEXT("ATeamSetupStage::RefreshStage: 找不到 Tag [%s] 的 RegistryRow"), *CharacterTag.ToString());
            SlotMesh->SetVisibility(false);
            continue;
        }

        // 解析 VisualDataAsset 软引用（同步加载，编队界面切换瞬间完成）
        const UCharacterVisualDataAsset* VisualData = Row.VisualData.Get();
        if (!VisualData && !Row.VisualData.IsNull())
        {
            VisualData = Row.VisualData.LoadSynchronous();
        }

        if (!VisualData)
        {
            UE_LOG(LogTemp, Warning, TEXT("ATeamSetupStage::RefreshStage: Tag [%s] 的 VisualDataAsset 加载失败"), *CharacterTag.ToString());
            SlotMesh->SetVisibility(false);
            continue;
        }

        // 1. 加载骨骼网格体
        if (!VisualData->CharacterMesh.IsNull())
        {
            USkeletalMesh* LoadedMesh = VisualData->CharacterMesh.LoadSynchronous();
            if (LoadedMesh)
            {
                SlotMesh->SetSkeletalMesh(LoadedMesh);
            }
        }
        else
        {
            SlotMesh->SetSkeletalMesh(nullptr);
        }

        // 2. 优先使用展台专用展示动画蓝图，未配置则回退角色动画蓝图
        const auto& AnimBPPtr =
            VisualData->ShowcaseAnimationBlueprint.IsNull()
                ? VisualData->AnimationBlueprint
                : VisualData->ShowcaseAnimationBlueprint;

        if (!AnimBPPtr.IsNull())
        {
            TSubclassOf<UAnimInstance> LoadedAnimBP = AnimBPPtr.LoadSynchronous();
            if (LoadedAnimBP)
            {
                SlotMesh->SetAnimInstanceClass(LoadedAnimBP);
                // 强制初始化动画实例，确保立即进入待机/站姿状态机，避免 T-Pose 僵住
                SlotMesh->InitAnim(true);
            }
        }
        else
        {
            SlotMesh->SetAnimInstanceClass(nullptr);
        }

        // 3. 显示该槽位
        SlotMesh->SetVisibility(true);
    }
}
