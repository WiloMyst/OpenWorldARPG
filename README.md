# Open World ARPG

[![license](https://img.shields.io/badge/license-MIT-blue)](https://github.com/WiloMyst/OpenWorldARPG/blob/master/LICENSE) [![GitHub repo size](https://img.shields.io/github/repo-size/WiloMyst/OpenWorldARPG)](https://github.com/WiloMyst/OpenWorldARPG)

## 概述

基于 UE5.7 C++ 与蓝图混合开发的开放世界 ARPG 原型，参考当前热门游戏的多角色配队战斗与开放世界探索玩法。客户端以 GAS 为核心驱动框架，实现了自定义移动组件（攀爬/滑翔/墙角过渡）、多角色零延迟切换、数据驱动的背包装备系统等功能模块。

项目同时包含 **GameServer**——独立权威游戏服务器（C++17 / gRPC 异步双向流 + MySQL + Redis），持有会话、玩家数据与背包的权威状态，并承担对话授权信令面：为通过资格校验的在线玩家签发 HMAC-SHA256 短期票据，虚拟人推理流凭票直连 VHServer。

以及 **VHServer**——一个基于 C++17 gRPC 异步流式的 AI 虚拟人云端服务后端，构建了"云端 LLM 流式生成 → TTS 合成 → Audio2Face 面部动画"的三级推理流水线，所有重推理在云端完成后通过网络下发音频 PCM 与表情帧序列，UE5 端侧只负责渲染与播放。

（已去除第三方资源）

## 项目架构

### 整体架构（三端：客户端 / 游戏服 / 虚拟人服）

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
└──────────────────────────────────┬──────────────────────────────┬─────┘
                                  │                                │
                                  │ 信令面: GameChannel (50061)      │
                                  │ 登录 / 心跳 / 背包 / 对话授权       │
                                  │                                  │数据面: AvatarStream (50051)
                                  │                                   │持票直连 (音频/表情)
                                  │                                   │
                                  ▼                                   │
┌─────────────────────────────────────────────────────────────────┐   │
│                    GameServer 后端 (C++17 / Linux)               │   │
│                                                                 │   │
│  ┌───────────────────────────────────────────────────────┐      │   │
│  │              gRPC 异步服务端 (端口 50061)              │      │   │
│  │      ServerCompletionQueue + PlayerSession 状态机      │      │   │
│  │      (CONNECT → READ → WRITE → FINISH)                 │      │   │
│  │      (ACCEPTING → WAIT_LOGIN → ONLINE)                 │      │   │
│  └───────────────────────────┬───────────────────────────┘      │   │
│                              │                                  │   │
│                              ▼                                  │   │
│  ┌───────────────────────────────────────────────────────┐      │   │
│  │      Worker 线程池 (有界队列, 满则拒绝 503 背压)        │      │   │
│  │    GameLogic 消息分发                                    │      │   │
│  │    Login / Heartbeat / InventoryOp / DialogueAuth       │      │   │
│  │      └─ 权威背包 InventoryManager (items.yaml)          │      │   │
│  └───────────────────────────┬───────────────────────────┘      │   │
│                              │                                  │   │
│               ┌──────────────┴────────────────┐                 │   │
│               ▼                               ▼                 │   │
│  ┌──────────────────────────┐  ┌────────────────────────────┐  │   │
│  │ RedisStore (失败降级)    │  │ MysqlStore (预处理+事务)   │  │   │
│  │ player:{account} 读缓存  │  │ players: 代理主键          │  │   │
│  │ dlg:rate:{account} 频控  │  │ inventory_items uk_guid    │  │   │
│  │ session/online TTL 在线  │  │ (account,acquired_time)    │  │   │
│  └──────────────────────────┘  └────────────────────────────┘  │   │
│                                                                 │   │
│  ┌───────────────────────────────────────────────────────┐      │   │
│  │ SessionManager (后台扫描): 账号→会话绑定, 重复登录踢旧;    │      │   │
│  │ 心跳/登录超时踢出 (锁外执行, KickNotify 主动通知)          │      │   │
│  └───────────────────────────────────────────────────────┘      │   │
└─────────────────────────────────────────────────────────────────┘   │
                                                                      │
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
│  │ CloudLLMEng  │     │  PiperTTSModel   │     │  Audio2FaceModel │      │
│  │              │     │                  │     │                  │      │
│  │ Deepseek API │     │  zh_CN-huayan    │     │ NVIDIA Audio2Face│      │
│  │ (SSE 流式)   │     │  -medium.onnx    │     │ (16kHz PCM 输入  │      │
│  │              │     │                  │     │  ARKit 52D 输出) │      │
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

### 三端拓扑与对话授权（信令面 + 数据面）

推理媒体流（音频 PCM / 表情帧）不经 GameServer 中转，避免带宽与转发瓶颈；VHServer 也不裸奔——以 GameServer 签发的短期票据做准入控制：

```
                 信令面 (gRPC GameChannel, 50061)
UE5 客户端 ───────────────────────────────► GameServer
    │  ① 登录 / 心跳 / 背包操作              会话权威 + 背包权威 + MySQL/Redis
    │  ② DialogueAuthRequest(npc_id)
    │         ◄── DialogueAuthResult ◄── 校验在线状态 + 每账号频控,
    │             account.npc_id.expires_at.hmac  HMAC-SHA256 签发, 默认 30s 有效
    │
    │  ③ 持票直连 (AvatarStreamRequest.auth_token)
    └────────────数据面 (gRPC 双向流, 50051)──────► VHServer
                                                    本地验签 (常数时间比较) + 过期检查,
                                                    不回调 GameServer, 零额外往返
```

票据缓存至过期前 2 秒复用；密钥 `dialogue_secret` 双端共享、环境变量 `DIALOGUE_SECRET` 优先。跨服闭环由 `GameServer/tests/token_probe.cpp`（取票）+ `VHServer/tests/test_cross_server.py`（验票）覆盖。

## 主要实现

### 一、AI 虚拟人 NPC 前后端

#### 1. VHServer：gRPC 异步流式 AI 虚拟人服务后端（C++17 / Linux）

通过 gRPC 双向流式 RPC 与 UE5 客户端联动，实现 AI NPC 的语音对话与表情驱动：

- **三级流水线**：LLM 流式生成 → 标点断句 → Piper TTS 合成 → Audio2Face 面部动画。每级使用单线程 ThreadPool 保证保序，队列容量 1000；token 级流式产出整句送 TTS，PCM 按 8820 样本（约 0.4s）切片送 V2F，每片产出 `{audio_pcm, blendshape_frames}` 下发客户端。
- **gRPC 异步服务端**：基于 `ServerCompletionQueue` 事件循环，`AvatarSession` 状态机（CONNECT→READ→WRITE→FINISH）+ `shared_ptr` 引用计数管理生命周期；写队列通过互斥锁串行化，保证同一时刻仅一个在途 Write。
- **Deepseek API 集成**：`CloudLLMEngine` 通过 libcurl 调用 OpenAI 兼容的 `/v1/chat/completions` 端点，SSE 流式逐行解析 `choices[0].delta.content`，支持中途取消，`std::mutex` 串行化推理调用。
- **采样率自动对齐与音画同步保障**：Piper TTS 输出 22050Hz mono PCM，NVIDIA Audio2Face 官方推荐 16kHz 输入；`AIBrain` 在 V2F 推理分支独立调用 `LinearResampler`（线性插值）支持 22050Hz → 16kHz 重采样，音频播放分支保留原始 22050Hz 不变。当前自训练模型硬编码 22050Hz（735 样本/帧），默认配置与 TTS 一致走零开销透传；若切换 NVIDIA 官方 A2F 微服务，仅需将 `v2f_input_sample_rate` 改为 16000 即可自动触发重采样。
- **V2F 帧数补偿**：自训练模型按固定样本数切帧（`avg_pool1d` kernel=735），重采样到 16kHz 会导致输出帧数按比例缩减，破坏音画时长对齐。`AIBrain` 在 V2F 推理后按 `切片时长 × AnimationFPS` 计算预期帧数，不足则复制末帧补齐、超出则截断，保证表情帧序列覆盖时长始终等于音频时长，无论 `v2f_input_sample_rate` 取 22050 还是 16000 都能维持客户端 30FPS 消费下的音画同步。
- **ONNX Runtime 推理**：`Audio2FaceModel` 启用 CUDA ExecutionProvider（含 cuDNN），`PiperTTSModel` 走 CPU EP；CUDA 不可用时自动回退 CPU。
- **背压与降级**：队列满时 `enqueue` 返回 `nullopt`，`AIBrain` 记录丢弃数并下发末位错误 chunk；内置埋点统计（总请求数 / 活跃会话 / 各阶段耗时 / 总 chunk 数 / 丢弃数）。
- **NLP 音素微服务**：独立 Python TCP 微服务（端口 50052），基于 `piper_phonemize` 做中文注音 + 音素 ID 映射，作为 LLM→TTS 之间的文本归一化环节，用进程隔离避免把 Python 依赖拉进 C++ 进程。

#### 2. UE5 客户端：流式音频渲染与表情同步

- **AvatarStreamingComponent**：通过 gRPC 双向流连接 VHServer，`SendChatText` 上行文本后监听下行 `AvatarStreamResponse`；内置端侧熔断（队列积压超过 `MaxQueueChunks` 时强制 `InterruptAndFlush()` 截断流），作为回复长度失控场景的最后防线，避免队列无界积压。
- **AvatarSynthComponent**（继承 `USynthComponent`）：单声道 22050Hz 对齐 Piper TTS 采样率；`OnGenerateAudio` 在音频渲染线程通过无锁 `PCMQueue` 消费 int16 PCM → float 归一化输出；`TotalSamplesConsumed`（`std::atomic`）作为音频时钟基准。对话开头设起播水位（默认 0.5s）：首块到达后先攒够底部缓冲再开播，以少量首响换取开场阶段的抗抖动能力；水位未满期间音频时钟暂停、表情消费同步等待，开播后音画严格对齐。
- **表情同步**：`TickComponent` 消费无锁 `BlendShapeQueue`，按音频进度对齐当前帧并做帧间线性插值，将云端 30FPS 平滑到端侧高渲染帧率；网络饥饿时 `FInterpTo` 衰减至 0；流结束且缓冲耗尽时广播 `OnStreamComplete` 通知 UI 恢复。

#### 3. NPC 角色承载

`ANpcCharacter` 挂载 `UAvatarStreamingComponent`（gRPC 流式通道）并实现 `IInteractableInterface`，玩家输入文本交互时由 AvatarStreamingComponent 接管，触发 VHServer 后端推理流水线。

### 二、权威游戏服务器（GameServer 后端）

#### 1. 会话与消息通道

- **gRPC 异步服务端**：单一双向流 `GameChannel` 承载全部消息，按 oneof 类型路由，sequence/ack 关联请求与响应；`ServerCompletionQueue` 事件循环驱动 `PlayerSession` 状态机（CONNECT→READ/WRITE→FINISH，ACCEPTING→WAIT_LOGIN→ONLINE）。
- **会话管理**：登录鉴权（静态令牌）、心跳超时踢出、登录超时清理、同账号重复登录踢旧连接，踢出经 `KickNotify` 主动通知客户端；`SessionManager` 后台扫描线程在锁外执行踢出。
- **线程模型**：CQ 事件循环（单线程，只驱动状态机不做业务）+ Worker 线程池（消息处理与存储 IO，有界队列满时拒绝并回 503）+ 会话扫描线程，三层职责分离。

#### 2. 权威背包与 MySQL 持久化

- **服务器权威**：背包五类操作 Add / Remove / Equip / Unequip / Use 全部经服务器校验后执行（堆叠合并、分类容量、装备互斥、使用目标校验），客户端仅接收操作后的完整快照，无法伪造数据；`items.yaml` 物品静态配置为客户端 DT_ItemDatabase 的服务器侧镜像。
- **表设计**：`players` 采用自增代理主键（避免字符串主键的二级索引膨胀与随机插入页分裂）+ `uk_account` 业务唯一键 + `created_at` / `last_login_at` 运营字段；`inventory_items` 以 `uk_guid` 保证物品实例唯一，`(account, acquired_time)` 复合索引覆盖"按账号加载背包"主查询路径，`ext_json` 存武器成长数据与圣遗物词条。上线时完成 V1（字符串主键）到 V2 的在线迁移：影子表建好后搬移数据，`RENAME TABLE` 原子切换。
- **持久化策略**：所有带参数 SQL 走预处理语句，背包操作在事务内落库；落库失败时从 DB 重载内存数据回滚本次操作，保证内存与磁盘一致。

#### 3. Redis 热点层与高可用降级

- **玩家数据 Cache-Aside**：`player:{account}` 读缓存（TTL 300s），未命中回源 MySQL 并回填，写库成功后删除缓存，登录路径避免重复查库。
- **分布式频控**：对话票据签发使用 `dlg:rate:{account}` 固定窗口频控（`SET NX EX` + `INCR`），多实例部署下频控口径一致；`session:*` / `online:*` 以 TTL 管理会话与在线状态，心跳续期、超时自动过期。
- **可选依赖降级**：Redis 为可选依赖，连接失败自动降级——频控回退进程内存实现、玩家数据直连 MySQL，不阻断启动；命令失败标记不可用，恢复后懒重连自动回切。
- **对话授权信令面**：为通过资格校验（在线状态 + 频控）的玩家签发 HMAC-SHA256 短期票据，VHServer 本地验签、数据面直连零额外往返（见"三端拓扑与对话授权"）。

### 三、3C 与战斗（游戏客户端基础功能）

- **GAS 能力驱动**：跳跃/冲刺/慢走/攀爬/滑翔/钩索/瞄准等探索行为、近战连招/射击/下落攻击等战斗技能、角色切换均封装为独立 GA（`GA_*Base` 系列），通过 GameplayTag 管理状态互斥与事件流转；近战连招通过 `AnimNotify_SendGameplayEvent` 在动画关键帧触发 GameplayEvent Tag（伤害判定窗口、连招窗口开闭），动画时序与逻辑判定解耦。
- **自定义移动组件**：两层继承结构（`UBaseCharacterMovementComponent` 通用物理 + `UPlayerCharacterMovementComponent` 玩家专属），均派生自 `UCharacterMovementComponent`，通过 `PhysCustom` 实现攀爬、墙角过渡（阳角二次贝塞尔/阴角 Slerp）、翻越、滑翔、游泳等移动模式；CMC 同时负责物理模拟与射线检测，GAS 通过 GA 掌控状态与生命周期。
- **分层动画架构**：AnimGraph 分层设计；`NativeUpdateAnimation` 主线程快照速度/位置/ASC Tags 等 UObject 数据，`NativeThreadSafeUpdateAnimation` Worker Thread 只读消费快照做纯数学运算，避免工作线程访问 UObject 的线程安全问题。

### 四、游戏系统模块（游戏客户端基础功能）

- **多角色数据管理与角色切换**：基于 `UCharacterVisualDataAsset` / `UCharacterCombatDataAsset` 等 PrimaryDataAsset 配置角色静态数据，`FCharacterSaveData` 存储运行时动态数据；`GA_SwapOutBase`/`GA_SwapInBase` 驱动退场→出场流水线，StandbyMode 通过 `NetMulticast` 同步。
- **背包与装备系统**：采用 MVVM 架构——`UInventoryManagerSubsystem`（`ULocalPlayerSubsystem`）作为 Model 层数据源，`UInventoryViewModel` 作为中介层处理排序/筛选并通过委托广播驱动 View 更新，`UItemObject` 包装 `FItemInstance` + 静态数据指针作为 View 层数据载体；ViewModel 内置 `UItemObject` 对象池避免频繁 `NewObject` 造成的 GC 压力。
- **载具系统**：`AWheeledVehiclePawnBase` 基于 Chaos Vehicles，通过 `UWheeledVehicleConfigDataAsset` + `ApplyVehicleConfig` 数据驱动注入发动机/变速箱/悬挂/轮胎参数；`GA_MountVehicleBase` 三阶段上车流程（NavMesh 寻路到车门 → Motion Warping 跟随载具 Mesh + 上车蒙太奇 → `Server_PossessVehicle` 交接控制权）。
- **UI 与资源**：`UUIManagerSubsystem`（`ULocalPlayerSubsystem`）维护栈式 UI + GameplayTag 路由解耦；`UGameAssetManagerSubsystem` 两阶段异步关卡加载（关卡 + 队伍角色资源）通过 `FStreamableManager`，加载完成后分帧释放 StreamableHandle 避免集中 GC 卡顿。
- **敌人 AI**：`AEnemyController` 使用行为树 + `UAIPerceptionComponent` 驱动巡逻/追踪/攻击，敌人持有独立 ASC 与 `UAS_Enemy`，攻击伤害通过 GE 施加、死亡通过 `GA_DieBase` 处理。

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

**GameServer 后端**：

- C++17 / Linux
- gRPC + Protobuf（异步双向流式 RPC）
- MySQL（libmysqlclient，预处理语句 + 事务）
- Redis（hiredis，读缓存 / 分布式频控 / 在线状态）
- nlohmann_json / yaml-cpp / spdlog

## 版本

- Unreal Engine 5.7
- Visual Studio 2022, MSVC 14.38+
- CMake 3.10+ 
- GCC 9+ / Clang 10+ (Linux)

## 资源

Plugins：

- [TurboLink](https://github.com/thejinchao/turbolink) — gRPC 集成（C++ / Blueprint，UE5 客户端 ↔ VHServer 通信基础）
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
├── GameServer/                 # 权威游戏服务器 (C++17 gRPC + MySQL + Redis)
│   ├── src/                    # 源码 (core/logic/session/storage)
│   ├── include/                # 头文件
│   ├── protos/                 # protobuf 定义 (game.proto)
│   ├── items.yaml              # 物品静态配置 (客户端 DT_ItemDatabase 镜像)
│   ├── config.yaml            # 配置 (鉴权/频控/MySQL/Redis)
│   ├── tests/                  # 集成测试 + 跨服票据探针
│   └── scripts/                # 构建/验证/建表脚本
├── Content/                   # UE5 资源 (仅目录结构)
└── Protos/                    # protobuf 定义 (UE5 客户端侧)
```

## License

[MIT](LICENSE)
