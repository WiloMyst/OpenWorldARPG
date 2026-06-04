// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/BackpackComponent.h"
#include "Items/ItemBase.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

UBackpackComponent::UBackpackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 优化：不需要每帧执行射线检测，0.1秒测一次足够了
}

void UBackpackComponent::BeginPlay()
{
    Super::BeginPlay();

    // 缓存 Subsystem 并绑定丢弃事件
    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        InventorySubsystem = GI->GetSubsystem<UInventoryManagerSubsystem>();
        if (InventorySubsystem)
        {
            // 只绑定丢弃事件。添加物品的销毁逻辑已移至 PickUpItem 内部
            InventorySubsystem->OnItemDropped.AddDynamic(this, &UBackpackComponent::HandleOnItemDropped);
        }
    }
}

// 对应蓝图的 Event Tick + 针对Object进行球体追踪
void UBackpackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    // 球体追踪参数设置
    FVector StartLoc = OwnerActor->GetActorLocation();
    FVector EndLoc = StartLoc; // 起点和终点相同，原地进行球体检测
    float SphereRadius = 150.0f;

    // 对应蓝图中的 ObjectTypes (这里假设物品是 PhysicsBody 或 WorldDynamic)
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    FHitResult HitResult;
    bool bHit = UKismetSystemLibrary::SphereTraceSingleForObjects(
        this,
        StartLoc,
        EndLoc,
        SphereRadius,
        ObjectTypes,
        false, // bTraceComplex
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResult,
        true // Ignore Self
    );

    if (bHit && HitResult.GetActor())
    {
        // 对应蓝图的 Cast to BP_Item
        CurrentPickableItem = Cast<AItemBase>(HitResult.GetActor());
    }
    else
    {
        CurrentPickableItem = nullptr;
    }
}

// 辅助函数：通过 GAS 检查角色状态 (替代蓝图中的 HasMatchingGameplayTag)
bool UBackpackComponent::IsCharacterInStandby() const
{
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (PlayerChar)
    {
        if (UAbilitySystemComponent* ASC = PlayerChar->GetAbilitySystemComponent())
        {
            FGameplayTag StandbyTag = PlayerChar->GetStandbyStateTag();
            return StandbyTag.IsValid() && ASC->HasMatchingGameplayTag(StandbyTag);
        }
    }
    return false;
}

// 对应蓝图自定义事件：拾取
void UBackpackComponent::PickUpItem()
{
    if (IsCharacterInStandby() || !CurrentPickableItem.IsValid() || !InventorySubsystem)
    {
        return;
    }

    AItemBase* PickableItem = CurrentPickableItem.Get();

    // 提取物品数据并添加到背包
    int32 ItemID = PickableItem->ItemID;
    int32 Amount = PickableItem->ItemAmount;

    // 安全、封闭的拾取逻辑：直接在这里销毁 Actor，绝不依赖全局 Delegate 广播
    InventorySubsystem->AddItem(ItemID, Amount);

    PickableItem->Destroy();
    CurrentPickableItem = nullptr;
}

// 对应蓝图自定义事件：丢弃
void UBackpackComponent::DropItem(int32 DropIndex, int32 DropAmount)
{
    if (!InventorySubsystem) return;

    // 这里会触发 Subsystem 内部的数据扣减，并广播 OnItemDropped 事件
    InventorySubsystem->RemoveItemByIndex(DropIndex, DropAmount);
}

// 对应蓝图事件：HandleOnItemDropped
void UBackpackComponent::HandleOnItemDropped(int32 ItemID, int32 DroppedAmount)
{
    // 如果角色正在待机（比如在过场动画中），则不允许物理丢出表现
    if (IsCharacterInStandby()) return;

    SpawnDroppedItem(ItemID, DroppedAmount);
}

// 对应蓝图事件：将物品从角色位置丢出
void UBackpackComponent::SpawnDroppedItem(int32 ItemID, int32 DroppedAmount)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector OwnerLoc = OwnerActor->GetActorLocation();
    FVector OwnerForward = OwnerActor->GetActorForwardVector();
    FVector SpawnLocation = OwnerLoc + (OwnerForward * 100.0f);
    FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    SpawnParams.Instigator = Cast<APawn>(OwnerActor);

    // 【核心修改】：直接生成 AItemBase 的 StaticClass
    AItemBase* DroppedItem = GetWorld()->SpawnActor<AItemBase>(AItemBase::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);

    if (DroppedItem)
    {
        // 【核心修改】：使用我们刚写好的运行时初始化函数，它会自动赋予 ID 并加载模型
        DroppedItem->InitializeItem(ItemID, DroppedAmount);

        FVector LinearVelocity = (OwnerForward * 300.0f) + FVector(0.0f, 0.0f, 300.0f);
        DroppedItem->ApplyThrowPhysics(LinearVelocity);
    }
}