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
--      且 36 字节宽主键被所有二级索引携带, 存储放大; V2 改自增主键 + guid 唯一二级索引
--   2. 复合索引 (account, acquired_time): 精确覆盖高频查询
--      "WHERE account = ? ORDER BY acquired_time" 的过滤+排序, 免 filesort
--   3. ext_json JSON 列: 武器成长/圣遗物词条等非结构化扩展属性, 避免稀疏列
CREATE TABLE IF NOT EXISTS inventory_items (
    id                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
    guid                  CHAR(36)        NOT NULL COMMENT '物品实例 GUID, 业务唯一键',
    account               VARCHAR(32)     NOT NULL COMMENT '归属账号',
    item_id               INT             NOT NULL COMMENT '物品模板 ID (items.yaml)',
    count                 INT             NOT NULL DEFAULT 1 COMMENT '堆叠数量',
    equipped_character_id INT             NOT NULL DEFAULT -1 COMMENT '装备角色 ID, -1=未装备',
    acquired_time         BIGINT          NOT NULL COMMENT '获取时间 epoch ms',
    category              VARCHAR(16)     NOT NULL COMMENT '分类: weapon/artifact/other',
    ext_json              JSON            NULL COMMENT '扩展属性 (武器成长/圣遗物词条)',
    PRIMARY KEY (id),
    UNIQUE KEY uk_guid (guid),
    KEY idx_account_acquired (account, acquired_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='背包物品';

-- Redis 键设计 (热数据层, 非持久化):
--   session:{token}    -> account        TTL = 2 x 心跳超时   会话令牌注册 (无状态校验/多实例共享)
--   online:{account}   -> session_token  TTL = 2 x 心跳超时   在线状态, 心跳滑动续期, 崩溃自愈
--   player:{account}   -> JSON           TTL = 300s          玩家数据 Cache-Aside 读缓存 (写后失效)
--   dlg:rate:{account} -> counter        TTL = 1s           对话票据签发固定窗口限流 (SET NX + INCR)
