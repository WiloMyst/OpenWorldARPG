#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace game {
    namespace core { class PlayerSession; }
    namespace infra { struct AppConfig; }
}

namespace game {
namespace session {

// 会话管理器: 统一登记连接会话, 维护账号 -> 会话绑定
// 职责: 同账号重复登录踢旧连接 / 心跳超时踢出 / 登录超时清理
// 线程模型: 登记与解绑来自 CQ 线程和 worker 线程, 超时扫描由独立后台线程执行
class SessionManager {
public:
    explicit SessionManager(const infra::AppConfig& config);
    ~SessionManager();

    void Start();
    void Stop();

    // 连接建立时登记 (未登录态), 由 PlayerSession::Create 调用
    void RegisterSession(std::shared_ptr<core::PlayerSession> session);

    // 登录成功后绑定账号; 同账号旧连接会被踢下线
    // 同一会话重复绑定直接幂等返回
    bool BindAccount(const std::string& account, std::shared_ptr<core::PlayerSession> session);

    // 会话关闭回调 (正常退出/断连/被踢), 由 PlayerSession 调用
    // 只解绑仍指向该会话的账号, 避免误删重复登录后的新绑定
    void OnSessionClosed(std::shared_ptr<core::PlayerSession> session);

private:
    void CheckLoop();

    // ---- 配置 ----
    int64_t heartbeat_timeout_ms_;
    int64_t login_timeout_ms_;
    int64_t check_interval_ms_;

    // ---- 后台扫描线程 ----
    std::thread check_thread_;
    std::atomic<bool> stopped_{true};

    // ---- 会话登记表 (mtx_ 保护) ----
    std::mutex mtx_;
    std::unordered_map<uint64_t, std::weak_ptr<core::PlayerSession>> sessions_;
    std::unordered_map<std::string, std::weak_ptr<core::PlayerSession>> account_map_;
};

} // namespace session
} // namespace game
