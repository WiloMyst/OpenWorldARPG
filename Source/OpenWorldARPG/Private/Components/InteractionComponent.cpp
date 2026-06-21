// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Components/InteractionComponent.h"
#include "World/Interactables/PickableItemBase.h"
#include "Managers/InventoryManagerSubsystem.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"

UInteractionComponent::UInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;

    // 网络同步：组件需要复制才能让 Server RPC 工作
    SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        InventorySubsystem = GI->GetSubsystem<UInventoryManagerSubsystem>();
        if (InventorySubsystem)
        {
            InventorySubsystem->OnItemDropped.AddDynamic(this, &UInteractionComponent::HandleOnItemDropped);
        }
    }
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 仅在本地控制端执行拾取检测（避免服务器为所有客户端角色做检测）
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn && !OwnerPawn->IsLocallyControlled()) return;

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector StartLoc = OwnerActor->GetActorLocation();
    FVector EndLoc = StartLoc;
    float SphereRadius = 150.0f;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    // 改为多体检测，支持多个物品重叠
    TArray<FHitResult> HitResults;
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        this, StartLoc, EndLoc, SphereRadius, ObjectTypes,
        false, ActorsToIgnore, EDrawDebugTrace::None, HitResults, true);

    TArray<AActor*> NewPickableItems;
    for (const FHitResult& Hit : HitResults)
    {
        if (APickableItemBase* Item = Cast<APickableItemBase>(Hit.GetActor()))
        {
            NewPickableItems.Add(Item);
        }
    }

    // 比对新旧列表是否发生变化
    bool bChanged = false;
    if (NewPickableItems.Num() != CurrentPickableItems.Num())
    {
        bChanged = true;
    }
    else
    {
        for (int32 i = 0; i < NewPickableItems.Num(); ++i)
        {
            // 注意：由于旧列表存的是 TWeakObjectPtr，如果物品被销毁，Get() 会返回 nullptr，
            // 此时比较必为 false（由于新扫描到的数组不可能包含 nullptr），精准修复了销毁导致 UI 残留的 Bug。
            if (NewPickableItems[i] != CurrentPickableItems[i].Get())
            {
                bChanged = true;
                break;
            }
        }
    }

    // 仅当可交互对象列表发生改变时才广播
    if (bChanged)
    {
        CurrentPickableItems.Empty();
        for (AActor* Item : NewPickableItems)
        {
            CurrentPickableItems.Add(Item);
        }
        OnPickableListChangedDelegate.Broadcast(NewPickableItems);
    }
}

bool UInteractionComponent::IsCharacterInStandby() const
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

int32 UInteractionComponent::GetOwnerCharacterID() const
{
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwner());
    if (PlayerChar)
    {
        return PlayerChar->GetCharacterTag().GetTagName().GetNumber();
    }
    return -1;
}

// --- 拾取物品 (Client → Server RPC) ---

void UInteractionComponent::PickUpItem()
{
    // 客户端：只发送请求到服务器
    if (IsCharacterInStandby() || CurrentPickableItems.IsEmpty()) return;

    // 默认拾取列表里第一个有效的物品 (若要做《鸣潮》的一键拾取全部，可去掉这里的 break 循环发送)
    for (TWeakObjectPtr<AActor> WeakItem : CurrentPickableItems)
    {
        if (APickableItemBase* PickableItem = Cast<APickableItemBase>(WeakItem.Get()))
        {
            Server_PickUpItem(PickableItem->ItemID, PickableItem->ItemAmount);
            break; 
        }
    }
}

bool UInteractionComponent::Server_PickUpItem_Validate(int32 ItemID, int32 Amount)
{
    return ItemID > 0 && Amount > 0;
}

void UInteractionComponent::Server_PickUpItem_Implementation(int32 ItemID, int32 Amount)
{
    // 服务器端：执行拾取逻辑（权威操作）
    if (IsCharacterInStandby() || !InventorySubsystem) return;

    InventorySubsystem->AddItem(ItemID, Amount);

    // 服务器端销毁可拾取物品
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    FVector OwnerLoc = OwnerActor->GetActorLocation();
    float SearchRadius = 200.0f;

    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);

    GetWorld()->OverlapMultiByObjectType(
        OverlapResults,
        OwnerLoc,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECollisionChannel::ECC_PhysicsBody),
        FCollisionShape::MakeSphere(SearchRadius),
        QueryParams
    );

    for (const FOverlapResult& Result : OverlapResults)
    {
        if (APickableItemBase* Item = Cast<APickableItemBase>(Result.GetActor()))
        {
            if (Item->ItemID == ItemID)
            {
                Item->Destroy();
                break;
            }
        }
    }
}

// --- 丢弃物品 (Client → Server RPC) ---

void UInteractionComponent::DropItemByGUID(FGuid ItemGUID, int32 DropAmount)
{
    // 客户端：只发送请求到服务器
    Server_DropItemByGUID(ItemGUID, DropAmount);
}

bool UInteractionComponent::Server_DropItemByGUID_Validate(FGuid ItemGUID, int32 DropAmount)
{
    return DropAmount > 0;
}

void UInteractionComponent::Server_DropItemByGUID_Implementation(FGuid ItemGUID, int32 DropAmount)
{
    // 服务器端：执行丢弃逻辑（权威操作）
    if (!InventorySubsystem) return;
    InventorySubsystem->RemoveItemByGUID(ItemGUID, DropAmount);
}

void UInteractionComponent::DropItem(int32 DropIndex, int32 DropAmount)
{
    if (!InventorySubsystem) return;
    InventorySubsystem->RemoveItemByIndex(DropIndex, DropAmount);
}

bool UInteractionComponent::UseItemByGUID(FGuid ItemGUID, int32 UseAmount)
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->UseItem(ItemGUID, GetOwnerCharacterID(), UseAmount);
}

bool UInteractionComponent::EquipItemByGUID(FGuid ItemGUID)
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->EquipItem(ItemGUID, GetOwnerCharacterID());
}

bool UInteractionComponent::UnequipItemByGUID(FGuid ItemGUID)
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->UnequipItem(ItemGUID);
}

TArray<AActor*> UInteractionComponent::GetCurrentPickableItems() const
{
    TArray<AActor*> Result;
    for (TWeakObjectPtr<AActor> WeakItem : CurrentPickableItems)
    {
        if (WeakItem.IsValid())
        {
            Result.Add(WeakItem.Get());
        }
    }
    return Result;
}

void UInteractionComponent::HandleOnItemDropped(int32 ItemID, int32 DroppedAmount)
{
    // 仅在服务器端生成丢弃物品（服务器权威）
    if (!GetOwner()->HasAuthority()) return;
    if (IsCharacterInStandby()) return;

    SpawnDroppedItem(ItemID, DroppedAmount);
}

void UInteractionComponent::SpawnDroppedItem(int32 ItemID, int32 DroppedAmount)
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

    APickableItemBase* DroppedItem = GetWorld()->SpawnActor<APickableItemBase>(APickableItemBase::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
    if (DroppedItem)
    {
        DroppedItem->InitializeItem(ItemID, DroppedAmount);
        FVector LinearVelocity = (OwnerForward * 300.0f) + FVector(0.0f, 0.0f, 300.0f);
        DroppedItem->ApplyThrowPhysics(LinearVelocity);
    }
}
