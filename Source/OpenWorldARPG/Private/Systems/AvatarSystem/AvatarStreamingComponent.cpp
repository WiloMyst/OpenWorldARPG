// Copyright 2025 WiloMyst. All Rights Reserved.

#include "Systems/AvatarSystem/AvatarStreamingComponent.h"
#include "Systems/AvatarSystem/AvatarSynthComponent.h"
#include "Systems/GameServer/GameServerSubsystem.h"
#include "TurboLinkGrpcUtilities.h"
#include "TurboLinkGrpcManager.h"

#include "Engine/GameInstance.h"

// ====================================================================
// ARKit 52 维标准面部混合形状 (BlendShape) 命名映射字典
// ====================================================================
const FName UAvatarStreamingComponent::ARKitBlendShapeNames[52] = {
    TEXT("EyeBlinkLeft"), TEXT("EyeLookDownLeft"), TEXT("EyeLookInLeft"), TEXT("EyeLookOutLeft"), TEXT("EyeLookUpLeft"),
    TEXT("EyeSquintLeft"), TEXT("EyeWideLeft"), TEXT("EyeBlinkRight"), TEXT("EyeLookDownRight"), TEXT("EyeLookInRight"),
    TEXT("EyeLookOutRight"), TEXT("EyeLookUpRight"), TEXT("EyeSquintRight"), TEXT("EyeWideRight"), TEXT("JawForward"),
    TEXT("JawLeft"), TEXT("JawRight"), TEXT("JawOpen"), TEXT("MouthClose"), TEXT("MouthFunnel"),
    TEXT("MouthPucker"), TEXT("MouthLeft"), TEXT("MouthRight"), TEXT("MouthSmileLeft"), TEXT("MouthSmileRight"),
    TEXT("MouthFrownLeft"), TEXT("MouthFrownRight"), TEXT("MouthDimpleLeft"), TEXT("MouthDimpleRight"), TEXT("MouthStretchLeft"),
    TEXT("MouthStretchRight"), TEXT("MouthRollLower"), TEXT("MouthRollUpper"), TEXT("MouthShrugLower"), TEXT("MouthShrugUpper"),
    TEXT("MouthPressLeft"), TEXT("MouthPressRight"), TEXT("MouthLowerDownLeft"), TEXT("MouthLowerDownRight"), TEXT("MouthUpperUpLeft"),
    TEXT("MouthUpperUpRight"), TEXT("BrowDownLeft"), TEXT("BrowDownRight"), TEXT("BrowInnerUp"), TEXT("BrowOuterUpLeft"),
    TEXT("BrowOuterUpRight"), TEXT("CheekPuff"), TEXT("CheekSquintLeft"), TEXT("CheekSquintRight"), TEXT("NoseSneerLeft"),
    TEXT("NoseSneerRight"), TEXT("TongueOut")
};

UAvatarStreamingComponent::UAvatarStreamingComponent()
{
    // 启用组件 Tick，用于驱动面部动画与状态机检测
    PrimaryComponentTick.bCanEverTick = true;
    AvatarClient = nullptr;

    // 核心时钟与渲染参数初始化
    AnimationFPS = 30.0f;
    CurrentAudioTime = 0.0f;
    BufferBaseAudioTime = 0.0f;
    bIsNetworkStreamEnded = false;
    CurrentBlendShapes.Init(0.0f, 52);
}

