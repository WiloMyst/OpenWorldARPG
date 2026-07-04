// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/InteractionSystem/PickableItemBase.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Systems/GameFlowManager/GameAssetManagerSubsystem.h"
#include "Systems/InventoryManager/InventoryManagerSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

APickableItemBase::APickableItemBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    RootComponent = ItemMesh;

    ItemMesh->SetSimulatePhysics(true);
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ItemMesh->SetCollisionObjectType(ECC_PhysicsBody);

    // 忽略所有通道后按需开启
    ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    ItemMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    ItemMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    ItemMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    ItemMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    ItemMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void APickableItemBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshMeshFromID();
}

// --- 接口实现 ---

bool APickableItemBase::CanInteract_Implementation(ACharacter* InstigatorCharacter) const
{
    return IsValid(this) && ItemID > 0;
}

void APickableItemBase::OnInteract_Implementation(ACharacter* InstigatorCharacter)
{
    if (!InstigatorCharacter) return;

    if (InstigatorCharacter->HasAuthority())
    {
        if (APlayerController* PC = Cast<APlayerController>(InstigatorCharacter->GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UInventoryManagerSubsystem* InventorySubsystem = LocalPlayer->GetSubsystem<UInventoryManagerSubsystem>())
                {
                    InventorySubsystem->AddItem(ItemID, ItemAmount);
                    Destroy();
                    return;
                }
            }
        }
    }
}

FTransform APickableItemBase::GetInteractionTargetTransform_Implementation() const
{
    return GetActorTransform();
}

// --- 公开接口 ---

void APickableItemBase::InitializeItem(int32 InItemID, int32 InAmount)
{
    ItemID = InItemID;
    ItemAmount = InAmount;
    RefreshMeshFromID();
}

void APickableItemBase::ApplyThrowPhysics(FVector Velocity)
{
    if (ItemMesh && ItemMesh->IsSimulatingPhysics())
    {
        ItemMesh->SetPhysicsLinearVelocity(Velocity);
    }
}

// --- 内部逻辑 ---

void APickableItemBase::RefreshMeshFromID()
{
    if (ItemID <= 0) return;

    UGameAssetManagerSubsystem* AssetManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>() : nullptr;
    UDataTable* ItemTable = AssetManager ? AssetManager->GetItemDatabaseTable() : nullptr;

    // 编辑器无 GameInstance 时从项目设置同步加载
    if (!ItemTable)
    {
        const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
        if (!Settings.ItemDatabaseTable.IsNull())
        {
            ItemTable = Settings.ItemDatabaseTable.LoadSynchronous();
        }
    }

    if (!ItemTable) return;

    FName RowName = FName(*FString::FromInt(ItemID));
    FItemData* ItemData = ItemTable->FindRow<FItemData>(RowName, TEXT("PickableItemBase Refresh"));

    if (!ItemData) return;

    TSoftObjectPtr<UStaticMesh> MeshRef = ItemData->ItemMesh;
    if (MeshRef.IsNull()) return;

#if WITH_EDITOR
    if (UStaticMesh* LoadedMesh = MeshRef.LoadSynchronous())
    {
        ItemMesh->SetStaticMesh(LoadedMesh);
    }
#else
    FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
    StreamableManager.RequestAsyncLoad(
        MeshRef.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([this, MeshRef]()
        {
            if (UStaticMesh* LoadedMesh = MeshRef.Get())
            {
                ItemMesh->SetStaticMesh(LoadedMesh);
            }
        })
    );
#endif
}
