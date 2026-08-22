#pragma once
#include <atomic>
#include <memory>
#include <string>

#include "game/infra/config_manager.hpp"

namespace game {
    namespace infra { class ThreadPool; }
    namespace logic { class GameLogic; }
    namespace session { class SessionManager; }
}

namespace game {
namespace core {

// gRPC 服务器: 单 CompletionQueue 事件循环 + worker 线程池处理消息
// 生命周期: Run() 阻塞运行, 收到 SIGINT/SIGTERM 后优雅关闭
class GrpcServer final {
public:
    GrpcServer();
    ~GrpcServer();

    void Run(const infra::AppConfig& config);
    void Shutdown();

private:
    void HandleRpcs();

    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    std::unique_ptr<infra::ThreadPool> pool_;
    std::unique_ptr<session::SessionManager> session_manager_;
    std::unique_ptr<logic::GameLogic> logic_;

    static std::atomic<bool> shutdown_requested_;
    static GrpcServer* instance_;
};

} // namespace core
} // namespace game
