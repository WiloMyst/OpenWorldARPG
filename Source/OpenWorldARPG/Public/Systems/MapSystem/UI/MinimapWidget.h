// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Systems/MapSystem/Data/WorldMapConfigDataAsset.h"
#include "MinimapWidget.generated.h"

class UMapManagerSubsystem;

/** 小地图旋转模式 */
UENUM(BlueprintType)
enum class EMinimapRotationMode : uint8
{
    /** 北朝上：地图不旋转，玩家箭头随朝向旋转 */
    NorthUp      UMETA(DisplayName = "北朝上"),
    /** 车头朝上：地图随玩家旋转，玩家箭头始终朝上 */
    HeadingUp    UMETA(DisplayName = "车头朝上")
};

/** 小地图标记显示数据（每帧由 C++ 计算，供蓝图读取） */
USTRUCT(BlueprintType)
struct FMinimapMarkerData
{
    GENERATED_BODY()

    /** POI 标识 */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap")
    FGameplayTag POITag;

    /** 显示名称 */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap")
    FText DisplayName;

    /** POI 类型 */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap")
    EMapPOIType POIType = EMapPOIType::Custom;

    /**
     * 归一化位置 (-1~1)，(0,0) = 小地图中心
     * X = 前后（正=前方/上方），Y = 左右（正=右方）
     */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap")
    FVector2D NormalizedPosition = FVector2D::ZeroVector;

    /** 是否在小地图视野范围内 */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap")
    bool bIsInView = false;

    /** 是否可快速旅行 */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap")
    bool bCanFastTravel = false;
};

/**
 * 小地图 Widget 基类（对应 WBP_Minimap）
 *
 * C++ 职责：
 * - 每帧更新玩家位置和朝向
 * - 计算所有已发现 POI 的归一化小地图位置
 * - 支持北朝上 / 车头朝上两种旋转模式
 * - 视野外 POI 钳制到边缘（供蓝图显示方向箭头）
 *
 * 蓝图职责：
 * - 绑定 WBP_Minimap 到 MainWorldHUDLayout 的 WBP_Minimap 槽
 * - 每帧读取 Markers 数组，更新标记位置
 * - 读取 PlayerYaw 旋转玩家箭头（北朝上模式）或地图（车头朝上模式）
 * - 根据 POIType 设置标记图标颜色
 * - 视野外标记显示为边缘方向箭头
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UMinimapWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

    // --- 配置 ---

    /** 小地图视野半径 (cm)，即小地图覆盖的世界半径 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap|Config")
    float ViewRadius = 5000.0f;

    /** 旋转模式 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap|Config")
    EMinimapRotationMode RotationMode = EMinimapRotationMode::NorthUp;

    // --- 实时数据（供蓝图每帧读取） ---

    /** 玩家朝向角度 (Yaw, 0=北) */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap|Data")
    float PlayerYaw = 0.0f;

    /** 玩家世界位置 2D */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap|Data")
    FVector2D PlayerWorldPos2D = FVector2D::ZeroVector;

    /** 小地图标记数据列表（每帧更新） */
    UPROPERTY(BlueprintReadOnly, Category = "Minimap|Data")
    TArray<FMinimapMarkerData> Markers;

private:
    UPROPERTY(Transient)
    TObjectPtr<UMapManagerSubsystem> MapSubsystem;

    /** 缓存的已发现 POI 列表（仅数据变更时刷新） */
    TArray<FMapPOIData> CachedDiscoveredPOIs;

    /** 地图数据变更回调 */
    UFUNCTION()
    void OnMapDataChanged();

    /** 重新计算所有标记的位置 */
    void UpdateMarkers();
};
