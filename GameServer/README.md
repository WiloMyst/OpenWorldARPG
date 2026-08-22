# GameServer

OpenWorldARPG 的独立权威游戏服务器（C++17 / gRPC 异步双向流 + MySQL + Redis）。UE 客户端通过 gRPC 接入，服务器持有会话、玩家数据与背包的权威状态；MySQL 持久化玩家与背包数据，Redis 作为可选热点层（读缓存 / 分布式频控 / 在线状态，失败自动降级）。

## 范围

### M1 会话与通道

- 协议：单一双向流 `GameChannel`，消息按 oneof 类型路由，sequence/ack 关联请求与响应
- 会话：登录鉴权（静态令牌）、心跳超时踢出、登录超时清理、同账号重复登录踢旧连接
- 服务器主动消息：KickNotify（duplicate_login / heartbeat_timeout / login_timeout）

### M2 权威背包 + MySQL 持久化

- 背包五类操作 Add / Remove / Equip / Unequip / Use，全部由服务器权威校验后执行，客户端仅收快照
- 物品静态配置 `items.yaml`（堆叠上限、分类容量、使用方式、圣遗物词条），为客户端 DT_ItemDatabase 的服务器侧镜像
- 持久化替换 M1 的 JSON 存档：MySQL 两表（players + inventory_items），预处理语句 + 事务
- 登录时加载背包快照随 LoginResponse 返回；每次操作成功立即落库并回传操作后的完整快照

### M3 UE 客户端接入

- 客户端经 TurboLinkGrpc 插件（game.proto 生成代码位于 `Plugins/TurboLink/Source/TurboLinkGrpc` 的 SGame/ 与 pb/）连接 GameService
- `UGameServerSubsystem`（GameInstance 级）持有唯一 GameChannel：登录握手、10s 心跳、15s 登录超时、服务器消息分发、快照转译推送
- `UInventoryManagerSubsystem` 切换严格权威模式：已登录时所有变更接口只转发 InventoryOp 请求，本地状态唯一来源是 `ApplyServerSnapshot`；未登录（编辑器离线调试）回退本地直改
- 登录门禁：`AMainMenuPlayerController` 在登录按钮与开始界面之间插入服务器登录，失败保留登录界面并回调 `OnLoginResult` 供蓝图展示原因
- 事件：`OnLoginResult` / `OnInventoryOpResult` / `OnKicked` / `OnServerDisconnected`

### M4 对话授权（信令面）

三端采用"信令面授权 + 数据面直连"架构：UE 客户端与 VHServer 之间的虚拟人推理流（音频/表情）不经 GameServer 中转（避免带宽与转发瓶颈），但 VHServer 不再裸奔——先由 GameServer 签发短期票据，客户端持票直连：

- 客户端登录后经 `DialogueAuthRequest(npc_id)` 向 GameServer 申请票据
- GameServer 校验玩家资格（在线状态 + 每账号 1 秒频控）后签发 HMAC-SHA256 票据，默认有效期 30 秒
- 票据格式 `account.npc_id.expires_at.hmac_hex`：绑定账号、目标 NPC 与过期时刻，签名消息为前三段原文
- 密钥 `dialogue_secret` 与 VHServer 共享，环境变量 `DIALOGUE_SECRET` 优先于 config.yaml；未配置时拒绝签发（服务器侧不会出现"可无票直连"的灰区）
- VHServer 收到推理请求后本地验签（常数时间比较防时序攻击）+ 过期检查，不回调 GameServer（数据面零额外往返）
- 客户端缓存票据至过期前 2 秒复用，避免触发频控；`UGameServerSubsystem::RequestDialogueAuth` + `OnDialogueAuthResult` 为信令面接口，`UAvatarStreamingComponent` 在发送推理请求前自动完成取票-携带流程

### M5 存储升级：表结构 V2 + Redis 热点层

- **表结构 V2**：players / inventory_items 换自增代理主键（业务键降为唯一索引），inventory_items 增 `(account, acquired_time)` 复合索引；启动时检测旧表结构自动在线迁移（影子表 + `RENAME TABLE` 原子切换），详见"数据库与缓存"
- **玩家数据 Cache-Aside**：`player:{account}` 读缓存（TTL 300s），未命中回源 MySQL 并回填，写库成功后删除缓存失效
- **分布式频控**：对话票据签发走 `dlg:rate:{account}` 固定窗口计数限流（`SET NX EX` + `INCR`），多实例部署下频控口径一致
- **在线状态 TTL**：`session:{token}` / `online:{account}` 成对注册（TTL = 心跳超时×2），心跳续期、登出成对清理、超时自动过期
- **可选依赖降级**：Redis 连接失败不阻断启动——玩家数据直读 MySQL、频控回退进程内存实现；命令失败标记不可用，恢复后懒重连自动回切

