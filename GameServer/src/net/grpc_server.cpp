#include "game/net/grpc_server.h"
#include "game/net/call_base.h"
#include "game/net/player_session.h"
#include "game/net/unary_call.h"
#include "game/net/dialogue_revoker.h"
#include "game/session/session_manager.h"
#include "game/session/session_handler.h"
#include "game/logic/game_logic.h"
#include "game/world/world_manager.h"
#include "game/combat/combat_manager.h"
#include "game/infra/thread_pool.h"
#include <spdlog/spdlog.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <csignal>
#include <thread>

namespace game {
namespace net {

namespace {

grpc::Status Unauthenticated() {
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "invalid or offline session");
}

} // namespace

// ==================== 服务器外壳 ====================

struct GrpcServer::Impl {
    // 全方法异步: unary/World/StreamDialogue 统一经 AsyncService + CompletionQueue 驱动
    game::GameService::AsyncService service;
    std::unique_ptr<grpc::ServerCompletionQueue> cq;
    std::unique_ptr<grpc::Server> server;
    std::atomic<bool> shutdown_done{false};
};

GrpcServer* GrpcServer::instance_ = nullptr;
std::atomic<bool> GrpcServer::shutdown_requested_{false};

GrpcServer::GrpcServer() : pimpl_(std::make_unique<Impl>()) {}

GrpcServer::~GrpcServer() {
    Shutdown();
}

void GrpcServer::Shutdown() {
    if (pimpl_->shutdown_done.exchange(true)) return;

    // 停扫描 -> 断流: 关服取消令在途事件以 ok=false 到达
    if (session_manager_) session_manager_->Stop();

    if (pimpl_->server) {
        spdlog::info("[GrpcServer] Shutting down gRPC server...");
        pimpl_->server->Shutdown();
    }

    // 线程池先于 CQ 关停: 在途 unary 业务完成后会向 CQ 投递 FINISH tag,
    // 必须保证全部投递发生在 cq->Shutdown() 之前 (向已关停的 CQ 投递是未定义行为)
    pool_.reset();

    // 排空 CQ: 会话收尾 (世界离开 / VHServer 对话吊销入队) 在此过程中完成
    if (pimpl_->cq) {
        pimpl_->cq->Shutdown();
        void* raw_tag;
        bool ok;
        while (pimpl_->cq->Next(&raw_tag, &ok)) {
            auto* tag = static_cast<CallBase::EventTag*>(raw_tag);
            tag->call->HandleEvent(tag->type, ok);
            delete tag;
        }
    }

    // 会话收尾事件已处理完毕, 排空在途吊销通知再退出
    if (dialogue_revoker_) dialogue_revoker_->Stop();

    // 依赖析构顺序: 会话/世界/战斗先于逻辑 (回调引用逻辑)
    combat_.reset();
    world_.reset();
    handler_.reset();
    session_manager_.reset();
    dialogue_revoker_.reset();
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
    world_ = std::make_unique<world::WorldManager>(config);
    combat_ = std::make_unique<combat::CombatManager>(config);
    combat_->SetSessionManager(session_manager_.get());

    // 连续状态异步存档装配: 位置(WorldManager 节流) / HP(CombatManager 节流) 经回调异步落库
    world_->SetPositionPersistCallback(
        [this](const std::string& a, float x, float y, float z, float yaw) {
            game::storage::PlayerPosition p;
            p.x = x;
            p.y = y;
            p.z = z;
            p.yaw = yaw;
            logic_->SavePlayerPosition(a, p);
        });
    combat_->SetHpPersistCallback(
        [this](const std::string& a, const std::string& tag, double max_hp, double cur_hp) {
            logic_->SaveCharacterHp(a, tag, static_cast<float>(max_hp),
                                    static_cast<float>(cur_hp));
        });

    handler_ = std::make_unique<session::SessionHandler>(logic_.get(), session_manager_.get(),
                                                         world_.get(), combat_.get());
    world_->SetSessionManager(session_manager_.get());
    session_manager_->SetWorldManager(world_.get());
    session_manager_->SetCombatManager(combat_.get());

    // 对话流吊销联动 (控制面): 账号在 GameServer 侧终结 (登出/踢出/断线/超时) 时
    // 通知 VHServer 吊销已建立对话流, 封堵"被踢后凭未过期票据继续推理"的残留窗口;
    // endpoint 或 secret 未配置则整体禁用 (纯玩法服部署形态)
    if (!config.vhserver_admin_endpoint.empty() && !config.vhserver_admin_secret.empty()) {
        dialogue_revoker_ = std::make_unique<DialogueRevoker>(config.vhserver_admin_endpoint,
                                                              config.vhserver_admin_secret);
        dialogue_revoker_->Start();
        session_manager_->SetDialogueRevokeCallback(
            [revoker = dialogue_revoker_.get()](const std::string& account) {
                revoker->Revoke(account);
            });
    }

    session_manager_->Start();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.SetMaxReceiveMessageSize(16 * 1024 * 1024);
    builder.SetMaxSendMessageSize(16 * 1024 * 1024);
    builder.RegisterService(&pimpl_->service);
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
    pimpl_->cq = builder.AddCompletionQueue();

    pimpl_->server = builder.BuildAndStart();
    if (!pimpl_->server) {
        spdlog::critical("[GrpcServer] Failed to start on {}", server_address);
        return;
    }
    spdlog::info("[GrpcServer] Server listening on {}", server_address);

    // 各 RPC 进入首个 accept 状态 (到达后自续, 无需重复投放):
    //   WorldSession: World 双向流 (实时玩法)
    //   UnaryCall x7: 低频命令 (业务经线程池执行)
    //   DialogueCall: StreamDialogue 占位
    WorldSession::Spawn(&pimpl_->service, pimpl_->cq.get(), session_manager_.get(),
                        handler_.get());
    SpawnUnaryCalls();
    DialogueCall::Spawn(&pimpl_->service, pimpl_->cq.get(), session_manager_.get());

    instance_ = this;
    shutdown_requested_.store(false);
    std::signal(SIGINT, [](int) { shutdown_requested_.store(true); });
    std::signal(SIGTERM, [](int) { shutdown_requested_.store(true); });

    HandleRpcs();

    spdlog::info("[GrpcServer] Event loop exited");
    instance_ = nullptr;
}

void GrpcServer::SpawnUnaryCalls() {
    using AsyncService = game::GameService::AsyncService;  // AsyncService 是嵌套成员类型, 只能别名引入
    auto* svc = &pimpl_->service;
    auto* cq = pimpl_->cq.get();
    auto* pool = pool_.get();
    auto* sm = session_manager_.get();
    auto* handler = handler_.get();

    // Login: 鉴权建档 (不要求已挂载 World 流)
    UnaryCall<game::LoginRequest, game::LoginResponse>::Spawn(
        svc, cq, pool, &AsyncService::RequestLogin,
        [handler](const std::string&, const game::LoginRequest& req,
                  game::LoginResponse* resp) -> grpc::Status {
            std::string account;
            handler->PrepareLogin(req, &account, resp);
            return grpc::Status::OK;   // 业务失败经 resp.success() 表达, 不视为传输错误
        });

    // 其余 unary: Bearer 令牌鉴权 (须已挂载在线 World 流) 后编排业务
    UnaryCall<game::LogoutRequest, game::LogoutResponse>::Spawn(
        svc, cq, pool, &AsyncService::RequestLogout,
        [sm, handler](const std::string& token, const game::LogoutRequest&,
                      game::LogoutResponse* resp) -> grpc::Status {
            std::string account;
            if (!sm->AuthenticateUnary(token, &account)) return Unauthenticated();
            *resp = handler->HandleLogout(account);
            return grpc::Status::OK;
        });

    UnaryCall<game::RegisterCharacterRequest, game::RegisterCharacterResponse>::Spawn(
        svc, cq, pool, &AsyncService::RequestRegisterCharacter,
        [sm, handler](const std::string& token, const game::RegisterCharacterRequest& req,
                      game::RegisterCharacterResponse* resp) -> grpc::Status {
            std::string account;
            if (!sm->AuthenticateUnary(token, &account)) return Unauthenticated();
            *resp = handler->HandleRegisterCharacter(account, req);
            return grpc::Status::OK;
        });

    UnaryCall<game::SetActiveCharacterRequest, game::SetActiveCharacterResponse>::Spawn(
        svc, cq, pool, &AsyncService::RequestSetActiveCharacter,
        [sm, handler](const std::string& token, const game::SetActiveCharacterRequest& req,
                      game::SetActiveCharacterResponse* resp) -> grpc::Status {
            std::string account;
            if (!sm->AuthenticateUnary(token, &account)) return Unauthenticated();
            *resp = handler->HandleSetActiveCharacter(account, req);
            return grpc::Status::OK;
        });

    UnaryCall<game::EnemySpawnRequest, game::EnemySpawnResponse>::Spawn(
        svc, cq, pool, &AsyncService::RequestRequestEnemySpawn,
        [sm, handler](const std::string& token, const game::EnemySpawnRequest& req,
                      game::EnemySpawnResponse* resp) -> grpc::Status {
            std::string account;
            if (!sm->AuthenticateUnary(token, &account)) return Unauthenticated();
            *resp = handler->HandleEnemySpawnRequest(account, req);
            return grpc::Status::OK;
        });

    UnaryCall<game::InventoryOpRequest, game::InventoryOpResponse>::Spawn(
        svc, cq, pool, &AsyncService::RequestInventoryOp,
        [sm, handler](const std::string& token, const game::InventoryOpRequest& req,
                      game::InventoryOpResponse* resp) -> grpc::Status {
            std::string account;
            if (!sm->AuthenticateUnary(token, &account)) return Unauthenticated();
            *resp = handler->HandleInventoryOp(account, req);
            return grpc::Status::OK;
        });

    UnaryCall<game::DialogueAuthRequest, game::DialogueAuthResult>::Spawn(
        svc, cq, pool, &AsyncService::RequestAuthenticateDialogue,
        [sm, handler](const std::string& token, const game::DialogueAuthRequest& req,
                      game::DialogueAuthResult* resp) -> grpc::Status {
            std::string account;
            if (!sm->AuthenticateUnary(token, &account)) return Unauthenticated();
            *resp = handler->HandleDialogueAuth(account, req);
            return grpc::Status::OK;
        });
}

void GrpcServer::HandleRpcs() {
    void* raw_tag;
    bool ok;
    while (true) {
        if (shutdown_requested_.load()) {
            spdlog::info("[GrpcServer] Shutdown signal received, initiating graceful shutdown...");
            Shutdown();
            break;
        }

        // 带超时轮询: 事件到达即派发; 无事件时定期检查关闭信号
        const auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(500);
        if (pimpl_->cq->AsyncNext(&raw_tag, &ok, deadline) == grpc::CompletionQueue::GOT_EVENT) {
            auto* tag = static_cast<CallBase::EventTag*>(raw_tag);
            tag->call->HandleEvent(tag->type, ok);
            delete tag;
        }
    }
}

} // namespace net
} // namespace game
