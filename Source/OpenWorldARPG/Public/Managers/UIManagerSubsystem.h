// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "UIManagerSubsystem.generated.h"

class UWindowWidgetBase;
class UUIDataAsset;
class ULoadingScreenWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIStackChangedSignature);

/**
 * UI 管理子系统。基于栈的 UI 打开/关闭管理，自动处理输入模式切换。
 *
 * 继承自 ULocalPlayerSubsystem 而非 UGameInstanceSubsystem 的原因：
 * 1. UI 栈是"每玩家"状态，而非"每游戏实例"全局状态。改为 LocalPlayerSubsystem 后，
 *    每个本地玩家拥有独立的 UI 栈，天然支持本地多人分屏（Local Multiplayer）场景。
 * 2. UMG Widget 的创建、输入模式切换（SetInputMode）均与具体 PlayerController/LocalPlayer
 *    绑定。挂在 LocalPlayer 下可直接通过 GetLocalPlayer() 获取归属，避免在多本地玩家
 *    环境中误操作到其他玩家的输入状态。
 * 3. 生命周期与 LocalPlayer 对齐：玩家加入时创建、离开时销毁，比 GameInstance 更细粒度。
 */
UCLASS()
class OPENWORLDARPG_API UUIManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 通过 Tag 显示 UI（从 UIDataAsset 查找对应 WidgetClass） */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    UWindowWidgetBase* ShowUIByTag(FGameplayTag UITag);

    /** 打开指定 WidgetClass 的 UI */
    UWindowWidgetBase* OpenUI(TSubclassOf<UWindowWidgetBase> WidgetClass);

    /** 关闭栈顶 UI */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseTopUI();

    /**
     * 通过 Tag 关闭指定 UI。
     * 在 ActiveUIs 映射中查找对应 Widget，从 UIStack 与映射中移除并 RemoveFromParent。
     * 若该 UI 不在栈顶，仍可被精确关闭。
     */
    UFUNCTION(BlueprintCallable, Category = "UI Manager")
    void CloseUIByTag(const FGameplayTag& UITag);

    /** 是否有 UI 处于打开状态 */
    UFUNCTION(BlueprintPure, Category = "UI Manager")
    bool IsAnyUIOpen() const;

    /**
     * 查询指定 Tag 的 UI 是否处于打开状态。
     * 作为 Controller / UI 之间唯一事实来源 (SSOT) 的状态查询入口。
     * 仅当 Tag 存在于 ActiveUIs 映射中、对应 Widget 有效且仍在 Viewport 中时返回 true。
     */
    UFUNCTION(BlueprintPure, Category = "UI Manager")
    bool IsUIOpen(const FGameplayTag& UITag) const;

    /** 获取栈顶 UI，供 PlayerController 仲裁输入状态使用 */
	UFUNCTION(BlueprintPure, Category = "UI Manager")
	UWindowWidgetBase* GetTopWindowWidget() const;

    /** 当 UI 栈发生变化时（打开或关闭了全屏面板），发出广播 */
    UPROPERTY(BlueprintAssignable, Category = "UI Manager")
    FOnUIStackChangedSignature OnUIStackChanged;

    // --- 加载界面管理 ---

    /**
     * 显示加载界面。
     * 从 UOpenWorldARPGSettings 读取 LoadingScreenWidgetClass 并创建实例，
     * 添加到 Viewport 最顶层（Z-Order=100）。
     * 加载界面独立于 UIStack，不参与栈式输入管理。
     */
    UFUNCTION(BlueprintCallable, Category = "UI Manager|Loading")
    void ShowLoadingScreen();

    /**
     * 隐藏加载界面。
     * 从 Viewport 移除并销毁加载界面实例。
     */
    UFUNCTION(BlueprintCallable, Category = "UI Manager|Loading")
    void HideLoadingScreen();

protected:
    /** UI 栈，后进先出 */
    UPROPERTY()
    TArray<TObjectPtr<UWindowWidgetBase>> UIStack;

    /**
     * Tag → Widget 映射，记录通过 ShowUIByTag 打开的 UI。
     * 与 UIStack 并行维护：Open 时插入，Close 时移除。
     * 提供 O(1) 的 IsUIOpen / CloseUIByTag 查询能力，作为 UI 生命周期的唯一事实来源。
     */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UWindowWidgetBase>> ActiveUIs;

    /** 加载界面实例（独立于 UIStack，不参与栈式管理） */
    UPROPERTY()
    TObjectPtr<ULoadingScreenWidget> LoadingScreenInstance;
};