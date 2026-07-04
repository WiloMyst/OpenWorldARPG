// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterNavMenuWidget.generated.h"

/**
 * 左侧导航菜单（对应 WBP_CharacterNavMenu）。
 * 一组 Radio Button，控制右侧面板切换不同的 Widget 视图。
 * 页签索引：0=属性, 1=武器, 2=圣遗物, 3=命之座, 4=天赋, 5=资料
 */
UCLASS()
class OPENWORLDARPG_API UCharacterNavMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCharacterNavMenuWidget(const FObjectInitializer& ObjectInitializer);

    /** 选中指定页签 */
    UFUNCTION(BlueprintCallable, Category = "CharacterNavMenu")
    void SelectNav(int32 NavIndex);

    /** 获取当前选中的页签索引 */
    UFUNCTION(BlueprintPure, Category = "CharacterNavMenu")
    int32 GetSelectedNav() const { return SelectedNavIndex; }

public:
    /** 当页签切换时触发（向上传递给 MainWidget） */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavChangedSignature, int32, NavIndex);

    UPROPERTY(BlueprintAssignable, Category = "CharacterNavMenu|Events")
    FOnNavChangedSignature OnNavChanged;

protected:
    /** 当前选中的页签索引 */
    UPROPERTY(BlueprintReadOnly, Category = "CharacterNavMenu|State")
    int32 SelectedNavIndex = 0;

    /** 页签切换时的视觉更新（蓝图实现） */
    UFUNCTION(BlueprintImplementableEvent, Category = "CharacterNavMenu")
    void OnNavSelectionChanged(int32 NewIndex);
};