void UAvatarStreamingComponent::BeginPlay()
{
    Super::BeginPlay();

    // ====================================================================
    // 1. 挂载现代音频合成组件 (USynthComponent)
    // ====================================================================
    AActor* OwnerActor = GetOwner();
    if (OwnerActor)
    {
        SynthPlayer = NewObject<UAvatarSynthComponent>(OwnerActor);
        SynthPlayer->SetupAttachment(OwnerActor->GetRootComponent());
        SynthPlayer->RegisterComponent();

        // 启动合成器。底层音频渲染线程开始轮询，无数据时默认输出静音
        SynthPlayer->Start();
    }

    // ====================================================================
    // 2. 构建 gRPC 双向流网络链路
    // ====================================================================
    UTurboLinkGrpcManager* GrpcManager = UTurboLinkGrpcUtilities::GetTurboLinkGrpcManager(this);
    if (!GrpcManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] 核心故障：TurboLinkGrpcManager 实例化失败。"));
        return;
    }

    UAvatarService* AvatarService = Cast<UAvatarService>(GrpcManager->MakeService("AvatarService"));
    if (!AvatarService)
    {
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] 核心故障：AvatarService 实例化失败。"));
        return;
    }

    AvatarService->Connect();
    AvatarClient = AvatarService->MakeClient();

    if (!AvatarClient)
    {
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] 核心故障：AvatarClient 创建失败。"));
        return;
    }

    // 绑定异步网络 I/O 回调委托
    AvatarClient->OnChatWithAvatarResponse.AddDynamic(this, &UAvatarStreamingComponent::OnChatResponseReceived);
    AvatarClient->OnChatWithAvatarWriteComplete.AddDynamic(this, &UAvatarStreamingComponent::OnChatWriteComplete);

    // 初始化 gRPC Session
    CurrentSessionHandle = AvatarClient->InitChatWithAvatar();
    UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] gRPC 双向流通道已就绪，进入监听状态。"));

    // 绑定 GameServer 信令面回调: 对话票据签发结果
    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UGameServerSubsystem* GameServer = GI->GetSubsystem<UGameServerSubsystem>())
        {
            GameServer->OnDialogueAuthResult.AddDynamic(this, &UAvatarStreamingComponent::OnDialogueAuthResult);
        }
    }
}

void UAvatarStreamingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UGameServerSubsystem* GameServer = GI->GetSubsystem<UGameServerSubsystem>())
        {
            GameServer->OnDialogueAuthResult.RemoveDynamic(this, &UAvatarStreamingComponent::OnDialogueAuthResult);
        }
    }

    if (AvatarClient)
    {
        // 解绑动态委托，防御生命周期结束后的悬垂指针 (Dangling Pointers)
        AvatarClient->OnChatWithAvatarResponse.RemoveDynamic(this, &UAvatarStreamingComponent::OnChatResponseReceived);
        AvatarClient->OnChatWithAvatarWriteComplete.RemoveDynamic(this, &UAvatarStreamingComponent::OnChatWriteComplete);

        AvatarClient->TryCancel(CurrentSessionHandle);
        AvatarClient = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UAvatarStreamingComponent::SendChatText(const FString& InText)
{
    if (!AvatarClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AvatarStreaming] 无法发送指令：gRPC 客户端尚未就绪。"));
        return;
    }

    // 发起新会话前，强制执行本地缓冲清理与网络流重置
    InterruptAndFlush();

    UGameServerSubsystem* GameServer = nullptr;
    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        GameServer = GI->GetSubsystem<UGameServerSubsystem>();
    }

    // 未登录 GameServer (离线调试): 无票据直发，由 VHServer 决定放行与否
    if (!GameServer || !GameServer->IsLoggedIn())
    {
        SendChatRequest(InText, TEXT(""));
        return;
    }

    // 缓存票据未过期: 直接携带发送（避开服务器 1 秒频控）
    if (HasValidDialogueToken())
    {
        SendChatRequest(InText, CachedDialogueToken);
        return;
    }

    // 信令面向 GameServer 申请新票据，到账后在 OnDialogueAuthResult 中补发
    if (GameServer->RequestDialogueAuth(NpcId))
    {
        PendingChatText = InText;
        bWaitingDialogueAuth = true;
        UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] 正在向 GameServer 申请对话票据 [npc_id=%d]..."), NpcId);
        return;
    }

    // 票据申请发送失败（通道异常）: 以无票据模式直发，交由 VHServer 鉴权结果兜底
    SendChatRequest(InText, TEXT(""));
}

void UAvatarStreamingComponent::OnDialogueAuthResult(bool bOk, const FString& DialogueToken, int64 ExpiresAtMs, const FString& Reason)
{
    if (!bWaitingDialogueAuth)
    {
        return;
    }
    bWaitingDialogueAuth = false;

    if (!bOk)
    {
        PendingChatText.Reset();
        const FString ErrString = FString::Printf(TEXT("对话授权失败: %s"), *Reason);
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] %s"), *ErrString);

        OnStreamError.Broadcast(ErrString);
        OnStreamComplete.Broadcast();
        return;
    }

    CachedDialogueToken = DialogueToken;
    TokenExpiresAtMs = ExpiresAtMs;
    UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] 对话票据已获取，携带票据直连 VHServer 数据面。"));

    SendChatRequest(PendingChatText, CachedDialogueToken);
    PendingChatText.Reset();
}

bool UAvatarStreamingComponent::HasValidDialogueToken() const
{
    if (CachedDialogueToken.IsEmpty())
    {
        return false;
    }
    // 预留 2 秒余量，避免票据在发送途中过期
    const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
    return NowMs < TokenExpiresAtMs - 2000;
}

