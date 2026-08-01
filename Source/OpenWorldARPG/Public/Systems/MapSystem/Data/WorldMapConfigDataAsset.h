// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WorldMapConfigDataAsset.generated.h"

class UTexture2D;

/** POI（兴趣点）类型 */
UENUM(BlueprintType)
enum class EMapPOIType : uint8
{
    /** 传送/快速旅行点 */
    FastTravel       UMETA(DisplayName = "传送点"),
    /** 任务目标 */
    Quest            UMETA(DisplayName = "任务"),
    /** 商店/NPC */
    Shop             UMETA(DisplayName = "商店"),
    /** Boss 挑战 */
    Boss             UMETA(DisplayName = "Boss"),
    /** 资源采集点 */
    Resource         UMETA(DisplayName = "资源"),
    /** 收集品 */
    Collectible      UMETA(DisplayName = "收集品"),
    /** 自定义 */
    Custom           UMETA(DisplayName = "自定义")
};

/** 兴趣点数据定义 */
USTRUCT(BlueprintType)
struct FMapPOIData
{
    GENERATED_BODY()

    /** POI 唯一标识 Tag（如 POI.FastTravel.Camp01） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI")
    FGameplayTag POITag;

    /** 显示名称 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI")
    FText DisplayName;

    /** POI 类型 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI")
    EMapPOIType Type = EMapPOIType::Custom;

    /** 世界坐标位置 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI")
    FVector WorldLocation = FVector::ZeroVector;

    /** 所在关卡路径（空 = 当前关卡） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI")
    FSoftObjectPath Level;

    /** 发现半径 (cm)，玩家进入此范围自动发现；0 = 仅手动发现 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
    float DiscoverRadius = 0.0f;

    /** 是否需要任务解锁（false = 默认可用） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI")
    bool bRequiresUnlock = false;

    /** 解锁条件 Tag（任务完成后检查） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "POI", meta = (EditCondition = "bRequiresUnlock"))
    FGameplayTag UnlockConditionTag;
};

/** 地图区域数据定义 */
USTRUCT(BlueprintType)
struct FMapRegionData
{
    GENERATED_BODY()

    /** 区域唯一标识 Tag（如 Region.EastForest） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
    FGameplayTag RegionTag;

    /** 区域显示名称 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
    FText DisplayName;

    /** 区域在世界坐标中的 2D 边界 (MinXY, MaxXY) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
    FBox2D WorldBounds = FBox2D(FVector2D::ZeroVector, FVector2D::ZeroVector);

    /** 区域地图纹理（美术预渲染的俯视图） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
    TSoftObjectPtr<UTexture2D> MapTexture;

    /** 该区域在完整世界地图纹理上的 UV 矩形 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
    FBox2D MapUVRect = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
};

/**
 * 世界地图配置数据资产
 * 所有的 POI、区域、地图纹理等静态数据集中在此资产中配置
 * 运行时由 UMapManagerSubsystem 加载
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UWorldMapConfigDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 世界整体地图纹理（全屏地图使用） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WorldMap")
    TSoftObjectPtr<UTexture2D> WorldMapTexture;

    /** 世界整体 2D 边界 (cm)，用于世界坐标 ↔ 地图 UV 转换 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WorldMap")
    FBox2D WorldBounds = FBox2D(FVector2D(-50000.0f, -50000.0f), FVector2D(50000.0f, 50000.0f));

    /** 所有 POI 列表 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WorldMap|POI")
    TArray<FMapPOIData> POIs;

    /** 所有区域列表 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WorldMap|Region")
    TArray<FMapRegionData> Regions;

    /** 小地图默认显示范围 (cm)，即小地图覆盖的世界半径 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WorldMap|Minimap", meta = (ClampMin = "1000.0"))
    float MinimapViewRadius = 5000.0f;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("WorldMapConfig", GetFName());
    }
};
