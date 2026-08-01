// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DialogueDataTypes.h"
#include "DialogueGraphDataAsset.generated.h"

/**
 * 对话图数据资产（策划编辑的静态数据）。
 *
 * 对话图用节点数组表示，节点间通过 NodeID 引用跳转。
 * EdGraph 编辑器扩展（Phase 9）将提供可视化编辑能力，
 * 运行时由 UDialogueManagerSubsystem 解释执行。
 *
 * 支持节点类型：Speech / Choice / Condition / Action / End
 * 支持分支对话、条件路由、任务联动 Action。
 */
UCLASS(BlueprintType)
class UDialogueGraphDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 对话图唯一标识 Tag（如 Dialogue.Main.Ch001.NPC01） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Identity")
    FGameplayTag DialogueTag;

    /** 对话节点列表（按 NodeID 索引，入口节点 ID = 0） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Nodes")
    TArray<FDialogueNode> Nodes;

    /** 涉及的说话人 Tag 列表（供预加载头像用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Speakers")
    TArray<FGameplayTag> SpeakerTags;

    /** 获取入口节点（NodeID = 0） */
    const FDialogueNode* GetEntryNode() const;

    /** 按 NodeID 查找节点 */
    const FDialogueNode* FindNode(int32 InNodeID) const;

    // --- UPrimaryDataAsset ---

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("DialogueGraph"), GetFName());
    }
};
