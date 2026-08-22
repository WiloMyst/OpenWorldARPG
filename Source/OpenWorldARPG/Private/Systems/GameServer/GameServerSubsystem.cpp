// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/GameServer/GameServerSubsystem.h"
#include "Systems/InventoryManager/InventoryManagerSubsystem.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "Systems/InventoryManager/Types/WeaponTypes.h"
#include "Systems/InventoryManager/Types/ArtifactTypes.h"

#include "SGame/GameService.h"
#include "SGame/GameClient.h"
#include "TurboLinkGrpcManager.h"
#include "TurboLinkGrpcUtilities.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

namespace
{
    EArtifactSlot SlotFromString(const FString& Slot)
    {
        if (Slot == TEXT("flower"))  return EArtifactSlot::Flower;
        if (Slot == TEXT("plume"))   return EArtifactSlot::Plume;
        if (Slot == TEXT("sands"))   return EArtifactSlot::Sands;
        if (Slot == TEXT("goblet"))  return EArtifactSlot::Goblet;
        if (Slot == TEXT("circlet")) return EArtifactSlot::Circlet;
        return EArtifactSlot::None;
    }

    EArtifactStatType StatFromString(const FString& Stat)
    {
        if (Stat == TEXT("hp_flat"))     return EArtifactStatType::HP_Flat;
        if (Stat == TEXT("hp_percent"))  return EArtifactStatType::HP_Percent;
        if (Stat == TEXT("atk_flat"))    return EArtifactStatType::ATK_Flat;
        if (Stat == TEXT("atk_percent")) return EArtifactStatType::ATK_Percent;
        if (Stat == TEXT("def_flat"))    return EArtifactStatType::DEF_Flat;
        if (Stat == TEXT("def_percent")) return EArtifactStatType::DEF_Percent;
        if (Stat == TEXT("crit_rate"))   return EArtifactStatType::CritRate;
        if (Stat == TEXT("crit_dmg"))    return EArtifactStatType::CritDamage;
        if (Stat == TEXT("em"))          return EArtifactStatType::ElementalMastery;
        if (Stat == TEXT("er"))          return EArtifactStatType::EnergyRecharge;
        if (Stat == TEXT("phys_dmg"))    return EArtifactStatType::PhysDmgBonus;
        return EArtifactStatType::None;
    }
}

void UGameServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UGameServerSubsystem::Deinitialize()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
    }
    ResetChannel();
    Super::Deinitialize();
}

// ====================================================================
// 连接管理
// ====================================================================

bool UGameServerSubsystem::EnsureChannel()
{
    UTurboLinkGrpcManager* GrpcManager = UTurboLinkGrpcUtilities::GetTurboLinkGrpcManager(this);
    if (!GrpcManager)
    {
        FailLogin(TEXT("gRPC 管理器不可用"));
        return false;
    }

    GameService = Cast<UGameService>(GrpcManager->MakeService(TEXT("GameService")));
    if (!GameService)
    {
        FailLogin(TEXT("GameService 创建失败"));
        return false;
    }
    GameService->Connect();

    GameClient = GameService->MakeClient();
    if (!GameClient)
    {
        FailLogin(TEXT("GameService 客户端创建失败"));
        ResetChannel();
        return false;
    }

    GameClient->OnGameChannelResponse.AddDynamic(this, &UGameServerSubsystem::HandleServerMessage);
    ChannelHandle = GameClient->InitGameChannel();

    UE_LOG(LogTemp, Log, TEXT("[GameServer] GameChannel 已建立，等待登录确认。"));
    return true;
}

void UGameServerSubsystem::ResetChannel()
{
    if (GameClient)
    {
        GameClient->OnGameChannelResponse.RemoveAll(this);
        if (ChannelHandle.Value != 0)
        {
            GameClient->TryCancel(ChannelHandle);
        }
        GameClient->Shutdown();
        GameClient = nullptr;
    }
    if (GameService)
    {
        // 生成代码将 Shutdown 设为 private，统一走管理器引用计数释放
        if (UTurboLinkGrpcManager* GrpcManager = UTurboLinkGrpcUtilities::GetTurboLinkGrpcManager(this))
        {
            GrpcManager->ReleaseService(GameService);
        }
        GameService = nullptr;
    }
    ChannelHandle = FGrpcContextHandle();
}

