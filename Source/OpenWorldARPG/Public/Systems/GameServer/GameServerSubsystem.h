// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TurboLinkGrpcClient.h"
#include "SGame/GameMessage.h"
#include "Systems/InventoryManager/Data/ItemInstance.h"
#include "Characters/PlayerCharacter/Data/CharacterSaveData.h"
#include "GameplayTagContainer.h"
#include "GameServerSubsystem.generated.h"

class UGameService;
class UGameServiceClient;
class UInventoryManagerSubsystem;
class UTurboLinkGrpcManager;
class AEnemyCharacter;
class AAIPatrolAreaBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameLoginResult, bool, bSuccess, const FString&, ErrorMsg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameInventoryOpResult, bool, bSuccess, const FString&, ErrorMsg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnGameDialogueAuthResult, bool, bOk, const FString&, DialogueToken, int64, ExpiresAtMs, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameKicked, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameServerDisconnected);

// --- 世界同步 (Phase 1: 状态同步 + AOI) ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnGameRemotePlayerEnter, int64, PlayerId, const FString&, Account, FVector, Location, float, Yaw);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameRemotePlayerLeave, int64, PlayerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGameRemotePlayerMove, int64, PlayerId, FVector, Location, float, Yaw);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGamePositionCorrection, FVector, Location, float, Yaw);

/**
 * 游戏服务器连接子系统 (gRPC 单通道)
 *
 * 全部业务经 gRPC 承载, RPC 形态按业务语义收敛:
 *   unary           : Login/Logout/RegisterCharacter/SetActiveCharacter/
 *                     RequestEnemySpawn/InventoryOp/AuthenticateDialogue (低频命令)
 *   bidi-streaming  : World (实时玩法: 心跳/移动/伤害上报 + AOI/战斗权威推送)
 *
 * 登录流: unary Login 建档并签发会话令牌 (session_token), 登录响应携带权威快照
 * (出生点/背包/拥有角色/配队); 客户端随后凭 Bearer 令牌打开 World 双向流,
 * 服务端将令牌兑换为在线会话 (登录与会话建立分离), 之后所有 unary 以同一令牌鉴权.
 *
 * 断线重连: World 流断开后自动重试 Login + 重开 World 流;
 * 服务端同账号重复登录由 BindAccount 踢旧会话兜底, 客户端无需 resume 令牌.
 */
