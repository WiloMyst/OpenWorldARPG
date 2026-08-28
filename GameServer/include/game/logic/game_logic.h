#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "game.pb.h"
#include "game/data/initial_archive.h"
#include "game/data/owned_character.h"
#include "game/logic/inventory_manager.h"
#include "game/storage/mysql_store.h"

namespace game {
    namespace infra { struct AppConfig; }
    namespace storage { class MysqlStore; class RedisStore; struct PlayerRecord; }
}

namespace game {
namespace logic {

// 游戏逻辑层: 登录/背包权威操作与持久化, 不感知网络与会话
//
// 存储分工 (高并发实践):
//   MySQL  = 唯一事实源: 玩家档案/背包, 每次背包操作成功即落库
//   Redis  = 热数据层 (可选依赖, 断连自动降级):
//     player:{account}  玩家基础数据 Cache-Aside 读缓存 (登录快路径)
//     session:{token}   会话令牌注册 (无状态校验支持, 多实例共享)
//     online:{account}  在线状态, 心跳滑动续期, TTL 到期即离线 (崩溃自愈)
//     dlg:rate:{account} 对话票据签发分布式限流 (固定窗口计数)
//     Redis 不可用时: 读走 MySQL 直读, 频控退化为进程内存限流
//
// 线程模型: mtx_ 保护 inventories_ 与所有背包操作 (个人项目并发低, 正确性优先)
//
// 归属划分 (何者进缓存, 何者直读 DB):
//   进 Cache-Aside 的判据 = 存在高并发快路径热读, 且写频率低、失效协调成本可接受:
//     - player:{account}: 登录/心跳反复读, 进缓存
//   直读 DB 的判据 = 每会话只读一次或写频繁, 缓存零收益反添一致性负担:
//     - owned_characters (角色拥有/active): 仅登录边界读一次, 之后常驻 CombatManager
//       内存判定, 会话期不碰 Redis; 每次注册/切人都写库. 进缓存纯是负收益, 保持直读.
//     - inventory_items: 物品以 account 点查, 无跨会话热路径, 直读.
class GameLogic {
public:
    explicit GameLogic(const infra::AppConfig& config);
    ~GameLogic();

    // 校验登录并加载玩家数据与背包; 成功时写入 bound_account
    game::LoginResponse HandleLogin(const game::LoginRequest& req, std::string* bound_account);

    game::HeartbeatAck HandleHeartbeat(const std::string& account, const game::Heartbeat& req);

    // 权威背包操作: 校验 -> 执行 -> 落库, 响应携带操作后完整快照
    game::InventoryOpResponse HandleInventoryOp(const std::string& account,
                                                const game::InventoryOpRequest& req);

    // 对话授权 (信令面): 校验在线与频控, 签发 HMAC 短期票据供客户端直连 VHServer
    game::DialogueAuthResult HandleDialogueAuth(const std::string& account,
                                                const game::DialogueAuthRequest& req);

    // 登出: 卸载内存背包 (数据已在每次操作时落库), 清理 Redis 在线状态
    void HandleLogout(const std::string& account);

    // 玩家拥有角色存储接入 (由 SessionHandler 在登录/切人时编排):
    // 登录拉取 DB 拥有角色 -> CombatManager 恢复; 角色注册/切人时反向落库
    std::vector<data::OwnedCharacter> LoadOwnedCharacters(const std::string& account);
    void RewriteOwnedCharacters(const std::string& account,
                                const std::vector<data::OwnedCharacter>& chars);
    void UpsertOwnedCharacter(const std::string& account, const data::OwnedCharacter& c);
    void SetOwnedCharacterActive(const std::string& account, const std::string& character_tag);

    // 玩家配队存储接入: 新玩家首次登录由服务器从 initial_archive.yaml 播种落库
    std::vector<data::TeamSlot> LoadTeamSlots(const std::string& account);
    void RewriteTeamSlots(const std::string& account, const std::vector<data::TeamSlot>& slots);
    void SetTeamSlotActive(const std::string& account, const std::string& character_tag);

    // 新玩家初始存档 (镜像客户端 UInitialArchiveData): 登录播种数据源
    const data::InitialArchive& GetInitialArchive() const { return initial_archive_; }

    // 连续状态异步存档接入 (位置/HP): 由 WorldManager/CombatManager 经回调调用, 异步落库不阻塞模拟线程
    void SavePlayerPosition(const std::string& account, const storage::PlayerPosition& p);
    void LoadPlayerPosition(const std::string& account, storage::PlayerPosition* out);
    void SaveCharacterHp(const std::string& account, const std::string& character_tag,
                         float max_hp, float current_hp);
    void LoadCharacterHp(const std::string& account, std::vector<storage::CharacterHpRow>& out);

    // 冲库异步写 (登录读档前 / 登出调用): 保证 read-your-writes 与关服前可靠落入
    void FlushPersistence();

private:
    InventoryManager* GetInventory(const std::string& account);
    void WritePlayerCache(const std::string& account, const storage::PlayerRecord& rec);

    const std::string dialogue_secret_;
    const int dialogue_token_ttl_sec_;
    const int session_ttl_sec_;               // Redis 会话/在线 key TTL (心跳超时 x2)
    const int player_cache_ttl_sec_;          // 玩家数据缓存 TTL
    std::unique_ptr<storage::MysqlStore> store_;
    std::unique_ptr<storage::RedisStore> redis_;
    ItemDatabase item_db_;
    data::InitialArchive initial_archive_;  // 新玩家初始存档 (登录播种数据源)

    std::mutex mtx_;
    std::unordered_map<std::string, std::unique_ptr<InventoryManager>> inventories_;

    // 对话票据频控 (Redis 降级时的进程内存兜底): 每账号上次签发时刻 (ms)
    std::unordered_map<std::string, int64_t> last_token_ms_;
};

} // namespace logic
} // namespace game
