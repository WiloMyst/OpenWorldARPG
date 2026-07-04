// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "Blueprint/UserWidget.h"
#include "CharacterDetailsAttributesWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * 右侧详情面板 - 属性页签 (对应 WBP_CharacterDetails_Attributes)。
 * 从 CharacterManagerSubsystem 实时拉取 FCharacterSaveData 并显示，UI 层只读不写。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterDetailsAttributesWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterDetailsAttributesWidget(const FObjectInitializer& ObjectInitializer);

    /** 刷新属性面板：从 CharacterManagerSubsystem 拉取存档数据并更新控件 */
    UFUNCTION(BlueprintCallable, Category = "CharacterDetails")
    void RefreshData(const FGameplayTag& CharacterTag);

protected:
    // --- 文本控件 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> CharacterNameText;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> LevelText;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> MaxHPText;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> AttackText;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> DefenseText;

    // --- 进度条 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UProgressBar> ExperienceBar;

    // --- 数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|State")
    FGameplayTag CurrentCharacterTag;

    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|State")
    FCharacterSaveData CurrentSaveData;

    /** 数据拉取完毕（蓝图扩展自定义控件，如稀有度星星等） */
    UFUNCTION(BlueprintImplementableEvent, Category = "CharacterDetails")
    void OnDataRefreshed();
};
