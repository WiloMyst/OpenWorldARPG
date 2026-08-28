#include "game/storage/mysql_store.h"
#include "game/infra/config_manager.h"

#include <mysql/mysql.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <functional>

namespace game {
namespace storage {

namespace {

// RAII 语句包装: 析构自动 close
class StmtGuard {
public:
    StmtGuard(MYSQL* conn, const char* sql) {
        stmt_ = mysql_stmt_init(conn);
        if (!stmt_) throw std::runtime_error("mysql_stmt_init failed: out of memory");
        if (mysql_stmt_prepare(stmt_, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            const std::string err = mysql_stmt_error(stmt_);
            mysql_stmt_close(stmt_);
            stmt_ = nullptr;
            throw std::runtime_error("stmt prepare failed: " + err + " [sql=" + sql + "]");
        }
    }

    ~StmtGuard() {
        if (stmt_) mysql_stmt_close(stmt_);
    }

    MYSQL_STMT* get() { return stmt_; }

private:
    MYSQL_STMT* stmt_ = nullptr;
};

// ---- 新版表结构 (V3): 代理主键 + 复合索引, 详见 scripts/schema.sql ----
// 表名参数化: 建表与迁移影子表共用同一份 DDL, 避免两份 schema 漂移

std::string PlayersDdl(const char* table) {
    char sql[2048];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
            player_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
            account       VARCHAR(32)    NOT NULL COMMENT '账号, 业务唯一键',
            password_salt VARCHAR(64)    NOT NULL DEFAULT '' COMMENT '密码盐(hex, 16字节, 每账号独立)',
            password_hash VARCHAR(128)   NOT NULL DEFAULT '' COMMENT '密码哈希 SHA256(salt+password) hex',
            level         INT            NOT NULL DEFAULT 1 COMMENT '玩家等级',
            exp           BIGINT         NOT NULL DEFAULT 0 COMMENT '经验值',
            created_at    TIMESTAMP      NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
            last_login_at TIMESTAMP      NULL DEFAULT NULL COMMENT '最近登录时间',
            PRIMARY KEY (player_id),
            UNIQUE KEY uk_account (account)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家基础数据'
    )SQL", table);
    return sql;
}

std::string InventoryDdl(const char* table) {
    char sql[2048];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
            id                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
            guid                  CHAR(36)        NOT NULL COMMENT '物品实例 GUID, 业务唯一键',
            account               VARCHAR(32)     NOT NULL COMMENT '归属账号',
            item_id               INT             NOT NULL COMMENT '物品模板 ID (items.yaml)',
            count                 INT             NOT NULL DEFAULT 1 COMMENT '堆叠数量',
            acquired_time         BIGINT          NOT NULL COMMENT '获取时间 epoch ms',
            PRIMARY KEY (id),
            UNIQUE KEY uk_guid (guid),
            KEY idx_account_acquired (account, acquired_time)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='背包物品 (V3: 仅动态数据, 静态规则经 item_id 查 items.yaml)'
    )SQL", table);
    return sql;
}

std::string OwnedCharactersDdl(const char* table) {
    char sql[2048];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
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
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家拥有角色'
    )SQL", table);
    return sql;
}

// 玩家位置存档: 单账号一行, 登录恢复出生点 (防"上线回到出生点"丢进度)
// account 作主键, 每次移动节流异步 upsert, 无读放大诉求故不为坐标建索引
std::string PlayerPositionsDdl(const char* table) {
    char sql[1536];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
            account    VARCHAR(32) NOT NULL COMMENT '归属账号',
            x          FLOAT       NOT NULL DEFAULT 0 COMMENT '世界坐标 x',
            y          FLOAT       NOT NULL DEFAULT 0 COMMENT '世界坐标 y',
            z          FLOAT       NOT NULL DEFAULT 0 COMMENT '世界坐标 z',
            yaw        FLOAT       NOT NULL DEFAULT 0 COMMENT '朝向',
            updated_at BIGINT      NOT NULL COMMENT '最近存档 epoch ms',
            PRIMARY KEY (account)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家位置存档'
    )SQL", table);
    return sql;
}

// 角色 HP 存档: (account, character_tag) 一行为一个角色的血线, 登录恢复队伍血线
// 与 owned_characters 分表: HP 是"运行期连续状态", 拥有关系是"低频关键数据", 两表各自按不同节奏落库
std::string CharacterHpDdl(const char* table) {
    char sql[1536];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
            account       VARCHAR(32) NOT NULL COMMENT '归属账号',
            character_tag VARCHAR(32) NOT NULL COMMENT '角色 Tag',
            max_hp        FLOAT       NOT NULL DEFAULT 0 COMMENT '角色最大血量',
            current_hp    FLOAT       NOT NULL DEFAULT 0 COMMENT '当前血量',
            updated_at    BIGINT      NOT NULL COMMENT '最近存档 epoch ms',
            PRIMARY KEY (account, character_tag)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色 HP 存档'
    )SQL", table);
    return sql;
}

// 玩家配队存档: 一行为配队中的一个槽位, slot_index 表达配队顺序 (镜像 InitialTeamTags),
// is_active 表达当前上场 (对应 InitialActiveCharacterIndex).
// 与 owned_characters 分表: 拥有关系是"账户->角色全集", 配队是"当前上阵顺序", 语义独立.
std::string TeamSlotsDdl(const char* table) {
    char sql[1536];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
            account       VARCHAR(32) NOT NULL COMMENT '归属账号',
            slot_index    INT         NOT NULL COMMENT '配队槽位 (0-based, 对应 InitialTeamTags 顺序)',
            character_tag VARCHAR(32) NOT NULL COMMENT '该槽位角色 Tag',
            is_active     TINYINT     NOT NULL DEFAULT 0 COMMENT '当前上场槽位 1=是',
            updated_at    BIGINT      NOT NULL COMMENT '最近存档 epoch ms',
            PRIMARY KEY (account, slot_index),
            KEY idx_account_active (account, is_active)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家配队存档'
    )SQL", table);
    return sql;
}

