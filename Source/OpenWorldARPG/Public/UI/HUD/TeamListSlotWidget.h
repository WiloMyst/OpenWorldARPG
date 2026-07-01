// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TeamListSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UWidget;
class UAbilitySystemComponent;
class UTexture2D;

/**
 * 队伍角色槽位 UI（哑视图 Dumb View）
 * 不主动查询 Subsystem，数据由父容器传入。
 * 用 FDelegateHandle 安全解绑 Tag 事件委托。
 */
UCLASS(Abstract)
class OPENWORLDARPG_API UTeamListSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 初始化槽位（哑视图接口：父容器传入所有数据） */
    UFUNCTION(BlueprintCallable, Category = "UI|Team")
    void InitSlot(
        const FText& InCharacterName,
        TSoftObjectPtr<UTexture2D> InHeadIcon,
        FGameplayTag InCharacterTag,
        int32 InCharacterIndex,
        UAbilitySystemComponent* InASC
    );

    /** 重置槽位到空白状态（对象池复用时调用） */
    void ResetSlot();

protected:
    virtual void NativeDestruct() override;

    // --- 控件绑定 ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Text_CharacterName;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Text_CharacterIndex;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* Image_CharacterIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UWidget* DeathColor;

    // --- 配置与数据 ---

    UPROPERTY(EditDefaultsOnly, Category = "UI|Tags")
    FGameplayTag DeadStateTag;

    FGameplayTag CharacterTag;
    int32 CharacterIndex = 0;

    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
    FDelegateHandle DeadTagDelegateHandle;
    TSharedPtr<struct FStreamableHandle> IconLoadHandle;

private:
    void LoadCharacterIcon(TSoftObjectPtr<UTexture2D> SoftIcon);
    void OnIconLoaded(TSoftObjectPtr<UTexture2D> SoftIcon);
    void BindCharacterState(UAbilitySystemComponent* InASC);
    void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
};
