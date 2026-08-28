#include "game/world/world_manager.h"
#include "game/infra/config_manager.h"
#include "game/infra/time_utils.h"
#include "game/session/session_manager.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace world {

WorldManager::WorldManager(const infra::AppConfig& config)
    : grid_size_(config.world.grid_size),
      view_radius_cells_(config.world.view_radius_cells),
      max_move_speed_(config.world.max_move_speed),
      speed_tolerance_(config.world.speed_tolerance),
      teleport_threshold_(config.world.teleport_threshold),
      max_timestamp_skew_ms_(config.world.max_timestamp_skew_ms),
      spawn_x_(config.world.spawn_x),
      spawn_y_(config.world.spawn_y),
      spawn_z_(config.world.spawn_z),
      spawn_yaw_(config.world.spawn_yaw),
      aoi_(config.world.grid_size, config.world.view_radius_cells) {}

void WorldManager::PlayerEnter(const std::string& account, uint64_t player_id,
                               uint64_t session_id, const float spawn_pos[4]) {
    std::lock_guard<std::mutex> lock(mtx_);

    // 重复登录踢旧: 先移除旧实体, 保证 AOI 状态干净
    auto old = entities_.find(account);
    if (old != entities_.end()) {
        const auto left = aoi_.RemoveEntity(account);
        for (const auto& s : left) {
            if (s == account) continue;
            SendLeave(s, old->second.player_id);
        }
        entities_.erase(old);
    }

    // 出生点: 登录读档时用存档位置恢复, 否则用服务器配置出生点
    Entity e;
    e.player_id = player_id;
    e.session_id = session_id;
    e.x = spawn_pos ? spawn_pos[0] : spawn_x_;
    e.y = spawn_pos ? spawn_pos[1] : spawn_y_;
    e.z = spawn_pos ? spawn_pos[2] : spawn_z_;
    e.yaw = spawn_pos ? spawn_pos[3] : spawn_yaw_;
    e.last_update_ms = infra::NowMs();
    e.has_baseline = false;
    entities_[account] = e;

    const auto entered = aoi_.AddEntity(account, e.x, e.y);
    for (const auto& s : entered) {
        if (s == account) continue;
        auto it = entities_.find(s);
        if (it == entities_.end()) continue;
        SendEnter(account, it->second);  // 我看到了 s
        SendEnter(s, e);                 // s 看到了我
    }
    spdlog::info("[World] Player entered [account={}, player_id={}, pos=({:.1f},{:.1f},{:.1f})]",
                 account, player_id, e.x, e.y, e.z);
}

bool WorldManager::PlayerMove(const std::string& account, uint64_t session_id,
                              float x, float y, float z, float yaw, int64_t client_ts) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entities_.find(account);
    if (it == entities_.end() || it->second.session_id != session_id) {
        return false;  // 未在线或会话不匹配
    }

    Entity& e = it->second;
    const int64_t now = infra::NowMs();

    // 时间戳合理性: 单调递增 + 与服务器时钟偏差容忍
    if (client_ts > 0) {
        if (e.has_baseline && client_ts < e.last_client_ts) {
            spdlog::warn("[World] Movement rejected [account={}, reason=out_of_order_ts]",
                         account);
            SendCorrection(account, e);
            return false;
        }
        if (std::llabs(client_ts - now) > max_timestamp_skew_ms_) {
            spdlog::warn("[World] Movement rejected [account={}, reason=clock_skew]", account);
            SendCorrection(account, e);
            return false;
        }
    }

    const float dx = x - e.x, dy = y - e.y, dz = z - e.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (e.has_baseline) {
        // 单次位移硬上限 (防瞬移): 与时间无关, 超过直接判定作弊
        if (dist > teleport_threshold_) {
            spdlog::warn("[World] Movement rejected [account={}, reason=teleport, "
                         "dist={:.1f}m, cap={:.1f}m]",
                         account, dist, teleport_threshold_);
            SendCorrection(account, e);
            return false;
        }
        // 速率上限 (防超速): 基于上报间隔估算瞬时速度; dt 下限 50ms 防微小间隔误判
        const float dt = std::max(0.05f, static_cast<float>(now - e.last_update_ms)) / 1000.0f;
        const float speed_limit = max_move_speed_ * speed_tolerance_ * dt;
        if (dist > speed_limit) {
            spdlog::warn("[World] Movement rejected [account={}, reason=overspeed, "
                         "speed={:.1f}m/s, limit={:.1f}m/s]",
                         account, dist / dt, speed_limit / dt);
            SendCorrection(account, e);
            return false;
        }
    }

    e.x = x;
    e.y = y;
    e.z = z;
    e.yaw = yaw;
    e.last_update_ms = now;
    e.last_client_ts = client_ts;
    e.has_baseline = true;

    const auto change = aoi_.MoveEntity(account, x, y);
    for (const auto& s : change.left) {
        if (s == account) continue;
        auto it2 = entities_.find(s);
        if (it2 == entities_.end()) continue;
        SendLeave(account, it2->second.player_id);  // 我再也看不到 s
        SendLeave(s, e.player_id);                  // s 再也看不到我
    }
    for (const auto& s : change.entered) {
        if (s == account) continue;
        auto it2 = entities_.find(s);
        if (it2 == entities_.end()) continue;
        SendEnter(account, it2->second);  // 我新看到 s
        SendEnter(s, e);                  // s 新看到我
    }
    for (const auto& s : change.stayed) {
        if (s == account) continue;
        SendMove(s, e);  // s 仍看到我, 更新位置
    }

    // 位置异步存档 (节流): 达到间隔才入队一次, 异步写者落库, 不阻塞移动模拟
    if (position_persist_cb_ &&
        now - last_pos_persist_ms_[account] >= kPosPersistMs) {
        last_pos_persist_ms_[account] = now;
        position_persist_cb_(account, e.x, e.y, e.z, e.yaw);
    }
    return true;
}

