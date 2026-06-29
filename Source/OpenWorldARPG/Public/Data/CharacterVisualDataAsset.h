// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapons/WeaponBase.h"
#include "CharacterVisualDataAsset.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
class APlayerCharacter;

/**
 * 角色外观表现数据资产。仅存储用于 3D 渲染和动画表现的资源。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UCharacterVisualDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 骨骼与动画 ---

    /**
     * 角色的基础蓝图类（按体型区分，例如 BP_Player_MaleAdult, BP_Player_Loli）。
     * 运行时将根据此配置生成对应的 Actor 实体。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Blueprint", meta = (DisplayName = "角色蓝图类"))
    TSoftClassPtr<APlayerCharacter> CharacterBlueprint;

    /** 角色骨骼网格体（软引用，运行时异步加载） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Mesh", meta = (DisplayName = "角色骨骼网格体"))
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    /** 角色动画蓝图（软类引用，避免硬加载 AnimBP） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "角色动画蓝图"))
    TSoftClassPtr<UAnimInstance> AnimationBlueprint;

    /**
     * 3D 展台专用展示动画蓝图（软类引用）。
     * 为 UI 展台（角色界面、编队界面）定制的 AnimBP，内部状态机仅保留待机/站姿/呼吸等展示状态，
     * 不包含 GAS 战斗逻辑。若未配置则回退使用 AnimationBlueprint。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "展示动画蓝图"))
    TSoftClassPtr<UAnimInstance> ShowcaseAnimationBlueprint;

    /** 角色上半身动画层蓝图 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "上半身动画层"))
    TSoftClassPtr<UAnimInstance> UpperBodyLayers;

    // --- 武器 ---

    /** 角色武器蓝图（软类引用，避免硬加载武器类） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Weapon", meta = (DisplayName = "角色武器蓝图"))
    TSoftClassPtr<AWeaponBase> WeaponBlueprint;

    // --- 攀爬翻越 ---

    /** 攀爬翻越蒙太奇 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Climbing", meta = (DisplayName = "攀爬翻越蒙太奇"))
    TSoftObjectPtr<UAnimMontage> ClimbUpMontage;

    /** 攀爬翻越位移偏移（角色从攀爬边缘翻上后的目标位置偏移） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Climbing", meta = (DisplayName = "攀爬翻越位移偏移"))
    FVector ClimbUpOffset = FVector(80.0f, 0.0f, 70.0f);

    // --- 钩索 ---

    /** 钩索蒙太奇 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Grapple", meta = (DisplayName = "钩索蒙太奇"))
    TSoftObjectPtr<UAnimMontage> GrappleMontage;
};
