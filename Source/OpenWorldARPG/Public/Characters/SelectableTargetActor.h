// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SelectableTargetActor.generated.h"

class USphereComponent;
class USceneComponent;

/**
 * 可被视野选中的实体基类 (钩索点、可暗杀敌人、可互动物品等)。
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
    // --- 状态响应 ---

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting|Events")
    void OnSetAsTarget();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting|Events")
    void OnClearAsTarget();

    // --- 视觉辅助 ---
    UFUNCTION(BlueprintCallable, Category = "Targeting|Visuals")
    void OrientToScreen(USceneComponent* SceneCompToOrient);

protected:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting|Components")
    TObjectPtr<USceneComponent> DefaultRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting|Components")
    TObjectPtr<USphereComponent> DetectSphere;

    // --- 重叠事件 ---
    UFUNCTION()
    virtual void OnDetectSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnDetectSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};