// ====================================================================
// 登录 / 登出
// ====================================================================

void UGameServerSubsystem::RequestLogin(const FString& Account, const FString& Token)
{
    if (bLoginPending)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 登录请求进行中，忽略重复点击。"));
        return;
    }

    // 清理上一次会话的残留连接（断线重登 / 已登录再登录场景）
    if (GameClient || GameService)
    {
        ResetChannel();
    }

    AccountName = Account;

    if (!EnsureChannel())
    {
        return;
    }

    bLoginPending = true;
    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().SetTimer(LoginTimeoutHandle, this, &UGameServerSubsystem::OnLoginTimeout, LoginTimeoutSec, false);
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::Login;
    Message.Payload.Login = MakeShared<FGrpcGameLoginRequest>();
    Message.Payload.Login->Account = Account;
    Message.Payload.Login->Token = Token;
    SendMessage(MoveTemp(Message));

    UE_LOG(LogTemp, Log, TEXT("[GameServer] 登录请求已发送 [account=%s]。"), *Account);
}

void UGameServerSubsystem::RequestLogout()
{
    if (!bLoggedIn && !bLoginPending)
    {
        return;
    }

    if (bLoggedIn && GameClient && ChannelHandle.Value != 0)
    {
        FGrpcGameClientMessage Message;
        Message.Sequence = NextSequence++;
        Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::Logout;
        Message.Payload.Logout = MakeShared<FGrpcGameLogoutRequest>();
        // 不等待响应：服务器收到 logout 会主动关闭流，随后本端收到流结束事件
        SendMessage(MoveTemp(Message));
    }

    bLoggedIn = false;
    bLoginPending = false;
    AccountName.Reset();
    SessionToken.Reset();

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
    }
    ResetChannel();

    UE_LOG(LogTemp, Log, TEXT("[GameServer] 已登出。"));
}

void UGameServerSubsystem::FailLogin(const FString& Reason)
{
    bLoginPending = false;
    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
    }
    UE_LOG(LogTemp, Warning, TEXT("[GameServer] 登录失败: %s"), *Reason);
    OnLoginResult.Broadcast(false, Reason);
}

void UGameServerSubsystem::OnLoginTimeout()
{
    if (bLoginPending)
    {
        ResetChannel();
        FailLogin(TEXT("登录超时，请确认游戏服务器已启动"));
    }
}

void UGameServerSubsystem::SendHeartbeat()
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::Heartbeat;
    Message.Payload.Heartbeat = MakeShared<FGrpcGameHeartbeat>();
    Message.Payload.Heartbeat->ClientTimestamp = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
    SendMessage(MoveTemp(Message));
}

// ====================================================================
// 消息接收与分发
// ====================================================================

void UGameServerSubsystem::HandleServerMessage(FGrpcContextHandle Handle,
                                               const FGrpcResult& GrpcResult,
                                               const FGrpcGameServerMessage& Response)
{
    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        HandleConnectionFailure(GrpcResult);
        return;
    }

    switch (Response.Payload.PayloadCase)
    {
        case EGrpcGameServerMessagePayload::Login:
            if (Response.Payload.Login.IsValid())
            {
                HandleLoginResponse(*Response.Payload.Login);
            }
            break;

        case EGrpcGameServerMessagePayload::Heartbeat:
            // 心跳保活，无需处理
            break;

        case EGrpcGameServerMessagePayload::SaveData:
            break;

        case EGrpcGameServerMessagePayload::Kick:
            if (Response.Payload.Kick.IsValid())
            {
                HandleKick(Response.Payload.Kick->Reason);
            }
            break;

        case EGrpcGameServerMessagePayload::Error:
            if (Response.Payload.Error.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[GameServer] 服务器错误 [code=%d, msg=%s]"),
                       Response.Payload.Error->Code, *Response.Payload.Error->Message);
            }
            break;

        case EGrpcGameServerMessagePayload::InventoryOp:
            if (Response.Payload.InventoryOp.IsValid())
            {
                HandleInventoryOpResponse(*Response.Payload.InventoryOp);
            }
            break;

        case EGrpcGameServerMessagePayload::DialogueAuthResult:
            if (Response.Payload.DialogueAuthResult.IsValid())
            {
                HandleDialogueAuthResult(*Response.Payload.DialogueAuthResult);
            }
            break;

        default:
            break;
    }
}

