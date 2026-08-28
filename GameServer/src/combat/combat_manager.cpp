#include "game/combat/combat_manager.h"
#include "game/infra/config_manager.h"
#include "game/infra/time_utils.h"
#include "game/session/session_manager.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace game {
namespace combat {

namespace {

// 从 YAML 序列加载敌人配置表
std::unordered_map<int32_t, EnemyConfig> LoadEnemyConfigs(const std::string& path) {
    std::unordered_map<int32_t, EnemyConfig> out;
    try {
        YAML::Node root = YAML::LoadFile(path);
        for (const auto& n : root["enemies"]) {
            EnemyConfig c;
            c.enemy_type = n["enemy_type"].as<int32_t>();
            c.name = n["name"].as<std::string>(c.name);
            c.max_hp = n["max_hp"].as<float>(c.max_hp);
            c.attack = n["attack"].as<float>(c.attack);
            c.defense = n["defense"].as<float>(c.defense);
            c.patrol_radius = n["patrol_radius"].as<float>(c.patrol_radius);
            out[c.enemy_type] = c;
        }
    } catch (const YAML::Exception& e) {
        spdlog::error("[Combat] enemy config load failed [{}] {}", path, e.what());
    }
    return out;
}

// 从 YAML 序列加载技能配置表
std::unordered_map<int32_t, SkillConfig> LoadSkillConfigs(const std::string& path) {
    std::unordered_map<int32_t, SkillConfig> out;
    try {
        YAML::Node root = YAML::LoadFile(path);
        for (const auto& n : root["skills"]) {
            SkillConfig c;
            c.skill_id = n["skill_id"].as<int32_t>();
            c.name = n["name"].as<std::string>(c.name);
            c.damage_multiplier = n["damage_multiplier"].as<float>(c.damage_multiplier);
            c.crit_rate = n["crit_rate"].as<float>(c.crit_rate);
            c.crit_dmg = n["crit_dmg"].as<float>(c.crit_dmg);
            out[c.skill_id] = c;
        }
    } catch (const YAML::Exception& e) {
        spdlog::error("[Combat] skill config load failed [{}] {}", path, e.what());
    }
    return out;
}

} // namespace

CombatManager::CombatManager(const infra::AppConfig& config)
    : enemy_configs_(LoadEnemyConfigs(config.enemies_config_path)),
      skill_configs_(LoadSkillConfigs(config.skills_config_path)) {
    default_player_attack_ = config.default_player_attack;
    initial_enemy_count_ = config.initial_enemy_count;
}

game::EnemySpawnResponse CombatManager::HandleSpawnRequest(const game::EnemySpawnRequest& req) {
    game::EnemySpawnResponse resp;
    std::lock_guard<std::mutex> lock(mtx_);

    if (enemy_configs_.empty()) {
        resp.set_granted(false);
        resp.set_reason("no enemy config");
        spdlog::warn("[Combat] spawn rejected: no enemy config");
        return resp;
    }
    // 同巡逻区去重: 一个世界一个 AIPatrolAreaBase 只需一次性刷怪
    if (spawned_areas_.count(req.area_id())) {
        resp.set_granted(false);
        resp.set_reason("area already spawned");
        spdlog::warn("[Combat] spawn rejected: area already spawned [area_id={}]", req.area_id());
        return resp;
    }
    spawned_areas_.insert(req.area_id());

    const auto& [type, cfg] = *enemy_configs_.begin();
    // 巡逻半径: 优先用客户端上报的 PatrolRadius (保证刷点在圈内),
    // 上报非正时回退到敌人配置的巡逻半径.
    const float patrol_radius =
        req.patrol_radius() > 0.0f ? req.patrol_radius() : cfg.patrol_radius;
    // 散开偏移半径取巡逻半径的一部分, 使所有敌人落在以巡逻区为圆心的圆内:
    // 圆周均分角度保证节点互不堆叠, 偏移量 max(0.35*patrol_radius, 1) 恒 < 巡逻半径.
    const float spread_radius = std::max(patrol_radius * 0.35f, 1.0f);
    uint64_t first_enemy_id = 0;
    for (int i = 0; i < initial_enemy_count_; ++i) {
        EnemyEntity e;
        e.enemy_id = NextEnemyId();
        if (first_enemy_id == 0) first_enemy_id = e.enemy_id;
        e.enemy_type = type;
        e.area_id = req.area_id();
        e.max_hp = cfg.max_hp;
        e.current_hp = CurrentHpFor(cfg.max_hp);
        // 刷点 = 巡逻区圆心 + 水平面(圆周均分)偏移, 严格落在 patrol_radius 内;
        // 偏移必须落在水平面 (x/y), z 保持巡逻区高度, 否则敌人会随角度陷入地面/悬空
        const double ang = 2.0 * M_PI * i / initial_enemy_count_;
        e.x = req.x() + spread_radius * std::cos(ang);
        e.y = req.y() + spread_radius * std::sin(ang);
        e.z = req.z();
        e.patrol_radius = patrol_radius;
        e.killed = false;
        enemies_[e.enemy_id] = e;

        game::ServerMessage msg;
        msg.set_ack_sequence(0);
        auto* sp = msg.mutable_enemy_spawn();
        sp->set_enemy_id(e.enemy_id);
        sp->set_area_id(req.area_id());
        sp->set_enemy_type(type);
        sp->set_count(initial_enemy_count_);
        sp->set_spawn_x(e.x);
        sp->set_spawn_y(e.y);
        sp->set_spawn_z(e.z);
        sp->set_max_hp(static_cast<int32_t>(cfg.max_hp));
        sp->set_patrol_radius(static_cast<int32_t>(patrol_radius));
        SendToAll(msg);
    }
    resp.set_granted(true);
    resp.set_enemy_id(first_enemy_id);
    spdlog::info("[Combat] area [{}] spawned {} enemies of type {} (patrol_r={:.0f}, spread={:.0f})",
                 req.area_id(), enemies_.size(), type, patrol_radius, spread_radius);
    return resp;
}

void CombatManager::HandleDamageIntent(uint64_t attacker_player_id,
                                       const game::DamageIntent& intent) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto eit = enemies_.find(intent.target_enemy_id());
    if (eit == enemies_.end()) {
        spdlog::warn("[Combat] damage rejected: enemy not found [enemy_id={}]",
                     intent.target_enemy_id());
        return;
    }
    EnemyEntity& enemy = eit->second;
    if (enemy.killed) {
        spdlog::warn("[Combat] damage rejected: enemy already killed [enemy_id={}]",
                     intent.target_enemy_id());
        return;
    }

