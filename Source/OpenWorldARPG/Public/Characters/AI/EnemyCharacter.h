// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AI/AiCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "EnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UAS_Enemy;
class UWidgetComponent;
class UAnimMontage;
class UGameplayEffect;

// 委托声明：攻击结束时广播 (对应图: OnAttackFinished)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyAttackFinished);

UCLASS()
class OPENWORLDARPG_API AEnemyCharacter : public AAiCharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Enemy|GAS")
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

    // --- 接口 ---

    UFUNCTION(BlueprintCallable, Category = "Enemy|Animation")
    void BindAnimLayers();

    UFUNCTION(BlueprintCallable, Category = "Enemy|UI")
    void UpdateHealthBar();

    UFUNCTION(BlueprintCallable, Category = "Enemy|UI")
    void OrientToScreen(USceneComponent* SceneComp);

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void MeleeAttack();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void CancelMeleeAttack();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void ApplyDamage();

    UFUNCTION(BlueprintCallable, Category = "Enemy|State")
    void OnDead();

    // --- ICombatInterface ---

    virtual void HandleDeath_Implementation() override;

    // --- 事件 ---

    UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
    FOnEnemyAttackFinished OnAttackFinished;

protected:
    virtual void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void DestroyEnemy();

protected:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
    TObjectPtr<UAS_Enemy> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|UI")
    TObjectPtr<UWidgetComponent> HealthBarComponent;

    // --- 配置：武器与表现 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Weapon")
    TSubclassOf<AActor> WeaponClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Weapon")
    FName WeaponSocketName = FName("RightHandWeaponSocket");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Animation")
    TSubclassOf<UAnimInstance> AnimLayerClass;

    // --- 配置：战斗参数 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Combat")
    TObjectPtr<UAnimMontage> ComboAttackMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Combat")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Combat")
    float DamageTraceRadius = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Combat")
    float DamageTraceForwardStartOffset = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|Combat")
    float DamageTraceForwardEndOffset = 100.0f;

    // --- 配置：死亡参数 ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|State")
    TSubclassOf<class UGameplayAbility> DeathAbilityClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|State")
    FGameplayTag DeathAbilityTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|State")
    FGameplayTagContainer CancelTagsOnDeath;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|State")
    float DestroyDelayTime = 5.0f;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    TObjectPtr<AActor> PatrolArea;

private:
    UPROPERTY()
    TObjectPtr<AActor> EnemyWeapon;

    FTimerHandle DeathDestroyTimerHandle;
};