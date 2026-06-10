// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InteractionComponent.generated.h"

class AItemBase;
class UInventoryManagerSubsystem;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // --- 核心交互接口 ---

    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void PickUpItem();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_PickUpItem(int32 ItemID, int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void DropItemByGUID(FGuid ItemGUID, int32 DropAmount);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_DropItemByGUID(FGuid ItemGUID, int32 DropAmount);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void DropItem(int32 DropIndex, int32 DropAmount);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    bool UseItemByGUID(FGuid ItemGUID, int32 UseAmount = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool EquipItemByGUID(FGuid ItemGUID);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool UnequipItemByGUID(FGuid ItemGUID);

    UFUNCTION(BlueprintPure, Category = "Inventory|State")
    AItemBase* GetCurrentPickableItem() const { return CurrentPickableItem.Get(); }

protected:
    UFUNCTION()
    void HandleOnItemDropped(int32 ItemID, int32 DroppedAmount);

    void SpawnDroppedItem(int32 ItemID, int32 DroppedAmount);

    bool IsCharacterInStandby() const;

    int32 GetOwnerCharacterID() const;

private:
    TWeakObjectPtr<AItemBase> CurrentPickableItem;

    UPROPERTY()
    UInventoryManagerSubsystem* InventorySubsystem;
};
