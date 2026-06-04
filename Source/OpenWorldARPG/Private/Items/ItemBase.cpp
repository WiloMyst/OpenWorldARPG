// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Items/ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Managers/GameAssetManagerSubsystem.h"

AItemBase::AItemBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    RootComponent = ItemMesh;

    ItemMesh->SetSimulatePhysics(true);
    ItemMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void AItemBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshMeshFromID();
}

void AItemBase::InitializeItem(int32 InItemID, int32 InAmount)
{
    ItemID = InItemID;
    ItemAmount = InAmount;
    RefreshMeshFromID();
}

void AItemBase::RefreshMeshFromID()
{
    if (ItemID <= 0) return;

    UGameAssetManagerSubsystem* AssetManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>() : nullptr;
    UDataTable* ItemTable = AssetManager ? AssetManager->GetItemDatabaseTable() : nullptr;

    if (!ItemTable) return;

    FName RowName = FName(*FString::FromInt(ItemID));
    FItemData* ItemData = ItemTable->FindRow<FItemData>(RowName, TEXT("ItemBase Refresh"));

    if (!ItemData) return;

    TSoftObjectPtr<UStaticMesh> MeshRef = ItemData->ItemMesh;
    if (MeshRef.IsNull()) return;

    // 编辑器模式下 OnConstruction 需要同步加载，否则无法即时预览
#if WITH_EDITOR
    if (UStaticMesh* LoadedMesh = MeshRef.LoadSynchronous())
    {
        ItemMesh->SetStaticMesh(LoadedMesh);
    }
#else
    // 运行时异步加载，不阻塞主线程
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

void AItemBase::ApplyThrowPhysics(FVector Velocity)
{
    if (ItemMesh && ItemMesh->IsSimulatingPhysics())
    {
        ItemMesh->SetPhysicsLinearVelocity(Velocity);
    }
}