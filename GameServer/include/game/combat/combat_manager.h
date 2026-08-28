#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "game.pb.h"
#include "game/data/owned_character.h"

#include <unordered_set>

namespace game {
    namespace infra { struct AppConfig; }
    namespace session { class SessionManager; }
}

namespace game {
namespace combat {

// 敌人静态配置, 对应客户端 AEnemyCharacter 属性 + 服务器 `enemies.yaml`
struct EnemyConfig {
    int32_t enemy_type = 0;
    std::string name;
    float max_hp = 100.0f;
    float attack = 10.0f;
    float defense = 0.0f;
    float patrol_radius = 700.0f;
};

// 技能静态配置, 对应客户端攻击 GA + 服务器 `skills.yaml`
struct SkillConfig {
    int32_t skill_id = 0;
    std::string name;
    float damage_multiplier = 1.0f;  // 基础伤害 = attack * multiplier
    float crit_rate = 0.05f;         // 暴击率
    float crit_dmg = 0.5f;           // 暴击额外伤害 (crit hit 时 damage *= 1+crit_dmg)
};

// 服务器敌人实体: 持服务器权威 HP, AI 行为留客户端 (巡逻/感知/动画)
struct EnemyEntity {
    uint64_t enemy_id = 0;
    int32_t enemy_type = 0;
    int32_t area_id = 0;
    float max_hp = 0.0f;
    float current_hp = 0.0f;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float patrol_radius = 0.0f;
    bool killed = false;
};

// 服务器角色 HP 状态: 队伍内每个角色一条, 由服务器唯一裁决口径维护, 客户端只读.
// 角色按 character_tag 识别, HP 初值由客户端生成该角色时上报, 之后增减全归服务器.
struct CharacterHpState {
    std::string character_tag;
    double max_hp = 0.0;
    double current_hp = 0.0;
    bool killed = false;
};

// 在线会话的战斗态: 登录时建, 按角色 Tag 持有队伍内各角色独立 HP;
// 仅 active 角色受击 (切人上报更新 active, 各角色血线独立持久)
struct PlayerSessionCombat {
    std::string active_character_tag;
    std::unordered_map<std::string, CharacterHpState> team;
    // 已从 owned_characters 恢复的拥有角色 tag 集合: 作为"该账号 DB 已有"的认账基线.
    // 客户端 RegisterCharacter 落库判据用"是否已在 DB"(IsDbOwned), 而非"是否做过恢复",
    // 否则老玩家会话恒为已恢复, 新获得角色永远不会被持久化.
    std::unordered_set<std::string> db_owned_tags;
};

// 战斗管理器: 加载敌人/技能配置 + 刷怪 + 权威伤害结算
// 玩家命中只上报"打了哪个 enemy_id + 什么技能", 不含数值;
// 伤害完全按技能/敌人/玩家属性配置裁决, 结果以 DamageDeal 广播.
class CombatManager {
public:
    explicit CombatManager(const infra::AppConfig& config);

    void SetSessionManager(session::SessionManager* sm) { session_manager_ = sm; }

    // 处理巡逻区刷怪上报 (unary RequestEnemySpawn): 客户端 AIPatrolAreaBase BeginPlay 时上报.
    // 服务器判断是否允许刷怪 (同一巡逻区去重授权), 允许后登记敌人并广播 EnemySpawn;
    // 授权结果经返回值回执请求方 (granted + 首个敌人 ID / 拒绝原因).
    game::EnemySpawnResponse HandleSpawnRequest(const game::EnemySpawnRequest& req);

    // 处理玩家攻击命中上报: 校验 -> 计算伤害 -> 扣 HP -> 广播 DamageDeal.
    // attacker_player_id 由服务器按会话归属解析后传入, 不信任客户端自报的身份字段;
    // 客户端 DamageIntent 里的 attacker_player_id 一律忽略 (反作弊: 玩家身份服务器定).
    void HandleDamageIntent(uint64_t attacker_player_id, const game::DamageIntent& intent);

    // 玩家登录时建立会话战斗态 (服务器权威, account 关联); 登出/掉线时移除
    void RegisterPlayer(const std::string& account);
    void UnregisterPlayer(const std::string& account);

    // 客户端生成一个 PlayerCharacter 时上报: 按角色 Tag 建/更新独立 HP 实体,
    // 初值 max_hp 即该角色结算后的 HP, 服务器持有并裁决后续增减
    void RegisterCharacter(const std::string& account, const std::string& character_tag,
                           double max_hp);

