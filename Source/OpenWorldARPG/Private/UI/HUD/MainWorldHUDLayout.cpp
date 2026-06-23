// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/MainWorldHUDLayout.h"
#include "UI/HUD/PlayerCharacterBarWidget.h"
#include "UI/HUD/GameplayListWidget.h"
#include "UI/HUD/InteractionListWidget.h"
#include "Components/HeroUIExtensionComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PlayerStates/GameplayPlayerState.h"
#include "Managers/UIManagerSubsystem.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Data/CharacterRegistryRow.h"
#include "AbilitySystemComponent.h"

void UMainWorldHUDLayout::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 初始化显示状态 (避免 Tick 中反复设置)
    if (WBP_AimStar) 
    {
        WBP_AimStar->SetRenderOpacity(0.0f);
    }
    
    // 初始化交互列表占位防闪烁
    if (WBP_InteractionList) 
    {
        WBP_InteractionList->UpdateInteractionList(TArray<AActor*>());
    }

    if (WBP_PlayerCharacterBar)
    {
        // 初始化为满血占位防闪烁 (1.0, 1.0)，随后通过 ExtensionComp 获取真实数据
        WBP_PlayerCharacterBar->UpdateHealth(1.0f, 1.0f);
    }

    // 绑定 UI 扩展组件（MVVM：不再直接依赖 ASC / InteractionComponent）
    if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwningPlayerPawn()))
    {
        if (UHeroUIExtensionComponent* ExtensionComp = PlayerChar->GetHeroUIExtensionComp())
        {
            BindToExtensionComp(ExtensionComp);
        }
    }

    // 监听 Pawn 切换（角色换人时重新绑定 ExtensionComp）
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->OnPossessedPawnChanged.AddDynamic(this, &UMainWorldHUDLayout::OnPossessedPawnChanged);
    }
}

void UMainWorldHUDLayout::NativeDestruct()
{
    // 解绑 UI 扩展组件委托
    UnbindFromExtensionComp();

    Super::NativeDestruct();
}

// ==========================================
// MVVM 回调（监听 ExtensionComponent 的统一事件）
// ==========================================

void UMainWorldHUDLayout::OnHealthChanged(float NewHealth, float NewMaxHealth)
{
    CurrentHealth = NewHealth;
    CurrentMaxHealth = NewMaxHealth;

    // 转发数据给子控件，主HUD自身不再处理具体的计算和表现
    if (WBP_PlayerCharacterBar)
    {
        WBP_PlayerCharacterBar->UpdateHealth(CurrentHealth, CurrentMaxHealth);
    }
}

void UMainWorldHUDLayout::OnAimingStateChanged(bool bIsAiming)
{
    if (WBP_AimStar)
    {
        WBP_AimStar->SetRenderOpacity(bIsAiming ? 1.0f : 0.0f);
    }
}

void UMainWorldHUDLayout::OnInteractionListChanged(const TArray<AActor*>& InteractableActors)
{
    // 转发数据给交互列表控件
    if (WBP_InteractionList)
    {
        WBP_InteractionList->UpdateInteractionList(InteractableActors);
    }
}

// ==========================================
// ExtensionComponent 绑定/解绑
// ==========================================

void UMainWorldHUDLayout::BindToExtensionComp(UHeroUIExtensionComponent* NewExtensionComp)
{
    if (!NewExtensionComp) return;

    // 先解绑旧 ExtensionComp
    UnbindFromExtensionComp();

    CachedExtensionComp = NewExtensionComp;

    // 先监听 ExtensionComp 的统一事件，防止错过后续 BindToActor 时触发的内部初始广播
    NewExtensionComp->OnHealthChanged.AddDynamic(this, &UMainWorldHUDLayout::OnHealthChanged);
    NewExtensionComp->OnAimingStateChanged.AddDynamic(this, &UMainWorldHUDLayout::OnAimingStateChanged);
    NewExtensionComp->OnInteractionListChanged.AddDynamic(this, &UMainWorldHUDLayout::OnInteractionListChanged);

    // 绑定到 Owner Actor 的 ASC 与 InteractionComponent
    NewExtensionComp->BindToActor(GetOwningPlayerPawn());

    // 双重保险，主动拉取一次当前数据，防止组件已经初始化完毕而错失了广播
    OnHealthChanged(NewExtensionComp->GetCurrentHealth(), NewExtensionComp->GetCurrentMaxHealth());
    OnAimingStateChanged(NewExtensionComp->IsAiming());
}

void UMainWorldHUDLayout::UnbindFromExtensionComp()
{
    if (!CachedExtensionComp.IsValid()) return;

    CachedExtensionComp->OnHealthChanged.RemoveAll(this);
    CachedExtensionComp->OnAimingStateChanged.RemoveAll(this);
    CachedExtensionComp->OnInteractionListChanged.RemoveAll(this);
    CachedExtensionComp->UnbindAll();
    CachedExtensionComp = nullptr;
}

void UMainWorldHUDLayout::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    // 角色切换：重新绑定到新角色的 ExtensionComp
    if (APlayerCharacter* NewChar = Cast<APlayerCharacter>(NewPawn))
    {
        if (UHeroUIExtensionComponent* ExtensionComp = NewChar->GetHeroUIExtensionComp())
        {
            BindToExtensionComp(ExtensionComp);
        }
    }
}