// 列存在检测 (information_schema), 用于 V2 -> V3 收敛迁移触发
bool HasColumn(MYSQL* conn, const char* table, const char* column) {
    const std::string sql =
        "SELECT 1 FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) +
        "' AND COLUMN_NAME = '" + std::string(column) + "'";
    if (mysql_query(conn, sql.c_str()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;
    const bool found = mysql_fetch_row(res) != nullptr;
    mysql_free_result(res);
    return found;
}

// 表主键列名 (information_schema); 空串 = 表不存在
std::string PrimaryKeyColumn(MYSQL* conn, const char* table) {
    const std::string sql =
        "SELECT COLUMN_NAME FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) +
        "' AND COLUMN_KEY = 'PRI'";
    if (mysql_query(conn, sql.c_str()) != 0) return "";
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return "";
    std::string pk;
    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        if (row[0]) pk = row[0];
    }
    mysql_free_result(res);
    return pk;
}

bool RunQuery(MYSQL* conn, const char* sql) {
    if (mysql_query(conn, sql) != 0) {
        spdlog::error("[MysqlStore] DDL failed: {} [sql={}]", mysql_error(conn), sql);
        return false;
    }
    return true;
}

// 旧表 (V1) -> 新表 (V2) 在线迁移: 建影子表 -> 迁数据 -> 原子 RENAME 切换 -> 删旧表
// RENAME 双表切换原子生效, 中间无半开状态; 开发库规模小, 迁移瞬间完成
void MigratePlayers(MYSQL* conn) {
    spdlog::warn("[MysqlStore] players 表为 V1 结构 (account 主键), 开始迁移 -> V2 (自增代理主键)");
    if (!RunQuery(conn, "DROP TABLE IF EXISTS players_mig") ||
        !RunQuery(conn, PlayersDdl("players_mig").c_str()) ||
        // 丢弃 V1 的应用侧 hash player_id: 让影子表 AUTO_INCREMENT 重分配紧凑顺序号.
        // 否则 hash 原样进入自增列, 计数从 19 位起步 (自增矛盾). 各表均以 account 关联,
        // 无任何外键依赖 player_id, 重排安全.
        !RunQuery(conn, "INSERT INTO players_mig (account, level, exp) "
                        "SELECT account, level, exp FROM players") ||
        !RunQuery(conn, "RENAME TABLE players TO players_old, players_mig TO players") ||
        !RunQuery(conn, "DROP TABLE players_old")) {
        throw std::runtime_error(std::string("players migration failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] players 迁移完成 (重分配紧凑顺序 player_id, 新增 created_at/last_login_at)");
}

// 检测已被 V1 hash-residue 污染的 players 表 (早期迁移把 std::hash player_id 原样带进自增列):
// 签名 = max(player_id) 异常偏大 (序数 id COUNT 到不了 2^40, 64-bit hash 普遍落在高位)
bool HasHashHeritagePlayerIds(MYSQL* conn) {
    if (mysql_query(conn, "SELECT COUNT(*), MAX(player_id) FROM players") != 0) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;
    bool flag = false;
    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        if (row[0] && row[1]) {
            const unsigned long long max_id = std::strtoull(row[1], nullptr, 10);
            flag = max_id >= (1ULL << 40);
        }
    }
    mysql_free_result(res);
    return flag;
}

// 对 hash-residue 污染的表一次性重排为紧凑自增 id: 幂等 (归位后 max << 阈值不再触发).
// 与迁移同构: 影子表 -> 迁数据(不含 id, 自增重分配) -> 原子 RENAME 切换.
void NormalizePlayerIds(MYSQL* conn) {
    spdlog::warn("[MysqlStore] players 检测到 hash 残留 player_id, 重排为紧凑自增 id");
    if (!RunQuery(conn, "DROP TABLE IF EXISTS players_norm") ||
        !RunQuery(conn, PlayersDdl("players_norm").c_str()) ||
        !RunQuery(conn, "INSERT INTO players_norm (account, level, exp, created_at, last_login_at) "
                        "SELECT account, level, exp, created_at, last_login_at "
                        "FROM players ORDER BY player_id") ||
        !RunQuery(conn, "RENAME TABLE players TO players_old, players_norm TO players") ||
        !RunQuery(conn, "DROP TABLE players_old")) {
        throw std::runtime_error(std::string("players normalize failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] players player_id 已重排为紧凑顺序号");
}

// 认证改造 (V3.5): 旧结构 players 无密码列, 存量账号无法按新凭据校验.
// 直接清空玩家域数据并重建带密码列的表. 迁移期可接受数据重置:
//   - 关联表 TRUNCATE (结构不变, 仅清按 account 归属的数据)
//   - players 必须 DROP + CREATE: TRUNCATE 保留旧列定义, IF NOT EXISTS 不会换结构
// 幂等: 重建后 password_hash 列存在, 不再触发.
void RebuildPlayersWithAuth(MYSQL* conn) {
    spdlog::warn("[MysqlStore] players 为旧认证结构, 清空玩家域数据并重建 (加入密码列)");
    static const char* kPlayerTables[] = {
        "inventory_items", "owned_characters", "player_positions",
        "character_hp", "team_slots",
    };
    for (const char* t : kPlayerTables) {
        // 表可能不存在 (首次建库路径不经此), TRUNCATE 失败忽略
        char sql[256];
        std::snprintf(sql, sizeof(sql), "TRUNCATE TABLE %s", t);
        mysql_query(conn, sql);
    }
    if (!RunQuery(conn, "DROP TABLE IF EXISTS players") ||
        !RunQuery(conn, PlayersDdl("players").c_str())) {
        throw std::runtime_error(std::string("players rebuild with auth failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] players 已重建为 V3.5 (带 password_salt/password_hash)");
}

void MigrateInventory(MYSQL* conn) {
    spdlog::warn("[MysqlStore] inventory_items 表为 V1 结构 (guid 主键), 开始迁移 -> V3 (代理主键+复合索引)");
    if (!RunQuery(conn, "DROP TABLE IF EXISTS inventory_items_mig") ||
        !RunQuery(conn, InventoryDdl("inventory_items_mig").c_str()) ||
        !RunQuery(conn, "INSERT INTO inventory_items_mig "
                        "(guid, account, item_id, count, acquired_time) "
                        "SELECT guid, account, item_id, count, acquired_time "
                        "FROM inventory_items") ||
        !RunQuery(conn, "RENAME TABLE inventory_items TO inventory_items_old, "
                        "inventory_items_mig TO inventory_items") ||
        !RunQuery(conn, "DROP TABLE inventory_items_old")) {
        throw std::runtime_error(std::string("inventory_items migration failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] inventory_items 迁移完成 (背包数据全量保留)");
}

// V2 -> V3: 装备/使用业务收敛, equipped_character_id/category/ext_json 三列成为死数据,
// 单条 ALTER 原地收缩 (幂等: 列不存在后不再触发)
void SlimInventoryToV3(MYSQL* conn) {
    spdlog::warn("[MysqlStore] inventory_items 表为 V2 结构, 收缩为 V3 (移除装备位/分类/扩展列)");
    if (!RunQuery(conn, "ALTER TABLE inventory_items "
                        "DROP COLUMN equipped_character_id, "
                        "DROP COLUMN category, "
                        "DROP COLUMN ext_json")) {
        throw std::runtime_error(std::string("inventory_items slim to V3 failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] inventory_items 已收缩为 V3 (动态实例数据保留)");
}

// 绑定 VARCHAR 参数的通用辅助
void BindString(MYSQL_BIND* bind, int idx, const std::string& value, unsigned long* len) {
    *len = static_cast<unsigned long>(value.size());
    bind[idx].buffer_type = MYSQL_TYPE_STRING;
    bind[idx].buffer = const_cast<char*>(value.c_str());
    bind[idx].length = len;
}

} // namespace

struct MysqlStore::Impl {
    infra::MysqlConfig cfg;
    MYSQL* conn = nullptr;
    std::mutex mtx;
};

MysqlStore::MysqlStore(const infra::MysqlConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->cfg = config;
}

MysqlStore::~MysqlStore() {
    // 停止写者并等待其冲完队列内剩余写再退出 => 优雅关服不丢未落库的连续状态
    {
        std::lock_guard<std::mutex> lk(wq_mtx_);
        writer_stop_ = true;
    }
    wq_cv_.notify_all();
    wq_space_cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (impl_->conn) {
        mysql_close(impl_->conn);
        impl_->conn = nullptr;
    }
}

void MysqlStore::EnqueueWrite(std::function<void()> op) {
    {
        std::unique_lock<std::mutex> lk(wq_mtx_);
        if (!writer_thread_.joinable() && !writer_stop_) {
            // 写者未启动 (未 Connect 的单元路径): 退化为同步执行, 避免写被静默丢弃
            lk.unlock();
            op();
            return;
        }
        // 有界队列: 背压到生产者, 保住顺序与"关键写可靠性"语义 (写频低, 基本不会触发)
        wq_space_cv_.wait(lk, [&] { return writer_stop_ || write_queue_.size() < kMaxAsyncWrites; });
        if (writer_stop_) {
            // 写者已退出 (关服后残余写): 退化为同步执行, 避免静默丢弃
            lk.unlock();
            op();
            return;
        }
        write_queue_.push_back(std::move(op));
    }
    wq_cv_.notify_one();
}

void MysqlStore::WriterLoop() {
    for (;;) {
        std::function<void()> op;
        {
            std::unique_lock<std::mutex> lk(wq_mtx_);
            wq_cv_.wait(lk, [&] { return writer_stop_ || !write_queue_.empty(); });
            if (write_queue_.empty()) {
                return;  // 已请求停止且排空 => 退出
            }
            op = std::move(write_queue_.front());
            write_queue_.pop_front();
            wq_space_cv_.notify_one();
        }
        op();
    }
}

void MysqlStore::FlushAsyncWrites() {
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    {
        std::unique_lock<std::mutex> lk(wq_mtx_);
        if (writer_stop_ || !writer_thread_.joinable()) {
            return;  // 无写者或已关服: 之前写入均已同步/落库, 无需等待
        }
        // 屏障: FIFO 保证在它之前入队的写都已执行完毕
        write_queue_.push_back([&] {
            { std::lock_guard<std::mutex> z(m); done = true; }
            cv.notify_one();
        });
    }
    wq_cv_.notify_one();
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [&] { return done; });
}

void MysqlStore::Connect() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (impl_->conn) mysql_close(impl_->conn);

    impl_->conn = mysql_init(nullptr);
    if (!impl_->conn) throw std::runtime_error("mysql_init failed: out of memory");

    const unsigned int to = static_cast<unsigned int>(impl_->cfg.connect_timeout_ms);
    mysql_options(impl_->conn, MYSQL_OPT_CONNECT_TIMEOUT, &to);
    mysql_options(impl_->conn, MYSQL_OPT_READ_TIMEOUT, &to);
    mysql_options(impl_->conn, MYSQL_OPT_WRITE_TIMEOUT, &to);
    bool reconnect = true;
    mysql_options(impl_->conn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(impl_->conn,
                            impl_->cfg.host.c_str(),
                            impl_->cfg.user.c_str(),
                            impl_->cfg.password.c_str(),
                            impl_->cfg.database.c_str(),
                            static_cast<unsigned int>(impl_->cfg.port),
                            nullptr, 0)) {
        const std::string err = mysql_error(impl_->conn);
        mysql_close(impl_->conn);
        impl_->conn = nullptr;
        throw std::runtime_error("mysql connect failed: " + err);
    }
    mysql_set_character_set(impl_->conn, "utf8mb4");

    // schema 初始化与版本迁移 (幂等):
    //   V1 -> V2 判定依据 = 主键列 (players: account->player_id, inventory: guid->id)
    const std::string players_pk = PrimaryKeyColumn(impl_->conn, "players");
    if (players_pk.empty()) {
        if (!RunQuery(impl_->conn, PlayersDdl("players").c_str())) {
            throw std::runtime_error(std::string("players create failed: ") + mysql_error(impl_->conn));
        }
    } else if (players_pk != "player_id") {
        MigratePlayers(impl_->conn);
    } else if (!HasColumn(impl_->conn, "players", "password_hash")) {
        // 认证改造 (V3.5): 旧结构无密码列, 存量账号无法按新凭据校验.
        // 按需求直接清空玩家域数据并重建: 迁移期可接受数据重置, 避免"存量账号补密"分支的复杂度.
        RebuildPlayersWithAuth(impl_->conn);
    }

    // 一次性归位历史迁移的 hash 残留: 消除"自增代理主键却从 19 位 hash 起步"的自增矛盾.
    // 幂等: 归位后 max(player_id) 回到序数量级, 不再触发.
    if (HasHashHeritagePlayerIds(impl_->conn)) {
        NormalizePlayerIds(impl_->conn);
    }

    const std::string inv_pk = PrimaryKeyColumn(impl_->conn, "inventory_items");
    if (inv_pk.empty()) {
        if (!RunQuery(impl_->conn, InventoryDdl("inventory_items").c_str())) {
            throw std::runtime_error(std::string("inventory_items create failed: ") + mysql_error(impl_->conn));
        }
    } else if (inv_pk != "id") {
        MigrateInventory(impl_->conn);
    } else if (HasColumn(impl_->conn, "inventory_items", "equipped_character_id")) {
        SlimInventoryToV3(impl_->conn);
    }

    // owned_characters: 全新表, 无历史版本需迁移
    if (PrimaryKeyColumn(impl_->conn, "owned_characters").empty()) {
        if (!RunQuery(impl_->conn, OwnedCharactersDdl("owned_characters").c_str())) {
            throw std::runtime_error(std::string("owned_characters create failed: ") + mysql_error(impl_->conn));
        }
    }

    // player_positions / character_hp: 连续状态异步存档, 无历史版本需迁移
    if (PrimaryKeyColumn(impl_->conn, "player_positions").empty()) {
        if (!RunQuery(impl_->conn, PlayerPositionsDdl("player_positions").c_str())) {
            throw std::runtime_error(std::string("player_positions create failed: ") + mysql_error(impl_->conn));
        }
    }
    if (PrimaryKeyColumn(impl_->conn, "character_hp").empty()) {
        if (!RunQuery(impl_->conn, CharacterHpDdl("character_hp").c_str())) {
            throw std::runtime_error(std::string("character_hp create failed: ") + mysql_error(impl_->conn));
        }
    }

    // team_slots: 配队存档 (镜像 InitialTeamTags), 无历史版本需迁移
    if (PrimaryKeyColumn(impl_->conn, "team_slots").empty()) {
        if (!RunQuery(impl_->conn, TeamSlotsDdl("team_slots").c_str())) {
            throw std::runtime_error(std::string("team_slots create failed: ") + mysql_error(impl_->conn));
        }
    }

    spdlog::info("[MysqlStore] Connected to {}:{} db={} (server {})",
                 impl_->cfg.host, impl_->cfg.port, impl_->cfg.database,
                 mysql_get_server_info(impl_->conn));

    // 异步单写者就绪 (承载位置/HP/角色登记/切人的非阻塞落库). 需持有 conn, 故在连上后启动.
    std::lock_guard<std::mutex> wq(wq_mtx_);
    if (!writer_thread_.joinable()) {
        writer_stop_ = false;
        writer_thread_ = std::thread(&MysqlStore::WriterLoop, this);
    }
}

bool MysqlStore::Ping() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->conn && mysql_ping(impl_->conn) == 0;
}

PlayerRecord MysqlStore::LoadPlayer(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    PlayerRecord rec;

    StmtGuard stmt(impl_->conn,
                   "SELECT player_id, level, exp, password_salt, password_hash "
                   "FROM players WHERE account = ?");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("LoadPlayer execute: ") + mysql_stmt_error(stmt.get()));
    }

    uint64_t player_id = 0;
    int32_t level = 0;
    int64_t exp = 0;
    char salt_buf[128] = {0};
    char hash_buf[128] = {0};
    unsigned long salt_len = 0, hash_len = 0;
    MYSQL_BIND out[5];
    std::memset(out, 0, sizeof(out));
    out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    out[0].buffer = &player_id;
    out[1].buffer_type = MYSQL_TYPE_LONG;
    out[1].buffer = &level;
    out[2].buffer_type = MYSQL_TYPE_LONGLONG;
    out[2].buffer = &exp;
    out[3].buffer_type = MYSQL_TYPE_STRING;
    out[3].buffer = salt_buf;
    out[3].buffer_length = sizeof(salt_buf);
    out[3].length = &salt_len;
    out[4].buffer_type = MYSQL_TYPE_STRING;
    out[4].buffer = hash_buf;
    out[4].buffer_length = sizeof(hash_buf);
    out[4].length = &hash_len;
    mysql_stmt_bind_result(stmt.get(), out);

    if (mysql_stmt_fetch(stmt.get()) == 0) {
        rec.exists = true;
        rec.player_id = player_id;
        rec.level = level;
        rec.exp = exp;
        rec.password_salt.assign(salt_buf, salt_len);
        rec.password_hash.assign(hash_buf, hash_len);
    }
    mysql_stmt_free_result(stmt.get());
    return rec;
}

uint64_t MysqlStore::CreatePlayer(const std::string& account,
                                  const std::string& password_salt,
                                  const std::string& password_hash) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    StmtGuard stmt(impl_->conn,
                   "INSERT INTO players (account, password_salt, password_hash) VALUES (?, ?, ?)");
    MYSQL_BIND in[3];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0, salt_len = 0, hash_len = 0;
    BindString(in, 0, account, &acct_len);
    BindString(in, 1, password_salt, &salt_len);
    BindString(in, 2, password_hash, &hash_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("CreatePlayer: ") + mysql_stmt_error(stmt.get()));
    }
    const uint64_t id = mysql_stmt_insert_id(stmt.get());
    spdlog::info("[MysqlStore] Player registered [account={}, player_id={}]", account, id);
    return id;
}

void MysqlStore::TouchLastLogin(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    StmtGuard stmt(impl_->conn,
                   "UPDATE players SET last_login_at = CURRENT_TIMESTAMP WHERE account = ?");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("TouchLastLogin: ") + mysql_stmt_error(stmt.get()));
    }
}

