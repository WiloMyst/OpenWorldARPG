#include "game/logic/game_logic.h"
#include "game/infra/config_manager.hpp"
#include "game/infra/hmac.hpp"
#include "game/infra/time_utils.h"
#include "game/storage/mysql_store.h"
#include "game/storage/redis_store.h"
#include "game/logic/item_database.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <random>
#include <sstream>

namespace game {
namespace logic {

namespace {

using json = nlohmann::json;

// data::InvItem -> proto ItemInstance
game::ItemInstance ToProto(const data::InvItem& item, const ItemDatabase* db) {
    game::ItemInstance out;
    out.set_item_guid(item.guid);
    out.set_item_id(item.item_id);
    out.set_count(item.count);
    out.set_equipped_character_id(item.equipped_character_id);
    out.set_acquired_time(item.acquired_time);

    if (const ItemConfig* cfg = db->Find(item.item_id)) {
        out.set_category(cfg->category);
    }

    if (item.has_weapon) {
        game::WeaponInstanceData* w = out.mutable_weapon_data();
        w->set_level(item.weapon_level);
        w->set_ascension_level(item.weapon_ascension);
        w->set_refinement_level(item.weapon_refinement);
    }
    if (item.has_artifact) {
        game::ArtifactInstanceData* a = out.mutable_artifact_data();
        a->set_set_id(item.artifact_set_id);
        a->set_slot(item.artifact_slot);
        a->set_main_stat(item.artifact_main_stat);
        a->set_main_stat_value(item.artifact_main_stat_value);
        for (const auto& sub : item.artifact_sub_stats) {
            game::ArtifactSubStat* s = a->add_sub_stats();
            s->set_stat_type(sub.stat_type);
            s->set_stat_value(sub.stat_value);
            s->set_upgrade_count(sub.upgrade_count);
        }
    }
    return out;
}

bool IsValidAccount(const std::string& account) {
    if (account.size() < 3 || account.size() > 32) return false;
    for (char c : account) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    return true;
}

std::string MakeSessionToken() {
    static std::mt19937_64 rng(std::random_device{}());
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << rng();
    return oss.str();
}

} // namespace

GameLogic::GameLogic(const infra::AppConfig& config)
    : static_token_(config.static_token),
      dialogue_secret_(config.dialogue_secret),
      dialogue_token_ttl_sec_(config.dialogue_token_ttl_sec),
      session_ttl_sec_(std::max(60, config.heartbeat_timeout_sec * 2)),
      player_cache_ttl_sec_(config.redis.player_cache_ttl_sec),
      item_db_(ItemDatabase::LoadFromYaml(config.items_config_path)) {
    // MySQL 是存档核心依赖, 连接失败直接抛出 -> 服务器拒绝启动
    store_ = std::make_unique<storage::MysqlStore>(config.mysql);
    store_->Connect();

    // Redis 是热数据层可选依赖, 失败仅降级不阻断启动
    redis_ = std::make_unique<storage::RedisStore>(config.redis);
    redis_->Connect();
}

GameLogic::~GameLogic() = default;

InventoryManager* GameLogic::GetInventory(const std::string& account) {
    auto it = inventories_.find(account);
    return it == inventories_.end() ? nullptr : it->second.get();
}

void GameLogic::WritePlayerCache(const std::string& account,
                                 const storage::PlayerRecord& rec) {
    if (!redis_ || !redis_->Available()) return;
    const json j = {
        {"player_id", rec.player_id},
        {"level", rec.level},
        {"exp", rec.exp},
    };
    redis_->SetEx("player:" + account, j.dump(), player_cache_ttl_sec_);
}

game::LoginResponse GameLogic::HandleLogin(const game::LoginRequest& req,
                                           std::string* bound_account) {
    game::LoginResponse resp;

    if (!IsValidAccount(req.account())) {
        resp.set_success(false);
        resp.set_error_msg("Invalid account: 3-32 chars, [A-Za-z0-9_] only");
        return resp;
    }
    if (!static_token_.empty() && req.token() != static_token_) {
        resp.set_success(false);
        resp.set_error_msg("Auth failed: invalid token");
        spdlog::warn("[GameLogic] Login rejected [account={}, reason=bad_token]", req.account());
        return resp;
    }

    try {
        // Cache-Aside 读路径: Redis 命中免 MySQL 读; 脏缓存解析失败自动回源
        storage::PlayerRecord rec;
        const std::string cache_key = "player:" + req.account();
        bool cache_hit = false;
        if (redis_ && redis_->Available()) {
            std::string cached;
            if (redis_->Get(cache_key, &cached)) {
                try {
                    const json j = json::parse(cached);
                    rec.exists = true;
                    rec.player_id = j.value("player_id", 0ULL);
                    rec.level = j.value("level", 1);
                    rec.exp = j.value("exp", 0LL);
                    cache_hit = true;
                } catch (const json::exception&) {
                    spdlog::warn("[GameLogic] 玩家缓存损坏, 回源 MySQL [account={}]", req.account());
                }
            }
        }
        if (!cache_hit) {
            rec = store_->LoadPlayer(req.account());
            if (rec.exists) WritePlayerCache(req.account(), rec);
        }

        uint64_t player_id = 0;
        if (rec.exists) {
            player_id = rec.player_id;
            store_->TouchLastLogin(req.account());
        } else {
            // 新玩家: 注册建档 (自增代理主键), 写入缓存
            player_id = store_->CreatePlayer(req.account());
            rec.level = 1;
            rec.exp = 0;
            WritePlayerCache(req.account(), storage::PlayerRecord{player_id, 1, 0, true});
        }

        resp.set_success(true);
        resp.set_player_id(player_id);
        resp.set_session_token(MakeSessionToken());

        game::PlayerData* data = resp.mutable_player_data();
        data->set_account(req.account());
        data->set_level(rec.level);
        data->set_exp(rec.exp);

        // 会话令牌注册到 Redis: 无状态校验支持 (多实例共享), 心跳驱动 TTL
        if (redis_ && redis_->Available()) {
            redis_->SetEx("session:" + resp.session_token(), req.account(), session_ttl_sec_);
            redis_->SetEx("online:" + req.account(), resp.session_token(), session_ttl_sec_);
        }

        // 背包是强一致权威数据, 不进缓存: 每次登录直读 MySQL
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto inv = std::make_unique<InventoryManager>(&item_db_, req.account());
            inv->LoadFrom(store_->LoadInventory(req.account()));
            for (const auto& item : inv->items()) {
                *resp.add_inventory() = ToProto(item, &item_db_);
            }
            inventories_[req.account()] = std::move(inv);
        }

        *bound_account = req.account();
        spdlog::info("[GameLogic] Login ok [account={}, level={}, items={}, cache={}]",
                     req.account(), data->level(), resp.inventory_size(),
                     cache_hit ? "hit" : "miss");
    } catch (const std::exception& e) {
        resp.set_success(false);
        resp.set_error_msg("Load player data failed, please retry");
        spdlog::error("[GameLogic] Login DB error [account={}, err={}]", req.account(), e.what());
    }
    return resp;
}

game::HeartbeatAck GameLogic::HandleHeartbeat(const std::string& account,
                                              const game::Heartbeat& req) {
    // 心跳续期在线状态 (滑动 TTL): key 过期即判定离线, 进程崩溃无残留 key
    if (!account.empty() && redis_ && redis_->Available()) {
        redis_->Expire("online:" + account, session_ttl_sec_);
    }

    game::HeartbeatAck ack;
    ack.set_server_timestamp(infra::NowMs());
    return ack;
}

game::SaveDataResponse GameLogic::HandleSaveData(const std::string& account,
                                                 const game::SaveDataRequest& req) {
    game::SaveDataResponse resp;
    const game::PlayerData& data = req.player_data();

    if (data.account() != account) {
        resp.set_success(false);
        resp.set_error_msg("Account mismatch: save data belongs to another player");
        return resp;
    }
    if (data.level() < 1 || data.level() > 90) {
        resp.set_success(false);
        resp.set_error_msg("Invalid level range");
        return resp;
    }

    try {
        // Cache-Aside 写路径: 先更新 MySQL (事实源), 成功后失效缓存, 下次登录回源重建
        store_->UpdatePlayerProgress(account, data.level(), data.exp());
        if (redis_ && redis_->Available()) {
            redis_->Del("player:" + account);
        }
        resp.set_success(true);
        spdlog::info("[GameLogic] Save ok [account={}, level={}, exp={}]",
                     account, data.level(), data.exp());
    } catch (const std::exception& e) {
        resp.set_success(false);
        resp.set_error_msg("Persist failed, see server log");
        spdlog::error("[GameLogic] Save DB error [account={}, err={}]", account, e.what());
    }
    return resp;
}

game::InventoryOpResponse GameLogic::HandleInventoryOp(const std::string& account,
                                                       const game::InventoryOpRequest& req) {
    game::InventoryOpResponse resp;

    std::lock_guard<std::mutex> lock(mtx_);
    InventoryManager* inv = GetInventory(account);
    if (!inv) {
        resp.set_success(false);
        resp.set_error_msg("Player not online");
        return resp;
    }

    OpResult result{false, "Unknown operation"};
    switch (req.op_case()) {
        case game::InventoryOpRequest::kAdd:
            result = inv->Add(req.add().item_id(), req.add().amount());
            break;
        case game::InventoryOpRequest::kRemove:
            result = inv->Remove(req.remove().item_guid(), req.remove().amount());
            break;
        case game::InventoryOpRequest::kEquip:
            result = inv->Equip(req.equip().item_guid(), req.equip().character_id());
            break;
        case game::InventoryOpRequest::kUnequip:
            result = inv->Unequip(req.unequip().item_guid());
            break;
        case game::InventoryOpRequest::kUse:
            result = inv->Use(req.use().item_guid(), req.use().target_character_id(),
                              req.use().amount());
            break;
        default:
            break;
    }

    if (!result.ok) {
        resp.set_success(false);
        resp.set_error_msg(result.reason);
        spdlog::info("[GameLogic] Inventory op rejected [account={}, reason={}]",
                     account, result.reason);
        return resp;
    }

    // 成功操作: 立即全量落库, 再回快照
    try {
        store_->RewriteInventory(account, inv->items());
    } catch (const std::exception& e) {
        // 落库失败: 回滚内存操作的最简方式是重载 DB 数据, 保持内存与磁盘一致
        spdlog::error("[GameLogic] Inventory persist failed [account={}, err={}]",
                      account, e.what());
        inv->LoadFrom(store_->LoadInventory(account));
        resp.set_success(false);
        resp.set_error_msg("Persist failed, operation rolled back");
        return resp;
    }

    resp.set_success(true);
    for (const auto& item : inv->items()) {
        *resp.add_items() = ToProto(item, &item_db_);
    }
    return resp;
}

void GameLogic::HandleLogout(const std::string& account) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        inventories_.erase(account);
        last_token_ms_.erase(account);
    }

    // 显式登出: 经 online:{account} 反查 token, 成对清理会话注册
    // 被踢/崩溃路径不清理, 统一由 TTL 到期自愈 (无泄漏)
    if (redis_ && redis_->Available() && !account.empty()) {
        std::string token;
        if (redis_->Get("online:" + account, &token) && !token.empty()) {
            redis_->Del("session:" + token);
        }
        redis_->Del("online:" + account);
    }
    spdlog::info("[GameLogic] Inventory unloaded [account={}]", account);
}

