// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/GameServer/GameServerSubsystem.h"
#include "Systems/InventoryManager/InventoryManagerSubsystem.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "Core/OpenWorldARPGSettings.h"
#include "Core/Data/InitialArchiveData.h"
#include "Systems/AISystem/AIPatrolAreaBase.h"
#include "Characters/AICharacter/EnemyCharacter.h"
#include "Characters/PlayerCharacter/PlayerCharacter.h"

#include "SGame/GameService.h"
#include "SGame/GameClient.h"
#include "TurboLinkGrpcManager.h"
#include "TurboLinkGrpcUtilities.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
    int64 NowMs()
    {
        return FDateTime::UtcNow().ToUnixTimestamp() * 1000;
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
        GI->GetTimerManager().ClearTimer(ReconnectTimerHandle);
    }
    ResetChannel();
    Super::Deinitialize();
}

// ====================================================================
// gRPC 通道管理
// ====================================================================

bool UGameServerSubsystem::EnsureChannel()
{
    UTurboLinkGrpcManager* GrpcManager = UTurboLinkGrpcUtilities::GetTurboLinkGrpcManager(this);
    if (!GrpcManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] gRPC 管理器不可用"));
        return false;
    }

    GameService = Cast<UGameService>(GrpcManager->MakeService(TEXT("GameService")));
    if (!GameService)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] GameService 创建失败"));
        return false;
    }
    GameService->Connect();

    GameClient = GameService->MakeClient();
    if (!GameClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] GameService 客户端创建失败"));
        ResetChannel();
        return false;
    }

    GameClient->OnLoginResponse.AddDynamic(this, &UGameServerSubsystem::HandleLoginResponse);
    GameClient->OnLogoutResponse.AddDynamic(this, &UGameServerSubsystem::HandleLogoutResponse);
    GameClient->OnRegisterCharacterResponse.AddDynamic(this, &UGameServerSubsystem::HandleRegisterCharacterResponse);
    GameClient->OnSetActiveCharacterResponse.AddDynamic(this, &UGameServerSubsystem::HandleSetActiveCharacterResponse);
    GameClient->OnRequestEnemySpawnResponse.AddDynamic(this, &UGameServerSubsystem::HandleEnemySpawnResponse);
    GameClient->OnInventoryOpResponse.AddDynamic(this, &UGameServerSubsystem::HandleInventoryOpResponse);
    GameClient->OnAuthenticateDialogueResponse.AddDynamic(this, &UGameServerSubsystem::HandleDialogueAuthResponse);
    GameClient->OnWorldResponse.AddDynamic(this, &UGameServerSubsystem::HandleWorldResponse);

    UE_LOG(LogTemp, Log, TEXT("[GameServer] gRPC 通道已建立 (GameService)。"));
    return true;
}

void UGameServerSubsystem::ResetChannel()
{
    if (GameClient)
    {
        GameClient->OnLoginResponse.RemoveAll(this);
        GameClient->OnLogoutResponse.RemoveAll(this);
        GameClient->OnRegisterCharacterResponse.RemoveAll(this);
        GameClient->OnSetActiveCharacterResponse.RemoveAll(this);
        GameClient->OnRequestEnemySpawnResponse.RemoveAll(this);
        GameClient->OnInventoryOpResponse.RemoveAll(this);
        GameClient->OnAuthenticateDialogueResponse.RemoveAll(this);
        GameClient->OnWorldResponse.RemoveAll(this);
        if (WorldHandle.Value != 0)
        {
            GameClient->TryCancel(WorldHandle);
            WorldHandle = FGrpcContextHandle();
        }
        if (LoginHandle.Value != 0)
        {
            GameClient->TryCancel(LoginHandle);
            LoginHandle = FGrpcContextHandle();
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
}

FGrpcMetaData UGameServerSubsystem::MakeAuthMeta() const
{
    FGrpcMetaData Meta;
    if (!SessionToken.IsEmpty())
    {
        Meta.MetaData.Add(TEXT("authorization"), TEXT("Bearer ") + SessionToken);
    }
    return Meta;
}

// ====================================================================
// 登录 / 登出
// ====================================================================

void UGameServerSubsystem::RequestLogin(const FString& Account, const FString& InPassword)
{
    if (bLoginPending)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 登录请求进行中，忽略重复点击。"));
        return;
    }

    // 清理上一次会话的残留连接（断线重登 / 已登录再登录场景）
    ResetChannel();

    AccountName = Account;
    Password = InPassword;
    bLoggedIn = false;
    bReconnecting = false;

    if (!EnsureChannel())
    {
        FailLogin(TEXT("gRPC 通道创建失败"));
        return;
    }

    bLoginPending = true;
    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().SetTimer(LoginTimeoutHandle, this, &UGameServerSubsystem::OnLoginTimeout, LoginTimeoutSec, false);
    }

    SendLoginRequest();
}

