// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "BaseCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnMovementModeChangedDelegate, EMovementMode, PrevMode, EMovementMode, NewMode, uint8, PrevCustomMode, uint8, NewCustomMode);

/**
 * 自定义角色移动组件基类。
 * 提供所有角色（玩家、敌人、NPC）共享的基础移动设施：
 * - 缓存 Owner 引用（Character + ASC）
 * - 基础状态查询（IsFalling / IsGrounded）
 * - 移动模式变更的 GAS Tag 管理（Airborne / Falling / Swimming / Moving）
 * - 离开 Falling 时恢复默认重力 / 空气控制
 * - MovingTag 输入意愿标记（UpdateCharacterStateBeforeMovement）
 *
 * 子类（如 UPlayerCharacterMovementComponent）通过 Override
 * OnMovementModeChanged / UpdateCharacterStateBeforeMovement 扩展自定义行为。
 *
 * 【权责分工】
 * - BaseCMC：管"通用物理状态"（重力恢复、基础Tag映射、移动意愿标记）
 * - PlayerCMC：管"玩家专属移动"（攀爬/滑翔/游泳/冲刺/慢走/瞄准、非对称重力）
 * - CMC 基类（UE）：管"底层物理模拟"（碰撞、网络复制、积分）
 */
UCLASS()
class OPENWORLDARPG_API UBaseCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UBaseCharacterMovementComponent();

    virtual void BeginPlay() override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

    // --- 初始化 ---

    void CacheOwnerReferences();

    // --- 通用状态查询 ---

    bool IsFalling() const { return MovementMode == MOVE_Falling; }
    bool IsGrounded() const { return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking; }

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
    FOnMovementModeChangedDelegate OnMovementModeChangedDelegate;

    // --- 地面配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Grounded|Physics")
    FRotator GroundedRotationRate = FRotator(0.0f, 540.0f, 0.0f);

    // --- 下落配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Falling|Physics")
    FRotator FallingRotationRate = FRotator(0.0f, 300.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Jump|Falling")
    float FallingAirControl = 0.0f;

    // --- 被动运动状态 Tag ---

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    FGameplayTag AirborneTag;

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    FGameplayTag FallingTag;

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    FGameplayTag SwimmingTag;

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    FGameplayTag FastSwimmingTag;

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    FGameplayTag MovingTag;

    UPROPERTY(EditDefaultsOnly, Category = "StateTags")
    float MovingSpeedThreshold = 50.0f;

protected:
    // --- 缓存指针（子类可直接访问） ---

    UPROPERTY()
    TObjectPtr<ACharacter> CachedOwnerCharacter;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> CachedASC;

private:
    // --- 默认值缓存（仅基类内部使用） ---

    float DefaultGravityScale = 1.0f;
    float DefaultAirControl = 0.05f;
};
