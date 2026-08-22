#include "game/core/grpc_server.h"
#include "game/core/player_session.h"
#include "game/session/session_manager.h"
#include "game/logic/game_logic.h"
#include "game/infra/thread_pool.hpp"
#include <spdlog/spdlog.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <csignal>

#include "game.grpc.pb.h"

namespace game {
namespace core {

struct GrpcServer::Impl {
    game::GameService::AsyncService service;
    std::unique_ptr<grpc::ServerCompletionQueue> cq;
    std::unique_ptr<grpc::Server> server;
};

GrpcServer* GrpcServer::instance_ = nullptr;
std::atomic<bool> GrpcServer::shutdown_requested_{false};

GrpcServer::GrpcServer() : pimpl_(std::make_unique<Impl>()) {}

GrpcServer::~GrpcServer() {
    Shutdown();
}

void GrpcServer::Shutdown() {
    // 先停会话扫描线程, 避免关闭过程中继续踢会话
    if (session_manager_) session_manager_->Stop();

    if (pimpl_->server) {
        spdlog::info("[GrpcServer] Shutting down gRPC server...");
        pimpl_->server->Shutdown();
    }

    if (pimpl_->cq) {
        pimpl_->cq->Shutdown();

        // 排空 CompletionQueue 中剩余事件, 让在途会话走完收尾
        void* ignored_tag;
        bool ignored_ok;
        while (pimpl_->cq->Next(&ignored_tag, &ignored_ok)) {}
    }

    pool_.reset();
    session_manager_.reset();
    logic_.reset();

    pimpl_->server.reset();
    pimpl_->cq.reset();
}

void GrpcServer::Run(const infra::AppConfig& config) {
    const std::string server_address = config.host + ":" + std::to_string(config.port);

    // 依赖先行初始化: MySQL/物品配置不可用时直接失败, 不进入监听状态
    pool_ = std::make_unique<infra::ThreadPool>(config.worker_threads, config.max_queue_size);
    logic_ = std::make_unique<logic::GameLogic>(config);
    session_manager_ = std::make_unique<session::SessionManager>(config);
    session_manager_->Start();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.SetMaxReceiveMessageSize(16 * 1024 * 1024);
    builder.SetMaxSendMessageSize(16 * 1024 * 1024);
    builder.RegisterService(&(pimpl_->service));

    pimpl_->cq = builder.AddCompletionQueue();
    pimpl_->server = builder.BuildAndStart();

    if (!pimpl_->server) {
        spdlog::critical("[GrpcServer] Failed to start on {}", server_address);
        return;
    }
    spdlog::info("[GrpcServer] Server listening on {}", server_address);

    instance_ = this;
    shutdown_requested_.store(false);
    std::signal(SIGINT, [](int) { shutdown_requested_.store(true); });
    std::signal(SIGTERM, [](int) { shutdown_requested_.store(true); });

    HandleRpcs();
}

void GrpcServer::HandleRpcs() {
    // 启动初始 Session 等待第一个客户端连接
    PlayerSession::Create(&(pimpl_->service), pimpl_->cq.get(),
                          pool_.get(), session_manager_.get(), logic_.get());

    void* raw_tag;
    bool ok;

    // CompletionQueue 事件循环: 带超时轮询以检查关闭信号
    while (true) {
        if (shutdown_requested_.load()) {
            spdlog::info("[GrpcServer] Shutdown signal received, initiating graceful shutdown...");
            Shutdown();
            break;
        }

        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(500);
        if (pimpl_->cq->AsyncNext(&raw_tag, &ok, deadline) == grpc::CompletionQueue::GOT_EVENT) {
            PlayerSession::EventTag* tag = static_cast<PlayerSession::EventTag*>(raw_tag);
            tag->instance->HandleEvent(tag->type, ok);
            delete tag;
        }
    }

    spdlog::info("[GrpcServer] Event loop exited");
    instance_ = nullptr;
}

} // namespace core
} // namespace game