void MysqlStore::UpdatePlayerProgress(const std::string& account, int32_t level, int64_t exp) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    StmtGuard stmt(impl_->conn,
                   "UPDATE players SET level = ?, exp = ? WHERE account = ?");
    MYSQL_BIND in[3];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    in[0].buffer_type = MYSQL_TYPE_LONG;
    in[0].buffer = &level;
    in[1].buffer_type = MYSQL_TYPE_LONGLONG;
    in[1].buffer = &exp;
    BindString(in, 2, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("UpdatePlayerProgress: ") + mysql_stmt_error(stmt.get()));
    }
}

std::vector<data::InvItem> MysqlStore::LoadInventory(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    std::vector<data::InvItem> items;

    // 排序走 idx_account_acquired (account, acquired_time) 复合索引, 免 filesort
    StmtGuard stmt(impl_->conn,
                   "SELECT guid, item_id, count, acquired_time "
                   "FROM inventory_items WHERE account = ? "
                   "ORDER BY acquired_time, id");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("LoadInventory execute: ") + mysql_stmt_error(stmt.get()));
    }

    char guid[37] = {0};
    int32_t item_id = 0;
    int32_t count = 0;
    int64_t acquired = 0;
    unsigned long guid_len = 0;

    MYSQL_BIND out[4];
    std::memset(out, 0, sizeof(out));
    out[0].buffer_type = MYSQL_TYPE_STRING;
    out[0].buffer = guid;
    out[0].buffer_length = sizeof(guid);
    out[0].length = &guid_len;
    out[1].buffer_type = MYSQL_TYPE_LONG;
    out[1].buffer = &item_id;
    out[2].buffer_type = MYSQL_TYPE_LONG;
    out[2].buffer = &count;
    out[3].buffer_type = MYSQL_TYPE_LONGLONG;
    out[3].buffer = &acquired;
    mysql_stmt_bind_result(stmt.get(), out);

    while (mysql_stmt_fetch(stmt.get()) == 0) {
        data::InvItem item;
        item.guid.assign(guid, guid_len);
        item.item_id = item_id;
        item.count = count;
        item.acquired_time = acquired;
        items.push_back(std::move(item));
    }
    mysql_stmt_free_result(stmt.get());
    return items;
}