void UGameServerSubsystem::SendLoginRequest()
{
    LoginHandle = GameClient->InitLogin();

    FGrpcGameLoginRequest Request;
    Request.Account = AccountName;
    Request.Password = Password;
    GameClient->Login(LoginHandle, Request, FGrpcMetaData(), LoginTimeoutSec);

    UE_LOG(LogTemp, Log, TEXT("[GameServer] unary Login 已发出 [account=%s]。"), *AccountName);
}

void UGameServerSubsystem::RequestLogout()
{
    if (!bLoggedIn && !bLoginPending && !bReconnecting)
    {
        return;
    }

    // fire-and-forget: 令服务器立即拆会话; 未送达时由服务器心跳超时兜底
    if (bLoggedIn && GameClient)
    {
        FGrpcContextHandle Handle = GameClient->InitLogout();
        FGrpcGameLogoutRequest Request;
        GameClient->Logout(Handle, Request, MakeAuthMeta());
    }

    bLoggedIn = false;
    bLoginPending = false;
    bReconnecting = false;
    AccountName.Reset();
    SessionToken.Reset();
    Password.Reset();
    PlayerId = 0;
    EnemyRegistry.Empty();

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
        GI->GetTimerManager().ClearTimer(ReconnectTimerHandle);
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
    FGrpcGameClientWorld Msg;
    Msg.Payload.PayloadCase = EGrpcGameClientWorldPayload::Heartbeat;
    Msg.Payload.Heartbeat = MakeShared<FGrpcGameHeartbeat>();
    Msg.Payload.Heartbeat->ClientTimestamp = NowMs();
    SendWorldMessage(Msg);
}

// ====================================================================
// World 双向流
// ====================================================================

void UGameServerSubsystem::OpenWorldStream()
{
    if (!GameClient)
    {
        return;
    }

    WorldHandle = GameClient->InitWorld();

    // 首帧心跳携带 session_token (消息体): TurboLink 双向流初始 metadata 无法上行,
    // 服务端改从首帧消息读 token 兑换在线会话 (登录与会话建立分离)
    FGrpcGameClientWorld Msg;
    Msg.Sequence = (uint64)NextSequence++;
    Msg.SessionToken = SessionToken;
    Msg.Payload.PayloadCase = EGrpcGameClientWorldPayload::Heartbeat;
    Msg.Payload.Heartbeat = MakeShared<FGrpcGameHeartbeat>();
    Msg.Payload.Heartbeat->ClientTimestamp = NowMs();
    GameClient->World(WorldHandle, Msg, MakeAuthMeta());

    UE_LOG(LogTemp, Log, TEXT("[GameServer] World 双向流已打开 [playerId=%llu]。"), PlayerId);
}

bool UGameServerSubsystem::SendWorldMessage(FGrpcGameClientWorld& Msg)
{
    if (!bLoggedIn || !GameClient || WorldHandle.Value == 0)
    {
        return false;
    }

    Msg.Sequence = (uint64)NextSequence++;
    GameClient->World(WorldHandle, Msg, MakeAuthMeta());
    return true;
}