void UGameServerSubsystem::HandleLoginResponse(const FGrpcGameLoginResponse& Login)
{
    if (!Login.Success)
    {
        FailLogin(Login.ErrorMsg.IsEmpty() ? TEXT("登录被拒绝") : Login.ErrorMsg);
        return;
    }

    bLoginPending = false;
    bLoggedIn = true;
    SessionToken = Login.SessionToken;

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
        GI->GetTimerManager().SetTimer(HeartbeatTimerHandle, this, &UGameServerSubsystem::SendHeartbeat, HeartbeatIntervalSec, true);
    }

    UE_LOG(LogTemp, Log, TEXT("[GameServer] 登录成功 [account=%s, playerId=%llu, items=%d]"),
           *AccountName, (uint64)Login.PlayerId, Login.Inventory.Num());

    PushInventorySnapshot(Login.Inventory);
    OnLoginResult.Broadcast(true, TEXT(""));
}

void UGameServerSubsystem::HandleInventoryOpResponse(const FGrpcGameInventoryOpResponse& Op)
{
    if (!Op.Success)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 背包操作被服务器拒绝: %s"), *Op.ErrorMsg);
        OnInventoryOpResult.Broadcast(false, Op.ErrorMsg);
        return;
    }

    PushInventorySnapshot(Op.Items);
    OnInventoryOpResult.Broadcast(true, TEXT(""));
}

void UGameServerSubsystem::HandleDialogueAuthResult(const FGrpcGameDialogueAuthResult& Result)
{
    if (Result.Ok)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameServer] 对话票据已签发 [expires_at=%lld ms]"), Result.ExpiresAt);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 对话票据申请被拒: %s"), *Result.Reason);
    }
    OnDialogueAuthResult.Broadcast(Result.Ok, Result.DialogueToken, Result.ExpiresAt, Result.Reason);
}

void UGameServerSubsystem::HandleKick(const FString& Reason)
{
    UE_LOG(LogTemp, Warning, TEXT("[GameServer] 被服务器踢出: %s"), *Reason);

    const bool bWasLoggedIn = bLoggedIn;
    ResetChannel();

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
    }

    bLoggedIn = false;
    bLoginPending = false;
    AccountName.Reset();
    SessionToken.Reset();

    if (bWasLoggedIn)
    {
        OnKicked.Broadcast(Reason);
    }
}

void UGameServerSubsystem::HandleConnectionFailure(const FGrpcResult& GrpcResult)
{
    UE_LOG(LogTemp, Warning, TEXT("[GameServer] 连接异常 [code=%d, msg=%s]"),
           static_cast<int32>(GrpcResult.Code), *GrpcResult.GetMessageString());

    const bool bWasLoggedIn = bLoggedIn;
    ResetChannel();

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
    }

    if (bLoginPending)
    {
        FailLogin(TEXT("无法连接游戏服务器，请确认服务器已启动"));
        return;
    }

    bLoggedIn = false;
    if (bWasLoggedIn)
    {
        OnServerDisconnected.Broadcast();
    }
}

// ====================================================================
// 背包操作转发
// ====================================================================

