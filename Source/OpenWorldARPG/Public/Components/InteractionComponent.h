// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

/** 当附近可交互对象列表发生变化时广播（从无到有、从有到无） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableListChangedSignature, const TArray<AActor*>&, InteractableActors);

/**
 * 通用交互组件。基于 IInteractableInterface 进行多态检测与调用。
 * 不依赖任何具体交互对象类型（载具、掉落物、NPC 等均可被扫描到）。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // --- 核心交互接口 ---

    /** 当附近可交互对象列表发生改变时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnInteractableListChangedSignature OnInteractableListChangedDelegate;

    /** 执行交互：对列表中第一个可交互对象调用 OnInteract（多态分发） */
    UFUNCTION(BlueprintCallable, Category = "Interaction|Action")
    void Interact();

    /** 获取当前附近可交互对象列表 */
    UFUNCTION(BlueprintPure, Category = "Interaction|State")
    TArray<AActor*> GetCurrentInteractableActors() const;

protected:
    bool IsCharacterInStandby() const;

    // --- 配置 ---

    /** 交互检测球半径 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Config")
    float InteractionRadius = 150.0f;

private:
    /** 缓存的附近所有可交互对象的弱引用列表 */
    TArray<TWeakObjectPtr<AActor>> CurrentInteractableActors;
};
