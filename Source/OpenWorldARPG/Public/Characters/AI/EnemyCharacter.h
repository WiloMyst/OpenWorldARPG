// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AI/AiCharacter.h"
#include "AbilitySystemInterface.h"
#include "EnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UAS_Enemy;

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API AEnemyCharacter : public AAiCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	// ======== IAbilitySystemInterface 核心实现 ========
	/**
	 * @brief 获取EnemyCharacter上的ASC指针。实现IAbilitySystemInterface接口。
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|GAS")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

protected:
	// ======== GAS核心组件 ========
	/** 角色的能力系统组件。管理所有技能、效果和属性。*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 角色的属性集。存储生命、攻击等核心数值。*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
	TObjectPtr<UAS_Enemy> AttributeSet;
	
};