UCLASS()
class OPENWORLDARPG_API UGameServerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- 登录 / 登出 ---

    // 发起服务器登录 (unary Login -> Bearer 开 World 流), 结果经 OnLoginResult 异步广播
    UFUNCTION(BlueprintCallable, Category = "GameServer")
    void RequestLogin(const FString& Account, const FString& Password);

    // 主动登出（unary Logout + 关闭 World 流）
    UFUNCTION(BlueprintCallable, Category = "GameServer")
    void RequestLogout();

    // --- 状态查询 ---

    UFUNCTION(BlueprintPure, Category = "GameServer")
    bool IsLoggedIn() const { return bLoggedIn; }

    UFUNCTION(BlueprintPure, Category = "GameServer")
    bool IsLoginPending() const { return bLoginPending; }

    UFUNCTION(BlueprintPure, Category = "GameServer")
    bool IsReconnecting() const { return bReconnecting; }

    UFUNCTION(BlueprintPure, Category = "GameServer")
    const FString& GetAccountName() const { return AccountName; }

    // --- 背包操作转发（由 UInventoryManagerSubsystem 调用，服务器权威执行，走 unary InventoryOp） ---

    bool ServerAddItem(int32 ItemID, int32 Amount);
    bool ServerRemoveItem(const FGuid& ItemGUID, int32 Amount);

    // --- 对话授权（信令面，走 unary AuthenticateDialogue）---

    // 向 GameServer 申请短期对话票据，客户端持票据直连 VHServer 推理流（数据面）；
    // 结果经 OnDialogueAuthResult 异步广播。返回 false 表示未登录或通道不可用。
    UFUNCTION(BlueprintCallable, Category = "GameServer")
    bool RequestDialogueAuth(int32 NpcId);

    // --- 世界同步 (Phase 1: 状态同步 + AOI，走 World 双向流) ---

    // 服务器权威出生点（登录响应携带）
    UFUNCTION(BlueprintPure, Category = "GameServer")
    FVector GetSpawnLocation() const { return SpawnLocation; }

    UFUNCTION(BlueprintPure, Category = "GameServer")
    float GetSpawnYaw() const { return SpawnYaw; }

    // --- 服务器权威存档快照 (登录响应携带, B 方案) ---

    // 登录响应是否携带权威存档 (拥有角色 + 配队 + 上场 index)
    UFUNCTION(BlueprintPure, Category = "GameServer")
    bool HasServerArchive() const { return bHasServerArchive; }

    // 拥有角色 (服务器权威拥有/等级/经验 + 本地资产兜底细节字段)
    const TArray<FCharacterSaveData>& GetServerOwnedCharacters() const { return ServerOwnedCharacters; }

    // 配队列表 (服务器权威顺序)
    const TArray<FGameplayTag>& GetServerTeamTags() const { return ServerTeamTags; }

    // 当前上场角色 index (服务器权威)
    int32 GetServerActiveCharacterIndex() const { return ServerActiveCharacterIndex; }

    // 移动上报（由 UPlayerMovementReportComponent 定时调用，走 World 流）：
    // 服务器权威校验（速度/瞬移/时间戳）后广播给视野内玩家，非法移动由 PositionCorrection 回执。
    // 返回 false 表示未登录或通道不可用。
    bool SendMovementUpdate(const FVector& Location, float Yaw);

    // --- 战斗 (Phase 3: 服务器权威伤害结算) ---

    // 攻击命中上报 (走 World 流)：客户端只上报"打了哪个 enemy_id + 什么技能"，
    // 不含伤害数值；伤害由服务器裁决并以 DamageDeal 广播，客户端据此做血条/死亡表现。
    // 返回 false 表示未登录或通道不可用。
    bool SendDamageIntent(int32 SkillId, uint64 TargetEnemyId);

    // 巡逻区刷怪上报 (走 unary RequestEnemySpawn)：AIPatrolAreaBase BeginPlay 时调用，
    // 上报所在巡逻区 ID、位置与巡逻半径；服务器以其约束刷点 (敌人出生即在巡逻圈内)，
    // 允许后经 World 流下发 EnemySpawn，客户端据此刷怪。返回 false 表示未登录或通道不可用。
    bool SendSpawnRequest(int32 AreaId, float PatrolRadius, const FVector& Location);

    // 敌人实体登记 (enemy_id -> 敌人): 巡逻区服务器刷怪成功后注册，供 DamageDeal 定位表现目标
    void RegisterEnemy(uint64 EnemyId, AEnemyCharacter* Enemy);
    void UnregisterEnemy(uint64 EnemyId);

    // 敌人攻击命中上报 (走 World 流)：EnemyCharacter 命中本地玩家时调用，
    // 只上报"哪个敌人打了谁"，不含伤害数值；服务器裁决伤害并以 PlayerDamage 广播，
    // 客户端据此改写玩家 ASC HP（纯表现）。返回 false 表示未登录或通道不可用。
    bool SendEnemyAttackIntent(uint64 EnemyId);

    // 生成/切换角色上报 (走 unary)：角色生成时按 Tag 建立服务器 HP 实体 (RegisterCharacter)，
    // 切人成为 active 时上报 (SetActiveCharacter)。服务器据此裁决受击对象 (仅 active 角色扣血)。
    // 返回 false 表示未登录或通道不可用或 Tag 无效。
    bool SendRegisterCharacter(const FGameplayTag& CharacterTag, double MaxHp);
    bool SendSetActiveCharacter(const FGameplayTag& CharacterTag);

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

    // 视野内玩家进入（服务器 AOI 广播）：RemotePlayerManager 据此生成远程玩家实体
    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameRemotePlayerEnter OnRemotePlayerEnter;

    // 视野内玩家离开：据此销毁远程玩家实体
    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameRemotePlayerLeave OnRemotePlayerLeave;

    // 视野内玩家移动广播（服务器权威位置）
    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGameRemotePlayerMove OnRemotePlayerMove;

    // 服务器位置校正：本地移动上报被拒绝（瞬移/超速/乱序）时回执权威位置
    UPROPERTY(BlueprintAssignable, Category = "GameServer|Events")
    FOnGamePositionCorrection OnPositionCorrection;

