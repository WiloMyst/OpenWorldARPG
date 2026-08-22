#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <grpcpp/grpcpp.h>
#include "game.pb.h"
#include "game.grpc.pb.h"

namespace game {
    namespace infra { class ThreadPool; }
    namespace session { class SessionManager; }
    namespace logic { class GameLogic; }
}

namespace game {
namespace core {

// 单连接会话: gRPC 异步双向流状态机
// 事件流转: CONNECT -> (READ / WRITE 交替) -> FINISH, shared_ptr 引用计数自管理
// 会话业务状态: ACCEPTING -> WAIT_LOGIN -> ONLINE
//   ACCEPTING: 挂在 RequestGameChannel 等待连接, 未绑定 RPC, 禁止任何流操作
//   WAIT_LOGIN: 已连接未登录, 仅接受 Login 消息
// 关闭路径: RequestClose 先下发 KickNotify, 写队列排空后统一 Finish
class PlayerSession : public std::enable_shared_from_this<PlayerSession> {
public:
    enum class EventType { CONNECT, READ, WRITE, FINISH };
    enum class State { ACCEPTING, WAIT_LOGIN, ONLINE };

    struct EventTag {
        std::shared_ptr<PlayerSession> instance;
        EventType type;
    };

    static void Create(game::GameService::AsyncService* service,
                       grpc::ServerCompletionQueue* cq,
                       infra::ThreadPool* pool,
                       session::SessionManager* session_manager,
                       logic::GameLogic* logic);

    void HandleEvent(EventType type, bool ok);

    // ---- 状态查询 (SessionManager 与自身消息处理使用) ----
    uint64_t session_id() const { return session_id_; }
    State state() const { return state_.load(std::memory_order_acquire); }
    int64_t connect_ms() const { return connect_ms_; }
    int64_t last_active_ms() const { return last_active_ms_.load(std::memory_order_relaxed); }

    // account_ 在 MarkOnline 前后均可能被读, 由 state_mtx_ 保护
    std::string account() const;

    // ---- 状态变更 (SessionManager 调用) ----
    void Touch();
    void MarkOnline(const std::string& account);
    // 请求关闭: notify=true 先下发 KickNotify 再收尾; 幂等, 仅首次生效
    void RequestClose(const std::string& reason, bool notify);

private:
    PlayerSession(game::GameService::AsyncService* service,
                  grpc::ServerCompletionQueue* cq,
                  infra::ThreadPool* pool,
                  session::SessionManager* session_manager,
                  logic::GameLogic* logic);

    // ---- gRPC 异步原语 ----
    void Start();
    // CONNECT 事件时调用: 绑定 RPC, 进入 WAIT_LOGIN 并重置登录计时起点
    void MarkConnected();
    void IssueRead();
    void IssueWrite(const game::ServerMessage& msg);
    void IssueFinish();

    // ---- 发送路径 ----
    void EnqueueWrite(const game::ServerMessage& msg);
    void SendError(uint64_t seq, int code, const std::string& message);

    // ---- 消息处理 ----
    void ProcessRequestAsync(game::ClientMessage req);
    void ProcessMessage(const game::ClientMessage& msg);

    // ---- 依赖 ----
    game::GameService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    infra::ThreadPool* pool_;
    session::SessionManager* session_manager_;
    logic::GameLogic* logic_;

    // ---- gRPC 流 ----
    grpc::ServerContext ctx_;
    game::ClientMessage request_;
    grpc::ServerAsyncReaderWriter<game::ServerMessage, game::ClientMessage> stream_;

    // ---- 发送队列 (write_mtx_ 保护, 单 in-flight 写保证顺序) ----
    std::mutex write_mtx_;
    std::queue<game::ServerMessage> write_queue_;
    bool is_writing_ = false;
    bool finish_after_drain_ = false;

    // ---- 会话状态 ----
    uint64_t session_id_;
    static std::atomic<uint64_t> next_session_id_;
    std::atomic<State> state_{State::ACCEPTING};
    mutable std::mutex state_mtx_;
    std::string account_;
    // RPC 连接建立时刻 (MarkConnected 时写入, 先于 state_ 的 release store)
    int64_t connect_ms_ = 0;
    std::atomic<int64_t> last_active_ms_{0};

    // ---- 关闭控制 ----
    std::atomic<bool> close_initiated_{false};
    std::atomic<bool> finish_issued_{false};
};

} // namespace core
} // namespace game
