#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <grpcpp/grpcpp.h>

#include "game.pb.h"
#include "game.grpc.pb.h"
#include "game/net/call_base.h"
#include "game/session/i_session.h"

namespace game {
    namespace session { class SessionHandler; class SessionManager; }
}

namespace game {
namespace net {

// World 双向流会话 (异步 Service + CompletionQueue 状态机):
// 承载实时玩法 (心跳/移动/伤害上报 上行, AOI 广播/战斗结算/踢出 下行).
// 构建环境为 gRPC 1.30 (apt), 无 CallbackService Reactor API, 且同步 Service
// 的"每流一线程"模型并发上限受线程数制约, 故改用异步状态机:
//   CONNECT(tag) : 新流到达 -> 自续下一个 accept -> 令牌兑换 (TryEnterWorld)
//                  -> 成功 IssueRead 进入读循环, 失败 Finish(UNAUTHENTICATED)
//   READ(tag)    : 上行消息到达 -> CQ 线程直接处理 (内存级操作: 世界校验/
//                  战斗结算, 重状态在 WorldManager/CombatManager 的内部锁中)
//                  -> 续读
//   WRITE(tag)   : 下行写完成 -> 出队 -> 队列空且 pending_cancel 时 TryCancel
//   FINISH(tag)  : 流终结 -> OnSessionClosed (联动世界离开与对话流吊销)
// 写路径: 写队列 + 单在途写 (write_mtx_ 串行), 广播方可为任意线程 (CQ 线程/
// 会话扫描线程). 踢出/登出经 TryCancel 唤醒, 在途事件以 ok=false 走统一收尾.
//
// 生命周期: 事件 tag 持强引用, 流终结 (FINISH) 后引用归零自动析构,
// SessionManager 仅持 weak_ptr. 登录与会话建立分离: 流建立凭 unary Login
// 签发的 Bearer 令牌兑换 (ClaimLogin -> EnterWorld); gRPC 通道不支持断线
// 重连 (resumable 恒 false).
class WorldSession final : public session::ISession,
                           public CallBase,
                           public std::enable_shared_from_this<WorldSession> {
public:
    enum EventType { CONNECT, READ, WRITE, FINISH };

    // 发起首个流 accept; 每次流到达时自续, 调用方无需重复调用
    static void Spawn(game::GameService::AsyncService* service,
                      grpc::ServerCompletionQueue* cq,
                      session::SessionManager* session_manager,
                      session::SessionHandler* handler);

    void HandleEvent(int type, bool ok) override;

    // ---- ISession ----
    uint64_t session_id() const override { return session_id_; }
    session::SessionState state() const override { return state_.load(std::memory_order_acquire); }
    int64_t connect_ms() const override { return connect_ms_; }
    int64_t last_active_ms() const override { return last_active_ms_.load(std::memory_order_relaxed); }
    std::string account() const override;
    bool resumable() const override { return false; }
    std::string resume_token() const override { return ""; }
    void SetResumeToken(const std::string&) override {}
    bool clean_logout() const override { return true; }
    void MarkCleanLogout() override {}
    void MarkReconnecting(int64_t) override {}
    bool resume_expired() const override { return false; }

    // ---- 状态变更 ----
    void Touch() override;
    void MarkOnline(const std::string& account) override;
    // 请求关闭: notify=true 时 kick 消息入队并待写队列排空后 TryCancel (尽力送达),
    // notify=false 直接 TryCancel; 幂等仅首次生效
    void RequestClose(const std::string& reason, bool notify) override;

    // ---- 发送路径 (SessionManager/AOI/战斗广播, 任意线程) ----
    void EnqueueWrite(const game::ServerMessage& msg) override;

private:
    WorldSession(game::GameService::AsyncService* service,
                 grpc::ServerCompletionQueue* cq,
                 session::SessionManager* session_manager,
                 session::SessionHandler* handler);

    // metadata 读 token -> ClaimLogin -> EnterWorld (BindAccount + 进世界)
    bool TryEnterWorld(const std::string& session_token);
    void IssueRead();
    void IssueWrite(const game::ServerWorld& response);

    // ---- 依赖 ----
    game::GameService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    session::SessionManager* session_manager_;
    session::SessionHandler* handler_;

    // ---- 会话状态 ----
    uint64_t session_id_;
    static std::atomic<uint64_t> next_session_id_;
    std::atomic<session::SessionState> state_{session::SessionState::ACCEPTING};
    mutable std::mutex state_mtx_;
    std::string account_;
    int64_t connect_ms_ = 0;
    std::atomic<int64_t> last_active_ms_{0};

    // ---- 流与写同步 ----
    grpc::ServerContext ctx_;
    grpc::ServerAsyncReaderWriter<game::ServerWorld, game::ClientWorld> stream_;
    game::ClientWorld request_;

    std::mutex write_mtx_;
    std::queue<game::ServerWorld> write_queue_;
    bool is_writing_ = false;
    bool pending_cancel_ = false;   // 写队列排空后须 TryCancel (kick 送达后的踢出路径)

    std::atomic<bool> close_initiated_{false};
    std::atomic<bool> is_finishing_{false};
};

} // namespace net
} // namespace game