    // 客户端切人上报: 更新会话的 active 角色 (仅 active 受击)
    void SetActiveCharacter(const std::string& account, const std::string& character_tag);

    // 登录时从 DB(owned_characters) 恢复账号拥有角色到会话: 建立拥有关系判据的权威基线,
    // 并把 DB 记录的 active 角色设为当前 active (客户端后续 RegisterCharacter/SetActiveCharacter 叠加)
    void RestoreOwnedCharacters(const std::string& account,
                                const std::vector<game::data::OwnedCharacter>& owned);

    // HP 存档恢复: 登录读档时注入各角色血线, 客户端 RegisterCharacter 建档后用于覆盖初始 HP
    struct HpRestoreEntry {
        std::string character_tag;
        double max_hp = 0.0;
        double current_hp = 0.0;
    };
    void RestoreCharacterHp(const std::string& account,
                            const std::vector<HpRestoreEntry>& entries);

    // HP 异步存档回调 (由装配层注入, 接 GameLogic::SaveCharacterHp):
    // 高血线结算经节流异步落库, 不阻塞战斗线程.
    using HpPersistCallback =
        std::function<void(const std::string& account, const std::string& character_tag,
                           double max_hp, double current_hp)>;
    void SetHpPersistCallback(HpPersistCallback cb) { hp_persist_cb_ = std::move(cb); }

    // 某角色是否在 DB 拥有集合中 (供角色落库决策)
    bool IsDbOwned(const std::string& account, const std::string& character_tag) const;

    // 队伍角色数 / 某角色是否拥有 (供测试与落库决策)
    int32_t TeamSize(const std::string& account) const;
    bool HasCharacter(const std::string& account, const std::string& character_tag) const;

    // 处理敌人攻击命中上报: 只结算会话内 active 角色的 HP, 广播 PlayerDamage
    void HandleEnemyAttackIntent(const std::string& account,
                                 const game::EnemyAttackIntent& intent);

    // 查询会话内某角色的 HP (供测试断言)
    bool GetCharacterHp(const std::string& account, const std::string& character_tag,
                        double* current_hp, double* max_hp) const;

    // 查询会话当前 active 角色 Tag (供测试断言)
    bool GetActiveCharacter(const std::string& account, std::string* character_tag) const;

    // 查询敌人, 供测试 (拷贝快照, 避免锁外悬垂)
    bool GetEnemy(uint64_t enemy_id, EnemyEntity* out) const;

    int32_t enemy_count() const { return static_cast<int32_t>(enemies_.size()); }

    // 敌人类型静态配置 (供测试断言)
    const EnemyConfig* FindEnemyConfig(int32_t enemy_type) const;

private:
    uint64_t NextEnemyId() { return ++next_enemy_id_; }

    float CurrentHpFor(float max_hp) const { return max_hp; }

    const SkillConfig* FindSkill(int32_t skill_id) const;

    // 向在线玩家广播服务器消息
    void SendToAll(const game::ServerMessage& msg);

    // HP 异步存档 (节流): 调用方需已持有 mtx_; 非阻塞入队写者
    void MaybePersistHpLocked(const std::string& account, const std::string& character_tag,
                              double max_hp, double current_hp);

    mutable std::mutex mtx_;
    session::SessionManager* session_manager_ = nullptr;

    // 已允许刷怪的巡逻区, 用于重复上报去重
    std::unordered_set<int32_t> spawned_areas_;

    std::unordered_map<int32_t, EnemyConfig> enemy_configs_;
    std::unordered_map<int32_t, SkillConfig> skill_configs_;

    std::unordered_map<uint64_t, EnemyEntity> enemies_;
    uint64_t next_enemy_id_ = 0;

    // 在线会话战斗态: account -> 会话, 会话内按角色 Tag 持独立 HP 实体
    std::unordered_map<std::string, PlayerSessionCombat> sessions_;

    // 玩家的服务器权威属性 (客户端 AS_Player 只有 HP, 攻击/防御由服务器持)
    float default_player_attack_ = 30.0f;
    int initial_enemy_count_ = 3;

    // HP 异步存档: 节流 + 每账号上次落库时刻; 恢复期暂存的血线 (登录读档注入, RegisterCharacter 时应用)
    HpPersistCallback hp_persist_cb_;
    std::unordered_map<std::string, int64_t> last_hp_persist_ms_;
    std::unordered_map<std::string, std::unordered_map<std::string, HpRestoreEntry>>
        pending_hp_restore_;
    static constexpr int64_t kHpPersistMs = 200;
};

} // namespace combat
} // namespace game