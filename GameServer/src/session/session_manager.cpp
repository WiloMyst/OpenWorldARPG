#include "game/session/session_manager.h"
#include "game/core/player_session.h"
#include "game/infra/config_manager.hpp"
#include "game/infra/time_utils.h"
#include <chrono>
#include <spdlog/spdlog.h>

namespace game {
namespace session {

SessionManager::SessionManager(const infra::AppConfig& config)
    : heartbeat_timeout_ms_(static_cast<int64_t>(config.heartbeat_timeout_sec) * 1000),
      login_timeout_ms_(static_cast<int64_t>(config.login_timeout_sec) * 1000),
      check_interval_ms_(static_cast<int64_t>(config.session_check_interval_sec) * 1000) {}

SessionManager::~SessionManager() {
    Stop();
}

void SessionManager::Start() {
    stopped_.store(false);
    check_thread_ = std::thread(&SessionManager::CheckLoop, this);
    spdlog::info("[SessionManager] Started [heartbeat_timeout={}s, login_timeout={}s]",
                 heartbeat_timeout_ms_ / 1000, login_timeout_ms_ / 1000);
}

void SessionManager::Stop() {
    if (stopped_.exchange(true)) return;
    if (check_thread_.joinable()) check_thread_.join();
    spdlog::info("[SessionManager] Stopped");
}

void SessionManager::RegisterSession(std::shared_ptr<core::PlayerSession> session) {
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_[session->session_id()] = session;
}

bool SessionManager::BindAccount(const std::string& account,
                                 std::shared_ptr<core::PlayerSession> session) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = account_map_.find(account);
    if (it != account_map_.end()) {
        if (auto old = it->second.lock()) {
            if (old.get() == session.get()) return true;
            spdlog::warn("[SessionManager] Duplicate login [account={}], kicking old session [id={}]",
                         account, old->session_id());
            old->RequestClose("duplicate_login", true);
        }
    }
    account_map_[account] = session;
    session->MarkOnline(account);
    return true;
}

void SessionManager::OnSessionClosed(std::shared_ptr<core::PlayerSession> session) {
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_.erase(session->session_id());

    const std::string account = session->account();
    if (account.empty()) return;

    auto it = account_map_.find(account);
    if (it == account_map_.end()) return;
    if (auto bound = it->second.lock(); bound && bound.get() == session.get()) {
        account_map_.erase(it);
        spdlog::info("[SessionManager] Player offline [account={}]", account);
    }
}

void SessionManager::CheckLoop() {
    struct KickItem {
        std::shared_ptr<core::PlayerSession> session;
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

                if (session->state() == core::PlayerSession::State::WAIT_LOGIN &&
                    now - session->connect_ms() > login_timeout_ms_) {
                    to_kick.push_back({session, "login_timeout"});
                } else if (session->state() == core::PlayerSession::State::ONLINE &&
                           now - session->last_active_ms() > heartbeat_timeout_ms_) {
                    to_kick.push_back({session, "heartbeat_timeout"});
                }
                ++it;
            }
        }

        // 踢出动作在锁外执行, 避免与 RequestClose 内部锁形成嵌套
        for (const auto& item : to_kick) {
            spdlog::warn("[SessionManager] Kick [id={}, account={}, reason={}]",
                         item.session->session_id(), item.session->account(), item.reason);
            item.session->RequestClose(item.reason, true);
        }
    }
}

} // namespace session
} // namespace game
