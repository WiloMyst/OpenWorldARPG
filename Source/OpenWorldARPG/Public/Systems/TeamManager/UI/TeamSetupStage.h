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
 * 仅持有 4 个纯展示用 SkeletalMeshComponent，不挂载 GAS / CMC。
 * 通过 RefreshStage 接收 Tag 数组，从 CharacterManagerSubsystem 拉取 VisualDataAsset 同步加载。
 * 由 UTeamSetupScreenWidget 动态 Spawn / Destroy，不依赖场景预设实例。
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API ATeamSetupStage : public AActor
{
    GENERATED_BODY()

public:
    ATeamSetupStage();

    /**
     * 刷新展台：从 CharacterManagerSubsystem 拉取 VisualDataAsset 并加载 Mesh/AnimBP。
     * 缺失槽位被隐藏。
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetupStage")
    void RefreshStage(const TArray<FGameplayTag>& TeamTags);

    /** 旋转所有槽位锚点（供 UI 拖拽调用） */
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
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Components")
    TObjectPtr<UCameraComponent> StageCamera;

    // --- 槽位锚点 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Slots")
    TArray<TObjectPtr<USceneComponent>> SlotAnchors;

    // --- 展示网格体 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Slots")
    TArray<TObjectPtr<USkeletalMeshComponent>> SlotMeshes;

    // --- 三点布光 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Lighting")
    TObjectPtr<USpotLightComponent> KeyLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Lighting")
    TObjectPtr<USpotLightComponent> FillLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Lighting")
    TObjectPtr<USpotLightComponent> RimLight;

    // --- 环境 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSetupStage|Environment")
    TObjectPtr<UStaticMeshComponent> BackdropMesh;

private:
    /** 初始化 4 个槽位锚点和网格体（构造函数中调用） */
    void InitializeSlots();
};