void MysqlStore::RewriteInventory(const std::string& account,
                                  const std::vector<data::InvItem>& items) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    // 全量重写: 事务内 DELETE + 批量 INSERT, 中途失败回滚不留脏数据
    if (mysql_autocommit(impl_->conn, 0) != 0) {
        throw std::runtime_error("autocommit off failed");
    }

    try {
        {
            StmtGuard del(impl_->conn, "DELETE FROM inventory_items WHERE account = ?");
            MYSQL_BIND in[1];
            std::memset(in, 0, sizeof(in));
            unsigned long acct_len = 0;
            BindString(in, 0, account, &acct_len);
            mysql_stmt_bind_param(del.get(), in);
            if (mysql_stmt_execute(del.get()) != 0) {
                throw std::runtime_error(std::string("delete: ") + mysql_stmt_error(del.get()));
            }
        }

        StmtGuard ins(impl_->conn,
                      "INSERT INTO inventory_items "
                      "(guid, account, item_id, count, acquired_time) "
                      "VALUES (?, ?, ?, ?, ?)");

        for (const auto& item : items) {
            MYSQL_BIND in[5];
            std::memset(in, 0, sizeof(in));

            unsigned long guid_len = static_cast<unsigned long>(item.guid.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(item.guid.c_str());
            in[0].length = &guid_len;

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[1].buffer_type = MYSQL_TYPE_STRING;
            in[1].buffer = const_cast<char*>(account.c_str());
            in[1].length = &acct_len;

            in[2].buffer_type = MYSQL_TYPE_LONG;
            in[2].buffer = const_cast<int32_t*>(&item.item_id);
            in[3].buffer_type = MYSQL_TYPE_LONG;
            in[3].buffer = const_cast<int32_t*>(&item.count);
            in[4].buffer_type = MYSQL_TYPE_LONGLONG;
            in[4].buffer = const_cast<int64_t*>(&item.acquired_time);

            mysql_stmt_bind_param(ins.get(), in);
            if (mysql_stmt_execute(ins.get()) != 0) {
                throw std::runtime_error(std::string("insert: ") + mysql_stmt_error(ins.get()));
            }
        }

        if (mysql_commit(impl_->conn) != 0) {
            throw std::runtime_error("commit failed");
        }
    } catch (...) {
        mysql_rollback(impl_->conn);
        mysql_autocommit(impl_->conn, 1);
        throw;
    }
    mysql_autocommit(impl_->conn, 1);
}

