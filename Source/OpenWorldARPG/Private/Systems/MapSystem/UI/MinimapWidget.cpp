// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MapSystem/UI/MinimapWidget.h"
#include "Systems/MapSystem/MapManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"

void UMinimapWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (const ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        MapSubsystem = LP->GetSubsystem<UMapManagerSubsystem>();
        if (MapSubsystem)
        {
            // 绑定数据变更委托
            MapSubsystem->OnMapDataChanged.AddDynamic(this, &UMinimapWidget::OnMapDataChanged);

            // 初始拉取已发现 POI
            OnMapDataChanged();
        }
    }
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
    Super::NativeTick(MyGeometry, DeltaTime);

    if (!MapSubsystem) return;

    // 更新玩家位置和朝向
    PlayerWorldPos2D = MapSubsystem->GetPlayerWorldPosition2D();
    PlayerYaw = MapSubsystem->GetPlayerFacingAngle();

    // 重新计算标记位置（基于缓存的 POI 列表）
    UpdateMarkers();
}

void UMinimapWidget::OnMapDataChanged()
{
    if (MapSubsystem)
    {
        MapSubsystem->GetDiscoveredPOIs(CachedDiscoveredPOIs);
    }
}

void UMinimapWidget::UpdateMarkers()
{
    Markers.Reset(CachedDiscoveredPOIs.Num());

    const float YawRad = FMath::DegreesToRadians(PlayerYaw);
    const float CosYaw = FMath::Cos(YawRad);
    const float SinYaw = FMath::Sin(YawRad);
    const float InvViewRadius = 1.0f / FMath::Max(ViewRadius, KINDA_SMALL_NUMBER);

    for (const FMapPOIData& POI : CachedDiscoveredPOIs)
    {
        FMinimapMarkerData& Marker = Markers.AddDefaulted_GetRef();
        Marker.POITag = POI.POITag;
        Marker.DisplayName = POI.DisplayName;
        Marker.POIType = POI.Type;
        Marker.bCanFastTravel = MapSubsystem->CanFastTravel(POI.POITag);

        // 世界坐标 → 相对玩家的偏移
        const FVector2D Relative(POI.WorldLocation.X - PlayerWorldPos2D.X,
                                  POI.WorldLocation.Y - PlayerWorldPos2D.Y);

        FVector2D Normalized;

        if (RotationMode == EMinimapRotationMode::HeadingUp)
        {
            // 车头朝上：将世界偏移转到玩家本地空间
            // Forward = (cos(Yaw), sin(Yaw)), Right = (-sin(Yaw), cos(Yaw))
            const float LocalX = Relative.X * CosYaw + Relative.Y * SinYaw;   // 前方距离
            const float LocalY = -Relative.X * SinYaw + Relative.Y * CosYaw;  // 右方距离
            Normalized = FVector2D(LocalX * InvViewRadius, LocalY * InvViewRadius);
        }
        else
        {
            // 北朝上：直接用世界坐标偏移
            Normalized = FVector2D(Relative.X * InvViewRadius, Relative.Y * InvViewRadius);
        }

        // 检查是否在视野范围内
        const float DistSq = Normalized.SizeSquared();
        Marker.bIsInView = (DistSq <= 1.0f);

        // 视野外：钳制到边缘（归一化为单位向量）
        if (!Marker.bIsInView && DistSq > KINDA_SMALL_NUMBER)
        {
            const float Dist = FMath::Sqrt(DistSq);
            Normalized /= Dist;
        }

        Marker.NormalizedPosition = Normalized;
    }
}