void UGameServerSubsystem::HandleWorldResponse(FGrpcContextHandle Handle,
                                               const FGrpcResult& GrpcResult,
                                               const FGrpcGameServerWorld& Response)
{
    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        // World 流中断: 登录中判失败, 在线则进入重连
        if (bLoginPending)
        {
            FailLogin(FString::Printf(TEXT("World 流建立失败 [%s]"), *GrpcResult.GetMessageString()));
        }
        else if (bLoggedIn && !bReconnecting)
        {
            StartReconnect();
        }
        return;
    }

    switch (Response.Payload.PayloadCase)
    {
        case EGrpcGameServerWorldPayload::Heartbeat:
            // 心跳保活，无需处理
            break;

        case EGrpcGameServerWorldPayload::PlayerEnter:
            if (Response.Payload.PlayerEnter.IsValid())
            {
                HandleWorldPlayerEnter(*Response.Payload.PlayerEnter);
            }
            break;

        case EGrpcGameServerWorldPayload::PlayerLeave:
            if (Response.Payload.PlayerLeave.IsValid())
            {
                HandleWorldPlayerLeave(*Response.Payload.PlayerLeave);
            }
            break;

        case EGrpcGameServerWorldPayload::PlayerMove:
            if (Response.Payload.PlayerMove.IsValid())
            {
                HandleWorldPlayerMove(*Response.Payload.PlayerMove);
            }
            break;

        case EGrpcGameServerWorldPayload::PositionCorrection:
            if (Response.Payload.PositionCorrection.IsValid())
            {
                HandleWorldPositionCorrection(*Response.Payload.PositionCorrection);
            }
            break;

        case EGrpcGameServerWorldPayload::EnemySpawn:
            if (Response.Payload.EnemySpawn.IsValid())
            {
                HandleWorldEnemySpawn(*Response.Payload.EnemySpawn);
            }
            break;

        case EGrpcGameServerWorldPayload::DamageDeal:
            if (Response.Payload.DamageDeal.IsValid())
            {
                HandleWorldDamageDeal(*Response.Payload.DamageDeal);
            }
            break;

        case EGrpcGameServerWorldPayload::PlayerDamage:
            if (Response.Payload.PlayerDamage.IsValid())
            {
                HandleWorldPlayerDamage(*Response.Payload.PlayerDamage);
            }
            break;

        case EGrpcGameServerWorldPayload::Kick:
            if (Response.Payload.Kick.IsValid())
            {
                HandleKick(Response.Payload.Kick->Reason);
            }
            break;

        case EGrpcGameServerWorldPayload::Error:
            if (Response.Payload.Error.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[GameServer] World 流错误 [code=%d, msg=%s]"),
                       Response.Payload.Error->Code, *Response.Payload.Error->Message);
            }
            break;

        default:
            break;
    }
}

// ====================================================================
// 断线重连
// ====================================================================

void UGameServerSubsystem::StartReconnect()
{
    if (bReconnecting)
    {
        return;
    }
    bReconnecting = true;
    bLoggedIn = false;
    ReconnectDeadlineMs = FPlatformTime::Seconds() * 1000.0 + ReconnectGraceSec * 1000.0;

    UE_LOG(LogTemp, Warning, TEXT("[GameServer] World 流断开，开始自动重连 [account=%s]。"), *AccountName);
    OnServerDisconnected.Broadcast();

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().SetTimer(ReconnectTimerHandle, this,
                                       &UGameServerSubsystem::OnReconnectTick,
                                       ReconnectIntervalSec, true);
    }
}

void UGameServerSubsystem::OnReconnectTick()
{
    if (!bReconnecting)
    {
        return;
    }

    const double NowMs = FPlatformTime::Seconds() * 1000.0;
    if (NowMs >= ReconnectDeadlineMs)
    {
        // 重连超时: 服务器侧会话已过期, 正式离线
        bReconnecting = false;
        if (UGameInstance* GI = GetGameInstance())
        {
            GI->GetTimerManager().ClearTimer(ReconnectTimerHandle);
        }
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 重连超时，会话已过期 [account=%s]。"), *AccountName);
        return;
    }

    // 重新走完整登录流: 服务端同账号 BindAccount 踢旧会话, 客户端无需 resume 令牌
    if (!bLoginPending)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameServer] 尝试重连 [account=%s]。"), *AccountName);
        bLoginPending = true;
        SendLoginRequest();
    }
}

