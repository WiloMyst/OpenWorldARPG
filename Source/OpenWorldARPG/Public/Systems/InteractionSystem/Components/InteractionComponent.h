// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableListChangedSignature, const TArray<AActor*>&, InteractableActors);

/** 通用交互组件，基于 IInteractableInterface 进行多态检测与调用。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- 核心交互 ---

    /** 本地选取交互目标并请求服务器权威执行交互。 */
    void Interact();
    TArray<AActor*> GetCurrentInteractableActors() const;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- 内部辅助 ---

    bool IsCharacterInStandby() const;

    // --- 服务器权威交互 ---

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_Interact(AActor* TargetActor);

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnInteractableListChangedSignature OnInteractableListChangedDelegate;

protected:
    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Config")
    float InteractionRadius = 150.0f;

private:
    // --- 运行时状态 ---

    TArray<TWeakObjectPtr<AActor>> CurrentInteractableActors;
};
