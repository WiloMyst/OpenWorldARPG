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

    /** 编辑器构造脚本，属性变更时自动刷新模型。 */
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, Category = "Item")
    void InitializeItem(int32 InItemID, int32 InAmount);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 ItemAmount = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* ItemMesh;

    void ApplyThrowPhysics(FVector Velocity);

private:
    void RefreshMeshFromID();
};