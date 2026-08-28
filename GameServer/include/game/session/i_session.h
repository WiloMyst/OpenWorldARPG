#pragma once
#include <cstdint>
#include <string>

namespace game {
    class ServerMessage;
}

namespace game {
namespace session {

// 会话状态机 (gRPC 与 TCP 双通道共享)
//   ACCEPTING   : 已创建未就绪 (gRPC 等待连接绑定)
//   WAIT_LOGIN  : 连接就绪未登录, 仅接受登录消息
//   ONLINE      : 登录成功, 正常收发
//   RECONNECTING: TCP 断线后保留会话等待重连 (gRPC 通道不支持)
enum class SessionState { ACCEPTING, WAIT_LOGIN, ONLINE, RECONNECTING };

// 会话抽象: 统一 gRPC 与 TCP 双通道的会话管理接口
// SessionManager 只依赖本接口, 不感知底层传输
class ISession {
public:
    virtual ~ISession() = default;

    // ---- 状态查询 ----
    virtual uint64_t session_id() const = 0;
    virtual SessionState state() const = 0;
    virtual int64_t connect_ms() const = 0;
    virtual int64_t last_active_ms() const = 0;
    virtual std::string account() const = 0;

    // ---- 断线重连 (仅 TCP 通道支持) ----
    virtual bool resumable() const = 0;            // 是否支持断线重连
    virtual std::string resume_token() const = 0;  // 登录签发的会话令牌
    virtual void SetResumeToken(const std::string& token) = 0;  // 登录成功后注入令牌
    virtual bool clean_logout() const = 0;         // 是否为显式登出 (登出不保留)
    virtual void MarkCleanLogout() = 0;            // 显式登出标记: 关闭时不进入重连等待
    virtual void MarkReconnecting(int64_t grace_ms) = 0;  // 进入重连等待
    virtual bool resume_expired() const = 0;       // 重连等待是否超时

    // ---- 状态变更 ----
    virtual void Touch() = 0;
    virtual void MarkOnline(const std::string& account) = 0;
    virtual void RequestClose(const std::string& reason, bool notify) = 0;

    // ---- 发送路径 (AOI 广播等) ----
    virtual void EnqueueWrite(const game::ServerMessage& msg) = 0;
};

} // namespace session
} // namespace game
