// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AS_Base.generated.h"

// 自动生成属性的访问器函数 get/set/init
// 所有 AttributeSet 子类统一使用此宏，避免重复定义
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * @brief AttributeSet 基类，提供 ATTRIBUTE_ACCESSORS 宏和公共基础设施。
 * 所有自定义 AttributeSet 应继承此类而非直接继承 UAttributeSet。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UAS_Base : public UAttributeSet
{
	GENERATED_BODY()

};
