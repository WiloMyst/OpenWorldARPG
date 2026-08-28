// Phase 3 战斗系统单测: 配置加载 / 服务器权威刷怪 / 伤害结算 / 击杀判定
// Phase 3 玩家角色 HP: 会话内按角色 Tag 独立 HP 实体 + 仅 active 角色受击
// 运行: ./test_combat
#include "game/combat/combat_manager.h"
#include "game/infra/config_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>

namespace {

bool g_fail = false;

void Expect(bool cond, const char* what) {
    if (cond) {
        std::cout << "[PASS] " << what << "\n";
    } else {
        g_fail = true;
        std::cout << "[FAIL] " << what << "\n";
    }
}

// 写临时 YAML, 返回路径; 测试静默依赖杀 (不自动删除, 便于排查)
std::string WriteYaml(const std::string& name, const std::string& content) {
    const std::string path = name;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) return "";
    std::fwrite(content.data(), 1, content.size(), fp);
    std::fclose(fp);
    return path;
}

} // namespace

int main() {
    const std::string enemy_yaml = WriteYaml(
        "test_enemies.yaml",
        "enemies:\n"
        "  - enemy_type: 1\n"
        "    name: \"Forest Wolf\"\n"
        "    max_hp: 100.0\n"
        "    attack: 15.0\n"
        "    defense: 5.0\n"
        "    patrol_radius: 700.0\n");
    const std::string skill_yaml = WriteYaml(
        "test_skills.yaml",
        "skills:\n"
        "  - skill_id: 1\n"
        "    name: \"Melee Slash\"\n"
        "    damage_multiplier: 1.0\n"
        "    crit_rate: 0.0\n"
        "    crit_dmg: 0.0\n");

    game::infra::AppConfig cfg;
    cfg.enemies_config_path = enemy_yaml;
    cfg.skills_config_path = skill_yaml;
    cfg.default_player_attack = 30.0f;
    cfg.initial_enemy_count = 3;

    game::combat::CombatManager combat(cfg);
    // 单测不接会话: SendToAll 检测到空则会安全跳过广播

    // 巡逻区刷怪上报 (area 1, 基准位置 0,0,0) -> 服务器授权刷出 3 只
    game::EnemySpawnRequest spawn;
    spawn.set_area_id(1);
    spawn.set_x(0.0f);
    spawn.set_y(0.0f);
    spawn.set_z(0.0f);
    spawn.set_patrol_radius(100.0f);   // 巡逻半径 100
    combat.HandleSpawnRequest(spawn);
    Expect(combat.enemy_count() == 3, "HandleSpawnRequest 授权刷出 3 只初始敌人");

    // 同巡逻区重复上报应被去重拒绝, 不再新增敌人
    combat.HandleSpawnRequest(spawn);
    Expect(combat.enemy_count() == 3, "同巡逻区重复上报被去重, 敌人数量不变");

    Expect(combat.FindEnemyConfig(1) != nullptr, "FindEnemyConfig 命中类型 1");
    Expect(combat.FindEnemyConfig(99) == nullptr, "FindEnemyConfig 未知类型返回空");
    Expect(combat.FindEnemyConfig(1)->max_hp == 100.0f, "敌人配置 max_hp=100");

    const uint64_t target = 1; // HandleSpawnRequest 生成的第一个 enemy_id = 1
    game::combat::EnemyEntity enemy;
    Expect(combat.GetEnemy(target, &enemy), "GetEnemy 取到敌人 1");
    Expect(enemy.current_hp == 100.0f, "初始 HP = max_hp = 100");
    // 刷点须落在巡逻圆内: 到巡逻区圆心 (0,0,0) 的水平距离 <= patrol_radius (100)
    const double dist_to_center =
        std::sqrt(enemy.x * enemy.x + enemy.z * enemy.z);
    Expect(enemy.area_id == 1 && enemy.patrol_radius == 100.0f &&
               dist_to_center <= 100.0 &&
               "敌人归属 area=1, 巡逻半径生效, 刷点位于巡逻圆内",
           "敌人归属 area=1 且刷点落在 PatrolRadius(100) 巡逻圆内");

    // 每次 Melee Slash (multiplier=1, 攻击30, 防御5) -> 伤害 = max(0, 30 - 5*0.1) = 29.5
    game::DamageIntent intent;
    intent.set_skill_id(1);
    intent.set_attacker_player_id(42);
    intent.set_target_enemy_id(target);

    combat.HandleDamageIntent(intent.attacker_player_id(), intent);
    Expect(combat.GetEnemy(target, &enemy) && enemy.current_hp == 70.5f,
           "第 1 击: HP 100 -> 70.5 (伤害 29.5)");

    combat.HandleDamageIntent(intent.attacker_player_id(), intent);
    combat.HandleDamageIntent(intent.attacker_player_id(), intent);
    Expect(combat.GetEnemy(target, &enemy) && enemy.current_hp == 11.5f,
           "第 3 击: HP -> 11.5");

    combat.HandleDamageIntent(intent.attacker_player_id(), intent);
    Expect(combat.GetEnemy(target, &enemy) && enemy.killed && enemy.current_hp == 0.0f,
           "第 4 击: HP 触底为 0, killed=true");

    // 已击杀目标再次上报应被拒绝 (HP/状态不变)
    combat.HandleDamageIntent(intent.attacker_player_id(), intent);
    Expect(combat.GetEnemy(target, &enemy) && enemy.current_hp == 0.0f && enemy.killed,
           "击杀后伤害上报被拒绝 (HP 保持 0)");

    // 未知 enemy_id 不崩溃
    game::DamageIntent ghost;
    ghost.set_skill_id(1);
    ghost.set_target_enemy_id(999);
    combat.HandleDamageIntent(0, ghost);
    Expect(true, "未知 enemy_id 上报安全忽略");

    // ---- 玩家角色 HP (服务器权威): 会话内按角色 Tag 多实体 + 仅 active 受击 ----
    const std::string kAcc = "tester";

    combat.RegisterPlayer(kAcc);
    combat.RegisterCharacter(kAcc, "Character.Fire.Lumine", 500.0);
    combat.RegisterCharacter(kAcc, "Character.Geo.Noelle", 800.0);

    double cur = 0.0, maxhp = 0.0;
    std::string active;
    Expect(combat.GetCharacterHp(kAcc, "Character.Fire.Lumine", &cur, &maxhp) &&
               maxhp == 500.0 && cur == 500.0,
           "角色 A 注册: 建立 HP 实体 (500/500)");
    Expect(combat.GetCharacterHp(kAcc, "Character.Geo.Noelle", &cur, &maxhp) &&
               maxhp == 800.0 && cur == 800.0,
           "角色 B 注册: 建立 HP 实体 (800/800)");

    // 重复注册同 Tag 视为 upsert, 更新 HP 上限; 已存在的当前 HP 不被重置
    combat.RegisterCharacter(kAcc, "Character.Fire.Lumine", 600.0);
    Expect(combat.GetCharacterHp(kAcc, "Character.Fire.Lumine", &cur, &maxhp) &&
               maxhp == 600.0 && cur == 500.0,
           "重复注册同一 Tag 更新 max_hp, 当前 HP 保持 500");

    // 未注册角色不能切为 active
    combat.SetActiveCharacter(kAcc, "Character.Anemo.Venti");
    Expect(combat.GetCharacterHp(kAcc, "Character.Geo.Noelle", &cur, &maxhp) &&
               combat.GetActiveCharacter(kAcc, &active) && active == "Character.Fire.Lumine",
           "未注册角色切 active 被拒绝 (active 仍为首个注册角色)");

    combat.SetActiveCharacter(kAcc, "Character.Geo.Noelle");
    Expect(combat.GetCharacterHp(kAcc, "Character.Geo.Noelle", &cur, &maxhp) &&
               combat.GetActiveCharacter(kAcc, &active) && active == "Character.Geo.Noelle",
           "切 active 成功: active = Noelle");

    // 刷一路居新敌人作为攻击者 (area 2 -> 下一批 enemy_id = 4,5,6)
    game::EnemySpawnRequest spawn2;
    spawn2.set_area_id(2);
    combat.HandleSpawnRequest(spawn2);

    // 未知敌人攻击不结算
    game::EnemyAttackIntent atk_unknown;
    atk_unknown.set_enemy_id(999);
    combat.HandleEnemyAttackIntent(kAcc, atk_unknown);
    Expect(combat.GetCharacterHp(kAcc, "Character.Geo.Noelle", &cur, &maxhp) && cur == 800.0,
           "未知 enemy 攻击被拒绝 (active HP 不变)");

    // 服务器已刷出的敌人攻击 -> 只扣 active (Noelle), 非 active 角色血线不动
    game::EnemyAttackIntent atk;
    atk.set_enemy_id(4);
    combat.HandleEnemyAttackIntent(kAcc, atk);
    double noelle_after = 0.0;
    Expect(combat.GetCharacterHp(kAcc, "Character.Geo.Noelle", &cur, &maxhp) &&
               (noelle_after = cur) < 800.0,
           "active (Noelle) 受击扣血");
    Expect(combat.GetCharacterHp(kAcc, "Character.Fire.Lumine", &cur, &maxhp) && cur == 500.0,
           "非 active (Lumine) 血线不变 (500)");

    // 注销会话 -> 清理其全部角色 HP 实体
    combat.UnregisterPlayer(kAcc);
    Expect(!combat.GetCharacterHp(kAcc, "Character.Geo.Noelle", &cur, &maxhp),
           "注销会话清理角色 HP 实体");

    std::cout << (g_fail ? "\n== COMBAT TEST FAILED ==\n" : "\n== COMBAT TEST PASSED ==\n");
    return g_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}