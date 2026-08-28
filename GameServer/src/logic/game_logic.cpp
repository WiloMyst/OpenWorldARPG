#include "game/logic/game_logic.h"
#include "game/infra/config_manager.h"
#include "game/infra/hmac.h"
#include "game/infra/sha256.h"
#include "game/infra/time_utils.h"
#include "game/storage/mysql_store.h"
#include "game/storage/redis_store.h"
#include "game/logic/item_database.h"
#include "game/logic/initial_archive_loader.h"

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
    out.set_acquired_time(item.acquired_time);

    if (const ItemConfig* cfg = db->Find(item.item_id)) {
        out.set_category(cfg->category);
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
    : dialogue_secret_(config.dialogue_secret),
      dialogue_token_ttl_sec_(config.dialogue_token_ttl_sec),
      session_ttl_sec_(std::max(60, config.heartbeat_timeout_sec * 2)),
      player_cache_ttl_sec_(config.redis.player_cache_ttl_sec),
      item_db_(ItemDatabase::LoadFromYaml(config.items_config_path)),
      initial_archive_(LoadInitialArchive(config.initial_archive_config_path)) {
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
    if (req.password().empty()) {
        resp.set_success(false);
        resp.set_error_msg("Password required");
        return resp;
    }

    try {
        // 玩家记录 (含密码盐/哈希) 从 MySQL 读, 不缓存到 Redis:
        // 密码是登录凭据, 只信任唯一事实源, 不置于热数据层
        storage::PlayerRecord rec = store_->LoadPlayer(req.account());

        std::string salt = rec.password_salt;
        std::string password_hash = rec.password_hash;
        if (rec.exists && password_hash.empty()) {
            // 理论不可达 (迁移时已清空无密码存量账号), 防御拒登
            resp.set_success(false);
            resp.set_error_msg("Account has no password, login refused");
            spdlog::warn("[GameLogic] login refused: account has empty password_hash [account={}]",
                         req.account());
            return resp;
        }
        if (!rec.exists) {
            // 首次登录即注册: 生成独立盐 -> 哈希 -> 落库建档
            // 盐以 hex 编码存储 (仅 ASCII, 兼容 utf8mb4 VARCHAR; 反查表由随机盐消解)
            salt = infra::HexEncode(infra::RandomSalt());
            password_hash = infra::HashPassword(salt, req.password());
            rec.player_id = store_->CreatePlayer(req.account(), salt, password_hash);
            rec.level = 1;
            rec.exp = 0;
            rec.password_salt = salt;
            rec.password_hash = password_hash;
        } else {
            // 已建档: 常数时间比对凭据, 拒绝暴露"账号存在/密码错"差异
            const std::string calc = infra::HashPassword(salt, req.password());
            if (!infra::ConstantTimeEquals(calc, password_hash)) {
                resp.set_success(false);
                resp.set_error_msg("Auth failed: invalid password");
                spdlog::warn("[GameLogic] Login rejected [account={}, reason=bad_password]", req.account());
                return resp;
            }
        }
        WritePlayerCache(req.account(), rec);
        store_->TouchLastLogin(req.account());

        uint64_t player_id = rec.player_id;
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
        spdlog::info("[GameLogic] Login ok [account={}, level={}, items={}]",
                     req.account(), data->level(), resp.inventory_size());
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

std::vector<data::OwnedCharacter> GameLogic::LoadOwnedCharacters(const std::string& account) {
    return store_->LoadOwnedCharacters(account);
}

void GameLogic::RewriteOwnedCharacters(const std::string& account,
                                       const std::vector<data::OwnedCharacter>& chars) {
    store_->RewriteOwnedCharacters(account, chars);
}

void GameLogic::UpsertOwnedCharacter(const std::string& account,
                                     const data::OwnedCharacter& c) {
    store_->UpsertOwnedCharacter(account, c);
}

void GameLogic::SetOwnedCharacterActive(const std::string& account,
                                        const std::string& character_tag) {
    store_->SetOwnedCharacterActive(account, character_tag);
}

std::vector<data::TeamSlot> GameLogic::LoadTeamSlots(const std::string& account) {
    return store_->LoadTeamSlots(account);
}

void GameLogic::RewriteTeamSlots(const std::string& account,
                                 const std::vector<data::TeamSlot>& slots) {
    store_->RewriteTeamSlots(account, slots);
}

void GameLogic::SetTeamSlotActive(const std::string& account,
                                  const std::string& character_tag) {
    store_->SetTeamSlotActive(account, character_tag);
}

void GameLogic::SavePlayerPosition(const std::string& account,
                                   const storage::PlayerPosition& p) {
    storage::PlayerPosition copy = p;
    copy.updated_at = infra::NowMs();
    store_->SavePlayerPosition(account, copy);
}

void GameLogic::LoadPlayerPosition(const std::string& account, storage::PlayerPosition* out) {
    store_->LoadPlayerPosition(account, out);
}

void GameLogic::SaveCharacterHp(const std::string& account, const std::string& character_tag,
                                float max_hp, float current_hp) {
    store_->SaveCharacterHp(account, character_tag, max_hp, current_hp);
}

void GameLogic::LoadCharacterHp(const std::string& account,
                                std::vector<storage::CharacterHpRow>& out) {
    store_->LoadCharacterHp(account, out);
}

void GameLogic::FlushPersistence() {
    store_->FlushAsyncWrites();
}

} // namespace logic
} // namespace game
