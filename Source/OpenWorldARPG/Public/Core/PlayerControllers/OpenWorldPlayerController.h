// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PlayerControllers/GameplayPlayerController.h"
#include "GameplayTagContainer.h"
#include "OpenWorldPlayerController.generated.h"

class UInputAction;
class AVehiclePawnBase;
class APlayerCharacter;

/**
 * 大世界 PlayerController。继承通用战斗输入绑定。
 * 专属职责：
 *   - 钩索 (Hook) 输入派发
 *   - 载具驾驶：Possess/UnPossess Server RPC + 相机平滑过渡
 * 未来可扩展：大地图输入映射、滑翔伞、骑乘等大世界特有输入。
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldPlayerController : public AGameplayPlayerController
{
    GENERATED_BODY()

public:
    AOpenWorldPlayerController();

    // --- 载具驾驶 Server RPC ---

    /** 服务器端：执行角色 → 载具 Possess 切换 */
    UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Vehicle|Possession")
    void Server_PossessVehicle(AVehiclePawnBase* TargetVehicle);
    void Server_PossessVehicle_Implementation(AVehiclePawnBase* TargetVehicle);
    bool Server_PossessVehicle_Validate(AVehiclePawnBase* TargetVehicle);

    /** 服务器端：执行载具 → 角色 UnPossess 切换 */
    UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Vehicle|Possession")
    void Server_UnPossessVehicle(FVector ExitLocation);
    void Server_UnPossessVehicle_Implementation(FVector ExitLocation);
    bool Server_UnPossessVehicle_Validate(FVector ExitLocation);

    // --- 载具相机过渡 Client RPC ---

    /** 客户端：准备相机平滑过渡（临时关闭自动相机管理防硬切） */
    UFUNCTION(Client, Reliable)
    void Client_PrepareForCameraBlend();
    void Client_PrepareForCameraBlend_Implementation();

    /** 客户端：镜头从人平滑拉升到车 */
    UFUNCTION(Client, Reliable)
    void Client_BlendCameraToVehicle(AVehiclePawnBase* TargetVehicle);
    void Client_BlendCameraToVehicle_Implementation(AVehiclePawnBase* TargetVehicle);

    /** 客户端：镜头从车平滑切回人，并清理载具 IMC */
    UFUNCTION(Client, Reliable)
    void Client_BlendCameraToCharacter(APlayerCharacter* InCharacter, AVehiclePawnBase* OldVehicle);
    void Client_BlendCameraToCharacter_Implementation(APlayerCharacter* InCharacter, AVehiclePawnBase* OldVehicle);

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // --- 大世界专属输入回调 ---

    /** 钩索输入：向角色发送 HookStartEventTag 事件 */
    void Input_Hook();

protected:
    // --- 配置：大世界专属输入资产 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Input|OpenWorld")
    TObjectPtr<UInputAction> IA_Hook;

    // --- 配置：大世界专属 Tags ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Tags|OpenWorld")
    FGameplayTag HookStartEventTag;

    // --- 配置：载具相机过渡 ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|Vehicle|Camera")
    float VehicleCameraBlendTime = 0.6f;

    UPROPERTY(EditDefaultsOnly, Category = "Config|Vehicle|Camera")
    float CharacterCameraBlendTime = 0.4f;

    // --- 配置：上车距离校验 ---

    /** 上车时玩家与载具最大允许距离（cm），防瞬移上车作弊 */
    UPROPERTY(EditDefaultsOnly, Category = "Config|Vehicle|Validation")
    float MaxEnterVehicleDistance = 600.0f;
};