// ====================================================================
// unary 回调
// ====================================================================

void UGameServerSubsystem::HandleLoginResponse(FGrpcContextHandle Handle,
                                               const FGrpcResult& GrpcResult,
                                               const FGrpcGameLoginResponse& Response)
{
    if (!bLoginPending)
    {
        return;
    }

    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        FailLogin(FString::Printf(TEXT("登录 RPC 失败 [%s]"), *GrpcResult.GetMessageString()));
        return;
    }
    if (!Response.Success)
    {
        const FString Reason = Response.ErrorMsg.IsEmpty() ? TEXT("登录被拒绝") : Response.ErrorMsg;
        FailLogin(Reason);
        return;
    }

    const bool bWasReconnecting = bReconnecting;
    bLoginPending = false;
    bReconnecting = false;
    SessionToken = Response.SessionToken;
    PlayerId = Response.PlayerId.Value;
    SpawnLocation = FVector(Response.SpawnX, Response.SpawnY, Response.SpawnZ);
    SpawnYaw = Response.SpawnYaw;

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(LoginTimeoutHandle);
        GI->GetTimerManager().ClearTimer(ReconnectTimerHandle);
    }

    UE_LOG(LogTemp, Log, TEXT("[GameServer] Login 成功 [account=%s, playerId=%llu, spawn=(%.1f,%.1f,%.1f)]"),
           *AccountName, PlayerId, SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);

    ApplyLoginSnapshot(Response);

    // Bearer 令牌打开 World 双向流, 服务端兑换为在线会话
    OpenWorldStream();
    bLoggedIn = true;

    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().SetTimer(HeartbeatTimerHandle, this, &UGameServerSubsystem::SendHeartbeat, HeartbeatIntervalSec, true);
    }

    if (!bWasReconnecting)
    {
        OnLoginResult.Broadcast(true, TEXT(""));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[GameServer] 重连成功 [account=%s]。"), *AccountName);
    }
}

void UGameServerSubsystem::HandleLogoutResponse(FGrpcContextHandle Handle,
                                                const FGrpcResult& GrpcResult,
                                                const FGrpcGameLogoutResponse& Response)
{
    UE_LOG(LogTemp, Log, TEXT("[GameServer] Logout 响应 [success=%d]。"), Response.Success ? 1 : 0);
}

void UGameServerSubsystem::HandleRegisterCharacterResponse(FGrpcContextHandle Handle,
                                                            const FGrpcResult& GrpcResult,
                                                            const FGrpcGameRegisterCharacterResponse& Response)
{
    if (GrpcResult.Code != EGrpcResultCode::Ok || !Response.Success)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] RegisterCharacter 被拒绝: %s"),
               GrpcResult.Code != EGrpcResultCode::Ok ? *GrpcResult.GetMessageString() : *Response.ErrorMsg);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[GameServer] 角色已登记 [max_hp=%.0f]"),
           Response.MaxHp.Value);
}

void UGameServerSubsystem::HandleSetActiveCharacterResponse(FGrpcContextHandle Handle,
                                                             const FGrpcResult& GrpcResult,
                                                             const FGrpcGameSetActiveCharacterResponse& Response)
{
    if (GrpcResult.Code != EGrpcResultCode::Ok || !Response.Success)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] SetActiveCharacter 被拒绝: %s"),
               GrpcResult.Code != EGrpcResultCode::Ok ? *GrpcResult.GetMessageString() : *Response.ErrorMsg);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[GameServer] 上场角色已切换 [tag=%s]"), *Response.CharacterTag);
}

void UGameServerSubsystem::HandleEnemySpawnResponse(FGrpcContextHandle Handle,
                                                    const FGrpcResult& GrpcResult,
                                                    const FGrpcGameEnemySpawnResponse& Response)
{
    // 授权结果仅日志: 敌人实体仍由服务器经 World 流 EnemySpawn 下发生成
    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] RequestEnemySpawn RPC 失败: %s"), *GrpcResult.GetMessageString());
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[GameServer] 刷怪授权 [granted=%d, reason=%s, enemy_id=%llu]"),
           Response.Granted ? 1 : 0, *Response.Reason, Response.EnemyId.Value);
}

