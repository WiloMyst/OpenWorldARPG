// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

/**
 * 通用交互接口。为载具、NPC、可交互物件等提供统一的交互入口。
 *
 * 设计原则：
 * - 交互发起方（玩家）通过 InteractionComponent 检测到实现了此接口的 Actor
 * - 调用 CanInteract 判断是否满足交互条件
 * - 调用 OnInteract 执行交互逻辑（通常发送 GameplayEvent 激活对应 GA）
 * - GetInteractionTargetTransform 返回精确吸附位置，供 Motion Warping 使用
 */

UINTERFACE(Blueprintable)
class UInteractableInterface : public UInterface
{
    GENERATED_BODY()
};

class OPENWORLDARPG_API IInteractableInterface
{
    GENERATED_BODY()

public:
    /** 是否可以与指定角色交互 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool CanInteract(ACharacter* InstigatorCharacter) const;

    /** 执行交互（通常发送 GameplayEvent 激活对应 GA） */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void OnInteract(ACharacter* InstigatorCharacter);

    /** 获取交互目标 Transform（如车门外的精确位置，供 Motion Warping 吸附使用） */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FTransform GetInteractionTargetTransform() const;
};
