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
 * 纯展示用 SkeletalMeshComponent，不挂 GAS / CMC，禁用碰撞和物理。
 * 对外暴露 SwitchDisplayCharacter 和 RotateCharacter 两个接口。
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API ACharacterShowcaseStage : public AActor
{
    GENERATED_BODY()

public:
    ACharacterShowcaseStage();

    /** 切换展示角色：异步加载 Mesh，同步加载 AnimBP */
    UFUNCTION(BlueprintCallable, Category = "CharacterShowcase")
    void SwitchDisplayCharacter(const UCharacterVisualDataAsset* VisualData);

    /** 旋转角色（供鼠标拖拽调用） */
    UFUNCTION(BlueprintCallable, Category = "CharacterShowcase")
    void RotateCharacter(float DeltaYaw);

    /** 获取展台摄像机（供 PlayerController 调用 SetViewTargetWithBlend） */
    UCameraComponent* GetShowcaseCamera() const { return ShowcaseCamera; }

protected:
    virtual void BeginPlay() override;

    /** 异步加载骨骼网格体完成的回调 */
    void OnMeshLoaded();

    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<USkeletalMeshComponent> DisplayMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components")
    TObjectPtr<UCameraComponent> ShowcaseCamera;

    // --- 环境 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Environment")
    TObjectPtr<UStaticMeshComponent> BackdropMesh;

    // --- 三点布光 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components|Lighting")
    TObjectPtr<USpotLightComponent> KeyLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components|Lighting")
    TObjectPtr<USpotLightComponent> FillLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Showcase|Components|Lighting")
    TObjectPtr<USpotLightComponent> RimLight;

    // --- 异步加载状态 ---

    UPROPERTY(Transient)
    TWeakObjectPtr<const UCharacterVisualDataAsset> PendingVisualData;

    TSharedPtr<FStreamableHandle> MeshStreamingHandle;
};
