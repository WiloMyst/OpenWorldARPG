// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TeamListSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UWidget;
class UAbilitySystemComponent;
class UTexture2D;

/**
 * 队伍角色槽位 UI（哑视图 Dumb View）
 *
 * 【架构设计：哑视图原则】
 * 本控件不主动查询任何 Subsystem，所有展示数据由父容器 (MainWorldHUDLayout) 传入。
 * 职责仅限于：
 * 1. 接收数据并刷新显示
 * 2. 异步加载头像
 * 3. 监听角色死亡状态（ASC 由父容器传入）
 *
 * 【安全解绑：FDelegateHandle】
 * 使用 FDelegateHandle 存储 Tag 事件委托句柄，
 * NativeDestruct 中通过 UnregisterGameplayTagEvent 精准解绑，
 * 避免误删其他监听者。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UTeamListSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * 初始化槽位（哑视图接口：父容器传入所有数据）
     * @param InCharacterName 角色名称
     * @param InHeadIcon 角色头像软引用
     * @param InCharacterTag 角色 Tag（用于绑定死亡状态）
     * @param InCharacterIndex 队伍序号
     * @param InASC 角色的 ASC（用于监听死亡 Tag，可为空）
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Team")
    void InitSlot(
        const FText& InCharacterName,
        TSoftObjectPtr<UTexture2D> InHeadIcon,
        FGameplayTag InCharacterTag,
        int32 InCharacterIndex,
        UAbilitySystemComponent* InASC
    );

    /** 重置槽位到空白状态（对象池复用时调用） */
    void ResetSlot();

protected:
    virtual void NativeDestruct() override;

    // ==========================================
    // UI 控件绑定 (变量名必须与蓝图中完全一致)
    // ==========================================

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Text_CharacterName;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Text_CharacterIndex;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* Image_CharacterIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UWidget* DeathColor;

    // ==========================================
    // 配置与数据
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag DeadStateTag;

    FGameplayTag CharacterTag;
    int32 CharacterIndex = 0;

    /** 缓存 ASC 用于销毁时安全解绑委托 */
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    /** 死亡 Tag 委托句柄（UE5.5 标准安全解绑） */
    FDelegateHandle DeadTagDelegateHandle;

    /** 异步加载句柄，用于取消未完成的加载 */
    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

private:
    // === 内部逻辑方法 ===

    void LoadCharacterIcon(TSoftObjectPtr<UTexture2D> SoftIcon);
    void OnIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);

    void BindCharacterState(UAbilitySystemComponent* InASC);

    /** GAS Tag 变化监听回调 */
    void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
};
