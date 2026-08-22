// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TurboLinkGrpcClient.h"
#include "SGame/GameMessage.h"
#include "GameServerSubsystem.generated.h"

class UGameService;
class UGameServiceClient;
class UInventoryManagerSubsystem;
class UTurboLinkGrpcManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameLoginResult, bool, bSuccess, const FString&, ErrorMsg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameInventoryOpResult, bool, bSuccess, const FString&, ErrorMsg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnGameDialogueAuthResult, bool, bOk, const FString&, DialogueToken, int64, ExpiresAtMs, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameKicked, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameServerDisconnected);

/**
 * 游戏服务器连接子系统 (M3)
 *
 * 持有到 GameServer (gRPC GameChannel 双向流) 的唯一客户端连接，
 * 负责登录握手、心跳保活、服务器消息分发与背包快照推送。
 * 背包数据以服务器为唯一权威：本端不产生背包状态，只应用服务器快照。
 */
UCLASS()
class OPENWORLDARPG_API UGameServerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 登录 / 登出 ---

    // 发起服务器登录，结果经 OnLoginResult 异步广播
    UFUNCTION(BlueprintCallable, Category = "GameServer")
    void RequestLogin(const FString& Account, const FString& Token);

    // 主动登出（关闭会话前调用）
    UFUNCTION(BlueprintCallable, Category = "GameServer")
    void RequestLogout();

    // --- 状态查询 ---

    UFUNCTION(BlueprintPure, Category = "GameServer")
    bool IsLoggedIn() const { return bLoggedIn; }

    UFUNCTION(BlueprintPure, Category = "GameServer")
    bool IsLoginPending() const { return bLoginPending; }

    UFUNCTION(BlueprintPure, Category = "GameServer")
    const FString& GetAccountName() const { return AccountName; }

    // --- 背包操作转发（由 UInventoryManagerSubsystem 调用，服务器权威执行） ---

    bool ServerAddItem(int32 ItemID, int32 Amount);
    bool ServerRemoveItem(const FGuid& ItemGUID, int32 Amount);
    bool ServerEquipItem(const FGuid& ItemGUID, int32 CharacterID);
    bool ServerUnequipItem(const FGuid& ItemGUID);
    bool ServerUseItem(const FGuid& ItemGUID, int32 TargetCharacterID, int32 Amount);

    // --- 对话授权（信令面）---

    // 向 GameServer 申请短期对话票据，客户端持票据直连 VHServer 推理流（数据面）；
    // 结果经 OnDialogueAuthResult 异步广播。返回 false 表示未登录或通道不可用。
    UFUNCTION(BlueprintCallable, Category = "GameServer")
    bool RequestDialogueAuth(int32 NpcId);

    // --- 事件委托 ---

    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameLoginResult OnLoginResult;

    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameInventoryOpResult OnInventoryOpResult;

    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameDialogueAuthResult OnDialogueAuthResult;

    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameKicked OnKicked;

    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameServerDisconnected OnServerDisconnected;

private:
    // 连接管理
    bool EnsureChannel();                       // 建 service + client + 流 context，失败返回 false
    void ResetChannel();                        // 销毁当前连接（登出/断线/重登前）
    void FailLogin(const FString& Reason);
    void SendHeartbeat();
    void OnLoginTimeout();

    // gRPC 回调
    UFUNCTION()
    void HandleServerMessage(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameServerMessage& Response);

    // 消息分发
    void HandleLoginResponse(const FGrpcGameLoginResponse& Login);
    void HandleInventoryOpResponse(const FGrpcGameInventoryOpResponse& Op);
    void HandleDialogueAuthResult(const FGrpcGameDialogueAuthResult& Result);
    void HandleKick(const FString& Reason);
    void HandleConnectionFailure(const FGrpcResult& GrpcResult);

    // 快照推送
    UInventoryManagerSubsystem* GetInventoryManager() const;
    void PushInventorySnapshot(const TArray<TSharedPtr<FGrpcGameItemInstance>>& Items);

    // 内部发送辅助
    void SendMessage(FGrpcGameClientMessage&& Message);

    UPROPERTY()
    TObjectPtr<UGameService> GameService;

    UPROPERTY()
    TObjectPtr<UGameServiceClient> GameClient;

    FGrpcContextHandle ChannelHandle;

    FString AccountName;
    FString SessionToken;
    uint64 NextSequence = 1;

    bool bLoginPending = false;
    bool bLoggedIn = false;

    FTimerHandle HeartbeatTimerHandle;
    FTimerHandle LoginTimeoutHandle;

    static constexpr float HeartbeatIntervalSec = 10.f;   // 服务器超时 30s，10s 心跳留足余量
    static constexpr float LoginTimeoutSec = 15.f;
};