void UGameServerSubsystem::HandleInventoryOpResponse(FGrpcContextHandle Handle,
                                                     const FGrpcResult& GrpcResult,
                                                     const FGrpcGameInventoryOpResponse& Response)
{
    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 背包操作 RPC 失败: %s"), *GrpcResult.GetMessageString());
        OnInventoryOpResult.Broadcast(false, GrpcResult.GetMessageString());
        return;
    }
    if (!Response.Success)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 背包操作被服务器拒绝: %s"), *Response.ErrorMsg);
        OnInventoryOpResult.Broadcast(false, Response.ErrorMsg);
        return;
    }

    PushInventorySnapshot(Response.Items);
    OnInventoryOpResult.Broadcast(true, TEXT(""));
}

void UGameServerSubsystem::HandleDialogueAuthResponse(FGrpcContextHandle Handle,
                                                      const FGrpcResult& GrpcResult,
                                                      const FGrpcGameDialogueAuthResult& Result)
{
    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        OnDialogueAuthResult.Broadcast(false, TEXT(""), 0, GrpcResult.GetMessageString());
        return;
    }
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

// ====================================================================
// World 流下行分发
// ====================================================================

void UGameServerSubsystem::HandleWorldPlayerEnter(const FGrpcGamePlayerEnter& Enter)
{
    UE_LOG(LogTemp, Log, TEXT("[GameServer] 远程玩家进入视野 [playerId=%llu, account=%s, pos=(%.1f,%.1f,%.1f)]"),
           Enter.PlayerId.Value, *Enter.Account, Enter.X, Enter.Y, Enter.Z);
    OnRemotePlayerEnter.Broadcast((int64)Enter.PlayerId.Value, Enter.Account,
                                  FVector(Enter.X, Enter.Y, Enter.Z), Enter.Yaw);
}

void UGameServerSubsystem::HandleWorldPlayerLeave(const FGrpcGamePlayerLeave& Leave)
{
    UE_LOG(LogTemp, Log, TEXT("[GameServer] 远程玩家离开视野 [playerId=%llu]"), Leave.PlayerId.Value);
    OnRemotePlayerLeave.Broadcast((int64)Leave.PlayerId.Value);
}

void UGameServerSubsystem::HandleWorldPlayerMove(const FGrpcGamePlayerMove& Move)
{
    OnRemotePlayerMove.Broadcast((int64)Move.PlayerId.Value,
                                 FVector(Move.X, Move.Y, Move.Z), Move.Yaw);
}

void UGameServerSubsystem::HandleWorldPositionCorrection(const FGrpcGamePositionCorrection& Correction)
{
    UE_LOG(LogTemp, Warning, TEXT("[GameServer] 位置校正 [pos=(%.1f,%.1f,%.1f), yaw=%.1f]"),
           Correction.X, Correction.Y, Correction.Z, Correction.Yaw);
    OnPositionCorrection.Broadcast(FVector(Correction.X, Correction.Y, Correction.Z), Correction.Yaw);
}

