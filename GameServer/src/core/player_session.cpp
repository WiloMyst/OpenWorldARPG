#include "game/core/player_session.h"
#include "game/infra/thread_pool.hpp"
#include "game/infra/time_utils.h"
#include "game/session/session_manager.h"
#include "game/logic/game_logic.h"
#include <spdlog/spdlog.h>

namespace game {
namespace core {

std::atomic<uint64_t> PlayerSession::next_session_id_{1};

void PlayerSession::Create(game::GameService::AsyncService* service,
                           grpc::ServerCompletionQueue* cq,
                           infra::ThreadPool* pool,
                           session::SessionManager* session_manager,
                           logic::GameLogic* logic) {
    std::shared_ptr<PlayerSession> session(
        new PlayerSession(service, cq, pool, session_manager, logic));
    session_manager->RegisterSession(session);
    session->Start();
}

PlayerSession::PlayerSession(game::GameService::AsyncService* service,
                             grpc::ServerCompletionQueue* cq,
                             infra::ThreadPool* pool,
                             session::SessionManager* session_manager,
                             logic::GameLogic* logic)
    : service_(service), cq_(cq), pool_(pool),
      session_manager_(session_manager), logic_(logic),
      stream_(&ctx_),
      session_id_(next_session_id_.fetch_add(1, std::memory_order_relaxed)) {
    last_active_ms_.store(infra::NowMs());
}

std::string PlayerSession::account() const {
    std::lock_guard<std::mutex> lock(state_mtx_);
    return account_;
}

void PlayerSession::Touch() {
    last_active_ms_.store(infra::NowMs(), std::memory_order_relaxed);
}

void PlayerSession::MarkOnline(const std::string& account) {
    {
        std::lock_guard<std::mutex> lock(state_mtx_);
        account_ = account;
    }
    state_.store(State::ONLINE, std::memory_order_release);
    spdlog::info("[Session#{}] Player online [account={}]", session_id_, account);
}

void PlayerSession::RequestClose(const std::string& reason, bool notify) {
    // ACCEPTING 会话未绑定 RPC, 无流可写可 Finish; 正常流程不会走到这里, 防御误用
    if (state() == State::ACCEPTING) {
        spdlog::warn("[Session#{}] Close requested before connection, ignored [reason={}]",
                     session_id_, reason);
        return;
    }

    if (close_initiated_.exchange(true)) return;

    spdlog::info("[Session#{}] Close requested [account={}, reason={}]",
                 session_id_, account(), reason);

    bool start_write = false;
    bool finish_now = false;
    game::ServerMessage front;

    {
        std::lock_guard<std::mutex> lock(write_mtx_);
        finish_after_drain_ = true;

        if (notify) {
            game::ServerMessage kick;
            kick.set_ack_sequence(0);
            kick.mutable_kick()->set_reason(reason);
            write_queue_.push(kick);
        }

        if (!write_queue_.empty()) {
            if (!is_writing_) {
                is_writing_ = true;
                start_write = true;
                front = write_queue_.front();
            }
            // 已有在途写: KickNotify 排在队尾, 写完后由 WRITE 事件触发 Finish
        } else {
            finish_now = true;
        }
    }

    if (start_write) IssueWrite(front);
    if (finish_now) IssueFinish();
}

void PlayerSession::HandleEvent(EventType type, bool ok) {
    // 终态: 流已收尾, 注销会话; 引用计数归零后自动析构
    if (type == EventType::FINISH) {
        spdlog::info("[Session#{}] Closed [account={}]", session_id_, account());
        session_manager_->OnSessionClosed(shared_from_this());
        return;
    }

    // ACCEPTING 阶段的连接请求被取消 (服务器关闭): 未绑定 RPC, 无流可收尾, 直接注销
    if (type == EventType::CONNECT && !ok) {
        spdlog::info("[Session#{}] Accept cancelled (server shutdown)", session_id_);
        session_manager_->OnSessionClosed(shared_from_this());
        return;
    }

    // 异常或客户端断开: 优雅收尾
    if (!ok) {
        spdlog::info("[Session#{}] Peer disconnected", session_id_);
        IssueFinish();
        return;
    }

    switch (type) {
        case EventType::CONNECT: {
            // 新连接到达: 当前会话绑定 RPC 进入登录阶段, 立即创建下一个 Session 接客
            spdlog::info("[Session#{}] Connection established", session_id_);
            MarkConnected();
            Create(service_, cq_, pool_, session_manager_, logic_);
            IssueRead();
            break;
        }
        case EventType::READ: {
            game::ClientMessage req = request_;
            ProcessRequestAsync(std::move(req));
            IssueRead();
            break;
        }
        case EventType::WRITE: {
            // 上一帧写完: 队列有积压则续写, 排空且待收尾则 Finish
            bool start_next = false;
            bool finish_now = false;
            game::ServerMessage next_msg;

            {
                std::lock_guard<std::mutex> lock(write_mtx_);
                if (!write_queue_.empty()) write_queue_.pop();

                if (!write_queue_.empty()) {
                    start_next = true;
                    next_msg = write_queue_.front();
                } else {
                    is_writing_ = false;
                    if (finish_after_drain_) finish_now = true;
                }
            }

            if (start_next) IssueWrite(next_msg);
            if (finish_now) IssueFinish();
            break;
        }
        default:
            break;
    }
}

void PlayerSession::Start() {
    auto* tag = new EventTag{shared_from_this(), EventType::CONNECT};
    service_->RequestGameChannel(&ctx_, &stream_, cq_, cq_, tag);
}

void PlayerSession::MarkConnected() {
    // connect_ms_ 先写, state_ release 发布; CheckLoop 侧 acquire 读 state_ 后可见
    connect_ms_ = infra::NowMs();
    state_.store(State::WAIT_LOGIN, std::memory_order_release);
}

void PlayerSession::IssueRead() {
    auto* tag = new EventTag{shared_from_this(), EventType::READ};
    stream_.Read(&request_, tag);
}

void PlayerSession::IssueWrite(const game::ServerMessage& msg) {
    auto* tag = new EventTag{shared_from_this(), EventType::WRITE};
    stream_.Write(msg, tag);
}

void PlayerSession::IssueFinish() {
    if (finish_issued_.exchange(true)) return;
    auto* tag = new EventTag{shared_from_this(), EventType::FINISH};
    stream_.Finish(grpc::Status::OK, tag);
}

void PlayerSession::EnqueueWrite(const game::ServerMessage& msg) {
    if (finish_issued_.load()) return;

    bool start_write = false;
    game::ServerMessage front;

    {
        std::lock_guard<std::mutex> lock(write_mtx_);
        if (finish_issued_.load()) return;
        write_queue_.push(msg);
        if (!is_writing_) {
            is_writing_ = true;
            start_write = true;
            front = write_queue_.front();
        }
    }

    if (start_write) IssueWrite(front);
}

void PlayerSession::SendError(uint64_t seq, int code, const std::string& message) {
    game::ServerMessage reply;
    reply.set_ack_sequence(seq);
    reply.mutable_error()->set_code(code);
    reply.mutable_error()->set_message(message);
    EnqueueWrite(reply);
}

void PlayerSession::ProcessRequestAsync(game::ClientMessage req) {
    Touch();

    auto future_opt = pool_->enqueue([self = shared_from_this(),
                                      req = std::move(req)]() {
        self->ProcessMessage(req);
    });

    // 背压保护: 线程池队列已满, 拒绝并回 503
    if (!future_opt.has_value()) {
        spdlog::warn("[Session#{}] Server overloaded, message dropped [seq={}]",
                     session_id_, req.sequence());
        SendError(req.sequence(), 503, "Server overloaded: task queue is full");
    }
}

void PlayerSession::ProcessMessage(const game::ClientMessage& msg) {
    switch (msg.payload_case()) {
        case game::ClientMessage::kLogin: {
            std::string account;
            game::LoginResponse resp = logic_->HandleLogin(msg.login(), &account);

            if (resp.success()) {
                // 绑定账号 (内部置 ONLINE, 同账号旧连接被踢)
                session_manager_->BindAccount(account, shared_from_this());
            }

            game::ServerMessage reply;
            reply.set_ack_sequence(msg.sequence());
            *reply.mutable_login() = resp;
            EnqueueWrite(reply);
            break;
        }
        case game::ClientMessage::kHeartbeat: {
            if (state() != State::ONLINE) {
                SendError(msg.sequence(), 401, "Not authenticated");
                break;
            }
            game::ServerMessage reply;
            reply.set_ack_sequence(msg.sequence());
            *reply.mutable_heartbeat() = logic_->HandleHeartbeat(account(), msg.heartbeat());
            EnqueueWrite(reply);
            break;
        }
        case game::ClientMessage::kSaveData: {
            if (state() != State::ONLINE) {
                SendError(msg.sequence(), 401, "Not authenticated");
                break;
            }
            game::ServerMessage reply;
            reply.set_ack_sequence(msg.sequence());
            *reply.mutable_save_data() = logic_->HandleSaveData(account(), msg.save_data());
            EnqueueWrite(reply);
            break;
        }
        case game::ClientMessage::kInventoryOp: {
            if (state() != State::ONLINE) {
                SendError(msg.sequence(), 401, "Not authenticated");
                break;
            }
            game::ServerMessage reply;
            reply.set_ack_sequence(msg.sequence());
            *reply.mutable_inventory_op() = logic_->HandleInventoryOp(account(), msg.inventory_op());
            EnqueueWrite(reply);
            break;
        }
        case game::ClientMessage::kDialogueAuth: {
            if (state() != State::ONLINE) {
                SendError(msg.sequence(), 401, "Not authenticated");
                break;
            }
            game::ServerMessage reply;
            reply.set_ack_sequence(msg.sequence());
            *reply.mutable_dialogue_auth_result() = logic_->HandleDialogueAuth(account(), msg.dialogue_auth());
            EnqueueWrite(reply);
            break;
        }
        case game::ClientMessage::kLogout: {
            spdlog::info("[Session#{}] Player logout [account={}]", session_id_, account());
            logic_->HandleLogout(account());
            RequestClose("logout", false);
            break;
        }
        default: {
            SendError(msg.sequence(), 400, "Unknown or empty message");
            break;
        }
    }
}

} // namespace core
} // namespace game
