// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BackpackComponent.generated.h"

class AItemBase;
class UInventoryManagerSubsystem;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UBackpackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBackpackComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // --- 核心交互接口 ---

    /** 客户端请求拾取物品（内部调用 Server RPC） */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void PickUpItem();

    /** 服务器执行拾取物品逻辑 */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_PickUpItem(int32 ItemID, int32 Amount);

    /** 按 GUID 丢弃物品（内部调用 Server RPC） */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void DropItemByGUID(FGuid ItemGUID, int32 DropAmount);

    /** 服务器执行丢弃物品逻辑 */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_DropItemByGUID(FGuid ItemGUID, int32 DropAmount);

    /** 按索引丢弃物品 (保留兼容) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void DropItem(int32 DropIndex, int32 DropAmount);

    /** 使用物品 (食物/经验书等) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    bool UseItemByGUID(FGuid ItemGUID, int32 UseAmount = 1);

    /** 装备物品到当前角色 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool EquipItemByGUID(FGuid ItemGUID);

    /** 卸下物品 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    bool UnequipItemByGUID(FGuid ItemGUID);

    // 供外界获取当前可拾取的物体
    UFUNCTION(BlueprintPure, Category = "Inventory|State")
    AItemBase* GetCurrentPickableItem() const { return CurrentPickableItem.Get(); }

protected:
    UFUNCTION()
    void HandleOnItemDropped(int32 ItemID, int32 DroppedAmount);

    /** 服务器端：在角色前方生成丢弃物品 */
    void SpawnDroppedItem(int32 ItemID, int32 DroppedAmount);

    bool IsCharacterInStandby() const;

    /** 获取当前角色的 CharacterID (用于装备系统) */
    int32 GetOwnerCharacterID() const;

private:
    TWeakObjectPtr<AItemBase> CurrentPickableItem;

    UPROPERTY()
    UInventoryManagerSubsystem* InventorySubsystem;
};
