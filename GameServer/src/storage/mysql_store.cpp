#include "game/storage/mysql_store.h"
#include "game/infra/config_manager.hpp"

#include <mysql/mysql.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>

namespace game {
namespace storage {

namespace {

using json = nlohmann::json;

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

json InvItemToJson(const data::InvItem& item) {
    json j;
    if (item.has_weapon) {
        j["weapon"] = {
            {"level", item.weapon_level},
            {"ascension", item.weapon_ascension},
            {"refinement", item.weapon_refinement},
        };
    }
    if (item.has_artifact) {
        json subs = json::array();
        for (const auto& s : item.artifact_sub_stats) {
            subs.push_back({
                {"type", s.stat_type},
                {"value", s.stat_value},
                {"upgrades", s.upgrade_count},
            });
        }
        j["artifact"] = {
            {"set_id", item.artifact_set_id},
            {"slot", item.artifact_slot},
            {"main_stat", item.artifact_main_stat},
            {"main_value", item.artifact_main_stat_value},
            {"sub_stats", std::move(subs)},
        };
    }
    return j;
}

void JsonToInvItem(const std::string& ext_json, data::InvItem* item) {
    if (ext_json.empty() || ext_json == "null") return;
    try {
        json j = json::parse(ext_json);
        if (j.contains("weapon")) {
            item->has_weapon = true;
            item->weapon_level = j["weapon"].value("level", 1);
            item->weapon_ascension = j["weapon"].value("ascension", 0);
            item->weapon_refinement = j["weapon"].value("refinement", 1);
        }
        if (j.contains("artifact")) {
            item->has_artifact = true;
            item->artifact_set_id = j["artifact"].value("set_id", 0);
            item->artifact_slot = j["artifact"].value("slot", "");
            item->artifact_main_stat = j["artifact"].value("main_stat", "");
            item->artifact_main_stat_value = j["artifact"].value("main_value", 0.f);
            if (j["artifact"].contains("sub_stats")) {
                for (const auto& s : j["artifact"]["sub_stats"]) {
                    data::ArtifactSubStat sub;
                    sub.stat_type = s.value("type", "");
                    sub.stat_value = s.value("value", 0.f);
                    sub.upgrade_count = s.value("upgrades", 0);
                    item->artifact_sub_stats.push_back(std::move(sub));
                }
            }
        }
    } catch (const json::exception&) {
        spdlog::error("[MysqlStore] Bad ext_json [guid={}]", item->guid);
    }
}

// ---- 新版表结构 (V2): 代理主键 + 复合索引, 详见 scripts/schema.sql ----
// 表名参数化: 建表与迁移影子表共用同一份 DDL, 避免两份 schema 漂移

std::string PlayersDdl(const char* table) {
    char sql[2048];
    std::snprintf(sql, sizeof(sql), R"SQL(
        CREATE TABLE IF NOT EXISTS %s (
            player_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '代理主键',
            account       VARCHAR(32)    NOT NULL COMMENT '账号, 业务唯一键',
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
            equipped_character_id INT             NOT NULL DEFAULT -1 COMMENT '装备角色 ID, -1=未装备',
            acquired_time         BIGINT          NOT NULL COMMENT '获取时间 epoch ms',
            category              VARCHAR(16)     NOT NULL COMMENT '分类: weapon/artifact/other',
            ext_json              JSON            NULL COMMENT '扩展属性 (武器成长/圣遗物词条)',
            PRIMARY KEY (id),
            UNIQUE KEY uk_guid (guid),
            KEY idx_account_acquired (account, acquired_time)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='背包物品'
    )SQL", table);
    return sql;
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
        !RunQuery(conn, "INSERT INTO players_mig (player_id, account, level, exp) "
                        "SELECT player_id, account, level, exp FROM players") ||
        !RunQuery(conn, "RENAME TABLE players TO players_old, players_mig TO players") ||
        !RunQuery(conn, "DROP TABLE players_old")) {
        throw std::runtime_error(std::string("players migration failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] players 迁移完成 (保留原 player_id, 新增 created_at/last_login_at)");
}

void MigrateInventory(MYSQL* conn) {
    spdlog::warn("[MysqlStore] inventory_items 表为 V1 结构 (guid 主键), 开始迁移 -> V2 (代理主键+复合索引)");
    if (!RunQuery(conn, "DROP TABLE IF EXISTS inventory_items_mig") ||
        !RunQuery(conn, InventoryDdl("inventory_items_mig").c_str()) ||
        !RunQuery(conn, "INSERT INTO inventory_items_mig "
                        "(guid, account, item_id, count, equipped_character_id, acquired_time, category, ext_json) "
                        "SELECT guid, account, item_id, count, equipped_character_id, "
                        "acquired_time, category, ext_json FROM inventory_items") ||
        !RunQuery(conn, "RENAME TABLE inventory_items TO inventory_items_old, "
                        "inventory_items_mig TO inventory_items") ||
        !RunQuery(conn, "DROP TABLE inventory_items_old")) {
        throw std::runtime_error(std::string("inventory_items migration failed: ") + mysql_error(conn));
    }
    spdlog::info("[MysqlStore] inventory_items 迁移完成 (背包数据全量保留)");
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
    if (impl_->conn) {
        mysql_close(impl_->conn);
        impl_->conn = nullptr;
    }
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
    }

    const std::string inv_pk = PrimaryKeyColumn(impl_->conn, "inventory_items");
    if (inv_pk.empty()) {
        if (!RunQuery(impl_->conn, InventoryDdl("inventory_items").c_str())) {
            throw std::runtime_error(std::string("inventory_items create failed: ") + mysql_error(impl_->conn));
        }
    } else if (inv_pk != "id") {
        MigrateInventory(impl_->conn);
    }

    spdlog::info("[MysqlStore] Connected to {}:{} db={} (server {})",
                 impl_->cfg.host, impl_->cfg.port, impl_->cfg.database,
                 mysql_get_server_info(impl_->conn));
}

bool MysqlStore::Ping() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->conn && mysql_ping(impl_->conn) == 0;
}

PlayerRecord MysqlStore::LoadPlayer(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    PlayerRecord rec;

    StmtGuard stmt(impl_->conn,
                   "SELECT player_id, level, exp FROM players WHERE account = ?");
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
    MYSQL_BIND out[3];
    std::memset(out, 0, sizeof(out));
    out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    out[0].buffer = &player_id;
    out[1].buffer_type = MYSQL_TYPE_LONG;
    out[1].buffer = &level;
    out[2].buffer_type = MYSQL_TYPE_LONGLONG;
    out[2].buffer = &exp;
    mysql_stmt_bind_result(stmt.get(), out);

    if (mysql_stmt_fetch(stmt.get()) == 0) {
        rec.exists = true;
        rec.player_id = player_id;
        rec.level = level;
        rec.exp = exp;
    }
    mysql_stmt_free_result(stmt.get());
    return rec;
}

uint64_t MysqlStore::CreatePlayer(const std::string& account) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    StmtGuard stmt(impl_->conn, "INSERT INTO players (account) VALUES (?)");
    MYSQL_BIND in[1];
    std::memset(in, 0, sizeof(in));
    unsigned long acct_len = 0;
    BindString(in, 0, account, &acct_len);
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
                   "SELECT guid, item_id, count, equipped_character_id, acquired_time, "
                   "category, ext_json FROM inventory_items WHERE account = ? "
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
    int32_t equipped = 0;
    int64_t acquired = 0;
    char category[17] = {0};
    char ext_buf[4096] = {0};
    unsigned long guid_len = 0, cat_len = 0, ext_len = 0;
    bool ext_null = false;

