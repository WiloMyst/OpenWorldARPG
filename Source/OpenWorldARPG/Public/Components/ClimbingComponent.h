// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "Types/MovementStateTypes.h"
#include "ClimbingComponent.generated.h"

class APlayerCharacter;
class UCharacterMovementComponent;
class UOpenWorldARPGCharacterMovementComponent;
class UAbilitySystemComponent;
class UAnimMontage;
class UMovementStateMachineComponent;
class UCharacterDataAsset;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UClimbingComponent : public UActorComponent
{
    GENERATED_BODY()

    // CMC 在 PhysClimbing 中回写碰撞结果
    friend class UOpenWorldARPGCharacterMovementComponent;

public:
    UClimbingComponent();

    // ==========================================
    // 核心接口
    // ==========================================

    /** 尝试攀爬 (检测墙壁 + 切换模式) */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void TryClimb();

    /** 退出攀爬模式 */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void ExitClimb();

    /** 检测头部上方是否有障碍物并尝试翻越 */
    UFUNCTION(BlueprintCallable, Category = "Climbing")
    void CheckAndClimbUp();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleOwnerMovementInput(float InputX, float InputY);

    /** 进入攀爬模式 */
    void EnterClimb(const FHitResult& WallHit);

    /** 翻越动作 (由 CMC 的 PhysClimbUp 驱动位移，蒙太奇驱动动画) */
    void DoClimbUp();

    /** 翻越蒙太奇播放完成的回调 */
    UFUNCTION()
    void OnClimbUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    /** 翻越位移结束后的清理 */
    void FinishClimbUp();

    /** 执行胸部与头部射线检测 (支持偏移检测) */
    bool PerformClimbTraces(const FVector& TraceOffset, FHitResult& OutChestHit, FHitResult& OutHeadHit);

    // ==========================================
    // 下落转攀爬
    // ==========================================

    /** 下落状态下检测是否可以攀爬 */
    void CheckFallingToClimb();

    /** 验证墙面法线角度是否适合攀爬 */
    bool IsWallClimbable(const FVector& WallNormal) const;

    // ==========================================
    // 攀爬转地面
    // ==========================================

    /** 攀爬状态下检测正下方是否有地面 */
    void CheckClimbToGround();

    // ==========================================
    // 墙角检测与过渡
    // ==========================================

    /** 攀爬横移时检测墙角 */
    void CheckCornerTransition();

    /** 阳角 (外拐角) 过渡 */
    void HandleConvexCorner(const FHitResult& NewWallHit);

    /** 阴角 (内拐角) 过渡 */
    void HandleConcaveCorner(const FHitResult& NewWallHit);

    /** 计算阳角环绕弧线的目标点 */
    FVector CalculateConvexTargetLocation(const FVector& CornerPoint, const FVector& CurrentNormal, const FVector& NewNormal) const;

    /** 计算阴角吸附目标点 */
    FVector CalculateConcaveTargetLocation(const FVector& NewWallHitLocation, const FVector& NewWallNormal) const;

    /** 墙角过渡完成回调 (由 CMC 在过渡完成时直接调用，消除一帧真空) */
    void OnCornerTransitionFinished();

protected:
    // ==========================================
    // 配置项：降频检测 (性能优化)
    // ==========================================

    /** 攀爬期间低频检测间隔 (秒)，触地/墙角/法线更新均按此频率执行 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Performance")
    float ClimbDetectionInterval = 0.1f;

    /** 下落转攀爬检测间隔 (秒) */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Performance")
    float FallingToClimbDetectionInterval = 0.1f;

    // ==========================================
    // 配置项：射线检测
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float TraceDistance = 60.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    FName HeadSocketName = FName("Head");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbUpCheckRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbUpCheckHalfHeight = 50.0f;

    // ==========================================
    // 配置项：逻辑判断与标签
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Logic")
    float MinInputDotProduct = -0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FName UnclimbableActorTag = FName("NotClimbable");

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTagContainer BlockClimbingTags;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag ClimbingStateTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag EventClimbStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTag EventClimbStopTag;

    // ==========================================
    // 配置项：攀爬检测
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    float ClimbPredictOffset = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbSpaceCheckRadius = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Trace")
    float ClimbSpaceCheckHalfHeight = 90.0f;

    // ==========================================
    // 配置项：表现与动画
    // ==========================================

    /** 攀爬贴墙吸附的 Z 轴偏移 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float WallSnapZOffset = 0.0f;

    /** 攀爬贴墙吸附的插值时间 (秒) */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Animation")
    float WallSnapTime = 0.2f;

    // ==========================================
    // 配置项：墙角过渡
    // ==========================================

    /** 墙角检测球体扫掠半径 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float CornerSweepRadius = 30.0f;

    /** 墙角检测扫掠距离 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float CornerSweepDistance = 60.0f;

    /** 阳角环绕弧线半径 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float ConvexArcRadius = 30.0f;

    /** 阴角吸附偏移距离 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Corner")
    float ConcaveSnapDistance = 30.0f;

    // ==========================================
    // 配置项：攀爬转地面
    // ==========================================

    /** 正下方射线检测触地距离阈值 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|GroundTransition")
    float GroundDetectDistance = 20.0f;

    /** 最小攀爬离地高度：角色双脚离地低于此值时不触发攀爬转地面，
        防止在墙根处进入攀爬后立即被 CheckClimbToGround 退出导致鬼畜 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|GroundTransition")
    float MinClimbHeightAboveGround = 50.0f;

private:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> OwnerCharacter;

    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> MovementComp;

    UPROPERTY()
    TObjectPtr<UOpenWorldARPGCharacterMovementComponent> CustomMovementComp;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC;

    UPROPERTY()
    TObjectPtr<UMovementStateMachineComponent> FSMComp;

    /** 角色数据资产 (用于加载攀爬蒙太奇等) */
    UPROPERTY()
    TObjectPtr<UCharacterDataAsset> CharacterData;

    // 状态锁：是否正在执行翻越动作
    bool bIsClimbingUp = false;

    // 当前贴墙法线缓存 (用于墙角检测)
    FVector CurrentWallNormal = FVector::ZeroVector;

    // ==========================================
    // 降频检测定时器
    // ==========================================

    FTimerHandle ClimbDetectionTimerHandle;
    FTimerHandle FallingToClimbTimerHandle;

    // ==========================================
    // CMC 碰撞结果复用
    // ==========================================

    FVector CMCHitWallNormal = FVector::ZeroVector;
    bool bCMCHitWall = false;

    // ==========================================
    // 降频检测回调
    // ==========================================

    UFUNCTION()
    void OnClimbDetectionTick();

    UFUNCTION()
    void OnFallingToClimbTick();
};
