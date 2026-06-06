// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/ItemData.h"
#include "ItemBase.generated.h"

UCLASS()
class OPENWORLDARPG_API AItemBase : public AActor
{
    GENERATED_BODY()

public:
    AItemBase();

    // 当 Actor 在编辑器中被移动，或者其属性（如 ItemID）被修改时，自动调用此函数
    // 它是 C++ 中对应蓝图 Construction Script 的等价物
    virtual void OnConstruction(const FTransform& Transform) override;

    // 用于在游戏运行时动态初始化掉落物
    UFUNCTION(BlueprintCallable, Category = "Item")
    void InitializeItem(int32 InItemID, int32 InAmount);

    // 物品ID (修改这个值，模型就会在编辑器里自动刷新)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemAmount = 1;

    // 视觉与物理组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* ItemMesh;

    // 用于生成后施加抛出物理力的接口
    void ApplyThrowPhysics(FVector Velocity);

private:
    // 内部函数：根据当前的 ItemID 刷新模型
    void RefreshMeshFromID();
};