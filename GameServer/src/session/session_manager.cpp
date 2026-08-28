#include "game/session/session_manager.h"
#include "game/session/i_session.h"
#include "game/infra/config_manager.h"
#include "game/infra/time_utils.h"
#include "game/world/world_manager.h"
#include "game/combat/combat_manager.h"
#include "game.pb.h"
#include <chrono>
#include <spdlog/spdlog.h>

namespace game {
namespace session {

SessionManager::SessionManager(const infra::AppConfig& config)
    : heartbeat_timeout_ms_(static_cast<int64_t>(config.heartbeat_timeout_sec) * 1000),
      login_timeout_ms_(static_cast<int64_t>(config.login_timeout_sec) * 1000),
      resume_grace_ms_(10000),   // 断线重连保留时长已随自定义 TCP 移除; 扫描骨架保留兼容但 gRPC 不触发
      check_interval_ms_(static_cast<int64_t>(config.session_check_interval_sec) * 1000) {}

SessionManager::~SessionManager() {
    Stop();
}

void SessionManager::Start() {
    stopped_.store(false);
    check_thread_ = std::thread(&SessionManager::CheckLoop, this);
    spdlog::info("[SessionManager] Started [heartbeat_timeout={}s, login_timeout={}s, "
                 "resume_grace={}s]",
                 heartbeat_timeout_ms_ / 1000, login_timeout_ms_ / 1000,
                 resume_grace_ms_ / 1000);
}

void SessionManager::Stop() {
    if (stopped_.exchange(true)) return;
    if (check_thread_.joinable()) check_thread_.join();
    spdlog::info("[SessionManager] Stopped");
}

void SessionManager::RegisterSession(std::shared_ptr<ISession> session) {
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_[session->session_id()] = session;
}

void SessionManager::RegisterPendingLogin(const std::string& token,
                                          const PendingLogin& ctx) {
    std::lock_guard<std::mutex> lock(mtx_);
    login_pending_[token] = ctx;
}

bool SessionManager::ClaimLogin(const std::string& token, PendingLogin* out) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = login_pending_.find(token);
    if (it == login_pending_.end()) return false;
    // 非消费式: 令牌保留供后续 unary 以 Bearer 鉴权; 重复挂载由 BindAccount 踢旧兜底
    *out = it->second;
    return true;
}

bool SessionManager::AuthenticateUnary(const std::string& token, std::string* account) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = login_pending_.find(token);
    if (it == login_pending_.end()) return false;
    // 令牌须已挂载在线 World 流 (account_map_ 命中), 才允许 unary 业务操作
    auto ait = account_map_.find(it->second.account);
    if (ait == account_map_.end()) return false;
    auto session = ait->second.lock();
    if (!session || session->state() != SessionState::ONLINE) return false;
    *account = it->second.account;
    return true;
}

void SessionManager::CloseByAccount(const std::string& account) {
    std::shared_ptr<ISession> session;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = account_map_.find(account);
        if (it != account_map_.end()) session = it->second.lock();
    }
    if (session) session->RequestClose("logout", false);
}

bool SessionManager::BindAccount(const std::string& account,
                                 std::shared_ptr<ISession> session,
                                 const std::string& resume_token) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = account_map_.find(account);
    if (it != account_map_.end()) {
        if (auto old = it->second.lock()) {
            if (old.get() == session.get()) return false;

            // 断线重连 (曾服务于自定义 TCP): 旧会话处于重连等待且令牌匹配 -> 移交账号绑定.
            // 现仅 gRPC 通道且 resumable() 恒 false, 该分支不会触发, 保留扫描骨架兼容
            if (old->state() == SessionState::RECONNECTING &&
                !resume_token.empty() && resume_token == old->resume_token()) {
                spdlog::info("[SessionManager] Resume [account={}, old_id={}, new_id={}]",
                             account, old->session_id(), session->session_id());
                resume_hold_.erase(account);
                account_map_[account] = session;
                session->MarkOnline(account);
                old->RequestClose("resumed", false);
                return true;
            }

            spdlog::warn("[SessionManager] Duplicate login [account={}], kicking old session [id={}]",
                         account, old->session_id());
            old->RequestClose("duplicate_login", true);
        }
    }
    account_map_[account] = session;
    session->MarkOnline(account);
    return false;
}

