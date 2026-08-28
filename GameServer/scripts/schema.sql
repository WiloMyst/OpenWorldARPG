-- OpenWorldARPG GameServer 存储层表结构 (V2)
-- 服务器启动时由 MysqlStore::Connect() 幂等自动执行, 本文件为设计文档
-- 迁移: V1 -> V2 自动检测主键列并在线切换 (建新表 -> 迁数据 -> 原子 RENAME)

-- 玩家基础数据
-- 设计要点:
--   1. 代理主键 BIGINT AUTO_INCREMENT: 替代 V1 的应用侧 std::hash 生成 ID, 消除碰撞风险,
--      自增主键保证 InnoDB 聚簇索引顺序写 (无页分裂)
--   2. account 加唯一索引作为业务键: 登录按 account 点查, 走 uk_account
--   3. created_at / last_login_at 运营字段: 注册留存与登录活跃统计
CREATE TABLE IF NOT EXISTS players (
    player_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
    account       VARCHAR(32)    NOT NULL COMMENT '账号, 业务唯一键',
    level         INT            NOT NULL DEFAULT 1 COMMENT '玩家等级',
    exp           BIGINT         NOT NULL DEFAULT 0 COMMENT '经验值',
    created_at    TIMESTAMP      NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
    last_login_at TIMESTAMP      NULL DEFAULT NULL COMMENT '最近登录时间',
    PRIMARY KEY (player_id),
    UNIQUE KEY uk_account (account)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家基础数据';

-- 背包物品
-- 设计要点:
--   1. 代理主键 BIGINT 自增: V1 用 CHAR(36) UUID 做聚簇主键, 随机写导致页分裂,
--      且 36 字节宽主键被所有二级索引携带, 存储放大; V2 起改自增主键 + guid 唯一二级索引
--   2. 复合索引 (account, acquired_time): 精确覆盖高频查询
--      "WHERE account = ? ORDER BY acquired_time" 的过滤+排序, 免 filesort
--   3. V3 仅存动态实例数据 (guid/item_id/count/acquired_time): 静态规则 (分类/堆叠上限)
--      经 item_id 查 items.yaml, 装备位/扩展列随业务收敛移除;
--      V2 -> V3 由启动时 ALTER 自动收缩 (检测 equipped_character_id 列存在)
CREATE TABLE IF NOT EXISTS inventory_items (
    id                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
    guid                  CHAR(36)        NOT NULL COMMENT '物品实例 GUID, 业务唯一键',
    account               VARCHAR(32)     NOT NULL COMMENT '归属账号',
    item_id               INT             NOT NULL COMMENT '物品模板 ID (items.yaml)',
    count                 INT             NOT NULL DEFAULT 1 COMMENT '堆叠数量',
    acquired_time         BIGINT          NOT NULL COMMENT '获取时间 epoch ms',
    PRIMARY KEY (id),
    UNIQUE KEY uk_guid (guid),
    KEY idx_account_acquired (account, acquired_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='背包物品';

-- 玩家拥有角色
-- 游戏三支柱之一 (账户 players / 角色 owned_characters / 道具 inventory_items)
-- 设计要点:
--   1. (account, character_tag) 复合唯一键: 描述"账户 -> 角色"拥有关系, 一个账户不重复拥有同一角色;
--      同时是登录拉全量的点查前缀
--   2. character_tag 用业务字符 (CharacterRegistry 主键) 而非自增 ID: 跨服/跨版本角色身份可迁移,
--      且与服务器权威 CombatManager 的角色 HP 实体键 (character_tag) 对齐
--   3. is_active 表达当前上场角色; 二级索引 (account, is_active) 精确覆盖
--      "某账户当前上场角色是谁" 这一高频点查
--   4. 代理主键 id 保留聚簇顺序写; is_active 用 TINYINT 而非 BIT, 避免 BIT 的怪异排序
CREATE TABLE IF NOT EXISTS owned_characters (
    id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
    account       VARCHAR(32)    NOT NULL COMMENT '归属账号',
    character_tag VARCHAR(32)    NOT NULL COMMENT '角色 Tag (CharacterRegistry 主键)',
    level         INT            NOT NULL DEFAULT 1 COMMENT '角色等级',
    exp           BIGINT         NOT NULL DEFAULT 0 COMMENT '经验值',
    is_active     TINYINT        NOT NULL DEFAULT 0 COMMENT '当前上场角色 1=是',
    acquired_time BIGINT         NOT NULL COMMENT '获取时间 epoch ms',
    PRIMARY KEY (id),
    UNIQUE KEY uk_account_tag (account, character_tag),
    KEY idx_account_active (account, is_active)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家拥有角色';

-- Redis 键设计 (热数据层, 非持久化):
--   session:{token}    -> account        TTL = 2 x 心跳超时   会话令牌注册 (无状态校验/多实例共享)
--   online:{account}   -> session_token  TTL = 2 x 心跳超时   在线状态, 心跳滑动续期, 崩溃自愈
--   player:{account}   -> JSON           TTL = 300s          玩家数据 Cache-Aside 读缓存 (写后失效)
--   dlg:rate:{account} -> counter        TTL = 1s           对话票据签发固定窗口限流 (SET NX + INCR)
-- 归属约定: inventory_items / owned_characters / player_positions / character_hp 不进 Redis, 直读 DB.
--   判据 = 无跨会话热读路径 (owned/HP/位置仅登录边界读一次, 会话期常驻服务端内存裁决),
--   进缓存零收益反添写一致性负担.

-- 玩家位置存档 (运行期连续状态, 异步落库)
-- 设计要点:
--   1. account 主键: 单账号一行, 登录读档恢复出生点 (防"上线回到出生点"丢进度)
--   2. 坐标 FLOAT 即可满足世界精度; updated_at 供读档有效性判断
--   3. 由异步单写者 (MysqlStore::WriterLoop, FIFO) 承接高频节流写入,
--      模拟/战斗线程永不阻塞等 DB IO; 登出/关服/登录读档前 FlushAsyncWrites 冲库
CREATE TABLE IF NOT EXISTS player_positions (
    account    VARCHAR(32) NOT NULL COMMENT '归属账号',
    x          FLOAT       NOT NULL DEFAULT 0 COMMENT '世界坐标 x',
    y          FLOAT       NOT NULL DEFAULT 0 COMMENT '世界坐标 y',
    z          FLOAT       NOT NULL DEFAULT 0 COMMENT '世界坐标 z',
    yaw        FLOAT       NOT NULL DEFAULT 0 COMMENT '朝向',
    updated_at BIGINT      NOT NULL COMMENT '最近存档 epoch ms',
    PRIMARY KEY (account)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家位置存档';

-- 角色 HP 存档 (运行期连续状态, 异步落库)
-- 设计要点:
--   1. (account, character_tag) 复合主键: 一行为一个角色的血线, 登录恢复队伍血线
--   2. 与 owned_characters 分表: HP 是"运行期连续状态", 拥有关系是"低频关键数据",
--      两表按不同节奏落库 (HP 节流异步, 拥有关系异步但低频)
--   3. 恢复语义: 登录读档注入 CombatManager, 客户端 RegisterCharacter 建档时覆盖初始 HP
CREATE TABLE IF NOT EXISTS character_hp (
    account       VARCHAR(32) NOT NULL COMMENT '归属账号',
    character_tag VARCHAR(32) NOT NULL COMMENT '角色 Tag',
    max_hp        FLOAT       NOT NULL DEFAULT 0 COMMENT '角色最大血量',
    current_hp    FLOAT       NOT NULL DEFAULT 0 COMMENT '当前血量',
    updated_at    BIGINT      NOT NULL COMMENT '最近存档 epoch ms',
    PRIMARY KEY (account, character_tag)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色 HP 存档';

-- 玩家配队存档 (镜像客户端 InitialTeamTags + InitialActiveCharacterIndex)
-- 设计要点:
--   1. (account, slot_index) 复合主键: 一行为配队中的一个槽位, slot_index 表达配队顺序
--      (0-based, 与 InitialTeamTags 数组顺序一一对应)
--   2. is_active 表达当前上场槽位 (对应 InitialActiveCharacterIndex);
--      (account, is_active) 二级索引覆盖"当前上场槽位"点查
--   3. 与 owned_characters 分表: 拥有关系是"账户->角色全集", 配队是"当前上阵顺序",
--      语义独立; 新玩家首次登录由服务器从 initial_archive.yaml 播种 (方案 B)
CREATE TABLE IF NOT EXISTS team_slots (
    account       VARCHAR(32) NOT NULL COMMENT '归属账号',
    slot_index    INT         NOT NULL COMMENT '配队槽位 (0-based, 对应 InitialTeamTags 顺序)',
    character_tag VARCHAR(32) NOT NULL COMMENT '该槽位角色 Tag',
    is_active     TINYINT     NOT NULL DEFAULT 0 COMMENT '当前上场槽位 1=是',
    updated_at    BIGINT      NOT NULL COMMENT '最近存档 epoch ms',
    PRIMARY KEY (account, slot_index),
    KEY idx_account_active (account, is_active)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家配队存档';
