#include <csignal>

#include "game/infra/logger_setup.hpp"
#include "game/infra/config_manager.hpp"
#include "game/core/grpc_server.h"

using namespace game::infra;

int main() {
    // 临时初始化日志, 后续根据配置重新初始化
    InitLogger();

    spdlog::info("====================================");
    spdlog::info(" GameServer - OpenWorldARPG");
    spdlog::info(" Authoritative Session / Auth / Save");
    spdlog::info("====================================");

    try {
        AppConfig config = LoadConfig("../config.yaml");

        spdlog::drop("GameServer");
        InitLogger(config.log_level, config.log_file_path,
                   config.max_file_size_mb, config.max_files);

        spdlog::info("Config loaded [server={}:{}, workers={}, queue={}]",
                     config.host, config.port, config.worker_threads, config.max_queue_size);
        spdlog::info("Session [heartbeat_timeout={}s, login_timeout={}s, check_interval={}s]",
                     config.heartbeat_timeout_sec, config.login_timeout_sec,
                     config.session_check_interval_sec);

        // 阻塞运行, 直到收到关闭信号
        game::core::GrpcServer server;
        server.Run(config);

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("GameServer stopped.");
    return EXIT_SUCCESS;
}
