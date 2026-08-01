// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/DialogueSystem/Components/DialogueComponent.h"
#include "Systems/DialogueSystem/DialogueManagerSubsystem.h"
#include "Systems/DialogueSystem/Data/DialogueGraphDataAsset.h"
#include "GameFramework/Character.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/LocalPlayerSubsystem.h"

UDialogueComponent::UDialogueComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDialogueComponent::BeginPlay()
{
    Super::BeginPlay();
}

UDialogueGraphDataAsset* UDialogueComponent::GetDefaultDialogue() const
{
    if (DialogueGraphs.IsEmpty())
    {
        return nullptr;
    }

    // 尝试从 AssetManager 获取已加载的第一个对话图
    const FPrimaryAssetId AssetId(TEXT("DialogueGraph"), FName(*DialogueGraphs[0].ToSoftObjectPath().GetAssetName()));
    return Cast<UDialogueGraphDataAsset>(UAssetManager::Get().GetPrimaryAssetObject(AssetId));
}

void UDialogueComponent::StartDialogue(ACharacter* InstigatorCharacter)
{
    if (DialogueGraphs.IsEmpty())
    {
        return;
    }

    // 获取 LocalPlayer 的 DialogueManagerSubsystem
    UDialogueManagerSubsystem* DialogueManager = nullptr;
    if (const UWorld* World = GetWorld())
    {
        if (const APlayerController* PC = World->GetFirstPlayerController())
        {
            if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                DialogueManager = LocalPlayer->GetSubsystem<UDialogueManagerSubsystem>();
            }
        }
    }

    if (!DialogueManager)
    {
        return;
    }

    // 构造对话图的 PrimaryAssetId（与 UDialogueGraphDataAsset::GetPrimaryAssetId 一致）
    const FPrimaryAssetId AssetId(TEXT("DialogueGraph"), FName(*DialogueGraphs[0].ToSoftObjectPath().GetAssetName()));

    // 若对话图已加载，直接开始对话
    if (UDialogueGraphDataAsset* LoadedGraph = Cast<UDialogueGraphDataAsset>(UAssetManager::Get().GetPrimaryAssetObject(AssetId)))
    {
        DialogueManager->StartDialogue(LoadedGraph, InstigatorCharacter, GetOwner());
        return;
    }

    // 异步加载对话图，加载完成后开始对话
    TWeakObjectPtr<UDialogueComponent> WeakThis(this);
    TWeakObjectPtr<UDialogueManagerSubsystem> WeakManager(DialogueManager);
    TWeakObjectPtr<ACharacter> WeakInstigator(InstigatorCharacter);
    TWeakObjectPtr<AActor> WeakOwner(GetOwner());

    UAssetManager::Get().LoadPrimaryAsset(AssetId, TArray<FName>(), FStreamableDelegate::CreateLambda(
        [WeakThis, WeakManager, WeakInstigator, WeakOwner, AssetId]()
        {
            if (!WeakThis.IsValid() || !WeakManager.IsValid() || !WeakInstigator.IsValid() || !WeakOwner.IsValid())
            {
                return;
            }

            if (UDialogueGraphDataAsset* LoadedGraph = Cast<UDialogueGraphDataAsset>(UAssetManager::Get().GetPrimaryAssetObject(AssetId)))
            {
                WeakManager->StartDialogue(LoadedGraph, WeakInstigator.Get(), WeakOwner.Get());
            }
        }));
}
