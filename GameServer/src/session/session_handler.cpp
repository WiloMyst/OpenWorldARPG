#include "game/session/session_handler.h"
#include "game/session/i_session.h"
#include "game/session/session_manager.h"
#include "game/logic/game_logic.h"
#include "game/world/world_manager.h"
#include "game/combat/combat_manager.h"
#include "game/infra/time_utils.h"
#include <chrono>
#include <spdlog/spdlog.h>

namespace game {
namespace session {

SessionHandler::SessionHandler(logic::GameLogic* logic, SessionManager* session_manager,
                               world::WorldManager* world, combat::CombatManager* combat)
    : logic_(logic), session_manager_(session_manager), world_(world), combat_(combat) {}

void SessionHandler::ResolveSpawnOrDefault(const std::string& account, float out[4]) {
    out[0] = world_->spawn_x();
    out[1] = world_->spawn_y();
    out[2] = world_->spawn_z();
    out[3] = world_->spawn_yaw();
    try {
        game::storage::PlayerPosition saved;
        logic_->LoadPlayerPosition(account, &saved);
        if (saved.updated_at > 0) {
            out[0] = saved.x;
            out[1] = saved.y;
            out[2] = saved.z;
            out[3] = saved.yaw;
            spdlog::info("[Session] restore spawn pos [account={}, pos=({:.1f},{:.1f},{:.1f})]",
                         account, saved.x, saved.y, saved.z);
        }
    } catch (const std::exception& e) {
        spdlog::warn("[Session] position restore failed, use default spawn [account={}, err={}]",
                     account, e.what());
    }
}

bool SessionHandler::PrepareLogin(const game::LoginRequest& req, std::string* account,
                                  game::LoginResponse* resp) {
    *resp = logic_->HandleLogin(req, account);
    if (!resp->success()) return false;
    const std::string acc = *account;

    // 服务器权威出生点 (存档恢复或默认), 供客户端定位与后续 World 挂载进入世界
    float spawn[4];
    ResolveSpawnOrDefault(acc, spawn);
    resp->set_spawn_x(spawn[0]);
    resp->set_spawn_y(spawn[1]);
    resp->set_spawn_z(spawn[2]);
    resp->set_spawn_yaw(spawn[3]);

    // 建立会话战斗态 (服务器权威, 按账号); 角色实体由客户端 RegisterCharacter 上报
    combat_->RegisterPlayer(acc);

    // 冲库上一会话未落地的异步写 (read-your-writes), 再读档保证读到最新状态
    try {
        logic_->FlushPersistence();
    } catch (const std::exception& e) {
        spdlog::warn("[Session] persistence flush failed [account={}, err={}]", acc, e.what());
    }

    // 角色存档闭环: 恢复拥有角色 (DB owned_characters -> CombatManager) + active;
    // 新玩家首次登录无存档: 由服务器从 initial_archive.yaml 直接播种拥有全集 + 配队
    // (方案 B: 镜像客户端 UInitialArchiveData, 不依赖客户端逐个生成角色上报).
    // 登录响应携带权威快照 (拥有角色 + 配队 + 上场 index), 客户端据此初始化本地缓存,
    // 不再依赖本地 UInitialArchiveData.
    try {
        auto owned = logic_->LoadOwnedCharacters(acc);
        if (owned.empty()) {
            const auto& archive = logic_->GetInitialArchive();
            if (!archive.owned_characters.empty()) {
                logic_->RewriteOwnedCharacters(acc, archive.owned_characters);
                logic_->RewriteTeamSlots(acc, archive.team_slots);
                owned = archive.owned_characters;
                spdlog::info("[Session] new player archive seeded [account={}, owned={}, team={}]",
                             acc, archive.owned_characters.size(), archive.team_slots.size());
            }
        }
        if (!owned.empty()) {
            combat_->RestoreOwnedCharacters(acc, owned);
        }

        for (const auto& oc : owned) {
            auto* p = resp->add_owned_characters();
            p->set_character_tag(oc.character_tag);
            p->set_level(oc.level);
            p->set_exp(oc.exp);
            p->set_is_active(oc.is_active);
            p->set_acquired_time(oc.acquired_time);
        }
        for (const auto& ts : logic_->LoadTeamSlots(acc)) {
            auto* p = resp->add_team_slots();
            p->set_slot_index(ts.slot_index);
            p->set_character_tag(ts.character_tag);
            p->set_is_active(ts.is_active);
        }
    } catch (const std::exception& e) {
        spdlog::warn("[Session] owned characters load failed [account={}, err={}]", acc, e.what());
    }

    // HP 存档恢复: 读档各角色血线注入战斗会话, 客户端 RegisterCharacter 建档时覆盖初始 HP
    try {
        std::vector<game::storage::CharacterHpRow> hp;
        logic_->LoadCharacterHp(acc, hp);
        if (!hp.empty()) {
            std::vector<combat::CombatManager::HpRestoreEntry> entries;
            entries.reserve(hp.size());
            for (const auto& r : hp) {
                combat::CombatManager::HpRestoreEntry e;
                e.character_tag = r.character_tag;
                e.max_hp = r.max_hp;
                e.current_hp = r.current_hp;
                entries.push_back(std::move(e));
            }
            combat_->RestoreCharacterHp(acc, entries);
        }
    } catch (const std::exception& e) {
        spdlog::warn("[Session] character hp load failed [account={}, err={}]", acc, e.what());
    }

    // 登记待挂载令牌 + 登录上下文: 客户端开 World 流凭令牌兑换在线会话
    SessionManager::PendingLogin pending;
    pending.account = acc;
    pending.player_id = resp->player_id();
    pending.spawn[0] = spawn[0];
    pending.spawn[1] = spawn[1];
    pending.spawn[2] = spawn[2];
    pending.spawn[3] = spawn[3];
    session_manager_->RegisterPendingLogin(resp->session_token(), pending);
    return true;
}

void SessionHandler::EnterWorld(const std::shared_ptr<ISession>& session,
                                const std::string& account, uint64_t player_id,
                                const float spawn[4]) {
    // 先消费重连保留窗口: 账号断线且在窗口内重连时, 消费标志返回 true,
    // 若世界实体仍存则走 PlayerResume 无缝接管 (不重复 enter/leave 广播、不重刷怪);
    // 未消费 = 全新登录
    const bool resuming = session_manager_->TakeReconnect(account);

    // 绑定账号 (内部置 ONLINE)
    session_manager_->BindAccount(account, session, "");

    if (resuming) {
        float x, y, z, yaw;
        if (world_->PlayerResume(account, session->session_id(), &x, &y, &z, &yaw)) {
            spdlog::info("[Session] Player resumed into world [account={}]", account);
            return;  // 已接管; combat/对话流在断线窗口内被保留, 无需重建
        }
        // 窗口内竞态 (窗口刚过期被 CheckLoop 释放实体): 退化为全新进入
        spdlog::warn("[Session] Resume missed, fallback to fresh enter [account={}]", account);
    }

    // 全新进入世界 (AOI 登记与广播)
    world_->PlayerEnter(account, player_id, session->session_id(), spawn);
}

game::LogoutResponse SessionHandler::HandleLogout(const std::string& account) {
    game::LogoutResponse resp;
    spdlog::info("[Session] Player logout [account={}]", account);
    logic_->HandleLogout(account);
    combat_->UnregisterPlayer(account);
    // 冲库本次会话未落地的异步写 (位置/HP/角色), 保证登出即落盘
    try {
        logic_->FlushPersistence();
    } catch (const std::exception& e) {
        spdlog::warn("[Session] logout persistence flush failed [account={}, err={}]",
                     account, e.what());
    }
    // 关闭账号在线 World 流 (OnSessionClosed 解绑账号+移除世界实体+令牌失效)
    session_manager_->CloseByAccount(account);
    resp.set_success(true);
    return resp;
}

game::RegisterCharacterResponse SessionHandler::HandleRegisterCharacter(
    const std::string& account, const game::RegisterCharacterRequest& req) {
    game::RegisterCharacterResponse resp;
    combat_->RegisterCharacter(account, req.character_tag(), req.max_hp());

    // 认账落库: 上报的角色若是"会话内已有、但不在 DB 拥有集合"即为新获得 -> 落库建档.
    // 用 IsDbOwned 而非 IsRestored: 否则老玩家会话已恢复, 新获得角色永不被持久化.
    try {
        if (combat_->HasCharacter(account, req.character_tag()) &&
            !combat_->IsDbOwned(account, req.character_tag())) {
            game::data::OwnedCharacter oc;
            oc.character_tag = req.character_tag();
            oc.level = 1;
            oc.exp = 0;
            oc.is_active = false;
            oc.acquired_time = static_cast<int64_t>(infra::NowMs());
            logic_->UpsertOwnedCharacter(account, oc);
        }
    } catch (const std::exception& e) {
        spdlog::warn("[Session] owned character upsert failed [account={}, tag={}, err={}]",
                     account, req.character_tag(), e.what());
    }

    if (combat_->HasCharacter(account, req.character_tag())) {
        resp.set_success(true);
        resp.set_max_hp(req.max_hp());
    } else {
        resp.set_success(false);
        resp.set_error_msg("Character not registered");
    }
    return resp;
}

game::SetActiveCharacterResponse SessionHandler::HandleSetActiveCharacter(
    const std::string& account, const game::SetActiveCharacterRequest& req) {
    game::SetActiveCharacterResponse resp;
    combat_->SetActiveCharacter(account, req.character_tag());

    // 切人落库: 更新 DB 的 active 标记 (仅对已拥有角色生效; 幂等)
    try {
        if (combat_->HasCharacter(account, req.character_tag())) {
            logic_->SetOwnedCharacterActive(account, req.character_tag());
            logic_->SetTeamSlotActive(account, req.character_tag());
        }
    } catch (const std::exception& e) {
        spdlog::warn("[Session] owned character active persist failed [account={}, tag={}, err={}]",
                     account, req.character_tag(), e.what());
    }

    resp.set_success(combat_->HasCharacter(account, req.character_tag()));
    if (resp.success()) resp.set_character_tag(req.character_tag());
    else resp.set_error_msg("Character not registered");
    return resp;
}

game::EnemySpawnResponse SessionHandler::HandleEnemySpawnRequest(
    const std::string& account, const game::EnemySpawnRequest& req) {
    return combat_->HandleSpawnRequest(req);
}

game::InventoryOpResponse SessionHandler::HandleInventoryOp(
    const std::string& account, const game::InventoryOpRequest& req) {
    return logic_->HandleInventoryOp(account, req);
}

game::DialogueAuthResult SessionHandler::HandleDialogueAuth(
    const std::string& account, const game::DialogueAuthRequest& req) {
    return logic_->HandleDialogueAuth(account, req);
}

void SessionHandler::HandleWorldMessage(const std::shared_ptr<ISession>& session,
                                        const game::ClientWorld& msg) {
    if (session->state() != SessionState::ONLINE) {
        SendError(session, msg.sequence(), 401, "Not authenticated");
        return;
    }
    const std::string account = session->account();

    switch (msg.payload_case()) {
        case game::ClientWorld::kHeartbeat: {
            game::ServerMessage reply;
            reply.set_ack_sequence(msg.sequence());
            *reply.mutable_heartbeat() = logic_->HandleHeartbeat(account, msg.heartbeat());
            session->EnqueueWrite(reply);
            break;
        }
        case game::ClientWorld::kMovement: {
            // 移动上报: 服务器权威校验 + AOI 广播, 无回执 (拒绝时由 PositionCorrection 推送)
            const auto& m = msg.movement();
            world_->PlayerMove(account, session->session_id(),
                               m.x(), m.y(), m.z(), m.yaw(), m.client_timestamp());
            break;
        }
        case game::ClientWorld::kDamageIntent: {
            // 攻击者身份由服务器按会话归属解析, 不信任客户端自报 (反作弊)
            combat_->HandleDamageIntent(world_->GetPlayerId(account), msg.damage_intent());
            break;
        }
        case game::ClientWorld::kEnemyAttackIntent: {
            combat_->HandleEnemyAttackIntent(account, msg.enemy_attack_intent());
            break;
        }
        default: {
            SendError(session, msg.sequence(), 400, "Unknown or empty world message");
            break;
        }
    }
}

void SessionHandler::SendError(const std::shared_ptr<ISession>& session, uint64_t seq, int code,
                               const std::string& message) {
    game::ServerMessage reply;
    reply.set_ack_sequence(seq);
    reply.mutable_error()->set_code(code);
    reply.mutable_error()->set_message(message);
    session->EnqueueWrite(reply);
}

} // namespace session
} // namespace game