bool UGameServerSubsystem::ServerAddItem(int32 ItemID, int32 Amount)
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return false;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::InventoryOp;
    Message.Payload.InventoryOp = MakeShared<FGrpcGameInventoryOpRequest>();
    Message.Payload.InventoryOp->Op.OpCase = EGrpcGameInventoryOpRequestOp::Add;
    Message.Payload.InventoryOp->Op.Add = MakeShared<FGrpcGameInventoryAdd>();
    Message.Payload.InventoryOp->Op.Add->ItemId = ItemID;
    Message.Payload.InventoryOp->Op.Add->Amount = Amount;
    SendMessage(MoveTemp(Message));
    return true;
}

bool UGameServerSubsystem::ServerRemoveItem(const FGuid& ItemGUID, int32 Amount)
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return false;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::InventoryOp;
    Message.Payload.InventoryOp = MakeShared<FGrpcGameInventoryOpRequest>();
    Message.Payload.InventoryOp->Op.OpCase = EGrpcGameInventoryOpRequestOp::Remove;
    Message.Payload.InventoryOp->Op.Remove = MakeShared<FGrpcGameInventoryRemove>();
    Message.Payload.InventoryOp->Op.Remove->ItemGuid = ItemGUID.ToString(EGuidFormats::DigitsWithHyphensLower);
    Message.Payload.InventoryOp->Op.Remove->Amount = Amount;
    SendMessage(MoveTemp(Message));
    return true;
}

bool UGameServerSubsystem::ServerEquipItem(const FGuid& ItemGUID, int32 CharacterID)
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return false;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::InventoryOp;
    Message.Payload.InventoryOp = MakeShared<FGrpcGameInventoryOpRequest>();
    Message.Payload.InventoryOp->Op.OpCase = EGrpcGameInventoryOpRequestOp::Equip;
    Message.Payload.InventoryOp->Op.Equip = MakeShared<FGrpcGameInventoryEquip>();
    Message.Payload.InventoryOp->Op.Equip->ItemGuid = ItemGUID.ToString(EGuidFormats::DigitsWithHyphensLower);
    Message.Payload.InventoryOp->Op.Equip->CharacterId = CharacterID;
    SendMessage(MoveTemp(Message));
    return true;
}

bool UGameServerSubsystem::ServerUnequipItem(const FGuid& ItemGUID)
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return false;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::InventoryOp;
    Message.Payload.InventoryOp = MakeShared<FGrpcGameInventoryOpRequest>();
    Message.Payload.InventoryOp->Op.OpCase = EGrpcGameInventoryOpRequestOp::Unequip;
    Message.Payload.InventoryOp->Op.Unequip = MakeShared<FGrpcGameInventoryUnequip>();
    Message.Payload.InventoryOp->Op.Unequip->ItemGuid = ItemGUID.ToString(EGuidFormats::DigitsWithHyphensLower);
    SendMessage(MoveTemp(Message));
    return true;
}

bool UGameServerSubsystem::ServerUseItem(const FGuid& ItemGUID, int32 TargetCharacterID, int32 Amount)
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return false;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::InventoryOp;
    Message.Payload.InventoryOp = MakeShared<FGrpcGameInventoryOpRequest>();
    Message.Payload.InventoryOp->Op.OpCase = EGrpcGameInventoryOpRequestOp::Use;
    Message.Payload.InventoryOp->Op.Use = MakeShared<FGrpcGameInventoryUse>();
    Message.Payload.InventoryOp->Op.Use->ItemGuid = ItemGUID.ToString(EGuidFormats::DigitsWithHyphensLower);
    Message.Payload.InventoryOp->Op.Use->TargetCharacterId = TargetCharacterID;
    Message.Payload.InventoryOp->Op.Use->Amount = Amount;
    SendMessage(MoveTemp(Message));
    return true;
}

// ====================================================================
// 对话授权（信令面）
// ====================================================================

