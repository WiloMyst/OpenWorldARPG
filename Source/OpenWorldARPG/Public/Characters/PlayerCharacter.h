// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/OpenWorldARPGCharacter.h"
#include "AbilitySystemInterface.h"
#include "PlayerCharacter.generated.h"

class ACharacterData;
class UCharacterDataAsset;

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API APlayerCharacter : public AOpenWorldARPGCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
    APlayerCharacter();

    /** Camera boom positioning the camera behind the character */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    class USpringArmComponent* CameraBoom;

    /** Follow camera */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent* FollowCamera;

public:
    // ======== 角色数据绑定 ========
    /**
     * @brief 将此Character与一个后台的CharacterData实例进行绑定。
     * 这是角色生成后最重要的函数，它赋予了"傀儡"以"灵魂"。
     * @param DataToBind 要绑定的角色数据。
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Initialization")
    void BindData(ACharacterData* InCharacterData);

    // ======== 角色状态管理 ========
    /**
     * @brief 设置角色的待机模式。
     * @param bNewStandbyState true表示进入待机模式，false表示进入活动模式。
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|State")
    void SetStandbyMode(bool bNewStandbyState);

    /**
     * @brief 设置角色的基础行为动画层
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
    void SetupBaseBehaviorAnimLayers();

    /**
     * @brief 设置角色的瞄准动画层
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
    void SetupAimAnimLayers();

    /**
     * @brief 开启布料模拟
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
    void SetupPhysicsAnimLayers();

    /**
     * @brief 关闭布料模拟
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Animation")
    void ClearPhysicsAnimLayers();

protected:
    // 在数据绑定后调用，用于设置所有视觉相关的部分
    UFUNCTION(BlueprintNativeEvent, Category = "PlayerCharacter|Initialization")
    void OnDataBound();
	virtual void OnDataBound_Implementation();

    // 回调函数，用于在异步加载角色模型后进行设置
    void OnMeshLoaded(const UCharacterDataAsset* DataAsset);

public:
    // ======== 公共Getters ========
    /** Returns CameraBoom subobject **/
    FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    /** Returns FollowCamera subobject **/
    FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

    /**
     * @brief 获取角色的CharacterData上的ASC指针。实现IAbilitySystemInterface接口。
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|GAS")
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
    /**
     * @brief 获取角色的CharacterData指针。
     */
    UFUNCTION(BlueprintCallable, Category = "PlayerCharacter|Data")
	ACharacterData* GetCharacterData() const { return CharacterData; }

protected:
    /**
     * @brief 指向该角色"灵魂"（数据核心）的指针。
     * 角色所有的状态和能力都存储在这里面。
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerCharacter|Data")
    TObjectPtr<ACharacterData> CharacterData;

};