void UGameServerSubsystem::HandleWorldEnemySpawn(const FGrpcGameEnemySpawn& Spawn)
{
    UE_LOG(LogTemp, Log, TEXT("[GameServer] 服务器刷怪 [enemy_id=%llu, area=%d, type=%d, count=%d, hp=%d]"),
           Spawn.EnemyId.Value, Spawn.AreaId, Spawn.EnemyType, Spawn.Count, Spawn.MaxHp);

    UWorld* World = GetWorld();
    if (!World) return;

    // 定位目标巡逻区 (area_id 匹配), 由巡逻区负责在其旁生成敌人并绑定 PatrolArea
    for (TActorIterator<AAIPatrolAreaBase> It(World); It; ++It)
    {
        if (AAIPatrolAreaBase* Area = *It)
        {
            if (Area->MatchesAreaId(Spawn.AreaId))
            {
                Area->HandleServerEnemySpawn(Spawn);
                return;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[GameServer] 未找到匹配的巡逻区 [area_id=%d], 刷怪跳过"), Spawn.AreaId);
}

void UGameServerSubsystem::HandleWorldDamageDeal(const FGrpcGameDamageDeal& Deal)
{
    // 服务器权威伤害 -> 命中敌人做血条/死亡表现 (纯表现, 无本地扣血计算)
    const uint64 EnemyId = Deal.TargetEnemyId.Value;
    if (TWeakObjectPtr<AEnemyCharacter>* Found = EnemyRegistry.Find(EnemyId))
    {
        if (AEnemyCharacter* Enemy = Found->Get())
        {
            Enemy->ApplyServerDamage(Deal);
            return;
        }
        EnemyRegistry.Remove(EnemyId);
    }
    UE_LOG(LogTemp, Warning, TEXT("[GameServer] DamageDeal 找不到敌人 [enemy_id=%llu]"), EnemyId);
}

void UGameServerSubsystem::HandleWorldPlayerDamage(const FGrpcGamePlayerDamage& Damage)
{
    // 服务器权威玩家 HP -> 按 character_tag 找到对应 PlayerCharacter, 改写其 ASC 血条/死亡表现
    // (纯表现, 无本地扣血). 队伍内各角色血线独立, 此 Tag 即服务器 HP 实体的主键.
    FGameplayTag CharacterTag = FGameplayTag::RequestGameplayTag(FName(*Damage.CharacterTag));
    if (!CharacterTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] PlayerDamage 无效角色 Tag [tag=%s]"), *Damage.CharacterTag);
        return;
    }

    APlayerCharacter* Target = nullptr;
    UWorld* World = GetWorld();
    if (World)
    {
        for (TActorIterator<APlayerCharacter> It(World); It; ++It)
        {
            APlayerCharacter* Char = *It;
            if (Char && Char->GetServerCharacterTag() == CharacterTag)
            {
                Target = Char;
                break;
            }
        }
    }

    if (Target)
    {
        Target->ApplyServerPlayerDamage(Damage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] PlayerDamage 找不到角色 [tag=%s]"),
               *CharacterTag.ToString());
    }
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
        GI->GetTimerManager().ClearTimer(ReconnectTimerHandle);
    }

    bLoggedIn = false;
    bLoginPending = false;
    bReconnecting = false;
    AccountName.Reset();
    SessionToken.Reset();
    Password.Reset();
    PlayerId = 0;
    EnemyRegistry.Empty();

    if (bWasLoggedIn)
    {
        OnKicked.Broadcast(Reason);
    }
}

// ====================================================================
// 实时玩法上报 (World 流) 与命令 (unary)
// ====================================================================

bool UGameServerSubsystem::SendMovementUpdate(const FVector& Location, float Yaw)
{
    FGrpcGameClientWorld Msg;
    Msg.Payload.PayloadCase = EGrpcGameClientWorldPayload::Movement;
    Msg.Payload.Movement = MakeShared<FGrpcGameMovementUpdate>();
    Msg.Payload.Movement->X = Location.X;
    Msg.Payload.Movement->Y = Location.Y;
    Msg.Payload.Movement->Z = Location.Z;
    Msg.Payload.Movement->Yaw = Yaw;
    Msg.Payload.Movement->ClientTimestamp = NowMs();
    return SendWorldMessage(Msg);
}

bool UGameServerSubsystem::SendDamageIntent(int32 SkillId, uint64 TargetEnemyId)
{
    FGrpcGameClientWorld Msg;
    Msg.Payload.PayloadCase = EGrpcGameClientWorldPayload::DamageIntent;
    Msg.Payload.DamageIntent = MakeShared<FGrpcGameDamageIntent>();
    Msg.Payload.DamageIntent->SkillId = SkillId;
    // attacker_player_id 由服务器从会话归属填充, 客户端无需自报
    Msg.Payload.DamageIntent->TargetEnemyId = TargetEnemyId;
    Msg.Payload.DamageIntent->ClientTimestamp = NowMs();
    return SendWorldMessage(Msg);
}

