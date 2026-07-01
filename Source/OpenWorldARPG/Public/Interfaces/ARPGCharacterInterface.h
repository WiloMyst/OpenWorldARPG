// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ARPGCharacterInterface.generated.h"

class UCharacterVisualDataAsset;
class UCharacterCombatDataAsset;
class UTargetingComponent;
class UHeroUIExtensionComponent;
class UOpenWorldARPGCharacterMovementComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UARPGCharacterInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ARPG 角色通用接口。组件通过接口获取数据，无需依赖具体 Character 子类。
 *
 * 【架构设计：三层解耦】
 * 旧接口只暴露一个 GetCharacterDataAsset()，返回超级资产。
 * 新接口拆分为 GetVisualDataAsset() 和 GetCombatDataAsset()，
 * 调用方按需获取表现层或战斗层数据，不引入不必要的依赖。
 */
class OPENWORLDARPG_API IARPGCharacterInterface
{
    GENERATED_BODY()

public:
    /** 获取角色的外观表现数据资产 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ARPG|Character")
    UCharacterVisualDataAsset* GetVisualDataAsset() const;

    /** 获取角色的战斗逻辑数据资产 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ARPG|Character")
    UCharacterCombatDataAsset* GetCombatDataAsset() const;

    /** 获取瞄准/锁定组件（接口化：外部系统无需依赖 APlayerCharacter） */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ARPG|Character")
    UTargetingComponent* GetTargetingComponent() const;

    /** 获取 UI 扩展组件（接口化：HUD 绑定无需依赖 APlayerCharacter） */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ARPG|Character")
    UHeroUIExtensionComponent* GetHeroUIExtensionComponent() const;

    /** 获取自定义移动组件（接口化：GA / AI 无需依赖 APlayerCharacter） */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ARPG|Character")
    UOpenWorldARPGCharacterMovementComponent* GetCustomMovementComponent() const;
};
