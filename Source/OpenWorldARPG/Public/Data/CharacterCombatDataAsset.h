// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CharacterCombatDataAsset.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class UTexture2D;
class UAnimMontage;

/**
 * 连招动作节点。数据驱动的连招图基本单元。
 *
 * 设计思路：
 * - 每个节点代表一段攻击动作（如"普攻第1段"、"重击"）
 * - NextNodes 是派生表：Key = 玩家输入 Tag（如 Input.Attack.Normal），
 *   Value = 下一个动作的节点名称（对应 ComboGraph 中的 Key）
 * - 这种设计支持：普攻链、普攻→重击派生、重击→特殊技派生等任意连招拓扑
 *
 * 数据与表现分离：
 * - 每个节点携带自己的伤害 GE，不同段攻击可以有不同的伤害倍率/效果
 * - 每个节点配置自己的 MotionWarpDistance，不同段攻击的滑步距离可以不同
 * - GA 不再硬编码任何伤害逻辑，完全由数据驱动
 *
 * 蒙太奇配置：
 * - 战斗连招蒙太奇直接在节点中用 TSoftObjectPtr 配置（策划在连招图中设置）
 * - 非战斗蒙太奇（攀爬、钩索等）在 VisualDataAsset 中配置
 */
USTRUCT(BlueprintType)
struct FComboActionNode
{
    GENERATED_BODY()

    /** 本节点播放的蒙太奇（软引用，运行时异步加载） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSoftObjectPtr<UAnimMontage> Montage;

    /**
     * 本节点附带的伤害 GameplayEffect。
     * 如果不设置，GA 会回退使用 GA 自身的 DamageEffectClass。
     * 不同段攻击可以配置不同的伤害倍率/效果（如终结段伤害更高）。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> DamageEffect;

    /** 本段攻击期望的滑步吸附距离上限（cm）。Motion Warping 会将角色向目标方向吸附，但不超过此距离 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    float MaxWarpDistance = 200.0f;

    /**
     * 连招派生表。
     * Key = 输入 Tag（如 Input.Attack.Normal、Input.Attack.Heavy）
     * Value = 下一个动作的节点名称（对应 ComboGraph 中的 Key）
     *
     * 示例配置：
     *   { Input.Attack.Normal → "Combo_02" }  普攻接普攻
     *   { Input.Attack.Heavy  → "Heavy_01" }  普攻接重击
     *   空表 = 连招终点，动画结束后 EndAbility
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TMap<FGameplayTag, FName> NextNodes;
};

/**
 * 天赋技能配置。
 *
 * 【架构设计：消除二义性】
 * 删除了 TArray<TSoftObjectPtr<UAnimMontage>> Montages 字段。
 * 原先 Montages 和 ComboGraph 并存导致策划配置二义性：
 * - 有 Montages 时走硬编码数组逻辑
 * - 有 ComboGraph 时走数据驱动连招逻辑
 *
 * 现在统一使用 ComboGraph 作为唯一的动画配置入口：
 * - 简单天赋（如被动技能）：ComboGraph 可以为空，只需配置 AbilityClasses
 * - 连招天赋（如普攻/重击）：必须通过 ComboGraph 配置动画和派生关系
 * - 特殊天赋（如下落攻击）：通过 ComboGraph 节点配置不同阶段的蒙太奇
 *   （如 "Plunge_Fall" 节点配置空中动画，"Plunge_Land" 节点配置落地动画）
 *
 * 这确保了所有动画配置走同一条数据管线，消除了"走 Montages 还是走 ComboGraph"的歧义。
 */
USTRUCT(BlueprintType)
struct FTalentConfig
{
    GENERATED_BODY()

    /** 天赋技能Tag */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能Tag"))
    FGameplayTag TalentTag;

