// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Systems/MapSystem/Data/WorldMapConfigDataAsset.h"
#include "WorldMapWidget.generated.h"

class UMapManagerSubsystem;
class UTexture2D;

/**
 * 全屏世界地图 Widget 基类（对应 WBP_WorldMap）
 *
 * C++ 职责：
 * - 缓存 MapManagerSubsystem 并绑定数据变更委托
 * - 提供已发现 POI / 区域数据供蓝图创建标记
 * - 提供 POI 选中、快速旅行等业务接口
 * - 世界坐标 ↔ 地图 UV 转换
 *
 * 蓝图职责：
 * - 显示世界地图纹理（WorldMapTexture）
 * - 遍历 DiscoveredPOIs 创建标记 Widget
 * - 标记位置 = GetPOIMapUV(POITag) × 地图 Image 尺寸
 * - 点击标记调用 SelectPOI，弹出确认面板
 * - 确认旅行调用 RequestTravelToSelected
 * - 监听 OnMapDataChanged 刷新标记（POI 新发现时）
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UWorldMapWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    // --- 数据（供蓝图读取创建标记） ---

    /** 所有已发现的 POI */
    UPROPERTY(BlueprintReadOnly, Category = "WorldMap|Data")
    TArray<FMapPOIData> DiscoveredPOIs;

    /** 所有已发现的区域 */
    UPROPERTY(BlueprintReadOnly, Category = "WorldMap|Data")
    TArray<FMapRegionData> DiscoveredRegions;

    /** 世界地图纹理 */
    UPROPERTY(BlueprintReadOnly, Category = "WorldMap|Data")
    TObjectPtr<UTexture2D> WorldMapTexture;

    /** 世界 2D 边界（cm） */
    UPROPERTY(BlueprintReadOnly, Category = "WorldMap|Data")
    FBox2D WorldBounds = FBox2D(FVector2D::ZeroVector, FVector2D::ZeroVector);

    /** 当前选中的 POI Tag */
    UPROPERTY(BlueprintReadOnly, Category = "WorldMap|Data")
    FGameplayTag SelectedPOITag;

    // --- 蓝图可调用接口 ---

    /** 获取指定 POI 在地图上的 UV 坐标 (0~1) */
    UFUNCTION(BlueprintCallable, Category = "WorldMap")
    FVector2D GetPOIMapUV(FGameplayTag POITag) const;

    /** 获取选中 POI 的数据 */
    UFUNCTION(BlueprintCallable, Category = "WorldMap", BlueprintPure)
    bool GetSelectedPOI(FMapPOIData& OutPOI) const;

    /** 选中某个 POI */
    UFUNCTION(BlueprintCallable, Category = "WorldMap")
    void SelectPOI(FGameplayTag POITag);

    /** 检查指定 POI 是否可快速旅行 */
    UFUNCTION(BlueprintCallable, Category = "WorldMap", BlueprintPure)
    bool CanTravelToPOI(FGameplayTag POITag) const;

    /** 请求快速旅行到当前选中的 POI */
    UFUNCTION(BlueprintCallable, Category = "WorldMap")
    bool RequestTravelToSelected();

    /** 请求快速旅行到指定 POI */
    UFUNCTION(BlueprintCallable, Category = "WorldMap")
    bool RequestTravelToPOI(FGameplayTag POITag);

    /** 手动刷新地图数据（POI 列表、纹理等） */
    UFUNCTION(BlueprintCallable, Category = "WorldMap")
    void RefreshMapData();

    /** 关闭世界地图 */
    UFUNCTION(BlueprintCallable, Category = "WorldMap")
    void CloseWorldMap();

private:
    UPROPERTY(Transient)
    TObjectPtr<UMapManagerSubsystem> MapSubsystem;

    /** 地图数据变更回调 */
    UFUNCTION()
    void OnMapDataChanged();
};
