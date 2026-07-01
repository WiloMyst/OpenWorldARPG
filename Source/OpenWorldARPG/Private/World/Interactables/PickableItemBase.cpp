// Copyright 2025 WiloMyst. All Rights Reserved.


#include "World/Interactables/PickableItemBase.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Managers/GameAssetManagerSubsystem.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

// ============================================================================
// IInteractableInterface 实现
// ============================================================================

bool APickableItemBase::CanInteract_Implementation(ACharacter* InstigatorCharacter) const
{
    // 已被销毁或无效时不可交互
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

// ============================================================================

APickableItemBase::APickableItemBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    RootComponent = ItemMesh;

    ItemMesh->SetSimulatePhysics(true);
    
    // 理由：PhysicsActor 会 Block Pawn（导致玩家绊倒/卡走位）和 Block Camera（导致镜头抽搐拉近）
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ItemMesh->SetCollisionObjectType(ECC_PhysicsBody);
    
    // 1. 默认先忽略所有通道，做到最干净的基础状态
    ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    
    // 2. 阻挡静态地形和动态物体，确保能正常受到重力掉落在地上，不穿模
    ItemMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    ItemMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    
    // 3. 开启 Visibility 阻挡，以便玩家准星或交互射线 (Line Trace) 能够打到它进行拾取
    ItemMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    
    // 4. 明确忽略 Pawn（绝对不绊脚）和 Camera（绝对不卡镜头）
    ItemMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    ItemMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void APickableItemBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshMeshFromID();
}

void APickableItemBase::InitializeItem(int32 InItemID, int32 InAmount)
{
    ItemID = InItemID;
    ItemAmount = InAmount;
    RefreshMeshFromID();
}

void APickableItemBase::RefreshMeshFromID()
{
    if (ItemID <= 0) return;

    UGameAssetManagerSubsystem* AssetManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameAssetManagerSubsystem>() : nullptr;
    UDataTable* ItemTable = AssetManager ? AssetManager->GetItemDatabaseTable() : nullptr;

    // 如果没拿到（说明可能在编辑器中拖拽，没有 GameInstance），从全局设置读取
	if (!ItemTable)
	{
		const UOpenWorldARPGSettings& Settings = UOpenWorldARPGSettings::Get();
		if (!Settings.ItemDatabaseTable.IsNull())
		{
			// 同步加载数据表以供查询。不用担心开销，数据表很小，且编辑器下同步加载是正常的。
			ItemTable = Settings.ItemDatabaseTable.LoadSynchronous();
		}
	}

    if (!ItemTable) return;

    FName RowName = FName(*FString::FromInt(ItemID));
    FItemData* ItemData = ItemTable->FindRow<FItemData>(RowName, TEXT("PickableItemBase Refresh"));

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

void APickableItemBase::ApplyThrowPhysics(FVector Velocity)
{
    if (ItemMesh && ItemMesh->IsSimulatingPhysics())
    {
        ItemMesh->SetPhysicsLinearVelocity(Velocity);
    }
}
