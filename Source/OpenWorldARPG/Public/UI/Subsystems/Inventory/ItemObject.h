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

    /**
     * 静态数据缓存指针 (Transient，不参与序列化)。
     * 由 ViewModel 在创建/复用 UItemObject 时一次性注入。
     * UI 层直接通过此指针访问 ItemName, ItemIcon, ItemRarity 等配置字段。
     */
    const FItemData* CachedStaticData = nullptr;

    /** 物品数据变化委托（用于通知 UI 刷新） */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemDataChanged OnItemDataChanged;

    /** 选中状态变化委托（Slot Widget 绑定此委托自动更新高亮） */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnSelectionStateChanged OnSelectionStateChanged;

    /**
     * 初始化方法：一次性注入动态实例与静态数据。
     * @param InInstance 物品动态实例数据
     * @param InStaticData 物品静态配置数据指针 (来自 DataTable，生命周期由 DataTable 管理)
     */
    void Initialize(const FItemInstance& InInstance, const FItemData* InStaticData);

    /**
     * 提供给蓝图 UI (UMG) 绑定的安全接口。
     * 每次 UI 需要读取配置（如名字、图标）时，调用此节点获取一份安全的数据拷贝。
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Data")
    FItemData GetItemStaticData() const;

    /**
     * 设置选中状态并广播变化。
     * 仅当状态实际改变时才广播，避免冗余刷新。
     */
    void SetSelected(bool bNewSelected);
};
