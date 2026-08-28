#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>

namespace game {
namespace infra {

// 日志初始化: 异步控制台 + 轮转文件双输出
inline void InitLogger(const std::string& level_str = "info",
                       const std::string& file_path = "logs/game_server.log",
                       size_t max_file_size_mb = 10,
                       size_t max_files = 3) {
    auto log_dir = std::filesystem::path(file_path).parent_path();
    if (!log_dir.empty()) {
        std::filesystem::create_directories(log_dir);
    }

    // 线程池只初始化一次: 重复 init_thread_pool 会销毁旧池并 join 其 worker,
    // 与队列中在途消息竞态, 会导致重初始化后异步日志静默丢失。
    // 重初始化仅重建 logger/sinks, 复用全局池 (spdlog 异步 logger 标准用法)。
    if (spdlog::thread_pool() == nullptr) {
        spdlog::init_thread_pool(8192, 1);
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        file_path, max_file_size_mb * 1024 * 1024, max_files);

    auto logger = std::make_shared<spdlog::async_logger>(
        "GameServer",
        spdlog::sinks_init_list{console_sink, file_sink},
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l$%] %v");

    if (level_str == "trace") {
        logger->set_level(spdlog::level::trace);
    } else if (level_str == "debug") {
        logger->set_level(spdlog::level::debug);
    } else if (level_str == "info") {
        logger->set_level(spdlog::level::info);
    } else if (level_str == "warn") {
        logger->set_level(spdlog::level::warn);
    } else if (level_str == "error") {
        logger->set_level(spdlog::level::err);
    } else {
        logger->set_level(spdlog::level::info);
    }

    spdlog::set_default_logger(logger);
    spdlog::info("Logger initialized [level={}, file={}]", level_str, file_path);
}

} // namespace infra
} // namespace game
