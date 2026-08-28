#pragma once
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

namespace game {
namespace net {

// 从 client metadata 提取 Bearer 令牌 ("authorization: Bearer <token>")
inline std::string ExtractBearerToken(const grpc::ServerContext& ctx) {
    const auto& md = ctx.client_metadata();
    auto it = md.find("authorization");
    if (it == md.end()) return "";
    std::string header(it->second.data(), it->second.size());
    constexpr const char* kPrefix = "Bearer ";
    if (header.rfind(kPrefix, 0) == 0) return header.substr(7);
    return "";
}

// 异步调用基类: 所有经 CompletionQueue 驱动的 RPC (unary / World 流 / StreamDialogue)
// 统一继承本类. 生命周期由事件 tag 持有的强引用管理——流/调用终结 (FINISH) 后引用
// 归零自动析构, 调用方 (GrpcServer/SessionManager) 不持强引用.
class CallBase {
public:
    virtual ~CallBase() = default;

    // CompletionQueue 事件入口: type 为各派生类的 EventType;
    // ok=false 表示该操作被取消 (对端断开 / 服务器关停), 走统一收尾
    virtual void HandleEvent(int type, bool ok) = 0;

    // 事件 tag: 派发期间持有调用强引用, 保证对象存活至 HandleEvent 返回;
    // 同一时刻各类操作 (读/写/finish) 各至多一个在途 tag, 事件天然不重入
    struct EventTag {
        std::shared_ptr<CallBase> call;
        int type;
    };

    static EventTag* MakeTag(std::shared_ptr<CallBase> call, int type) {
        return new EventTag{std::move(call), type};
    }
};

} // namespace net
} // namespace game
