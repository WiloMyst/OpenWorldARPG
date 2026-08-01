// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MapSystem/UI/WorldMapWidget.h"
#include "Systems/MapSystem/MapManagerSubsystem.h"
#include "UI/Core/UIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"

void UWorldMapWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (const ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        MapSubsystem = LP->GetSubsystem<UMapManagerSubsystem>();
        if (MapSubsystem)
        {
            // 绑定数据变更委托
            MapSubsystem->OnMapDataChanged.AddDynamic(this, &UWorldMapWidget::OnMapDataChanged);

            // 初始拉取数据
            RefreshMapData();
        }
    }
}

void UWorldMapWidget::NativeDestruct()
{
    if (MapSubsystem)
    {
        MapSubsystem->OnMapDataChanged.RemoveAll(this);
    }
    Super::NativeDestruct();
}

// ============================================================================
// 数据刷新
// ============================================================================

void UWorldMapWidget::RefreshMapData()
{
    if (!MapSubsystem) return;

    // 拉取已发现 POI 和区域
    MapSubsystem->GetDiscoveredPOIs(DiscoveredPOIs);
    MapSubsystem->GetDiscoveredRegions(DiscoveredRegions);

    // 拉取地图配置（纹理、边界）
    if (UWorldMapConfigDataAsset* Config = MapSubsystem->GetMapConfig())
    {
        if (Config->WorldMapTexture.IsValid())
        {
            WorldMapTexture = Config->WorldMapTexture.Get();
        }
        else if (!Config->WorldMapTexture.IsNull())
        {
            WorldMapTexture = Config->WorldMapTexture.LoadSynchronous();
        }
        WorldBounds = Config->WorldBounds;
    }
}

void UWorldMapWidget::OnMapDataChanged()
{
    RefreshMapData();
}

// ============================================================================
// POI 查询与选中
// ============================================================================

FVector2D UWorldMapWidget::GetPOIMapUV(FGameplayTag POITag) const
{
    if (!MapSubsystem) return FVector2D(0.5f, 0.5f);

    FMapPOIData POI;
    if (MapSubsystem->GetPOIByTag(POITag, POI))
    {
        return MapSubsystem->WorldToMapUV(POI.WorldLocation);
    }
    return FVector2D(0.5f, 0.5f);
}

bool UWorldMapWidget::GetSelectedPOI(FMapPOIData& OutPOI) const
{
    if (!SelectedPOITag.IsValid() || !MapSubsystem) return false;
    return MapSubsystem->GetPOIByTag(SelectedPOITag, OutPOI);
}

void UWorldMapWidget::SelectPOI(FGameplayTag POITag)
{
    SelectedPOITag = POITag;
}

bool UWorldMapWidget::CanTravelToPOI(FGameplayTag POITag) const
{
    if (!MapSubsystem) return false;
    return MapSubsystem->CanFastTravel(POITag);
}

// ============================================================================
// 快速旅行
// ============================================================================

bool UWorldMapWidget::RequestTravelToSelected()
{
    if (!SelectedPOITag.IsValid()) return false;
    return RequestTravelToPOI(SelectedPOITag);
}

bool UWorldMapWidget::RequestTravelToPOI(FGameplayTag POITag)
{
    if (!MapSubsystem) return false;

    if (MapSubsystem->RequestFastTravel(POITag))
    {
        CloseWorldMap();
        return true;
    }
    return false;
}

// ============================================================================
// 关闭地图
// ============================================================================

void UWorldMapWidget::CloseWorldMap()
{
    // 通过 UIManager 关闭自身
    if (const ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        if (UUIManagerSubsystem* UIManager = LP->GetSubsystem<UUIManagerSubsystem>())
        {
            // UIManager 负责关闭逻辑，这里仅触发
            // 具体的关闭行为取决于 UIManager 的实现
            RemoveFromParent();
        }
        else
        {
            RemoveFromParent();
        }
    }
    else
    {
        RemoveFromParent();
    }
}
