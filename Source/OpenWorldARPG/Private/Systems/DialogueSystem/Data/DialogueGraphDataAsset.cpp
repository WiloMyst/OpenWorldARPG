// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/DialogueSystem/Data/DialogueGraphDataAsset.h"

const FDialogueNode* UDialogueGraphDataAsset::GetEntryNode() const
{
    return FindNode(0);
}

const FDialogueNode* UDialogueGraphDataAsset::FindNode(int32 InNodeID) const
{
    for (const FDialogueNode& Node : Nodes)
    {
        if (Node.NodeID == InNodeID)
        {
            return &Node;
        }
    }
    return nullptr;
}
