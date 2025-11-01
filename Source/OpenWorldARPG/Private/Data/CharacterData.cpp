// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/CharacterData.h"
#include "Data/CharacterGeneralDataAsset.h"
#include "GAS/AttributeSets/AS_Player.h"
#include "GameplayEffect.h"
#include "Engine/CurveTable.h"
#include "GameFramework/PlayerState.h"

ACharacterData::ACharacterData()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	// 让这个Actor在网络游戏中可以被复制
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	// 创建能力系统组件(ASC)。
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 设置ASC的复制模式
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed); // 或 Minimal

	// 创建属性集(AttributeSet)。
	AttributeSet = CreateDefaultSubobject<UAS_Player>(TEXT("AttributeSet"));
}

void ACharacterData::InitializeData(const FCharacterSaveData& InSaveData, UCharacterDataAsset* InDataAsset)
{
	// 1. 安全性检查
	if (!InDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharacterData::Initialize - InDataAsset is nullptr. Initialization aborted."));
		return;
	}
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ACharacterData::Initialize - AbilitySystemComponent is nullptr. Initialization aborted."));
		return;
	}

	// 2. 数据映射
	// 静态数据
	DataSourceAsset = InDataAsset;
	CharacterTag = InDataAsset->CharacterTag;

	if(InDataAsset->ElementType.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(InDataAsset->ElementType, 1);
	}
	if(InDataAsset->WeaponType.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(InDataAsset->WeaponType, 1);
	}

	// 动态数据
	CharacterLevel = InSaveData.CharacterLevel;
	Experience = InSaveData.Experience;
	AscensionLevel = InSaveData.AscensionLevel;
	FriendshipLevel = InSaveData.FriendshipLevel;
	ConstellationLevel = InSaveData.ConstellationLevel;




	// 4. 根据等级和曲线表，计算并应用基础属性 (核心逻辑)
	//if (InDataAsset->BaseAttributesEffect && InDataAsset->BaseStatsGrowthCurveTable)
	//{
	//    // a. 创建一个GameplayEffectSpec，这是GE的“待应用实例”
	//    FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	//    ContextHandle.AddSourceObject(this); // 将自身作为效果来源
	//    FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InDataAsset->BaseAttributesEffect, Level, ContextHandle);

	//    if (SpecHandle.IsValid())
	//    {
	//        // b. 从曲线表中查找当前等级对应的所有基础属性值
	//        UCurveTable* StatsTable = InDataAsset->BaseStatsGrowthCurveTable.LoadSynchronous(); // 在初始化时可以同步加载
	//        if (StatsTable)
	//        {
	//            // 定义一个辅助Lambda函数，让代码更简洁
	//            auto SetMagnitudeFromCurve = [&](const FName& RowName, const FGameplayTag& DataTag)
	//                {
	//                    const FRealCurve* Curve = StatsTable->FindCurve(RowName, TEXT(""));
	//                    if (Curve)
	//                    {
	//                        const float Value = Curve->GetFloatValue(Level);
	//                        SpecHandle.Data->SetSetByCallerMagnitude(DataTag, Value);
	//                    }
	//                };

	//            // c. 使用Lambda函数，为GE Spec中的每个SetByCallerMagnitude赋值
	//            SetMagnitudeFromCurve(FName("BaseHP"), FGameplayTag::RequestGameplayTag(FName("Data.BaseHP")));
	//            SetMagnitudeFromCurve(FName("BaseATK"), FGameplayTag::RequestGameplayTag(FName("Data.BaseATK")));
	//            SetMagnitudeFromCurve(FName("BaseDEF"), FGameplayTag::RequestGameplayTag(FName("Data.BaseDEF")));
	//            // ... 为你的所有其他基础属性（如暴击率、暴击伤害等）添加更多调用 ...
	//        }

	//        // d. 将配置好的GE Spec应用到自己身上
	//        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	//        // e. [重要] 应用完基础属性后，立即将当前属性值设置为最大值（例如，满血满蓝）
	//        // 这样做可以避免角色生成时是1点血。
	//        // 假设你的AttributeSet是UMyAttributeSet类型
	//        if (const UAS_Player* MyAttributeSet = Cast<const UAS_Player>(AttributeSet))
	//        {
	//            AbilitySystemComponent->SetNumericAttributeBase(MyAttributeSet->GetHealthAttribute(), MyAttributeSet->GetMaxHealth());
	//            AbilitySystemComponent->SetNumericAttributeBase(MyAttributeSet->GetStaminaAttribute(), MyAttributeSet->GetMaxStamina());
	//            //AbilitySystemComponent->SetNumericAttributeBase(MyAttributeSet->GetEnergyAttribute(), MyAttributeSet->GetMaxEnergy());
	//        }
	//    }
	//}


	// 5. 赋予所有 Gameplay Abilities (GA)
	
	// 赋予角色共有的GA
	const FString AssetPath = TEXT("/Game/Characters/_Shared/Data/DA_CharacterGeneral.DA_CharacterGeneral");
	TSoftObjectPtr<UCharacterGeneralDataAsset> GeneralDataAsset = TSoftObjectPtr<UCharacterGeneralDataAsset>(FSoftObjectPath(AssetPath));
	UCharacterGeneralDataAsset* LoadedGeneralDataAsset = GeneralDataAsset.LoadSynchronous();
	if (LoadedGeneralDataAsset)
	{
		if (!LoadedGeneralDataAsset->GeneralPermanentAbilityClasses.IsEmpty())
		{
			for (TSubclassOf<UGameplayAbility> AbilityClass : LoadedGeneralDataAsset->GeneralPermanentAbilityClasses)
			{
				if (AbilityClass)
				{
					AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
				}
			}
		}
		if (!LoadedGeneralDataAsset->GeneralAbilityClasses.IsEmpty())
		{
			for (TSubclassOf<UGameplayAbility> AbilityClass : LoadedGeneralDataAsset->GeneralAbilityClasses)
			{
				if (AbilityClass)
				{
					AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ACharacterData::InitializeData - Failed to load CharacterGeneralDataAsset at path: %s"), *AssetPath);
	}


	// 赋予普通攻击
	if (!InDataAsset->NormalAttack.AbilityClasses.IsEmpty())
	{
		for(TSubclassOf<UGameplayAbility> AbilityClass : InDataAsset->NormalAttack.AbilityClasses)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}
	}
	// 赋予重击
	if (!InDataAsset->HeavyAttack.AbilityClasses.IsEmpty())
	{
		for(TSubclassOf<UGameplayAbility> AbilityClass : InDataAsset->HeavyAttack.AbilityClasses)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}
	}
	// 赋予下落攻击
	if (!InDataAsset->PlungeAttack.AbilityClasses.IsEmpty())
	{
		for(TSubclassOf<UGameplayAbility> AbilityClass : InDataAsset->PlungeAttack.AbilityClasses)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}
	}
	// 赋予战技攻击
	if (!InDataAsset->SkillAttack.AbilityClasses.IsEmpty())
	{
		for(TSubclassOf<UGameplayAbility> AbilityClass : InDataAsset->SkillAttack.AbilityClasses)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}
	}
	// 赋予终结技攻击
	if (!InDataAsset->UltimateAttack.AbilityClasses.IsEmpty())
	{
		for(TSubclassOf<UGameplayAbility> AbilityClass : InDataAsset->UltimateAttack.AbilityClasses)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}
	}
	// 赋予被动天赋
	for (const FTalentConfig& Talent : InDataAsset->PassiveTalents)
	{
		if (!Talent.AbilityClasses.IsEmpty())
		{
			for(TSubclassOf<UGameplayAbility> AbilityClass : Talent.AbilityClasses)
			{
				if (AbilityClass)
				{
					AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
				}
			}
		}
	}


	UE_LOG(LogTemp, Log, TEXT("CharacterData for %s initialized successfully."), *CharacterTag.ToString());
}