bool UGameServerSubsystem::SendEnemyAttackIntent(uint64 EnemyId)
{
    FGrpcGameClientWorld Msg;
    Msg.Payload.PayloadCase = EGrpcGameClientWorldPayload::EnemyAttackIntent;
    Msg.Payload.EnemyAttackIntent = MakeShared<FGrpcGameEnemyAttackIntent>();
    Msg.Payload.EnemyAttackIntent->EnemyId = EnemyId;
    // 目标为本地玩家会话内 active 角色, 服务器据此扣血并以 PlayerDamage 回馈
    Msg.Payload.EnemyAttackIntent->ClientTimestamp = NowMs();
    return SendWorldMessage(Msg);
}

bool UGameServerSubsystem::SendSpawnRequest(int32 AreaId, float PatrolRadius, const FVector& Location)
{
    if (!bLoggedIn || !GameClient)
    {
        return false;
    }

    FGrpcContextHandle Handle = GameClient->InitRequestEnemySpawn();
    FGrpcGameEnemySpawnRequest Request;
    Request.AreaId = AreaId;
    Request.X = Location.X;
    Request.Y = Location.Y;
    Request.Z = Location.Z;
    Request.PatrolRadius = PatrolRadius;
    GameClient->RequestEnemySpawn(Handle, Request, MakeAuthMeta());
    return true;
}

bool UGameServerSubsystem::SendRegisterCharacter(const FGameplayTag& CharacterTag, double MaxHp)
{
    if (!bLoggedIn || !GameClient || !CharacterTag.IsValid())
    {
        return false;
    }

    FGrpcContextHandle Handle = GameClient->InitRegisterCharacter();
    FGrpcGameRegisterCharacterRequest Request;
    Request.CharacterTag = CharacterTag.ToString();
    Request.MaxHp = MaxHp;
    GameClient->RegisterCharacter(Handle, Request, MakeAuthMeta());
    return true;
}

bool UGameServerSubsystem::SendSetActiveCharacter(const FGameplayTag& CharacterTag)
{
    if (!bLoggedIn || !GameClient || !CharacterTag.IsValid())
    {
        return false;
    }

    FGrpcContextHandle Handle = GameClient->InitSetActiveCharacter();
    FGrpcGameSetActiveCharacterRequest Request;
    Request.CharacterTag = CharacterTag.ToString();
    GameClient->SetActiveCharacter(Handle, Request, MakeAuthMeta());
    return true;
}

void UGameServerSubsystem::RegisterEnemy(uint64 EnemyId, AEnemyCharacter* Enemy)
{
    if (!Enemy) return;
    EnemyRegistry.Add(EnemyId, Enemy);
}

void UGameServerSubsystem::UnregisterEnemy(uint64 EnemyId)
{
    EnemyRegistry.Remove(EnemyId);
}

// ====================================================================
// 背包操作转发 (unary InventoryOp)
// ====================================================================

bool UGameServerSubsystem::ServerAddItem(int32 ItemID, int32 Amount)
{
    if (!bLoggedIn || !GameClient)
    {
        return false;
    }

    FGrpcGameInventoryOpRequest Request;
    Request.Op.OpCase = EGrpcGameInventoryOpRequestOp::Add;
    Request.Op.Add = MakeShared<FGrpcGameInventoryAdd>();
    Request.Op.Add->ItemId = ItemID;
    Request.Op.Add->Amount = Amount;

    FGrpcContextHandle Handle = GameClient->InitInventoryOp();
    GameClient->InventoryOp(Handle, Request, MakeAuthMeta());
    return true;
}

bool UGameServerSubsystem::ServerRemoveItem(const FGuid& ItemGUID, int32 Amount)
{
    if (!bLoggedIn || !GameClient)
    {
        return false;
    }

    FGrpcGameInventoryOpRequest Request;
    Request.Op.OpCase = EGrpcGameInventoryOpRequestOp::Remove;
    Request.Op.Remove = MakeShared<FGrpcGameInventoryRemove>();
    Request.Op.Remove->ItemGuid = ItemGUID.ToString(EGuidFormats::DigitsWithHyphensLower);
    Request.Op.Remove->Amount = Amount;

    FGrpcContextHandle Handle = GameClient->InitInventoryOp();
    GameClient->InventoryOp(Handle, Request, MakeAuthMeta());
    return true;
}

