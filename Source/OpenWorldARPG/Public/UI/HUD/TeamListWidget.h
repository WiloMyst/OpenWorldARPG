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
 *
 * 【架构升级：高内聚】
 * 彻底将队伍列表的拉取、对象池管理 (Widget Pooling) 与主 HUD 解耦。
 * 实现自我闭环的组件化设计。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UTeamListWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    // ==========================================
    // 控件绑定
    // ==========================================
    
    // 【修正】根据蓝图层级，将基类 UPanelWidget 明确指定为 UVerticalBox
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UVerticalBox> TeamList;

    // ==========================================
    // 配置项
    // ==========================================
    
    /** 队伍槽位蓝图类 (修改权限，允许在外部主HUD面板中直接覆盖配置) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamList|Config")
    TSubclassOf<UTeamListSlotWidget> TeamListSlotClass;

    /** 对象池最大容量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamList|Config", meta = (ClampMin = "1", ClampMax = "16"))
    int32 TeamSlotPoolSize = 4;

protected:
    // 新增：重写 NativeConstruct 确保安全的初始化时机
    virtual void NativeConstruct() override;

private:
    // --- 核心事件回调 ---
    UFUNCTION()
    void UpdateTeamList();

    // --- Widget Pooling ---
    void InitTeamSlotPool();
    UTeamListSlotWidget* GetOrCreateSlot(int32 Index);

    /** 对象池：预创建的 Slot 数组 */
    UPROPERTY()
    TArray<TObjectPtr<UTeamListSlotWidget>> TeamSlotPool;
};