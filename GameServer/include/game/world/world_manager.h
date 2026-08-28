#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "game.pb.h"
#include "game/world/aoi_manager.h"

namespace game {
    namespace infra { struct AppConfig; }
    namespace session { class SessionManager; }
}

namespace game {
namespace world {

// 世界管理器: 服务器权威位置 + 移动校验 + AOI 广播 (Phase 1 状态同步)
// 校验规则: 速度上限 / 单次位移(防瞬移) / 时间戳单调与时钟偏差
// 线程模型: mtx_ 保护, 移动上报来自 worker 线程, 进入/离开来自会话线程
class WorldManager {
public:
    explicit WorldManager(const infra::AppConfig& config);

    void SetSessionManager(session::SessionManager* sm) { session_manager_ = sm; }

    // 位置异步存档回调 (由装配层注入, 接 GameLogic::SavePlayerPosition):
    // 高频位置经节流异步落库, 不阻塞移动模拟线程. 回调内禁止再取 world 锁.
    using PositionPersistCallback =
        std::function<void(const std::string& account, float x, float y, float z, float yaw)>;
    void SetPositionPersistCallback(PositionPersistCallback cb) {
        position_persist_cb_ = std::move(cb);
    }

    // 玩家登录进入世界; 同账号重复进入先移除旧实体 (重复登录踢旧).
    // spawn_pos 可选: 非空则用存档位置作为出生点恢复 (登录读档), 否则用服务器配置出生点.
    void PlayerEnter(const std::string& account, uint64_t player_id, uint64_t session_id,
                     const float spawn_pos[4] = nullptr);

    // 应用存档恢复的出生点 (登录时) / 移动校验, 见 PlayerEnter
    bool PlayerMove(const std::string& account, uint64_t session_id,
                    float x, float y, float z, float yaw, int64_t client_ts);

    // 玩家离开世界 (登出/断线/被踢); session_id 不匹配则忽略 (防重复登录竞态).
    // 离开时把最终权威位置异步落库, 保证退出即存档.
    void PlayerLeave(const std::string& account, uint64_t session_id);

    bool PlayerResume(const std::string& account, uint64_t new_session_id,
                      float* x, float* y, float* z, float* yaw);

    float spawn_x() const { return spawn_x_; }
    float spawn_y() const { return spawn_y_; }
    float spawn_z() const { return spawn_z_; }
    float spawn_yaw() const { return spawn_yaw_; }

    uint64_t GetPlayerId(const std::string& account);

private:
    struct Entity {
        uint64_t player_id = 0;
        uint64_t session_id = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
        int64_t last_update_ms = 0;
        int64_t last_client_ts = 0;
        bool has_baseline = false;
    };

    void SendEnter(const std::string& observer, const Entity& subject);
    void SendLeave(const std::string& observer, uint64_t subject_player_id);
    void SendMove(const std::string& observer, const Entity& subject);
    void SendCorrection(const std::string& account, const Entity& subject);

    std::mutex mtx_;
    session::SessionManager* session_manager_ = nullptr;

    float grid_size_;
    int view_radius_cells_;
    float max_move_speed_;
    float speed_tolerance_;
    float teleport_threshold_;
    int64_t max_timestamp_skew_ms_;
    float spawn_x_, spawn_y_, spawn_z_, spawn_yaw_;

    AoiManager aoi_;
    std::unordered_map<std::string, Entity> entities_;

    // 位置异步存档: 节流间隔 + 每账号上次落库时刻 (节流防队列洪峰, 异步写者已解阻塞)
    PositionPersistCallback position_persist_cb_;
    std::unordered_map<std::string, int64_t> last_pos_persist_ms_;
    static constexpr int64_t kPosPersistMs = 500;
};

} // namespace world
} // namespace game