private:
    // ---- gRPC 通道管理 ----
    bool EnsureChannel();                       // 建 service + client + 绑定 RPC 回调
    void ResetChannel();                        // 取消在途 RPC 并销毁客户端 (登出/断线/重登前)
    void FailLogin(const FString& Reason);
    void SendHeartbeat();
    void OnLoginTimeout();
    FGrpcMetaData MakeAuthMeta() const;         // Bearer session_token 鉴权 metadata

    // ---- 登录与 World 流 ----
    void SendLoginRequest();                    // unary Login (初次登录/重连共用)
    void OpenWorldStream();                     // Bearer 令牌打开 World 双向流 (首帧心跳携带 metadata)
    bool SendWorldMessage(FGrpcGameClientWorld& Msg);   // World 流上行 (自动编号 + 携带鉴权)
    void StartReconnect();                      // World 流断开后进入自动重连
    void OnReconnectTick();                     // 重连定时器: 重新 Login / 超时放弃

    // ---- gRPC 回调 (unary) ----
    UFUNCTION()
    void HandleLoginResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameLoginResponse& Response);
    UFUNCTION()
    void HandleLogoutResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameLogoutResponse& Response);
    UFUNCTION()
    void HandleRegisterCharacterResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameRegisterCharacterResponse& Response);
    UFUNCTION()
    void HandleSetActiveCharacterResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameSetActiveCharacterResponse& Response);
    UFUNCTION()
    void HandleEnemySpawnResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameEnemySpawnResponse& Response);
    UFUNCTION()
    void HandleInventoryOpResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameInventoryOpResponse& Response);
    UFUNCTION()
    void HandleDialogueAuthResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameDialogueAuthResult& Result);

    // ---- gRPC 回调 (World 双向流) ----
    UFUNCTION()
    void HandleWorldResponse(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcGameServerWorld& Response);

    // ---- World 流下行分发 ----
    void HandleWorldPlayerEnter(const FGrpcGamePlayerEnter& Enter);
    void HandleWorldPlayerLeave(const FGrpcGamePlayerLeave& Leave);
    void HandleWorldPlayerMove(const FGrpcGamePlayerMove& Move);
    void HandleWorldPositionCorrection(const FGrpcGamePositionCorrection& Correction);
    void HandleWorldEnemySpawn(const FGrpcGameEnemySpawn& Spawn);
    void HandleWorldDamageDeal(const FGrpcGameDamageDeal& Deal);
    void HandleWorldPlayerDamage(const FGrpcGamePlayerDamage& Damage);
    void HandleKick(const FString& Reason);
    void ApplyLoginSnapshot(const FGrpcGameLoginResponse& Login);   // 背包/拥有角色/配队快照落地

    // ---- 快照推送 ----
    UInventoryManagerSubsystem* GetInventoryManager() const;
    void ApplyInventorySnapshot(TArray<FItemInstance>&& OutItems);
    void PushInventorySnapshot(const TArray<TSharedPtr<FGrpcGameItemInstance>>& Items);

    UPROPERTY()
    TObjectPtr<UGameService> GameService;

    UPROPERTY()
    TObjectPtr<UGameServiceClient> GameClient;

    FGrpcContextHandle WorldHandle;    // World 双向流句柄 (登录成功后常驻)
    FGrpcContextHandle LoginHandle;    // 在途 unary Login 句柄

    FString AccountName;
    FString SessionToken;   // unary Login 签发的会话令牌, 兼作 Bearer 鉴权与重连凭据
    FString Password;       // 登录密码 (仅会话内存, 重连时复用)
    uint64 NextSequence = 1;
    uint64 PlayerId = 0;    // 本地玩家服务器 ID (登录响应携带)

    // 服务器权威出生点 (Phase 1)
    FVector SpawnLocation = FVector::ZeroVector;
    float SpawnYaw = 0.0f;

    // 服务器权威存档快照 (登录响应携带, B 方案): 拥有角色 + 配队 + 上场 index
    bool bHasServerArchive = false;
    TArray<FCharacterSaveData> ServerOwnedCharacters;
    TArray<FGameplayTag> ServerTeamTags;
    int32 ServerActiveCharacterIndex = -1;

    // 战斗 (Phase 3): enemy_id -> 敌人实体, 供 DamageDeal 定位血条/死亡表现
    TMap<uint64, TWeakObjectPtr<AEnemyCharacter>> EnemyRegistry;

    bool bLoginPending = false;
    bool bLoggedIn = false;
    bool bReconnecting = false;
    double ReconnectDeadlineMs = 0.0;

    FTimerHandle HeartbeatTimerHandle;
    FTimerHandle LoginTimeoutHandle;
    FTimerHandle ReconnectTimerHandle;

    static constexpr float HeartbeatIntervalSec = 10.f;   // 服务器超时 30s，10s 心跳留足余量
    static constexpr float LoginTimeoutSec = 15.f;
    static constexpr float ReconnectIntervalSec = 2.f;    // 重连尝试间隔
    static constexpr float ReconnectGraceSec = 10.f;      // 服务器 heartbeat_timeout 的重连宽限窗口
};
