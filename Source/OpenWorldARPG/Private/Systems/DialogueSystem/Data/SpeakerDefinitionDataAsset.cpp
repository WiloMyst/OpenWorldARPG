// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/DialogueSystem/Data/SpeakerDefinitionDataAsset.h"

TSoftObjectPtr<UTexture2D> USpeakerDefinitionDataAsset::GetPortraitByEmotion(EDialogueEmotion Emotion) const
{
    switch (Emotion)
    {
    case EDialogueEmotion::Happy:       return PortraitHappy;
    case EDialogueEmotion::Angry:       return PortraitAngry;
    case EDialogueEmotion::Sad:         return PortraitSad;
    case EDialogueEmotion::Surprised:   return PortraitSurprised;
    case EDialogueEmotion::Neutral:
    default:                            return PortraitNeutral;
    }
}