game::DialogueAuthResult GameLogic::HandleDialogueAuth(const std::string& account,
                                                       const game::DialogueAuthRequest& req) {
    game::DialogueAuthResult resp;

    if (dialogue_secret_.empty()) {
        resp.set_ok(false);
        resp.set_reason("Dialogue auth disabled: server has no dialogue_secret");
        spdlog::warn("[GameLogic] Dialogue auth rejected: dialogue_secret not configured");
        return resp;
    }

    const int64_t now = infra::NowMs();

    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (inventories_.find(account) == inventories_.end()) {
            resp.set_ok(false);
            resp.set_reason("Player not online");
            return resp;
        }
    }

    // 频控在锁外执行: Redis 命令带超时, 阻塞在线检查会拖慢所有玩家的背包操作
    // Redis 固定窗口限流 (多实例一致); 不可用时降级进程内存限流 (单实例语义)
    if (redis_ && redis_->Available()) {
        if (!redis_->RateAllow("dlg:rate:" + account, 1, 1)) {
            resp.set_ok(false);
            resp.set_reason("Too many requests, retry later");
            return resp;
        }
    } else {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = last_token_ms_.find(account);
        if (it != last_token_ms_.end() && now - it->second < 1000) {
            resp.set_ok(false);
            resp.set_reason("Too many requests, retry later");
            return resp;
        }
        last_token_ms_[account] = now;
    }

    // 票据格式: account.npc_id.expires_at.hmac_hex
    // HMAC 消息 = 前三段原文, 与 VHServer 侧验证逻辑构成共享契约
    const int64_t expires_at = now + static_cast<int64_t>(dialogue_token_ttl_sec_) * 1000;
    const std::string payload = account + "." + std::to_string(req.npc_id()) +
                                "." + std::to_string(expires_at);

    resp.set_ok(true);
    resp.set_dialogue_token(payload + "." + crypto::HmacSha256Hex(dialogue_secret_, payload));
    resp.set_expires_at(expires_at);
    spdlog::info("[GameLogic] Dialogue token issued [account={}, npc_id={}, ttl_sec={}, limiter={}]",
                 account, req.npc_id(), dialogue_token_ttl_sec_,
                 (redis_ && redis_->Available()) ? "redis" : "memory");
    return resp;
}

} // namespace logic
} // namespace game
