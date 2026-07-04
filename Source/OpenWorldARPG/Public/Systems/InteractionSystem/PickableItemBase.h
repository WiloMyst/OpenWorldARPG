// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/InventoryManager/Data/ItemData.h"
#include "Systems/InteractionSystem/Interfaces/InteractableInterface.h"
#include "PickableItemBase.generated.h"

UCLASS()
class OPENWORLDARPG_API APickableItemBase : public AActor, public IInteractableInterface
{
    GENERATED_BODY()

public:
    APickableItemBase();
    virtual void OnConstruction(const FTransform& Transform) override;

    // --- 接口实现 (IInteractableInterface) ---

    virtual bool CanInteract_Implementation(ACharacter* InstigatorCharacter) const override;
    virtual void OnInteract_Implementation(ACharacter* InstigatorCharacter) override;
    virtual FTransform GetInteractionTargetTransform_Implementation() const override;

    // --- 公开接口 ---

    void InitializeItem(int32 InItemID, int32 InAmount);
    void ApplyThrowPhysics(FVector Velocity);

private:
    // --- 内部逻辑 ---

    void RefreshMeshFromID();

public:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* ItemMesh;

    // --- 物品数据 ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemAmount = 1;
};