    const SkillConfig* skill = FindSkill(intent.skill_id());
    if (!skill) {
        spdlog::warn("[Combat] damage rejected: unknown skill [skill_id={}]",
                     intent.skill_id());
        return;
    }

    // 伤害 = 玩家攻击 × 技能倍率 (客户端 AS_Player 只有 HP, 攻击由服务器持);
    // 目标敌人防御计入减伤; 暴击判定在服务器, 结果带 is_critical.
    const EnemyConfig* econf = FindEnemyConfig(enemy.enemy_type);
    const float defense = econf ? econf->defense : 0.0f;
    const float base = default_player_attack_ * skill->damage_multiplier;
    const float after_defense = std::max(0.0f, base - defense * 0.1f);

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    const bool is_critical = roll(rng) <= skill->crit_rate;
    const float damage =
        is_critical ? after_defense * (1.0f + skill->crit_dmg) : after_defense;

    enemy.current_hp = std::max(0.0f, enemy.current_hp - damage);
    const bool killed = enemy.current_hp <= 0.0f;
    enemy.killed = killed;

    game::ServerMessage msg;
    msg.set_ack_sequence(0);
    auto* dd = msg.mutable_damage_deal();
    dd->set_attacker_player_id(attacker_player_id);
    dd->set_target_enemy_id(enemy.enemy_id);
    dd->set_skill_id(skill->skill_id);
    dd->set_damage(damage);
    dd->set_target_current_hp(enemy.current_hp);
    dd->set_target_max_hp(enemy.max_hp);
    dd->set_target_killed(killed);
    dd->set_is_critical(is_critical);
    SendToAll(msg);