bool UGameServerSubsystem::RequestDialogueAuth(int32 NpcId)
{
    if (!bLoggedIn || !GameClient || ChannelHandle.Value == 0)
    {
        return false;
    }

    FGrpcGameClientMessage Message;
    Message.Sequence = NextSequence++;
    Message.Payload.PayloadCase = EGrpcGameClientMessagePayload::DialogueAuth;
    Message.Payload.DialogueAuth = MakeShared<FGrpcGameDialogueAuthRequest>();
    Message.Payload.DialogueAuth->NpcId = NpcId;
    SendMessage(MoveTemp(Message));
    return true;
}

// ====================================================================
// 快照推送
// ====================================================================

UInventoryManagerSubsystem* UGameServerSubsystem::GetInventoryManager() const
{
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return nullptr;
    }
    ULocalPlayer* LocalPlayer = GI->GetFirstGamePlayer();
    return LocalPlayer ? LocalPlayer->GetSubsystem<UInventoryManagerSubsystem>() : nullptr;
}

void UGameServerSubsystem::PushInventorySnapshot(const TArray<TSharedPtr<FGrpcGameItemInstance>>& Items)
{
    UInventoryManagerSubsystem* InventoryManager = GetInventoryManager();
    if (!InventoryManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 找不到 InventoryManagerSubsystem，快照丢弃。"));
        return;
    }

    TArray<FItemInstance> OutItems;
    TMap<FGuid, FWeaponInstanceData> OutWeaponMap;
    TMap<FGuid, FArtifactInstanceData> OutArtifactMap;
    OutItems.Reserve(Items.Num());

    for (const TSharedPtr<FGrpcGameItemInstance>& ItemPtr : Items)
    {
        if (!ItemPtr.IsValid())
        {
            continue;
        }
        const FGrpcGameItemInstance& In = *ItemPtr;

        FItemInstance Out;
        if (!FGuid::Parse(In.ItemGuid, Out.ItemGUID))
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameServer] 快照物品 GUID 非法 [%s]，跳过。"), *In.ItemGuid);
            continue;
        }
        Out.ItemID = In.ItemId;
        Out.Count = In.Count;
        Out.EquippedCharacterID = In.EquippedCharacterId;
        Out.AcquiredTime = FDateTime::FromUnixTimestamp(In.AcquiredTime / 1000);

        if (In.WeaponData.IsValid())
        {
            FWeaponInstanceData WeaponData;
            WeaponData.Level = In.WeaponData->Level;
            WeaponData.AscensionLevel = In.WeaponData->AscensionLevel;
            WeaponData.RefinementLevel = In.WeaponData->RefinementLevel;
            OutWeaponMap.Add(Out.ItemGUID, MoveTemp(WeaponData));
        }
        if (In.ArtifactData.IsValid())
        {
            FArtifactInstanceData ArtifactData;
            ArtifactData.SetID = In.ArtifactData->SetId;
            ArtifactData.Slot = SlotFromString(In.ArtifactData->Slot);
            ArtifactData.MainStat = StatFromString(In.ArtifactData->MainStat);
            ArtifactData.MainStatValue = In.ArtifactData->MainStatValue;
            for (const TSharedPtr<FGrpcGameArtifactSubStat>& SubPtr : In.ArtifactData->SubStats)
            {
                if (!SubPtr.IsValid())
                {
                    continue;
                }
                FArtifactSubStat SubStat;
                SubStat.StatType = StatFromString(SubPtr->StatType);
                SubStat.StatValue = SubPtr->StatValue;
                SubStat.UpgradeCount = SubPtr->UpgradeCount;
                ArtifactData.SubStats.Add(MoveTemp(SubStat));
            }
            OutArtifactMap.Add(Out.ItemGUID, MoveTemp(ArtifactData));
        }

        OutItems.Add(MoveTemp(Out));
    }

    InventoryManager->ApplyServerSnapshot(OutItems, OutWeaponMap, OutArtifactMap);
}

// ====================================================================
// 内部辅助
// ====================================================================

void UGameServerSubsystem::SendMessage(FGrpcGameClientMessage&& Message)
{
    if (GameClient && ChannelHandle.Value != 0)
    {
        GameClient->GameChannel(ChannelHandle, Message);
    }
}
