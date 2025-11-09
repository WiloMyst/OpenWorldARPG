// Copyright 2025 WiloMyst. All Rights Reserved.


#include "GAS/AbilityTask/AbilityTask_ListenForInputAction.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"

UAbilityTask_ListenForInputAction* UAbilityTask_ListenForInputAction::ListenForInputAction(UGameplayAbility* OwningAbility, UInputAction* ActionToListenFor)
{
    // 标准的Task创建样板代码
    UAbilityTask_ListenForInputAction* MyObj = NewAbilityTask<UAbilityTask_ListenForInputAction>(OwningAbility);
    MyObj->Action = ActionToListenFor;
    return MyObj;
}

void UAbilityTask_ListenForInputAction::Activate()
{
    Super::Activate();

    if (!Action)
    {
        EndTask();
        return;
    }

    // 获取玩家控制器和增强输入组件
    AActor* OwnerActor = AbilitySystemComponent->GetOwnerActor();
    APlayerController* PC = Cast<APlayerController>(OwnerActor);
    if (!PC)
    {
        // 如果Owner不是PC，尝试获取其控制器
        if (APawn* Pawn = Cast<APawn>(OwnerActor))
        {
            PC = Cast<APlayerController>(Pawn->GetController());
        }
    }

    if (PC && PC->InputComponent)
    {
        // [核心] 绑定到增强输入组件的Triggered事件
        if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
        {
            BindingHandle = EIC->BindAction(Action, ETriggerEvent::Triggered, this, &UAbilityTask_ListenForInputAction::OnActionTriggered).GetHandle();
        }
    }
    else
    {
        EndTask();
    }
}

// 这是每次输入（WASD按下）时都会被调用的函数
void UAbilityTask_ListenForInputAction::OnActionTriggered(const FInputActionInstance& ActionInstance)
{
    // 获取输入值并广播给蓝图
    FVector2D Value = ActionInstance.GetValue().Get<FVector2D>();
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnInputActionUpdated.Broadcast(Value);
    }
}

// 当GA结束时，这个函数被自动调用，用于清理
void UAbilityTask_ListenForInputAction::OnDestroy(bool bInOwnerFinished)
{
    // 获取输入组件并移除我们之前添加的绑定，防止内存泄漏
    AActor* OwnerActor = AbilitySystemComponent->GetOwnerActor();
    APlayerController* PC = Cast<APlayerController>(OwnerActor);
    if (!PC)
    {
        if (APawn* Pawn = Cast<APawn>(OwnerActor))
        {
            PC = Cast<APlayerController>(Pawn->GetController());
        }
    }

    if (PC && PC->InputComponent)
    {
        if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
        {
            EIC->RemoveBindingByHandle(BindingHandle);
        }
    }

    Super::OnDestroy(bInOwnerFinished);
}