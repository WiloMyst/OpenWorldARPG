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

public:
    // --- 核心交互接口 ---

    // 对应蓝图自定义事件：拾取
    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void PickUpItem();

    // 对应蓝图自定义事件：丢弃
    UFUNCTION(BlueprintCallable, Category = "Inventory|Action")
    void DropItem(int32 DropIndex, int32 DropAmount);

    // 供外界（特别是 UI 系统）获取当前可拾取的物体
    UFUNCTION(BlueprintPure, Category = "Inventory|State")
    AItemBase* GetCurrentPickableItem() const { return CurrentPickableItem.Get(); }

protected:
    // 对应蓝图绑定的全局丢弃回调
    UFUNCTION()
    void HandleOnItemDropped(int32 ItemID, int32 DroppedAmount);

    // 对应蓝图：将物品从角色位置丢出
    void SpawnDroppedItem(int32 ItemID, int32 DroppedAmount);

    // 检查角色当前是否处于待机(不可交互)状态
    bool IsCharacterInStandby() const;

private:
    // 使用弱引用，Actor 被销毁后自动置空，避免悬空指针
    TWeakObjectPtr<AItemBase> CurrentPickableItem;

    // 缓存子系统的指针，避免频繁调用
    UPROPERTY()
    UInventoryManagerSubsystem* InventorySubsystem;
};