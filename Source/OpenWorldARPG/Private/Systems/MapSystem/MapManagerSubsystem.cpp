// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/MapSystem/MapManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// ============================================================================
// 子系统生命周期
// ============================================================================

void UMapManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UMapManagerSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

// ============================================================================
// 配置加载
// ============================================================================

bool UMapManagerSubsystem::LoadMapConfig(TSoftObjectPtr<UWorldMapConfigDataAsset> ConfigPath)
{
    if (!ConfigPath.IsValid() && !ConfigPath.IsPending()) return false;

    MapConfig = ConfigPath.LoadSynchronous();
    if (MapConfig)
    {
        // 初始解锁所有不需要解锁条件的 POI
        for (const FMapPOIData& POI : MapConfig->POIs)
        {
            if (!POI.bRequiresUnlock)
            {
                UnlockedPOITags.Add(POI.POITag);
            }
        }

        OnMapDataChanged.Broadcast();
        return true;
    }
    return false;
}

// ============================================================================
// POI 查询
// ============================================================================

void UMapManagerSubsystem::GetAllPOIs(TArray<FMapPOIData>& OutPOIs) const
{
    OutPOIs.Reset();
    if (MapConfig)
    {
        OutPOIs = MapConfig->POIs;
    }
}

void UMapManagerSubsystem::GetDiscoveredPOIs(TArray<FMapPOIData>& OutPOIs) const
{
    OutPOIs.Reset();
    if (!MapConfig) return;

    for (const FMapPOIData& POI : MapConfig->POIs)
    {
        if (DiscoveredPOITags.Contains(POI.POITag))
        {
            OutPOIs.Add(POI);
        }
    }
}

void UMapManagerSubsystem::GetPOIsByType(EMapPOIType Type, TArray<FMapPOIData>& OutPOIs) const
{
    OutPOIs.Reset();
    if (!MapConfig) return;

    for (const FMapPOIData& POI : MapConfig->POIs)
    {
        if (POI.Type == Type)
        {
            OutPOIs.Add(POI);
        }
    }
}

bool UMapManagerSubsystem::GetPOIByTag(FGameplayTag POITag, FMapPOIData& OutPOI) const
{
    if (!MapConfig) return false;

    for (const FMapPOIData& POI : MapConfig->POIs)
    {
        if (POI.POITag == POITag)
        {
            OutPOI = POI;
            return true;
        }
    }
    return false;
}

// ============================================================================
// POI 发现/解锁
// ============================================================================

bool UMapManagerSubsystem::DiscoverPOI(FGameplayTag POITag)
{
    if (!POITag.IsValid() || !MapConfig) return false;

    // 验证 Tag 存在
    bool bFound = false;
    for (const FMapPOIData& POI : MapConfig->POIs)
    {
        if (POI.POITag == POITag)
        {
            bFound = true;
            break;
        }
    }
    if (!bFound) return false;

    if (DiscoveredPOITags.Contains(POITag)) return false; // 已发现

    DiscoveredPOITags.Add(POITag);
    OnPOIDiscovered.Broadcast(POITag);
    OnMapDataChanged.Broadcast();
    return true;
}

bool UMapManagerSubsystem::IsPOIDiscovered(FGameplayTag POITag) const
{
    return DiscoveredPOITags.Contains(POITag);
}

bool UMapManagerSubsystem::UnlockPOI(FGameplayTag POITag)
{
    if (!POITag.IsValid()) return false;

    if (UnlockedPOITags.Contains(POITag)) return false; // 已解锁

    UnlockedPOITags.Add(POITag);
    OnMapDataChanged.Broadcast();
    return true;
}

bool UMapManagerSubsystem::IsPOIUnlocked(FGameplayTag POITag) const
{
    return UnlockedPOITags.Contains(POITag);
}

void UMapManagerSubsystem::UpdateProximityDiscovery(FVector PlayerLocation)
{
    if (!MapConfig) return;

    // 移动距离不够则跳过（性能优化）
    if (FVector::DistSquared2D(PlayerLocation, LastProximityCheckLocation) <
        ProximityCheckMinDistance * ProximityCheckMinDistance)
    {
        return;
    }
    LastProximityCheckLocation = PlayerLocation;

    bool bAnyDiscovered = false;

    // 检查 POI 近距离发现
    for (const FMapPOIData& POI : MapConfig->POIs)
    {
        if (POI.DiscoverRadius > 0.0f && !DiscoveredPOITags.Contains(POI.POITag))
        {
            const float DistSq = FVector::DistSquared2D(PlayerLocation, POI.WorldLocation);
            if (DistSq <= POI.DiscoverRadius * POI.DiscoverRadius)
            {
                DiscoveredPOITags.Add(POI.POITag);
                OnPOIDiscovered.Broadcast(POI.POITag);
                bAnyDiscovered = true;
            }
        }
    }

    // 检查区域发现
    for (const FMapRegionData& Region : MapConfig->Regions)
    {
        if (!DiscoveredRegionTags.Contains(Region.RegionTag))
        {
            if (Region.WorldBounds.IsInside(FVector2D(PlayerLocation)))
            {
                DiscoveredRegionTags.Add(Region.RegionTag);
                OnRegionDiscovered.Broadcast(Region.RegionTag);
                bAnyDiscovered = true;
            }
        }
    }

    if (bAnyDiscovered)
    {
        OnMapDataChanged.Broadcast();
    }
}

// ============================================================================
// 区域查询/发现
// ============================================================================

