// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "HeroUIExtensionComponent.generated.h"

class UAbilitySystemComponent;
class UInteractionComponent;
class UAS_Player;

/** 血量变化广播（参数：当前血量，最大血量） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedSignature, float, CurrentHealth, float, MaxHealth);

/** 瞄准状态变化广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAimingStateChangedSignature, bool, bIsAiming);

/** 可交互物品列表变化广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionListChangedSignature, const TArray<AActor*>&, InteractableActors);

/**
 * 英雄 UI 扩展组件 (MVVM ViewModel 层)
 *
 * 【架构设计：物理隔离 Gameplay 与 UI】
 * 本组件是 Gameplay 层与 UI 层之间的唯一桥梁。
 * HUD 只认本组件，不再直接依赖 APlayerCharacter / UAbilitySystemComponent / UInteractionComponent。
 *
 * 优势：
 * 1. UI 代码与具体 Actor 类型解耦 —— 未来给载具/机甲做 HUD 只需挂载本组件
 * 2. 集中管理委托生命周期 —— Pawn 切换时只需重新 Bind 一次
 * 3. 统一事件接口 —— UI 层无需关心数据来源是 GAS 还是其他系统
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UHeroUIExtensionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHeroUIExtensionComponent();

    /** 绑定到指定 Actor 的 ASC 与 InteractionComponent（Pawn 切换时复用） */
    void BindToActor(AActor* InOwner);

    /** 解绑当前所有委托 */
    void UnbindAll();

    // --- 事件（UI 层监听这些广播即可） ---

    UPROPERTY(BlueprintAssignable, Category = "UIExtension|Events")
    FOnHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "UIExtension|Events")
    FOnAimingStateChangedSignature OnAimingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "UIExtension|Events")
    FOnInteractionListChangedSignature OnInteractionListChanged;

    // --- 查询接口 ---

    UFUNCTION(BlueprintPure, Category = "UIExtension|State")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category = "UIExtension|State")
    float GetCurrentMaxHealth() const { return CurrentMaxHealth; }

    UFUNCTION(BlueprintPure, Category = "UIExtension|State")
    bool IsAiming() const { return bIsAiming; }

    UFUNCTION(BlueprintPure, Category = "UIExtension|State")
    const TArray<AActor*>& GetInteractableActors() const { return CachedInteractableActors; }

protected:
    // --- 内部回调 ---
    void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);
    void OnMaxHealthAttributeChanged(const FOnAttributeChangeData& Data);
    void OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount);
    UFUNCTION()
    void HandleInteractionListChanged(const TArray<AActor*>& InteractableActors);

    void UpdateHealthBroadcast();

private:
    // --- 缓存指针（弱引用，防止野指针） ---
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
    TWeakObjectPtr<UInteractionComponent> CachedInteractionComp;

    // --- 委托句柄 ---
    FDelegateHandle HealthAttrHandle;
    FDelegateHandle MaxHealthAttrHandle;
    FDelegateHandle AimingTagHandle;

    // --- 运行时状态缓存 ---
    float CurrentHealth = 0.0f;
    float CurrentMaxHealth = 1.0f;
    bool bIsAiming = false;
    TArray<AActor*> CachedInteractableActors;

    // --- 配置 ---
    UPROPERTY(EditDefaultsOnly, Category = "UIExtension|Config")
    FGameplayTag AimingStateTag;
};
