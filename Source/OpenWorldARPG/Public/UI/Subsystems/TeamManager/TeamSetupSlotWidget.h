// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "TeamSetupSlotWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 编队界面槽位 / 头像列表项 Widget（对应 WBP_TeamSetupSlot）。
 * 既可作为底部已拥有角色头像列表的 Item，也可作为中部 DropZone 的占位 Widget。
 */
UCLASS()
class OPENWORLDARPG_API UTeamSetupSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTeamSetupSlotWidget(const FObjectInitializer& ObjectInitializer);

    /** 初始化槽位数据 */
    UFUNCTION(BlueprintCallable, Category = "TeamSetupSlot")
    void InitializeSlot(const FGameplayTag& InCharacterTag, const FText& InDisplayName, UTexture2D* InHeadIcon, int32 InSlotIndex = -1);

    /** 获取此 Slot 对应的角色 Tag */
    UFUNCTION(BlueprintPure, Category = "TeamSetupSlot")
    FGameplayTag GetCharacterTag() const { return CharacterTag; }

    /** 获取槽位索引（-1 表示这是头像列表 Item 而非 DropZone） */
    UFUNCTION(BlueprintPure, Category = "TeamSetupSlot")
    int32 GetSlotIndex() const { return SlotIndex; }

    /** 是否为空槽位 */
    UFUNCTION(BlueprintPure, Category = "TeamSetupSlot")
    bool IsEmpty() const { return !CharacterTag.IsValid(); }

    /** 设置选中高亮（蓝图实现） */
    UFUNCTION(BlueprintImplementableEvent, Category = "TeamSetupSlot")
    void SetHighlighted(bool bIsHighlighted);

public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotClickedSignature, const FGameplayTag&, CharacterTag, int32, SlotIndex);

    /** 当此 Slot 被点击时触发 */
    UPROPERTY(BlueprintAssignable, Category = "TeamSetupSlot|Events")
    FOnSlotClickedSignature OnSlotClicked;

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // --- 绑定控件 ---

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Widgets", meta = (BindWidget))
    TObjectPtr<UImage> HeadIconImage;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Widgets", meta = (BindWidget))
    TObjectPtr<UTextBlock> NameText;

    // --- 数据 ---

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Data")
    FGameplayTag CharacterTag;

    UPROPERTY(BlueprintReadOnly, Category = "TeamSetupSlot|Data")
    int32 SlotIndex = -1;
};
