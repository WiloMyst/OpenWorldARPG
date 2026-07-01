// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "TeamSetupSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;

/**
 * 编队界面槽位 / 头像列表项 (对应 WBP_TeamSetupSlot)。
 * 可作为已拥有角色头像列表的 Item，也可作为 DropZone 占位 Widget。
 */
UCLASS()
class OPENWORLDARPG_API UTeamSetupSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTeamSetupSlotWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "TeamSetupSlot")
    void InitializeSlot(const FGameplayTag& InCharacterTag, const FText& InDisplayName, UTexture2D* InHeadIcon, int32 InSlotIndex = -1);

    UFUNCTION(BlueprintPure, Category = "TeamSetupSlot")
    FGameplayTag GetCharacterTag() const { return CharacterTag; }

    /** -1 表示头像列表 Item 而非 DropZone */
    UFUNCTION(BlueprintPure, Category = "TeamSetupSlot")
    int32 GetSlotIndex() const { return SlotIndex; }

    UFUNCTION(BlueprintPure, Category = "TeamSetupSlot")
    bool IsEmpty() const { return !CharacterTag.IsValid(); }

    UFUNCTION(BlueprintCallable, Category = "TeamSetupSlot")
    void SetHighlighted(bool bIsHighlighted);

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotClickedSignature, const FGameplayTag&, CharacterTag, int32, SlotIndex);

    UPROPERTY(BlueprintAssignable, Category = "TeamSetupSlot|Events")
    FOnSlotClickedSignature OnSlotClicked;

protected:
    UFUNCTION()
    void OnButtonClicked();

    // --- 绑定控件 ---

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Widgets", meta = (BindWidget))
    TObjectPtr<UImage> HeadIconImage;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Widgets", meta = (BindWidget))
    TObjectPtr<UButton> ItemButton;

    // --- 数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Data")
    FGameplayTag CharacterTag;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Data")
    int32 SlotIndex = -1;
};
