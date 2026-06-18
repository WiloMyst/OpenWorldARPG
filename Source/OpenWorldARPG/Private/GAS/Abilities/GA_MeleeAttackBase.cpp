// Copyright 2025 WiloMyst. All Rights Reserved.

#include "GAS/Abilities/GA_MeleeAttackBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/PlayerCharacter.h"
#include "Components/WeaponManagerComponent.h"
#include "Data/CharacterCombatDataAsset.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MotionWarpingComponent.h"

UGA_MeleeAttackBase::UGA_MeleeAttackBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_MeleeAttackBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // 初始化状态：使用连招图入口节点
    // 通过 GA 蓝图中配置的 TalentTag 从 CombatData 字典中查询连招数据
    const FTalentConfig* TalentConfig = CachedPlayer->FindTalentConfig(TalentTag);
    CurrentNodeName = TalentConfig ? TalentConfig->EntryNodeName : NAME_None;
    CurrentNode = nullptr;
    bComboWindowOpen = false;
    BufferedInput = FGameplayTag();
    ActiveAttackMontage = nullptr;

    ExecuteAttack(CurrentNodeName);
}

void UGA_MeleeAttackBase::ExecuteAttack(FName NodeName)
{
    if (!CachedPlayer)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 1. 武器切换到手
    if (UWeaponManagerComponent* WeaponComp = CachedPlayer->FindComponentByClass<UWeaponManagerComponent>())
    {
        WeaponComp->WeaponToHand();
    }

    // 2. 查找连招节点（通过 TalentTag 从 CombatData 字典中查询连招图）
    const FTalentConfig* TalentConfig = CachedPlayer->FindTalentConfig(TalentTag);
    if (!TalentConfig)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    const TMap<FName, FComboActionNode>& ComboGraph = TalentConfig->ComboGraph;

    // 如果 NodeName 无效，回退到入口节点
    if (NodeName.IsNone() || !ComboGraph.Contains(NodeName))
    {
        NodeName = TalentConfig->EntryNodeName;
    }

    CurrentNode = ComboGraph.Find(NodeName);
    if (!CurrentNode)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    CurrentNodeName = NodeName;

    // 3. 加载蒙太奇
    UAnimMontage* MontageToPlay = CurrentNode->Montage.LoadSynchronous();
    if (!MontageToPlay)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 4. 重置本轮攻击状态
    ActiveAttackMontage = MontageToPlay;
    HitActors.Empty();
    bComboWindowOpen = false;
    BufferedInput = FGameplayTag();
    ClearAllTasks();

    // 5. 索敌检测 + Motion Warping 目标设置（使用节点配置的 MaxWarpDistance）
    AttackOrientation(CurrentNode->MaxWarpDistance);

    // ==========================================
    // 6. 创建所有 Task 并绑定回调（先不 Activate）
    // ==========================================

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f);
    MontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->OnBlendOut.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttackBase::OnMontageFinished);

    if (DamageDealEventTag.IsValid())
    {
        DamageTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DamageDealEventTag, nullptr, false, false);
        DamageTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnDamageEventReceived);
    }

    if (ComboWindowOpenTag.IsValid())
    {
        ComboOpenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowOpenTag, nullptr, false, false);
        ComboOpenTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnComboOpenEventReceived);
    }

    if (ComboWindowCloseTag.IsValid())
    {
        ComboCloseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowCloseTag, nullptr, false, false);
        ComboCloseTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnComboCloseEventReceived);
    }

    if (AttackInputTag.IsValid())
    {
        InputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, AttackInputTag, nullptr, false, false);
        InputTask->EventReceived.AddDynamic(this, &UGA_MeleeAttackBase::OnAttackInputEventReceived);
    }

    // ==========================================
    // 7. 严格按顺序 Activate
    //    先激活 MontageTask（播放动画，强制打断旧动画），
    //    旧动画的 AnimNotifyState 触发 NotifyEnd 发出的"旧 Close 事件"
    //    会散逸到空气中，因为此时新 Task 还没开始监听。
    // ==========================================

    MontageTask->ReadyForActivation();

    if (DamageTask) DamageTask->ReadyForActivation();
    if (ComboOpenTask) ComboOpenTask->ReadyForActivation();
    if (ComboCloseTask) ComboCloseTask->ReadyForActivation();
    if (InputTask) InputTask->ReadyForActivation();
}

