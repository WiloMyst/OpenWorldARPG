// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "UIManagerSubsystem.generated.h"

class UBaseMenuWidget;
class UUIDataAsset;

/**
 * @class UUIManagerSubsystem
 * @brief UI管理子系统。
 */
UCLASS(Blueprintable)
class OPENWORLDARPG_API UUIManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 显示指定Tag的UI
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	UBaseMenuWidget* ShowUIByTag(FGameplayTag UITag);

    // 尝试打开指定Widget类的UI
    UBaseMenuWidget* OpenUI(TSubclassOf<UBaseMenuWidget> WidgetClass);

    // 关闭UI堆栈最顶层的UI
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseTopUI();

    // 判断是否有UI已经被打开
    UFUNCTION(BlueprintPure, Category = "UI Manager")
    bool IsAnyUIOpen() const;

    // 根据UI的设置，更新PlayerController的输入模式
    void UpdateInputMode();


protected:
    // UI映射数据资产，包含UI的GameplayTag与Widget类的映射
    UPROPERTY(EditDefaultsOnly, Category = "UI Manager")
    TObjectPtr<UUIDataAsset> UIMapDataAsset;

    // UI堆栈，管理打开的UI，实现后进先出
    UPROPERTY()
    TArray<TObjectPtr<UBaseMenuWidget>> UIStack;

};