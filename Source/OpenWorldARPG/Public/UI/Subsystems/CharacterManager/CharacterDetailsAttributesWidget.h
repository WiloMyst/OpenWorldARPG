// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/CharacterSaveData.h"
#include "Blueprint/UserWidget.h"
#include "CharacterDetailsAttributesWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * 右侧详情面板 - 属性页签（对应 WBP_CharacterDetails_Attributes）。
 *
 * 读取并绑定 FCharacterSaveData，显示角色等级、生命值上限、攻击力、防御力等文本。
 * 所有数据从 CharacterManagerSubsystem 实时拉取，UI 层只读不写。
 */
UCLASS()
class OPENWORLDARPG_API UCharacterDetailsAttributesWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterDetailsAttributesWidget(const FObjectInitializer& ObjectInitializer);

    /**
     * 刷新属性面板数据。
     * 通过 CharacterTag 从 CharacterManagerSubsystem 拉取最新存档数据，
     * 并更新所有绑定的 UI 控件。
     */
    UFUNCTION(BlueprintCallable, Category = "CharacterDetails")
    void RefreshData(const FGameplayTag& CharacterTag);

protected:
    // --- 绑定控件：文本 ---

    /** 角色名称 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> CharacterNameText;

    /** 角色等级 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> LevelText;

    /** 生命值上限 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> MaxHPText;

    /** 攻击力 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> AttackText;

    /** 防御力 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> DefenseText;

    // --- 绑定控件：进度条 ---

    /** 经验进度条 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|Widgets", meta = (BindWidget))
    TObjectPtr<UProgressBar> ExperienceBar;

    // --- 数据 ---

    /** 当前显示的角色 Tag */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|State")
    FGameplayTag CurrentCharacterTag;

    /** 当前显示的存档数据快照 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterDetails|State")
    FCharacterSaveData CurrentSaveData;

    /**
     * 数据拉取完毕后触发（蓝图侧在此更新自定义控件，如稀有度星星、元素图标等）。
     * C++ 基类仅更新基础文本，蓝图可扩展更多表现。
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "CharacterDetails")
    void OnDataRefreshed();
};
