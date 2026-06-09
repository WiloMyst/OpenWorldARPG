// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ARPGCharacterInterface.generated.h"

class UCharacterDataAsset;

UINTERFACE(MinimalAPI, Blueprintable)
class UARPGCharacterInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ARPG 角色通用接口。组件通过接口获取数据，无需依赖具体 Character 子类。
 */
class OPENWORLDARPG_API IARPGCharacterInterface
{
    GENERATED_BODY()

public:
    /** 获取角色的静态配置数据资产 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ARPG|Character")
    UCharacterDataAsset* GetCharacterDataAsset() const;
};
