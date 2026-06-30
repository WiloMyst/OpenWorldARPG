// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WeaponManagerComponent.generated.h"

class APlayerCharacter;
class AWeaponBase;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UWeaponManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponManagerComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void DestroyCharacterWeapon();

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

    // --- 配置 ---

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Sockets")
    FName HandSocketName = FName("HandSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Tags")
    FGameplayTagContainer PreventStowTags;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon System|Config|Thresholds")
    float MovementInputThreshold = 0.01f;

private:
    bool bIsWeaponStowed = true;
};
