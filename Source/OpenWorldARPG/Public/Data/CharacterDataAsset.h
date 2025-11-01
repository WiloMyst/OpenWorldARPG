// Fill out your copyright notice in the Description page of Project Settings.

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
 * @struct FTalentConfig
 * @brief 定义了一个角色天赋技能的详细配置
 */
USTRUCT(BlueprintType)
struct FTalentConfig
{
    GENERATED_BODY()

    // 该天赋的GameplayTag，例如 "Ability.Talent.NormalAttack"
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能Tag"))
    FGameplayTag TalentTag;

    // 该天赋在UI中显示的图标
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能图标"))
    TSoftObjectPtr<UTexture2D> Icon;

    // 该天赋对应的GameplayAbility蓝图
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能GA"))
    TArray<TSubclassOf<UGameplayAbility>> AbilityClasses;

    // 该天赋对应的动画蒙太奇
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋动画蒙太奇"))
    TArray<TSoftObjectPtr<UAnimMontage>> Montages;

    // 天赋等级对伤害/效果倍率的成长曲线
    // UCurveTable可以定义每一级（1-15级）的具体数值
    //UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能成长曲线"))
    //TSoftObjectPtr<UCurveTable> LevelUpMultiplierCurveTable;
};


/**
 * @class UCharacterDataAsset
 * @brief 定义一个角色所有静态、不变的配置数据。
 * 这是角色的“出厂设置”，不包含任何玩家存档相关的动态数据（如等级、经验值）。
 * 它继承自UPrimaryDataAsset，以便于资产管理器进行扫描和管理。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UCharacterDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ======== 角色身份信息 ========

    /** 角色GameplayTag */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色Tag"))
    FGameplayTag CharacterTag;

    /** 角色全名 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色名称"))
    FText CharacterName;

    /** 角色头衔 (例如: "一心净土") */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色头衔"))
    FText CharacterTitle;

    /** 角色头像（小尺寸）软指针 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色头像"))
    TSoftObjectPtr<UTexture2D> HeadIcon;

    /** 角色立绘（大尺寸）软指针 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色立绘"))
    TSoftObjectPtr<UTexture2D> SplashArt;

    /** 角色稀有度 (4星或5星) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色稀有度"))
    int32 Rarity = 5;

    /** 角色所属元素GameplayTag (例如: "Element.Electro") */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色所属元素"))
    FGameplayTag ElementType;

    /** 角色所属武器类型GameplayTag (例如: "Weapon.Sword", "Weapon.Polearm") */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (DisplayName = "角色武器类型"))
    FGameplayTag WeaponType;


    // ======== 角色外观与动画 ========

    /** 角色的骨骼网格体软指针 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色骨骼网格体"))
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    /** 角色的动画蓝图 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色动画蓝图"))
    TSubclassOf<UAnimInstance> AnimationBlueprint;

    /**
     * @brief 角色的基础行为动画层蓝图。
     * 用于实现角色各种状态中的动画层的附加动画逻辑。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色基础行为动画层蓝图"))
    TSubclassOf<UAnimInstance> BaseBehaviorAnimLayers;

    /**
     * @brief 角色的瞄准动画层蓝图。
     * 用于实现角色瞄准中的动画层的附加动画逻辑。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色瞄准动画层蓝图"))
    TSubclassOf<UAnimInstance> AimAnimLayers;

    /**
     * @brief 角色的物理模拟动画层蓝图。
     * 用于实现布料模拟、物理骨骼等与特定模型相关的附加动画逻辑。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色物理模拟动画层蓝图"))
    TSubclassOf<UAnimInstance> PhysicsAnimLayers;

    /** 角色的武器子类 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (DisplayName = "角色武器蓝图"))
    TSubclassOf<AWeaponBase> WeaponBlueprint;


    // ======== 角色成长曲线 ========

    /**
     * @brief 定义角色从1级到90级，每一级的基础属性值。
     * CurveTable的每一行代表一个属性（如BaseHP, BaseATK, BaseDEF），
     * 每一行中的曲线定义了该属性随等级的变化。
     */
    //UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (DisplayName = "角色成长曲线"))
    //TSoftObjectPtr<UCurveTable> BaseStatsGrowthCurveTable;

    // ======== 角色战斗属性GE加成 ========

    /**
     * @brief 用于初始化角色1级属性的GameplayEffect。
     * 这个GE会定义角色在1级时的基础生命、攻击、防御等。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Attributes", meta = (DisplayName = "角色1级属性GE"))
    TSubclassOf<UGameplayEffect> BaseAttributesEffect;

    /**
     * @brief 角色突破增加的额外属性。
     * TMap<int32, TSubclassOf<UGameplayEffect>>，其中Key是突破等级(1-6)，Value是突破后应用的永久性属性增益GE。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Attributes", meta = (DisplayName = "角色突破增益GE"))
    TMap<int32, TSubclassOf<UGameplayEffect>> AscensionBonusEffects;


    // ======== 角色战斗天赋（技能） ========

    /**
     * @brief 定义角色的普通攻击。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色普攻"))
    FTalentConfig NormalAttack;

    /**
     * @brief 定义角色的重击。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色重击"))
    FTalentConfig HeavyAttack;

    /**
     * @brief 定义角色的下落攻击。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色下落攻击"))
    FTalentConfig PlungeAttack;

    /**
     * @brief 定义角色的战技攻击。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色战技攻击"))
    FTalentConfig SkillAttack;

    /**
     * @brief 定义角色的终结技攻击。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色终结技攻击"))
    FTalentConfig UltimateAttack;

    /**
     * @brief 角色固有的被动天赋。
     * 这些能力在角色被赋予时就会自动激活，不需要玩家主动释放。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS | Abilities", meta = (DisplayName = "角色所有被动天赋"))
    TArray<FTalentConfig> PassiveTalents;

};