// Copyright 2025 WiloMyst. All Rights Reserved.

#include "UI/HUD/TeamListWidget.h"
#include "UI/HUD/TeamListSlotWidget.h"
#include "Components/VerticalBox.h"
#include "Managers/TeamManagerSubsystem.h"
#include "Managers/CharacterManagerSubsystem.h"
#include "Data/CharacterRegistryRow.h"
#include "Core/PlayerStates/MainGamePlayerState.h"
#include "Characters/PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h" // 【新增】引入定时器管理器

void UTeamListWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 2. 绑定队伍管理器
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>())
        {
            TeamManager->OnTeamListUpdatedDelegate.AddDynamic(this, &UTeamListWidget::UpdateTeamList);
        }
    }
}

void UTeamListWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 【核心修复：延迟一帧初始化】
    // 解决 UMG 嵌套控件时，父控件（MainHUD）的实例属性还没来得及传递给子控件的问题
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick([this]()
        {
            // 确保控件还没被销毁
            if (IsValid(this) && TeamSlotPool.IsEmpty())
            {
                InitTeamSlotPool();
                UpdateTeamList();
            }
        });
    }
}

void UTeamListWidget::NativeDestruct()
{
    // 解绑委托，防止内存泄漏
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>())
        {
            TeamManager->OnTeamListUpdatedDelegate.RemoveAll(this);
        }
    }

    Super::NativeDestruct();
}

void UTeamListWidget::InitTeamSlotPool()
{
    // 此时已经延迟了一帧，如果还是空，说明蓝图里是真的彻底没配
    if (!TeamListSlotClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[TeamListWidget] 严重错误：未配置 TeamListSlotClass！请打开 WBP_TeamList 蓝图，在 Class Defaults 中指定！"));
        return;
    }

    if (!TeamList) return;

    for (int32 i = 0; i < TeamSlotPoolSize; ++i)
    {
        UTeamListSlotWidget* NewSlot = CreateWidget<UTeamListSlotWidget>(this, TeamListSlotClass);
        if (NewSlot)
        {
            NewSlot->SetVisibility(ESlateVisibility::Collapsed);
            TeamList->AddChild(NewSlot);
            TeamSlotPool.Add(NewSlot);
        }
    }
}

UTeamListSlotWidget* UTeamListWidget::GetOrCreateSlot(int32 Index)
{
    if (TeamSlotPool.IsValidIndex(Index))
    {
        return TeamSlotPool[Index];
    }

    if (TeamList && TeamListSlotClass)
    {
        UTeamListSlotWidget* NewSlot = CreateWidget<UTeamListSlotWidget>(this, TeamListSlotClass);
        if (NewSlot)
        {
            TeamList->AddChild(NewSlot);
            TeamSlotPool.Add(NewSlot);
            return NewSlot;
        }
    }
    return nullptr;
}

void UTeamListWidget::UpdateTeamList()
{
    if (!TeamList) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UTeamManagerSubsystem* TeamManager = GI->GetSubsystem<UTeamManagerSubsystem>();
    UCharacterManagerSubsystem* CharManager = GI->GetSubsystem<UCharacterManagerSubsystem>();
    if (!TeamManager || !CharManager) return;

    TArray<FGameplayTag> TeamTags = TeamManager->GetCurrentTeamCharacterTags();

    UE_LOG(LogTemp, Warning, TEXT("[TeamListWidget] 执行队伍刷新，当前队伍人数: %d"), TeamTags.Num());

    APlayerController* PC = GetOwningPlayer();
    AMainGamePlayerState* PS = PC ? PC->GetPlayerState<AMainGamePlayerState>() : nullptr;

    int32 MaxLoopCount = FMath::Max(TeamTags.Num(), TeamSlotPool.Num());

    for (int32 i = 0; i < MaxLoopCount; ++i)
    {
        UTeamListSlotWidget* SlotWidget = GetOrCreateSlot(i);
        if (!SlotWidget) continue;

        if (i < TeamTags.Num())
        {
            const FGameplayTag& CharTag = TeamTags[i];
            FCharacterRegistryRow OutRow;

            if (CharManager->GetCharacterRegistryRowByTag(CharTag, OutRow))
            {
                UAbilitySystemComponent* CharASC = nullptr;
                if (PS)
                {
                    if (APlayerCharacter* TeamChar = PS->GetTeamCharacterByTag(CharTag))
                    {
                        CharASC = TeamChar->GetAbilitySystemComponent();
                    }
                }

                SlotWidget->InitSlot(OutRow.CharacterName, OutRow.HeadIcon, CharTag, i, CharASC);
                SlotWidget->SetVisibility(ESlateVisibility::Visible);
            }
        }
        else
        {
            SlotWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}