// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Systems/InventoryManager/Types/ItemTypes.h"
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
 * MVVM 中介层：从 Model 获取数据并处理排序/筛选，通过委托广播驱动 View 更新。
 * 零GC：使用对象池复用 UItemObject。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UInventoryViewModel : public UObject
{
    GENERATED_BODY()

public:
    void InitializeViewModel(UInventoryManagerSubsystem* Subsystem);

    // --- View 调用的命令 ---

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SelectCategory(EItemCategory NewCategory);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SelectItem(UItemObject* ItemObj);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void RequestDiscardSelectedItem();

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SetSortMode(EItemSortMode NewSortMode);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
    void SetRarityFilter(EItemRarity NewFilter);

    // --- View 绑定的属性 ---

    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    const TArray<UItemObject*>& GetFilteredItemObjects() const { return FilteredItemObjects; }

    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    UItemObject* GetSelectedItemObject() const { return SelectedItemObject; }

    UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
    EItemCategory GetCurrentCategory() const { return CurrentCategory; }

    // --- 委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Inventory|ViewModel|Events")
    FOnInventoryListUpdated OnInventoryListUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|ViewModel|Events")
    FOnSelectedItemChanged OnSelectedItemChanged;

private:
    UPROPERTY()
    TObjectPtr<UInventoryManagerSubsystem> InventoryManager;

    UPROPERTY()
    TArray<TObjectPtr<UItemObject>> FilteredItemObjects;

    UPROPERTY()
    TObjectPtr<UItemObject> SelectedItemObject;

    EItemCategory CurrentCategory = EItemCategory::Weapon;
    EItemSortMode SortMode = EItemSortMode::ByRarity;
    EItemRarity RarityFilter = EItemRarity::Star1;

    /** 对象池：复用 UItemObject，避免频繁 NewObject */
    UPROPERTY()
    TArray<TObjectPtr<UItemObject>> ItemObjectPool;

    UFUNCTION()
    void HandleInventoryUpdated();

    UItemObject* AcquireItemObject();
    void RebuildFilteredItems();
    void SetSelectedItem(UItemObject* NewItem);
    void SortItemObjects();
};