    MYSQL_BIND out[7];
    std::memset(out, 0, sizeof(out));
    out[0].buffer_type = MYSQL_TYPE_STRING;
    out[0].buffer = guid;
    out[0].buffer_length = sizeof(guid);
    out[0].length = &guid_len;
    out[1].buffer_type = MYSQL_TYPE_LONG;
    out[1].buffer = &item_id;
    out[2].buffer_type = MYSQL_TYPE_LONG;
    out[2].buffer = &count;
    out[3].buffer_type = MYSQL_TYPE_LONG;
    out[3].buffer = &equipped;
    out[4].buffer_type = MYSQL_TYPE_LONGLONG;
    out[4].buffer = &acquired;
    out[5].buffer_type = MYSQL_TYPE_STRING;
    out[5].buffer = category;
    out[5].buffer_length = sizeof(category);
    out[5].length = &cat_len;
    out[6].buffer_type = MYSQL_TYPE_STRING;
    out[6].buffer = ext_buf;
    out[6].buffer_length = sizeof(ext_buf);
    out[6].length = &ext_len;
    out[6].is_null = &ext_null;
    mysql_stmt_bind_result(stmt.get(), out);

    while (mysql_stmt_fetch(stmt.get()) == 0) {
        data::InvItem item;
        item.guid.assign(guid, guid_len);
        item.item_id = item_id;
        item.count = count;
        item.equipped_character_id = equipped;
        item.acquired_time = acquired;
        if (!ext_null && ext_len > 0) {
            JsonToInvItem(std::string(ext_buf, ext_len), &item);
        }
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
                      "(guid, account, item_id, count, equipped_character_id, acquired_time, category, ext_json) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

        for (const auto& item : items) {
            const std::string ext = InvItemToJson(item).dump();
            MYSQL_BIND in[8];
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
            in[4].buffer_type = MYSQL_TYPE_LONG;
            in[4].buffer = const_cast<int32_t*>(&item.equipped_character_id);
            in[5].buffer_type = MYSQL_TYPE_LONGLONG;
            in[5].buffer = const_cast<int64_t*>(&item.acquired_time);

            const std::string category = item.has_artifact ? "artifact"
                                       : item.has_weapon ? "weapon" : "other";
            unsigned long cat_len = static_cast<unsigned long>(category.size());
            in[6].buffer_type = MYSQL_TYPE_STRING;
            in[6].buffer = const_cast<char*>(category.c_str());
            in[6].length = &cat_len;

            unsigned long ext_len = static_cast<unsigned long>(ext.size());
            in[7].buffer_type = MYSQL_TYPE_STRING;
            in[7].buffer = const_cast<char*>(ext.c_str());
            in[7].length = &ext_len;

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

} // namespace storage
} // namespace game
