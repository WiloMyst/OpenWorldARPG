// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/CombatSystem/Abilities/GA_PlungeAttackBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitMovementModeChange.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"
#include "Characters/AICharacter/EnemyCharacter.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "Systems/CombatSystem/Components/WeaponManagerComponent.h"
#include "Systems/CombatSystem/Data/CharacterCombatDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UGA_PlungeAttackBase::UGA_PlungeAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_PlungeAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedPlayer = Cast<APlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!CachedPlayer)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ExecuteAttack();
}

void UGA_PlungeAttackBase::ExecuteAttack()
{
    if (!CachedPlayer) return;

    bInterruptedByLand = false;

    // 1. Weapon to Hand
    if (UWeaponManagerComponent* WeaponComp = CachedPlayer->FindComponentByClass<UWeaponManagerComponent>())
    {
        WeaponComp->WeaponToHand();
    }

    // 2. 滞空时清理速度 (可选：或者赋予一个向下的冲刺力)
    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (MoveComp)
    {
        MoveComp->Velocity = FVector::ZeroVector;

        // --- 超低空下落防卡死保护 ---
        // 边界情况：玩家在离地极低的位置触发下落攻击（如台阶边缘），
        // 此时角色可能已经处于 MOVE_Walking 状态。
        // 如果仍走正常流程（播放 FallMontage + 启动 WaitMovementModeChange Task），
        // 由于已经落地，WaitMovementModeChange 永远不会触发 → 技能死锁。
        //
        // 解决方案：检测到已在地面时，跳过空中阶段，直接手动调用
        // OnMovementModeChanged(MOVE_Walking) 无缝衔接进入砸地阶段（Plunge_Land）。
        if (MoveComp->IsMovingOnGround() || MoveComp->MovementMode == MOVE_Walking)
        {
            OnMovementModeChanged(MOVE_Walking);
            return;
        }
    }

    // 3. 从 CombatData 获取下落攻击天赋配置（通过 GA 蓝图中配置的 TalentTag 查询连招图）
    const FTalentConfig* PlungeTalent = CachedPlayer->GetTalentConfig(TalentTag);
    if (!PlungeTalent || PlungeTalent->ComboGraph.IsEmpty())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 从 ComboGraph 中查找入口节点（空中下落循环动画）
    const FComboActionNode* EntryNode = PlungeTalent->ComboGraph.Find(PlungeTalent->EntryNodeName);
    if (!EntryNode || EntryNode->Montage.IsNull())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 直接从连招节点的软引用加载蒙太奇
    UAnimMontage* FallMontage = EntryNode->Montage.LoadSynchronous();
    if (!FallMontage)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    ClearAllTasks();

    // --- 阶段一：多线程并发监听区 ---

    // 任务1：播放空中下落蒙太奇 (通常这是一个Loop动画，不会自然结束)
    FallMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FallMontage, 1.0f, NAME_None, true, 1.0f, 0.0f);
    // 如果下落被打断或取消，结束技能
    FallMontageTask->OnInterrupted.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    FallMontageTask->OnCancelled.AddDynamic(this, &UGA_PlungeAttackBase::OnMontageFinished);
    FallMontageTask->ReadyForActivation();

    // 任务2：监听移动模式变为 "行走(Walking)" 即落地判定
    MovementModeTask = UAbilityTask_WaitMovementModeChange::CreateWaitMovementModeChange(this, MOVE_Walking);
    MovementModeTask->OnChange.AddDynamic(this, &UGA_PlungeAttackBase::OnMovementModeChanged);
    MovementModeTask->ReadyForActivation();

    // 任务3：全局监听伤害判定事件 (等待 [1] 号蒙太奇里的 AnimNotify 触发)
    if (DamageDealEventTag.IsValid())
    {
        DamageTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DamageDealEventTag, nullptr, false, false);
        DamageTask->EventReceived.AddDynamic(this, &UGA_PlungeAttackBase::OnDamageEventReceived);
        DamageTask->ReadyForActivation();
    }
}