std::vector<data::OwnedCharacter> MysqlStore::LoadOwnedCharacters(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    std::vector<data::OwnedCharacter> chars;

    // (account, character_tag) 复合唯一键保证一个账户不重复拥有同一角色;
    // character_tag 前缀匹配走 uk_account_tag, 天然按账户拉全量
    StmtGuard stmt(impl_->conn,
                   "SELECT character_tag, level, exp, is_active, acquired_time "
                   "FROM owned_characters WHERE account = ? ORDER BY acquired_time, id");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("LoadOwnedCharacters execute: ") + mysql_stmt_error(stmt.get()));
    }

    char tag[33] = {0};
    int32_t level = 0;
    int64_t exp = 0;
    int8_t is_active = 0;
    int64_t acquired = 0;
    unsigned long tag_len = 0;

    MYSQL_BIND out[5];
    std::memset(out, 0, sizeof(out));
    out[0].buffer_type = MYSQL_TYPE_STRING;
    out[0].buffer = tag;
    out[0].buffer_length = sizeof(tag);
    out[0].length = &tag_len;
    out[1].buffer_type = MYSQL_TYPE_LONG;
    out[1].buffer = &level;
    out[2].buffer_type = MYSQL_TYPE_LONGLONG;
    out[2].buffer = &exp;
    out[3].buffer_type = MYSQL_TYPE_TINY;
    out[3].buffer = &is_active;
    out[4].buffer_type = MYSQL_TYPE_LONGLONG;
    out[4].buffer = &acquired;
    mysql_stmt_bind_result(stmt.get(), out);

    while (mysql_stmt_fetch(stmt.get()) == 0) {
        data::OwnedCharacter c;
        c.character_tag.assign(tag, tag_len);
        c.level = level;
        c.exp = exp;
        c.is_active = is_active != 0;
        c.acquired_time = acquired;
        chars.push_back(std::move(c));
    }
    mysql_stmt_free_result(stmt.get());
    return chars;
}

