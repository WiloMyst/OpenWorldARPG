# Open World ARPG

[![license](https://img.shields.io/badge/license-MIT-blue)](https://github.com/WiloMyst/OpenWorldARPG/blob/master/LICENSE) [![GitHub repo size](https://img.shields.io/github/repo-size/WiloMyst/OpenWorldARPG)](https://github.com/WiloMyst/OpenWorldARPG)

## 概述

面向ARPG的游戏前后端原型：UE5.7 客户端与两个独立 C++17 后端经 gRPC 互联，从登录鉴权、移动同步、AOI 广播、战斗裁决到背包存档，跑通完整的服务器权威闭环。

- **GameServer**——权威游戏服务器（gRPC 异步 + MySQL + Redis），持有会话、玩家数据与背包的权威状态：低频命令走 unary，实时玩法走 World 双向流，客户端只上报意图，数值全部由服务器裁决下发。
- **VHServer**——AI 虚拟人推理服务（gRPC 流式 + ONNX Runtime），"LLM 流式生成 → TTS 合成 → Audio2Face 面部动画"三级流水线，音频与表情帧下发、UE5 端侧只做渲染与播放；与 GameServer 以信令面（票据授权）+ 数据面（持票直连）+ 控制面（会话终结吊销）协作。

UE5 客户端作为联机对端，为服务器提供真实玩法流量并验证协议闭环。

（已去除第三方资源）

## 演示

[Video01](assets/Video01.mp4) [Video02](assets/Video02.mp4) [Video03](assets/Video03.mp4) [Video04](assets/Video04.mp4) 



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
                                  │ 玩法通道: gRPC (50061)          │
                                  │ unary: 登录/角色/背包/对话授权   │
                                  │ World 双向流: 心跳/移动/AOI 广播 │
                                  │                                │数据面: gRPC 异步双向流 (50051)
                                  │                                 │持票直连 (音频/表情)
                                  │                                 │
                                  ▼                                 │
┌─────────────────────────────────────────────────────────────────┐   │
│                    GameServer 后端 (C++17 / Linux)               │   │
│                                                                 │   │
│  ┌───────────────────────────────────────────────────────┐      │   │
│  │   gRPC 异步服务端 (端口 50061, AsyncService + CQ)      │      │   │
│  │   unary: Login/Logout/角色/背包/对话授权 (线程池执行)   │      │   │
│  │   World 双向流: WorldSession CQ 状态机                 │      │   │
│  │   (写队列单在途写, 踢出/登出经 TryCancel 收尾)          │      │   │
│  │   登录: Login 签发令牌 → World 流首帧兑换在线会话       │      │   │
│  └───────────────────────────┬───────────────────────────┘      │   │
│                              │                                  │   │
│                              ▼                                  │   │
│  ┌───────────────────────────────────────────────────────┐      │   │
│  │   CompletionQueue 事件循环 (单线程派发全部 RPC 事件)     │      │   │
│  │   World 流消息在 CQ 线程直接处理 (轻量内存操作)          │      │   │
│  │   unary 业务经 Worker 线程池 (DB IO 不占事件线程)        │      │   │
│  │   GameLogic 编排 / WorldManager 权威校验 + AOI           │      │   │
│  │   Worker 池有界队列, 满则拒绝 503 背压                    │      │   │
│  │     └─ 权威背包 InventoryManager (items.yaml)            │      │   │
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
│  │ 心跳/登录超时踢出 (锁外执行, Kick 经 World 流主动通知)     │      │   │
│  │ 会话终结 → DialogueRevoker → 控制面吊销 VHServer 对话流  │      │   │
│  └───────────────────────────────────────────────────────┘      │   │
└─────────────────────────────────────────────────────────────────┘   │
                       │ 控制面 (gRPC unary, 50053)                     │
                       ▼                                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    VHServer 后端 (C++17 / Linux)                        │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  控制面 gRPC 服务端 (端口 50053, 仅内网, Bearer 密钥鉴权)             │   │
│  │  AdminService.RevokeDialogue ──► DialogueRegistry 按账号吊销流     │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    gRPC 异步服务端 (端口 50051)                    │   │
│  │         ServerCompletionQueue + AvatarSession 状态机             │   │
│  │         (CONNECT → READ → WRITE → FINISH)                       │   │
│  │         DialogueRegistry: 按账号登记活跃对话流                      │   │
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
│  ┌──────────────────┐                                                   │
│  │  NLP 音素微服务   │  TCP 127.0.0.1:50052                              │
│  │  (Python)        │  piper_phonemize 中文注音                          │
│  └──────────────────┘                                                   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 三端拓扑与对话授权（信令面 + 数据面 + 控制面）

推理媒体流（音频 PCM / 表情帧）不经 GameServer 中转，避免带宽与转发瓶颈；VHServer 也不裸奔——以 GameServer 签发的短期票据做准入控制，票据只在流建立时校验，已建流的终止由 GameServer 经控制面主动下发吊销：

```
UE5 客户端 ───gRPC (50061, unary + World 双向流)───► GameServer
    │  ① Login (unary): 鉴权建档, 签发 session_token + 权威快照
    │  ② World 双向流: 首帧心跳持 authorization 令牌兑换在线会话,
    │     心跳 / 移动上报 / AOI 广播 / 战斗结算     (会话权威 + 世界权威 + MySQL/Redis)
    │  ③ InventoryOp / DialogueAuth (unary): 背包操作 / 对话授权 (Bearer 令牌鉴权)
    │         ◄── DialogueAuthResult ◄── 校验在线状态 + 每账号频控,
    │             account.npc_id.expires_at.hmac  HMAC-SHA256 签发, 默认 30s 有效
    │
    │  ④ 持票直连 (AvatarStreamRequest.auth_token)
    └────────────数据面 (gRPC 双向流, 50051)──────► VHServer
                                                    本地验签 (常数时间比较) + 过期检查,
                                                    不回调 GameServer, 零额外往返

GameServer ───控制面 (gRPC unary, 50053, Bearer 密钥)───► VHServer AdminService
                 ⑤ 会话终结 (登出/踢出/断线/心跳超时) → RevokeDialogue(account):
                    DialogueRegistry 吊销该账号已建立对话流 (尽力而为, 票据 TTL 兜底)
```

票据缓存至过期前 2 秒复用；密钥 `dialogue_secret` 双端共享、环境变量 `DIALOGUE_SECRET` 优先。跨服闭环由 `GameServer/tests/token_probe.cpp`（取票）+ `VHServer/tests/test_cross_server.py`（验票）覆盖。

## 主要实现

### 一、权威游戏服务器（GameServer 后端）

单进程 C++17 权威服务器：客户端全部流量经 gRPC（低频命令走 unary，实时玩法走 World 双向流），MySQL 持久化权威状态，Redis 做读缓存与频控。玩家可见的一切数值由服务器计算下发，客户端只上报意图。

#### 1. 网络与并发模型：从同步到异步的一次重构

项目早期用同步 Service——每个 World 流占一个线程跑阻塞读循环，并发上限直接等于线程数，一百个在线玩家就是一百个常驻线程。后来全量重写为 AsyncService + CompletionQueue 状态机（构建环境 gRPC 1.30 无 Callback API），这也是当前实现：

- 全部 RPC 由单线程 CQ 事件循环驱动。World 流是事件状态机（CONNECT → READ/WRITE → FINISH），新流到达即自续 accept，读写事件原地处理——心跳、移动、战斗都是内存级操作，单线程派发天然保序，避免了多线程广播的锁竞争。
- 写路径是"队列 + 单在途写"。任意线程（CQ 线程、会话扫描线程）都可向会话投递广播，会话内部互斥锁保证同一时刻只有一个 Write 在途，写完成事件再取下一条，乱序与撕裂被结构排除。
- 会话生命周期挂在事件 tag 上：每个在途事件持会话强引用，流终结后引用归零自动析构。SessionManager 只持 weak_ptr，不存在悬垂引用。
- unary 不占 CQ 线程：请求到达即自续 accept、业务投 Worker 线程池执行（登录一次有五六次 MySQL 往返，不能阻塞事件循环），池满直接回 503 做背压。

#### 2. 服务器权威

客户端上报的一切数值都不信任，只接受意图：

- 移动校验三道关：速度上限（按上报间隔估算瞬时速度，容差容忍网络抖动）、单次位移硬上限（防瞬移）、时间戳单调性与服务器时钟偏差校验。任一不过即下发权威位置纠正，客户端拉回。
- 伤害裁决：客户端只报"打了谁 + 什么技能"，数值由服务器按 enemies.yaml / skills.yaml 属性表计算，结果广播给视野内玩家；攻击者身份由会话归属解析，不信任客户端自报的 player_id。
- 玩家 HP 只由服务器驱动：客户端不写自己的 HP，全部经服务器 PlayerDamage 下发覆盖，杜绝本地改值。

#### 3. AOI：九宫格

地图按 20m 等分格子，视野为所在格周围 3×3（60m × 60m）。两张哈希表（实体→格子、格子→实体集）维护归属，玩家移动切格时对新旧视野求差集，产生 Enter / Leave / Stay 三类事件双向通知——谁移动谁负责给自己和对方发消息，站着不动的一方也会收到对方的离开消息。

选九宫格的理由：ARPG 单服玩家密度均匀、视野统一，网格法判定开销与全图人数无关、实现能完整闭环；分布不均或视野差异大的场景才轮到十字链表或四叉树。

#### 4. 会话生命周期

- 登录与会话建立分离：unary Login 鉴权建档、签发 Bearer 令牌并返回权威快照；客户端随后开 World 流，首帧心跳携带令牌兑换在线会话。之后所有 unary 以同一令牌鉴权，令牌与账号绑定，会话终结即失效。
- 心跳是应用层的，与传输层 keepalive 分工明确：HTTP/2 keepalive 只保连接不死，应用层心跳（10s 发、30s 超时踢出）保会话不死。且任何上行消息都刷新活跃度——心跳真正的角色是静止玩家的保活手段。
- 同账号重复登录踢旧、登录超时清理、心跳超时踢出，由后台扫描线程锁外执行，踢出经 World 流主动通知客户端。
- 断线重连做了减法：gRPC 流不可恢复，与其做 resume_token + 重连等待状态机，不如客户端 2s 间隔重走登录流，服务端靠"重复登录踢旧会话"兜底。会话状态机因此没有 RECONNECTING 分支，复杂度大幅下降。

#### 5. 持久化

MySQL 写路径全部异步：独立写线程消费 FIFO 有界队列，位置和 HP 按节流间隔入队，移动模拟不等落库。登录与登出设屏障——登录读档前先排空该账号在途写（保证 read-your-writes，不会读到旧位置），登出立即存最终位置。背包操作走事务，落库失败从 DB 重载内存回滚，内存与磁盘不脱节。

表设计上，`players` 用自增代理主键 + `uk_account` 业务唯一键（避免字符串主键的二级索引膨胀与随机插入页分裂）；`inventory_items` 以 `uk_guid` 保证实例唯一，`(account, acquired_time)` 复合索引覆盖"按账号加载背包"主查询路径，行内仅存动态实例数据（数量/获取时间），静态规则（堆叠上限/分类）经 item_id 查 items.yaml，不落库。

#### 6. 三平面分离：信令 / 数据 / 控制

推理媒体流（音频 PCM、表情帧）不经 GameServer 中转，对话授权拆成三段：

- 信令面（GameServer）：对话授权时校验在线状态与每账号频控，签发 HMAC-SHA256 短期票据，默认 30s 有效。
- 数据面（客户端 → VHServer 直连）：凭票建流，VHServer 本地验签（常数时间比较）后不再回调 GameServer，零额外往返。
- 控制面（GameServer → VHServer）：票据只在建流时校验，已建流的终止由控制面负责——玩家会话终结（登出/踢出/断线/超时）时调用 RevokeDialogue 吊销其对话流，封堵"被踢玩家凭未过期票据继续推理"的残留窗口。吊销尽力而为（独立线程、1.5s 超时即弃、不重试），票据 TTL 兜底。

#### 7. Redis 热点层

- 玩家数据 Cache-Aside：`player:{account}` 读缓存（TTL 300s），未命中回源 MySQL 回填，写库成功后删缓存。
- 对话票据签发用 `dlg:rate:{account}` 固定窗口频控（`SET NX EX` + `INCR`），多实例部署下频控口径一致。
- Redis 是可选依赖：连接失败自动降级为进程内频控 + 直连 MySQL，命令失败标记不可用、恢复后懒重连回切。Redis 宕机只降性能，不停服。

#### 8. 设计取舍

| 决策点 | 选择 | 理由 |
|---|---|---|
| 自研 TCP vs gRPC | gRPC | 序列化、流控、keepalive、跨语言生态免费获得；代价是流不可恢复，用重走登录流补 |
| 断线恢复 | 重走登录流 | 复杂度远低于跨连接会话迁移；2s 重登延迟在 ARPG 可接受 |
| AOI 视野形状 | 方形（3×3 格） | 判定零距离计算；比内切圆多覆盖约 27%，需要精确时加二次距离过滤即可 |
| 对话流吊销 | 尽力而为 + TTL 兜底 | 控制面故障不能阻塞会话关闭路径；残留窗口上限即票据 TTL |
| Redis 可用性 | 可选依赖降级 | 频控回退进程内存、缓存直连 MySQL；宕机只降性能不停服 |

### 二、AI 虚拟人服务（VHServer + UE5 客户端）

#### 1. VHServer 后端（C++17 / Linux）

gRPC 双向流式服务。一次对话的链路是"LLM 流式生成 → 标点断句 → Piper TTS 合成 → Audio2Face 推理"，产出音频 PCM 切片与表情帧序列下发客户端。几个关键设计：

- 三级流水线每级单线程 ThreadPool：保序是硬需求（音频帧不能乱序），单线程消费换天然顺序；队列容量 1000，满则丢弃、计数并在流末下发错误标记。
- 音画同步是核心难点：TTS 输出 22050Hz、Audio2Face 输入 16kHz，流水线内置线性重采样适配；自训练 V2F 模型按固定样本数切帧，重采样后帧数按比例缩减会破坏时长对齐，因此推理后按"音频时长 × 30FPS"补齐或截断表情帧，保证表情序列与音频严格等长。
- gRPC 服务端与 GameServer 同构：CompletionQueue 事件循环 + AvatarSession 状态机，写队列单在途写。
- LLM 走 Deepseek 的 OpenAI 兼容接口，libcurl SSE 逐行解析流式 token，支持中途取消。
- 中文注音独立成 Python 微服务（piper_phonemize），进程隔离，避免把 Python 依赖拉进 C++ 构建链。

#### 2. UE5 客户端

- AvatarStreamingComponent 维护 gRPC 流，内置端侧熔断：音频队列积压超阈值即强制清空截断流，作为回复长度失控的最后防线。
- 音频由 USynthComponent 子类在渲染线程消费无锁 PCM 队列，消费计数即音频时钟；表情按音频进度对齐取帧并做帧间插值，云端 30FPS 平滑到端侧渲染帧率。开播前设 0.5s 起播水位，用少量首响延迟换开场阶段抗抖动。
- NPC 角色挂载流式组件并实现交互接口，玩家输入文本即触发 VHServer 推理链路。

<img src="assets\Image01.jpg" alt="Image01" style="zoom:100%;" />

### 三、客户端玩法（UE5.7）

- GAS 驱动全部玩法行为：探索（跳跃/冲刺/攀爬/滑翔/钩索/瞄准）、战斗（近战连招/射击/下落攻击）、角色切换都是独立 GA，GameplayTag 管理状态互斥；近战的伤害判定窗口由 AnimNotify 在动画关键帧发 GameplayEvent 触发，动画时序与逻辑解耦。
- 自定义移动组件派生自 CMC：PhysCustom 实现攀爬、墙角过渡（阳角二次贝塞尔/阴角 Slerp）、翻越、滑翔等模式；CMC 管物理模拟与射线检测，GAS 只管状态切换，职责边界清晰。
- 分层动画：主线程快照速度/ASC Tags 等 UObject 数据，Worker 线程只读快照做纯数学运算，规避工作线程访问 UObject 的线程安全问题。
- 多角色管理：`UCharacterVisualDataAsset` 等 PrimaryDataAsset 配置静态数据，运行时动态数据独立存储；GA 驱动退场→出场切换流水线，StandbyMode 经 NetMulticast 同步。
- 背包 UI 用 MVVM：InventoryManagerSubsystem 是 Model，ViewModel 做排序/筛选并以委托广播驱动 View；View 层数据载体走对象池，避免频繁 NewObject 的 GC 压力。
- 载具基于 Chaos Vehicles，发动机/变速箱/悬挂/轮胎参数全走 DataAsset 注入；上车三阶段——NavMesh 寻路到车门、Motion Warping 跟随载具 Mesh、RPC 交接控制权。
- UI 栈式管理 + GameplayTag 路由解耦；两阶段异步关卡加载（关卡 + 队伍角色资源），加载完成后分帧释放 StreamableHandle 避免集中 GC。
- 敌人 AI 行为树 + AI Perception 驱动巡逻/追踪/攻击，敌人持独立 ASC，攻击与死亡同样走 GA。

<img src="assets\Image02.jpg" alt="Image02" style="zoom:100%;" />

## 技术栈

- **UE5 客户端**：Unreal Engine 5.7 C++ / GAS / Chaos Vehicles / Motion Warping
- **GameServer**：C++17 / gRPC 异步流式 / MySQL / Redis / spdlog / yaml-cpp
- **VHServer**：C++17 / gRPC 异步流式 / ONNX Runtime (CUDA) / libcurl / spdlog

## 版本

- Unreal Engine 5.7
- Visual Studio 2022, MSVC 14.38+
- CMake 3.10+
- GCC 9+ / Clang 10+ (Linux)

## 资源

Plugins：

- [TurboLink](https://github.com/thejinchao/turbolink) — gRPC 集成（C++ / Blueprint，UE5 客户端 ↔ GameServer / VHServer 通信基础）
- [KawaiiPhysics](https://github.com/pafuhana1213/KawaiiPhysics) — 物理骨骼动画
- [UnLua](https://github.com/Tencent/UnLua) — Lua 脚本绑定

Models：

- [模之屋 (PlayBox)](https://www.aplaybox.com/)
- [cats-blender-plugin](https://github.com/absolute-quantum/cats-blender-plugin) — Blender MMD 模型转换

## 目录结构

```
OpenWorldARPG/
├── Source/OpenWorldARPG/      # UE5 C++ 源码 (Core/Systems/Characters/UI)
├── VHServer/                  # AI 虚拟人服务后端 (C++17 gRPC + ONNX)
│   ├── src / include          # business (AIBrain/Models) + core (Server/Session)
│   ├── protos / config.yaml
│   └── nlp_server.py          # NLP 音素微服务
├── GameServer/                # 权威游戏服务器 (C++17 gRPC + MySQL + Redis)
│   ├── src / include          # net/logic/session/storage/world/combat
│   ├── protos                 # game.proto / admin.proto
│   ├── config/                # 运行时配置 (端口/鉴权/MySQL/Redis)
│   ├── data/                  # 静态数据表 (items/enemies/skills/initial_archive, 客户端 DataTable 镜像)
│   └── tests / scripts
├── Content/                   # UE5 资源 (仅目录结构)
└── Protos/                    # protobuf 定义 (UE5 客户端侧)
```

## 启动

两后端为独立 C++17 进程，在 Linux（或 WSL2）下运行。以下命令在项目根 `OpenWorldARPG/` 下执行。

前置依赖：gRPC、protobuf、spdlog、yaml-cpp、nlohmann_json；GameServer 另需 MySQL 客户端、Hiredis；VHServer 另需 libcurl、Python3 + venv（ONNX Runtime 已随 `third_party/` 提供）。

### GameServer（游戏服，端口 50061）

前置：MySQL（含 `game_server` 库与表）与 Redis。连接信息在 `GameServer/config/config.yaml`，按部署环境修改。

```bash
cd GameServer/build
cmake .. && make -j$(nproc)
./game_server        # 配置相对 build: ../config/config.yaml, 数据表 ../data/
```

### VHServer（虚拟人服，数据面 50051 / 控制面 50053 / NLP 50052）

首次需要建 Python 虚拟环境并安装 NLP 音素依赖：

```bash
cd VHServer
python3 -m venv .venv
source .venv/bin/activate
pip install piper-phonemize
```

启动分两个进程，NLP 微服务必须先于 C++ 引擎：

```bash
# 终端 1: NLP 音素微服务 (127.0.0.1:50052)
cd VHServer && source .venv/bin/activate
python -u nlp_server.py    # -u 关闭输出缓冲, 日志实时落盘

# 终端 2: C++ 引擎
cd VHServer/build
cmake .. && make -j$(nproc)
./vh_server
```

也可用 `VHServer/start_server.sh` 一键启动 C++ 引擎：它会自动激活 `.venv`、挂载 `third_party/onnxruntime` 动态库、缺失构建时自动编译。LLM 需在 `config.yaml` 填入 Deepseek API Key（当前占位 `sk-xxxx`）。

### 启动顺序

先启动 VHServer 再启动 GameServer：GameServer 启动后会用控制面（`vhserver.admin_endpoint`，默认 `127.0.0.1:50053`）连接 VHServer 执行对话流吊销；反向则 `DialogueRevoker` 会在 VHServer 就绪后按需重连，两端口间无硬依赖。

## License

[MIT](LICENSE)
