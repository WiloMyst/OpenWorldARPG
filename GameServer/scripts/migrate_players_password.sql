-- 玩家认证改造: players 表增加密码两列 (SHA-256 + 盐)
-- 一次性迁移参考脚本。实际执行由 game_server 启动 Connect() 幂等自动完成
-- (HasColumn 检测缺失列后 ADD COLUMN, 见 mysql_store.cpp 迁移分支)。
-- 本脚本保留为运维/手工核对用, 与自动迁移 DDL 保持一致。

ALTER TABLE players
    ADD COLUMN password_salt VARCHAR(64) NOT NULL DEFAULT '' COMMENT '密码盐(hex, 16字节)',
    ADD COLUMN password_hash VARCHAR(128) NOT NULL DEFAULT '' COMMENT '密码哈希 SHA256(salt+password) hex';

-- 核对: 列已存在
-- SELECT column_name FROM information_schema.columns
--   WHERE table_schema = DATABASE() AND table_name = 'players'
--     AND column_name IN ('password_salt','password_hash');

-- 存量账号两列默认空字符串, 首次登录走补密路径自动写入, 无需手工处理。