void MysqlStore::RewriteOwnedCharacters(const std::string& account,
                                        const std::vector<data::OwnedCharacter>& chars) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    // 全量重写: 事务内 DELETE + 批量 INSERT, 与背包同构, 中途失败回滚
    if (mysql_autocommit(impl_->conn, 0) != 0) {
        throw std::runtime_error("autocommit off failed");
    }

    try {
        {
            StmtGuard del(impl_->conn, "DELETE FROM owned_characters WHERE account = ?");
            MYSQL_BIND in[1];
            std::memset(in, 0, sizeof(in));
            unsigned long acct_len = 0;
            BindString(in, 0, account, &acct_len);
            mysql_stmt_bind_param(del.get(), in);
            if (mysql_stmt_execute(del.get()) != 0) {
                throw std::runtime_error(std::string("delete: ") + mysql_stmt_error(del.get()));
            }
        }

        StmtGuard ins(impl_->conn,
                      "INSERT INTO owned_characters "
                      "(account, character_tag, level, exp, is_active, acquired_time) "
                      "VALUES (?, ?, ?, ?, ?, ?)");

        for (const auto& c : chars) {
            MYSQL_BIND in[6];
            std::memset(in, 0, sizeof(in));

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(account.c_str());
            in[0].length = &acct_len;

            unsigned long tag_len = static_cast<unsigned long>(c.character_tag.size());
            in[1].buffer_type = MYSQL_TYPE_STRING;
            in[1].buffer = const_cast<char*>(c.character_tag.c_str());
            in[1].length = &tag_len;

            in[2].buffer_type = MYSQL_TYPE_LONG;
            in[2].buffer = const_cast<int32_t*>(&c.level);
            in[3].buffer_type = MYSQL_TYPE_LONGLONG;
            in[3].buffer = const_cast<int64_t*>(&c.exp);

            int8_t active = c.is_active ? 1 : 0;
            in[4].buffer_type = MYSQL_TYPE_TINY;
            in[4].buffer = &active;

            in[5].buffer_type = MYSQL_TYPE_LONGLONG;
            in[5].buffer = const_cast<int64_t*>(&c.acquired_time);

            mysql_stmt_bind_param(ins.get(), in);
            if (mysql_stmt_execute(ins.get()) != 0) {
                throw std::runtime_error(std::string("insert: ") + mysql_stmt_error(ins.get()));
            }
        }

        if (mysql_commit(impl_->conn) != 0) {
            throw std::runtime_error("commit failed");
        }
    } catch (...) {
        mysql_rollback(impl_->conn);
        mysql_autocommit(impl_->conn, 1);
        throw;
    }
    mysql_autocommit(impl_->conn, 1);
}

void MysqlStore::UpsertOwnedCharacter(const std::string& account,
                                      const data::OwnedCharacter& c) {
    // 关键写异步落库: 单写者保序, 请求线程不阻塞等 DB IO (切人/登记可发生在战斗高频期)
    EnqueueWrite([this, account, c]() {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        try {
            StmtGuard stmt(impl_->conn,
                           "INSERT INTO owned_characters "
                           "(account, character_tag, level, exp, is_active, acquired_time) "
                           "VALUES (?, ?, ?, ?, ?, ?) "
                           "ON DUPLICATE KEY UPDATE level = VALUES(level), exp = VALUES(exp), "
                           "is_active = VALUES(is_active), acquired_time = VALUES(acquired_time)");
            MYSQL_BIND in[6];
            std::memset(in, 0, sizeof(in));

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(account.c_str());
            in[0].length = &acct_len;

            unsigned long tag_len = static_cast<unsigned long>(c.character_tag.size());
            in[1].buffer_type = MYSQL_TYPE_STRING;
            in[1].buffer = const_cast<char*>(c.character_tag.c_str());
            in[1].length = &tag_len;

            in[2].buffer_type = MYSQL_TYPE_LONG;
            in[2].buffer = const_cast<int32_t*>(&c.level);
            in[3].buffer_type = MYSQL_TYPE_LONGLONG;
            in[3].buffer = const_cast<int64_t*>(&c.exp);

            int8_t active = c.is_active ? 1 : 0;
            in[4].buffer_type = MYSQL_TYPE_TINY;
            in[4].buffer = &active;

            in[5].buffer_type = MYSQL_TYPE_LONGLONG;
            in[5].buffer = const_cast<int64_t*>(&c.acquired_time);

            mysql_stmt_bind_param(stmt.get(), in);
            if (mysql_stmt_execute(stmt.get()) != 0) {
                throw std::runtime_error(std::string("UpsertOwnedCharacter: ") + mysql_stmt_error(stmt.get()));
            }
        } catch (const std::exception& e) {
            spdlog::error("[MysqlStore] UpsertOwnedCharacter async failed [account={}, tag={}, err={}]",
                          account, c.character_tag, e.what());
        }
    });
}

