// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/ItemInstance.h"
#include "Types/ItemData.h"
#include "ItemObject.generated.h"

class UItemObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemDataChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectionStateChanged, UItemObject*, ItemObject, bool, bIsSelected);

/**
 * 物品数据包装类 (ViewModel 内部载体)
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UItemObject : public UObject
{
    GENERATED_BODY()

public:
    /** 物品动态实例数据 */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Data")
    FItemInstance ItemInstance;

    /** 在筛选后数组中的索引 */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Data")
    int32 ItemArrayIndex = -1;

    /** 是否被选中 */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|Data")
    bool bIsSelected = false;

    /** 静态数据缓存（Transient，由 ViewModel 注入，UI 直接访问配置字段） */
    const FItemData* CachedStaticData = nullptr;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemDataChanged OnItemDataChanged;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnSelectionStateChanged OnSelectionStateChanged;

    /** 注入动态实例与静态数据 */
    void Initialize(const FItemInstance& InInstance, const FItemData* InStaticData);

    /** 蓝图安全接口：返回静态数据拷贝 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Data")
    FItemData GetItemStaticData() const;

    /** 设置选中状态并广播（仅状态改变时广播） */
    void SetSelected(bool bNewSelected);
};
