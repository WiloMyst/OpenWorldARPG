#include "game/net/unary_call.h"

#include "game/session/session_manager.h"
#include "game/infra/thread_pool.h"
#include <spdlog/spdlog.h>

namespace game {
namespace net {

// ==================== UnaryCall (模板, 显式实例化见文件末尾) ====================

template <typename ReqT, typename RespT>
void UnaryCall<ReqT, RespT>::Spawn(game::GameService::AsyncService* service,
                                   grpc::ServerCompletionQueue* cq,
                                   infra::ThreadPool* pool, RequestFn request_fn, Job job) {
    auto call = std::shared_ptr<UnaryCall>(
        new UnaryCall(service, cq, pool, request_fn, std::move(job)));
    auto* tag = MakeTag(call, CALL);
    (service->*request_fn)(&call->ctx_, &call->req_, &call->responder_, cq, cq, tag);
}

template <typename ReqT, typename RespT>
UnaryCall<ReqT, RespT>::UnaryCall(game::GameService::AsyncService* service,
                                  grpc::ServerCompletionQueue* cq, infra::ThreadPool* pool,
                                  RequestFn request_fn, Job job)
    : service_(service), cq_(cq), pool_(pool), request_fn_(request_fn),
      job_(std::move(job)), responder_(&ctx_) {}

template <typename ReqT, typename RespT>
void UnaryCall<ReqT, RespT>::HandleEvent(int type, bool ok) {
    if (type == FINISH) return;   // 收尾完成, tag 释放后析构
    if (!ok) return;              // 关服: accept 未送达, 直接释放

    // 请求到达: 先自续下一个 accept, 再处理当前请求 (此后本对象只服务本次调用)
    Spawn(service_, cq_, pool_, request_fn_, job_);
    const std::string token = ExtractBearerToken(ctx_);

    auto self = this->shared_from_this();
    auto job = job_;
    auto future_opt = pool_->enqueue([self, job, token]() {
        RespT resp;
        const grpc::Status st = job(token, self->req_, &resp);
        self->Complete(resp, st);
    });

    // 背压: 线程池队列满时直接回 RESOURCE_EXHAUSTED, 不排队
    if (!future_opt.has_value()) {
        spdlog::warn("[UnaryCall] Worker pool saturated, rejecting request");
        RespT resp;
        Complete(resp, grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "server busy"));
    }
}

template <typename ReqT, typename RespT>
void UnaryCall<ReqT, RespT>::Complete(const RespT& resp, const grpc::Status& status) {
    responder_.Finish(resp, status, MakeTag(this->shared_from_this(), FINISH));
}

// 显式实例化: 本编译单元内 grpc_server.cpp 的 Spawn 调用据此链接
template class UnaryCall<game::LoginRequest, game::LoginResponse>;
template class UnaryCall<game::LogoutRequest, game::LogoutResponse>;
template class UnaryCall<game::RegisterCharacterRequest, game::RegisterCharacterResponse>;
template class UnaryCall<game::SetActiveCharacterRequest, game::SetActiveCharacterResponse>;
template class UnaryCall<game::EnemySpawnRequest, game::EnemySpawnResponse>;
template class UnaryCall<game::InventoryOpRequest, game::InventoryOpResponse>;
template class UnaryCall<game::DialogueAuthRequest, game::DialogueAuthResult>;

// ==================== DialogueCall (StreamDialogue 占位) ====================

void DialogueCall::Spawn(game::GameService::AsyncService* service,
                         grpc::ServerCompletionQueue* cq,
                         session::SessionManager* session_manager) {
    auto call = std::shared_ptr<DialogueCall>(new DialogueCall(service, cq, session_manager));
    service->RequestStreamDialogue(&call->ctx_, &call->req_, &call->writer_, cq, cq,
                                   MakeTag(call, CALL));
}

DialogueCall::DialogueCall(game::GameService::AsyncService* service,
                           grpc::ServerCompletionQueue* cq,
                           session::SessionManager* session_manager)
    : service_(service), cq_(cq), session_manager_(session_manager), writer_(&ctx_) {}

void DialogueCall::HandleEvent(int type, bool ok) {
    switch (type) {
        case CALL:
            if (!ok) return;   // 关服: accept 未送达, 直接释放
            // 自续下一个 accept, 再处理当前调用
            Spawn(service_, cq_, session_manager_);
            {
                std::string account;
                if (!session_manager_->AuthenticateUnary(ExtractBearerToken(ctx_), &account)) {
                    writer_.Finish(grpc::Status(grpc::StatusCode::UNAUTHENTICATED,
                                                "invalid dialogue token"),
                                   MakeTag(shared_from_this(), FINISH));
                    return;
                }

                // 信令占位: 单条回执即结束流, 待接入推理后端后改为逐字下发
                game::DialogueWord word;
                word.set_text("(占位) 对话流已建立，等待接入推理后端。");
                word.set_last(true);
                writer_.Write(word, MakeTag(shared_from_this(), WRITE));
            }
            return;
        case WRITE:
            // 单条回执送达 (或对端已断开), 统一结束流
            writer_.Finish(grpc::Status::OK, MakeTag(shared_from_this(), FINISH));
            return;
        case FINISH:
        default:
            return;   // 收尾完成, tag 释放后析构
    }
}

} // namespace net
} // namespace game
