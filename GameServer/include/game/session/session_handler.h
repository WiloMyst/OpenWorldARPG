#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "game.pb.h"

namespace game {
    namespace logic { class GameLogic; }
    namespace session { class ISession; class SessionManager; }
    namespace world { class WorldManager; }
    namespace combat { class CombatManager; }
}

namespace game {
namespace session {

// gRPC 单通道下的业务处理. 对外协议为拆分后的各 RPC (unary + World 双向流),
// 本层消费这些协议消息并编排 GameLogic / WorldManager / CombatManager 与会话;
// 下行推送统一以内部事件容器 ServerMessage 下发 (经 SessionManager -> ISession).
class SessionHandler {
public:
    SessionHandler(logic::GameLogic* logic, SessionManager* session_manager,
                   world::WorldManager* world, combat::CombatManager* combat);

    // ---- unary: Login ----
    // 建档 + 装载权威快照 + 登记待挂载令牌 (session_token). 不下发世界/不绑定账号;
    // 成功后填充 resp (player_id/spawn/session_token/owned/team/inventory), 输出 account
    bool PrepareLogin(const game::LoginRequest& req, std::string* account, game::LoginResponse* resp);

    // ---- World 流令牌兑换后的挂载: 绑定账号 + 进入世界 ----
    void EnterWorld(const std::shared_ptr<ISession>& session,
                    const std::string& account, uint64_t player_id, const float spawn[4]);

    // ---- unary: 低频命令 (均以账号为上下文, 账号身份由令牌鉴权在传输层解析) ----
    game::LogoutResponse HandleLogout(const std::string& account);
    game::RegisterCharacterResponse HandleRegisterCharacter(const std::string& account,
                                                            const game::RegisterCharacterRequest& req);
    game::SetActiveCharacterResponse HandleSetActiveCharacter(const std::string& account,
                                                              const game::SetActiveCharacterRequest& req);
    game::EnemySpawnResponse HandleEnemySpawnRequest(const std::string& account,
                                                     const game::EnemySpawnRequest& req);
    game::InventoryOpResponse HandleInventoryOp(const std::string& account,
                                                const game::InventoryOpRequest& req);
    game::DialogueAuthResult HandleDialogueAuth(const std::string& account,
                                                const game::DialogueAuthRequest& req);

    // ---- World 双向流: 实时玩法分发 (心跳/移动/攻击命中) ----
    void HandleWorldMessage(const std::shared_ptr<ISession>& session, const game::ClientWorld& msg);

    // 通用错误回执 (经 World 流下发)
    void SendError(const std::shared_ptr<ISession>& session, uint64_t seq, int code,
                   const std::string& message);

private:
    void ResolveSpawnOrDefault(const std::string& account, float out[4]);

    logic::GameLogic* logic_;
    SessionManager* session_manager_;
    world::WorldManager* world_;
    combat::CombatManager* combat_;
};

} // namespace session
} // namespace game