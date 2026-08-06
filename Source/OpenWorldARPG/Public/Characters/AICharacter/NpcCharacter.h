// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AICharacter/AiCharacter.h"
#include "Systems/InteractionSystem/Interfaces/InteractableInterface.h"
#include "NpcCharacter.generated.h"

class UAvatarStreamingComponent;
class UAgentActionComponent;
class UWidgetComponent;
class UNpcAgentStatusWidget;

/**
 * NPC 角色。挂载 AvatarStreamingComponent 并实现 IInteractableInterface，
 * 作为 AI 虚拟人的承载实体。
 */
UCLASS()
class OPENWORLDARPG_API ANpcCharacter : public AAiCharacter, public IInteractableInterface
{
    GENERATED_BODY()

public:
    ANpcCharacter();

    // --- IInteractableInterface ---

    virtual bool CanInteract_Implementation(ACharacter* InstigatorCharacter) const override;
    virtual void OnInteract_Implementation(ACharacter* InstigatorCharacter) override;
    virtual FTransform GetInteractionTargetTransform_Implementation() const override;

protected:
    // --- 组件 ---

    /** gRPC 流式通道组件，接收 VHServer 下发音频/表情/工具调用 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Agent")
    TObjectPtr<UAvatarStreamingComponent> AvatarStreamingComponent;

    virtual void BeginPlay() override;
};
