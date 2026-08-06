# Open World ARPG

[![license](https://img.shields.io/badge/license-MIT-blue)](https://github.com/WiloMyst/OpenWorldARPG/blob/master/LICENSE) [![GitHub repo size](https://img.shields.io/github/repo-size/WiloMyst/OpenWorldARPG)](https://github.com/WiloMyst/OpenWorldARPG)

## 概述

基于 UE5.7 C++ 与蓝图混合开发的开放世界 ARPG 原型，参考原神式的多角色配队战斗与开放世界探索玩法。客户端以 GAS 为核心驱动框架，实现了自定义移动组件（攀爬/滑翔/墙角过渡）、多角色零延迟切换、数据驱动的背包装备系统等功能模块。

项目同时包含 **VHServer**——一个基于 C++17 gRPC 异步流式的 AI 虚拟人服务后端，构建了"云端 LLM 流式生成 → 本地 TTS 合成 → Audio2Face 面部动画"的三级推理流水线，与 UE5 客户端通过双向流式 RPC 实现 AI NPC 的语音对话与表情驱动。（已去除第三方资源）

## 项目架构

### 整体架构（前后端分离）

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          UE5 客户端 (Windows)                            │
│                                                                         │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌────────────┐  │
│  │  Core 层     │  │  Systems 层   │  │ Characters 层 │  │   UI 层    │  │
│  │             │  │              │  │              │  │            │  │
│  │ GameMode    │  │ AISystem     │  │ PlayerChar   │  │ HUD        │  │
│  │ GameState   │  │ AbilitySystem│  │ EnemyChar    │  │ Screens    │  │
│  │ PlayerCtrl  │  │ AvatarSystem │  │ NpcChar      │  │ Inventory  │  │
│  │ PlayerState │  │ CombatSystem │  │ AnimInstance │  │ Team       │  │
│  │ GameInstance│  │ VehicleSystem│  │              │  │            │  │
│  │             │  │ Movement     │  │              │  │            │  │
│  │             │  │ Inventory    │  │              │  │            │  │
│  │             │  │ Interaction  │  │              │  │            │  │
│  │             │  │ GameFlow    │  │              │  │            │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └────────────┘  │
│                                                                         │
│  GAS (Gameplay Ability System) 统一驱动                                  │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  GA (GameplayAbility)  ←→  GE (GameplayEffect)  ←→  Tag         │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────┬───────────────────────────────────────┘
                                  │
                                  │ gRPC 双向流式 RPC
                                  │ (AvatarService.ChatWithAvatar)
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    VHServer 后端 (C++17 / Linux)                        │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    gRPC 异步服务端 (端口 50051)                    │   │
│  │         ServerCompletionQueue + AvatarSession 状态机             │   │
│  │         (CONNECT → READ → WRITE → FINISH)                       │   │
│  └──────────────────────────────┬───────────────────────────────────┘   │
│                                 │                                        │
│                                 ▼                                        │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                       AIBrain 三级流水线                          │   │
│  │                                                                  │   │
│  │  ┌─────────────┐  标点  ┌─────────────┐  8820  ┌─────────────┐   │   │
│  │  │  Stage 1    │  断句  │  Stage 2    │  样本  │  Stage 3    │   │   │
│  │  │  LLM 流式   │──────►│  TTS 合成    │───────►│  V2F 推理   │   │   │
│  │  │  ThreadPool │       │  ThreadPool │        │  ThreadPool │   │   │
│  │  │  (1, 1000)  │       │  (1, 1000)  │        │  (1, 1000)  │   │   │
│  │  └──────┬──────┘       └──────┬──────┘        └──────┬──────┘   │   │
│  └─────────┼─────────────────────┼───────────────────────┼──────────┘   │
│            │                     │                       │              │
│            ▼                     ▼                       ▼              │
│  ┌──────────────┐     ┌──────────────────┐     ┌──────────────────┐      │
│  │ CloudLLMEng  │     │  PiperTTSModel   │     │ Audio2FaceModel │      │
│  │              │     │                  │     │                  │      │
│  │ Deepseek API │     │  zh_CN-huayan    │     │  v2f_audio2face  │      │
│  │ (SSE 流式)   │     │  -medium.onnx    │     │  .onnx          │      │
│  │              │     │                  │     │                  │      │
│  │ OpenAI 兼容   │     │  ONNX Runtime    │     │  ONNX Runtime    │      │
│  │ /v1/chat/    │     │  (CPU EP)        │     │  (CUDA EP +      │      │
│  │ completions  │     │                  │     │   cuDNN)        │      │
│  └──────────────┘     └──────────────────┘     └──────────────────┘      │
│                                                                         │
│  ┌──────────────────┐                                                   │
│  │  NLP 音素微服务   │  TCP 127.0.0.1:50052                              │
│  │  (Python)        │  piper_phonemize 中文注音                          │
│  └──────────────────┘                                                   │
└─────────────────────────────────────────────────────────────────────────┘
```

### AI 虚拟人 NPC 数据流

```
UE5 客户端                        VHServer 后端
─────────────                    ──────────────

[玩家输入文本]
     │
     ▼
AvatarStreaming
Component
.SendChatText()
     │
     │ AvatarStreamRequest
     │ (gRPC 双向流)
     ▼
                                 [gRPC Server]
                                 AvatarSession
                                      │
                                      ▼
                                 [AIBrain.InferStream]
                                      │
                    ┌─────────────────┼─────────────────┐
                    │                 │                 │
                    ▼                 ▼                 ▼
              [Stage 1: LLM]    [Stage 2: TTS]    [Stage 3: V2F]
              Deepseek API      Piper TTS          Audio2Face
              (SSE 流式)         (CPU ONNX)         (CUDA ONNX)
                    │                 │                 │
                    │ token           │ sentence        │ PCM 切片
                    ▼                 ▼                 ▼
              [标点断句]         [PCM 22050Hz]    [BlendShape 52D]
                                      │                 │
                                      └────────┬────────┘
                                               │
                                      [ChunkResult]
                                      {audio_pcm,
                                       frames}
                                               │
                                               │ AvatarStreamResponse
                                               │ (gRPC 双向流)
                                               ▼
AvatarStreaming
Component
.OnChatResponseReceived()
     │
     ├─► AvatarSynthComponent     ├─► TickComponent
     │   .QueueAudio()            │   消费 BlendShapeQueue
     │   (无锁 PCMQueue)          │   帧间线性插值
     │                            │   30FPS → 高帧率
     ▼                            ▼
[Audio Render Thread]         [Skeletal Mesh]
OnGenerateAudio()             BlendShape 应用
int16 → float
22050Hz 单声道

[最终效果: AI NPC 实时说话 + 面部表情同步驱动]
```

## 主要实现

### 一、AI 虚拟人 NPC 前后端（项目重点）

#### 1. VHServer：gRPC 异步流式 AI 虚拟人服务后端（C++17 / Linux）

通过 gRPC 双向流式 RPC 与 UE5 客户端联动，实现 AI NPC 的语音对话与表情驱动：

- **三级流水线**：LLM 流式生成 → 标点断句 → Piper TTS 合成 → Audio2Face 面部动画。每级使用单线程 ThreadPool 保证保序，队列容量 1000；token 级流式产出整句送 TTS，PCM 按 8820 样本（约 0.4s）切片送 V2F，每片产出 `{audio_pcm, blendshape_frames}` 下发客户端。
- **gRPC 异步服务端**：基于 `ServerCompletionQueue` 事件循环，`AvatarSession` 状态机（CONNECT→READ→WRITE→FINISH）+ `shared_ptr` 引用计数管理生命周期；写队列通过互斥锁串行化，保证同一时刻仅一个在途 Write。
- **Deepseek API 集成**：`CloudLLMEngine` 通过 libcurl 调用 OpenAI 兼容的 `/v1/chat/completions` 端点，SSE 流式逐行解析 `choices[0].delta.content`，支持中途取消，`std::mutex` 串行化推理调用。
- **ONNX Runtime 推理**：`Audio2FaceModel` 启用 CUDA ExecutionProvider（含 cuDNN），`PiperTTSModel` 走 CPU EP；CUDA 不可用时自动回退 CPU。
- **背压与降级**：队列满时 `enqueue` 返回 `nullopt`，`AIBrain` 记录丢弃数并下发末位错误 chunk；内置埋点统计（总请求数 / 活跃会话 / 各阶段耗时 / 总 chunk 数 / 丢弃数）。
- **NLP 音素微服务**：独立 Python TCP 微服务（端口 50052），基于 `piper_phonemize` 做中文注音 + 音素 ID 映射，作为 LLM→TTS 之间的文本归一化环节，用进程隔离避免把 Python 依赖拉进 C++ 进程。

#### 2. UE5 客户端：流式音频渲染与表情同步

- **AvatarStreamingComponent**：通过 gRPC 双向流连接 VHServer，`SendChatText` 上行文本后监听下行 `AvatarStreamResponse`；内置端侧缓冲水位检测（队列积压超阈值时强制 `InterruptAndFlush()`），防止云端高频发包导致 UE5 OOM。
- **AvatarSynthComponent**（继承 `USynthComponent`）：单声道 22050Hz 对齐 Piper TTS 采样率；`OnGenerateAudio` 在音频渲染线程通过无锁 `PCMQueue` 消费 int16 PCM → float 归一化输出；`TotalSamplesConsumed`（`std::atomic`）作为音频时钟基准。
- **表情同步**：`TickComponent` 消费无锁 `BlendShapeQueue`，按音频进度对齐当前帧并做帧间线性插值，将云端 30FPS 平滑到本地高渲染帧率；网络饥饿时 `FInterpTo` 衰减至 0；流结束且缓冲耗尽时广播 `OnStreamComplete` 通知 UI 恢复。

#### 3. NPC 角色承载

`ANpcCharacter` 挂载 `UAvatarStreamingComponent`（gRPC 流式通道）并实现 `IInteractableInterface`，玩家交互时由 AvatarStreamingComponent 接管，触发 VHServer 推理流水线。NPC 的交互能力完全由 AvatarStreamingComponent 承载，与推理流水线在 C++ 层解耦。

### 二、3C 与战斗（游戏客户端基础功能）

- **GAS 能力驱动**：跳跃/冲刺/攀爬/滑翔/钩索/瞄准等探索行为、近战连招/射击/下落攻击等战斗技能、角色切换均封装为独立 GA，通过 GameplayTag 管理状态互斥与事件流转；近战连招通过 GameplayEvent Tag（伤害判定窗口、连招窗口开闭）驱动 Combo，动画时序与逻辑判定解耦。
- **自定义移动组件**：继承 `UCharacterMovementComponent`，通过 `PhysCustom` 实现攀爬、墙角过渡（阳角二次贝塞尔/阴角 Slerp）、翻越、滑翔等移动模式，"CMC 管物理、ActorComponent 管检测"职责分离。
- **分层动画架构**：父子 AnimInstance 实现逻辑分层；`NativeUpdateAnimation` 主线程快照 ASC Tags，`NativeThreadSafeUpdateAnimation` Worker Thread 只读消费快照，避免多线程安全问题。

### 三、游戏系统模块（游戏客户端基础功能）

- **多角色配队与切换**：`UCharacterDataAsset` 配置静态数据 + `FCharacterSaveData` 存储运行时数据，GA_SwapOut/GA_SwapIn 驱动退场→出场流水线，StandbyMode 通过 NetMulticast 同步。
- **背包与装备系统**：基于 `UGameInstanceSubsystem` 的全局管理器，`FItemInstance` 以 GUID 唯一标识物品实例，支持圣遗物套装效果判定与分类排序筛选。
- **载具系统**：统一继承体系（`AVehiclePawnBase` → 轮式/悬浮/飞行/水上），Chaos 物理载具 + 物理级客户端预测，GAS 驱动上下车（Motion Warping 对齐上车位置）。
- **UI 与资源**：栈式 UI 管理 + Tag 路由解耦；两阶段异步加载（关卡 20% + 队伍角色 80%）+ 分帧释放避免集中 GC 卡顿。
- **敌人 AI**：行为树 + AI 感知，敌人持有独立 ASC，攻击通过 GE 施加、死亡通过 GA 处理。

## 技术栈

**UE5 客户端**：

- Unreal Engine 5.7+ C++ & Blueprint
- Gameplay Ability System (GAS)
- Chaos Physics Vehicle
- Motion Warping

**VHServer 后端**：

- C++17 / Linux
- gRPC + Protobuf（异步流式 RPC）
- ONNX Runtime（CUDA / cuDNN）
- libcurl（LLM API SSE 流式调用）
- nlohmann_json / yaml-cpp / spdlog
- Python（piper_phonemize NLP 微服务）

## 版本

- Unreal Engine 5.7
- Visual Studio 2022, MSVC 14.38+
- CMake 3.10+ 
- GCC 9+ / Clang 10+ (Linux)

## 资源

Plugins：

- [KawaiiPhysics](https://github.com/pafuhana1213/KawaiiPhysics) — 物理骨骼动画
- [UnLua](https://github.com/Tencent/UnLua) — Lua 脚本绑定

Models：

- [模之屋 (PlayBox)](https://www.aplaybox.com/)
- [cats-blender-plugin](https://github.com/absolute-quantum/cats-blender-plugin) — Blender MMD 模型转换

## 目录结构

```
OpenWorldARPG/
├── Source/OpenWorldARPG/      # UE5 C++ 源码
│   └── Private/
│       ├── Characters/        # 角色 (Player/Enemy/NPC)
│       ├── Core/              # GameMode/GameState/PlayerController/PlayerState
│       └── Systems/           # 游戏系统模块
│           ├── AISystem/              # 敌人 AI
│           ├── AbilitySystem/        # GAS (AttributeSet/AbilityTask)
│           ├── AvatarSystem/         # AI 虚拟人客户端 (gRPC 流式)
│           ├── CharacterManager/     # 角色管理 (DataAsset/Registry)
│           ├── CombatSystem/         # 战斗 (GA/Weapon/AnimNotify)
│           ├── GameFlowManager/     # 游戏流程 (Loading/Asset)
│           ├── InteractionSystem/   # 交互系统
│           ├── InventoryManager/    # 背包系统
│           ├── MovementSystem/      # 移动系统 (CMC/GA)
│           ├── PlayerManager/       # 玩家面板
│           ├── TeamManager/         # 队伍系统
│           └── VehicleSystem/       # 载具系统
├── VHServer/                  # AI 虚拟人服务后端 (C++17 gRPC)
│   ├── src/                    # 源码
│   │   ├── business/           # 业务层 (AIBrain/Models)
│   │   ├── core/               # 核心 (gRPC Server/Session)
│   │   └── main.cpp
│   ├── include/                # 头文件
│   ├── protos/                 # protobuf 定义
│   ├── config.yaml            # 配置 (Deepseek API/ONNX/流式参数)
│   ├── nlp_server.py           # NLP 音素微服务
│   └── start_server.sh         # 启动脚本
├── Content/                   # UE5 资源 (仅目录结构)
└── Protos/                    # protobuf 定义 (UE5 客户端侧)
```

## License

[MIT](LICENSE)
