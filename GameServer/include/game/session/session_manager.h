#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace game {
    namespace infra { struct AppConfig; }
    namespace world { class WorldManager; }
    namespace combat { class CombatManager; }
    class ServerMessage;
}

namespace game {
namespace session {

class ISession;

// 会话管理器: 统一登记 gRPC 会话, 维护账号 -> 会话绑定
// 职责: 同账号重复登录踢旧 / 心跳超时踢出 / 登录超时清理 / 断线重连保留
// 线程模型: 登记与解绑来自 gRPC 回调线程与 worker 线程, 超时扫描由独立后台线程执行
class SessionManager {
public:
    explicit SessionManager(const infra::AppConfig& config);
    ~SessionManager();

    void Start();
    void Stop();

    // World 流建立时登记 (未进入世界/未绑定账号)
    void RegisterSession(std::shared_ptr<ISession> session);

    // 登录成功后绑定账号; 同账号旧连接被踢下线
    // 返回 true = 重连恢复 (gRPC 通道恒 false), false = 全新登录
    bool BindAccount(const std::string& account, std::shared_ptr<ISession> session,
                     const std::string& resume_token = "");

    // unary Login 建档后登记待挂载令牌及其登录上下文; 客户端开 World 流携带令牌
    // 兑换为在线会话 (登录与会话建立分离). 引脚 session_token 恒有效, 供后续
    // unary 业务 (RegisterCharacter/切人/背包/对话授权) 以 Bearer 令牌鉴权.
    struct PendingLogin {
        std::string account;
        uint64_t player_id = 0;
        float spawn[4] = {0, 0, 0, 0};
    };
    void RegisterPendingLogin(const std::string& token, const PendingLogin& ctx);
    // World 流兑换: 取出登录上下文 (account/player_id/出生点); 令牌未登记返回 false
    bool ClaimLogin(const std::string& token, PendingLogin* out);
    // unary 鉴权: 令牌已登记且对应账号当前在线 (已挂载 World 流), 输出账号
    bool AuthenticateUnary(const std::string& token, std::string* account);

    // 会话关闭回调 (正常退出/登出/断连/心跳超时), 由会话实现调用
    // 只需绑仍指向该会话的账号, 避免误删重复登录后的新绑定.
    // 断线(非显式登出)登记重连保留窗口, 业务态暂不清理; 显式登出立即完整释放
    void OnSessionClosed(std::shared_ptr<ISession> session);

    // 断线重连接管: 账号处于重连保留窗口且世界实体仍存 -> 消费窗口返回 true,
    // 由调用方 (SessionHandler::EnterWorld) 走 PlayerResume 接管实体而非重建. 返回 false = 全新进入
    bool TakeReconnect(const std::string& account);

    // 按账号断开在线会话 (unary Logout 用): 标记显式登出后关闭其 World 流
    void CloseByAccount(const std::string& account);

    // 向在线账号推送服务器消息 (AOI 广播等); 未在线/会话不存在则静默丢弃
    void SendToAccount(const std::string& account, const game::ServerMessage& msg);

    // 向所有在线账号广播服务器消息 (战斗等无差别广播)
    void BroadcastOnline(const game::ServerMessage& msg);

    // 查询在线账号绑定的会话 id; 未在线返回 0
    uint64_t GetSessionId(const std::string& account);

    // 注入世界管理器: 会话关闭时通知世界移除玩家 (AOI 清理)
    void SetWorldManager(world::WorldManager* world) { world_ = world; }

    // 注入战斗管理器: 玩家离开世界 (登出/踢出/断线/心跳超时) 时清理其会话战斗态,
    // 并由 CombatManager 在"最后玩家离开"时重置世界刷怪状态 (下次进入重新刷怪)
    void SetCombatManager(combat::CombatManager* combat) { combat_ = combat; }

    // 对话流吊销联动: 账号解绑 (登出/踢出/断线/心跳超时) 时回调, 通知 VHServer 吊销
    // 该账号已建立的对话流. 回调须非阻塞 (入队语义), 由传输层在 mtx_ 外调用; 未设置则不联动
    void SetDialogueRevokeCallback(std::function<void(const std::string&)> cb) {
        dialogue_revoke_cb_ = std::move(cb);
    }

private:
    void CheckLoop();

    // ---- 配置 ----
    int64_t heartbeat_timeout_ms_;
    int64_t login_timeout_ms_;
    int64_t reconnect_grace_ms_;
    int64_t check_interval_ms_;

    // ---- 后台扫描线程 ----
    std::thread check_thread_;
    std::atomic<bool> stopped_{true};

    // ---- 会话登记表 (mtx_ 保护) ----
    std::mutex mtx_;
    std::unordered_map<uint64_t, std::weak_ptr<ISession>> sessions_;
    std::unordered_map<std::string, std::weak_ptr<ISession>> account_map_;
    // unary Login 签发的待挂载令牌: token -> 登录上下文 (account/player_id/出生点);
    // World 流凭此兑换在线会话, 并供后续 unary 以 Bearer 令牌鉴权
    std::unordered_map<std::string, PendingLogin> login_pending_;

    // ---- 断线重连保留 (gRPC 会话壳随流终结销毁, 故保留账号业务态而非会话对象) ----
    // 断线(非登出/被踢)时: 暂不清理该账号的世界实体/战斗态/token/对话流, 登记到此表,
    // 窗口内 (reconnect_grace_ms_) 客户端重连经 TakeReconnect + PlayerResume 无缝接管;
    // 窗口超时由 CheckLoop 消费兜底 (PlayerLeave + UnregisterPlayer + 清token + 吊销对话)
    struct ReconnectHold {
        uint64_t session_id = 0;   // 断线方会话 id, 供超时按协放世界实体 (PlayerLeave 需匹配)
        int64_t deadline_ms = 0;   // 保留截止时刻 (ms)
    };
    std::unordered_map<std::string, ReconnectHold> reconnect_hold_;  // account -> 保留状态

    // ---- 世界同步联动 ----
    world::WorldManager* world_ = nullptr;

    // ---- 战斗态联动 ----
    combat::CombatManager* combat_ = nullptr;

    // ---- VHServer 对话流吊销联动 (mtx_ 外调用, 非阻塞) ----
    std::function<void(const std::string&)> dialogue_revoke_cb_;
};

} // namespace session
} // namespace game