## 架构

```
                  ┌──────────────────────────────────────────────┐
                  │                 game_server                  │
                  │                                              │
ClientMessage ───►│ CQ 事件循环 (单线程)                          │
                  │   └─ PlayerSession 状态机                     │
                  │        CONNECT → READ/WRITE → FINISH         │
                  │        ACCEPTING → WAIT_LOGIN → ONLINE       │
                  │                                              │
                  │ Worker 线程池 (背压: 队列满拒绝 503)            │
                  │   └─ GameLogic 消息分发                       │
                  │        Login / Heartbeat / SaveData          │
                  │        InventoryOp                           │
                  │          └─ InventoryManager 权威背包         │
                  │               └─ ItemDatabase (items.yaml)   │
                  │                                              │
                  │ SessionManager (后台扫描线程)                  │
                  │   ├─ 账号→会话绑定, 重复登录踢旧               │
                  │   └─ 心跳超时 / 登录超时踢出                   │
                  │                                              │
                  │ RedisStore (可选依赖, 失败自动降级)            │
                  │   ├─ player:{account} 读缓存 (Cache-Aside)    │
                  │   ├─ dlg:rate:{account} 固定窗口频控           │
                  │   └─ session/online TTL 在线状态               │
                  │                                              │
                  │ MysqlStore (硬依赖, 预处理语句+事务)            │
                  │   ├─ players 表: 代理主键+uk_account          │
                  │   └─ inventory_items 表: uk_guid+复合索引     │
                  └──────────────────────────────────────────────┘
```

## 线程模型

- **CQ 事件循环（1 线程）**：轮询 CompletionQueue，驱动会话状态机，不做业务
- **Worker 线程池（N 线程）**：消息处理与 MySQL/Redis IO；有界队列满时拒绝并回 503
- **会话扫描线程（1 线程）**：周期检查心跳/登录超时，在锁外执行踢出

## 权威背包规则

规则对齐客户端 InventoryManagerSubsystem，客户端不可伪造：

- **堆叠**：可堆叠物品同 item_id 按 max_stack 合并，溢出开新槽；不可堆叠物品 amount 必须为 1
- **分类容量**（槽位数上限）：weapon 1000 / artifact 1500 / material 9999 / food 2000 / quest 100，满则拒绝
- **装备互斥**：同角色已有武器先卸下再装新武器；圣遗物同部位（flower/plume/sands/goblet/circlet）互斥
- **删除**：已装备物品必须先卸下才能删除
- **使用**：`use_target=none` 拒绝；`select_character` 必须携带目标角色；堆叠物品校验并扣减数量
- **实例生成**：武器发放时初始化成长数据（等级/突破/精炼）；圣遗物按静态配置生成主属性词条，guid 为 UUID v4

## 数据库与缓存

### 表结构（V2：代理主键 + 复合索引）

服务器启动时自动建表/迁移，完整 DDL 参考见 `scripts/schema.sql`：

