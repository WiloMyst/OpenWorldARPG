// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TeamListWidget.generated.h"

class UVerticalBox;
class UTeamListSlotWidget;

/**
 * 队伍列表容器 UI (对应 WBP_TeamList)
 * 自管理拉取、对象池 (Widget Pooling) 与主 HUD 解耦。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UTeamListWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    // --- 控件绑定 ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UVerticalBox> TeamList;

    // --- 配置项 ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamList|Config")
    TSubclassOf<UTeamListSlotWidget> TeamListSlotClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamList|Config", meta = (ClampMin = "1", ClampMax = "16"))
    int32 TeamSlotPoolSize = 4;

protected:
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void UpdateTeamList();

    // --- Widget Pooling ---
    void InitTeamSlotPool();
    UTeamListSlotWidget* GetOrCreateSlot(int32 Index);

    UPROPERTY()
    TArray<TObjectPtr<UTeamListSlotWidget>> TeamSlotPool;
};