// 查询在线玩家在世界的服务器权威 player_id (供战斗广播标注攻击者等); 未在线返回 0
uint64_t WorldManager::GetPlayerId(const std::string& account) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entities_.find(account);
    return it == entities_.end() ? 0 : it->second.player_id;
}

void WorldManager::PlayerLeave(const std::string& account, uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entities_.find(account);
    if (it == entities_.end()) return;
    if (it->second.session_id != session_id) return;  // 旧会话的迟到离开, 忽略

    // 离开即存档: 先记录最终权威位置, 实体删除后异步落库 (退出即存档, 不阻塞登出)
    const float px = it->second.x, py = it->second.y;
    const float pz = it->second.z, pyaw = it->second.yaw;

    const auto left = aoi_.RemoveEntity(account);
    for (const auto& s : left) {
        if (s == account) continue;
        SendLeave(s, it->second.player_id);
    }
    spdlog::info("[World] Player left [account={}]", account);
    entities_.erase(it);

    if (position_persist_cb_) {
        last_pos_persist_ms_[account] = infra::NowMs();
        position_persist_cb_(account, px, py, pz, pyaw);
    }
}

bool WorldManager::PlayerResume(const std::string& account, uint64_t new_session_id,
                                float* x, float* y, float* z, float* yaw) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entities_.find(account);
    if (it == entities_.end()) return false;
    it->second.session_id = new_session_id;
    *x = it->second.x;
    *y = it->second.y;
    *z = it->second.z;
    *yaw = it->second.yaw;
    spdlog::info("[World] Player resumed [account={}, session_id={}, pos=({:.1f},{:.1f},{:.1f})]",
                 account, new_session_id, it->second.x, it->second.y, it->second.z);
    return true;
}

void WorldManager::SendEnter(const std::string& observer, const Entity& subject) {
    game::ServerMessage msg;
    msg.set_ack_sequence(0);
    auto* enter = msg.mutable_player_enter();
    enter->set_player_id(subject.player_id);
    enter->set_x(subject.x);
    enter->set_y(subject.y);
    enter->set_z(subject.z);
    enter->set_yaw(subject.yaw);
    if (session_manager_) session_manager_->SendToAccount(observer, msg);
}

void WorldManager::SendLeave(const std::string& observer, uint64_t subject_player_id) {
    game::ServerMessage msg;
    msg.set_ack_sequence(0);
    msg.mutable_player_leave()->set_player_id(subject_player_id);
    if (session_manager_) session_manager_->SendToAccount(observer, msg);
}

void WorldManager::SendMove(const std::string& observer, const Entity& subject) {
    game::ServerMessage msg;
    msg.set_ack_sequence(0);
    auto* move = msg.mutable_player_move();
    move->set_player_id(subject.player_id);
    move->set_x(subject.x);
    move->set_y(subject.y);
    move->set_z(subject.z);
    move->set_yaw(subject.yaw);
    if (session_manager_) session_manager_->SendToAccount(observer, msg);
}

void WorldManager::SendCorrection(const std::string& account, const Entity& subject) {
    game::ServerMessage msg;
    msg.set_ack_sequence(0);
    auto* corr = msg.mutable_position_correction();
    corr->set_x(subject.x);
    corr->set_y(subject.y);
    corr->set_z(subject.z);
    corr->set_yaw(subject.yaw);
    if (session_manager_) session_manager_->SendToAccount(account, msg);
}

} // namespace world
} // namespace game
