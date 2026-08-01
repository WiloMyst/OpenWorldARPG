// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DialogueDataTypes.h"
#include "SpeakerDefinitionDataAsset.generated.h"

class UTexture2D;

/**
 * 说话人定义（NPC/角色的对话表现数据）。
 *
 * 每个 Speaker 用 FGameplayTag 唯一标识（如 Speaker.NPC.Scrochi）。
 * 按情绪提供不同头像，供对话框 UI 使用。
 */
UCLASS(BlueprintType)
class USpeakerDefinitionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 标识 ---

    /** 说话人唯一 Tag（如 Speaker.NPC.Villager01） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Identity")
    FGameplayTag SpeakerTag;

    /** 说话人显示名 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Display")
    FText DisplayName;

    // --- 头像 ---

    /** 中性表情头像 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Portrait")
    TSoftObjectPtr<UTexture2D> PortraitNeutral;

    /** 开心表情头像 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Portrait")
    TSoftObjectPtr<UTexture2D> PortraitHappy;

    /** 愤怒表情头像 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Portrait")
    TSoftObjectPtr<UTexture2D> PortraitAngry;

    /** 悲伤表情头像 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Portrait")
    TSoftObjectPtr<UTexture2D> PortraitSad;

    /** 惊讶表情头像 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker|Portrait")
    TSoftObjectPtr<UTexture2D> PortraitSurprised;

    // --- 便捷查询 ---

    /** 按情绪获取头像 */
    UFUNCTION(BlueprintCallable, Category = "Speaker")
    TSoftObjectPtr<UTexture2D> GetPortraitByEmotion(EDialogueEmotion Emotion) const;

    // --- UPrimaryDataAsset ---

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("SpeakerDefinition"), GetFName());
    }
};
