// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OpenWorldARPGPlayerController.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "MainGamePlayerController.generated.h"

class UNiagaraSystem;
class UUserWidget;

UCLASS()
class OPENWORLDARPG_API AMainGamePlayerController : public AOpenWorldARPGPlayerController
{
    GENERATED_BODY()

public:
    AMainGamePlayerController();

protected:
    virtual void BeginPlay() override;

    // 对应蓝图自定义事件：处理切换角色请求
    UFUNCTION()
    void HandleSwitchCharacter(int32 TargetIndex);

protected:
    // ==========================================
    // 暴露给蓝图的配置项 (彻底告别硬编码)
    // ==========================================

    // --- UI 配置 ---
    UPROPERTY(EditDefaultsOnly, Category = "Config|UI")
    TSubclassOf<UUserWidget> MainHUDClass;

    // --- 特效配置 ---
    // 对应蓝图里的 P_UpgradeGlow
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    TObjectPtr<UNiagaraSystem> CharacterSwapFX;

    // 【新增】特效生成的位置偏移 (完美替代蓝图里 Z 轴 -100 的逻辑)
    // 默认给一个 (0, 0, -100) 的值，方便你在蓝图里根据不同角色或特效随时微调
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    FVector SwapFXLocationOffset = FVector(0.0f, 0.0f, -100.0f);

    // 【终极补漏：新增】特效生成的缩放比例 (对应蓝图的 0.5, 0.5, 0.5)
    UPROPERTY(EditDefaultsOnly, Category = "Config|Effects")
    FVector SwapFXScale = FVector(0.5f, 0.5f, 0.5f);

    // --- 状态拦截配置 ---
    // 目标角色包含这些 Tag 时禁止切换到该角色 (例如：死亡、处于不可取消的硬直中)
    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags")
    FGameplayTagContainer PreventSwitchTags;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Movement")
    TArray<TEnumAsByte<EMovementMode>> AllowedMovementModes;

private:
    // 缓存主 UI 实例
    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDInstance;
};