```sql
CREATE TABLE players (
    player_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
    account       VARCHAR(32)    NOT NULL COMMENT '账号, 业务唯一键',
    level         INT            NOT NULL DEFAULT 1 COMMENT '玩家等级',
    exp           BIGINT         NOT NULL DEFAULT 0 COMMENT '经验值',
    created_at    TIMESTAMP      NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
    last_login_at TIMESTAMP      NULL DEFAULT NULL COMMENT '最近登录时间',
    PRIMARY KEY (player_id),
    UNIQUE KEY uk_account (account)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE inventory_items (
    id                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
    guid                  CHAR(36)        NOT NULL COMMENT '物品实例 GUID, 业务唯一键',
    account               VARCHAR(32)     NOT NULL COMMENT '归属账号',
    item_id               INT             NOT NULL COMMENT '物品模板 ID (items.yaml)',
    count                 INT             NOT NULL DEFAULT 1 COMMENT '堆叠数量',
    equipped_character_id INT             NOT NULL DEFAULT -1 COMMENT '装备角色 ID, -1=未装备',
    acquired_time         BIGINT          NOT NULL COMMENT '获取时间 epoch ms',
    category              VARCHAR(16)     NOT NULL COMMENT '分类: weapon/artifact/other',
    ext_json              JSON            NULL COMMENT '武器成长数据 / 圣遗物词条',
    PRIMARY KEY (id),
    UNIQUE KEY uk_guid (guid),
    KEY idx_account_acquired (account, acquired_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

设计要点：

- **自增代理主键**：账号 / GUID 等业务键降为唯一索引。字符串做主键时所有二级索引叶子节点都要携带完整主键值（索引膨胀），GUID 随机插入还会导致 B+ 树页分裂；自增主键顺序追加、写友好
- **`(account, acquired_time)` 复合索引**：覆盖"按账号加载背包"主查询路径，最左前缀匹配 account，同时满足 acquired_time 排序，避免 filesort
- **V1 → V2 在线迁移**：启动时按主键列检测旧结构（players 以 account 为主键 / inventory_items 以 guid 为主键），自动执行"建影子表 → 搬数据 → `RENAME TABLE` 原子切换 → 删旧表"，原 player_id / guid 完整保留

### Redis Key 布局

| Key | 值 | TTL | 用途 |
|---|---|---|---|
| `player:{account}` | 玩家数据 JSON | 300s | Cache-Aside 读缓存：未命中回源 MySQL 并回填；写库成功后 DEL 失效 |
| `session:{token}` | account | 心跳超时×2 | 会话注册；登出时与 `online:` 成对清理 |
| `online:{account}` | session_token | 心跳超时×2 | 在线状态：心跳续期，超时自动过期 |
| `dlg:rate:{account}` | 窗口计数 | 1s | 对话票据签发固定窗口频控（`SET NX EX` + `INCR`） |

### 持久化与降级策略

- MySQL 为硬依赖：启动时 `Connect()` 失败直接拒绝启动；运行期 `Ping()` 探活并自动重连
- Redis 为可选依赖：连接失败不阻断启动，自动降级（玩家数据直读 MySQL、频控回退进程内存实现）；命令失败标记不可用，恢复后懒重连自动回切
- 所有带参数 SQL 走 prepared statement，无字符串拼接
- 背包每次成功操作在事务内全量重写（DELETE + INSERT）：背包规模小，可靠性优先，规避增量同步的 diff 状态机
- 落库失败时从 DB 重载内存数据回滚本次操作，保证内存与磁盘一致

## 构建与运行（Linux / WSL）

依赖：gRPC、protobuf、spdlog、yaml-cpp、nlohmann-json、libmysqlclient（`pkg-config mysqlclient`）、hiredis（`pkg-config hiredis`），以及 protoc 与 grpc_cpp_plugin。免 root 环境可用 `scripts/setup_hiredis_local.sh` 解包到 `~/hiredis-local`，CMake 自动回退该目录。

```bash
cd GameServer
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 终端1: 启动服务器 (在 build 目录下, 读取 ../config.yaml)
./game_server

# 终端2: 跑集成测试
./test_client

# 或一键: 清测试数据 -> 启动 -> 测试 -> 优雅关闭 (需 mysql 客户端清数据)
bash ../run_tests.sh
```

MySQL 连接配置在 `config.yaml` 的 `mysql` 段，密码优先读环境变量 `GAME_DB_PASSWORD`；Redis 连接配置在 `redis` 段，密码优先读环境变量 `REDIS_PASSWORD`，缓存 TTL（`player_cache_ttl_sec`）也在该段配置。

开发环境备注：在 WSL 构建运行、MySQL/Redis 跑在 Docker Desktop 时，`localhost` 不通，host 需填 Windows 网关 IP（WSL 内 `ip route show default` 查看，如 `172.28.80.1`）；Redis 容器 my-redis-stack 映射宿主机 7000 → 容器 6379。

辅助脚本：`scripts/restart_server.sh`（清理 m2inv 测试数据 → 杀旧进程 → setsid 分离启动新实例，避免 WSL 会话退出连带终止）；`scripts/verify_storage.sh`（表结构迁移 + Redis key 布局验证）；`scripts/verify_degraded.sh`（Redis 失联降级路径验证）；`scripts/schema.sql`（V2 完整 DDL 参考）。

## 客户端配置与联调（M3）

`Config/DefaultGame.ini`：

```ini
[/Script/TurboLinkGrpc.TurboLinkGrpcConfig]
DefaultEndPoint=<WSL_IP>:50051                      ; AvatarService
ServiceEndPoint=(("GameService","<WSL_IP>:50061"))  ; 顶层 TMap 为位置式 (("键","值"))，多条目写同一行

