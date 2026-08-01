// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Characters/AICharacter/EnemyCharacter.h"
#include "Systems/AbilitySystem/AttributeSets/AS_Enemy.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Net/UnrealNetwork.h"

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    AttributeSet = CreateDefaultSubobject<UAS_Enemy>(TEXT("AttributeSet"));

    HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
    HealthBarComponent->SetupAttachment(RootComponent);
    HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
}

void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, EnemyWeapon);
}

void AEnemyCharacter::OnRep_EnemyWeapon()
{
    // 客户端收到服务器复制的武器引用后挂载到武器 Socket
    if (EnemyWeapon && GetMesh())
    {
        EnemyWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, WeaponSocketName);
    }
}

void AEnemyCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
        AttributeSet->GetHealthAttribute()).AddUObject(this, &AEnemyCharacter::OnHealthAttributeChanged);
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    BindAnimLayers();
    UpdateHealthBar();

    // 武器仅在服务器生成，通过复制 + OnRep 在客户端挂载，避免各端各自 Spawn 导致不同步
    if (HasAuthority() && WeaponClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = this;

        EnemyWeapon = GetWorld()->SpawnActor<AActor>(WeaponClass, GetActorTransform(), SpawnParams);
        if (EnemyWeapon)
        {
            EnemyWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, WeaponSocketName);
        }
    }

    if (DeathAbilityClass)
    {
        AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DeathAbilityClass, 1, INDEX_NONE, this));
    }
}

void AEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
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

    // 反射调用 UI 的 UpdateHealth 事件，避免硬编码 WBP 类型
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
        SceneComp->AddLocalRotation(FRotator(180.0f, 0.0f, 180.0f));
    }
}

void AEnemyCharacter::MeleeAttack()
{
    ApplyDamage();
    if (ComboAttackMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
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

        if (OnAttackFinished.IsBound())
        {
            OnAttackFinished.Broadcast();
        }

        if (GetMesh() && GetMesh()->GetAnimInstance())
        {
            GetMesh()->GetAnimInstance()->OnMontageEnded.RemoveDynamic(this, &AEnemyCharacter::OnMontageEnded);
        }
    }
}

void AEnemyCharacter::ApplyDamage()
{
    // 伤害结算必须服务器权威，否则客户端会非法施加 GameplayEffect
    if (!HasAuthority()) return;

    TArray<AActor*> LocalHitActors;

    FVector StartLoc = GetActorLocation() + GetActorForwardVector() * DamageTraceForwardStartOffset;
    FVector EndLoc = GetActorLocation() + GetActorForwardVector() * DamageTraceForwardEndOffset;

    TArray<FHitResult> OutHits;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    UKismetSystemLibrary::SphereTraceMultiForObjects(
        this, StartLoc, EndLoc, DamageTraceRadius, ObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor && !LocalHitActors.Contains(HitActor))
        {
            LocalHitActors.Add(HitActor);

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

    if (Data.NewValue <= 0.0f && Data.OldValue > 0.0f)
    {
        if (AbilitySystemComponent && DieEventTag.IsValid())
        {
            FGameplayEventData EventData;
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, DieEventTag, EventData);
        }
        else
        {
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

    if (HealthBarComponent)
    {
        HealthBarComponent->SetHiddenInGame(true);
    }

    AAIController* AIController = Cast<AAIController>(GetController());
    if (AIController && AIController->GetBrainComponent())
    {
        AIController->GetBrainComponent()->StopLogic(TEXT("Dead"));
    }

    GetWorld()->GetTimerManager().SetTimer(
        DeathDestroyTimerHandle, this, &AEnemyCharacter::DestroyEnemy, DestroyDelayTime, false
    );
}

void AEnemyCharacter::OnRep_IsDead(bool bOldIsDead)
{
    Super::OnRep_IsDead(bOldIsDead);

    // 模拟代理上死亡 GA 不会激活，这里通过复制状态兜底隐藏血条
    if (bIsDead && HealthBarComponent)
    {
        HealthBarComponent->SetHiddenInGame(true);
    }
}

void AEnemyCharacter::DestroyEnemy()
{
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
