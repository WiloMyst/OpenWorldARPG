#pragma once
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace game {
namespace infra {

struct MysqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "game";
    std::string password;
    std::string database = "game_server";
    int connect_timeout_ms = 3000;
};

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password;               // 空串 = 无认证 (开发环境)
    int connect_timeout_ms = 2000;
    int player_cache_ttl_sec = 300;     // 玩家数据 Cache-Aside 读缓存 TTL
};

// 世界同步配置 (Phase 1: 状态同步 + AOI)
struct WorldConfig {
    float grid_size = 20.0f;            // AOI 格子边长 (m)
    int view_radius_cells = 1;          // 视野半径 (格), 1 = 九宫格
    float max_move_speed = 12.0f;       // 移动速度上限 (m/s)
    float speed_tolerance = 1.5f;       // 速度校验容差系数 (容忍网络抖动)
    float teleport_threshold = 100.0f;  // 单次位移硬上限 (m), 超过直接判定瞬移
    int64_t max_timestamp_skew_ms = 5000; // 客户端时间戳与服务器最大偏差 (ms)
    float spawn_x = 0.0f;               // 服务器权威出生点
    float spawn_y = 0.0f;
    float spawn_z = 0.0f;
    float spawn_yaw = 0.0f;
};

struct AppConfig {
    // Server
    std::string host = "0.0.0.0";
    int port = 50061;
    int worker_threads = 4;
    int max_queue_size = 1000;

    // Session
    int heartbeat_timeout_sec = 30;
    int login_timeout_sec = 15;
    int session_check_interval_sec = 5;

    // 对话票据签名密钥: 与 VHServer 共享, 环境变量 DIALOGUE_SECRET 优先; 为空则拒绝签发
    std::string dialogue_secret;
    int dialogue_token_ttl_sec = 30;

    // VHServer 控制面联动: 玩家会话终结 (登出/踢出/断线/超时) 时通知 VHServer 吊销对话流.
    // endpoint 或 admin_secret 为空时禁用; 须与 VHServer config.yaml 的 admin.host:port /
    // admin.secret 一致, 环境变量 ADMIN_SECRET 优先
    std::string vhserver_admin_endpoint = "127.0.0.1:50053";
    std::string vhserver_admin_secret;

    // 静态数据表 (data/ 目录, 相对运行目录)
    std::string items_config_path = "data/items.yaml";

    // Combat (Phase 3): 敌人/技能静态配置 (数据驱动伤害计算)
    std::string enemies_config_path = "data/enemies.yaml";
    std::string skills_config_path = "data/skills.yaml";
    float default_player_attack = 30.0f;  // 客户端 AS_Player 只有 HP, 玩家攻击由服务器持
    int initial_enemy_count = 3;          // 服务器启动时默认巡逻区刷怪数量

    // InitialArchive (Phase 3): 新玩家初始存档配置 (镜像客户端 UInitialArchiveData)
    std::string initial_archive_config_path = "data/initial_archive.yaml";

    // MySQL
    MysqlConfig mysql;

    // Redis (可选依赖, 失败降级运行)
    RedisConfig redis;

    // World (Phase 1: 状态同步 + AOI)
    WorldConfig world;

    // Logging
    std::string log_level = "info";
    std::string log_file_path = "logs/game_server.log";
    int max_file_size_mb = 10;
    int max_files = 3;
};