    /** 天赋技能图标 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能图标"))
    TSoftObjectPtr<UTexture2D> Icon;

    /** 天赋技能GA列表 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "天赋技能GA列表"))
    TArray<TSubclassOf<UGameplayAbility>> AbilityClasses;

    /**
     * 连招图。Key = 节点名称（如 "Combo_01"、"Heavy_01"），Value = 节点配置。
     * 这是动画配置的唯一入口，替代了旧的 Montages 数组。
     *
     * 配置示例：
     * - 普攻连招：Combo_01 → Combo_02 → Combo_03（通过 NextNodes 派生）
     * - 下落攻击：Plunge_Fall（空中循环） → Plunge_Land（落地砸地）
     * - 被动技能：ComboGraph 为空，只需 AbilityClasses
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "连招图"))
    TMap<FName, FComboActionNode> ComboGraph;

    /** 连招图的入口节点名称（如 "Combo_01"、"Plunge_Fall"） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (DisplayName = "连招入口节点"))
    FName EntryNodeName;
};


/**
 * 角色战斗逻辑数据资产。仅存储 GAS 战斗相关的核心数据。
 *
 * 【架构设计：UI / 表现 / 战斗 三层解耦】
 * 本资产是"战斗层"的唯一载体，职责极度单一：
 * - 只存放属性 GE、突破增益、天赋技能字典等纯战斗数据
 * - 不包含任何 UI 展示字段 → 由 FCharacterRegistryRow 负责
 * - 不包含任何外观表现字段 → 由 UCharacterVisualDataAsset 负责
 *
 * 【架构设计：解决 Super Asset 协同锁死】
 * 原先所有数据（UI/外观/战斗）都塞在同一个 DataAsset 中，
 * 美术改 Mesh 和战斗策划改 Talents 会争抢同一个资产的 Perforce 独占锁。
 *
 * 拆分后：
 * - UCharacterVisualDataAsset：外观/动画数据 → 美术负责
 * - UCharacterCombatDataAsset：战斗/天赋数据 → 战斗策划负责
 * 两者独立签出，互不阻塞。
 *
 * 【架构设计：解决硬编码天赋槽位】
 * 原先 NormalAttack/HeavyAttack/SkillAttack 等是硬编码变量，
 * 无法支持多形态角色或复杂技能链。
 *
 * 现在统一使用 TMap<FGameplayTag, FTalentConfig> CharacterTalents：
 * - Key = 天赋 Tag（如 Ability.Attack.Normal、Ability.Attack.Heavy、Ability.Passive.CritUp）
 * - Value = 天赋配置
 * 策划可以自由扩展任意数量的主动/被动天赋，无需修改 C++ 代码。
 *
 * 【极致内存管理】
 * 本资产不持有任何 TSoftObjectPtr 资源引用（蒙太奇等在 ComboGraph 节点中），
 * 资产本身极轻量，常驻内存无压力。
 * ComboGraph 中的 TSoftObjectPtr<UAnimMontage> 由 GameAssetManagerSubsystem 按需异步加载。
 */
UCLASS(BlueprintType)
class OPENWORLDARPG_API UCharacterCombatDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 战斗属性 GE ---

    /** 角色基础属性 GameplayEffect（生命、攻击、防御等） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Attributes", meta = (DisplayName = "角色基础属性GE"))
    TSubclassOf<UGameplayEffect> BaseAttributesEffect;

    /** 角色突破等级增益 GE。Key = 突破等级，Value = 对应的增益 GE */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Attributes", meta = (DisplayName = "角色突破增益GE"))
    TMap<int32, TSubclassOf<UGameplayEffect>> AscensionBonusEffects;


    // --- 天赋技能（字典化：替代硬编码槽位） ---

    /**
     * 角色所有天赋技能的字典。
     *
     * Key = 天赋 Tag，由策划在编辑器中自由定义，如：
     *   - Ability.Attack.Normal   → 普攻
     *   - Ability.Attack.Heavy    → 重击
     *   - Ability.Attack.Plunge   → 下落攻击
     *   - Ability.Skill.Elemental → 元素战技
     *   - Ability.Skill.Ultimate  → 元素爆发
     *   - Ability.Passive.CritUp  → 被动：暴击率提升
     *   - Ability.Passive.SwimSpeed → 被动：游泳速度提升
     *
     * Value = FTalentConfig，包含该天赋的 GA、连招图等配置。
     *
     * 优势：
     * - 无需修改 C++ 代码即可添加新天赋槽位
     * - 支持多形态角色（如切换形态后拥有不同的 Ability.Form2.Attack.Normal）
     * - 被动天赋和主动天赋统一管理，不再分两个数组
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Talents", meta = (DisplayName = "角色天赋字典"))
    TMap<FGameplayTag, FTalentConfig> CharacterTalents;

    /**
     * 其他通用 GA 能力列表（仅需配置 GA 类，无需蒙太奇/连招图）。
     *
     * 用于角色专属但不需要连招图的简单能力，例如：
     *   - 攀爬跳跃（GA_ClimbJumpBase）
     *   - 钩索
     *   - 闪避
     *   - 交互
     *
     * 与 CharacterTalents 的区别：
     * - CharacterTalents 面向连招型天赋，需要 ComboGraph/Icon/Tag
     * - 本字段面向纯 GA 能力，只需配置类即可，轻量无负担
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Abilities", meta = (DisplayName = "其他GA能力"))
    TArray<TSubclassOf<UGameplayAbility>> GenericAbilities;
};
