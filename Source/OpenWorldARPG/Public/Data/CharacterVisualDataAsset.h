// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Weapons/WeaponBase.h"
#include "CharacterVisualDataAsset.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;

/**
 * 角色外观表现数据资产。仅存储用于 3D 渲染和动画表现的资源。
 *
 * 【架构设计：UI / 表现 / 战斗 三层解耦】
 * 本资产是"表现层"的唯一载体，职责极度单一：
 * - 只存放骨骼网格体、动画蓝图、动画层、武器蓝图、攀爬蒙太奇等纯视觉数据
 * - 不包含任何 UI 展示字段（Name, Icon, Rarity 等）→ 由 FCharacterRegistryRow 负责
 * - 不包含任何战斗逻辑字段（Attributes, Talents 等）→ 由 UCharacterCombatDataAsset 负责
 *
 * 【极致内存管理：全软引用】
 * 所有资源引用均使用 TSoftObjectPtr / TSoftClassPtr，不产生硬引用加载。
 * 资产本身在编辑器中配置时只记录路径字符串，运行时由 GameAssetManagerSubsystem
 * 按需异步加载，避免将所有角色的 Mesh/AnimBP 常驻内存。
 *
 * 【Perforce 协同规范】
 * 本资产由美术人员负责签出和修改（调整 Mesh、AnimBP、蒙太奇等），
 * 与战斗策划签出的 UCharacterCombatDataAsset 完全独立，不会产生锁冲突。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UCharacterVisualDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 骨骼与动画 ---

    /** 角色骨骼网格体（软引用，运行时异步加载） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Mesh", meta = (DisplayName = "角色骨骼网格体"))
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    /** 角色动画蓝图（软类引用，避免硬加载 AnimBP） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "角色动画蓝图"))
    TSoftClassPtr<UAnimInstance> AnimationBlueprint;

    /** 角色基础行为动画层蓝图（待机/移动/跳跃等） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "基础行为动画层"))
    TSoftClassPtr<UAnimInstance> BaseBehaviorAnimLayers;

    /** 角色瞄准动画层蓝图（瞄准时叠加） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "瞄准动画层"))
    TSoftClassPtr<UAnimInstance> AimAnimLayers;

    /** 角色物理模拟动画层蓝图（布娃娃/受击等） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Animation", meta = (DisplayName = "物理模拟动画层"))
    TSoftClassPtr<UAnimInstance> PhysicsAnimLayers;

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
};
