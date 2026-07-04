// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CombatSystem/Weapons/GunBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AGunBase::AGunBase()
{
    // 基础枪械不需要每帧 Tick
    PrimaryActorTick.bCanEverTick = false;
}

FTransform AGunBase::GetMuzzleTransform() const
{
    if (WeaponMesh)
    {
        // 自动获取你在蓝图里配置的 WeaponMesh 上的对应插槽
        return WeaponMesh->GetSocketTransform(MuzzleSocketName);
    }
    return GetActorTransform();
}

void AGunBase::PlayShootFX()
{
    if (!WeaponMesh) return;

    // 1. 播放枪口火焰 (Muzzle Flash)
    if (MuzzleFlashEffect)
    {
        // 使用 SpawnEmitterAttached 播放旧版粒子系统
        UGameplayStatics::SpawnEmitterAttached(
            MuzzleFlashEffect,
            WeaponMesh,
            MuzzleSocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            MuzzleFlashScale,
            EAttachLocation::SnapToTarget,
            true, // bAutoDestroy
            EPSCPoolMethod::None,
            true // bAutoActivate
        );
    }

    // 2. 播放开火音效
    if (FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,                          // WorldContextObject
            FireSound,                     // 播放的音效
            GetMuzzleTransform().GetLocation(), // 播放位置
            FRotator::ZeroRotator,         // 旋转
            1.0f,                          // 音量乘数
            1.0f,                          // 音高乘数
            0.0f,                          // 开始时间
            FireSoundAttenuation           // 衰减配置
        );
    }

    // 3. 播放摄像机震动
    if (FireCameraShakeClass)
    {
        if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
        {
            // 只有当这把枪的主人是玩家（有 PlayerController）时，才触发屏幕震动
            if (APlayerController* PC = Cast<APlayerController>(OwningPawn->GetController()))
            {
                if (PC->PlayerCameraManager)
                {
                    // Scale 设置为 1.0f，PlaySpace 默认为 CameraLocal
                    PC->PlayerCameraManager->StartCameraShake(FireCameraShakeClass, 1.0f);
                }
            }
        }
    }
}