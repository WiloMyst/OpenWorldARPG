// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/AI/EnemyCharacter.h"
#include "GAS/AttributeSets/AS_Enemy.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // 1. ASC & AS_Enemy
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    AttributeSet = CreateDefaultSubobject<UAS_Enemy>(TEXT("AttributeSet"));

    // 2. Health Bar Component
    HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
    HealthBarComponent->SetupAttachment(RootComponent);
    HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
}

void AEnemyCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // 绑定血量变化回调
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
        AttributeSet->GetHealthAttribute()).AddUObject(this, &AEnemyCharacter::OnHealthAttributeChanged);
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 1. 对应蓝图: Bind Anim Layers
    BindAnimLayers();

    // 2. 对应蓝图: Update Health Bar (初始刷新)
    UpdateHealthBar();

    // 3. 对应蓝图: 生成Actor并附加到组件
    if (WeaponClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        EnemyWeapon = GetWorld()->SpawnActor<AActor>(WeaponClass, GetActorTransform(), SpawnParams);
        if (EnemyWeapon)
        {
            EnemyWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, WeaponSocketName);
        }
    }

    // 4. 对应蓝图: 赋予死亡GA
    if (DeathAbilityClass)
    {
        AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DeathAbilityClass, 1, INDEX_NONE, this));
    }
}

void AEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 对应蓝图: Orient To Screen
    OrientToScreen(HealthBarComponent);
}

void AEnemyCharacter::BindAnimLayers()
{
    if (AnimLayerClass && GetMesh())
    {
        GetMesh()->LinkAnimClassLayers(AnimLayerClass);
    }
}

void AEnemyCharacter::UpdateHealthBar()
{
    if (!HealthBarComponent || !HealthBarComponent->GetUserWidgetObject() || !AttributeSet) return;

    UUserWidget* Widget = HealthBarComponent->GetUserWidgetObject();

    // 【极致解耦设计】：利用反射调用 UI 的 UpdateHealth 事件，避免硬编码转换成 WBP_EnemyHealthBar
    // 假设你在 WBP 里有一个名为 UpdateHealth 的自定义事件/函数，带有 CurrentHealth 和 MaxHealth 两个 Float 参数
    UFunction* UpdateFunc = Widget->FindFunction(FName("UpdateHealth"));
    if (UpdateFunc)
    {
        struct {
            double Current;
            double Max;
        } Params;

        Params.Current = AttributeSet->GetHealth();
        Params.Max = AttributeSet->GetMaxHealth();

        Widget->ProcessEvent(UpdateFunc, &Params);
    }
}

void AEnemyCharacter::OrientToScreen(USceneComponent* SceneComp)
{
    if (!SceneComp) return;

    APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (CameraManager)
    {
        FRotator CameraRot = CameraManager->GetCameraRotation();
        SceneComp->SetWorldRotation(CameraRot);

        // 对应蓝图中的 Delta Rotation X=180, Y=180, Z=0
        // 在 C++ 的 FRotator 中，顺序是 (Pitch, Yaw, Roll)。X是Roll，Y是Pitch，Z是Yaw。
        SceneComp->AddLocalRotation(FRotator(180.0f, 0.0f, 180.0f));
    }
}

void AEnemyCharacter::MeleeAttack()
{
    ApplyDamage();
    if (ComboAttackMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();

        // 绑定结束委托，对应蓝图的 Completed, BlendOut, Interrupted
        AnimInst->OnMontageEnded.AddUniqueDynamic(this, &AEnemyCharacter::OnMontageEnded);

        PlayAnimMontage(ComboAttackMontage);
    }
}

void AEnemyCharacter::CancelMeleeAttack()
{
    if (ComboAttackMontage)
    {
        StopAnimMontage(ComboAttackMontage);
    }
    CorrectPawnOrient();
}

void AEnemyCharacter::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == ComboAttackMontage)
    {
        CorrectPawnOrient();

        // 广播攻击完成
        if (OnAttackFinished.IsBound())
        {
            OnAttackFinished.Broadcast();
        }

        // 注销委托
        if (GetMesh() && GetMesh()->GetAnimInstance())
        {
            GetMesh()->GetAnimInstance()->OnMontageEnded.RemoveDynamic(this, &AEnemyCharacter::OnMontageEnded);
        }
    }
}

void AEnemyCharacter::ApplyDamage()
{
    // 使用局部数组来过滤单词挥击造成的多次判定（替换蓝图的 Clear HitActors）
    TArray<AActor*> LocalHitActors;

    // 对应蓝图：Start = Loc + Fwd * 50, End = Loc + Fwd * 100
    FVector StartLoc = GetActorLocation() + GetActorForwardVector() * DamageTraceForwardStartOffset;
    FVector EndLoc = GetActorLocation() + GetActorForwardVector() * DamageTraceForwardEndOffset;

    TArray<FHitResult> OutHits;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    // 球体追踪
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        this, StartLoc, EndLoc, DamageTraceRadius, ObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    // 遍历击中目标
    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor && !LocalHitActors.Contains(HitActor))
        {
            LocalHitActors.Add(HitActor); // 记录已命中的对象

            // 对应蓝图：获取目标的 ASC 并应用伤害
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            if (TargetASC && DamageEffectClass)
            {
                FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
                EffectContext.AddSourceObject(this);

                FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
                if (SpecHandle.IsValid())
                {
                    TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                }
            }
        }
    }
}

void AEnemyCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    UpdateHealthBar();

    // 对应蓝图：血量变化时检查是否死亡
    if (Data.NewValue <= 0.0f && Data.OldValue > 0.0f)
    {
        // 死亡GA（GA_DieBase）会调用 HandleDeath()，这里不再重复调用
        // 只负责触发死亡GA
        if (AbilitySystemComponent && DeathAbilityTag.IsValid())
        {
            FGameplayTagContainer TagContainer(DeathAbilityTag);
            AbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer, true);
        }
        else
        {
            // 没有死亡GA时，直接执行死亡处理
            OnDead();
        }
    }
}

void AEnemyCharacter::OnDead()
{
    HandleDeath();
}

void AEnemyCharacter::HandleDeath_Implementation()
{
    Super::HandleDeath_Implementation();

    // 对应蓝图：获取 AI 控制器并停止逻辑
    AAIController* AIController = Cast<AAIController>(GetController());
    if (AIController && AIController->GetBrainComponent())
    {
        AIController->GetBrainComponent()->StopLogic(TEXT("Dead"));
    }

    // 设置延迟销毁定时器
    GetWorld()->GetTimerManager().SetTimer(
        DeathDestroyTimerHandle, this, &AEnemyCharacter::DestroyEnemy, DestroyDelayTime, false
    );
}

void AEnemyCharacter::DestroyEnemy()
{
    // 对应蓝图：延迟后的清理动作
    if (AbilitySystemComponent && CancelTagsOnDeath.IsValid())
    {
        AbilitySystemComponent->CancelAbilities(&CancelTagsOnDeath);
    }

    if (EnemyWeapon)
    {
        EnemyWeapon->Destroy();
    }

    Destroy();
}