inline AppConfig LoadConfig(const std::string& filepath) {
    AppConfig config;
    try {
        YAML::Node node = YAML::LoadFile(filepath);

        if (node["server"]) {
            config.host = node["server"]["host"].as<std::string>(config.host);
            config.port = node["server"]["port"].as<int>(config.port);
            config.worker_threads = node["server"]["worker_threads"].as<int>(config.worker_threads);
            config.max_queue_size = node["server"]["max_queue_size"].as<int>(config.max_queue_size);
        }

        if (node["session"]) {
            config.heartbeat_timeout_sec =
                node["session"]["heartbeat_timeout_sec"].as<int>(config.heartbeat_timeout_sec);
            config.login_timeout_sec =
                node["session"]["login_timeout_sec"].as<int>(config.login_timeout_sec);
            config.session_check_interval_sec =
                node["session"]["check_interval_sec"].as<int>(config.session_check_interval_sec);
        }

        if (node["auth"]) {
            // 环境变量 DIALOGUE_SECRET 优先, 避免密钥明文进配置文件
            const char* env_secret = std::getenv("DIALOGUE_SECRET");
            if (env_secret != nullptr && *env_secret != '\0') {
                config.dialogue_secret = env_secret;
            } else {
                config.dialogue_secret = node["auth"]["dialogue_secret"].as<std::string>("");
            }
            config.dialogue_token_ttl_sec =
                node["auth"]["dialogue_token_ttl_sec"].as<int>(config.dialogue_token_ttl_sec);
        }

        if (node["vhserver"]) {
            const YAML::Node& v = node["vhserver"];
            // 环境变量 ADMIN_SECRET 优先, 避免密钥明文进配置文件
            const char* env_admin = std::getenv("ADMIN_SECRET");
            if (env_admin != nullptr && *env_admin != '\0') {
                config.vhserver_admin_secret = env_admin;
            } else {
                config.vhserver_admin_secret = v["admin_secret"].as<std::string>("");
            }
            config.vhserver_admin_endpoint =
                v["admin_endpoint"].as<std::string>(config.vhserver_admin_endpoint);
        }

        if (node["items"]) {
            config.items_config_path = node["items"]["config_path"].as<std::string>(config.items_config_path);
        }

        if (node["archive"]) {
            config.initial_archive_config_path =
                node["archive"]["config_path"].as<std::string>(config.initial_archive_config_path);
        }

        if (node["combat"]) {
            const YAML::Node& c = node["combat"];
            config.enemies_config_path =
                c["enemy_config_path"].as<std::string>(config.enemies_config_path);
            config.skills_config_path =
                c["skill_config_path"].as<std::string>(config.skills_config_path);
            config.default_player_attack =
                c["default_player_attack"].as<float>(config.default_player_attack);
            config.initial_enemy_count =
                c["initial_enemy_count"].as<int>(config.initial_enemy_count);
        }

        if (node["mysql"]) {
            const YAML::Node& m = node["mysql"];
            // 环境变量 GAME_DB_PASSWORD 优先, 避免密码明文进配置文件
            const char* env_pw = std::getenv("GAME_DB_PASSWORD");
            if (env_pw != nullptr && *env_pw != '\0') {
                config.mysql.password = env_pw;
            } else {
                config.mysql.password = m["password"].as<std::string>("");
            }
            config.mysql.host = m["host"].as<std::string>(config.mysql.host);
            config.mysql.port = m["port"].as<int>(config.mysql.port);
            config.mysql.user = m["user"].as<std::string>(config.mysql.user);
            config.mysql.database = m["database"].as<std::string>(config.mysql.database);
            config.mysql.connect_timeout_ms = m["connect_timeout_ms"].as<int>(config.mysql.connect_timeout_ms);
        }

        if (node["redis"]) {
            const YAML::Node& r = node["redis"];
            // 环境变量 REDIS_PASSWORD 优先, 避免密码明文进配置文件
            const char* env_rp = std::getenv("REDIS_PASSWORD");
            if (env_rp != nullptr && *env_rp != '\0') {
                config.redis.password = env_rp;
            } else {
                config.redis.password = r["password"].as<std::string>("");
            }
            config.redis.host = r["host"].as<std::string>(config.redis.host);
            config.redis.port = r["port"].as<int>(config.redis.port);
            config.redis.connect_timeout_ms =
                r["connect_timeout_ms"].as<int>(config.redis.connect_timeout_ms);
            config.redis.player_cache_ttl_sec =
                r["player_cache_ttl_sec"].as<int>(config.redis.player_cache_ttl_sec);
        }

        if (node["world"]) {
            const YAML::Node& w = node["world"];
            config.world.grid_size = w["grid_size"].as<float>(config.world.grid_size);
            config.world.view_radius_cells =
                w["view_radius_cells"].as<int>(config.world.view_radius_cells);
            config.world.max_move_speed =
                w["max_move_speed"].as<float>(config.world.max_move_speed);
            config.world.speed_tolerance =
                w["speed_tolerance"].as<float>(config.world.speed_tolerance);
            config.world.teleport_threshold =
                w["teleport_threshold"].as<float>(config.world.teleport_threshold);
            config.world.max_timestamp_skew_ms =
                w["max_timestamp_skew_ms"].as<int64_t>(config.world.max_timestamp_skew_ms);
            config.world.spawn_x = w["spawn_x"].as<float>(config.world.spawn_x);
            config.world.spawn_y = w["spawn_y"].as<float>(config.world.spawn_y);
            config.world.spawn_z = w["spawn_z"].as<float>(config.world.spawn_z);
            config.world.spawn_yaw = w["spawn_yaw"].as<float>(config.world.spawn_yaw);
        }

        if (node["logging"]) {
            config.log_level = node["logging"]["level"].as<std::string>(config.log_level);
            config.log_file_path = node["logging"]["file_path"].as<std::string>(config.log_file_path);
            config.max_file_size_mb = node["logging"]["max_file_size_mb"].as<int>(config.max_file_size_mb);
            config.max_files = node["logging"]["max_files"].as<int>(config.max_files);
        }

        return config;
    } catch (const YAML::Exception& e) {
        spdlog::critical("Config load failed: {}", e.what());
        throw std::runtime_error("Config load failed");
    }
}

} // namespace infra
} // namespace game