void UMapManagerSubsystem::GetAllRegions(TArray<FMapRegionData>& OutRegions) const
{
    OutRegions.Reset();
    if (MapConfig)
    {
        OutRegions = MapConfig->Regions;
    }
}

void UMapManagerSubsystem::GetDiscoveredRegions(TArray<FMapRegionData>& OutRegions) const
{
    OutRegions.Reset();
    if (!MapConfig) return;
    for (const FMapRegionData& Region : MapConfig->Regions)
    {
        if (DiscoveredRegionTags.Contains(Region.RegionTag))
        {
            OutRegions.Add(Region);
        }
    }
}

bool UMapManagerSubsystem::DiscoverRegion(FGameplayTag RegionTag)
{
    if (!RegionTag.IsValid() || !MapConfig) return false;

    // 验证 Tag 存在
    bool bFound = false;
    for (const FMapRegionData& Region : MapConfig->Regions)
    {
        if (Region.RegionTag == RegionTag)
        {
            bFound = true;
            break;
        }
    }
    if (!bFound) return false;

    if (DiscoveredRegionTags.Contains(RegionTag)) return false;

    DiscoveredRegionTags.Add(RegionTag);
    OnRegionDiscovered.Broadcast(RegionTag);
    OnMapDataChanged.Broadcast();
    return true;
}

bool UMapManagerSubsystem::IsRegionDiscovered(FGameplayTag RegionTag) const
{
    return DiscoveredRegionTags.Contains(RegionTag);
}

// ============================================================================
// 坐标转换
// ============================================================================

FVector2D UMapManagerSubsystem::WorldToMapUV(FVector WorldLocation) const
{
    if (!MapConfig) return FVector2D(0.5f, 0.5f);

    const FBox2D& Bounds = MapConfig->WorldBounds;
    const FVector2D World2D(WorldLocation);

    if (Bounds.GetSize().SizeSquared() < KINDA_SMALL_NUMBER)
    {
        return FVector2D(0.5f, 0.5f);
    }

    // 归一化到 0~1
    float U = (World2D.X - Bounds.Min.X) / (Bounds.Max.X - Bounds.Min.X);
    float V = (World2D.Y - Bounds.Min.Y) / (Bounds.Max.Y - Bounds.Min.Y);

    return FVector2D(FMath::Clamp(U, 0.0f, 1.0f), FMath::Clamp(V, 0.0f, 1.0f));
}

FVector UMapManagerSubsystem::MapUVToWorld(FVector2D MapUV) const
{
    if (!MapConfig) return FVector::ZeroVector;

    const FBox2D& Bounds = MapConfig->WorldBounds;

    float WorldX = FMath::Lerp(Bounds.Min.X, Bounds.Max.X, MapUV.X);
    float WorldY = FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, MapUV.Y);

    return FVector(WorldX, WorldY, 0.0f);
}

// ============================================================================
// 玩家位置
// ============================================================================

FVector2D UMapManagerSubsystem::GetPlayerWorldPosition2D() const
{
    if (const ULocalPlayer* LP = GetLocalPlayer())
    {
        if (const APlayerController* PC = LP->GetPlayerController(GetWorld()))
        {
            if (const APawn* Pawn = PC->GetPawn())
            {
                return FVector2D(Pawn->GetActorLocation());
            }
        }
    }
    return FVector2D::ZeroVector;
}

float UMapManagerSubsystem::GetPlayerFacingAngle() const
{
    if (const ULocalPlayer* LP = GetLocalPlayer())
    {
        if (const APlayerController* PC = LP->GetPlayerController(GetWorld()))
        {
            if (const APawn* Pawn = PC->GetPawn())
            {
                return Pawn->GetActorRotation().Yaw;
            }
        }
    }
    return 0.0f;
}

FGameplayTag UMapManagerSubsystem::GetPlayerCurrentRegion() const
{
    if (!MapConfig) return FGameplayTag::EmptyTag;

    const FVector2D PlayerPos = GetPlayerWorldPosition2D();

    for (const FMapRegionData& Region : MapConfig->Regions)
    {
        if (Region.WorldBounds.IsInside(PlayerPos))
        {
            return Region.RegionTag;
        }
    }
    return FGameplayTag::EmptyTag;
}

// ============================================================================
// 快速旅行
// ============================================================================

bool UMapManagerSubsystem::CanFastTravel(FGameplayTag POITag) const
{
    if (!MapConfig) return false;

    FMapPOIData POI;
    if (!GetPOIByTag(POITag, POI)) return false;

    // 必须是传送点类型
    if (POI.Type != EMapPOIType::FastTravel) return false;

    // 必须已发现
    if (!IsPOIDiscovered(POITag)) return false;

    // 必须已解锁
    if (POI.bRequiresUnlock && !IsPOIUnlocked(POITag)) return false;

    return true;
}

bool UMapManagerSubsystem::RequestFastTravel(FGameplayTag POITag)
{
    if (!CanFastTravel(POITag)) return false;

    FMapPOIData POI;
    if (!GetPOIByTag(POITag, POI)) return false;

    // 同关卡传送：直接移动玩家
    // TODO: [Network Architecture] 跨关卡传送应通过 GameFlowSubsystem 的 ServerTravel 流程
    if (const ULocalPlayer* LP = GetLocalPlayer())
    {
        if (APlayerController* PC = LP->GetPlayerController(GetWorld()))
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                // 保持当前 Z 高度 + POI 的 Z
                FVector TargetLocation = POI.WorldLocation;
                TargetLocation.Z = Pawn->GetActorLocation().Z;

                Pawn->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
                return true;
            }
        }
    }
    return false;
}