[/Script/OpenWorldARPG.OpenWorldARPGSettings]
GameServerDefaultAccount=DevPlayer
GameServerStaticToken=dev-token-2026                ; 与 config.yaml 的 auth.static_token 一致
```

WSL2 IP 重启后可能漂移，用 `wsl -d Ubuntu -- hostname -I` 确认后同步更新两处端点。

联调步骤：

1. WSL 侧启动服务器（保持该终端存活）：`cd GameServer/build && ./game_server`
2. UE 编辑器打开主菜单关卡，PIE
3. 登录界面点击登录 → 服务器日志出现 `Login ok [account=DevPlayer, ...]` → 客户端流转到开始界面
4. 进入游戏世界拾取物品 → 服务器日志出现 `Inventory op ok`，背包 UI 数据来自服务器快照
5. 停服再拾取 → 操作静默失败（转发被丢弃），验证权威模式

验证要点：登录失败（停服）时界面停留在登录页并提示超时；MySQL 中 `inventory_items` 表随拾取实时增长；重新 PIE 登录后背包从数据库完整恢复。

## 测试场景

| 场景 | 验证点 |
|---|---|
| 登录/心跳/存档/登出 | 鉴权通过、数据加载、序列号回执、登出后服务器关闭流 |
| 重复登录 | 同账号新连接登录成功，旧连接收到 duplicate_login 踢出 |
| 未登录发消息 | 心跳在 WAIT_LOGIN 状态被拒，返回 401 |
| 错误令牌 | 登录被拒 |
| 背包：堆叠与容量 | 未知物品拒绝、同 ID 合并至上限、溢出开新槽、不可堆叠 amount!=1 拒绝、分类容量上限 |
| 背包：装备规则 | 武器/圣遗物可装备、武器互斥顶替、圣遗物同部位互斥、已装备物品不可删除 |
| 背包：使用校验 | 不可使用物品拒绝、需选目标未选拒绝、使用后数量正确扣减 |
| 持久化 | 重新登录后物品数量、武器成长数据、圣遗物词条与装备状态完整恢复 |
| 对话授权 | 未登录申请被 401 拒；登录后签发票据；票据绑定 account/npc_id/expires_at 且签名 64 位十六进制；1 秒内重复申请被频控拒绝 |
| Redis 缓存 | 登录后 `player:{account}` 写入且 TTL 正常倒计时，缓存内容含 player_id/level/exp；二次登录命中缓存不回源 MySQL |
| Redis 降级 | Redis 不可用时登录/背包/对话全部正常（玩家数据直读 MySQL、频控回退内存实现，日志显示 limiter=memory）；Redis 恢复后自动回切缓存与分布式频控 |

跨服闭环（需双端服务器均已启动）：

```bash
# VHServer 侧运行: token_probe 登录取真实票据 -> 送 VHServer 验签
cd ../VHServer && .venv/bin/python tests/test_cross_server.py
```

覆盖两点：GameServer 签发的真实票据被 VHServer 接受并进入推理管线；错误密钥伪造的票据被拒绝（`Invalid token signature`）。`build/token_probe` 亦可单独使用，stdout 即票据原文（联调排障用）。

## 协议摘要

```protobuf
service GameService {
    rpc GameChannel(stream ClientMessage) returns (stream ServerMessage);
}
// ClientMessage:  sequence + oneof{login, heartbeat, save_data, logout, inventory_op, dialogue_auth}
// ServerMessage: ack_sequence + oneof{login, heartbeat, save_data, kick, error, inventory_op, dialogue_auth_result}
// InventoryOpRequest:  oneof{add, remove, equip, unequip, use}
// InventoryOpResponse: success + error_msg + 操作后完整背包快照(items)
// DialogueAuthRequest: npc_id → DialogueAuthResult: ok + reason + dialogue_token + expires_at
// ItemInstance 对齐客户端 FItemInstance (guid/count/装备角色/武器成长/圣遗物词条)
```

## Roadmap

- M2：权威背包 + MySQL 持久化 —— 已完成
- M3：UE 客户端接入（登录流程走 GameServer，背包改请求/响应模式）—— 已完成（客户端代码 + 服务器集成测试通过，PIE 联调待人工验证）
- M4：对话授权（信令面签发短期票据，VHServer 数据面本地验签）—— 已完成（集成测试 + VHServer 端到端票据校验矩阵通过）
- M5：存储升级 —— 表结构 V2（代理主键 + 复合索引 + V1 在线迁移）与 Redis 热点层（Cache-Aside / 分布式频控 / 在线状态 / 降级）—— 已完成（表迁移 + key 布局 + 降级路径脚本验证）
- M6：战斗校验、断线重连（session_token 恢复会话）