void MysqlStore::SetOwnedCharacterActive(const std::string& account,
                                         const std::string& character_tag) {
    // 单语句原子置位: 目标角色置 1, 同账号其它角色统一清 0 (避免先清后置的非原子滑窗)
    EnqueueWrite([this, account, character_tag]() {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        try {
            StmtGuard stmt(impl_->conn,
                           "UPDATE owned_characters SET is_active = (character_tag = ?) WHERE account = ?");
            MYSQL_BIND in[2];
            std::memset(in, 0, sizeof(in));

            unsigned long tag_len = static_cast<unsigned long>(character_tag.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(character_tag.c_str());
            in[0].length = &tag_len;

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[1].buffer_type = MYSQL_TYPE_STRING;
            in[1].buffer = const_cast<char*>(account.c_str());
            in[1].length = &acct_len;

            mysql_stmt_bind_param(stmt.get(), in);
            if (mysql_stmt_execute(stmt.get()) != 0) {
                throw std::runtime_error(std::string("SetOwnedCharacterActive: ") + mysql_stmt_error(stmt.get()));
            }
        } catch (const std::exception& e) {
            spdlog::error("[MysqlStore] SetOwnedCharacterActive async failed [account={}, tag={}, err={}]",
                          account, character_tag, e.what());
        }
    });
}

void MysqlStore::SetTeamSlotActive(const std::string& account,
                                   const std::string& character_tag) {
    // 与 owned_characters.is_active 对齐: 配队槽位中匹配 tag 的槽位置 1, 其余清 0
    EnqueueWrite([this, account, character_tag]() {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        try {
            StmtGuard stmt(impl_->conn,
                           "UPDATE team_slots SET is_active = (character_tag = ?) WHERE account = ?");
            MYSQL_BIND in[2];
            std::memset(in, 0, sizeof(in));

            unsigned long tag_len = static_cast<unsigned long>(character_tag.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(character_tag.c_str());
            in[0].length = &tag_len;

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[1].buffer_type = MYSQL_TYPE_STRING;
            in[1].buffer = const_cast<char*>(account.c_str());
            in[1].length = &acct_len;

            mysql_stmt_bind_param(stmt.get(), in);
            if (mysql_stmt_execute(stmt.get()) != 0) {
                throw std::runtime_error(std::string("SetTeamSlotActive: ") + mysql_stmt_error(stmt.get()));
            }
        } catch (const std::exception& e) {
            spdlog::error("[MysqlStore] SetTeamSlotActive async failed [account={}, tag={}, err={}]",
                          account, character_tag, e.what());
        }
    });
}

std::vector<data::TeamSlot> MysqlStore::LoadTeamSlots(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    std::vector<data::TeamSlot> slots;

    // (account, slot_index) 主键保证一个账户一个槽位一行; 按 slot_index 排序还原配队顺序
    StmtGuard stmt(impl_->conn,
                   "SELECT slot_index, character_tag, is_active, updated_at "
                   "FROM team_slots WHERE account = ? ORDER BY slot_index");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("LoadTeamSlots execute: ") + mysql_stmt_error(stmt.get()));
    }

    int32_t slot_index = 0;
    char tag[33] = {0};
    int8_t is_active = 0;
    int64_t updated = 0;
    unsigned long tag_len = 0;

    MYSQL_BIND out[4];
    std::memset(out, 0, sizeof(out));
    out[0].buffer_type = MYSQL_TYPE_LONG;
    out[0].buffer = &slot_index;
    out[1].buffer_type = MYSQL_TYPE_STRING;
    out[1].buffer = tag;
    out[1].buffer_length = sizeof(tag);
    out[1].length = &tag_len;
    out[2].buffer_type = MYSQL_TYPE_TINY;
    out[2].buffer = &is_active;
    out[3].buffer_type = MYSQL_TYPE_LONGLONG;
    out[3].buffer = &updated;
    mysql_stmt_bind_result(stmt.get(), out);

    while (mysql_stmt_fetch(stmt.get()) == 0) {
        data::TeamSlot s;
        s.slot_index = slot_index;
        s.character_tag.assign(tag, tag_len);
        s.is_active = is_active != 0;
        s.updated_at = updated;
        slots.push_back(std::move(s));
    }
    mysql_stmt_free_result(stmt.get());
    return slots;
}

void MysqlStore::RewriteTeamSlots(const std::string& account,
                                  const std::vector<data::TeamSlot>& slots) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    // 全量重写: 事务内 DELETE + 批量 INSERT, 与 owned_characters 同构, 中途失败回滚
    if (mysql_autocommit(impl_->conn, 0) != 0) {
        throw std::runtime_error("autocommit off failed");
    }

    try {
        {
            StmtGuard del(impl_->conn, "DELETE FROM team_slots WHERE account = ?");
            MYSQL_BIND in[1];
            std::memset(in, 0, sizeof(in));
            unsigned long acct_len = 0;
            BindString(in, 0, account, &acct_len);
            mysql_stmt_bind_param(del.get(), in);
            if (mysql_stmt_execute(del.get()) != 0) {
                throw std::runtime_error(std::string("delete: ") + mysql_stmt_error(del.get()));
            }
        }

        StmtGuard ins(impl_->conn,
                      "INSERT INTO team_slots (account, slot_index, character_tag, is_active, updated_at) "
                      "VALUES (?, ?, ?, ?, ?)");

        for (const auto& s : slots) {
            MYSQL_BIND in[5];
            std::memset(in, 0, sizeof(in));

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(account.c_str());
            in[0].length = &acct_len;

            in[1].buffer_type = MYSQL_TYPE_LONG;
            in[1].buffer = const_cast<int32_t*>(&s.slot_index);

            unsigned long tag_len = static_cast<unsigned long>(s.character_tag.size());
            in[2].buffer_type = MYSQL_TYPE_STRING;
            in[2].buffer = const_cast<char*>(s.character_tag.c_str());
            in[2].length = &tag_len;

            int8_t active = s.is_active ? 1 : 0;
            in[3].buffer_type = MYSQL_TYPE_TINY;
            in[3].buffer = &active;

            in[4].buffer_type = MYSQL_TYPE_LONGLONG;
            in[4].buffer = const_cast<int64_t*>(&s.updated_at);

            mysql_stmt_bind_param(ins.get(), in);
            if (mysql_stmt_execute(ins.get()) != 0) {
                throw std::runtime_error(std::string("insert: ") + mysql_stmt_error(ins.get()));
            }
        }

        if (mysql_commit(impl_->conn) != 0) {
            throw std::runtime_error("commit failed");
        }
    } catch (...) {
        mysql_rollback(impl_->conn);
        mysql_autocommit(impl_->conn, 1);
        throw;
    }
    mysql_autocommit(impl_->conn, 1);
}