// ====================================================================
// 对话授权（信令面，unary AuthenticateDialogue）
// ====================================================================

bool UGameServerSubsystem::RequestDialogueAuth(int32 NpcId)
{
    if (!bLoggedIn || !GameClient)
    {
        return false;
    }

    FGrpcContextHandle Handle = GameClient->InitAuthenticateDialogue();
    FGrpcGameDialogueAuthRequest Request;
    Request.NpcId = NpcId;
    GameClient->AuthenticateDialogue(Handle, Request, MakeAuthMeta());
    return true;
}

// ====================================================================
// 登录快照落地
// ====================================================================

void UGameServerSubsystem::ApplyLoginSnapshot(const FGrpcGameLoginResponse& Login)
{
    PushInventorySnapshot(Login.Inventory);

    // 服务器权威存档快照 (B 方案): 拥有角色 + 配队 + 上场 index.
    // 拥有角色以服务器为准 (拥有/等级/经验), 本地资产兜底角色细节字段 (属性快照/装备/天赋).
    ServerOwnedCharacters.Reset();
    ServerTeamTags.Reset();
    ServerActiveCharacterIndex = -1;

    UInitialArchiveData* LocalArchive = nullptr;
    const TSoftObjectPtr<UInitialArchiveData>& ArchiveSoftPtr = UOpenWorldARPGSettings::Get().InitialArchiveData;
    if (!ArchiveSoftPtr.IsNull())
    {
        LocalArchive = ArchiveSoftPtr.LoadSynchronous();
    }

    for (const TSharedPtr<FGrpcGameOwnedCharacter>& OcPtr : Login.OwnedCharacters)
    {
        if (!OcPtr.IsValid())
        {
            continue;
        }
        const FGrpcGameOwnedCharacter& Oc = *OcPtr;
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Oc.CharacterTag));
        if (!Tag.IsValid()) continue;

        FCharacterSaveData Merged;
        if (LocalArchive)
        {
            for (const FCharacterSaveData& LocalData : LocalArchive->InitialOwnedCharacters)
            {
                if (LocalData.CharacterTag == Tag)
                {
                    Merged = LocalData;
                    break;
                }
            }
        }
        Merged.CharacterTag = Tag;
        Merged.CharacterLevel = Oc.Level;
        Merged.Experience = Oc.Exp;
        ServerOwnedCharacters.Add(Merged);
    }

    for (const TSharedPtr<FGrpcGameTeamSlot>& TsPtr : Login.TeamSlots)
    {
        if (!TsPtr.IsValid())
        {
            continue;
        }
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TsPtr->CharacterTag));
        ServerTeamTags.Add(Tag);
        if (TsPtr->IsActive)
        {
            ServerActiveCharacterIndex = TsPtr->SlotIndex;
        }
    }
    bHasServerArchive = ServerOwnedCharacters.Num() > 0;
    if (ServerActiveCharacterIndex < 0) ServerActiveCharacterIndex = 0;

    UE_LOG(LogTemp, Log, TEXT("[GameServer] 登录携带权威存档 [owned=%d, team=%d, active=%d]"),
           ServerOwnedCharacters.Num(), ServerTeamTags.Num(), ServerActiveCharacterIndex);
}

// ====================================================================
// 背包快照推送
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

void UGameServerSubsystem::ApplyInventorySnapshot(TArray<FItemInstance>&& OutItems)
{
    UInventoryManagerSubsystem* InventoryManager = GetInventoryManager();
    if (!InventoryManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameServer] 找不到 InventoryManagerSubsystem，快照丢弃。"));
        return;
    }
    InventoryManager->ApplyServerSnapshot(OutItems);
}

void UGameServerSubsystem::PushInventorySnapshot(const TArray<TSharedPtr<FGrpcGameItemInstance>>& Items)
{
    TArray<FItemInstance> OutItems;
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
        Out.AcquiredTime = FDateTime::FromUnixTimestamp(In.AcquiredTime / 1000);

        OutItems.Add(MoveTemp(Out));
    }

    ApplyInventorySnapshot(MoveTemp(OutItems));
}
