// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "TeamSetupStage.generated.h"

class USceneComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * 3D 编队展台 Actor（Diorama 黑盒）。
 *
 * 【设计原则】
 * - 严禁挂载 GAS / CharacterMovement 等战斗逻辑组件。
 * - 仅持有 4 个纯展示用 SkeletalMeshComponent，用于渲染编队排排站的 4 名角色。
 * - 通过 RefreshStage 接收 Tag 数组，从 CharacterManagerSubsystem 拉取 VisualDataAsset，
 *   同步加载骨骼网格体和动画蓝图并赋值给对应槽位。
 * - 不缓存任何冗余的存档数据，所有数据每次刷新都从 Subsystem 实时拉取。
 *
 * 【生命周期】
 * 由 UTeamSetupScreenWidget 在 NativeConstruct 中动态 SpawnActor 生成，
 * 在 NativeDestruct 中 Destroy。不依赖场景中预先放置的实例。
 *
 * 【槽位布局】
 * Slot0 / Slot1 / Slot2 / Slot3 横向排开，类似原神编队界面站位。
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API ATeamSetupStage : public AActor
{
    GENERATED_BODY()

public:
    ATeamSetupStage();

    /**
     * 刷新编队展台显示。
     * 遍历 TeamTags，从 CharacterManagerSubsystem 拉取每个角色的 VisualDataAsset，
     * 同步加载骨骼网格体和动画蓝图并赋值给对应槽位。
     * 缺失的槽位（Tag 无效或数组长度不足）将被隐藏。
     *
     * @param TeamTags 编队角色 Tag 数组（最多支持 4 个）
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetupStage")
    void RefreshStage(const TArray<FGameplayTag>& TeamTags);

    /**
     * 旋转展台上的所有角色（供 UI 拖拽调用）。
     * 所有槽位锚点围绕 Z 轴同步旋转，摄像机保持不动。
     *
     * @param DeltaYaw Yaw 旋转增量（度）
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetupStage")
    void RotateCharacters(float DeltaYaw);

    /** 获取展台摄像机（供 UI 切换 ViewTarget） */
    UFUNCTION(BlueprintPure, Category = "TeamSetupStage")
    UCameraComponent* GetStageCamera() const { return StageCamera; }

    /** 获取指定槽位的展示网格体 */
    UFUNCTION(BlueprintPure, Category = "TeamSetupStage")
    USkeletalMeshComponent* GetSlotMesh(int32 SlotIndex) const;

    /** 编队最大槽位数 */
    static constexpr int32 MaxTeamSize = 4;

protected:
    // --- 基础组件 ---

    /** 根组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Components")
    TObjectPtr<USceneComponent> Root;

    /** 展台摄像机（正对 4 个角色） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Components")
    TObjectPtr<UCameraComponent> StageCamera;

    // --- 槽位锚点（横向排开） ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Slots")
    TArray<TObjectPtr<USceneComponent>> SlotAnchors;

    // --- 展示网格体（纯渲染，无逻辑） ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Slots")
    TArray<TObjectPtr<USkeletalMeshComponent>> SlotMeshes;

    // --- 打光（三点布光：主光 / 补光 / 轮廓光） ---

    /** 主光（Key Light） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Lighting")
    TObjectPtr<USpotLightComponent> KeyLight;

    /** 补光（Fill Light） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Lighting")
    TObjectPtr<USpotLightComponent> FillLight;

    /** 轮廓光（Rim/Back Light） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Lighting")
    TObjectPtr<USpotLightComponent> RimLight;

    // --- 展台环境 ---

    /** 展台纯色背景幕布（深灰色，禁用碰撞/物理/投影，纯视觉） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Environment")
    TObjectPtr<UStaticMeshComponent> BackdropMesh;

private:
    /** 初始化 4 个槽位锚点和网格体（构造函数中调用） */
    void InitializeSlots();
};
