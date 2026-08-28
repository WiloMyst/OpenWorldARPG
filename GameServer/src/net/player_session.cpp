#include "game/net/player_session.h"

#include "game/infra/time_utils.h"
#include "game/session/session_manager.h"
#include "game/session/session_handler.h"
#include <spdlog/spdlog.h>

namespace game {
namespace net {

std::atomic<uint64_t> WorldSession::next_session_id_{1};

namespace {

// 内部事件容器 ServerMessage -> 协议 ServerWorld (单字段透传).
// 返回 false 表示非法下行 (login 仅存在于 unary, 不应流入 World 流)
bool ToServerWorld(const game::ServerMessage& in, game::ServerWorld* out) {
    out->set_ack_sequence(in.ack_sequence());
    switch (in.payload_case()) {
        case game::ServerMessage::kHeartbeat:
            *out->mutable_heartbeat() = in.heartbeat();
            break;
        case game::ServerMessage::kPlayerEnter:
            *out->mutable_player_enter() = in.player_enter();
            break;
        case game::ServerMessage::kPlayerLeave:
            *out->mutable_player_leave() = in.player_leave();
            break;
        case game::ServerMessage::kPlayerMove:
            *out->mutable_player_move() = in.player_move();
            break;
        case game::ServerMessage::kPositionCorrection:
            *out->mutable_position_correction() = in.position_correction();
            break;
        case game::ServerMessage::kEnemySpawn:
            *out->mutable_enemy_spawn() = in.enemy_spawn();
            break;
        case game::ServerMessage::kDamageDeal:
            *out->mutable_damage_deal() = in.damage_deal();
            break;
        case game::ServerMessage::kPlayerDamage:
            *out->mutable_player_damage() = in.player_damage();
            break;
        case game::ServerMessage::kKick:
            *out->mutable_kick() = in.kick();
            break;
        case game::ServerMessage::kError:
            *out->mutable_error() = in.error();
            break;
        case game::ServerMessage::kLogin:
        default:
            // login 响应经 unary RPC 返回, 不流入 World 流; 到达即协议异常, 丢弃
            spdlog::warn("[World] internal msg 'login' cannot traverse World stream, dropped");
            out->Clear();
            return false;
    }
    return true;
}

} // namespace

void WorldSession::Spawn(game::GameService::AsyncService* service,
                         grpc::ServerCompletionQueue* cq,
                         session::SessionManager* session_manager,
                         session::SessionHandler* handler) {
    auto session = std::shared_ptr<WorldSession>(
        new WorldSession(service, cq, session_manager, handler));
    service->RequestWorld(&session->ctx_, &session->stream_, cq, cq,
                          CallBase::MakeTag(session, CONNECT));
}

WorldSession::WorldSession(game::GameService::AsyncService* service,
                           grpc::ServerCompletionQueue* cq,
                           session::SessionManager* session_manager,
                           session::SessionHandler* handler)
    : service_(service), cq_(cq), session_manager_(session_manager), handler_(handler),
      session_id_(next_session_id_.fetch_add(1, std::memory_order_relaxed)),
      stream_(&ctx_) {
    connect_ms_ = infra::NowMs();
    last_active_ms_.store(connect_ms_, std::memory_order_relaxed);
}

std::string WorldSession::account() const {
    std::lock_guard<std::mutex> lock(state_mtx_);
    return account_;
}

void WorldSession::Touch() {
    last_active_ms_.store(infra::NowMs(), std::memory_order_relaxed);
}

void WorldSession::MarkOnline(const std::string& account) {
    {
        std::lock_guard<std::mutex> lock(state_mtx_);
        account_ = account;
    }
    state_.store(session::SessionState::ONLINE, std::memory_order_release);
    spdlog::info("[WorldSession#{}] Player online [account={}]", session_id_, account);
}

void WorldSession::HandleEvent(int type, bool ok) {
    // 终态: 注销会话 (联动世界离开 / VHServer 对话流吊销), tag 释放后析构
    if (type == FINISH) {
        session_manager_->OnSessionClosed(
            std::static_pointer_cast<session::ISession>(shared_from_this()));
        return;
    }

    // 流断开/关服/取消: 优雅收尾. Read 与 Write 在途 tag 均可能触发, 仅首个生效
    if (!ok) {
        if (!is_finishing_.exchange(true)) {
            spdlog::info("[WorldSession#{}] Stream broken, initiating graceful close [account={}]",
                         session_id_, account());
            stream_.Finish(grpc::Status::OK, CallBase::MakeTag(shared_from_this(), FINISH));
        }
        return;
    }

    switch (type) {
        case CONNECT: {
            // 新流到达: 立即自续下一个 accept, 登记会话.
            // 会话令牌由首个 READ 帧的消息体携带兑换 (已验证 gRPC 双向流 initial metadata
            // 在客户端不可达, 包括裸 grpc 与 TurboLink), CONNECT 阶段不做凭据判定.
            Spawn(service_, cq_, session_manager_, handler_);
            state_.store(session::SessionState::WAIT_LOGIN, std::memory_order_release);
            session_manager_->RegisterSession(
                std::static_pointer_cast<session::ISession>(shared_from_this()));
            IssueRead();
            break;
        }
        case READ: {
            // 上行消息到达: CQ 线程直接处理 (轻量内存操作) 并续读, 天然保序
            Touch();
            // 首帧携带 session_token 兑换进入 World (metadata 在双向流上不可达, 故走消息体)
            // 仅在 WAIT_LOGIN 态尝试一次, 之后帧复用已绑定账号
            if (state_.load(std::memory_order_acquire) == session::SessionState::WAIT_LOGIN &&
                !TryEnterWorld(request_.session_token())) {
                spdlog::warn("[WorldSession#{}] Rejected: invalid session token", session_id_);
                is_finishing_.store(true);
                stream_.Finish(
                    grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "invalid session token"),
                    CallBase::MakeTag(shared_from_this(), FINISH));
                return;
            }
            handler_->HandleWorldMessage(
                std::static_pointer_cast<session::ISession>(shared_from_this()), request_);
            IssueRead();
            break;
        }
        case WRITE: {
            game::ServerWorld next;
            bool has_more = false;
            bool do_cancel = false;
            {
                std::lock_guard<std::mutex> lock(write_mtx_);
                write_queue_.pop();
                if (!write_queue_.empty()) {
                    has_more = true;
                    next = write_queue_.front();
                } else {
                    is_writing_ = false;
                    if (pending_cancel_) do_cancel = true;
                }
            }
            if (has_more) IssueWrite(next);
            // kick 已送达且队列排空: 唤醒读循环, Read 以 ok=false 到达走统一收尾
            if (do_cancel) ctx_.TryCancel();
            break;
        }
        default:
            break;
    }
}

bool WorldSession::TryEnterWorld(const std::string& session_token) {
    // 优先取首帧消息体 token; 空则回退 gRPC metadata (裸 grpc 测试客户端走这条).
    // TurbroLink 客户端双向流 initial metadata 不可达, 必须走消息体分支.
    std::string token = session_token;
    if (token.empty()) token = ExtractBearerToken(ctx_);

    if (token.empty()) {
        spdlog::warn("[WorldSession#{}] TryEnterWorld: empty session token", session_id_);
        return false;
    }
    spdlog::info("[WorldSession#{}] TryEnterWorld token=[{}]", session_id_, token);

    session::SessionManager::PendingLogin pending;
    if (!session_manager_->ClaimLogin(token, &pending)) {
        spdlog::warn("[WorldSession#{}] TryEnterWorld: unknown token (not in login_pending_)", session_id_);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(state_mtx_);
        account_ = pending.account;
    }
    handler_->EnterWorld(std::static_pointer_cast<session::ISession>(shared_from_this()),
                         pending.account, pending.player_id, pending.spawn);
    return true;
}

void WorldSession::IssueRead() {
    stream_.Read(&request_, CallBase::MakeTag(shared_from_this(), READ));
}

void WorldSession::IssueWrite(const game::ServerWorld& response) {
    stream_.Write(response, CallBase::MakeTag(shared_from_this(), WRITE));
}

void WorldSession::RequestClose(const std::string& reason, bool notify) {
    if (close_initiated_.exchange(true)) return;
    spdlog::info("[WorldSession#{}] Close requested [account={}, reason={}]",
                 session_id_, account(), reason);

    if (!notify) {
        // 立即终结 (登出/重连接管): 无需送达通知
        ctx_.TryCancel();
        return;
    }

    // 尽力送达: kick 入队, 待写队列排空 (WRITE 事件) 后 TryCancel;
    // 已在收尾则 kick 发不出去, 直接取消加速终结
    {
        std::lock_guard<std::mutex> lock(write_mtx_);
        pending_cancel_ = true;
    }
    game::ServerMessage kick;
    kick.set_ack_sequence(0);
    kick.mutable_kick()->set_reason(reason);
    EnqueueWrite(kick);
    if (is_finishing_.load()) ctx_.TryCancel();
}

void WorldSession::EnqueueWrite(const game::ServerMessage& msg) {
    if (is_finishing_.load()) return;

    game::ServerWorld wire;
    if (!ToServerWorld(msg, &wire)) return;   // 非法下行已丢弃

    bool should_start = false;
    game::ServerWorld next;
    {
        std::lock_guard<std::mutex> lock(write_mtx_);
        if (is_finishing_.load()) return;
        write_queue_.push(std::move(wire));
        if (!is_writing_) {
            is_writing_ = true;
            should_start = true;
            next = write_queue_.front();
        }
    }
    if (should_start) IssueWrite(next);
}

} // namespace net
} // namespace game
