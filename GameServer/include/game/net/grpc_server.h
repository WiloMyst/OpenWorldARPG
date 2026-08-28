#pragma once
#include <atomic>
#include <memory>
#include <string>

#include "game/infra/config_manager.h"

namespace game {
    namespace infra { class ThreadPool; }
    namespace logic { class GameLogic; }
    namespace net { class DialogueRevoker; }
    namespace session { class SessionHandler; class SessionManager; }
    namespace world { class WorldManager; }
    namespace combat { class CombatManager; }
}

namespace game {
namespace net {

// gRPC 异步服务器: 全部 RPC 经 AsyncService + CompletionQueue 驱动 (构建环境为
// gRPC 1.30, 无 CallbackService Reactor API). 事件循环单线程派发 (AsyncNext 带超时
// 轮询关闭信号): World 流消息在 CQ 线程直接处理 (轻量内存操作), unary 业务含
// MySQL/Redis IO 经 ThreadPool 执行避免阻塞事件循环.
// RPC 形态按业务语义收敛:
//   unary              : Login/Logout/RegisterCharacter/SetActiveCharacter/
//                        RequestEnemySpawn/InventoryOp/AuthenticateDialogue (低频命令)
//   bidi-streaming     : World (实时玩法: 心跳/移动/伤害)
//   server-streaming   : StreamDialogue (AI 对话逐字返回)
// 控制面: 玩家会话终结时经 DialogueRevoker 通知 VHServer 吊销对话流
// 生命周期: Run() 阻塞直至收到 SIGINT/SIGTERM, 排空 CompletionQueue 后优雅关闭
class GrpcServer final {
public:
    GrpcServer();
    ~GrpcServer();

    void Run(const infra::AppConfig& config);
    void Shutdown();

private:
    // 7 个 unary 各自进入首个 accept 状态 (请求到达后由 UnaryCall 自续)
    void SpawnUnaryCalls();
    // CompletionQueue 事件循环: 带超时轮询以检查关闭信号
    void HandleRpcs();

    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    std::unique_ptr<infra::ThreadPool> pool_;
    // 声明先于 session_manager_: 析构晚于会话管理器, 会话终结回调不悬垂
    std::unique_ptr<DialogueRevoker> dialogue_revoker_;
    std::unique_ptr<session::SessionManager> session_manager_;
    std::unique_ptr<session::SessionHandler> handler_;
    std::unique_ptr<logic::GameLogic> logic_;
    std::unique_ptr<world::WorldManager> world_;
    std::unique_ptr<combat::CombatManager> combat_;

    static std::atomic<bool> shutdown_requested_;
    static GrpcServer* instance_;
};

} // namespace net
} // namespace game