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

    // Auth: M1 静态令牌, 后续替换为账号服务校验
    std::string static_token = "dev-token-2026";

    // 对话票据签名密钥: 与 VHServer 共享, 环境变量 DIALOGUE_SECRET 优先; 为空则拒绝签发
    std::string dialogue_secret;
    int dialogue_token_ttl_sec = 30;

    // Items
    std::string items_config_path = "items.yaml";

    // MySQL
    MysqlConfig mysql;

    // Redis (可选依赖, 失败降级运行)
    RedisConfig redis;

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
            config.static_token = node["auth"]["static_token"].as<std::string>(config.static_token);
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

        if (node["items"]) {
            config.items_config_path = node["items"]["config_path"].as<std::string>(config.items_config_path);
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
