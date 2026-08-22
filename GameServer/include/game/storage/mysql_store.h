#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "game/data/inv_item.h"

namespace game {
    namespace infra { struct MysqlConfig; }
}

struct MYSQL;

namespace game {
namespace storage {

struct PlayerRecord {
    uint64_t player_id = 0;
    int32_t level = 1;
    int64_t exp = 0;
    bool exists = false;   // false = 新玩家, 首次登录
};

// MySQL 持久化层: 唯一事实源 (Source of Truth), players + inventory_items 两表
// 表设计要点 (JD: 熟悉表设计和优化):
//   players        代理主键 BIGINT AUTO_INCREMENT, account 唯一业务键
//                  (替代应用侧 hash 生成 ID, 消除碰撞风险)
//   inventory_items 代理主键 + guid 唯一二级索引 (CHAR(36) UUID 不做聚簇主键,
//                  避免随机写页分裂); (account, acquired_time) 复合索引
//                  覆盖 "WHERE account=? ORDER BY acquired_time" 免 filesort
// 线程模型: 单连接 + 内部互斥锁, worker 线程并发调用安全
// 所有带参数 SQL 均走 prepared statement, 无拼接注入面
// 简化策略: 背包操作成功后事务内全量重写 (DELETE+INSERT), 背包规模小, 可靠性优先
class MysqlStore {
public:
    explicit MysqlStore(const infra::MysqlConfig& config);
    ~MysqlStore();

    void Connect();          // 连接 + 建表 + 旧表结构迁移, 失败抛异常
    bool Ping();             // 探活, 断线自动重连

    // 玩家记录: 不存在时 exists=false
    PlayerRecord LoadPlayer(const std::string& account);

    // 新玩家注册: INSERT 后返回自增 player_id
    uint64_t CreatePlayer(const std::string& account);

    // 登录成功刷新最近登录时间 (运营字段)
    void TouchLastLogin(const std::string& account);

    // 等级/经验进度更新 (玩家登录时已建档, UPDATE 即可)
    void UpdatePlayerProgress(const std::string& account, int32_t level, int64_t exp);

    // 背包: 全量加载 / 事务内全量重写
    std::vector<data::InvItem> LoadInventory(const std::string& account);
    void RewriteInventory(const std::string& account, const std::vector<data::InvItem>& items);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace storage
} // namespace game
