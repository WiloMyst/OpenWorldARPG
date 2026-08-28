#pragma once
#include <functional>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "game.pb.h"
#include "game.grpc.pb.h"
#include "game/net/call_base.h"

namespace game {
    namespace infra { class ThreadPool; }
    namespace session { class SessionManager; }
}

namespace game {
namespace net {

// Unary 泛型异步调用: 请求到达即自续下一个 accept, 业务经 ThreadPool 执行——
// unary 含 MySQL/Redis IO, 不能占 CQ 事件线程 (否则拖慢全部会话的事件派发);
// 业务完成后在线程池线程调 Finish, FINISH tag 派回 CQ 线程收尾.
//
// 状态机:
//   CALL(tag)   : 请求到达 (req_ 已填充) -> 自续 accept -> 提交线程池 ->
//                 业务执行 -> responder_.Finish(resp, status, FINISH tag)
//   FINISH(tag) : 响应送达或取消完成, tag 释放, 调用析构
//   CALL(!ok)   : 关服时未送达的 accept, 直接释放
template <typename ReqT, typename RespT>
class UnaryCall final : public CallBase,
                        public std::enable_shared_from_this<UnaryCall<ReqT, RespT>> {
public:
    enum EventType { CALL, FINISH };

    using RequestFn = void (game::GameService::AsyncService::*)(
        grpc::ServerContext*, ReqT*, grpc::ServerAsyncResponseWriter<RespT>*,
        grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

    // token = 请求到达时从 metadata 提取的 Bearer 令牌, 鉴权由各 Job 自行决定
    using Job = std::function<grpc::Status(const std::string& token, const ReqT& req,
                                           RespT* resp)>;

    // 发起首个 accept; 每次请求到达时自续, 调用方无需重复调用
    static void Spawn(game::GameService::AsyncService* service,
                      grpc::ServerCompletionQueue* cq, infra::ThreadPool* pool,
                      RequestFn request_fn, Job job);

    void HandleEvent(int type, bool ok) override;

private:
    UnaryCall(game::GameService::AsyncService* service, grpc::ServerCompletionQueue* cq,
              infra::ThreadPool* pool, RequestFn request_fn, Job job);

    // 业务完成后回执; 可在线程池线程调用 (responder 线程安全, 每调用仅 Finish 一次)
    void Complete(const RespT& resp, const grpc::Status& status);

    game::GameService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    infra::ThreadPool* pool_;
    RequestFn request_fn_;
    Job job_;

    grpc::ServerContext ctx_;
    ReqT req_;
    grpc::ServerAsyncResponseWriter<RespT> responder_;
};

// StreamDialogue (server-streaming, 信令占位实现): 令牌鉴权后单条回执即结束流
class DialogueCall final : public CallBase,
                           public std::enable_shared_from_this<DialogueCall> {
public:
    enum EventType { CALL, WRITE, FINISH };

    static void Spawn(game::GameService::AsyncService* service,
                      grpc::ServerCompletionQueue* cq,
                      session::SessionManager* session_manager);

    void HandleEvent(int type, bool ok) override;

private:
    DialogueCall(game::GameService::AsyncService* service,
                 grpc::ServerCompletionQueue* cq,
                 session::SessionManager* session_manager);

    game::GameService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    session::SessionManager* session_manager_;

    grpc::ServerContext ctx_;
    game::DialogueRequest req_;
    grpc::ServerAsyncWriter<game::DialogueWord> writer_;
};

} // namespace net
} // namespace game
