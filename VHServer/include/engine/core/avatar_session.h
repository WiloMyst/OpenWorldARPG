#pragma once
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <grpcpp/grpcpp.h>
#include "avatarStream.grpc.pb.h"
#include "avatarStream.pb.h"

namespace engine {
    namespace infra { class ThreadPool; }
    namespace business { class AIBrain; }
}

namespace engine {
namespace core {

class AvatarSession : public std::enable_shared_from_this<AvatarSession> {
public:
    enum class EventType { CONNECT, READ, WRITE, FINISH };

    struct EventTag {
        std::shared_ptr<AvatarSession> instance;
        EventType type;
    };

    static void Create(Avatar::AvatarService::AsyncService* service, grpc::ServerCompletionQueue* cq,
                       infra::ThreadPool* pool, business::AIBrain* brain,
                       const std::string& dialogue_secret);

    void HandleEvent(EventType type, bool ok);

private:
    // 构造函数与私有方法的声明
    AvatarSession(Avatar::AvatarService::AsyncService* service, grpc::ServerCompletionQueue* cq,
                  infra::ThreadPool* pool, business::AIBrain* brain,
                  const std::string& dialogue_secret);

    void Start();
    void IssueRead();
    void IssueWrite(const Avatar::AvatarStreamResponse& response);
    void EnqueueWrite(const Avatar::AvatarStreamResponse& response);
    void ProcessRequestAsync(Avatar::AvatarStreamRequest req);

    // 对话票据验证: 解析 account.npc_id.expires_at.hmac, 重算签名 + 过期检查
    // 返回 false 时 reason 写明拒绝原因; 密钥未配置时放行 (离线调试模式)
    bool ValidateDialogueToken(const std::string& token, std::string* reason);

    // 成员变量
    Avatar::AvatarService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    infra::ThreadPool* pool_;
    business::AIBrain* brain_;
    const std::string dialogue_secret_;

    grpc::ServerContext ctx_;
    Avatar::AvatarStreamRequest request_;
    grpc::ServerAsyncReaderWriter<Avatar::AvatarStreamResponse, Avatar::AvatarStreamRequest> stream_;

    std::mutex write_mtx_;
    std::queue<Avatar::AvatarStreamResponse> write_queue_;
    bool is_writing_;
    
    std::atomic<bool> is_active_{true};
    std::atomic<bool> is_finishing_{false};
    std::atomic<uint64_t> current_request_id_{0};
};

} // namespace core
} // namespace engine