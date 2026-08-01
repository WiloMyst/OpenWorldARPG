// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AICharacter/AiCharacter.h"
#include "Systems/InteractionSystem/Interfaces/InteractableInterface.h"
#include "NpcCharacter.generated.h"

class UDialogueComponent;
class UAvatarStreamingComponent;
class UAgentActionComponent;
class UWidgetComponent;
class UNpcAgentStatusWidget;

/**
 * NPC 角色。挂载 DialogueComponent 并实现 IInteractableInterface，
 * 玩家交互时通过 DialogueComponent 启动对话。
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

    /** 对话组件，持有对话图引用并驱动对话流程 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TObjectPtr<UDialogueComponent> DialogueComponent;

    /** gRPC 流式通道组件，接收 VHServer 下发音频/表情/工具调用 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Agent")
    TObjectPtr<UAvatarStreamingComponent> AvatarStreamingComponent;

    virtual void BeginPlay() override;
};
