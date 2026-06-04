// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SelectableTargetActor.generated.h"

class USphereComponent;
class USceneComponent;

/**
 * @class ASelectableTargetActor
 * @brief 任何可以被玩家视野选中的实体的基类 (钩索点、可暗杀敌人、可互动物品等)。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API ASelectableTargetActor : public AActor
{
    GENERATED_BODY()
    
public:    
    ASelectableTargetActor();

protected:
    virtual void BeginPlay() override;

public:    
    // ==========================================
    // 状态响应接口 (蓝图重写，用于显示高亮或UI)
    // ==========================================

    /** 当被选取组件判定为“当前最优目标”时触发 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting|Events")
    void OnSetAsTarget();

    /** 当失去最优目标状态时触发 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting|Events")
    void OnClearAsTarget();

    // ==========================================
    // 视觉辅助
    // ==========================================

    /** 让指定组件（如提示 UI）始终朝向玩家屏幕 */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Visuals")
    void OrientToScreen(USceneComponent* SceneCompToOrient);

protected:
    // ==========================================
    // 核心组件
    // ==========================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting|Components")
    TObjectPtr<USceneComponent> DefaultRoot;

    /** 触发检测的球体范围 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting|Components")
    TObjectPtr<USphereComponent> DetectSphere;

    // ==========================================
    // 自动注册/注销重叠事件
    // ==========================================
    UFUNCTION()
    virtual void OnDetectSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnDetectSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};