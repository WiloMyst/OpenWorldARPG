// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterShowcaseStage.generated.h"

class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UCharacterVisualDataAsset;
class UAnimInstance;
struct FStreamableHandle;

/**
 * 角色 3D 展台（Diorama）。
 *
 * 【架构原则：纯净黑盒】
 * - 仅包含纯展示用的 SkeletalMeshComponent，不挂载任何战斗逻辑组件（GAS/CMC）。
 * - 禁用碰撞、禁用物理，纯粹用于 UI 摄像机渲染角色待机动画。
 * - 对外暴露 SwitchDisplayCharacter 和 RotateCharacter 两个接口，
 *   供 UI 主壳子和 PlayerController 调用。
 *
 * 【数据流向】
 * UI 不直接修改数据 → 通知 ShowcaseStage 切换模型 → ShowcaseStage 从 VisualDataAsset 异步加载 Mesh 和同步加载 AnimBP
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API ACharacterShowcaseStage : public AActor
{
    GENERATED_BODY()

public:
    ACharacterShowcaseStage();

    /**
     * 切换展台展示的角色。
     * 异步加载 SkeletalMesh，同步加载动画蓝图，赋值给 DisplayMesh。
     * @param VisualData 角色外观数据资产（包含 Mesh 和 AnimBP 软引用）
     */
    UFUNCTION(BlueprintCallable, Category = "CharacterShowcase")
    void SwitchDisplayCharacter(const UCharacterVisualDataAsset* VisualData);

    /**
     * 旋转展台上的角色模型（供鼠标拖拽调用）。
     * @param DeltaYaw Yaw 旋转增量（度）
     */
    UFUNCTION(BlueprintCallable, Category = "CharacterShowcase")
    void RotateCharacter(float DeltaYaw);

    /** 获取展台摄像机（供 PlayerController 调用 SetViewTargetWithBlend） */
    UCameraComponent* GetShowcaseCamera() const { return ShowcaseCamera; }

protected:
    virtual void BeginPlay() override;

    /** 异步加载骨骼网格体完成的回调 */
    void OnMeshLoaded();

    // --- 组件 ---

    /** 根组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<USceneComponent> Root;

    /** 纯展示网格体（禁用碰撞、禁用物理、不挂载逻辑组件） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<USkeletalMeshComponent> DisplayMesh;

    /** 相机弹簧臂 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<USpringArmComponent> CameraBoom;

    /** 展台摄像机 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<UCameraComponent> ShowcaseCamera;

    // --- 展台环境 ---

    /** 展台纯色背景幕布（深灰色，禁用碰撞/物理/投影，纯视觉） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Environment")
    TObjectPtr<UStaticMeshComponent> BackdropMesh;

    // --- 打光 ---

    /** 主光（Key Light）：正面偏上方打光 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components|Lighting")
    TObjectPtr<USpotLightComponent> KeyLight;

    /** 补光（Fill Light）：侧面柔光 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components|Lighting")
    TObjectPtr<USpotLightComponent> FillLight;

    /** 轮廓光（Rim/Back Light）：背后打光，勾勒角色轮廓 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components|Lighting")
    TObjectPtr<USpotLightComponent> RimLight;

    // --- 异步加载状态 ---

    /** 当前正在异步加载的 VisualData（弱引用，避免影响外部生命周期） */
    UPROPERTY(Transient)
    TWeakObjectPtr<const UCharacterVisualDataAsset> PendingVisualData;

    /** 异步加载句柄（保持存活直到加载完成，避免被提前 GC） */
    TSharedPtr<FStreamableHandle> MeshStreamingHandle;
};