void UGA_PlungeAttackBase::OnMovementModeChanged(EMovementMode NewMovementMode)
{
    // --- 阶段二：落地砸地 ---

    // 1. 停止监听落地事件，停止当前的空中下落蒙太奇
    if (MovementModeTask)
    {
        MovementModeTask->EndTask();
        MovementModeTask = nullptr;
    }
    if (FallMontageTask)
    {
        FallMontageTask->EndTask();
        FallMontageTask = nullptr;
    }

    // 强行停止当前的动画，为落地动画腾出轨道
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        ASC->CurrentMontageStop();
    }

    // 2. 从 ComboGraph 查找落地砸地蒙太奇（事件驱动：通过 LandedTransitionTag 精准匹配）
    //
    // 【设计理念：将 GameplayTag 作为状态机事件触发器】
    // ComboGraph 的 NextNodes 是一个 TMap<FGameplayTag, FName> 派生表：
    //   - Key   = 触发事件/输入的 GameplayTag（如 Event.Movement.Landed）
    //   - Value = 目标节点名称（如 Plunge_Land）
    //
    // 当角色落地时，使用 LandedTransitionTag 去 NextNodes 中 Find()，
    // 即可精准取出落地节点名称，再从 ComboGraph 中加载该节点的蒙太奇。
    //
    // 相比原 for 循环遍历的优势：
    //   1. 一个入口节点可配置多条派生路径（落地/被打断/超时），各自用不同 Tag 区分
    //   2. 策划可自由扩展新事件 Tag，无需修改 C++ 代码
    //   3. Tag 本身即语义文档，配置表可读性强
    const FTalentConfig* PlungeTalent = CachedPlayer->GetTalentConfig(TalentTag);
    UAnimMontage* LandingMontage = nullptr;

    if (PlungeTalent && LandedTransitionTag.IsValid())
    {
        // 重新查找入口节点
        const FComboActionNode* LocalEntryNode = PlungeTalent->ComboGraph.Find(PlungeTalent->EntryNodeName);
        if (LocalEntryNode)
        {
            // 事件驱动核心：用 LandedTransitionTag 精准 Find，而非遍历整个 NextNodes
            if (const FName* LandingNodeName = LocalEntryNode->NextNodes.Find(LandedTransitionTag))
            {
                if (const FComboActionNode* LandNode = PlungeTalent->ComboGraph.Find(*LandingNodeName))
                {
                    // 直接从连招节点的软引用加载蒙太奇
                    if (!LandNode->Montage.IsNull())
                    {
                        LandingMontage = LandNode->Montage.LoadSynchronous();
                    }
                }
            }
        }
    }

    if (LandingMontage)
    {
        LandingMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, LandingMontage, 1.0f, NAME_None, true, 1.0f, 0.0f);

        // 绑定落地动画结束的回调（动画播完后技能才算真正结束）
        LandingMontageTask->OnCompleted.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);
        LandingMontageTask->OnBlendOut.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);
        LandingMontageTask->OnInterrupted.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);
        LandingMontageTask->OnCancelled.AddDynamic(this, &UGA_PlungeAttackBase::OnLandingMontageFinished);

        LandingMontageTask->ReadyForActivation();
        return; // 成功播放落地动画，跳出函数等待动画结束
    }

    // 兜底：如果 ComboGraph 中没有配置落地节点，落地后直接结束技能
    CorrectPawnOrient();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnMontageFinished()
{
    // 这个用于处理“空中阶段”意外结束（比如被击飞打断）
    CorrectPawnOrient();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnLandingMontageFinished()
{
    // 落地砸地动画播放完毕，完美收尾
    CorrectPawnOrient();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PlungeAttackBase::OnDamageEventReceived(FGameplayEventData Payload)
{
    // 收到 [1] 号蒙太奇里 AnimNotify 发出的 Tag 广播，执行伤害结算
    ApplyDamageToTargets();
}

void UGA_PlungeAttackBase::ApplyDamageToTargets()
{
    if (!CachedPlayer) return;

    FVector ActorLoc = CachedPlayer->GetActorLocation();
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);
    TArray<FHitResult> OutHits;

    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        ActorLoc, ActorLoc,
        PlungeDamageRadius,
        TraceObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    UGameServerSubsystem* GS = CachedPlayer->GetWorld()
        ? CachedPlayer->GetWorld()->GetGameInstance()->GetSubsystem<UGameServerSubsystem>()
        : nullptr;

    TArray<AActor*> HitActors;

    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || HitActors.Contains(HitActor)) continue;
        HitActors.Add(HitActor);

        // 服务器权威伤害：客户端只上报命中, 伤害由服务器裁决并广播 (废弃本地伤害 GE)
        const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(HitActor);
        if (!Enemy || Enemy->GetServerEnemyId() == 0) continue;

        if (GS)
        {
            GS->SendDamageIntent(ServerSkillId, Enemy->GetServerEnemyId());
        }
    }
}

void UGA_PlungeAttackBase::ClearAllTasks()
{
    if (FallMontageTask) { FallMontageTask->EndTask(); FallMontageTask = nullptr; }
    if (LandingMontageTask) { LandingMontageTask->EndTask(); LandingMontageTask = nullptr; }
    if (MovementModeTask) { MovementModeTask->EndTask(); MovementModeTask = nullptr; }
    if (DamageTask) { DamageTask->EndTask(); DamageTask = nullptr; }
}

void UGA_PlungeAttackBase::CorrectPawnOrient()
{
    if (!CachedPlayer) return;
    FRotator CurrentRot = CachedPlayer->GetActorRotation();
    FRotator CorrectedRot = FRotator(0.0f, CurrentRot.Yaw, 0.0f);
    CachedPlayer->SetActorRotation(CorrectedRot);
}

void UGA_PlungeAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 被其他 GA Cancel 时，武器回到背上
    if (bWasCancelled && CachedPlayer)
    {
        if (UWeaponManagerComponent* WeaponComp = CachedPlayer->FindComponentByClass<UWeaponManagerComponent>())
        {
            WeaponComp->WeaponToBack();
        }
    }

    ClearAllTasks();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}