// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CharacterDataAsset.generated.h"

class UTexture2D;
class USkeletalMesh;
class AWeaponBase;
class UCurveTable;
class UGameplayAbility;
class UGameplayEffect;

/**
 * 天赋技能配置。
 */
USTRUCT(BlueprintType)
struct FTalentConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能Tag"))
    FGameplayTag TalentTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能图标"))
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能GA"))
    TArray<TSubclassOf<UGameplayAbility>> AbilityClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋动画蒙太奇"))
    TArray<TSoftObjectPtr<UAnimMontage>> Montages;
};


/**
 * 角色静态配置数据资产。不含运行时动态数据（等级、经验值等）。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UCharacterDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 身份信息 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色Tag"))
    FGameplayTag CharacterTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色名称"))
    FText CharacterName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色头衔"))
    FText CharacterTitle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色头像"))
    TSoftObjectPtr<UTexture2D> HeadIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色立绘"))
    TSoftObjectPtr<UTexture2D> SplashArt;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色稀有度"))
    int32 Rarity = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色所属元素"))
    FGameplayTag ElementType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色武器类型"))
    FGameplayTag WeaponType;


    // --- 外观与动画 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色骨骼网格体"))
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色动画蓝图"))
    TSubclassOf<UAnimInstance> AnimationBlueprint;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色基础行为动画层蓝图"))
    TSubclassOf<UAnimInstance> BaseBehaviorAnimLayers;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色瞄准动画层蓝图"))
    TSubclassOf<UAnimInstance> AimAnimLayers;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色物理模拟动画层蓝图"))
    TSubclassOf<UAnimInstance> PhysicsAnimLayers;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色武器蓝图"))
    TSubclassOf<AWeaponBase> WeaponBlueprint;


    // --- 战斗属性 GE ---
    TSubclassOf<UGameplayEffect> BaseAttributesEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Attributes", meta = (DisplayName = "角色突破增益GE"))
    TMap<int32, TSubclassOf<UGameplayEffect>> AscensionBonusEffects;


    // --- 战斗天赋 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色普攻"))
    FTalentConfig NormalAttack;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色重击"))
    FTalentConfig HeavyAttack;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色下落攻击"))
    FTalentConfig PlungeAttack;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色战技攻击"))
    FTalentConfig SkillAttack;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色终结技攻击"))
    FTalentConfig UltimateAttack;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色所有被动天赋"))
    TArray<FTalentConfig> PassiveTalents;


    // --- 攀爬动画 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals | Climbing", meta = (DisplayName = "攀爬翻越蒙太奇"))
    TSoftObjectPtr<UAnimMontage> ClimbUpMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals | Climbing", meta = (DisplayName = "攀爬翻越位移偏移"))
    FVector ClimbUpOffset = FVector(80.0f, 0.0f, 70.0f);

};