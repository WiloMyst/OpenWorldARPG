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
 * 编队界面"已拥有角色列表"子组件 (对应 WBP_TeamSetupOwnedCharacterList)。
 * 从 CharacterManagerSubsystem 拉取头像生成 Item，点击时通过委托广播。
 * 不关心 3D 展台和 PendingTeam 写入逻辑。
 */
UCLASS()
class OPENWORLDARPG_API UTeamSetupOwnedCharacterListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTeamSetupOwnedCharacterListWidget(const FObjectInitializer& ObjectInitializer);

    /** 刷新列表（CurrentPendingTeam 仅用于标记已上阵角色） */
    UFUNCTION(BlueprintCallable, Category = "TeamSetup|OwnedCharacterList")
    void RefreshList(const TArray<FGameplayTag>& CurrentPendingTeam);

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnedCharacterClickedSignature, const FGameplayTag&, CharacterTag);

    UPROPERTY(BlueprintAssignable, Category = "TeamSetup|OwnedCharacterList|Events")
    FOnOwnedCharacterClickedSignature OnOwnedCharacterClicked;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "TeamSetup|OwnedCharacterList|Widgets", meta = (BindWidget))
    TObjectPtr<UPanelWidget> CharacterListContainer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamSetup|OwnedCharacterList|Config")
    TSubclassOf<UTeamSetupSlotWidget> SlotItemClass;

private:
    UFUNCTION()
    void HandleItemClicked(const FGameplayTag& CharacterTag, int32 SlotIndex);
};
