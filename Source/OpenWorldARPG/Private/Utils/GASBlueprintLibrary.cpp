// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/GASBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

FString UGASBlueprintLibrary::GetLastPartOfGameplayTag(const FGameplayTag& InTag)
{
    if (!InTag.IsValid())
    {
        return FString();
    }

    FString TagString = InTag.ToString();
    int32 LastDotIndex;
    if (TagString.FindLastChar(TEXT('.'), LastDotIndex))
    {
        // RightChop 会移除从左边数 LastDotIndex + 1 个字符
        return TagString.RightChop(LastDotIndex + 1);
    }
    return TagString;
}

void UGASBlueprintLibrary::CancelAbilitiesWithTags(UAbilitySystemComponent* ASC, const FGameplayTagContainer& WithTags)
{
	if (ASC)
	{
		ASC->CancelAbilities(&WithTags, nullptr, nullptr);
	}
}