void UGA_MeleeAttackBase::AttackOrientation(float MaxWarpDistance)
{
    if (!CachedPlayer) return;

    FVector ActorLoc = CachedPlayer->GetActorLocation();

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);
    TArray<FHitResult> OutHits;

    // 1. 球体多重追踪寻找最近的敌人
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        ActorLoc, ActorLoc,
        OrientRadius,
        OrientObjectTypes,
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        OutHits,
        true
    );

    AActor* BestTarget = nullptr;
    float MinDistance = OrientRadius;

    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor && HitActor->ActorHasTag(EnemyActorTag))
        {
            float Distance = FVector::Dist(ActorLoc, HitActor->GetActorLocation());
            if (Distance < MinDistance)
            {
                MinDistance = Distance;
                BestTarget = HitActor;
            }
        }
    }

    // 2. 设置 Motion Warping Target（废弃 SetActorRotation）
    //
    // 架构原则：不再使用 SetActorRotation 强制转向，
    // 完全由 Motion Warping 在动画帧上处理位移和朝向吸附。
    // 动画师在蒙太奇中配置 Motion Warping AnimNotifyState，
    // 引擎会在指定帧将角色根骨骼平滑吸附到 Warp Target。
    //
    // 计算逻辑：
    // - 方向：从角色指向目标的方向
    // - 距离：取"角色到目标的实际距离"和"节点配置的 MaxWarpDistance"的较小值
    //   角色会向目标滑步，但不会滑过目标
    UMotionWarpingComponent* MotionWarpingComp = CachedPlayer->GetMotionWarpingComp();

    if (BestTarget && MotionWarpingComp)
    {
        FVector Direction = (BestTarget->GetActorLocation() - ActorLoc).GetSafeNormal2D();
        float ActualDistance = FMath::Min(FVector::Dist2D(ActorLoc, BestTarget->GetActorLocation()), MaxWarpDistance);
        FVector WarpLocation = ActorLoc + Direction * ActualDistance;

        // 使用 AddOrUpdateWarpTargetFromLocation：
        // 仅设置位置吸附，朝向由蒙太奇中的 Root Motion + Motion Warping AnimNotifyState 的
        // bFacing 处理。动画师可以精确控制角色在吸附过程中的朝向过渡。
        MotionWarpingComp->AddOrUpdateWarpTargetFromLocation(WarpTargetName, WarpLocation);
    }
    else if (MotionWarpingComp)
    {
        // 没有目标时，清除之前的 Warp Target，避免残留
        MotionWarpingComp->RemoveWarpTarget(WarpTargetName);
    }
}

void UGA_MeleeAttackBase::OnDamageEventReceived(FGameplayEventData Payload)
{
    // 防串线校验：确保伤害事件属于当前正在播放的蒙太奇
    // 如果动画已经切换到下一段，但旧 Task 还残留了一帧事件，
    // OptionalObject 会指向旧蒙太奇，此时应忽略
    if (Payload.OptionalObject && Payload.OptionalObject != ActiveAttackMontage)
    {
        return;
    }

    ApplyDamageToTargets();
}

void UGA_MeleeAttackBase::ApplyDamageToTargets()
{
    if (!CachedPlayer) return;

    // LocalPredicted GA：伤害应用必须只在权威端（服务器）执行
    if (!GetAvatarActorFromActorInfo()->HasAuthority()) return;

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!SourceASC) return;

    // 优先使用节点级 DamageEffect，回退到 GA 级 DamageEffectClass
    TSubclassOf<UGameplayEffect> EffectToApply = CurrentNode ? CurrentNode->DamageEffect : nullptr;
    if (!EffectToApply)
    {
        EffectToApply = DamageEffectClass;
    }
    if (!EffectToApply) return;

    FVector ActorLoc = CachedPlayer->GetActorLocation();
    FVector ForwardDir = CachedPlayer->GetActorForwardVector();
    FVector StartLoc = ActorLoc + ForwardDir * TraceForwardOffset1;
    FVector EndLoc = ActorLoc + ForwardDir * TraceForwardOffset2;

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(CachedPlayer);

    TArray<FHitResult> OutHits;

    UKismetSystemLibrary::SphereTraceMultiForObjects(
        CachedPlayer,
        StartLoc, EndLoc, TraceRadius,
        TraceObjectTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, OutHits, true
    );

    for (const FHitResult& Hit : OutHits)
    {
        AActor* HitActor = Hit.GetActor();

        if (HitActor && !HitActors.Contains(HitActor))
        {
            HitActors.Add(HitActor);

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            if (TargetASC)
            {
                FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
                Context.AddHitResult(Hit);
                Context.AddSourceObject(this);

                FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectToApply, GetAbilityLevel(), Context);
                if (SpecHandle.IsValid())
                {
                    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
                }
            }
        }
    }
}

