// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    /**
     * @brief 在蓝图中实现的函数，用于更新进度条。
     * @param Progress A value from 0.0 to 1.0.
     */
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Loading Screen")
    void UpdateProgress(float Progress);
	
};
