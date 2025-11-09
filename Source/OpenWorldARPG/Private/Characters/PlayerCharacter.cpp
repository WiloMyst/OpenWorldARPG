// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Characters/PlayerCharacter.h"
#include "Data/CharacterData.h"
#include "Data/CharacterDataAsset.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

APlayerCharacter::APlayerCharacter()
{
    // Don't rotate when the controller rotates. Let that just affect the camera.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // Create a camera boom (pulls in towards the player if there is a collision)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
    CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

    // Create a follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
    FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
}

void APlayerCharacter::BindData(ACharacterData* InCharacterData)
{
    if (!InCharacterData)
    {
        UE_LOG(LogTemp, Error, TEXT("APlayerCharacter::BindData - InCharacterData is nullptr."));
        return;
    }

    // 1. 保存对数据核心的引用
    CharacterData = InCharacterData;

    // 2. 初始化ASC
    UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
    if (AbilitySystemComponent)
    {
        // 初始化Owner和Avatar
        AbilitySystemComponent->InitAbilityActorInfo(CharacterData, this);

        // 将AttributeSet添加到ASC的管理列表
        AbilitySystemComponent->GetSpawnedAttributes_Mutable().Add(CharacterData->GetAttributeSet());

        // ...后续逻辑，如赋予能力等
    }

    // 3. 从DataAsset更新外观
    if (const UCharacterDataAsset* DataAsset = CharacterData->GetDataSourceAsset())
    {
        if (USkeletalMeshComponent* MeshComponent = GetMesh())
        {
            // 异步加载骨骼网格体模型资源
            TSoftObjectPtr<USkeletalMesh> MeshToLoad = DataAsset->CharacterMesh;
            if (MeshToLoad.IsPending())
            {
                FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
                StreamableManager.RequestAsyncLoad(MeshToLoad.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &APlayerCharacter::OnMeshLoaded, DataAsset));
            }
            else
            {
                // 如果模型已经加载，则直接同步处理
                OnMeshLoaded(DataAsset);
            }
        }

        // ===== 绑定属性值 =====
        


    }

    // 在蓝图中执行额外的绑定后逻辑
    OnDataBound();
}

void APlayerCharacter::OnDataBound_Implementation()
{
    SetupBaseBehaviorAnimLayers();
    SetupAimAnimLayers();
    SetupPhysicsAnimLayers();

}

void APlayerCharacter::OnMeshLoaded(const UCharacterDataAsset* DataAsset)
{
    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!MeshComponent || !DataAsset) return;

    USkeletalMesh* LoadedMesh = DataAsset->CharacterMesh.Get();
    if (!LoadedMesh) return;

    // 设置模型
    MeshComponent->SetSkeletalMesh(LoadedMesh);

    // 设置动画蓝图类
    if (DataAsset->AnimationBlueprint)
    {
        MeshComponent->SetAnimInstanceClass(DataAsset->AnimationBlueprint);
    }
}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
    return CharacterData ? CharacterData->GetAbilitySystemComponent() : nullptr;
}

void APlayerCharacter::SetStandbyMode(bool bNewStandbyState)
{
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("APlayerCharacter::SetStandbyMode - CharacterMovementComponent is null."));
        return;
    }

    if (bNewStandbyState)
    {
        // ----- 进入待机模式 -----

        // 1. 首先冻结物理，杜绝掉落风险。
        MoveComp->StopMovementImmediately();
        MoveComp->DisableMovement();

        // 2. 然后安全地移除碰撞。
        if (UCapsuleComponent* Capsule = GetCapsuleComponent())
        {
            Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Capsule->SetCollisionProfileName(TEXT("Spectator")); // "Spectator"预设通常只与世界碰撞
            // 明确设置对Visibility通道的响应为忽略，这将防止AI的视线追踪被此角色阻挡
            Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        }

        // 3. 接着关闭所有更新，优化性能。
        SetActorTickEnabled(false);
        //if (GetMesh())
        //{
        //    GetMesh()->SetComponentTickEnabled(false);
        //}
        MoveComp->SetComponentTickEnabled(false);

        // 4. 最后隐藏视觉。
        SetActorHiddenInGame(true);

        // 停止动画蒙太奇
        if (GetMesh() && GetMesh()->GetAnimInstance())
        {
            GetMesh()->GetAnimInstance()->StopAllMontages(0.1f);
        }
    }
    else
    {
        // ----- 进入活动模式 -----

        // 1. 先让角色在物理世界中有实体。
        if (UCapsuleComponent* Capsule = GetCapsuleComponent())
        {
            Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Capsule->SetCollisionProfileName(TEXT("Pawn"));
            // 将对Visibility通道的响应恢复为阻挡
            Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }

        // 2. 再让角色可见。
        SetActorHiddenInGame(false);

        // 3. 激活移动能力。此时角色会受重力影响并站稳在地面上。
        MoveComp->SetMovementMode(MOVE_Walking);

        // 4. 开启所有更新。
        SetActorTickEnabled(true);
        //if (GetMesh())
        //{
        //    GetMesh()->SetComponentTickEnabled(true);
        //}
        MoveComp->SetComponentTickEnabled(true);
    }
}

void APlayerCharacter::SetupBaseBehaviorAnimLayers()
{
    if (CharacterData)
    {
        if (const UCharacterDataAsset* DataSource = CharacterData->GetDataSourceAsset())
        {
            if (TSubclassOf<UAnimInstance> BaseBehaviorAnimLayerClass = DataSource->BaseBehaviorAnimLayers)
            {
                if (USkeletalMeshComponent* CharacterMesh = GetMesh())
                {
                    CharacterMesh->LinkAnimClassLayers(BaseBehaviorAnimLayerClass);
                }
            }
        }
    }
}

void APlayerCharacter::SetupAimAnimLayers()
{
    if (CharacterData)
    {
        if (const UCharacterDataAsset* DataSource = CharacterData->GetDataSourceAsset())
        {
            if (TSubclassOf<UAnimInstance> AimAnimLayerClass = DataSource->AimAnimLayers)
            {
                if (USkeletalMeshComponent* CharacterMesh = GetMesh())
                {
                    CharacterMesh->LinkAnimClassLayers(AimAnimLayerClass);
                }
            }
        }
    }
}

void APlayerCharacter::SetupPhysicsAnimLayers()
{
    if (CharacterData)
    {
        if (const UCharacterDataAsset* DataSource = CharacterData->GetDataSourceAsset())
        {
            if (TSubclassOf<UAnimInstance> PhysicsLayerClass = DataSource->PhysicsAnimLayers)
            {
                if (USkeletalMeshComponent* CharacterMesh = GetMesh())
                {
                    CharacterMesh->LinkAnimClassLayers(PhysicsLayerClass);
                }
            }
        }
    }
}

void APlayerCharacter::ClearPhysicsAnimLayers()
{
    if (CharacterData)
    {
        if (const UCharacterDataAsset* DataSource = CharacterData->GetDataSourceAsset())
        {
            if (TSubclassOf<UAnimInstance> PhysicsLayerClass = DataSource->PhysicsAnimLayers)
            {
                if (USkeletalMeshComponent* CharacterMesh = GetMesh())
                {
                    CharacterMesh->UnlinkAnimClassLayers(PhysicsLayerClass);
                }
            }
        }
    }
}