    spdlog::info("[Combat] damage [enemy_id={}, skill={}, damage={:.1f}{}, hp={:.1f}/{:.1f}]",
                 enemy.enemy_id, skill->name, damage, is_critical ? " CRIT" : "",
                 enemy.current_hp, enemy.max_hp);
}

void CombatManager::RegisterPlayer(const std::string& account) {
    std::lock_guard<std::mutex> lock(mtx_);
    // 会话战斗态: 建空队伍, 角色实体由客户端 RegisterCharacter 上报建立
    sessions_[account];
    spdlog::info("[Combat] player session registered [account={}]", account);
}

void CombatManager::UnregisterPlayer(const std::string& account) {
    std::lock_guard<std::mutex> lock(mtx_);
    // 幂等: 登出 (HandleLogout) 与断线 (OnSessionClosed) 双路径可能重复调用
    auto it = sessions_.find(account);
    if (it == sessions_.end()) return;
    sessions_.erase(it);

    // 最后一个在线玩家离开: 重置世界刷怪状态, 下次进入世界重新刷怪.
    // spawned_areas_ 是进程级去重, 若不随"世界清空"重置, 客户端重进关卡时
    // 巡逻区刷怪请求会被拒 (area already spawned), 导致新位置不再生成敌人.
    if (sessions_.empty()) {
        spawned_areas_.clear();
        enemies_.clear();
        spdlog::info("[Combat] last player left, world spawn state reset");
    }
    spdlog::info("[Combat] player session unregistered [account={}]", account);
}

void CombatManager::RegisterCharacter(const std::string& account,
                                      const std::string& character_tag,
                                      double max_hp) {
    if (character_tag.empty() || max_hp <= 0.0) {
        spdlog::warn("[Combat] character register rejected: bad args [account={}, tag={}, max_hp={:.0f}]",
                     account, character_tag, max_hp);
        return;
    }
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    if (sit == sessions_.end()) {
        spdlog::warn("[Combat] character register rejected: session offline [account={}]", account);
        return;
    }
    CharacterHpState& ch = sit->second.team[character_tag];
    ch.character_tag = character_tag;
    ch.max_hp = max_hp;
    ch.current_hp = std::min(ch.current_hp, max_hp);
    if (ch.current_hp <= 0.0) ch.current_hp = max_hp;
    ch.killed = false;

    // HP 存档恢复: 登录读档注入的血线覆盖客户端上报初值 (仅建档时若有存档)
    auto pr = pending_hp_restore_.find(account);
    if (pr != pending_hp_restore_.end()) {
        auto it = pr->second.find(character_tag);
        if (it != pr->second.end()) {
            if (it->second.max_hp > 0.0) ch.max_hp = it->second.max_hp;
            if (it->second.current_hp >= 0.0) {
                ch.current_hp = it->second.current_hp;
                ch.killed = false;
            }
            pr->second.erase(it);
            if (pr->second.empty()) pending_hp_restore_.erase(pr);
        }
    }

    // 首个上报的角色默认设为 active, 后续由客户端 SetActiveCharacter 明确切换
    if (sit->second.active_character_tag.empty()) {
        sit->second.active_character_tag = character_tag;
    }
    spdlog::info("[Combat] character registered [account={}, tag={}, max_hp={:.0f}, active={}]",
                 account, character_tag, ch.max_hp, sit->second.active_character_tag);

    // 建档血线必落库 (不节流): 登录时多个角色在爆发窗口内注册, 按账号节流会把部分角色的
    // 初始血线误吞; 建档低频, 直接异步入队写者. 战斗期连续扣血才走节流 (HandleEnemyAttackIntent).
    if (hp_persist_cb_) {
        hp_persist_cb_(account, character_tag, ch.max_hp, ch.current_hp);
    }
}

