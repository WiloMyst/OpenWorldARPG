// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AICharacter/AiCharacter.h"
#include "Systems/AISystem/AIPatrolAreaBase.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "EnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UAS_Enemy;
class UWidgetComponent;
class UAnimMontage;
class UGameplayEffect;

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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- 接口实现 (IAbilitySystemInterface / ICombatInterface) ---

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
    virtual void HandleDeath_Implementation() override;
    virtual void OnRep_IsDead(bool bOldIsDead) override;

    // --- 动画 ---

    void BindAnimLayers();

    // --- UI ---

    void UpdateHealthBar();
    void OrientToScreen(USceneComponent* SceneComp);

    // --- 战斗 ---

    void MeleeAttack();
    void CancelMeleeAttack();
    void ApplyDamage();

    // --- 死亡 ---

    void OnDead();

protected:
    // --- GAS 回调 ---

    void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

    // --- 动画回调 ---

    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // --- 武器复制回调 ---

    UFUNCTION()
    void OnRep_EnemyWeapon();

    // --- 死亡辅助 ---

    void DestroyEnemy();

public:
    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
    FOnEnemyAttackFinished OnAttackFinished;

    // --- AI ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    TObjectPtr<AAIPatrolAreaBase> PatrolArea;

protected:
    // --- 组件 ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
    TObjectPtr<UAS_Enemy> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|UI")
    TObjectPtr<UWidgetComponent> HealthBarComponent;

    // --- 配置：武器与动画 ---

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
    FGameplayTag DieEventTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|State")
    FGameplayTagContainer CancelTagsOnDeath;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Config|State")
    float DestroyDelayTime = 5.0f;

private:
    // --- 运行时状态 ---

    UPROPERTY(ReplicatedUsing = OnRep_EnemyWeapon)
    TObjectPtr<AActor> EnemyWeapon;

    FTimerHandle DeathDestroyTimerHandle;
};
