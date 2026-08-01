// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Systems/MapSystem/Data/WorldMapConfigDataAsset.h"
#include "MapManagerSubsystem.generated.h"

class UTexture2D;

/** POI 发现委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPOIDiscovered, FGameplayTag, POITag);

/** 区域发现委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRegionDiscovered, FGameplayTag, RegionTag);

/** 地图数据变更委托（POI 发现/解锁等） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMapDataChanged);

/**
 * 地图管理子系统（LocalPlayerSubsystem）
 *
 * 职责：
 * - 加载和管理世界地图静态配置（POI、区域、纹理）
 * - 维护玩家发现状态（哪些 POI/区域已发现）
 * - 提供地图数据查询接口供 UI 读取
 * - 世界坐标 ↔ 地图 UV 坐标转换
 * - 玩家位置和朝向查询（供小地图/全屏地图定位）
 * - 快速旅行请求与执行
 *
 * 数据流：
 *   UWorldMapConfigDataAsset（静态配置）
 *     → UMapManagerSubsystem（运行时管理 + 发现状态）
 *       → UI Widget（读取数据渲染地图标记）
 *
 * 发现机制：
 *   - 自动发现：玩家进入 POI 的 DiscoverRadius 范围（UpdateProximityDiscovery 由 PC 调用）
 *   - 手动发现：任务完成、剧情触发等调用 DiscoverPOI()
 *   - 区域发现：玩家进入区域 WorldBounds 范围
 */
