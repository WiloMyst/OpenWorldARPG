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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedSignature, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAimingStateChangedSignature, bool, bIsAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionListChangedSignature, const TArray<AActor*>&, InteractableActors);

/** 英雄 UI 扩展组件 (MVVM ViewModel 层)，Gameplay 与 UI 之间的唯一桥梁。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UHeroUIExtensionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHeroUIExtensionComponent();

    // --- 绑定 ---

    void BindToActor(AActor* InOwner);
    void UnbindAll();

    // --- 状态查询 ---

    float GetCurrentHealth() const { return CurrentHealth; }
    float GetCurrentMaxHealth() const { return CurrentMaxHealth; }
    bool IsAiming() const { return bIsAiming; }
    const TArray<AActor*>& GetInteractableActors() const { return CachedInteractableActors; }

protected:
    // --- GAS 回调 ---

    void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);
    void OnMaxHealthAttributeChanged(const FOnAttributeChangeData& Data);
    void OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount);

    // --- 交互回调 ---

    UFUNCTION()
    void HandleInteractionListChanged(const TArray<AActor*>& InteractableActors);

    // --- 内部辅助 ---

    void UpdateHealthBroadcast();

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "UIExtension|Events")
    FOnHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "UIExtension|Events")
    FOnAimingStateChangedSignature OnAimingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "UIExtension|Events")
    FOnInteractionListChangedSignature OnInteractionListChanged;

private:
    // --- 缓存指针 ---

    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
    TWeakObjectPtr<UInteractionComponent> CachedInteractionComp;

    // --- 委托句柄 ---

    FDelegateHandle HealthAttrHandle;
    FDelegateHandle MaxHealthAttrHandle;
    FDelegateHandle AimingTagHandle;

    // --- 运行时状态 ---

    float CurrentHealth = 0.0f;
    float CurrentMaxHealth = 1.0f;
    bool bIsAiming = false;
    TArray<AActor*> CachedInteractableActors;

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "UIExtension|Config")
    FGameplayTag AimingStateTag;
};
