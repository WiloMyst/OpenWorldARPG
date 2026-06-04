// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h" // 必须包含这个才能在头文件用 FGameplayTag
#include "CharacterWeaponComponent.generated.h"

class APlayerCharacter;
class AWeaponBase;

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UCharacterWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCharacterWeaponComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;

public:
    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void InitializeCharacterWeapon();

    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void WeaponToHand();

    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void WeaponToBack();

    /** 设置武器可见性 (供 Owner 调用，无需暴露内部武器引用) */
    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void SetWeaponHidden(bool bHidden);

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Weapon System|State")
    TObjectPtr<AWeaponBase> CharacterWeapon;

protected:
    UPROPERTY()
    TObjectPtr<APlayerCharacter> CachedCharacter;

    // ==========================================
    // 暴露给蓝图的配置项 (彻底告别硬编码)
    // ==========================================

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Sockets")
    FName HandSocketName = FName("HandSocket");

    // --- 【修改这里】：使用 Tag Container 容纳所有阻止武器收起的标签 ---
    // 策划可以在蓝图里配无数个状态（攻击、瞄准、施法、格挡、僵直...）
    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Tags")
    FGameplayTagContainer PreventStowTags;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Thresholds")
    float MovementInputThreshold = 0.01f;

private:
    bool bIsWeaponStowed = true;
};