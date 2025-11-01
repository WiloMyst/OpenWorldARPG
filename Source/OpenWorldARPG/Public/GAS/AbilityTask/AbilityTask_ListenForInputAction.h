// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "InputAction.h"
#include "AbilityTask_ListenForInputAction.generated.h"

// 声明一个动态多播委托，它将把输入的Vector2D值广播出去
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputActionUpdatedDelegate, const FVector2D&, ActionValue);

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API UAbilityTask_ListenForInputAction : public UAbilityTask
{
	GENERATED_BODY()

public:
    // 这是我们任务在蓝图中的输出引脚，每次输入更新都会触发
    UPROPERTY(BlueprintAssignable)
    FInputActionUpdatedDelegate OnInputActionUpdated;

    /**
     * @brief 在蓝图中创建此任务的静态函数。
     * @param OwningAbility 调用此任务的能力
     * @param ActionToListenFor 我们要监听的InputAction资产
     */
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
    static UAbilityTask_ListenForInputAction* ListenForInputAction(UGameplayAbility* OwningAbility, UInputAction* ActionToListenFor);

protected:
    // 任务的核心逻辑
    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;

    // 当绑定的InputAction被触发时，会调用这个回调函数
    void OnActionTriggered(const struct FInputActionInstance& ActionInstance);

private:
    // 要监听的InputAction
    UPROPERTY()
    UInputAction* Action;

    // 用于在任务结束时取消绑定的句柄
    uint32 BindingHandle;
	
};
