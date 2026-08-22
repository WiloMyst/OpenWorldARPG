#pragma once
#include <memory>
#include <mutex>
#include <string>

namespace game {
    namespace infra { struct RedisConfig; }
}

struct redisContext;

namespace game {
namespace storage {

// Redis 热数据层: 缓存 (Cache-Aside) + 在线状态 TTL + 分布式固定窗口限流
// 定位为可选依赖: 连接失败不阻断服务器启动, 全部能力自动降级 (MySQL 直读/内存频控)
// 线程模型: 单连接 + 内部互斥锁, worker 线程并发调用安全; 命令失败触发懒重连
class RedisStore {
public:
    explicit RedisStore(const infra::RedisConfig& config);
    ~RedisStore();

    // 建立连接; 失败仅告警不抛异常 (降级运行, 供多实例部署时旁路缓存角色)
    void Connect();

    bool Available() const;

    // SET key value EX ttl (SETEX)
    bool SetEx(const std::string& key, const std::string& value, int ttl_sec);

    // GET; key 不存在返回 false 且 *value 清空, 连接失败同样返回 false
    bool Get(const std::string& key, std::string* value);

    // 续期; key 已过期/不存在返回 false
    bool Expire(const std::string& key, int ttl_sec);

    void Del(const std::string& key);

    // 固定窗口计数限流: 窗口内首个请求 SET NX 抢占并带 TTL,
    // 后续 INCR 计数; INCR 到 1 时补 EXPIRE 兜底 (覆盖 SET 与 INCR 间的过期竞态)
    bool RateAllow(const std::string& key, int window_sec, int max_count);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace storage
} // namespace game