void SessionManager::OnSessionClosed(std::shared_ptr<ISession> session) {
    std::string account;
    bool should_leave = false;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        sessions_.erase(session->session_id());

        account = session->account();
        if (account.empty()) return;

        auto it = account_map_.find(account);
        if (it != account_map_.end()) {
            auto bound = it->second.lock();
            if (!bound || bound.get() != session.get()) return;

            // 断线重连等待 (曾服务于自定义 TCP): gRPC 会话 resumable() 恒 false 不会触发
            if (session->state() == SessionState::ONLINE && session->resumable() &&
                !session->clean_logout()) {
                session->MarkReconnecting(resume_grace_ms_);
                resume_hold_[account] = session;
                spdlog::info("[SessionManager] Player reconnecting [account={}, id={}, grace={}ms]",
                             account, session->session_id(), resume_grace_ms_);
                return;
            }

            account_map_.erase(it);
            resume_hold_.erase(account);
            // 清理该账号的待挂载令牌 (注销后令牌失效, 后续 unary/Bearer 鉴权拒绝)
            for (auto pit = login_pending_.begin(); pit != login_pending_.end();) {
                if (pit->second.account == account) {
                    pit = login_pending_.erase(pit);
                } else {
                    ++pit;
                }
            }
            spdlog::info("[SessionManager] Player offline [account={}]", account);
            should_leave = true;
        }
    }

    // 锁外通知世界移除玩家 (AOI 清理): 保持 世界锁 -> 会话锁 的全局锁序, 避免 ABBA 死锁
    if (should_leave && world_) world_->PlayerLeave(account, session->session_id());

    // 锁外清理会话战斗态 (登出/踢出/断线/心跳超时统一出口);
    // CombatManager 在"最后玩家离开"时重置世界刷怪状态, 下次进入重新刷怪
    if (should_leave && combat_) combat_->UnregisterPlayer(account);

    // 锁外联动: 通知 VHServer 吊销该账号对话流 (尽力而为, 不阻塞会话收尾)
    if (should_leave && dialogue_revoke_cb_) dialogue_revoke_cb_(account);
}

void SessionManager::SendToAccount(const std::string& account,
                                   const game::ServerMessage& msg) {
    std::shared_ptr<ISession> session;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = account_map_.find(account);
        if (it == account_map_.end()) return;
        session = it->second.lock();
    }
    if (session) session->EnqueueWrite(msg);
}

void SessionManager::BroadcastOnline(const game::ServerMessage& msg) {
    std::vector<std::shared_ptr<ISession>> online;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        online.reserve(account_map_.size());
        for (const auto& [account, weak] : account_map_) {
            if (auto session = weak.lock()) online.push_back(session);
        }
    }
    for (const auto& session : online) session->EnqueueWrite(msg);
}

uint64_t SessionManager::GetSessionId(const std::string& account) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = account_map_.find(account);
    if (it == account_map_.end()) return 0;
    if (auto session = it->second.lock()) return session->session_id();
    return 0;
}

void SessionManager::CheckLoop() {
    struct KickItem {
        std::shared_ptr<ISession> session;
        std::string reason;
    };

    while (!stopped_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms_));
        if (stopped_.load()) break;

        std::vector<KickItem> to_kick;
        const int64_t now = infra::NowMs();

        {
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto it = sessions_.begin(); it != sessions_.end();) {
                auto session = it->second.lock();
                if (!session) {
                    it = sessions_.erase(it);
                    continue;
                }

                if (session->state() == SessionState::WAIT_LOGIN &&
                    now - session->connect_ms() > login_timeout_ms_) {
                    to_kick.push_back({session, "login_timeout"});
                } else if (session->state() == SessionState::ONLINE &&
                           now - session->last_active_ms() > heartbeat_timeout_ms_) {
                    to_kick.push_back({session, "heartbeat_timeout"});
                }
                ++it;
            }

            // 重连等待超时: 清理保留的会话, 玩家正式离线
            for (auto it = resume_hold_.begin(); it != resume_hold_.end();) {
                if (it->second->resume_expired()) {
                    to_kick.push_back({it->second, "resume_timeout"});
                    it = resume_hold_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 踢出动作在锁外执行, 避免与 RequestClose 内部锁形成嵌套
        for (const auto& item : to_kick) {
            spdlog::warn("[SessionManager] Kick [id={}, account={}, reason={}]",
                         item.session->session_id(), item.session->account(), item.reason);
            item.session->RequestClose(item.reason,
                                       item.reason != "resume_timeout");
        }
    }
}

} // namespace session
} // namespace game