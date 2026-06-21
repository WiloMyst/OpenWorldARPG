// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/ItemTypes.h"
#include "InventoryViewModel.generated.h"

class UInventoryManagerSubsystem;
class UItemObject;
class UInteractionComponent;

/** 列表刷新时广播 (UI 监听后调用 SetListItems) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryListUpdated);

/** 选中物品切换时广播 (DetailPanel 监听后更新详情) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedItemChanged, UItemObject*, SelectedItemObject);

/**
 * 背包视图模型 (ViewModel)
 *
 * MVVM 架构的核心中介层，位于 Model (UInventoryManagerSubsystem) 与 View (UMG Widget) 之间。
 *
 * 职责：
 *  - 从 Model 获取数据，处理排序、筛选逻辑
 *  - 暴露纯粹的属性供 UI 绑定 (FilteredItemObjects, SelectedItemObject, CurrentCategory)
 *  - 接收 View 的用户操作 (SelectCategory, SelectItem, RequestDiscardSelectedItem)
 *  - 通过委托广播状态变化，驱动 View 更新
 *
 * 零GC原则：使用对象池复用 UItemObject，切换分类时不重新 NewObject。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UInventoryViewModel : public UObject
{
    GENERATED_BODY()

public:
    /**
     * 初始化视图模型，绑定 Model 层委托。
     * @param Subsystem 背包管理子系统 (Model)
     */
    void InitializeViewModel(UInventoryManagerSubsystem* Subsystem);

    // --- View 调用的命令 ---

    /** 切换分类，VM 内部负责过滤并重构 FilteredItemObjects，最后广播列表更新 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SelectCategory(EItemCategory NewCategory);

    /** 选中物品，VM 内部更新选中状态，广播选中更新 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SelectItem(UItemObject* ItemObj);

    /** 请求丢弃选中物品 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestDiscardSelectedItem();

    /** 切换排序模式 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SetSortMode(EItemSortMode NewSortMode);

    /** 切换稀有度筛选 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SetRarityFilter(EItemRarity NewFilter);

    // --- View 绑定的属性 (只读) ---

    /** 当前分类与排序下，展示给 TileView 的对象数组 */
    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    const TArray<UItemObject*>& GetFilteredItemObjects() const { return FilteredItemObjects; }

    /** 当前选中的物品 */
    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    UItemObject* GetSelectedItemObject() const { return SelectedItemObject; }

    /** 当前分类 */
    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    EItemCategory GetCurrentCategory() const { return CurrentCategory; }

    // --- View 监听的委托 ---

    /** 列表刷新时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|ViewModel|Events")
    FOnInventoryListUpdated OnInventoryListUpdated;

    /** 选中物品切换时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|ViewModel|Events")
    FOnSelectedItemChanged OnSelectedItemChanged;

private:
    /** Model 层引用 */
    UPROPERTY()
    TObjectPtr<UInventoryManagerSubsystem> InventoryManager;

    /** 当前分类与排序下，展示给 TileView 的对象数组 */
    UPROPERTY()
    TArray<TObjectPtr<UItemObject>> FilteredItemObjects;

    /** 当前选中的物品 */
    UPROPERTY()
    TObjectPtr<UItemObject> SelectedItemObject;

    /** 当前分类 */
    EItemCategory CurrentCategory = EItemCategory::Weapon;

    /** 排序模式 */
    EItemSortMode SortMode = EItemSortMode::ByRarity;

    /** 稀有度筛选 */
    EItemRarity RarityFilter = EItemRarity::Star1;

    /** 对象池：缓存所有创建过的 UItemObject，避免频繁 NewObject (零GC) */
    UPROPERTY()
    TArray<TObjectPtr<UItemObject>> ItemObjectPool;

    /** Model 数据变化回调 */
    UFUNCTION()
    void HandleInventoryUpdated();

    /** 从对象池获取或新建 UItemObject */
    UItemObject* AcquireItemObject();

    /** 重新过滤并填充 FilteredItemObjects */
    void RebuildFilteredItems();

    /** 选中指定物品（内部实现，不广播） */
    void SetSelectedItem(UItemObject* NewItem);

    void SortItemObjects();
};