void MysqlStore::SavePlayerPosition(const std::string& account, const PlayerPosition& p) {
    EnqueueWrite([this, account, p]() {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        try {
            // updated_at 在写者侧取当前时间: 调用方 (节流后的 WorldManager) 不感知时间戳,
            // 且登录读档用 updated_at > 0 判定"有无存档", 必须保证入库非 0.
            const int64_t now = static_cast<int64_t>(::time(nullptr)) * 1000;
            StmtGuard stmt(impl_->conn,
                           "INSERT INTO player_positions (account, x, y, z, yaw, updated_at) "
                           "VALUES (?, ?, ?, ?, ?, ?) "
                           "ON DUPLICATE KEY UPDATE x = VALUES(x), y = VALUES(y), "
                           "z = VALUES(z), yaw = VALUES(yaw), updated_at = VALUES(updated_at)");
            MYSQL_BIND in[6];
            std::memset(in, 0, sizeof(in));

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(account.c_str());
            in[0].length = &acct_len;

            in[1].buffer_type = MYSQL_TYPE_FLOAT;
            in[1].buffer = const_cast<float*>(&p.x);
            in[2].buffer_type = MYSQL_TYPE_FLOAT;
            in[2].buffer = const_cast<float*>(&p.y);
            in[3].buffer_type = MYSQL_TYPE_FLOAT;
            in[3].buffer = const_cast<float*>(&p.z);
            in[4].buffer_type = MYSQL_TYPE_FLOAT;
            in[4].buffer = const_cast<float*>(&p.yaw);

            in[5].buffer_type = MYSQL_TYPE_LONGLONG;
            in[5].buffer = const_cast<int64_t*>(&now);

            mysql_stmt_bind_param(stmt.get(), in);
            if (mysql_stmt_execute(stmt.get()) != 0) {
                throw std::runtime_error(std::string("SavePlayerPosition: ") + mysql_stmt_error(stmt.get()));
            }
        } catch (const std::exception& e) {
            spdlog::error("[MysqlStore] SavePlayerPosition async failed [account={}, err={}]", account, e.what());
        }
    });
}

void MysqlStore::LoadPlayerPosition(const std::string& account, PlayerPosition* out) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    StmtGuard stmt(impl_->conn,
                   "SELECT x, y, z, yaw, updated_at FROM player_positions WHERE account = ?");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("LoadPlayerPosition execute: ") + mysql_stmt_error(stmt.get()));
    }

    float x = 0, y = 0, z = 0, yaw = 0;
    int64_t updated = 0;
    MYSQL_BIND obind[5];
    std::memset(obind, 0, sizeof(obind));
    obind[0].buffer_type = MYSQL_TYPE_FLOAT;
    obind[0].buffer = &x;
    obind[1].buffer_type = MYSQL_TYPE_FLOAT;
    obind[1].buffer = &y;
    obind[2].buffer_type = MYSQL_TYPE_FLOAT;
    obind[2].buffer = &z;
    obind[3].buffer_type = MYSQL_TYPE_FLOAT;
    obind[3].buffer = &yaw;
    obind[4].buffer_type = MYSQL_TYPE_LONGLONG;
    obind[4].buffer = &updated;

    const int rc = mysql_stmt_fetch(stmt.get());
    mysql_stmt_free_result(stmt.get());
    if (rc == 0) {
        out->x = x;
        out->y = y;
        out->z = z;
        out->yaw = yaw;
        out->updated_at = updated;
    }
}

void MysqlStore::SaveCharacterHp(const std::string& account, const std::string& character_tag,
                                 float max_hp, float current_hp) {
    EnqueueWrite([this, account, character_tag, max_hp, current_hp]() {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        try {
            StmtGuard stmt(impl_->conn,
                           "INSERT INTO character_hp (account, character_tag, max_hp, current_hp, updated_at) "
                           "VALUES (?, ?, ?, ?, ?) "
                           "ON DUPLICATE KEY UPDATE max_hp = VALUES(max_hp), "
                           "current_hp = VALUES(current_hp), updated_at = VALUES(updated_at)");
            MYSQL_BIND in[5];
            std::memset(in, 0, sizeof(in));

            unsigned long acct_len = static_cast<unsigned long>(account.size());
            in[0].buffer_type = MYSQL_TYPE_STRING;
            in[0].buffer = const_cast<char*>(account.c_str());
            in[0].length = &acct_len;

            unsigned long tag_len = static_cast<unsigned long>(character_tag.size());
            in[1].buffer_type = MYSQL_TYPE_STRING;
            in[1].buffer = const_cast<char*>(character_tag.c_str());
            in[1].length = &tag_len;

            in[2].buffer_type = MYSQL_TYPE_FLOAT;
            in[2].buffer = const_cast<float*>(&max_hp);
            in[3].buffer_type = MYSQL_TYPE_FLOAT;
            in[3].buffer = const_cast<float*>(&current_hp);

            int64_t now = static_cast<int64_t>(::time(nullptr)) * 1000;
            in[4].buffer_type = MYSQL_TYPE_LONGLONG;
            in[4].buffer = &now;

            mysql_stmt_bind_param(stmt.get(), in);
            if (mysql_stmt_execute(stmt.get()) != 0) {
                throw std::runtime_error(std::string("SaveCharacterHp: ") + mysql_stmt_error(stmt.get()));
            }
        } catch (const std::exception& e) {
            spdlog::error("[MysqlStore] SaveCharacterHp async failed [account={}, tag={}, err={}]",
                          account, character_tag, e.what());
        }
    });
}

void MysqlStore::LoadCharacterHp(const std::string& account,
                                 std::vector<CharacterHpRow>& out) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    StmtGuard stmt(impl_->conn,
                   "SELECT character_tag, max_hp, current_hp, updated_at "
                   "FROM character_hp WHERE account = ?");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
    mysql_stmt_bind_param(stmt.get(), in);

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw std::runtime_error(std::string("LoadCharacterHp execute: ") + mysql_stmt_error(stmt.get()));
    }

    char tag[33] = {0};
    float max_hp = 0, cur_hp = 0;
    int64_t updated = 0;
    unsigned long tag_len = 0;
    MYSQL_BIND outc[4];
    std::memset(outc, 0, sizeof(outc));
    outc[0].buffer_type = MYSQL_TYPE_STRING;
    outc[0].buffer = tag;
    outc[0].buffer_length = sizeof(tag);
    outc[0].length = &tag_len;
    outc[1].buffer_type = MYSQL_TYPE_FLOAT;
    outc[1].buffer = &max_hp;
    outc[2].buffer_type = MYSQL_TYPE_FLOAT;
    outc[2].buffer = &cur_hp;
    outc[3].buffer_type = MYSQL_TYPE_LONGLONG;
    outc[3].buffer = &updated;
    mysql_stmt_bind_result(stmt.get(), outc);

    while (mysql_stmt_fetch(stmt.get()) == 0) {
        CharacterHpRow r;
        r.character_tag.assign(tag, tag_len);
        r.max_hp = max_hp;
        r.current_hp = cur_hp;
        r.updated_at = updated;
        out.push_back(std::move(r));
    }
    mysql_stmt_free_result(stmt.get());
}

} // namespace storage
} // namespace game
