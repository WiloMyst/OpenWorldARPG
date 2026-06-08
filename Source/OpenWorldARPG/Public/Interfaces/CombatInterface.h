// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatInterface.generated.h"

// 这个类是给虚幻引擎底层的反射系统（UHT）用的，不要动它
UINTERFACE(MinimalAPI, Blueprintable)
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * @class ICombatInterface
 * @brief 战斗实体接口，任何参与战斗的 Actor（玩家、怪物、可破坏物）都应继承此接口。
 */
class OPENWORLDARPG_API ICombatInterface
{
	GENERATED_BODY()

public:
	// ==========================================
	// 接口定义
	// 使用 BlueprintNativeEvent 是大厂神级做法：
	// 它允许你在 C++ 中写一段基础的 HandleDeath 逻辑，同时也允许策划/美术在具体怪物的蓝图里重写它（比如加个溶解特效）。
	// ==========================================

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|State")
	void HandleDeath();
};
