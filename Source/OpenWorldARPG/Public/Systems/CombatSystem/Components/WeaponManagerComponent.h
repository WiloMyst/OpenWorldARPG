// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WeaponManagerComponent.generated.h"

class APlayerCharacter;
class AWeaponBase;
class UAbilitySystemComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UWeaponManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponManagerComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- 武器管理 ---

    void InitializeCharacterWeapon();
    void DestroyCharacterWeapon();
    void WeaponToHand();
    void WeaponToBack();
    void SetWeaponHidden(bool bHidden);

protected:
    virtual void BeginPlay() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
    // --- 内部逻辑 ---

    void UpdateWeaponState();

    // 客户端根据复制状态重新挂载武器到对应 Socket
    void ApplyWeaponAttachState();

    // --- 复制回调 ---

    UFUNCTION()
    void OnRep_CharacterWeapon();

    UFUNCTION()
    void OnRep_IsWeaponStowed();

    // --- Tag 回调 ---

    UFUNCTION()
    void OnMovementTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    UFUNCTION()
    void OnPreventStowTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

public:
    // --- 武器引用 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_CharacterWeapon, Category = "Weapon System|State")
    TObjectPtr<AWeaponBase> CharacterWeapon;

protected:
    // --- 缓存 ---

    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedCharacter;

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Sockets")
    FName HandSocketName = FName("HandSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Tags")
    FGameplayTagContainer PreventStowTags;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Tags")
    FGameplayTag MovingTag;

private:
    // --- 运行时状态 ---

    // 武器收/拔状态由服务器权威设置并复制，客户端通过 OnRep 重新挂载
    UPROPERTY(ReplicatedUsing = OnRep_IsWeaponStowed)
    bool bIsWeaponStowed = true;

    // --- 缓存 ---

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> CachedASC;
};