UCLASS()
class OPENWORLDARPG_API UMapManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    // --- 子系统生命周期 ---

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 配置加载 ---

    /** 加载世界地图配置（同步加载） */
    UFUNCTION(BlueprintCallable, Category = "Map|Config")
    bool LoadMapConfig(TSoftObjectPtr<UWorldMapConfigDataAsset> ConfigPath);

    /** 获取当前加载的地图配置 */
    UFUNCTION(BlueprintCallable, Category = "Map|Config", BlueprintPure)
    UWorldMapConfigDataAsset* GetMapConfig() const { return MapConfig; }

    // --- POI 查询 ---

    /** 获取所有 POI */
    UFUNCTION(BlueprintCallable, Category = "Map|POI", BlueprintPure)
    void GetAllPOIs(TArray<FMapPOIData>& OutPOIs) const;

    /** 获取所有已发现的 POI */
    UFUNCTION(BlueprintCallable, Category = "Map|POI", BlueprintPure)
    void GetDiscoveredPOIs(TArray<FMapPOIData>& OutPOIs) const;

    /** 按类型获取 POI */
    UFUNCTION(BlueprintCallable, Category = "Map|POI", BlueprintPure)
    void GetPOIsByType(EMapPOIType Type, TArray<FMapPOIData>& OutPOIs) const;

    /** 按 Tag 获取单个 POI */
    UFUNCTION(BlueprintCallable, Category = "Map|POI", BlueprintPure)
    bool GetPOIByTag(FGameplayTag POITag, FMapPOIData& OutPOI) const;

    // --- POI 发现/解锁 ---

    /** 发现 POI（手动触发，如任务完成） */
    UFUNCTION(BlueprintCallable, Category = "Map|POI")
    bool DiscoverPOI(FGameplayTag POITag);

    /** POI 是否已发现 */
    UFUNCTION(BlueprintCallable, Category = "Map|POI", BlueprintPure)
    bool IsPOIDiscovered(FGameplayTag POITag) const;

    /** 解锁 POI（满足任务条件后调用） */
    UFUNCTION(BlueprintCallable, Category = "Map|POI")
    bool UnlockPOI(FGameplayTag POITag);

    /** POI 是否已解锁 */
    UFUNCTION(BlueprintCallable, Category = "Map|POI", BlueprintPure)
    bool IsPOIUnlocked(FGameplayTag POITag) const;

    /** 近距离自动发现（由 PlayerController 定期调用） */
    UFUNCTION(BlueprintCallable, Category = "Map|POI")
    void UpdateProximityDiscovery(FVector PlayerLocation);

    // --- 区域查询/发现 ---

    /** 获取所有区域 */
    UFUNCTION(BlueprintCallable, Category = "Map|Region", BlueprintPure)
    void GetAllRegions(TArray<FMapRegionData>& OutRegions) const;

    /** 获取所有已发现区域 */
    UFUNCTION(BlueprintCallable, Category = "Map|Region", BlueprintPure)
    void GetDiscoveredRegions(TArray<FMapRegionData>& OutRegions) const;

    /** 发现区域 */
    UFUNCTION(BlueprintCallable, Category = "Map|Region")
    bool DiscoverRegion(FGameplayTag RegionTag);

    /** 区域是否已发现 */
    UFUNCTION(BlueprintCallable, Category = "Map|Region", BlueprintPure)
    bool IsRegionDiscovered(FGameplayTag RegionTag) const;

    // --- 坐标转换 ---

    /**
     * 世界坐标 → 地图 UV 坐标
     * @param WorldLocation 世界坐标
     * @return UV 坐标 (0~1)，超出世界边界则钳制
     */
    UFUNCTION(BlueprintCallable, Category = "Map|Coordinate", BlueprintPure)
    FVector2D WorldToMapUV(FVector WorldLocation) const;

    /**
     * 地图 UV → 世界坐标（用于点击地图选择目标）
     */
    UFUNCTION(BlueprintCallable, Category = "Map|Coordinate", BlueprintPure)
    FVector MapUVToWorld(FVector2D MapUV) const;

    // --- 玩家位置 ---

    /** 获取玩家世界坐标 2D（X, Y） */
    UFUNCTION(BlueprintCallable, Category = "Map|Player", BlueprintPure)
    FVector2D GetPlayerWorldPosition2D() const;

    /** 获取玩家朝向角度（Yaw） */
    UFUNCTION(BlueprintCallable, Category = "Map|Player", BlueprintPure)
    float GetPlayerFacingAngle() const;

    /** 获取玩家所在区域 */
    UFUNCTION(BlueprintCallable, Category = "Map|Player", BlueprintPure)
    FGameplayTag GetPlayerCurrentRegion() const;

    // --- 快速旅行 ---

    /** 检查是否可以快速旅行到指定 POI */
    UFUNCTION(BlueprintCallable, Category = "Map|FastTravel", BlueprintPure)
    bool CanFastTravel(FGameplayTag POITag) const;

    /** 请求快速旅行到指定 POI */
    UFUNCTION(BlueprintCallable, Category = "Map|FastTravel")
    bool RequestFastTravel(FGameplayTag POITag);

    // --- 委托 ---

    /** POI 发现时触发 */
    UPROPERTY(BlueprintAssignable, Category = "Map|Delegate")
    FOnPOIDiscovered OnPOIDiscovered;

    /** 区域发现时触发 */
    UPROPERTY(BlueprintAssignable, Category = "Map|Delegate")
    FOnRegionDiscovered OnRegionDiscovered;

    /** 地图数据变更时触发（UI 刷新用） */
    UPROPERTY(BlueprintAssignable, Category = "Map|Delegate")
    FOnMapDataChanged OnMapDataChanged;

private:
    /** 加载的地图配置 */
    UPROPERTY(Transient)
    TObjectPtr<UWorldMapConfigDataAsset> MapConfig;

    /** 已发现的 POI Tag 集合 */
    TSet<FGameplayTag> DiscoveredPOITags;

    /** 已发现的区域 Tag 集合 */
    TSet<FGameplayTag> DiscoveredRegionTags;

    /** 已解锁的 POI Tag 集合 */
    TSet<FGameplayTag> UnlockedPOITags;

    /** 上次自动发现检查的位置（避免重复检测） */
    FVector LastProximityCheckLocation = FVector::ZeroVector;

    /** 自动发现检查的最小移动距离 (cm) */
    static constexpr float ProximityCheckMinDistance = 500.0f;
};
