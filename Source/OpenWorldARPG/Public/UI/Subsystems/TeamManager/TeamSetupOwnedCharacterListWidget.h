// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "TeamSetupOwnedCharacterListWidget.generated.h"

class UPanelWidget;
class UTeamSetupSlotWidget;
class UCharacterManagerSubsystem;

/**
 * 编队界面"已拥有角色列表"子组件（对应 WBP_TeamSetupOwnedCharacterList）。
 *
 * 【职责单一】
 * - 仅负责从 CharacterManagerSubsystem 拉取已拥有角色头像，生成 Item Widget 并排布。
 * - 不关心 3D 展台、不关心 PendingTeam 的写入逻辑。
 * - 玩家点击头像时，通过委托 OnOwnedCharacterClicked 向上级（主壳子 UTeamSetupScreenWidget）广播。
 *
 * 【组件化设计】
 * - 主壳子通过 RefreshList(PendingTeam) 通知本组件刷新；
 *   传入 PendingTeam 仅用于标记"已上阵"状态（高亮/灰底），不修改 PendingTeam。
 * - 上级监听 OnOwnedCharacterClicked 委托，自行决定放入哪个槽位。
 */
UCLASS()
class OPENWORLDARPG_API UTeamSetupOwnedCharacterListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTeamSetupOwnedCharacterListWidget(const FObjectInitializer& ObjectInitializer);

    /**
     * 刷新已拥有角色头像列表。
     * @param CurrentPendingTeam 当前编队状态（仅用于标记已上阵角色，不修改）
     */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup|OwnedCharacterList")
    void RefreshList(const TArray<FGameplayTag>& CurrentPendingTeam);

public:
    /** 当玩家点击某个已拥有角色头像时触发（向上级广播） */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnedCharacterClickedSignature, const FGameplayTag&, CharacterTag);

    UPROPERTY(BlueprintAssignable, Category = "TeamSetup|OwnedCharacterList|Events")
    FOnOwnedCharacterClickedSignature OnOwnedCharacterClicked;

protected:
    /** 容纳头像 Item 的容器（ScrollBox / WrapBox / HorizontalBox 均可） */
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|OwnedCharacterList|Widgets", meta = (BindWidget))
    TObjectPtr<UPanelWidget> CharacterListContainer;

    /** 头像 Item Widget 类（蓝图配置） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|OwnedCharacterList|Config")
    TSubclassOf<UTeamSetupSlotWidget> SlotItemClass;

private:
    /** 接收子 Item 的点击事件，转发为对外的委托广播 */
    UFUNCTION()
    void HandleItemClicked(const FGameplayTag& CharacterTag, int32 SlotIndex);
};