void UGA_MeleeAttackBase::OnComboOpenEventReceived(FGameplayEventData Payload)
{
    // 防串线校验
    if (Payload.OptionalObject && Payload.OptionalObject != ActiveAttackMontage)
    {
        return;
    }

    // 连招窗口打开
    bComboWindowOpen = true;

    // ==========================================
    // 核心连招流转：窗口打开时立即消费输入缓存
    //
    // 为什么在 Open 时消费？
    // - 响应速度最快：玩家输入在窗口打开的瞬间就被消费
    // - 预输入支持：玩家可以在窗口打开前提前按键，窗口一开就流转
    // - AnimNotifyState 的 NotifyEnd 保证窗口一定会被关闭，
    //   即使动画被打断，ComboWindowClose 也必定触发，不会锁死
    //
    // 时序安全：
    // - ComboWindowOpen 由 AnimNotifyState 的 NotifyBegin 触发
    // - 此时当前动画仍在播放，不会被打断
    // - TryConsumeBufferedInput 如果命中派生表，会调用 ExecuteAttack
    // - ExecuteAttack 内部先 ClearAllTasks（杀死旧的监听器），
    //   再创建新的 Task 并先激活 MontageTask（打断旧动画），
    //   旧动画的 AnimNotifyState 触发 NotifyEnd 发出 Close 事件，
    //   但此时旧 Task 已被杀死，不会干扰新状态
    // ==========================================
    TryConsumeBufferedInput();
}

void UGA_MeleeAttackBase::OnComboCloseEventReceived(FGameplayEventData Payload)
{
    // 防串线校验
    if (Payload.OptionalObject && Payload.OptionalObject != ActiveAttackMontage)
    {
        return;
    }

    // 连招窗口关闭
    bComboWindowOpen = false;

    // 清空过期输入缓存。
    // 窗口关闭后，之前缓存的输入不再有效，防止过期输入导致角色"自动"攻击。
    // 如果玩家在窗口关闭后再次按键，OnAttackInputEventReceived 会重新记录。
    BufferedInput = FGameplayTag();
}

void UGA_MeleeAttackBase::OnAttackInputEventReceived(FGameplayEventData Payload)
{
    // 防串线校验
    if (Payload.OptionalObject && Payload.OptionalObject != ActiveAttackMontage)
    {
        return;
    }

    // ==========================================
    // 输入缓存机制
    //
    // 无论窗口是否打开，都记录输入到缓存：
    // - 窗口已打开：TryConsumeBufferedInput 会在下一次 Open 事件时消费
    //   （实际上 Open 已经触发过了，这里记录后需要手动检查）
    // - 窗口未打开：预输入，等待窗口打开时自动消费
    //
    // 如果窗口已经打开，收到输入后立即尝试消费
    // ==========================================
    if (Payload.EventTag.IsValid())
    {
        BufferedInput = Payload.EventTag;

        // 如果窗口已经打开，立即尝试消费
        if (bComboWindowOpen)
        {
            TryConsumeBufferedInput();
        }
    }
}

void UGA_MeleeAttackBase::TryConsumeBufferedInput()
{
    if (!BufferedInput.IsValid() || !CurrentNode)
    {
        return;
    }

    // 在当前节点的派生表中查找缓存输入对应的下一个节点名称
    if (const FName* NextNodeName = CurrentNode->NextNodes.Find(BufferedInput))
    {
        FName ResolvedNextName = *NextNodeName;
        BufferedInput = FGameplayTag(); // 清空缓存
        bComboWindowOpen = false;       // 重置窗口状态
        ExecuteAttack(ResolvedNextName);
    }
}

void UGA_MeleeAttackBase::OnMontageFinished()
{
    CorrectPawnOrient();

    // 动画自然结束，重置状态
    bComboWindowOpen = false;
    BufferedInput = FGameplayTag();
    CurrentNodeName = NAME_None;
    CurrentNode = nullptr;
    ActiveAttackMontage = nullptr;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttackBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
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

void UGA_MeleeAttackBase::ClearAllTasks()
{
    if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
    if (DamageTask) { DamageTask->EndTask(); DamageTask = nullptr; }
    if (ComboOpenTask) { ComboOpenTask->EndTask(); ComboOpenTask = nullptr; }
    if (ComboCloseTask) { ComboCloseTask->EndTask(); ComboCloseTask = nullptr; }
    if (InputTask) { InputTask->EndTask(); InputTask = nullptr; }
}

void UGA_MeleeAttackBase::CorrectPawnOrient()
{
    if (!CachedPlayer) return;
    FRotator CurrentRot = CachedPlayer->GetActorRotation();
    CachedPlayer->SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
}