void CombatManager::SetActiveCharacter(const std::string& account,
                                       const std::string& character_tag) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    if (sit == sessions_.end()) {
        spdlog::warn("[Combat] set active rejected: session offline [account={}]", account);
        return;
    }
    if (sit->second.team.count(character_tag) == 0) {
        spdlog::warn("[Combat] set active rejected: character not registered [account={}, tag={}]",
                     account, character_tag);
        return;
    }
    sit->second.active_character_tag = character_tag;
    spdlog::info("[Combat] active character switched [account={}, tag={}]", account, character_tag);
}

bool CombatManager::GetCharacterHp(const std::string& account, const std::string& character_tag,
                                   double* current_hp, double* max_hp) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    if (sit == sessions_.end()) return false;
    auto cit = sit->second.team.find(character_tag);
    if (cit == sit->second.team.end()) return false;
    *current_hp = cit->second.current_hp;
    *max_hp = cit->second.max_hp;
    return true;
}

bool CombatManager::GetActiveCharacter(const std::string& account,
                                       std::string* character_tag) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    if (sit == sessions_.end()) return false;
    *character_tag = sit->second.active_character_tag;
    return !sit->second.active_character_tag.empty();
}

void CombatManager::RestoreOwnedCharacters(const std::string& account,
                                           const std::vector<game::data::OwnedCharacter>& owned) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& sess = sessions_[account];

    // 恢复拥有关系: 只建立"账号应有角色"的判据, 不触碰 max_hp/current_hp (HP 仍由
    // 客户端 RegisterCharacter 按结算初值上报). 已存在的角色保持原本注册状态.
    for (const auto& oc : owned) {
        auto& ch = sess.team[oc.character_tag];
        if (ch.character_tag.empty()) {
            ch.character_tag = oc.character_tag;
            ch.max_hp = 0.0;
            ch.current_hp = 0.0;
        }
        sess.db_owned_tags.insert(oc.character_tag);
        if (oc.is_active) {
            sess.active_character_tag = oc.character_tag;
        }
    }
    spdlog::info("[Combat] restore {} owned characters [account={}, active={}]",
                 owned.size(), account,
                 sess.active_character_tag.empty() ? "(none)" : sess.active_character_tag);
}

void CombatManager::RestoreCharacterHp(const std::string& account,
                                       const std::vector<HpRestoreEntry>& entries) {
    if (entries.empty()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    auto& stash = pending_hp_restore_[account];
    for (const auto& e : entries) {
        stash[e.character_tag] = e;
    }
    spdlog::info("[Combat] hp restore staged [account={}, count={}]", account, entries.size());
}

void CombatManager::MaybePersistHpLocked(const std::string& account,
                                         const std::string& character_tag,
                                         double max_hp, double current_hp) {
    if (!hp_persist_cb_) return;
    const int64_t now = infra::NowMs();
    if (now - last_hp_persist_ms_[account] < kHpPersistMs) {
        return;
    }
    last_hp_persist_ms_[account] = now;
    hp_persist_cb_(account, character_tag, max_hp, current_hp);
}

int32_t CombatManager::TeamSize(const std::string& account) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    return sit == sessions_.end() ? -1 : static_cast<int32_t>(sit->second.team.size());
}

bool CombatManager::HasCharacter(const std::string& account,
                                 const std::string& character_tag) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    return sit != sessions_.end() && sit->second.team.count(character_tag) > 0;
}

bool CombatManager::IsDbOwned(const std::string& account,
                              const std::string& character_tag) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto sit = sessions_.find(account);
    return sit != sessions_.end() && sit->second.db_owned_tags.count(character_tag) > 0;
}