void UAvatarStreamingComponent::SendChatRequest(const FString& InText, const FString& AuthToken)
{
    if (!AvatarClient)
    {
        return;
    }

    FGrpcAvatarAvatarStreamRequest Request;
    Request.SessionId = TEXT("UE5_Session_001");
    Request.StreamType = TEXT("TEXT_INFER");
    Request.TextPayload = InText;
    Request.IsEndOfStream = false;
    Request.AuthToken = AuthToken;

    // 执行异步非阻塞网络写操作
    AvatarClient->ChatWithAvatar(CurrentSessionHandle, Request);
    UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] 上行请求已发出，Payload: %s"), *InText);
}

void UAvatarStreamingComponent::InterruptAndFlush()
{
    // 1. 终止历史网络流：断开旧句柄并重新初始化，丢弃网络层残留的分片
    if (AvatarClient)
    {
        AvatarClient->TryCancel(CurrentSessionHandle);
        CurrentSessionHandle = AvatarClient->InitChatWithAvatar();
    }

    // 2. 音频与时钟重置
    if (SynthPlayer)
    {
        SynthPlayer->ResetAudioState();
    }

    // 3. 无锁队列强制清空：迭代出队以清空缓存
    TArray<float> TempFrame;
    while (BlendShapeQueue.Dequeue(TempFrame)) {}

    // 4. 核心状态机全面复位
    QueuedChunkCounter.Reset();
    CurrentAudioTime = 0.0f;
    BufferBaseAudioTime = 0.0f;
    FrameBuffer.Empty();
    bIsNetworkStreamEnded = false;

    // 5. 面部权重归零：确保动画网络中断后，角色面部恢复至初始状态
    for (int32 i = 0; i < 52; ++i)
    {
        CurrentBlendShapes[i] = 0.0f;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AvatarStreaming] 流中断信号触发。网络通道已重建，所有本地缓存区已清空。"));
}

void UAvatarStreamingComponent::OnChatWriteComplete(FGrpcContextHandle Handle)
{
    if (!UTurboLinkGrpcUtilities::EqualEqual_GrpcContextHandle(Handle, CurrentSessionHandle)) return;
    // 留存接口：可扩展用于上行发送速率控制与背压探测
}

void UAvatarStreamingComponent::OnChatResponseReceived(FGrpcContextHandle Handle, const FGrpcResult& GrpcResult, const FGrpcAvatarAvatarStreamResponse& Response)
{
    // 鉴权校验：抛弃非当前活动 Session 的残影数据
    if (!UTurboLinkGrpcUtilities::EqualEqual_GrpcContextHandle(Handle, CurrentSessionHandle)) return;

    // 底层网络异常（如后端未启动、断网）
    if (GrpcResult.Code != EGrpcResultCode::Ok)
    {
        FString ErrString = FString::Printf(TEXT("网络连接失败: %s"), *GrpcResult.GetMessageString());
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] %s"), *ErrString);

        OnStreamError.Broadcast(ErrString);
        OnStreamComplete.Broadcast();
        return;
    }

    // 云端模型抛出异常
    if (!Response.Success)
    {
        FString ErrString = FString::Printf(TEXT("AI 引擎异常: %s"), *Response.ErrorMsg);
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] %s"), *ErrString);

        OnStreamError.Broadcast(ErrString);
        OnStreamComplete.Broadcast();
        return;
    }

    // ====================================================================
    // 端侧硬熔断机制 (Circuit Breaker)
    // 防止云端高频发包引发 UE5 端侧物理内存耗尽 (OOM)
    // ====================================================================
    if (QueuedChunkCounter.GetValue() > MaxQueueChunks)
    {
        UE_LOG(LogTemp, Error, TEXT("[AvatarStreaming] 熔断触发：端侧缓冲池积压超出阈值。强制截断当前流！"));
        InterruptAndFlush();
        return;
    }

    // ====================================================================
    // 音频与面部数据分发
    // ====================================================================
    if (Response.AudioPcm.Value.Num() > 0 && SynthPlayer)
    {
        // 音频数据直接投递给现代合成器，底层音频线程将自动消费
        SynthPlayer->QueueAudio(Response.AudioPcm.Value);
        QueuedChunkCounter.Increment(); // 记录背压水位
    }

    if (Response.Frames.Num() > 0)
    {
        for (const FGrpcAvatarBlendShapeFrame& Frame : Response.Frames)
        {
            BlendShapeQueue.Enqueue(Frame.Weights);
        }
    }

    // 检测网络流结束标志 (EOF)
    if (Response.IsEndOfStream)
    {
        UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] 接收到网络流 EOF 标志，等待本地动画序列执行完毕..."));
        bIsNetworkStreamEnded = true;
    }
}

void UAvatarStreamingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ====================================================================
    // 1. 音频主时钟采样
    // ====================================================================
    if (SynthPlayer)
    {
        CurrentAudioTime = SynthPlayer->GetCurrentAudioTime();
    }

    // ====================================================================
    // 2. 消费网络帧入缓冲；空→非空跳变时锚定 FrameBuffer[0] 的音频时刻
    // ====================================================================
    const bool bWasEmpty = FrameBuffer.Num() == 0;
    TArray<float> FrameData;
    while (BlendShapeQueue.Dequeue(FrameData))
    {
        QueuedChunkCounter.Decrement(); // 消费成功，释放背压额度
        if (FrameData.Num() == 52)
        {
            FrameBuffer.Add(FrameData);
        }
    }
    if (bWasEmpty && FrameBuffer.Num() > 0)
    {
        BufferBaseAudioTime = CurrentAudioTime;
    }

    // ====================================================================
    // 3. 滑窗式帧驱动：按音频进度消费已播放帧，对齐当前帧并插值
    //    BufferBaseAudioTime 始终等于 FrameBuffer[0] 应播放的音频时刻
    // ====================================================================
    if (FrameBuffer.Num() > 0)
    {
        float ContinuousIdx = (CurrentAudioTime - BufferBaseAudioTime) * AnimationFPS;
        int32 Idx0 = FMath::FloorToInt(ContinuousIdx);

        if (Idx0 >= FrameBuffer.Num())
        {
            // 音频已播过所有已到达帧：清空缓冲并重锚定，等待新帧或进入衰减
            FrameBuffer.Empty();
            BufferBaseAudioTime = CurrentAudioTime;
        }
        else
        {
            // 丢弃已播放过的帧，基准时间同步推进（滑窗前移）
            if (Idx0 > 0)
            {
                FrameBuffer.RemoveAt(0, Idx0);
                BufferBaseAudioTime += static_cast<float>(Idx0) / AnimationFPS;
                ContinuousIdx = (CurrentAudioTime - BufferBaseAudioTime) * AnimationFPS;
                Idx0 = FMath::FloorToInt(ContinuousIdx);
            }
            Idx0 = FMath::Clamp(Idx0, 0, FrameBuffer.Num() - 1);
            const int32 Idx1 = Idx0 + 1;
            const float Alpha = FMath::Clamp(ContinuousIdx - Idx0, 0.0f, 1.0f);

            const TArray<float>& Shapes0 = FrameBuffer[Idx0];
            if (Idx1 < FrameBuffer.Num())
            {
                // 帧间线性插值，将云端 30FPS 平滑补偿至 UE5 本地高渲染帧率
                const TArray<float>& Shapes1 = FrameBuffer[Idx1];
                for (int32 i = 0; i < 52; ++i)
                {
                    CurrentBlendShapes[i] = FMath::Lerp(Shapes0[i], Shapes1[i], Alpha);
                }
            }
            else
            {
                // 抵达缓冲末尾，保持末帧
                CurrentBlendShapes = Shapes0;
            }
        }
    }

    if (FrameBuffer.Num() == 0)
    {
        // 网络饥饿或播放结束：面部权重平滑衰减至静默
        for (int32 i = 0; i < 52; ++i)
        {
            CurrentBlendShapes[i] = FMath::FInterpTo(CurrentBlendShapes[i], 0.0f, DeltaTime, 15.0f);
        }
    }

    // ====================================================================
    // 4. 流生命周期闭环检测：网络下发完毕且本地帧缓冲已耗尽
    // ====================================================================
    if (bIsNetworkStreamEnded && FrameBuffer.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] 对话生命周期完整闭环！执行 UI 恢复广播。"));
        OnStreamComplete.Broadcast();
        bIsNetworkStreamEnded = false;
    }
}

void UAvatarStreamingComponent::BindTargetFaceMesh(USkeletalMeshComponent* InFaceMesh)
{
    if (InFaceMesh)
    {
        TargetFaceMesh = InFaceMesh;
        UE_LOG(LogTemp, Log, TEXT("[AvatarStreaming] 渲染链路建立：目标面部网格体绑定成功。"));
    }
}
