#include "game/storage/redis_store.h"
#include "game/infra/config_manager.hpp"

#include <hiredis/hiredis.h>
#include <spdlog/spdlog.h>

#include <cstdarg>
#include <cstring>
#include <strings.h>

namespace game {
namespace storage {

namespace {

// 命令执行包装: 连接失效时尝试一次重连; 返回 reply 由调用方释放
redisReply* CommandWithRetry(redisContext*& ctx, bool& available,
                             const struct timeval& timeout, const char* format, ...) {
    // hiredis 线程模型下单连接串行使用, 重连同样由调用方的 mtx 串行化
    auto ensure = [&ctx, &available, &timeout]() -> bool {
        if (ctx != nullptr && ctx->err == 0) return true;
        if (ctx == nullptr) return false;
        // 断线懒重连: redisReconnect 复用建连时的地址, 0.14 不保留超时需重设
        if (redisReconnect(ctx) == REDIS_OK && ctx->err == 0) {
            redisSetTimeout(ctx, timeout);
            return true;
        }
        available = false;
        return false;
    };
    if (!ensure()) return nullptr;

    va_list ap;
    va_start(ap, format);
    redisReply* reply = static_cast<redisReply*>(redisvCommand(ctx, format, ap));
    va_end(ap);

    if (reply == nullptr || ctx->err != REDIS_OK) {
        // 网络级失败 (超时/断连): 标记不可用, 下次调用走重连
        if (reply != nullptr) freeReplyObject(reply);
        available = false;
        spdlog::warn("[RedisStore] Command failed [err={}]", ctx->errstr);
        return nullptr;
    }
    return reply;
}

struct timeval CmdTimeout(const infra::RedisConfig& cfg) {
    return { cfg.connect_timeout_ms / 1000, (cfg.connect_timeout_ms % 1000) * 1000 };
}

} // namespace

struct RedisStore::Impl {
    infra::RedisConfig cfg;
    redisContext* ctx = nullptr;
    std::mutex mtx;
    bool available = false;
};

RedisStore::RedisStore(const infra::RedisConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->cfg = config;
}

RedisStore::~RedisStore() {
    if (impl_ && impl_->ctx) {
        redisFree(impl_->ctx);
        impl_->ctx = nullptr;
    }
}

void RedisStore::Connect() {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    if (impl_->ctx) {
        redisFree(impl_->ctx);
        impl_->ctx = nullptr;
    }

    // hiredis 0.14 API: 连接超时走 redisConnectWithTimeout, 命令读写超时再补 redisSetTimeout
    struct timeval tv { impl_->cfg.connect_timeout_ms / 1000,
                        (impl_->cfg.connect_timeout_ms % 1000) * 1000 };

    impl_->ctx = redisConnectWithTimeout(impl_->cfg.host.c_str(),
                                         impl_->cfg.port, tv);
    if (impl_->ctx == nullptr || impl_->ctx->err != REDIS_OK) {
        spdlog::warn("[RedisStore] Connect failed [{}:{} err={}], 运行于降级模式 (无缓存/内存频控)",
                     impl_->cfg.host, impl_->cfg.port,
                     impl_->ctx ? impl_->ctx->errstr : "alloc failed");
        if (impl_->ctx) {
            redisFree(impl_->ctx);
            impl_->ctx = nullptr;
        }
        impl_->available = false;
        return;
    }
    redisSetTimeout(impl_->ctx, tv);

    // 认证 (Redis 6+ ACL; 无密码时跳过)
    if (!impl_->cfg.password.empty()) {
        redisReply* auth = static_cast<redisReply*>(
            redisCommand(impl_->ctx, "AUTH %b",
                         impl_->cfg.password.data(), impl_->cfg.password.size()));
        const bool ok = auth != nullptr && auth->type == REDIS_REPLY_STATUS;
        if (auth != nullptr) freeReplyObject(auth);
        if (!ok) {
            spdlog::warn("[RedisStore] AUTH failed, 运行于降级模式");
            redisFree(impl_->ctx);
            impl_->ctx = nullptr;
            impl_->available = false;
            return;
        }
    }

    impl_->available = true;
    spdlog::info("[RedisStore] Connected to {}:{} (degraded={} ttl={}s)",
                 impl_->cfg.host, impl_->cfg.port,
                 impl_->available ? "no" : "yes", impl_->cfg.player_cache_ttl_sec);
}

bool RedisStore::Available() const {
    return impl_->available;
}

bool RedisStore::SetEx(const std::string& key, const std::string& value, int ttl_sec) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    redisReply* reply = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "SET %b %b EX %d",
                                         key.data(), key.size(),
                                         value.data(), value.size(), ttl_sec);
    if (reply == nullptr) return false;
    const bool ok = reply->type == REDIS_REPLY_STATUS &&
                    reply->str != nullptr && strcasecmp(reply->str, "OK") == 0;
    freeReplyObject(reply);
    return ok;
}

bool RedisStore::Get(const std::string& key, std::string* value) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (value) value->clear();

    redisReply* reply = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "GET %b", key.data(), key.size());
    if (reply == nullptr) return false;

    bool ok = false;
    if (reply->type == REDIS_REPLY_STRING && reply->str != nullptr && value != nullptr) {
        value->assign(reply->str, reply->len);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisStore::Expire(const std::string& key, int ttl_sec) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    redisReply* reply = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "EXPIRE %b %d",
                                         key.data(), key.size(), ttl_sec);
    if (reply == nullptr) return false;
    const bool ok = reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
    freeReplyObject(reply);
    return ok;
}

void RedisStore::Del(const std::string& key) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    redisReply* reply = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "DEL %b", key.data(), key.size());
    if (reply != nullptr) freeReplyObject(reply);
}

bool RedisStore::RateAllow(const std::string& key, int window_sec, int max_count) {
    std::lock_guard<std::mutex> lock(impl_->mtx);

    // 1) 窗口首个请求: SET NX + EX 原子抢占
    redisReply* set = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "SET %b 1 EX %d NX",
                                       key.data(), key.size(), window_sec);
    if (set == nullptr) return false;
    const bool first = set->type == REDIS_REPLY_STATUS &&
                       set->str != nullptr && strcasecmp(set->str, "OK") == 0;
    freeReplyObject(set);
    if (first) return true;

    // 2) 窗口内计数: INCR; 回到 1 说明 SET 与 INCR 之间 key 已过期重建, 补 TTL
    redisReply* incr = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "INCR %b", key.data(), key.size());
    if (incr == nullptr) return false;
    const bool ok = incr->type == REDIS_REPLY_INTEGER && incr->integer <= max_count;
    const bool need_ttl = incr->type == REDIS_REPLY_INTEGER && incr->integer == 1;
    freeReplyObject(incr);

    if (need_ttl) {
        redisReply* exp = CommandWithRetry(impl_->ctx, impl_->available, CmdTimeout(impl_->cfg), "EXPIRE %b %d",
                                           key.data(), key.size(), window_sec);
        if (exp != nullptr) freeReplyObject(exp);
    }
    return ok;
}

} // namespace storage
} // namespace game
