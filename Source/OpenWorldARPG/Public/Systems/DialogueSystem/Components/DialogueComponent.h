// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DialogueComponent.generated.h"

class UDialogueGraphDataAsset;
class ACharacter;

/**
 * 对话组件。挂在 NPC 上，持有对话图引用并驱动对话流程。
 *
 * 【职责】
 * - 持有 NPC 可用的对话图（软引用，按需异步加载）
 * - 提供 StartDialogue 入口，通过 LocalPlayer 的 DialogueManagerSubsystem 推进对话
 * - 持有 NPC 身份 Tag，供对话系统识别说话人
 *
 * 对话推进与情绪/LookAt 由 DialogueManagerSubsystem 协调，
 * 通过 NpcAnimInstance 在动画线程上表现。
 */
UCLASS(ClassGroup = (Dialogue), meta = (BlueprintSpawnableComponent))
class UDialogueComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDialogueComponent();

    // --- 对话图引用 ---

    /** 对话图引用（可配置多个，根据条件选择）。默认使用第一个 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TArray<TSoftObjectPtr<UDialogueGraphDataAsset>> DialogueGraphs;

    /** NPC 身份 Tag（如 NPC.Ch001.QuestGiver） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    FGameplayTag NPCTag;

    // --- 对话生命周期 ---

    /**
     * 开始对话。
     * 获取 LocalPlayer 的 DialogueManagerSubsystem，异步加载对话图后调用其 StartDialogue。
     * @param InstigatorCharacter 发起对话的角色（通常是玩家）
     */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void StartDialogue(ACharacter* InstigatorCharacter);

    /** 获取默认对话图（第一个已加载的），未加载则返回 nullptr */
    UDialogueGraphDataAsset* GetDefaultDialogue() const;

protected:
    virtual void BeginPlay() override;
};
