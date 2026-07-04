// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_CheckHeavyBranch.generated.h"

/**
 * 长按重击派生检测 AnimNotify。
 *
 * 在动画指定帧触发一次 Gameplay Event，由 GA_MeleeAttackBase 通过 WaitGameplayEvent 监听。
 * 收到后检查玩家是否仍在按住普攻键，若是则模拟发送重击输入，顺滑走入连招图的重击分支。
 *
 * 与 AnimNotify_SendGameplayEvent 的区别：
 * - 语义明确：策划在动画轨道上看到此 Notify 即知是长按检测点
 * - EventTag 可在编辑器中配置，默认值为 Character.Event.CheckHeavyBranch
 */
UCLASS(const, hidecategories = Object, meta = (DisplayName = "Check Heavy Branch"))
class OPENWORLDARPG_API UAnimNotify_CheckHeavyBranch : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_CheckHeavyBranch();

    /** 检查玩家是否仍在按住普攻键，要发送的 Gameplay Event Tag */
    UPROPERTY(EditAnywhere, Category = "GameplayEvent", meta = (Categories = "Character.Event"))
    FGameplayTag CheckHeavyBranchEventTag;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override;
};
