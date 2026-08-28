#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "game/data/inv_item.h"
#include "game/data/owned_character.h"
#include "game/data/team_slot.h"

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
    std::string password_salt;   // hex (空串 = 未建档/未设密)
    std::string password_hash;   // hex SHA256(salt+password)
    bool exists = false;         // false = 新玩家, 首次登录
};

// 运行期连续状态 DTO: 位置/HP, 走异步单写者落库, 登录读档恢复
struct PlayerPosition {
    float x = 0.f, y = 0.f, z = 0.f, yaw = 0.f;
    int64_t updated_at = 0;
};

struct CharacterHpRow {
    std::string character_tag;
    float max_hp = 0.f;
    float current_hp = 0.f;
    int64_t updated_at = 0;
};

// MySQL 持久化层: 唯一事实源 (Source of Truth), players + inventory_items + owned_characters 三表
// 表设计要点 (JD: 熟悉表设计和优化):
//   players          代理主键 BIGINT AUTO_INCREMENT, account 唯一业务键
//                    (替代应用侧 hash 生成 ID, 消除碰撞风险)
//   inventory_items  代理主键 + guid 唯一二级索引 (CHAR(36) UUID 不做聚簇主键,
//                    避免随机写页分裂); (account, acquired_time) 复合索引
//                    覆盖 "WHERE account=? ORDER BY acquired_time" 免 filesort
//   owned_characters 代理主键, (account, character_tag) 复合唯一键描述"账户->角色"拥有关系;
//                    is_active 表达当前上场角色; (account, is_active) 覆盖 "查上场角色" 点查
// 线程模型: 单连接 + 内部互斥锁, worker 线程并发调用安全
// 所有带参数 SQL 均走 prepared statement, 无拼接注入面
// 简化策略: 背包/角色操作成功后事务内全量重写 (DELETE+INSERT), 数据规模小, 可靠性优先
class MysqlStore {
public:
    explicit MysqlStore(const infra::MysqlConfig& config);
    ~MysqlStore();

    void Connect();          // 连接 + 建表 + 旧表结构迁移, 失败抛异常
    bool Ping();             // 探活, 断线自动重连

    // 玩家记录: 不存在时 exists=false. 含密码盐与哈希 (认证校验用)
    PlayerRecord LoadPlayer(const std::string& account);

    // 新玩家注册: INSERT 盐+哈希后返回自增 player_id
    uint64_t CreatePlayer(const std::string& account,
                          const std::string& password_salt,
                          const std::string& password_hash);

    // 登录成功刷新最近登录时间 (运营字段)
    void TouchLastLogin(const std::string& account);

    // 等级/经验进度更新 (玩家登录时已建档, UPDATE 即可)
    void UpdatePlayerProgress(const std::string& account, int32_t level, int64_t exp);

    // 背包: 全量加载 / 事务内全量重写
    std::vector<data::InvItem> LoadInventory(const std::string& account);
    void RewriteInventory(const std::string& account, const std::vector<data::InvItem>& items);

    // 玩家拥有角色: 全量加载 / 事务内全量重写
    std::vector<data::OwnedCharacter> LoadOwnedCharacters(const std::string& account);
    void RewriteOwnedCharacters(const std::string& account, const std::vector<data::OwnedCharacter>& chars);

    // 单角色幂等登记 (注册上限扩展/重复上报防抖): 已有则更新 level/exp/acquired_time
    void UpsertOwnedCharacter(const std::string& account, const data::OwnedCharacter& c);

    // 切人落库: 把该账号拥有的角色中, 指定 tag 置为 active, 其余清 0 (单语句原子置位)
    void SetOwnedCharacterActive(const std::string& account, const std::string& character_tag);

    // 切人落库: 配队槽位中匹配 tag 的槽位置 active, 其余清 0 (与 owned_characters.is_active 对齐)
    void SetTeamSlotActive(const std::string& account, const std::string& character_tag);

    // 玩家配队: 全量加载 / 事务内全量重写 (配队列表 + 上场 index, 镜像 InitialTeamTags)
    std::vector<data::TeamSlot> LoadTeamSlots(const std::string& account);
    void RewriteTeamSlots(const std::string& account, const std::vector<data::TeamSlot>& slots);

    // 连续状态异步存档 (位置/HP): 高频变化经单写者 FIFO 落库, 模拟线程永不阻塞等 DB IO
    void SavePlayerPosition(const std::string& account, const PlayerPosition& p);
    void LoadPlayerPosition(const std::string& account, PlayerPosition* out) const;
    void SaveCharacterHp(const std::string& account, const std::string& character_tag,
                         float max_hp, float current_hp);
    void LoadCharacterHp(const std::string& account,
                         std::vector<CharacterHpRow>& out) const;

    // 关键写 (角色登记/切人) + 连续状态写 均为 FIFO 单写者异步落库, 解请求线程同步等 DB IO 的阻塞;
    // 单写者保序 (后到的切人覆盖先到的), 写者与所有读共享同一连接并在 impl_->mtx 下执行。
    // read-your-writes: 登录读档前 / 登出 / 关服调用本方法冲库; 崩溃最多丢未冲缓存的写。
    void FlushAsyncWrites();

private:
    void EnqueueWrite(std::function<void()> op);
    void WriterLoop();

    struct Impl;
    std::unique_ptr<Impl> impl_;

    // 异步单写者 (承载 owned_characters 登记/切人 + 位置/HP 连续状态存档)
    std::thread writer_thread_;
    std::mutex wq_mtx_;
    std::condition_variable wq_cv_;        // 写操作就绪
    std::condition_variable wq_space_cv_;  // 队列有空位
    std::deque<std::function<void()>> write_queue_;
    bool writer_stop_ = false;
    static constexpr size_t kMaxAsyncWrites = 4096;
};

} // namespace storage
} // namespace game