void CombatManager::HandleEnemyAttackIntent(const std::string& account,
                                            const game::EnemyAttackIntent& intent) {
    std::lock_guard<std::mutex> lock(mtx_);

    // 敌人须为服务器已刷出且存活
    auto eit = enemies_.find(intent.enemy_id());
    if (eit == enemies_.end()) {
        spdlog::warn("[Combat] enemy attack rejected: enemy not found [enemy_id={}]",
                     intent.enemy_id());
        return;
    }
    const EnemyEntity& enemy = eit->second;
    if (enemy.killed) {
        spdlog::warn("[Combat] enemy attack rejected: enemy dead [enemy_id={}]",
                     intent.enemy_id());
        return;
    }

    // 只结算会话内 active 角色, 其余角色血线不动
    auto sit = sessions_.find(account);
    if (sit == sessions_.end()) {
        spdlog::warn("[Combat] enemy attack rejected: session offline [account={}]", account);
        return;
    }
    if (sit->second.active_character_tag.empty()) {
        spdlog::warn("[Combat] enemy attack rejected: no active character [account={}]", account);
        return;
    }
    auto cit = sit->second.team.find(sit->second.active_character_tag);
    if (cit == sit->second.team.end()) {
        spdlog::warn("[Combat] enemy attack rejected: active character not found [account={}, tag={}]",
                     account, sit->second.active_character_tag);
        return;
    }
    CharacterHpState& player = cit->second;
    if (player.current_hp <= 0.0) {
        return; // 已死亡角色不再结算受伤
    }

    // 伤害 = 敌人攻击 (enemies.yaml), 可带暴击判定; 玩家无防御时直接扣除
    const EnemyConfig* econf = FindEnemyConfig(enemy.enemy_type);
    const float atk = econf ? econf->attack : 10.0f;

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    const bool is_critical = roll(rng) <= 0.05f;   // 敌人 5% 暴击
    const float damage = is_critical ? atk * 1.5f : atk;

    player.current_hp = std::max(0.0, player.current_hp - damage);
    const bool killed = player.current_hp <= 0.0;
    player.killed = killed;

    game::ServerMessage msg;
    msg.set_ack_sequence(0);
    auto* pd = msg.mutable_player_damage();
    pd->set_character_tag(player.character_tag);
    pd->set_attacker_enemy_id(enemy.enemy_id);
    pd->set_damage(damage);
    pd->set_current_hp(player.current_hp);
    pd->set_max_hp(player.max_hp);
    pd->set_killed(killed);
    pd->set_is_critical(is_critical);
    // 只下发给受伤者所属会话
    if (session_manager_) session_manager_->SendToAccount(account, msg);

    spdlog::info("[Combat] player hurt [account={}, tag={}, enemy_id={}, damage={:.1f}{}, hp={:.0f}/{:.0f}{}]",
                 account, player.character_tag, enemy.enemy_id, damage, is_critical ? " CRIT" : "",
                 player.current_hp, player.max_hp, killed ? " KILLED" : "");

    MaybePersistHpLocked(account, player.character_tag, player.max_hp, player.current_hp);
}

bool CombatManager::GetEnemy(uint64_t enemy_id, EnemyEntity* out) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = enemies_.find(enemy_id);
    if (it == enemies_.end()) return false;
    *out = it->second;
    return true;
}

const EnemyConfig* CombatManager::FindEnemyConfig(int32_t enemy_type) const {
    auto it = enemy_configs_.find(enemy_type);
    return it == enemy_configs_.end() ? nullptr : &it->second;
}

const SkillConfig* CombatManager::FindSkill(int32_t skill_id) const {
    auto it = skill_configs_.find(skill_id);
    return it == skill_configs_.end() ? nullptr : &it->second;
}

void CombatManager::SendToAll(const game::ServerMessage& msg) {
    if (session_manager_) session_manager_->BroadcastOnline(msg);
}

} // namespace combat
} // namespace game